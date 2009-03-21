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

#include "common/datatype.h"
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <pspkernel.h>
#include <malloc.h>
#include <stdlib.h>
#include "strsafe.h"
#include "musicdrv.h"
#include "mp3info.h"
#include "common/utils.h"
#include "crc16.h"
#include "charsets.h"
#include "dbg.h"

typedef int64_t offset_t;

#define ID3v2_HEADER_SIZE 10

static int id3v2_match(const uint8_t * buf)
{
	return buf[0] == 'I' &&
		buf[1] == 'D' &&
		buf[2] == '3' &&
		buf[3] != 0xff &&
		buf[4] != 0xff &&
		(buf[6] & 0x80) == 0 &&
		(buf[7] & 0x80) == 0 && (buf[8] & 0x80) == 0 && (buf[9] & 0x80) == 0;
}

static int _bitrate[9][16] = {
	{0, 32, 64, 96, 128, 160, 192, 224, 256, 288, 320, 352, 384, 416, 448,
	 0},
	{0, 32, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 384, 0},
	{0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 0},
	{0, 32, 48, 56, 64, 80, 96, 112, 128, 144, 160, 176, 192, 224, 256, 0},
	{0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, 0},
	{0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, 0},
	{0, 32, 48, 56, 64, 80, 96, 112, 128, 144, 160, 176, 192, 224, 256, 0},
	{0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, 0},
	{0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, 0}
};

static int _sample_freq[3][4] = {
	{44100, 48000, 32000, 0},
	{22050, 24000, 16000, 0},
	{11025, 12000, 8000, 0}
};

static int check_bc_combination(int bitrate, uint8_t channel_mode)
{
	if (bitrate == 64 || bitrate == 96 || bitrate == 112 || bitrate == 128
		|| bitrate == 160 || bitrate == 192)
		return 0;

	if (channel_mode == 0 || channel_mode == 1 || channel_mode == 2) {
		if (bitrate == 224 || bitrate == 256 || bitrate == 320
			|| bitrate == 384)
			return 0;
	}

	if (channel_mode == 3) {
		if (bitrate == 32 || bitrate == 48 || bitrate == 56 || bitrate == 80)
			return 0;
	}

	return -1;
}

static int calc_crc(buffered_reader_t * reader, size_t nbytes,
					uint16_t * crc_value)
{
	uint8_t *bytes;

	bytes = malloc(nbytes);

	if (bytes == NULL)
		return -1;

	if (buffered_reader_read(reader, bytes, nbytes) != nbytes) {
		free(bytes);
		return -1;
	}

	crc16(bytes, nbytes, crc_value);
	free(bytes);

	return 0;
}

