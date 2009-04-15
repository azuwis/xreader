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

#ifndef TEST_PRX_H
#define TEST_PRX_H

#include <pspkernel.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <math.h>
#include "config.h"
#include "ffmpeg/avcodec.h"
#include "ffmpeg/avformat.h"
#include "ffmpeg/metadata.h"

#define WMA_MAX_BUF_SIZE 12288

struct TestStruct {
	int cnt;
};

int test_get_version(void);
int test_modify_struct(struct TestStruct *p);

typedef struct wmadec_context {
	AVCodecContext *avc_context ;
	AVFormatContext *avf_context ;
	int wma_idx;
	AVPacket pkt;
	uint8_t *inbuf_ptr;
	int inbuf_size;
	char outbuf[AVCODEC_MAX_AUDIO_FRAME_SIZE];
} wmadec_context;

void wmaprx_seek_seconds(wmadec_context *decoder, double npt);

#endif
