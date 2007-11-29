/*
 * $Id: IORead.h 74 2007-10-20 10:46:39Z soarchin $

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

#include "Filesys.h"
#include "Datatype.h"
#include "unzip.h"

class IOReadBase
{
public:
	IOReadBase() {}
	virtual ~IOReadBase() {}
	static IOReadBase * AutoOpen(CONST CHAR * filename, CONST CHAR * archive = NULL);
	virtual BOOL Open(CONST CHAR * filename, CONST CHAR * archive = NULL) = 0;
	virtual VOID Close() = 0;
	virtual DWORD Read(VOID * buf, DWORD n) = 0;
	virtual DWORD Seek(INT offset, INT origin) = 0;
	virtual DWORD Tell() = 0;
	virtual DWORD GetSize() = 0;
};

class IOReadFile: public IOReadBase
{
public:
	IOReadFile();
	virtual ~IOReadFile();
	virtual BOOL Open(CONST CHAR * filename, CONST CHAR * archive = NULL);
	virtual VOID Close();
	virtual DWORD Read(VOID * buf, DWORD n);
	virtual DWORD Seek(INT offset, INT origin);
	virtual DWORD Tell();
	virtual DWORD GetSize();
private:
	FATFile * handle;
};

class IOReadZip: public IOReadBase
{
public:
	IOReadZip();
	virtual ~IOReadZip();
	virtual BOOL Open(CONST CHAR * filename, CONST CHAR * archive = NULL);
	virtual VOID Close();
	virtual DWORD Read(VOID * buf, DWORD n);
	virtual DWORD Seek(INT offset, INT origin);
	virtual DWORD Tell();
	virtual DWORD GetSize();
private:
	unzFile unzf;
};

struct IOReadStruct
{
	IOReadBase * iorc;
};
