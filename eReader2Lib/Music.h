/*
 * $Id: Music.h 79 2007-11-07 17:13:49Z soarchin $

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

#pragma once

#include "Datatype.h"
#include "IORead.h"
#include "Thread.h"
#include <vector>
using namespace std;

class Music;
class MusicThread;

class MusicManager
{
public:
	friend class Music;
	friend class MusicThread;
	MusicManager();
	virtual ~MusicManager();
	virtual VOID Start();
	virtual VOID Play();
	virtual VOID Pause();
	virtual VOID Stop();
	virtual VOID Prev();
	virtual VOID Next();
	virtual VOID Random();
	virtual VOID SetPos(DWORD index);
	virtual INT Add(CONST CHAR * filename, BOOL unique = TRUE);
	virtual INT AddDir(CONST CHAR * dirname, BOOL unqiue = TRUE);
	virtual INT Insert(CONST CHAR * filename, DWORD index);
	virtual INT Remove(CONST CHAR * filename);
	virtual INT Remove(DWORD index);
	virtual VOID Clear();
	virtual Music * GetAt(DWORD index);
	virtual Music * GetCur();
	virtual VOID Swap(DWORD idx1, DWORD idx2);
	virtual INT GetCount();
	virtual VOID Release();
	virtual BOOL GetPause() { return m_pause; }
	virtual INT GetCurPos() { return m_curidx; }
	virtual VOID MP3Forward();
	virtual VOID MP3Backward();
	virtual VOID SetRepeat(BOOL b) { m_repeat = b; }
	virtual BOOL GetRepeat() { return m_repeat; }
	VOID PowerDown();
	VOID PowerUp();
	static VOID Init();
protected:
	INT searchForUnqiue(CONST CHAR *filename);
	typedef vector<Music *> _MusicList;
	_MusicList m_list;
	DWORD m_curidx;
	BOOL m_pause, m_stop, m_skip, m_release, m_repeat;
	MusicThread * m_thread;
	BOOL m_orgpause;
	INT64 m_orgpos;
};

class Music
{
public:
	friend class MusicThread;
	friend class MusicManager;
	STATIC Music * Create(MusicManager * manager, CONST CHAR * filename);
	Music(MusicManager * manager, CONST CHAR * filename);
	virtual ~Music();
	CONST CHAR * GetFilename() {return m_filename;}
protected:
	CHAR * m_filename;
	MusicManager * m_manager;
	ULONG m_codec_buffer[65] __attribute__((aligned(64)));
	IOReadBase * m_file;
	BOOL m_getEDRAM;
	virtual BOOL Open();
	virtual VOID Close();
	virtual VOID _ReadInfo() {}
	virtual VOID _FreeInfo() {}
	virtual VOID _PlayCycle() {}
	virtual VOID _PlayProc();
	INLINE BOOL GetSkip() {return m_manager->m_skip;}
	INLINE BOOL GetStop() {return m_manager->m_stop;}
	INLINE BOOL GetPause() {return m_manager->m_pause;}
	INLINE VOID SetStop(BOOL stop) {m_manager->m_stop = stop;}
	INLINE VOID SetPause(BOOL pause) {m_manager->m_pause = pause;}
};
