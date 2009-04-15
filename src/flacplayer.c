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
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include "config.h"
#include "scene.h"
#include "xmp3audiolib.h"
#include "musicmgr.h"
#include "musicdrv.h"
#include "FLAC/stream_decoder.h"
#include "FLAC/metadata.h"
#include "strsafe.h"
#include "common/utils.h"
#include "dbg.h"
#include "ssv.h"
#include "genericplayer.h"
#include "musicinfo.h"
#include "xrhal.h"

#ifdef ENABLE_FLAC

#define NUM_AUDIO_SAMPLES (1024 * 8)

static int __end(void);

/**
 * Flac���ֲ��Ż���
 */
static uint16_t *g_buff = NULL;

/**
 * Flac���ֲ��Ż����ܴ�С, ��֡����
 */
static unsigned g_buff_size;

/**
 * Flac���ֲ��Ż����С����֡����
 */
static unsigned g_buff_frame_size;

/**
 * Flac���ֲ��Ż��嵱ǰλ�ã���֡����
 */
static int g_buff_frame_start;

/**
 * Flac����ÿ����λ��
 */
static int g_flac_bits_per_sample = 0;

/**
 * һ��Flac��������ѽ���֡������
 */
static int g_decoded_sample_size = 0;

/**
 * Flac����������
 */
static char g_encode_name[80];

/**
 * FLAC������
 */
static FLAC__StreamDecoder *g_decoder = NULL;

static FLAC__uint64 decode_position = 0;
static FLAC__uint64 last_decode_position = 0;

/**
 * �������ݵ�����������
 *
 * @note �����������ĸ�ʽΪ˫������16λ������
 *
 * @param buf ����������ָ��
 * @param srcbuf �������ݻ�����ָ��
 * @param frames ����֡��
 * @param channels ������
 */
static void send_to_sndbuf(void *buf, uint16_t * srcbuf, int frames,
						   int channels)
{
	int n;
	signed short *p = (signed short *) buf;

	if (frames <= 0)
		return;
	
	if (channels == 2) {
		memcpy(buf, srcbuf, frames * channels * sizeof(*srcbuf));
	} else {
		for (n = 0; n < frames * channels; n++) {
			*p++ = srcbuf[n];
			*p++ = srcbuf[n];
		}
	}

}

static int flac_seek_seconds(double seconds)
{
	if (g_info.duration == 0)
		return -1;

	if (seconds >= g_info.duration) {
		__end();
		return 0;
	}

	if (seconds < 0)
		seconds = 0;

	free_bitrate(&g_inst_br);

	if (!FLAC__stream_decoder_seek_absolute
		(g_decoder, g_info.samples * seconds / g_info.duration)) {
		return -1;
	}

	FLAC__StreamDecoderState state = FLAC__stream_decoder_get_state(g_decoder);

	if (state == FLAC__STREAM_DECODER_SEEK_ERROR) {
		dbg_printf(d, "Reflush the stream");
		FLAC__stream_decoder_flush(g_decoder);
	}

	g_play_time = seconds;

	if (!FLAC__stream_decoder_get_decode_position(g_decoder, &decode_position))
		decode_position = 0;

	last_decode_position = decode_position;

	return 0;
}

static FLAC__StreamDecoderWriteStatus write_callback(const FLAC__StreamDecoder *
													 decoder,
													 const FLAC__Frame * frame,
													 const FLAC__int32 *
													 const buffer[],
													 void *client_data)
{
	size_t i;

	(void) decoder;

	if (g_info.samples == 0) {
		dbg_printf(d,
				   "ERROR: this example only works for FLAC files that have a g_info.samples count in STREAMINFO");
		return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
	}
	if (g_info.channels != 2 || g_flac_bits_per_sample != 16) {
		dbg_printf(d, "ERROR: this example only supports 16bit stereo streams");
		return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
	}

	g_decoded_sample_size = frame->header.blocksize;

	if (frame->header.blocksize > g_buff_size) {
		g_buff_size = frame->header.blocksize;
		g_buff = safe_realloc(g_buff, g_buff_size * frame->header.channels * sizeof(*g_buff));

		if (g_buff == NULL)
			return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
	}

	uint16_t *output = (uint16_t*) g_buff;

	if (frame->header.channels == 2) {
		for (i = 0; i < frame->header.blocksize; i++) {
			*output++ = (FLAC__int16) buffer[0][i];
			*output++ = (FLAC__int16) buffer[1][i];
		}
	} else if (frame->header.channels == 1) {
		for (i = 0; i < frame->header.blocksize; i++) {
			*output++ = (FLAC__int16) buffer[0][i];
			*output++ = (FLAC__int16) buffer[0][i];
		}
	}

	return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}

