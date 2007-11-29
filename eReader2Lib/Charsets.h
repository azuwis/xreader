/*
 * $Id: Charsets.h 74 2007-10-20 10:46:39Z soarchin $

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
#include "Compat.h"
#include <string.h>

class Charsets
{
public:
	Charsets() {}
	virtual ~Charsets() {}
	DWORD ProcessStr(CONST CHAR * str, WORD *& res);
	WORD ProcessChar(CONST CHAR * str, CONST CHAR *& out);
	virtual WORD ProcessChar(CONST CHAR *& str) = 0;
	virtual INT Compare(CONST CHAR * str1, CONST CHAR * str2, INT count = 0) = 0;
	virtual INT ICompare(CONST CHAR * str1, CONST CHAR * str2, INT count = 0) = 0;
	virtual VOID Copy(CHAR * str1, CONST CHAR * str2, INT count = 0) = 0;
	virtual INT Len(CONST CHAR * str) = 0;
	virtual VOID Cat(CHAR * str1, CONST CHAR * str2, INT count = 0) = 0;
	virtual CHAR * Dup(CONST CHAR * str) = 0;
	virtual CONST CHAR * Find(CONST CHAR * str, WORD chr) = 0;
	virtual CONST CHAR * RFind(CONST CHAR * str, WORD chr) = 0;
};

class CharsetUCS: public Charsets
{
public:
	CharsetUCS() {}
	virtual ~CharsetUCS() {}
	virtual WORD ProcessChar(CONST CHAR *& str);
	virtual INT Compare(CONST CHAR * str1, CONST CHAR * str2, INT count = 0);
	virtual INT ICompare(CONST CHAR * str1, CONST CHAR * str2, INT count = 0);
	virtual VOID Copy(CHAR * str1, CONST CHAR * str2, INT count = 0);
	virtual INT Len(CONST CHAR * str);
	virtual VOID Cat(CHAR * str1, CONST CHAR * str2, INT count = 0);
	virtual CHAR * Dup(CONST CHAR * str);
	virtual CONST CHAR * Find(CONST CHAR * str, WORD chr);
	virtual CONST CHAR * RFind(CONST CHAR * str, WORD chr);
};

class CharsetUCSBE: public CharsetUCS
{
public:
	CharsetUCSBE() {}
	virtual ~CharsetUCSBE() {}
	virtual WORD ProcessChar(CONST CHAR *& str);
};

class CharsetASC: public Charsets
{
public:
	CharsetASC() {}
	virtual ~CharsetASC() {}
	virtual WORD ProcessChar(CONST CHAR *& str);
	virtual INT Compare(CONST CHAR * str1, CONST CHAR * str2, INT count = 0) {return (count > 0) ? strcmp(str1, str2) : strncmp(str1, str2, count);}
	virtual INT ICompare(CONST CHAR * str1, CONST CHAR * str2, INT count = 0) {return (count > 0) ? stricmp(str1, str2) : strnicmp(str1, str2, count);}
	virtual VOID Copy(CHAR * str1, CONST CHAR * str2, INT count = 0) {if(count > 0) strcpy(str1, str2); else strncpy(str1, str2, count);}
	virtual INT Len(CONST CHAR * str) {return strlen(str);}
	virtual VOID Cat(CHAR * str1, CONST CHAR * str2, INT count = 0) {if(count > 0) strncat(str1, str2, count); else strcat(str1, str2);}
	virtual CHAR * Dup(CONST CHAR * str) {return compat_strdup(str);}
	virtual CONST CHAR * Find(CONST CHAR * str, WORD chr) {return strchr(str, chr);}
	virtual CONST CHAR * RFind(CONST CHAR * str, WORD chr) {return strrchr(str, chr);}
};

class CharsetUTF8: public CharsetASC
{
public:
	CharsetUTF8() {}
	virtual ~CharsetUTF8() {}
	virtual WORD ProcessChar(CONST CHAR *& str);
};

class CharsetGBK: public CharsetASC
{
public:
	CharsetGBK() {}
	virtual ~CharsetGBK() {}
	virtual WORD ProcessChar(CONST CHAR *& str);
};

extern CharsetUCS UCS;
extern CharsetUCSBE UCSBE;
extern CharsetUTF8 UTF8;
extern CharsetASC ASC;
extern CharsetGBK GBK;
