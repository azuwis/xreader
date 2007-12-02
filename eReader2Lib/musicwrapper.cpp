#include <pspkernel.h>
#include <malloc.h>
#include <pspaudiocodec.h>
#include <pspaudio.h>
#include <vector>
#include "MP3.h"
#include "Music.h"
typedef DWORD dword;
#include "musicwrapper.h"

extern "C" {
	void DirRelease(void)
	{
		Dir::Release();
	}

	void *MusicMgrCreateClass(void)
	{
		MusicManager *pNew = new MusicManager();
		return pNew;
	}

	void MusicMgrDestroyClass(void* pMusicMgrClass)
	{
		MusicManager *pMgr = (MusicManager*) pMusicMgrClass;
		delete pMgr;
	}

	void MusicMgrStart(void *pMusicMgrClass)
	{
		MusicManager *pMgr = (MusicManager*) pMusicMgrClass;
		pMgr->Start();
	}

	void MusicMgrPlay(void *pMusicMgrClass)
	{
		MusicManager *pMgr = (MusicManager*) pMusicMgrClass;
		pMgr->Play();
	}

	void MusicMgrPause(void *pMusicMgrClass)
	{
		MusicManager *pMgr = (MusicManager*) pMusicMgrClass;
		pMgr->Pause();
	}

	void MusicMgrStop(void *pMusicMgrClass)
	{
		MusicManager *pMgr = (MusicManager*) pMusicMgrClass;
		pMgr->Stop();
	}

	void MusicMgrPrev(void *pMusicMgrClass)
	{
		MusicManager *pMgr = (MusicManager*) pMusicMgrClass;
		pMgr->Prev();
	}

	void MusicMgrNext(void *pMusicMgrClass)
	{
		MusicManager *pMgr = (MusicManager*) pMusicMgrClass;
		pMgr->Next();
	}

	void MusicMgrRandom(void *pMusicMgrClass)
	{
		MusicManager *pMgr = (MusicManager*) pMusicMgrClass;
		pMgr->Random();
	}

	void MusicMgrSetPos(void *pMusicMgrClass, dword index)
	{
		MusicManager *pMgr = (MusicManager*) pMusicMgrClass;
		pMgr->SetPos(index);
	}

	int MusicMgrAdd(void *pMusicMgrClass, const char * filename)
	{
		MusicManager *pMgr = (MusicManager*) pMusicMgrClass;
		return pMgr->Add(filename);
	}

	int MusicMgrAddDir(void *pMusicMgrClass, const char * dirname)
	{
		MusicManager *pMgr = (MusicManager*) pMusicMgrClass;
		return pMgr->AddDir(dirname);
	}

	int MusicMgrInsert(void *pMusicMgrClass, const char* filename, dword index)
	{
		MusicManager *pMgr = (MusicManager*) pMusicMgrClass;
		return pMgr->Insert(filename, index);
	}

	int MusicMgrRemoveByFilename(void *pMusicMgrClass, const char* filename)
	{
		MusicManager *pMgr = (MusicManager*) pMusicMgrClass;
		return pMgr->Remove(filename);
	}
	
	int MusicMgrRemoveByIndex(void *pMusicMgrClass, dword index)
	{
		MusicManager *pMgr = (MusicManager*) pMusicMgrClass;
		return pMgr->Remove(index);
	}

	void MusicMgrClear(void *pMusicMgrClass)
	{
		MusicManager *pMgr = (MusicManager*) pMusicMgrClass;
		pMgr->Clear();
	}

	void *MusicMgrGetAt(void *pMusicMgrClass, dword index)
	{
		MusicManager *pMgr = (MusicManager*) pMusicMgrClass;
		return (void*) pMgr->GetAt(index);
	}

	void *MusicMgrGetCur(void *pMusicMgrClass)
	{
		MusicManager *pMgr = (MusicManager*) pMusicMgrClass;
		return (void*) pMgr->GetCur();
	}

	void MusicMgrSwap(void *pMusicMgrClass, dword idx1, dword idx2)
	{
		MusicManager *pMgr = (MusicManager*) pMusicMgrClass;
		pMgr->Swap(idx1, idx2);
	}

	int MusicMgrGetCount(void *pMusicMgrClass)
	{
		MusicManager *pMgr = (MusicManager*) pMusicMgrClass;
		return pMgr->GetCount();
	}

	void MusicMgrRelease(void *pMusicMgrClass)
	{
		MusicManager *pMgr = (MusicManager*) pMusicMgrClass;
		pMgr->Release();
	}

	void MusicMgrPowerDown(void *pMusicMgrClass)
	{
		MusicManager *pMgr = (MusicManager*) pMusicMgrClass;
		pMgr->PowerDown();
	}

	void MusicMgrPowerUp(void *pMusicMgrClass)
	{
		MusicManager *pMgr = (MusicManager*) pMusicMgrClass;
		pMgr->PowerUp();
	}

	void MusicMgrInit(void)
	{
		Dir::Init();
		MusicManager::Init();
	}

	const char* MusicMgrGetName(void *pMusicMgrClass, dword index)
	{
		MusicManager *pMgr = (MusicManager*) pMusicMgrClass;
		Music *pMusic = pMgr->GetAt(index);
		if(!pMusic)
			return NULL;
		return pMusic->GetFilename();
	}

	int MusicMgrGetPause(void *pMusicMgrClass)
	{
		MusicManager *pMgr = (MusicManager*) pMusicMgrClass;		
		if(pMgr)
			return pMgr->GetPause();
		else
			return 0;
	}

	int MusicMgrGetCurPos(void *pMusicMgrClass)
	{
		MusicManager *pMgr = (MusicManager*) pMusicMgrClass;		
		if(pMgr)
			return pMgr->GetCurPos();
		else
			return 0;
	}
	

	void MusicMgrForward(void *pMusicMgrClass)
	{
		MusicManager *pMgr = (MusicManager*) pMusicMgrClass;		
		if(pMgr)
			return pMgr->MP3Forward();
	}

	void MusicMgrBackward(void *pMusicMgrClass)
	{
		MusicManager *pMgr = (MusicManager*) pMusicMgrClass;		
		if(pMgr)
			return pMgr->MP3Backward();
	}

	void MusicMgrSetRepeat(void *pMusicMgrClass, int b)
	{
		MusicManager *pMgr = (MusicManager*) pMusicMgrClass;		
		if(pMgr)
			pMgr->SetRepeat(b);
	}

	int MusicMgrGetRepeat(void *pMusicMgrClass)
	{
		MusicManager *pMgr = (MusicManager*) pMusicMgrClass;		
		if(pMgr)
			return pMgr->GetRepeat();
		return 0;
	}
	
}