static void metadata_callback(const FLAC__StreamDecoder * decoder,
							  const FLAC__StreamMetadata * metadata,
							  void *client_data)
{
	(void) decoder, (void) client_data;

	/* print some stats */
	if (metadata->type == FLAC__METADATA_TYPE_STREAMINFO) {
		/* save for later */
		g_info.samples = metadata->data.stream_info.total_samples;
		g_info.sample_freq = metadata->data.stream_info.sample_rate;
		g_info.channels = metadata->data.stream_info.channels;
		g_flac_bits_per_sample = metadata->data.stream_info.bits_per_sample;

		if (g_info.samples > 0) {
			g_info.duration = 1.0 * g_info.samples / g_info.sample_freq;
			g_info.avg_bps = g_info.filesize * 8 / g_info.duration;
		} else {
			g_info.duration = 0;
			g_info.avg_bps = 0;
		}

	}
}

static void error_callback(const FLAC__StreamDecoder * decoder,
						   FLAC__StreamDecoderErrorStatus status,
						   void *client_data)
{
	(void) decoder, (void) client_data;

	dbg_printf(d, "Got error callback: %s",
			   FLAC__StreamDecoderErrorStatusString[status]);
}

/**
 * Flac���ֲ��Żص�������
 * ���𽫽��������������������
 *
 * @note �����������ĸ�ʽΪ˫������16λ������
 *
 * @param buf ����������ָ��
 * @param reqn ������֡��С
 * @param pdata �û����ݣ�����
 */
static int flac_audiocallback(void *buf, unsigned int reqn, void *pdata)
{
	int avail_frame;
	int snd_buf_frame_size = (int) reqn;
	double incr;
	signed short *audio_buf = buf;

	UNUSED(pdata);

	if (g_status != ST_PLAYING) {
		if (g_status == ST_FFORWARD) {
			generic_lock();
			g_status = ST_PLAYING;
			generic_set_playback(true);
			generic_unlock();
			flac_seek_seconds(g_play_time + g_seek_seconds);
		} else if (g_status == ST_FBACKWARD) {
			generic_lock();
			g_status = ST_PLAYING;
			generic_set_playback(true);
			generic_unlock();
			flac_seek_seconds(g_play_time - g_seek_seconds);
		}
		xMP3ClearSndBuf(buf, snd_buf_frame_size);
		xrKernelDelayThread(100000);
		return 0;
	}

	while (snd_buf_frame_size > 0) {
		avail_frame = g_buff_frame_size - g_buff_frame_start;

		if (avail_frame >= snd_buf_frame_size) {
			send_to_sndbuf(audio_buf,
						   &g_buff[g_buff_frame_start * g_info.channels],
						   snd_buf_frame_size, g_info.channels);
			g_buff_frame_start += snd_buf_frame_size;
			audio_buf += snd_buf_frame_size * 2;
			snd_buf_frame_size = 0;
		} else {
			send_to_sndbuf(audio_buf,
						   &g_buff[g_buff_frame_start * g_info.channels],
						   avail_frame, g_info.channels);
			snd_buf_frame_size -= avail_frame;
			audio_buf += avail_frame * 2;

			if (!FLAC__stream_decoder_process_single(g_decoder)) {
				__end();
				return -1;
			}

			FLAC__StreamDecoderState state =
				FLAC__stream_decoder_get_state(g_decoder);

			if (state == FLAC__STREAM_DECODER_END_OF_STREAM
				|| state == FLAC__STREAM_DECODER_ABORTED
				|| state == FLAC__STREAM_DECODER_MEMORY_ALLOCATION_ERROR) {
				__end();
				return -1;
			}

			g_buff_frame_size = g_decoded_sample_size;
			g_buff_frame_start = 0;

			incr = 1.0 * g_buff_frame_size / g_info.sample_freq;

			FLAC__stream_decoder_get_bits_per_sample(g_decoder);
			g_play_time += incr;

			/* Count the bitrate */

			if (!FLAC__stream_decoder_get_decode_position
				(g_decoder, &decode_position))
				decode_position = 0;

			if (decode_position > last_decode_position) {
				int bitrate;
				int bytes_per_sec =
					g_flac_bits_per_sample / 8 * g_info.sample_freq *
					g_info.channels;

				bitrate =
					(decode_position -
					 last_decode_position) * 8.0 / (g_buff_frame_size * 4 /
													(float) bytes_per_sec);
				add_bitrate(&g_inst_br, bitrate, incr);
			}

			last_decode_position = decode_position;
		}
	}

	return 0;
}

/**
 * ��ʼ������������Դ��
 *
 * @return �ɹ�ʱ����0
 */
