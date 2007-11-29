/*
 * $Id: MP3.h 80 2007-11-07 17:19:12Z soarchin $

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
#include "Music.h"

class MP3: public Music
{
public:
	struct _MP3FrameInfo
	{
		DWORD frameoff;
		DWORD framesize;
	};
	struct MP3Info
	{
		CHAR artist[512];
		CHAR title[512];
		INT level;
		INT bitrate;
		INT samplerate;
		INT channels;
		DWORD length;
		DOUBLE framelen;
		DWORD maxframesize;
		vector<_MP3FrameInfo> frames;
	};
	MP3(MusicManager * manager, CONST CHAR * filename);
	virtual ~MP3();
	VOID GetInfo(MP3Info& info);
protected:
	MP3Info m_info;
	INLINE INT _CalcFrameSize(BYTE h1, BYTE h2, BYTE h3, INT& mpl, INT& br, INT& sr, INT& chs);
	INLINE VOID _ReadID3Tag(BYTE * buf, WORD * tag, INT tsize);
	virtual VOID _ReadInfo();
	virtual VOID _FreeInfo();
	virtual VOID _PlayCycle();
};
