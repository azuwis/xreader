//    xMP3
//    Copyright (C) 2008 Hrimfaxi
//    outmatch@gmail.com
//
//    This program is free software; you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation; either version 2 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program; if not, write to the Free Software
//    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

//
//    $Id$
//

/* vim:set ts=4 sw=4 cindent ignorecase enc=gbk: */

#pragma once

size_t strncpy_s(char *strDest, size_t numberOfElements, const char *strSource,
				 size_t count);
size_t strcpy_s(char *strDestination, size_t numberOfElements,
				const char *strSource);
size_t strncat_s(char *strDest, size_t numberOfElements, const char *strSource,
				 size_t count);
size_t strcat_s(char *strDestination, size_t numberOfElements,
				const char *strSource);
int snprintf_s(char *buffer, size_t sizeOfBuffer, const char *format, ...);
size_t mbcslen(const unsigned char *str);
size_t mbcsncpy_s(unsigned char *dst, size_t nBytes, const unsigned char *src,
				  size_t n);

#define NELEMS(n) (sizeof(n) / sizeof(n[0]))

#define STRCPY_S(d, s) strcpy_s((d), (sizeof(d) / sizeof(d[0])), (s))
#define STRCAT_S(d, s) strcat_s((d), (sizeof(d) / sizeof(d[0])), (s))
#define SPRINTF_S(str, fmt...) snprintf_s((str), \
							  ((sizeof(str) / sizeof(str[0]))), fmt)
#define MBCSCPY_S(dst, src, n) mbcsncpy_s((dst), (sizeof(dst) / sizeof(dst[0])), (src), (n))
