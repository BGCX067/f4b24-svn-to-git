/*
 * bass_tag.cpp
 *
 * Copyright(C) 2004-2008 Mallow <mallow at livedoor.com>
 * All Rights Reserved
 */

#include "fittle.h"
#include "bass_tag.h"
#include "readtag.h"

static BOOL Riff_ReadTag(DWORD handle, TAGINFO *pTagInfo){
	LPBYTE p = (LPBYTE)BASS_ChannelGetTags(handle, BASS_TAG_RIFF_INFO);
	while(p && *p){
		Riff_ReadFrame(p, "INAM=", pTagInfo->szTitle, 256);
		Riff_ReadFrame(p, "IART=", pTagInfo->szArtist, 256);
		Riff_ReadFrame(p, "IPRD=", pTagInfo->szAlbum, 256);
		p += lstrlenA((LPCSTR)p) + 1;
	}
	if(*pTagInfo->szTitle || *pTagInfo->szArtist) return TRUE;
	return FALSE;
}

static BOOL ID3V2_ReadTag(DWORD handle, TAGINFO *pTagInfo){
	LPBYTE p = (LPBYTE)BASS_ChannelGetTags(handle, BASS_TAG_ID3V2);
	if(p && p[0] == 'I' && p[1] == 'D' && p[2] == '3'){
		char szFrameID[5];
		unsigned nFrameSize;
		unsigned nTotal = 0;	// �w�b�_�T�C�Y�𑫂��Ă���
		unsigned nTagSize = GetSyncSafeInt(p + 6);
		unsigned nVersion = *(p + 3);
		unsigned nFlag = *(p + 5);
		LPBYTE pUnsync = NULL;
		if (nFlag & 0x80)
			p = pUnsync = Unsync(p + 10, 0, nTagSize, &nTagSize);
		else
			p += 10;
		if (!p) return FALSE;

		// �t���[����O���珇�Ɏ擾
		if(nVersion >=3){	// �o�[�W�����̎擾
			if (nFlag & 0x40) {
				/* �g���w�b�_ */
				nTotal += (nVersion == 3) ? (4 + GetNonSyncSafeInt23(p + nTotal)) : GetSyncSafeInt(p + nTotal);
			}
			while(nTotal<nTagSize){
				int nFrameFlag = 0;
				int nLenID;
				// Ver.2.3�ȏ�
				lstrcpynA(szFrameID, (LPCSTR)(p + nTotal), 5); // �t���[��ID�̎擾
				if (nVersion == 3)
					nFrameSize = GetNonSyncSafeInt23(p + nTotal + 4); // �t���[���T�C�Y�̎擾
				else
					nFrameSize = GetSyncSafeInt(p + nTotal + 4); // �t���[���T�C�Y�̎擾
				nLenID = lstrlenA(szFrameID);
				if (nLenID > 0 && nLenID < 4 && nTotal > 4){
					/* broken tag */
					lstrcpynA(szFrameID, (LPCSTR)(p + nTotal - 4 + nLenID), 5); // �t���[��ID�̎擾
					if (lstrlenA(szFrameID) == 4){
						nTotal += - 4 + nLenID;
						continue;
					}
				}
				if(nLenID !=4) break;
				if (nVersion == 4)
					nFrameFlag = p[nTotal+9] & 3;
				if(!lstrcmpA(szFrameID, "TIT2"))
					ID3V23_ReadFrame(nFrameFlag, p + nTotal + 10, nFrameSize, pTagInfo->szTitle, 256);
				else if(!lstrcmpA(szFrameID, "TPE1"))
					ID3V23_ReadFrame(nFrameFlag, p + nTotal + 10, nFrameSize, pTagInfo->szArtist, 256);
				else if(!lstrcmpA(szFrameID, "TALB"))
					ID3V23_ReadFrame(nFrameFlag, p + nTotal + 10, nFrameSize, pTagInfo->szAlbum, 256);
				else if(!lstrcmpA(szFrameID, "TRCK"))
					ID3V23_ReadFrame(nFrameFlag, p + nTotal + 10, nFrameSize, pTagInfo->szTrack, 10);

				nTotal += nFrameSize + 10;
			}
		}else{
			while(nTotal<nTagSize){
				int nFrameFlag = 0;
				// Ver.2.2�ȉ�
				lstrcpynA(szFrameID, (LPCSTR)(p + nTotal), 4); // �t���[��ID�̎擾
				nFrameSize = GetNonSyncSafeInt22(p + nTotal + 3); // �t���[���T�C�Y�̎擾
				if(lstrlenA(szFrameID)!=3) break;

				if(!lstrcmpA(szFrameID, "TP1"))
					ID3V23_ReadFrame(nFrameFlag, p + nTotal + 6, nFrameSize, pTagInfo->szArtist, 256);
				else if(!lstrcmpA(szFrameID, "TT2"))
					ID3V23_ReadFrame(nFrameFlag, p + nTotal + 6, nFrameSize, pTagInfo->szTitle, 256);
				else if(!lstrcmpA(szFrameID, "TAL"))
					ID3V23_ReadFrame(nFrameFlag, p + nTotal + 6, nFrameSize, pTagInfo->szAlbum, 256);
				else if(!lstrcmpA(szFrameID, "TRK"))
					ID3V23_ReadFrame(nFrameFlag, p + nTotal + 6, nFrameSize, pTagInfo->szTrack, 10);

				nTotal += nFrameSize + 6;
			}
		}

		if (pUnsync) HFree(pUnsync);
	}
	if(*pTagInfo->szTitle || *pTagInfo->szArtist) return TRUE;
	return FALSE;
}

