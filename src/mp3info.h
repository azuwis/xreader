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

#ifndef _MP3INFO_H_
#define _MP3INFO_H_

#include "common/datatype.h"
#include <mad.h>

typedef struct
{
	char artist[256];
	char title[256];
	int level;
	int bitrate;
	int samplerate;
	dword length;
	double framelen;
	int framecount;
	dword *frameoff;
	bool found_apetag;
	bool found_id3v2;
	bool found_id3v1;
} t_mp3info, *p_mp3info;

extern void mp3info_init(p_mp3info info);
extern bool mp3info_read(p_mp3info info, int fd);
extern void mp3info_free(p_mp3info info);
extern void readAPETag(p_mp3info pInfo, const char *filename);
extern void readID3Tag(p_mp3info pInfo, const char *filename);

#endif
