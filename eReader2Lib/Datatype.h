/*
 * $Id: Datatype.h 74 2007-10-20 10:46:39Z soarchin $

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

#ifndef BYTE
typedef unsigned char BYTE;
#endif

#ifndef WORD
typedef unsigned short WORD;
#endif

#ifndef DWORD
typedef unsigned int DWORD;
#endif

#ifndef INT
typedef int INT;
#endif

#ifndef UINT
typedef unsigned int UINT;
#endif

#ifndef LONG
typedef long LONG;
#endif

#ifndef ULONG
typedef unsigned long ULONG;
#endif

#ifndef INT64
typedef long long INT64;
#endif

#ifndef UINT64
typedef unsigned long long UINT64;
#endif

#ifndef CHAR
typedef char CHAR;
#endif

#ifndef UCHAR
typedef unsigned char UCHAR;
#endif

#ifndef SHORT
typedef short SHORT;
#endif

#ifndef USHORT
typedef unsigned short USHORT;
#endif

#ifndef SINGLE
typedef float SINGLE;
#endif

#ifndef DOUBLE
typedef double DOUBLE;
#endif

#ifndef VOID
typedef void VOID;
#endif

#ifndef BOOL
#ifdef __cplusplus
typedef bool BOOL;
#else
typedef int BOOL;
#endif
#endif

#ifndef TRUE
#ifdef __cplusplus
#define TRUE true
#else
#define TRUE 1
#endif
#endif

#ifndef FALSE
#ifdef __cplusplus
#define FALSE false
#else
#define FALSE 0
#endif
#endif

#ifndef NULL
#define NULL 0
#endif

#ifndef null
#define null 0
#endif

#ifndef MIN
#define MIN(a,b) (((a)<(b))?(a):(b))
#endif

#ifndef MAX
#define MAX(a,b) (((a)<(b))?(a):(b))
#endif

#ifndef INVALID
#define INVALID ((DWORD)-1)
#endif

#ifndef CONST
#define CONST const
#endif

#ifndef STATIC
#define STATIC static
#endif

#ifndef INLINE
#define INLINE __inline
#endif

#ifndef PACKED_ATTR
#define PACKED_ATTR __attribute__((packed))
#endif