static BOOL ID3V1_ReadTag(DWORD handle, TAGINFO *pTagInfo){
	ID3TAG *pID3Tag = (ID3TAG *)BASS_ChannelGetTags(handle, BASS_TAG_ID3);
	if (!pID3Tag) return FALSE;
	ID3V1_ReadFrame(pID3Tag->Title, 30, pTagInfo->szTitle, 256);
	ID3V1_ReadFrame(pID3Tag->Artist, 30, pTagInfo->szArtist, 256);
	ID3V1_ReadFrame(pID3Tag->Album, 30, pTagInfo->szAlbum, 256);
	ID3V1_ReadFrame(pID3Tag->Track, 2, pTagInfo->szTrack, 10);
	if(*pTagInfo->szTitle || *pTagInfo->szArtist) return TRUE;
	return FALSE;
}

static BOOL WMA_ReadTag(DWORD handle, TAGINFO *pTagInfo){
	LPBYTE p = (LPBYTE)BASS_ChannelGetTags(handle, BASS_TAG_WMA);
	while(p && *p){
		Vorbis_ReadFrame(p, "Title=", pTagInfo->szTitle, 256);
		Vorbis_ReadFrame(p, "Author=", pTagInfo->szArtist, 256);
		Vorbis_ReadFrame(p, "WM/AlbumTitle=", pTagInfo->szAlbum, 256);
		Vorbis_ReadFrame(p, "WM/Track=", pTagInfo->szTrack, 10);
		Vorbis_ReadFrame(p, "WM/TrackNumber=", pTagInfo->szTrack, 10);
		p += lstrlenA((LPCSTR)p) + 1;
	}
	if(*pTagInfo->szTitle || *pTagInfo->szArtist) return TRUE;
	return FALSE;
}

static BOOL ReadTagSub(LPBYTE p, TAGINFO *pTagInfo){
	while(p && *p){
		Vorbis_ReadFrame(p, "TITLE=", pTagInfo->szTitle, 256);
		Vorbis_ReadFrame(p, "ARTIST=", pTagInfo->szArtist, 256);
		Vorbis_ReadFrame(p, "ALBUM=", pTagInfo->szAlbum, 256);
		Vorbis_ReadFrame(p, "TRACKNUMBER=", pTagInfo->szTrack, 10);
		Vorbis_ReadFrame(p, "TRACKN=", pTagInfo->szTrack, 10);
		p += lstrlenA((LPCSTR)p) + 1;
	}
	if(*pTagInfo->szTitle || *pTagInfo->szArtist) return TRUE;
	return FALSE;
}

BOOL BASS_TAG_Read(DWORD handle, TAGINFO *pTagInfo){
	BASS_CHANNELINFO bcinfo = {0};
	LPCSTR p = NULL;

	ZeroMemory(pTagInfo, sizeof(TAGINFO));

	BASS_ChannelGetInfo(handle, &bcinfo);
	switch(bcinfo.ctype){
		case BASS_CTYPE_STREAM_AIFF:
		case BASS_CTYPE_STREAM_WAV:
		case BASS_CTYPE_STREAM_WAV_PCM:  
		case BASS_CTYPE_STREAM_WAV_FLOAT:  
		case BASS_CTYPE_STREAM_WAV_MP3: /* removed in BASS 2.4 */
			return Riff_ReadTag(handle, pTagInfo);

		case BASS_CTYPE_STREAM_MP1:
		case BASS_CTYPE_STREAM_MP2:
		case BASS_CTYPE_STREAM_MP3:
		case BASS_CTYPE_STREAM_APE:
		case BASS_CTYPE_STREAM_MPC:
		case BASS_CTYPE_STREAM_TTA:
		case BASS_CTYPE_STREAM_OFR:
		case BASS_CTYPE_STREAM_WV:
		case BASS_CTYPE_STREAM_WV_H:
		case BASS_CTYPE_STREAM_WV_L:
		case BASS_CTYPE_STREAM_WV_LH:
		case BASS_CTYPE_STREAM_TAK:
			/* bass_ape.dll�����Ă��ǂ�ł���Ȃ� */
			if (ReadTagSub((LPBYTE)BASS_ChannelGetTags(handle, BASS_TAG_APE), pTagInfo)) return TRUE;
			if (ID3V2_ReadTag(handle, pTagInfo)) return TRUE;
			return ID3V1_ReadTag(handle, pTagInfo);

		case BASS_CTYPE_STREAM_WMA:
		case BASS_CTYPE_STREAM_WMA_MP3:
			return WMA_ReadTag(handle, pTagInfo);

		case BASS_CTYPE_STREAM_OGG:	// Vorbis comment
		case BASS_CTYPE_STREAM_FLAC:
		case BASS_CTYPE_STREAM_FLAC_OGG:
		case BASS_CTYPE_STREAM_SPX:
			return ReadTagSub((LPBYTE)BASS_ChannelGetTags(handle, BASS_TAG_OGG), pTagInfo);

		case BASS_CTYPE_STREAM_MP4: // iTunes metadata
		case BASS_CTYPE_STREAM_ALAC:
		case BASS_CTYPE_STREAM_AAC:
			return ReadTagSub((LPBYTE)BASS_ChannelGetTags(handle, BASS_TAG_MP4), pTagInfo);

		case BASS_CTYPE_STREAM_AC3:
			return FALSE;

		case BASS_CTYPE_STREAM_CD:
			return FALSE;

		case BASS_CTYPE_STREAM:
			p = BASS_ChannelGetTags(handle, BASS_TAG_ICY);
			return FALSE;

	}
	return FALSE;
}