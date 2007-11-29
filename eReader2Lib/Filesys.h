/*
 * $Id: Filesys.h 74 2007-10-20 10:46:39Z soarchin $

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
#include <vector>
using namespace std;

struct FileInfo {
	WORD filename[256];
	CHAR shortname[16];
	DWORD filesize;
	DWORD clus;
	BYTE attr;
} __attribute__((aligned(4)));

class Dir
{
public:
	Dir();
	virtual ~Dir();
	STATIC BOOL Init();
	STATIC VOID Release();
	STATIC VOID PowerDown();
	STATIC VOID PowerUp();
	DWORD ReadDir(CONST CHAR * path, FileInfo *& info);
};

namespace eReader2space {
	class File
	{
		public:
			enum FileOpenMode {
				FO_READ,
				FO_WRITE,
				FO_CREATE,
				FO_APPEND
			};
			enum FileSeekMode {
				FS_BEGIN = 0,
				FS_CURRENT = 1,
				FS_END = 2,
			};
			File();
			virtual ~File();
			BOOL Open(CONST CHAR * filename, FileOpenMode mode);
			VOID Close();
			DWORD Read(VOID * data, DWORD size);
			DWORD Write(VOID * data, DWORD size);
			DWORD GetSize();
			DWORD Tell();
			DWORD Seek(INT pos, FileSeekMode seek);
		protected:
			INT m_handle;
	};
};

class FATFile
{
public:
	enum FileSeekMode {
		FS_BEGIN = 0,
		FS_CURRENT = 1,
		FS_END = 2,
	};
	FATFile();
	virtual ~FATFile();
	BOOL Open(CONST CHAR * filename);
	VOID Close();
	DWORD Read(VOID * data, DWORD size);
	DWORD GetSize();
	DWORD Tell();
	DWORD Seek(INT pos, FileSeekMode seek);
protected:
	vector<DWORD> m_cluslist;
	DWORD m_size, m_pos;
	BYTE * m_cache;
	BOOL m_cached;
	INT m_handle;
};
