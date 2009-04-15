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

#include <pspkernel.h>
#include <pspdebug.h>
#include <pspsdk.h>
#include <psputilsforkernel.h>
#include <pspdisplay_kernel.h>
#include <psploadexec_kernel.h>
#include <pspsysmem_kernel.h>
#include <pspimpose_driver.h>
#include <string.h>
#include "strsafe.h"
#include "config.h"
#include "testprx.h"
#include "musicinfo.h"

PSP_MODULE_INFO("testprx", PSP_MODULE_USER, 1, 1);
// PSP_MODULE_INFO("testprx", 0x1007, 1, 1);
PSP_MAIN_THREAD_ATTR(0);
PSP_HEAP_SIZE_KB(2 * 1024);

int test_get_version(void)
{
	return 1;
}

int test_modify_struct(struct TestStruct *p)
{
	p->cnt++;
	return 0;
}

extern void wmaprx_seek_seconds(wmadec_context *decoder, double npt)
{
	av_seek_frame(decoder->avf_context, -1, npt * AV_TIME_BASE, AVSEEK_FLAG_ANY);
	decoder->inbuf_size = 0;
	decoder->inbuf_ptr = NULL;
}

static void *safe_realloc(void *ptr, size_t size)
{
	void *p = realloc(ptr, size);

	if (p == NULL) {
		if (ptr)
			free(ptr);
		ptr = NULL;
	}

	return p;
}

/**
 * 解码WMA
 *
 * TODO move to prx
 */
extern int wmaprx_process_single(wmadec_context *decoder, PMusicInfo info, uint16_t *buf, unsigned *bufsize)
{
	wmadec_context *p_context = decoder;

	if ( p_context->inbuf_size <= 0 ) {
		if(av_read_frame(p_context->avf_context, &(p_context->pkt)) < 0) 
			return -1;
		p_context->inbuf_size = p_context->pkt.size;
		p_context->inbuf_ptr = p_context->pkt.data;
	}

	if(p_context->inbuf_size == 0) 
		return -1;

	int use_len, outbuf_size = sizeof(p_context->outbuf);

	use_len = avcodec_decode_audio2(p_context->avc_context, (short *)(p_context->outbuf), &outbuf_size,
			p_context->inbuf_ptr, p_context->inbuf_size);

	if(use_len < 0) {
		if(p_context->pkt.data)
			av_free_packet(&(p_context->pkt));
		return -1;
	}

	if(outbuf_size <= 0) {
		p_context->inbuf_size = 0;
		p_context->inbuf_ptr = NULL;
		if(p_context->pkt.data) av_free_packet(&(p_context->pkt));
		return 0;
	}

	p_context->inbuf_size -= use_len;
	p_context->inbuf_ptr += use_len;

	if(p_context->inbuf_size <= 0){
		p_context->inbuf_size = 0;
		p_context->inbuf_ptr = NULL;
		if(p_context->pkt.data) 
			av_free_packet(&(p_context->pkt));
	}

	int samplesdecoded = outbuf_size / 2 / info->channels;

	if (samplesdecoded * info->channels > *bufsize) {
		*bufsize = samplesdecoded * info->channels;
		buf = safe_realloc(buf, *bufsize * sizeof(*buf));

		if (buf == NULL)
			return -1;
	}

	memcpy(buf, p_context->outbuf, outbuf_size);

	return samplesdecoded;
}

// FIXME ffmpeg 获取的tag键名有问题
// TODO move to prx
extern void wmaprx_get_tag(wmadec_context *decoder, PMusicInfo info)
{
	info->tag.type = WMATAG;
	info->tag.encode = conf_encode_utf8;

	AVMetadataTag *tag;

	tag = av_metadata_get(decoder->avf_context->metadata, "le", NULL, AV_METADATA_IGNORE_SUFFIX);

	if (tag != NULL) {
		STRCPY_S(info->tag.title, tag->value);
	}

	tag = av_metadata_get(decoder->avf_context->metadata, "hor", NULL, AV_METADATA_IGNORE_SUFFIX);

	if (tag != NULL) {
		STRCPY_S(info->tag.artist, tag->value);
	}

	tag = av_metadata_get(decoder->avf_context->metadata, "WM/AlbumTitle", NULL, AV_METADATA_IGNORE_SUFFIX);

	if (tag != NULL) {
		STRCPY_S(info->tag.album, tag->value);
	}
}

extern void wmaprx_init()
{
	avcodec_init();
	avcodec_register_all();
	av_register_all();
}

void wmaprx_free(wmadec_context *decoder)
{
	if (decoder != NULL) {
		if(decoder->avc_context) 
			avcodec_close(decoder->avc_context);

		if(decoder->avf_context) 
			av_close_input_file(decoder->avf_context);

		free(decoder);
	}
}

extern wmadec_context *wmaprx_open(const char* spath, PMusicInfo info)
{
	wmadec_context *decoder;

	///TODO move to prx
	decoder = calloc(1, sizeof(*decoder));

	if (decoder == NULL) {
		wma_free(decoder);
		return NULL;
	}

	if(av_open_input_file(&(decoder->avf_context), spath, NULL, 0, NULL) < 0) {
		wma_free(decoder);
		return NULL;
	}

	for(decoder->wma_idx = 0; decoder->wma_idx < decoder->avf_context->nb_streams; decoder->wma_idx++) {
		decoder->avc_context = (decoder->avf_context->streams[decoder->wma_idx]->codec);
		if(decoder->avc_context->codec_type == CODEC_TYPE_AUDIO)
			break;
	}

	if ( decoder->wma_idx >= decoder->avf_context->nb_streams ) {
		wma_free(decoder);
		return NULL;
	}

	av_find_stream_info(decoder->avf_context);

	AVCodec *codec;
	codec = avcodec_find_decoder(decoder->avc_context->codec_id);
	
	if ( !codec ) {
		wma_free(decoder);
		return NULL;
	}
	
	if(avcodec_open(decoder->avc_context, codec) < 0) {
		wma_free(decoder);
		return NULL;
	}

	info->sample_freq = decoder->avc_context->sample_rate;
	info->channels = decoder->avc_context->channels;

	if (info->channels > 2 || info->channels <= 0) {
		wma_free(decoder);
		return NULL;
	}
	
	info->duration = (double) decoder->avf_context->duration / 1000000L;
	info->avg_bps = (double) decoder->avf_context->bit_rate;

	return decoder;
}

extern long long llrint(double x)
{
	return rint(x);
}

/* Entry point */
int module_start(SceSize args, void *argp)
{
	wmaprx_init();
	return 0;
}

int module_stop(SceSize args, void *argp)
{
	return 0;
}
