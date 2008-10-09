/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

//    Copyright (C) 2008 hrimfaxi
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

//
//    $Id: mpcplayer.c 39 2008-02-15 09:38:27Z hrimfaxi $
//

#pragma once

#define FILE_T                 SceUID

typedef unsigned char Uint8_t;
typedef signed long OFF_T;
typedef int Int;
typedef unsigned int Uint32_t;	// guaranteed 32 bit unsigned integer type with range 0...4294967295

typedef struct
{
	OFF_T FileSize;
	Int GenreNo;
	Int TrackNo;
	char Genre[128];
	char Year[20];
	char Track[8];
	char Title[256];
	char Artist[256];
	char Album[256];
	char Comment[512];
} TagInfo_t;

int Read_ID3V1_Tags(FILE_T fp, TagInfo_t * tip);
int Read_APE_Tags(FILE_T fp, TagInfo_t * tip);
