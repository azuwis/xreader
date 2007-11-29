/*
 * $Id: IORead.cpp 74 2007-10-20 10:46:39Z soarchin $

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

#include "IORead.h"
#include <pspkernel.h>

extern "C" {
	static voidpf ZCALLBACK zopen(voidpf opaque, const char* filename, int mode)
	{
		if (filename == NULL)
			return NULL;
		FATFile* file = new FATFile();
		if(!file->Open(filename))
		{
			delete file;
			return NULL;
		}
		return file;
	}

	static uLong  ZCALLBACK zread(voidpf opaque, voidpf stream, void* buf, uLong size)
	{
		if(stream == NULL)
			return 0;
		return ((FATFile *)stream)->Read(buf, size);
	}

	static uLong  ZCALLBACK zwrite(voidpf opaque, voidpf stream, const void* buf, uLong size)
	{
		return 0;
	}

	static long   ZCALLBACK ztell(voidpf opaque, voidpf stream)
	{
		if(stream == NULL)
			return 0;
		return ((FATFile *)stream)->Tell();
	}

	static long   ZCALLBACK zseek(voidpf opaque, voidpf stream, uLong offset, int origin)
	{
		if(stream == NULL)
			return 0;
		((FATFile *)stream)->Seek(offset, (FATFile::FileSeekMode)origin);
		return 0;
	}

	static int    ZCALLBACK zclose(voidpf opaque, voidpf stream)
	{
		if(stream == NULL)
			return 0;
		((FATFile *)stream)->Close();
		delete (FATFile *)stream;
		return 0;
	}

	static int    ZCALLBACK zerror(voidpf opaque, voidpf stream)
	{
		return stream == NULL;
	}
}

zlib_filefunc_def fatiofuncdef =
{
	zopen,
	zread,
	zwrite,
	ztell,
	zseek,
	zclose,
	zerror,
	NULL
};

IOReadBase * IOReadBase::AutoOpen(CONST CHAR * filename, CONST CHAR * archive)
{
	IOReadBase * res;
	if (archive == NULL)
		res = new IOReadFile();
	else
		res = new IOReadZip();
	if(res->Open(filename, archive))
		return res;
	delete res;
	return 0;
}

IOReadFile::IOReadFile():handle(0)
{
}

IOReadFile::~IOReadFile()
{
	if(handle != 0)
	{
		delete handle;
		handle = 0;
	}
}

BOOL IOReadFile::Open(CONST CHAR * filename, CONST CHAR * archive)
{
	if(handle != 0)
		handle->Close();
	else
		handle = new FATFile();
	return handle->Open(filename);
}

VOID IOReadFile::Close()
{
	if(handle != 0)
	{
		delete handle;
		handle = 0;
	}
}

DWORD IOReadFile::Read(VOID * buf, DWORD n)
{
	if(handle != 0)
		return handle->Read(buf, n);
	return 0;
}

DWORD IOReadFile::Seek(INT offset, INT origin)
{
	if(handle != 0)
		return handle->Seek(offset, (FATFile::FileSeekMode)origin);
	return 0;
}

DWORD IOReadFile::Tell()
{
	if(handle != 0)
		return handle->Tell();
	return 0;
}

DWORD IOReadFile::GetSize()
{
	if(handle != 0)
		return handle->GetSize();
	return 0;
}

IOReadZip::IOReadZip():unzf(NULL)
{
}

IOReadZip::~IOReadZip()
{
	if(unzf != NULL)
	{
		unzCloseCurrentFile(unzf);
		unzClose(unzf);
		unzf = NULL;
	}
}

BOOL IOReadZip::Open(CONST CHAR * filename, CONST CHAR * archive)
{
	unzf = unzOpen2(archive, &fatiofuncdef);
	if(unzf == NULL)
		return FALSE;
	if(unzLocateFile(unzf, filename, 0) != UNZ_OK || unzOpenCurrentFile(unzf) != UNZ_OK)
	{
		unzClose(unzf);
		return FALSE;
	}
	return TRUE;
}

VOID IOReadZip::Close()
{
	if(unzf != NULL)
	{
		unzCloseCurrentFile(unzf);
		unzClose(unzf);
		unzf = NULL;
	}
}

DWORD IOReadZip::Read(VOID * buf, DWORD n)
{
	if(unzf != NULL)
	{
		INT size = unzReadCurrentFile(unzf, buf, n);
		if (size < 0)
			return 0;
		else
			return n;
	}
	return 0;
}

DWORD IOReadZip::Seek(INT offset, INT origin)
{
	if(unzf != NULL)
	{
		if(origin != SEEK_SET)
			return 0;
		int size = offset - unztell(unzf);
		if(size <= 0)
			return 0;
		BYTE buf[1024];
		while(size > 1024)
		{
			unzReadCurrentFile(unzf, buf, 1024);
			size -= 1024;
		}
		unzReadCurrentFile(unzf, buf, size);
		return offset;
	}
	return 0;
}

DWORD IOReadZip::Tell()
{
	if(unzf != NULL)
		return unztell(unzf);
	return 0;
}

DWORD IOReadZip::GetSize()
{
	unz_file_info info;
	if(unzGetCurrentFileInfo(unzf, &info, NULL, 0, NULL, 0, NULL, 0) != UNZ_OK)
		return 0;
	return info.uncompressed_size;
}
