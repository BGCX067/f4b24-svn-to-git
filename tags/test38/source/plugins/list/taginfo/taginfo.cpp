#include "../../../fittle/src/fittle.h"
#include "../../../fittle/src/f4b24.h"
#include "../../../fittle/src/f4b24lx.h"
#include "../../ui/lplugin/lplugin.h"

#include <shlwapi.h>

#if defined(_MSC_VER)
#pragma comment(lib,"kernel32.lib")
#pragma comment(lib,"user32.lib")
#pragma comment(lib,"gdi32.lib")
#pragma comment(lib,"comdlg32.lib")
#pragma comment(lib,"comctl32.lib")
#pragma comment(lib,"shlwapi.lib")
#pragma comment(lib,"shell32.lib")
#pragma comment(lib,"ole32.lib")
#pragma comment(linker, "/EXPORT:GetLXPluginInfo=_GetLXPluginInfo@0")
#endif
#if defined(_MSC_VER) && !defined(_DEBUG)
#pragma comment(linker,"/ENTRY:DllMain")
#pragma comment(linker,"/MERGE:.rdata=.text")
#pragma comment(linker,"/OPT:NOWIN98")
#endif

#define COLUMN_TYPE_ID 8
#define COLUMN_TYPE_NUM 5

// {C3FC1E93-09A1-497b-BC58-1CAECD93AF78}
static const GUID ID_TAGINFO = { 0xc3fc1e93, 0x9a1, 0x497b, { 0xbc, 0x58, 0x1c, 0xae, 0xcd, 0x93, 0xaf, 0x78 } };

int CALLBACK GetTypeNum();
int CALLBACK GetTypeCode(int nIndex);
LPCSTR CALLBACK GetTypeName(int nIndex);
BOOL CALLBACK IsSupported(int nType);
BOOL CALLBACK InitColumnOrder(int nColumn, int nType);
void CALLBACK AddColumn(HWND hList, int nColumn, int nType);
void CALLBACK GetColumnText(LPVOID pFileInfo, int nRow, int nColumn, int nType, LPVOID pBuf, int nBufSize);
int CALLBACK CompareColumnText(LPVOID pFileInfoLeft, LPVOID pFileInfoRight, int nColumn, int nType);
void CALLBACK OnSave(HWND hList, int nColumn, int nType, int nWidth);

static LX_PLUGIN_INFO lxpinfo = {
	LPDK_VER,
	GetTypeNum,
	GetTypeCode,
	GetTypeName,
	IsSupported,
	InitColumnOrder,
	AddColumn,
	GetColumnText,
	CompareColumnText,
	OnSave
};

#ifdef __cplusplus
extern "C"
#endif
LX_PLUGIN_INFO * CALLBACK GetLXPluginInfo(void){
	return &lxpinfo;
}

static HMODULE m_hDLL = 0;

int (CALLBACK *pStrCmpLogicalW)(LPCWSTR psz1, LPCWSTR psz2) = 0;
 
void *HZAlloc(DWORD dwSize){
	return HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, dwSize);
}

void HFree(LPVOID pPtr){
	HeapFree(GetProcessHeap(), 0, pPtr);
}


BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved){
	(void)lpvReserved;
	if (fdwReason == DLL_PROCESS_ATTACH){
		m_hDLL = hinstDLL;
		DisableThreadLibraryCalls(hinstDLL);
	}else if (fdwReason == DLL_PROCESS_DETACH){
	}
	return TRUE;
}

int CALLBACK GetTypeNum(){
	return COLUMN_TYPE_NUM;
}
int CALLBACK GetTypeCode(int nIndex){
	if (nIndex >= 0 && nIndex < COLUMN_TYPE_NUM) return COLUMN_TYPE_ID + nIndex;
	return -1;
}

static LPCSTR lbltblL[]= {
	"Title",
	"Artist",
	"Album",
	"Track",
	"Titleが無ければファイル名"
};
static LPCSTR lbltblA[]= {
	"Title",
	"Artist",
	"Album",
	"Track",
	"Title"
};
static LPCWSTR lbltblW[]= {
	L"Title",
	L"Artist",
	L"Album",
	L"Track",
	L"Title"
};