static inline int parse_frame(uint8_t * h, int *lv, int *br,
							  struct MP3Info *info, mp3_reader_data * data,
							  offset_t start)
{
	uint8_t mp3_version, mp3_level;
	uint8_t layer;
	uint8_t crc;
	uint8_t bitrate_bit;
	uint8_t pad;
	uint8_t channel_mode;
	uint32_t framelenbyte;
	int bitrate;
	int freq;
	uint16_t mp3_samples_per_frames[9] =
		{ 384, 1152, 1152, 384, 1152, 576, 384, 1152, 576 };

	if (h[0] != 0xff)
		return -1;

	switch ((h[1] >> 3) & 0x03) {
		case 0:
			mp3_version = 2;	// MPEG 2.5
			break;
		case 2:
			mp3_version = 1;	// MPEG 2
			break;
		case 3:
			mp3_version = 0;	// MPEG 1
			break;
		default:
			mp3_version = 0;
			break;
	}

	mp3_level = 3 - ((h[1] >> 1) & 0x03);
	if (mp3_level == 3)
		mp3_level--;

	info->spf = mp3_samples_per_frames[mp3_version * 3 + mp3_level];

	layer = 4 - ((h[1] >> 1) & 3);

	if (layer < 1 || layer > 3)
		return -1;

	crc = !(h[1] & 1);
	bitrate_bit = (h[2] >> 4) & 0xf;
	bitrate = _bitrate[mp3_version * 3 + mp3_level][bitrate_bit];

	if (bitrate == 0)
		return -1;

	freq = _sample_freq[mp3_version][(h[2] >> 2) & 0x03];

	if (freq == 0) {
		return -1;
	}

	pad = (h[2] >> 1) & 1;
	channel_mode = (h[3] >> 6) & 3;

	if (layer == 2 && check_bc_combination(bitrate, channel_mode) != 0)
		return -1;

	if (info->check_crc && crc) {
		uint16_t crc_value, crc_frame;
		offset_t offset;

		crc_value = 0xffff;
		offset = buffered_reader_position(data->r);
		buffered_reader_seek(data->r, start);
		crc16((uint8_t *) & h[2], 2, &crc_value);
		buffered_reader_seek(data->r, 4 + buffered_reader_position(data->r));

		if (buffered_reader_read(data->r, &crc_frame, sizeof(crc_frame)) !=
			sizeof(crc_frame)) {
			buffered_reader_seek(data->r, offset);
			return -1;
		}

		crc_frame = crc_frame >> 8 | crc_frame << 8;

		switch (layer) {
			case 1:
				if (channel_mode == 3) {
					if (calc_crc(data->r, 128 / 8, &crc_value) < 0) {
						goto failed;
					}
				} else {
					if (calc_crc(data->r, 256 / 8, &crc_value) < 0) {
						goto failed;
					}
				}
				break;
			case 2:
				// TODO
				goto failed;
				break;
			case 3:
				if (channel_mode == 3) {
					if (calc_crc(data->r, 136 / 8, &crc_value) < 0) {
						goto failed;
					}
				} else {
					if (calc_crc(data->r, 256 / 8, &crc_value) < 0) {
						goto failed;
					}
				}
				break;
		}

		if (crc_value != crc_frame) {
			dbg_printf(d, "Checking crc failed: 0x%04x 0x%04x", crc_value,
					   crc_frame);
		  failed:
			buffered_reader_seek(data->r, offset);
			return -1;
		}

		buffered_reader_seek(data->r, offset);
		info->have_crc = true;
	}

	if (*lv == 0)
		*lv = layer;

	*br = bitrate;

	if (info->sample_freq == 0)
		info->sample_freq = freq;

	if (info->channels == 0) {
		if (channel_mode == 3)
			info->channels = 1;
		else
			info->channels = 2;
	}

	if (mp3_level == 0)
		framelenbyte = ((info->spf / 8 * 1000) * bitrate / freq + pad) << 2;
	else
		framelenbyte = (info->spf / 8 * 1000) * bitrate / freq + pad;

	return framelenbyte;
}

int skip_id3v2_tag_buffered(mp3_reader_data * data)
{
	int len;
	uint8_t buf[ID3v2_HEADER_SIZE];

	if (buffered_reader_read(data->r, buf, sizeof(buf)) != sizeof(buf)) {
		return -1;
	}

	if (id3v2_match(buf)) {
		struct MP3Info info;

		memset(&info, 0, sizeof(info));
		/* parse ID3v2 header */
		len = ((buf[6] & 0x7f) << 21) |
			((buf[7] & 0x7f) << 14) | ((buf[8] & 0x7f) << 7) | (buf[9] & 0x7f);
		buffered_reader_seek(data->r, buffered_reader_position(data->r) + len);
	} else {
		buffered_reader_seek(data->r, 0);
	}

	return 0;
}

int search_valid_frame_me_buffered(mp3_reader_data * data, int *brate)
{
	uint32_t off, start, dcount = 0;
	int size = 0;
	int end;
	int level;
	uint8_t buf[1024 + 4];
	struct MP3Info inf, *info;

	info = &inf;
	memset(&inf, 0, sizeof(inf));

	level = info->sample_freq = info->channels = info->frames = 0;
	off = buffered_reader_position(data->r);

	if (buffered_reader_read(data->r, buf, 4) != 4) {
		return -1;
	}

	start = 0;
	while ((end = buffered_reader_read(data->r, &buf[4], sizeof(buf) - 4)) > 0) {
		while (start < end) {
			size =
				parse_frame(buf + start, &level, brate, info, data,
							dcount * (sizeof(buf) - 4) + off + start);

			if (size > 0) {
				goto found;
				break;
			}

			start++;
		}

		start -= end;
		memmove(buf, &buf[end], 4);
		dcount++;
	}

	if (end <= 0) {
		return -1;
	}

  found:
	buffered_reader_seek(data->r, dcount * (sizeof(buf) - 4) + off + start);

	return size;
}