static int __init(void)
{
	generic_init();
	generic_lock();
	g_status = ST_UNKNOWN;
	generic_unlock();

	g_buff_frame_size = g_buff_frame_start = 0;
	g_seek_seconds = 0;
	g_play_time = 0.;
	memset(&g_info, 0, sizeof(g_info));
	g_decoder = NULL;
	g_encode_name[0] = '\0';

	return 0;
}

/**
 * װ��Flac�����ļ� 
 *
 * @param spath ��·����
 * @param lpath ��·����
 *
 * @return �ɹ�ʱ����0
 */
static int flac_load(const char *spath, const char *lpath)
{
	int fd;
	FLAC__StreamDecoderInitStatus init_status;

	__init();

	if (g_buff != NULL) {
		free(g_buff);
		g_buff = NULL;
	}

	g_buff_size = NUM_AUDIO_SAMPLES / 2;
	g_buff = calloc(g_buff_size * 2, sizeof(*g_buff));

	if (g_buff == NULL) {
		__end();
		return -1;
	}

	fd = xrIoOpen(spath, PSP_O_RDONLY, 0777);

	if (fd < 0) {
		__end();
		return -1;
	}

	g_info.filesize = xrIoLseek(fd, 0, PSP_SEEK_END);
	xrIoClose(fd);

	if ((g_decoder = FLAC__stream_decoder_new()) == NULL) {
		__end();
		return -1;
	}

	FLAC__StreamMetadata *ptag;

	if (FLAC__metadata_get_tags(spath, &ptag)) {
		if (ptag->type == FLAC__METADATA_TYPE_VORBIS_COMMENT) {
			const char *p =
				(const char *) ptag->data.vorbis_comment.vendor_string.entry;

			p = strstr(p, "reference ");

			if (p == NULL)
				p = (const char *) ptag->data.vorbis_comment.vendor_string.
					entry;
			else
				p += sizeof("reference ") - 1;

			STRCPY_S(g_encode_name, (const char *) p);

			FLAC__uint32 i;

			for (i = 0; i < ptag->data.vorbis_comment.num_comments; ++i) {
				const char *p =
					(const char *) ptag->data.vorbis_comment.comments[i].entry;
				const char *q;

				if (!strncmp(p, "ALBUM=", sizeof("ALBUM=") - 1)) {
					q = p + sizeof("ALBUM=") - 1;
					STRCPY_S(g_info.tag.album, q);
				} else if (!strncmp(p, "ARTIST=", sizeof("ARTIST=") - 1)) {
					q = p + sizeof("ARTIST=") - 1;
					STRCPY_S(g_info.tag.artist, q);
				} else if (!strncmp(p, "PERFORMER=", sizeof("PERFORMER=") - 1)) {
					q = p + sizeof("ARTIST=") - 1;
					STRCPY_S(g_info.tag.artist, q);
				} else if (!strncmp(p, "TITLE=", sizeof("TITLE=") - 1)) {
					q = p + sizeof("TITLE=") - 1;
					STRCPY_S(g_info.tag.title, q);
				}
			}

			g_info.tag.encode = conf_encode_utf8;
		}

		FLAC__metadata_object_delete(ptag);
	}
	// for speed issue
	(void) FLAC__stream_decoder_set_md5_checking(g_decoder, false);

	init_status =
		FLAC__stream_decoder_init_file(g_decoder, spath, write_callback,
									   metadata_callback, error_callback,
									   /*client_data= */ NULL);

	if (init_status != FLAC__STREAM_DECODER_INIT_STATUS_OK) {
		__end();
		return -1;
	}

	if (!FLAC__stream_decoder_process_until_end_of_metadata(g_decoder)) {
		__end();
		return -1;
	}

	if (g_info.channels != 1 && g_info.channels != 2) {
		__end();
		return -1;
	}

	if (xMP3AudioInit() < 0) {
		__end();
		return -1;
	}

	if (xMP3AudioSetFrequency(g_info.sample_freq) < 0) {
		__end();
		return -1;
	}

	generic_lock();
	g_status = ST_LOADED;
	generic_unlock();

	dbg_printf(d,
			   "[%d channel(s), %d Hz, %.2f kbps, %02d:%02d, encoder: %s, Ratio: %.3f]",
			   g_info.channels, g_info.sample_freq, g_info.avg_bps / 1000,
			   (int) (g_info.duration / 60), (int) g_info.duration % 60, g_encode_name,
			   1.0 * g_info.filesize / (g_info.samples *
										 g_info.channels *
										 (g_flac_bits_per_sample / 8))
		);

	dbg_printf(d, "[%s - %s - %s, flac tag]", g_info.tag.artist, g_info.tag.album,
			   g_info.tag.title);

	xMP3AudioSetChannelCallback(0, flac_audiocallback, NULL);

	return 0;
}