LPCSTR CALLBACK GetTypeName(int nIndex){
	if (nIndex >= 0 && nIndex < COLUMN_TYPE_NUM) return lbltblL[nIndex];
	return "";
}
BOOL CALLBACK IsSupported(int nType){
	return (nType >= COLUMN_TYPE_ID) && (nType < COLUMN_TYPE_ID + COLUMN_TYPE_NUM);
}
BOOL CALLBACK InitColumnOrder(int nColumn, int nType){
	return IsSupported(nType);
}
void CALLBACK AddColumn(HWND hList, int nColumn, int nType){
	if (IsSupported(nType)) {
		CHAR szSec[16];
		wsprintfA(szSec, "WidthEx%d", nType);
		lxpinfo.plxif->AddColumn(hList, nColumn, lxpinfo.plxif->nUnicode ? (LPVOID)lbltblW[nType - COLUMN_TYPE_ID] : (LPVOID)lbltblA[nType - COLUMN_TYPE_ID], lxpinfo.GetIniInt("Column", szSec, 100), LVCFMT_LEFT);
		if (nType == COLUMN_TYPE_ID + 3)
			pStrCmpLogicalW = (int (CALLBACK *)(LPCWSTR psz1, LPCWSTR psz2))GetProcAddress(GetModuleHandleA("shlwapi.dll"), "StrCmpLogicalW");
	}
}

static void CALLBACK FreeTagInfo(LPVOID p){
	HFree(p);
}

static BOOL CheckPath(LPVOID pFileInfo){
	if (lxpinfo.plxif->CheckPath(pFileInfo, F4B24LX_CHECK_URL)) return FALSE;
	if (lxpinfo.plxif->CheckPath(pFileInfo, F4B24LX_CHECK_CUE)) return TRUE;
	if (lxpinfo.GetIniInt("Column", "DisallowTagInArchive", 0) == 1){
		if (lxpinfo.plxif->CheckPath(pFileInfo, F4B24LX_CHECK_ARC)) return FALSE;
	}
	return TRUE;
}

static TAGINFOW *GetTagInfoW(LPVOID pFileInfo){
	TAGINFOW *pTagInfo = (TAGINFOW *)lxpinfo.GetUserData(pFileInfo, (LPVOID)&ID_TAGINFO);
	if (!pTagInfo) {
		pTagInfo = (TAGINFOW *)HZAlloc(sizeof(TAGINFOW));
		if (pTagInfo) {
			if (CheckPath(pFileInfo)) {
				LPVOID pMusic = lxpinfo.plxif->LoadMusic(pFileInfo);
				if (pMusic){
					lxpinfo.plxif->GetTag(pMusic, pTagInfo);
					lxpinfo.plxif->FreeMusic(pMusic);
				}
			}
			lxpinfo.SetUserData(pFileInfo, (LPVOID)&ID_TAGINFO, pTagInfo, FreeTagInfo);
		}
	}
	return pTagInfo;
}

static TAGINFOA *GetTagInfoA(LPVOID pFileInfo){
	TAGINFOA *pTagInfo = (TAGINFOA *)lxpinfo.GetUserData(pFileInfo, (LPVOID)&ID_TAGINFO);
	if (!pTagInfo) {
		pTagInfo = (TAGINFOA *)HZAlloc(sizeof(TAGINFOA));
		if (pTagInfo) {
			if (CheckPath(pFileInfo)) {
				LPVOID pMusic = lxpinfo.plxif->LoadMusic(pFileInfo);
				if (pMusic){
					lxpinfo.plxif->GetTag(pMusic, pTagInfo);
					lxpinfo.plxif->FreeMusic(pMusic);
				}
			}
			lxpinfo.SetUserData(pFileInfo, (LPVOID)&ID_TAGINFO, pTagInfo, FreeTagInfo);
		}
	}
	return pTagInfo;
}

