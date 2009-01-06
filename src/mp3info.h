/*
 * This file is part of xReader.
 *
 * Copyright (C) 2008 hrimfaxi (outmatch@gmail.com)
 *
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

#ifndef MP3INFO_H
#define MP3INFO_H

#include "musictag.h"

#define LB_CONV(x)	\
    (((x) & 0xff)<<24) |  \
    (((x>>8) & 0xff)<<16) |  \
    (((x>>16) & 0xff)<< 8) |  \
    (((x>>24) & 0xff)    )

#define MKBETAG(a,b,c,d) (d | (c << 8) | (b << 16) | (a << 24))

#define UNUSED(x) ((void)(x))
#define MODE_EXT_I_STEREO   1
#define MODE_EXT_MS_STEREO   2
#define MPA_MONO   3

typedef struct reader_data_t
{
	SceUID fd;
	long size;
} mp3_reader_data;

struct MP3Info
{
	bool is_mpeg1or2;
	uint32_t frames;
	int spf;
	int channels;
	int sample_freq;
	double duration;
	double average_bitrate;
	struct MusicTag tag;
	dword *frameoff;
};

int read_mp3_info(struct MP3Info *info, mp3_reader_data * data);
int read_mp3_info_brute(struct MP3Info *info, mp3_reader_data * data);
int free_mp3_info(struct MP3Info *info);
int search_valid_frame_me(mp3_reader_data * data);
int read_id3v2_tag(int fd, struct MP3Info *info);

#endif
