/*
 * $Id: Utils.cpp 74 2007-10-20 10:46:39Z soarchin $

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

#include "Utils.h"
#include "Display.h"
#include <string.h>

INLINE BYTE HexToByte(CHAR c)
{
	if(c >= '0' && c <= '9')
		return c - '0';
	if(c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	if(c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	return 0;
}

DWORD ColorStrToPixel(CONST CHAR * str)
{
	if(str[0] != '#')
		return 0;
	INT len = strlen(str);
	BYTE r = 0, g = 0, b = 0, a = 0xFF;
	if(len > 2)
		r = (HexToByte(str[1]) << 4) + HexToByte(str[2]);
	if(len > 4)
		g = (HexToByte(str[3]) << 4) + HexToByte(str[4]);
	if(len > 6)
		b = (HexToByte(str[5]) << 4) + HexToByte(str[6]);
	if(len > 8)
		a = (HexToByte(str[7]) << 4) + HexToByte(str[8]);
	return RGBA(r,g,b,a);
}

CHAR * StrProcessLine(CHAR *& str)
{
	if(str[0] == 0)
		return 0;
	CHAR * pstr = str;
	while(* str != '\r' && * str != '\n' && * str != 0)
	{
		str ++;
	}
	if(* str != 0)
	{
		* str = 0;
		str ++;
		while(* str == '\r' || * str == '\n')
			str ++;
	}

	return pstr;
}

CONST CHAR * GetFileExt(CONST CHAR * filename)
{
	CONST CHAR * strExt = strrchr(filename, '.');
	if(strExt == NULL)
		return "";
	else
		return strExt;
}