static void GetFileNameNoExtW(LPVOID pFileInfo, LPVOID pBuf, int nBufSize){
	LPWSTR pExt;
	lstrcpynW((LPWSTR)pBuf, (LPCWSTR)lxpinfo.plxif->GetFileName(pFileInfo), nBufSize);
	if (!lxpinfo.plxif->CheckPath(pFileInfo, F4B24LX_CHECK_URL) && !lxpinfo.plxif->CheckPath(pFileInfo, F4B24LX_CHECK_CUE)){
		pExt = PathFindExtensionW((LPWSTR)pBuf);
		if (pExt) *pExt = 0;
	}
}

static void GetFileNameNoExtA(LPVOID pFileInfo, LPVOID pBuf, int nBufSize){
	LPSTR pExt;
	lstrcpynA((LPSTR)pBuf, (LPCSTR)lxpinfo.plxif->GetFileName(pFileInfo), nBufSize);
	if (!lxpinfo.plxif->CheckPath(pFileInfo, F4B24LX_CHECK_URL) && !lxpinfo.plxif->CheckPath(pFileInfo, F4B24LX_CHECK_CUE)){
		pExt = PathFindExtensionA((LPSTR)pBuf);
		if (pExt) *pExt = 0;
	}
}

static void GetTitleFnameW(TAGINFOW *pTag, LPVOID pFileInfo, LPVOID pBuf, int nBufSize){
	if (pTag) lstrcpynW((LPWSTR)pBuf, pTag->szTitle, nBufSize);
	if (!pTag || !pTag->szTitle[0]) GetFileNameNoExtW(pFileInfo, pBuf, nBufSize);
}

static void GetTitleFnameA(TAGINFOA *pTag, LPVOID pFileInfo, LPVOID pBuf, int nBufSize){
	if (pTag) lstrcpynA((LPSTR)pBuf, pTag->szTitle, nBufSize);
	if (!pTag || !pTag->szTitle[0]) GetFileNameNoExtA(pFileInfo, pBuf, nBufSize);
}

void CALLBACK GetColumnText(LPVOID pFileInfo, int nRow, int nColumn, int nType, LPVOID pBuf, int nBufSize){
	if (lxpinfo.plxif->nUnicode) {
		TAGINFOW *pTag = GetTagInfoW(pFileInfo);
		if (nType == COLUMN_TYPE_ID + 4) {
			GetTitleFnameW(pTag, pFileInfo, pBuf, nBufSize);
			return;
		}
		if (!pTag) return;
		switch (nType) {
		case COLUMN_TYPE_ID + 0:
			lstrcpynW((LPWSTR)pBuf, pTag->szTitle, nBufSize);
			break;
		case COLUMN_TYPE_ID + 1:
			lstrcpynW((LPWSTR)pBuf, pTag->szArtist, nBufSize);
			break;
		case COLUMN_TYPE_ID + 2:
			lstrcpynW((LPWSTR)pBuf, pTag->szAlbum, nBufSize);
			break;
		case COLUMN_TYPE_ID + 3:
			lstrcpynW((LPWSTR)pBuf, pTag->szTrack, nBufSize);
			break;
		}
	}else{
		TAGINFOA *pTag = GetTagInfoA(pFileInfo);
		if (nType == COLUMN_TYPE_ID + 4) {
			GetTitleFnameA(pTag, pFileInfo, pBuf, nBufSize);
			return;
		}
		if (!pTag) return;
		switch (nType) {
		case COLUMN_TYPE_ID + 0:
			lstrcpynA((LPSTR)pBuf, pTag->szTitle, nBufSize);
			break;
		case COLUMN_TYPE_ID + 1:
			lstrcpynA((LPSTR)pBuf, pTag->szArtist, nBufSize);
			break;
		case COLUMN_TYPE_ID + 2:
			lstrcpynA((LPSTR)pBuf, pTag->szAlbum, nBufSize);
			break;
		case COLUMN_TYPE_ID + 3:
			lstrcpynA((LPSTR)pBuf, pTag->szTrack, nBufSize);
			break;
		}
	}
}

static int XATOIW(LPCWSTR p){
	int r = 0;
	while (*p == L' ' || *p == L'\t') p++;
	while (*p >= L'0' && *p <= L'9'){
		r = r * 10 + (*p - L'0');
		p++;
	}
	return r;
}

