/*
 * $Id: Music.cpp 79 2007-11-07 17:13:49Z soarchin $

 * Copyright 2007 aeolusc

 * This file is part of eReader2.

 * eReader2 is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.

 * eReader2 is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <pspkernel.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <pspaudiocodec.h>
#include <pspaudio.h>
#include <pspsdk.h>
#include <psputility_avmodules.h>
#include <pspmpeg.h>
#include "Thread.h"
#include "Charsets.h"
#include "Filesys.h"
#include "Log.h"
#include "Utils.h"
#include "MP3.h"
#include "AA3.h"
#include "Music.h"

extern "C"
{
	int sceAudio_38553111(unsigned short samples, unsigned short freq, char);
	int sceAudio_5C37C0AE(void);
	int sceAudio_E0727056(int volume, void *buffer);
}

Music * Music::Create(MusicManager * manager, CONST CHAR * filename)
{
	CONST CHAR * strExt = GetFileExt(filename);
	if(stricmp(strExt, ".mp3") == 0)
		return new MP3(manager, filename);
	if(stricmp(strExt, ".aa3") == 0)
		return new AA3(manager, filename);
	return NULL;
}

Music::Music(MusicManager * manager, CONST CHAR * filename)
{
	m_file = NULL;
	m_manager = manager;
	m_filename = ASC.Dup(filename);
}

Music::~Music()
{
	_FreeInfo();
	if(m_filename != NULL)
		delete[] m_filename;
	if(m_file != NULL)
		delete m_file;
}

BOOL Music::Open()
{
	if(m_file != NULL)
		delete m_file;
	m_file = IOReadBase::AutoOpen(m_filename);
	if(m_file == NULL)
		return FALSE;
	_ReadInfo();

	return TRUE;
}

VOID Music::Close()
{
	_FreeInfo();
	if(m_file != NULL)
	{
		delete m_file;
		m_file = NULL;
	}
}

VOID Music::_PlayProc()
{
	memset(m_codec_buffer, 0, 65 * sizeof(DWORD));
	m_getEDRAM = FALSE;

	_PlayCycle();

	if(m_getEDRAM)
		sceAudiocodecReleaseEDRAM(m_codec_buffer);
}

class MusicThread: public Thread
{
protected:
	virtual INT ThreadEntry(INT argc, VOID * argp)
	{
		MusicManager * mman = (MusicManager *)argp;
		while(!mman->m_release)
		{
			Music * music = mman->GetCur();
			if(music == NULL)
			{
				Thread::Delay(1000000);
				continue;
			}
			music->Open();
			music->_PlayProc();
			music->Close();
			if(!mman->m_skip)
				mman->Next();
			else
				mman->m_skip = FALSE;
		}
		Exit();
		return 0;
	}
public:
	MusicThread():Thread(TRUE, "MusicThread")
	{
	}
	virtual ~MusicThread()
	{
	}
};

VOID MusicManager::Init()
{
#if _PSP_FW_VERSION >= 200
	sceUtilityLoadAvModule(PSP_AV_MODULE_AVCODEC);
	sceUtilityLoadAvModule(PSP_AV_MODULE_MPEGBASE);
	sceUtilityLoadAvModule(PSP_AV_MODULE_ATRAC3PLUS);

	sceMpegInit(); 

#else
	pspSdkLoadStartModule("flash0:/kd/me_for_vsh.prx", PSP_MEMORY_PARTITION_KERNEL);
	pspSdkLoadStartModule("flash0:/kd/audiocodec.prx", PSP_MEMORY_PARTITION_KERNEL);
#endif
}

VOID MusicManager::PowerDown()
{
	m_orgpos = -1;
	Music * music = GetCur();
	if(music == NULL)
		return;
	m_orgpause = m_pause;
	m_pause = TRUE;
	if(music->m_file != NULL)
	{
		m_orgpos = music->m_file->Tell();
		delete music->m_file;
		music->m_file = NULL;
	}
}

VOID MusicManager::PowerUp()
{
	if(m_orgpos == -1)
		return;
	Music * music = GetCur();
	if(music == NULL)
		return;
	music->m_file = IOReadBase::AutoOpen(music->m_filename);
	music->m_file->Seek(m_orgpos, FATFile::FS_BEGIN);
	m_pause = m_orgpause;
}

MusicManager::MusicManager()
{
	m_orgpos = -1;
	m_curidx = 0;
	m_pause = m_stop = m_skip = m_repeat = m_release = FALSE;
	m_thread = NULL;
}

MusicManager::~MusicManager()
{
	Release();
}

INT MusicManager::searchForUnqiue(CONST CHAR *filename)
{
	INT l = GetCount();
	for(INT i=0; i<l; ++i) {
		Music *pMusic = GetAt(i);
		if(!pMusic)
			continue;
		if(stricmp(pMusic->GetFilename(), filename) == 0)
			return i;
	}
	return -1;
}

INT MusicManager::Add(CONST CHAR * filename, BOOL unique)
{
	INT pos = searchForUnqiue(filename);
	if(unique && pos != -1)
		return pos;
	Music * music = Music::Create(this, filename);
	if(music == NULL)
		return -1;
	m_list.push_back(music);
	return m_list.size() - 1;
}

INT MusicManager::AddDir(CONST CHAR * dirname, BOOL unique)
{
	Dir dir;
	FileInfo * info;
	DWORD count = dir.ReadDir(dirname, info);
	for(DWORD i = 0; i < count; i ++)
	{
		if(info[i].shortname[0] == '.')
			continue;
		CHAR fname[256];
		strcpy(fname, dirname);
		strcat(fname, info[i].shortname);
		INT pos = searchForUnqiue(fname);
		if(unique && pos != -1)
			continue;
		Music * music = Music::Create(this, fname);
		if(music == NULL)
			continue;
		m_list.push_back(music);
	}
	if(count > 0)
		delete[] info;
	return count;
}

INT MusicManager::Insert(CONST CHAR * filename, DWORD index)
{
	if(index < m_list.size())
	{
		Music * music = Music::Create(this, filename);
		if(music == NULL)
			return -1;
		m_list.insert(m_list.begin() + index, music);
		return index;
	}
	return Add(filename);
}

INT MusicManager::Remove(CONST CHAR * filename)
{
	for(DWORD i = 0; i < m_list.size(); i ++)
	{
		if(strcasecmp(filename, m_list.at(i)->GetFilename()) == 0)
		{
			delete m_list.at(i);
			m_list.erase(m_list.begin() + i);
			return i;
		}
	}
	return -1;
}

INT MusicManager::Remove(DWORD index)
{
	if(index >= m_list.size())
		return -1;
	delete m_list.at(index);
	m_list.erase(m_list.begin() + index);
	return index;
}

VOID MusicManager::Clear()
{
	for(DWORD i = 0; i < m_list.size(); i ++)
		delete m_list.at(i);
	m_list.clear();
}

VOID MusicManager::Prev()
{
	if(m_curidx == 0)
		m_curidx = m_list.size() - 1;
	else
		m_curidx --;
	m_skip = TRUE;
}

VOID MusicManager::Next()
{
	if(m_repeat != TRUE) {
		if(m_curidx == m_list.size() - 1)
			m_curidx = 0;
		else
			m_curidx ++;
	}
	m_skip = TRUE;
}

VOID MusicManager::Random()
{
	m_curidx = rand() % m_list.size();
	m_skip = TRUE;
}

VOID MusicManager::SetPos(DWORD index)
{
	if(index >= m_list.size())
		m_curidx = m_list.size() - 1;
	else
		m_curidx = index;
	m_skip = TRUE;
}

VOID MusicManager::Play()
{
	m_pause = FALSE;
	m_stop = FALSE;
	if(m_thread == NULL) {
		Start();
	}
}

VOID MusicManager::Pause()
{
	m_pause = !m_pause;
}

VOID MusicManager::Stop()
{
	m_stop = TRUE;
}

Music * MusicManager::GetAt(DWORD index)
{
	if(index >= m_list.size())
		return NULL;
	return m_list[index];
}

Music * MusicManager::GetCur()
{
	return m_list[m_curidx];
}

VOID MusicManager::Swap(DWORD idx1, DWORD idx2)
{
	if(idx1 >= m_list.size() || idx2 >= m_list.size())
		return;
	Music * t = m_list.at(idx1);
	m_list.at(idx1) = m_list.at(idx2);
	m_list.at(idx2) = t;
}

INT MusicManager::GetCount()
{
	return m_list.size();
}

VOID MusicManager::Start()
{
	if(m_list.size() > 0)
	{
		if(m_thread == NULL)
			m_thread = new MusicThread();
		m_thread->Start(0, this);
	}
}

VOID MusicManager::Release()
{
	if(m_thread != NULL)
	{
		m_skip = TRUE;
		m_release = TRUE;
		m_thread->WaitEnd(INVALID);
		delete m_thread;
		m_thread = NULL;
		Clear();
	}
}

//TODO
VOID MusicManager::MP3Forward()
{
/*
	MP3 *mp3 = (MP3*)m_list[m_curidx];
	if(mp3)
		mp3->jump = 1;
		*/
}

//TODO
VOID MusicManager::MP3Backward()
{
	/*
	MP3 *mp3 = (MP3*)m_list[m_curidx];
	if(mp3)
		mp3->jump = -1;
		*/
}
