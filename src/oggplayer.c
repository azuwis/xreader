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
#include "ssv.h"
#include "scene.h"
#include "xaudiolib.h"
#include "musicmgr.h"
#include "musicdrv.h"
#include "strsafe.h"
#include "common/utils.h"
#include "genericplayer.h"
#include "musicinfo.h"
#include "tremor/ivorbisfile.h"
#include "dbg.h"
#include "xrhal.h"
#ifdef DMALLOC
#include "dmalloc.h"
#endif

#ifdef ENABLE_OGG

#define OGG_BUFF_SIZE (6400 * 4)

static int __end(void);

/**
 * OGG���ֲ��Ż���
 */
static uint16_t *g_buff = NULL;

/**
 * OGG���ֲ��Ż����ܴ�С, ��֡����
 */
static unsigned g_buff_size;

/**
 * OGG���ֲ��Ż����С����֡����
 */
static unsigned g_buff_frame_size;

/**
 * OGG���ֲ��Ż��嵱ǰλ�ã���֡����
 */
static int g_buff_frame_start;

/**
 * OGG������
 */
static OggVorbis_File *decoder = NULL;

static char g_vendor_str[80];

/**
 * OGG�ļ����
 */
static buffered_reader_t *g_ogg_reader = NULL;

static size_t ovcb_read(void *ptr, size_t size, size_t nmemb, void *datasource);
static int ovcb_seek(void *datasource, int64_t offset, int whence);
static int ovcb_close(void *datasource);
static long ovcb_tell(void *datasource);

static ov_callbacks vorbis_callbacks = {
	ovcb_read,
	ovcb_seek,
	ovcb_close,
	ovcb_tell
};

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

static void ogg_seek_seconds(OggVorbis_File * decoder, double npt)
{
	if (decoder)
		ov_time_seek(decoder, npt * 1000);
}

/**
 * ����Ogg
 */
static int ogg_process_single(void)
{
	int samplesdecoded;
	int bytes;
	int current_section;

	bytes =
		ov_read(decoder, (char *) g_buff, g_buff_size * sizeof(g_buff[0]),
				&current_section);

	switch (bytes) {
		case 0:
			/* EOF */
			return -1;
			break;

		case OV_HOLE:
		case OV_EBADLINK:
			/*
			 * error in the stream.  Not a problem, just
			 * reporting it in case we (the app) cares.
			 * In this case, we don't.
			 */
			return 0;
			break;
	}

	samplesdecoded = bytes / 2 / g_info.channels;

	g_buff_frame_size = samplesdecoded;
	g_buff_frame_start = 0;

	return samplesdecoded;
}

/**
 * OGG���ֲ��Żص�������
 * ���𽫽��������������������
 *
 * @note �����������ĸ�ʽΪ˫������16λ������
 *
 * @param buf ����������ָ��
 * @param reqn ������֡��С
 * @param pdata �û����ݣ�����
 */
