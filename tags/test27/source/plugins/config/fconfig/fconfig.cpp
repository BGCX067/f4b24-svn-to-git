#define WINVER		0x0400	// 98�ȍ~
#define _WIN32_WINNT	0x0400
#define _WIN32_IE	0x0400	// IE4�ȍ~

#include <windows.h>
#include <commctrl.h>
#include <shlwapi.h>
#include <prsht.h>

#include "../cplugin.h"

#if defined(_MSC_VER)
#pragma comment(lib,"kernel32.lib")
#pragma comment(lib,"user32.lib")
//#pragma comment(lib,"gdi32.lib")
#pragma comment(lib,"comctl32.lib")
#pragma comment(lib,"shlwapi.lib")
#pragma comment(lib,"shell32.lib")
#endif
#if defined(_MSC_VER) && !defined(_DEBUG)
#pragma comment(linker,"/MERGE:.rdata=.text")
#pragma comment(linker,"/ENTRY:WinMain")
#pragma comment(linker,"/OPT:NOWIN98")
#endif

#define MAX_FITTLE_PATH 260*2
#define FCONFIG_MAPPING_NAME "fconfig.exe - <shared memory>"

typedef enum {
	SM_SUCCESS,
	SM_ALREADY_EXISTS,
	SM_ERROR
} SHARED_MEMORY_STATUS;

typedef struct {
	HWND hwnd;
} SHARED_MEMORY;

/* ���L������ */
static HANDLE hsm = NULL;
static SHARED_MEMORY *psm = NULL;

/* �v���O�C�����X�g */
static CONFIG_PLUGIN_INFO *pTop = NULL;

static BOOL fIsUnicode = FALSE;


static void CloseSharedMemory(){
	if (psm){
		UnmapViewOfFile(psm);
		psm = NULL;
	}
	if (hsm){
		CloseHandle(hsm);
		hsm = NULL;
	}
}

static SHARED_MEMORY_STATUS OpenSharedMemory(){
	SHARED_MEMORY_STATUS ret = SM_SUCCESS;
	CloseSharedMemory();
	hsm = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(SHARED_MEMORY), TEXT(FCONFIG_MAPPING_NAME));
	if (!hsm) return SM_ERROR;
	if (GetLastError() == ERROR_ALREADY_EXISTS)
		ret = SM_ALREADY_EXISTS;
	psm = (SHARED_MEMORY *)MapViewOfFile(hsm, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(SHARED_MEMORY));
	if (!psm){
		CloseSharedMemory();
		return SM_ERROR;
	}
	return ret;
}

/* �v���O�C����o�^ */
static BOOL RegisterPlugin(FARPROC lpfnProc){
	GetCPluginInfoFunc GetCPluginInfo = (GetCPluginInfoFunc)lpfnProc;
	if (GetCPluginInfo){
		CONFIG_PLUGIN_INFO *pNewPlugins = GetCPluginInfo();
		if (pNewPlugins){
			CONFIG_PLUGIN_INFO *pNext = pNewPlugins;
			do{
				pNext = pNext->pNext;
			} while (pNext);
			for (pNext = pNewPlugins; pNext->pNext; pNext = pNext->pNext);
			pNext->pNext = pTop;
			pTop = pNewPlugins;
			return TRUE;
		}
	}
	return FALSE;
}

static void InitPlugins(){
	HANDLE hFind;
	union {
		CHAR A[MAX_PATH];
		WCHAR W[MAX_PATH];
	} szDir, szPath;
	union {
		WIN32_FIND_DATAA A;
		WIN32_FIND_DATAW W;
	} wfd;

	if (fIsUnicode) {
		GetModuleFileNameW(NULL, szDir.W, MAX_FITTLE_PATH);
		*PathFindFileNameW(szDir.W) = L'\0';
		PathCombineW(szPath.W, szDir.W, L"*.fcp");
		hFind = FindFirstFileW(szPath.W, &wfd.W);
	} else {
		GetModuleFileNameA(NULL, szDir.A, MAX_FITTLE_PATH);
		*PathFindFileNameA(szDir.A) = '\0';
		PathCombineA(szPath.A, szDir.A, "*.fcp");
		hFind = FindFirstFileA(szPath.A, &wfd.A);
	}

	if(hFind!=INVALID_HANDLE_VALUE){
		do{
			HINSTANCE hDll;
			if (fIsUnicode){
				PathCombineW(szPath.W, szDir.W, wfd.W.cFileName);
				hDll = LoadLibraryW(szPath.W);
			}else{
				PathCombineA(szPath.A, szDir.A, wfd.A.cFileName);
				hDll = LoadLibraryA(szPath.A);
			}
			if(hDll){
				FARPROC pfnCPlugin = GetProcAddress(hDll, "GetCPluginInfo");
				if (!pfnCPlugin || !RegisterPlugin(pfnCPlugin))
				{
					FreeLibrary(hDll);
				}
			}
		}while(fIsUnicode ? FindNextFileW(hFind, &wfd.W) : FindNextFileA(hFind, &wfd.A));
		FindClose(hFind);
	}
}

static void *HAlloc(DWORD dwSize){
	return HeapAlloc(GetProcessHeap(), 0, dwSize);
}

