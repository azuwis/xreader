#ifndef MUSIC_WRAPPER_H
#define MUSIC_WRAPPER_H
#ifdef __cplusplus
extern  "C" {
#endif
	/* 提供C调用C++接口 */
	void DirRelease(void);
	void MusicMgrInit(void);
	void *MusicMgrCreateClass(void);
	void MusicMgrDestroyClass(void* pMusicMgrClass);
	void MusicMgrStart(void *pMusicMgrClass);
	void MusicMgrPlay(void *pMusicMgrClass);
	void MusicMgrPause(void *pMusicMgrClass);
	void MusicMgrStop(void *pMusicMgrClass);
	void MusicMgrPrev(void *pMusicMgrClass);
	void MusicMgrNext(void *pMusicMgrClass);
	void MusicMgrRandom(void *pMusicMgrClass);
	void MusicMgrSetPos(void *pMusicMgrClass, dword index);
	int MusicMgrAdd(void *pMusicMgrClass, const char * filename);
	int MusicMgrAddDir(void *pMusicMgrClass, const char * dirname);
	int MusicMgrInsert(void *pMusicMgrClass, const char* filename, dword index);
	int MusicMgrRemoveByFilename(void *pMusicMgrClass, const char* filename);
	int MusicMgrRemoveByIndex(void *pMusicMgrClass, dword index);
	void MusicMgrClear(void *pMusicMgrClass);
	void *MusicMgrGetAt(void *pMusicMgrClass, dword index);
	void *MusicMgrGetCur(void *pMusicMgrClass);
	int MusicMgrGetCurPos(void *pMusicMgrClass);
	void MusicMgrSwap(void *pMusicMgrClass, dword idx1, dword idx2);
	int MusicMgrGetCount(void *pMusicMgrClass);
	void MusicMgrRelease(void *pMusicMgrClass);
	void MusicMgrPowerDown(void *pMusicMgrClass);
	void MusicMgrPowerUp(void *pMusicMgrClass);
	const char* MusicMgrGetName(void *pMusicMgrClass, dword index);
	int MusicMgrGetPause(void *pMusicMgrClass);	
	void MusicMgrForward(void *pMusicMgrClass);
	void MusicMgrBackward(void *pMusicMgrClass);
	void MusicMgrSetRepeat(void *pMusicMgrClass, int b);
	int MusicMgrGetRepeat(void *pMusicMgrClass);
#ifdef __cplusplus
}
#endif
#endif