static int ogg_audiocallback(void *buf, unsigned int reqn, void *pdata)
{
	int avail_frame;
	int snd_buf_frame_size = (int) reqn;
	int ret;
	double incr;
	signed short *audio_buf = buf;

	UNUSED(pdata);

	if (g_status != ST_PLAYING) {
		if (g_status == ST_FFORWARD) {
			g_play_time += g_seek_seconds;
			if (g_play_time >= g_info.duration) {
				__end();
				return -1;
			}
			generic_lock();
			g_status = ST_PLAYING;
			generic_set_playback(true);
			generic_unlock();
			free_bitrate(&g_inst_br);
			ogg_seek_seconds(decoder, g_play_time);
			g_buff_frame_size = g_buff_frame_start = 0;
		} else if (g_status == ST_FBACKWARD) {
			g_play_time -= g_seek_seconds;
			if (g_play_time < 0.) {
				g_play_time = 0.;
			}
			generic_lock();
			g_status = ST_PLAYING;
			generic_set_playback(true);
			generic_unlock();
			free_bitrate(&g_inst_br);
			ogg_seek_seconds(decoder, g_play_time);
			g_buff_frame_size = g_buff_frame_start = 0;
		}
		xAudioClearSndBuf(buf, snd_buf_frame_size);
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

			ret = ogg_process_single();

			if (ret == -1) {
				__end();
				return -1;
			}

			incr = (double) ret / g_info.sample_freq;
			g_play_time += incr;
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
	decoder = NULL;
	g_ogg_reader = NULL;
	g_buff = NULL;
	g_buff_size = 0;
	memset(g_vendor_str, 0, sizeof(g_vendor_str));

	return 0;
}

static void get_ogg_tag(OggVorbis_File * decoder)
{
	int i;

	vorbis_comment *comment = ov_comment(decoder, -1);

	if (comment == NULL) {
		return;
	}

	if (comment->vendor) {
		STRCPY_S(g_vendor_str, comment->vendor);
	} else {
		STRCPY_S(g_vendor_str, "");
	}

	for (i = 0; i < comment->comments; i++) {
		dbg_printf(d, "%s: %s", __func__, comment->user_comments[i]);

		if (!strnicmp
			(comment->user_comments[i], "TITLE=", sizeof("TITLE=") - 1)) {
			STRCPY_S(g_info.tag.title,
					 comment->user_comments[i] + sizeof("TITLE=") - 1);
		} else if (!strnicmp
				   (comment->user_comments[i], "ALBUM=",
					sizeof("ALBUM=") - 1)) {
			STRCPY_S(g_info.tag.album,
					 comment->user_comments[i] + sizeof("ALBUM=") - 1);
		} else
			if (!strnicmp
				(comment->user_comments[i], "ARTIST=", sizeof("ARTIST=") - 1)) {
			STRCPY_S(g_info.tag.artist,
					 comment->user_comments[i] + sizeof("ARTIST=") - 1);
		} else
			if (!strnicmp
				(comment->user_comments[i], "COMMENT=",
				 sizeof("COMMENT=") - 1)) {
			STRCPY_S(g_info.tag.comment,
					 comment->user_comments[i] + sizeof("COMMENT=") - 1);
		}
	}

	if (i > 0) {
		g_info.tag.type = VORBIS;
		g_info.tag.encode = conf_encode_utf8;
	}
}

/**
 * װ��OGG�����ļ� 
 *
 * @param spath ��·����
 * @param lpath ��·����
 *
 * @return �ɹ�ʱ����0
 */
static int ogg_load(const char *spath, const char *lpath)
{
	vorbis_info *vi;
	ogg_int64_t duration;

	__init();

	g_buff_size = OGG_BUFF_SIZE / 2;
	g_buff = calloc(1, g_buff_size * sizeof(g_buff[0]));

	if (g_buff == NULL) {
		__end();
		return -1;
	}

	decoder = calloc(1, sizeof(*decoder));

	if (decoder == NULL) {
		__end();
		return -1;
	}

	ov_clear(decoder);
	g_ogg_reader = buffered_reader_open(spath, g_io_buffer_size, 1);

	if (g_ogg_reader == NULL) {
		__end();
		return -1;
	}

	g_info.filesize = buffered_reader_length(g_ogg_reader);

	if (ov_open_callbacks
		((void *) g_ogg_reader, decoder, NULL, 0, vorbis_callbacks) < 0) {
		vorbis_callbacks.close_func((void *) g_ogg_reader);
		g_ogg_reader = NULL;
		__end();
		return -1;
	}

	vi = ov_info(decoder, -1);

	if (vi->channels > 2 || vi->channels <= 0) {
		__end();
		return -1;
	}

	g_info.sample_freq = vi->rate;
	g_info.channels = vi->channels;

	duration = ov_time_total(decoder, -1);

	if (duration == OV_EINVAL) {
		g_info.duration = 0;
	} else {
		g_info.duration = (double) duration / 1000;
	}

	if (g_info.duration != 0) {
		g_info.avg_bps = (double) g_info.filesize * 8 / g_info.duration;
	} else {
		g_info.avg_bps = 0;
	}

	get_ogg_tag(decoder);

	if (xAudioInit() < 0) {
		__end();
		return -1;
	}

	if (xAudioSetFrequency(g_info.sample_freq) < 0) {
		__end();
		return -1;
	}

	xAudioSetChannelCallback(0, ogg_audiocallback, NULL);

	generic_lock();
	g_status = ST_LOADED;
	generic_unlock();

	return 0;
}

/**
 * ֹͣOGG�����ļ��Ĳ��ţ�������Դ��
 *
 * @note �����ڲ����߳��е���
 *
 * @return �ɹ�ʱ����0
 */
static int __end(void)
{
	xAudioEndPre();

	generic_lock();
	g_status = ST_STOPPED;
	generic_unlock();
	g_play_time = 0.;

	return 0;
}

/**
 * ֹͣOGG�����ļ��Ĳ��ţ�������ռ�е��̡߳���Դ��
 *
 * @note �������ڲ����߳��е��ã������ܹ�����ظ����ö�������
 *
 * @return �ɹ�ʱ����0
 */
static int ogg_end(void)
{
	__end();

	xAudioEnd();

	g_status = ST_STOPPED;

	if (decoder != NULL) {
		ov_clear(decoder);
		free(decoder);
		g_ogg_reader = NULL;
		decoder = NULL;
	}

	if (g_buff != NULL) {
		free(g_buff);
		g_buff = NULL;
	}

	g_buff_size = 0;
	free_bitrate(&g_inst_br);
	generic_end();

	return 0;
}

/**
 * PSP׼������ʱOgg�Ĳ���
 *
 * @return �ɹ�ʱ����0
 */
static int ogg_suspend(void)
{
	generic_suspend();
	ogg_end();

	return 0;
}

/**
 * PSP׼��������ʱ�ָ���Ogg�Ĳ���
 *
 * @param spath ��ǰ������������8.3·����ʽ
 * @param lpath ��ǰ���������������ļ�����ʽ
 *
 * @return �ɹ�ʱ����0
 */
static int ogg_resume(const char *spath, const char *lpath)
{
	int ret;

	ret = ogg_load(spath, lpath);
	if (ret != 0) {
		dbg_printf(d, "%s: ogg_load failed %d", __func__, ret);
		return -1;
	}

	g_play_time = g_suspend_playing_time;
	free_bitrate(&g_inst_br);
	ogg_seek_seconds(decoder, g_play_time);
	g_suspend_playing_time = 0;

	generic_resume(spath, lpath);

	return 0;
}

/**
 * �õ�OGG�����ļ������Ϣ
 *
 * @param pinfo ��Ϣ�ṹ��ָ��
 *
 * @return
 */
static int ogg_get_info(struct music_info *pinfo)
{
	if (pinfo->type & MD_GET_CURTIME) {
		pinfo->cur_time = g_play_time;
	}
	if (pinfo->type & MD_GET_CPUFREQ) {
		pinfo->psp_freq[0] = 66 + (120 - 66) * g_info.avg_bps / 1000 / 320;
		pinfo->psp_freq[1] = pinfo->psp_freq[0] / 2;
	}
	if (pinfo->type & MD_GET_INSKBPS) {
		if (decoder) {
			static long old_bitrate = 0;
			long bitrate;

			bitrate = ov_bitrate_instant(decoder);

			if (bitrate == 0) {
				bitrate = old_bitrate;
			} else if (bitrate == OV_FALSE || bitrate == OV_EINVAL) {
				bitrate = g_info.avg_bps;
			} else {
				old_bitrate = bitrate;
			}

			pinfo->ins_kbps = bitrate / 1000;
		} else {
			pinfo->ins_kbps = g_info.avg_bps / 1000;
		}
	}
	if (pinfo->type & MD_GET_DECODERNAME) {
		STRCPY_S(pinfo->decoder_name, "ogg");
	}
	if (pinfo->type & MD_GET_ENCODEMSG) {
		if (config.show_encoder_msg) {
			STRCPY_S(pinfo->encode_msg, g_vendor_str);
		} else {
			pinfo->encode_msg[0] = '\0';
		}
	}

	return generic_get_info(pinfo);
}

/**
 * ����Ƿ�ΪMPC�ļ���Ŀǰֻ����ļ���׺��
 *
 * @param spath ��ǰ������������8.3·����ʽ
 *
 * @return ��MPC�ļ�����1�����򷵻�0
 */
static int ogg_probe(const char *spath)
{
	const char *p;

	p = utils_fileext(spath);

	if (p) {
		if (stricmp(p, "ogg") == 0) {
			return 1;
		}
	}

	return 0;
}

static int ogg_set_opt(const char *unused, const char *values)
{
	int argc, i;
	char **argv;

	dbg_printf(d, "%s: options are %s", __func__, values);

	build_args(values, &argc, &argv);

	for (i = 0; i < argc; ++i) {
		if (!strncasecmp
			(argv[i], "ogg_buffer_size", sizeof("ogg_buffer_size") - 1)) {
			const char *p = argv[i];

			if ((p = strrchr(p, '=')) != NULL) {
				p++;

				g_io_buffer_size = atoi(p);

				if (g_io_buffer_size < 8192) {
					g_io_buffer_size = 8192;
				}
			}
		}
	}

	clean_args(argc, argv);

	generic_set_opt(unused, values);

	return 0;
}

static struct music_ops ogg_ops = {
	.name = "ogg",
	.set_opt = ogg_set_opt,
	.load = ogg_load,
	.play = NULL,
	.pause = NULL,
	.end = ogg_end,
	.get_status = NULL,
	.fforward = NULL,
	.fbackward = NULL,
	.suspend = ogg_suspend,
	.resume = ogg_resume,
	.get_info = ogg_get_info,
	.probe = ogg_probe,
	.next = NULL
};

int ogg_init(void)
{
	return register_musicdrv(&ogg_ops);
}

static size_t ovcb_read(void *ptr, size_t size, size_t nmemb, void *datasource)
{
	buffered_reader_t *p = (buffered_reader_t *) datasource;

	return buffered_reader_read(p, ptr, size * nmemb);
}

static int ovcb_seek(void *datasource, int64_t offset, int whence)
{
	buffered_reader_t *p = (buffered_reader_t *) datasource;
	int ret = -1;

	if (whence == PSP_SEEK_SET) {
		ret = buffered_reader_seek(p, offset);
	} else if (whence == PSP_SEEK_CUR) {
		ret = buffered_reader_seek(p, offset + buffered_reader_position(p));
	} else if (whence == PSP_SEEK_END) {
		ret = buffered_reader_seek(p, buffered_reader_length(p) - offset);
	}

	return ret;
}

static int ovcb_close(void *datasource)
{
	buffered_reader_t *p = (buffered_reader_t *) datasource;

	buffered_reader_close(p);
	return 0;
}

static long ovcb_tell(void *datasource)
{
	buffered_reader_t *p = (buffered_reader_t *) datasource;

	return buffered_reader_position(p);
}

#endif