static void HFree(void *pPtr){
	HeapFree(GetProcessHeap(), 0, pPtr);
}

static LPCSTR SkipSpace(LPCSTR p){
	while (*p == ' ') p++;
	return p;
}

static LPCSTR SkipToken(LPCSTR p){
	if (*p == '\"'){
		p++;
		while (*p && *p != '\"') p++;
		if (*p == '\"') p++;
		return p;
	}
	while (*p && *p != ' ') p++;
	return p;
}

static void SheetActivate(LPPROPSHEETHEADER ppsh){
	if (psm){
		HWND hwnd = psm->hwnd;
		if (hwnd && IsWindow(hwnd)){
			PropSheet_SetCurSel(hwnd, ppsh->nStartPage, ppsh->nStartPage);
			ShowWindow(hwnd, SW_SHOW);
			SetForegroundWindow(hwnd);
		}
	}
}

static void SheetInvoke(LPPROPSHEETHEADER ppsh){
	if (ppsh->phpage && ppsh->nPages > 0){
		int i;
		PropertySheet(ppsh);
		/* �y�[�W��PropertySheet���ŊJ������邽�ߓ�d�J����h�~���� */
		for (i = 0; i < ppsh->nPages; i++){
			if (ppsh->phpage[i]) ppsh->phpage[i] = 0;
		}
	}
}
static void SheetFree(LPPROPSHEETHEADER ppsh){
	if (ppsh->phpage){
		int i;
		for (i = 0; i < ppsh->nPages; i++){
			if (ppsh->phpage[i]) DestroyPropertySheetPage(ppsh->phpage[i]);
		}
		HFree(ppsh->phpage);
		ppsh->phpage = 0;
	}
}

static int CALLBACK PropSheetProc(HWND hwndDlg, UINT uMsg, LPARAM lParam){
	if (uMsg == PSCB_INITIALIZED){
		if (psm) psm->hwnd = hwndDlg;
	}
	return 0;
}

static int SheetSetup(PROPSHEETHEADER *ppsh, LPCSTR lpszStartPath, int nStartPathLen){
	int nMaxPage = 0;
	int nNumPage = 0;
	int nStartPage = 0;
	CONFIG_PLUGIN_INFO *pCur;

	ppsh->phpage = 0;
	ppsh->nPages = 0;

	for (pCur = pTop; pCur; pCur = pCur->pNext)
		nMaxPage += pCur->GetConfigPageCount();

	if (nMaxPage == 0) return 0;
	
	ppsh->phpage = (HPROPSHEETPAGE FAR *)HAlloc(sizeof(HPROPSHEETPAGE) * nMaxPage);
	if (ppsh->phpage) {
		int nLevel;
		for (nLevel = 0; nLevel < 4; nLevel++){
			for (pCur = pTop; pCur; pCur = pCur->pNext){
				int nIndex = 0;
				HPROPSHEETPAGE hpspAdd;
				do{
					char szPath[64];
					hpspAdd = pCur->GetConfigPage(nIndex++, nLevel, szPath, 64);
					if (hpspAdd && nNumPage < nMaxPage){
						if (nStartPathLen > 0 && memcmp(szPath, lpszStartPath, nStartPathLen * sizeof(CHAR)) == 0)
							nStartPage = nNumPage;
						ppsh->phpage[nNumPage++] = hpspAdd;
					}
				}while(hpspAdd);
			}
		}
		if (nNumPage > 0){
			ppsh->dwSize = sizeof (PROPSHEETHEADER);
			ppsh->dwFlags = PSH_USEICONID | PSH_USECALLBACK;
			ppsh->hwndParent = 0;
			ppsh->hInstance = GetModuleHandle(NULL);
			ppsh->pszIcon = TEXT("MYICON");
			ppsh->pszCaption = TEXT("�ݒ�");
			ppsh->nPages = nNumPage;
			ppsh->nStartPage = nStartPage;
			ppsh->pfnCallback = PropSheetProc; 
		}
	}
	return nNumPage;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow){
	SHARED_MEMORY_STATUS sms = OpenSharedMemory();
	if (SM_ERROR != sms){

		PROPSHEETHEADER psh;
		LPCSTR lpszCmdLine = GetCommandLineA();
		LPCSTR lpArg1Top = SkipSpace(SkipToken(lpszCmdLine));
		LPCSTR lpArg1End = SkipToken(lpArg1Top);

		int nNumPage;

		fIsUnicode = ((GetVersion() & 0x80000000) == 0);

		if (*lpArg1Top == '\"') lpArg1Top++;
		if (lpArg1End > lpArg1Top && *(lpArg1End-1) == '\"') lpArg1End--;

		InitCommonControls();
		InitPlugins();

		nNumPage = SheetSetup(&psh, lpArg1Top, lpArg1End - lpArg1Top);

		if (sms == SM_SUCCESS)
			SheetInvoke(&psh);
		else if (sms == SM_ALREADY_EXISTS)
			SheetActivate(&psh);

		SheetFree(&psh);
		CloseSharedMemory();
	}
	ExitProcess(0);
	return 0;
}