static int XATOIA(LPCSTR p){
	int r = 0;
	while (*p == ' ' || *p == '\t') p++;
	while (*p >= '0' && *p <= '9'){
		r = r * 10 + (*p - '0');
		p++;
	}
	return r;
}

int CALLBACK CompareColumnText(LPVOID pFileInfoLeft, LPVOID pFileInfoRight, int nColumn, int nType){
	if (lxpinfo.plxif->nUnicode) {
		TAGINFOW *pTagL = GetTagInfoW(pFileInfoLeft);
		TAGINFOW *pTagR = GetTagInfoW(pFileInfoRight);
		if (nType == COLUMN_TYPE_ID + 4) {
			WCHAR bufL[MAX_PATH];
			WCHAR bufR[MAX_PATH];
			GetTitleFnameW(pTagL, pFileInfoLeft, bufL, MAX_PATH);
			GetTitleFnameW(pTagR, pFileInfoRight, bufR, MAX_PATH);
			return lxpinfo.plxif->StrCmp(bufL, bufR);
		}
		if (!pTagL || !pTagR) return (!pTagL && !pTagR) ? 0 : ((!pTagL) ? -1 : 1);
		switch (nType) {
		case COLUMN_TYPE_ID + 0:
			return lxpinfo.plxif->StrCmp(pTagL->szTitle, pTagR->szTitle);
		case COLUMN_TYPE_ID + 1:
			return lxpinfo.plxif->StrCmp(pTagL->szArtist, pTagR->szArtist);
		case COLUMN_TYPE_ID + 2:
			return lxpinfo.plxif->StrCmp(pTagL->szAlbum, pTagR->szAlbum);
		case COLUMN_TYPE_ID + 3:
			if  (pStrCmpLogicalW)
				return pStrCmpLogicalW(pTagL->szTrack, pTagR->szTrack);
			if (XATOIW(pTagL->szTrack) > XATOIW(pTagR->szTrack)) return 1;
			if (XATOIW(pTagL->szTrack) < XATOIW(pTagR->szTrack)) return -1;
			return lxpinfo.plxif->StrCmp(pTagL->szTrack, pTagR->szTrack);
		}
	}else{
		TAGINFOA *pTagL = GetTagInfoA(pFileInfoLeft);
		TAGINFOA *pTagR = GetTagInfoA(pFileInfoRight);
		if (nType == COLUMN_TYPE_ID + 4) {
			CHAR bufL[MAX_PATH];
			CHAR bufR[MAX_PATH];
			GetTitleFnameA(pTagL, pFileInfoLeft, bufL, MAX_PATH);
			GetTitleFnameA(pTagR, pFileInfoRight, bufR, MAX_PATH);
			return lxpinfo.plxif->StrCmp(bufL, bufR);
		}
		if (!pTagL || !pTagR) return (!pTagL && !pTagR) ? 0 : ((!pTagL) ? -1 : 1);
		switch (nType) {
		case COLUMN_TYPE_ID + 0:
			return lxpinfo.plxif->StrCmp(pTagL->szTitle, pTagR->szTitle);
		case COLUMN_TYPE_ID + 1:
			return lxpinfo.plxif->StrCmp(pTagL->szArtist, pTagR->szArtist);
		case COLUMN_TYPE_ID + 2:
			return lxpinfo.plxif->StrCmp(pTagL->szAlbum, pTagR->szAlbum);
		case COLUMN_TYPE_ID + 3:
			if (XATOIA(pTagL->szTrack) > XATOIA(pTagR->szTrack)) return 1;
			if (XATOIA(pTagL->szTrack) < XATOIA(pTagR->szTrack)) return -1;
			return lxpinfo.plxif->StrCmp(pTagL->szTrack, pTagR->szTrack);
		}
	}
	return 0;
}

void CALLBACK OnSave(HWND hList, int nColumn, int nType, int nWidth){
	CHAR szSec[16];
	wsprintfA(szSec, "WidthEx%d", nType);
	lxpinfo.SetIniInt("Column", szSec, nWidth);
}