/**
 * ֹͣFlac�����ļ��Ĳ��ţ�������Դ��
 *
 * @note �����ڲ����߳��е���
 *
 * @return �ɹ�ʱ����0
 */
static int __end(void)
{
	xMP3AudioEndPre();

	generic_lock();
	g_status = ST_STOPPED;
	generic_unlock();

	g_play_time = 0.;

	return 0;
}

/**
 * ֹͣFlac�����ļ��Ĳ��ţ�������ռ�е��̡߳���Դ��
 *
 * @note �������ڲ����߳��е��ã������ܹ�����ظ����ö�������
 *
 * @return �ɹ�ʱ����0
 */
static int flac_end(void)
{
	__end();

	xMP3AudioEnd();

	if (g_buff != NULL) {
		free(g_buff);
		g_buff = NULL;
	}

	g_status = ST_STOPPED;

	if (g_decoder) {
		FLAC__stream_decoder_delete(g_decoder);
		g_decoder = NULL;
	}

	free_bitrate(&g_inst_br);
	generic_end();

	return 0;
}

/**
 * PSP׼������ʱFlac�Ĳ���
 *
 * @return �ɹ�ʱ����0
 */
static int flac_suspend(void)
{
	generic_suspend();
	flac_end();

	return 0;
}

/**
 * PSP׼��������ʱ�ָ���Flac�Ĳ���
 *
 * @param spath ��ǰ������������8.3·����ʽ
 * @param lpath ��ǰ���������������ļ�����ʽ
 *
 * @return �ɹ�ʱ����0
 */
static int flac_resume(const char *spath, const char *lpath)
{
	int ret;

	ret = flac_load(spath, lpath);
	if (ret != 0) {
		dbg_printf(d, "%s: flac_load failed %d", __func__, ret);
		return -1;
	}

	g_play_time = g_suspend_playing_time;
	flac_seek_seconds(g_play_time);
	g_suspend_playing_time = 0;

	generic_resume(spath, lpath);

	return 0;
}

/**
 * �õ�Flac�����ļ������Ϣ
 *
 * @param pinfo ��Ϣ�ṹ��ָ��
 *
 * @return
 */
static int flac_get_info(struct music_info *pinfo)
{
	if (pinfo->type & MD_GET_CURTIME) {
		pinfo->cur_time = g_play_time;
	}
	if (pinfo->type & MD_GET_CPUFREQ) {
		pinfo->psp_freq[0] = 66;
		pinfo->psp_freq[1] = 111;
	}
	if (pinfo->type & MD_GET_INSKBPS) {
		pinfo->ins_kbps = get_inst_bitrate(&g_inst_br) / 1000;
	}
	if (pinfo->type & MD_GET_DECODERNAME) {
		STRCPY_S(pinfo->decoder_name, "flac");
	}
	if (pinfo->type & MD_GET_ENCODEMSG) {
		if (show_encoder_msg) {
			SPRINTF_S(pinfo->encode_msg, "%s Ratio: %.3f", g_encode_name,
					  1.0 * g_info.filesize / (g_info.samples *
												g_info.channels *
												(g_flac_bits_per_sample / 8)));
		} else {
			pinfo->encode_msg[0] = '\0';
		}
	}

	return generic_get_info(pinfo);
}

/**
 * ����Ƿ�ΪFLAC�ļ���Ŀǰֻ����ļ���׺��
 *
 * @param spath ��ǰ������������8.3·����ʽ
 *
 * @return ��FLAC�ļ�����1�����򷵻�0
 */
static int flac_probe(const char* spath)
{
	const char *p;

	p = utils_fileext(spath);

	if (p) {
		if (stricmp(p, "flac") == 0) {
			return 1;
		}
	}

	return 0;
}

static struct music_ops flac_ops = {
	.name = "flac",
	.set_opt = NULL,
	.load = flac_load,
	.play = NULL,
	.pause = NULL,
	.end = flac_end,
	.get_status = NULL,
	.fforward = NULL,
	.fbackward = NULL,
	.suspend = flac_suspend,
	.resume = flac_resume,
	.get_info = flac_get_info,
	.probe = flac_probe,
	.next = NULL
};

int flac_init(void)
{
	return register_musicdrv(&flac_ops);
}

// Dummy function for libFLAC

int chmod(const char *path, int mode)
{
	return 0;
}

int chown(const char *path, int owner, int group)
{
	return 0;
}

int utime(const char *filename, const void *buf)
{
	return 0;
}

#endif
