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
#include "strsafe.h"
#include "common/utils.h"
#include "apetaglib/APETag.h"
#include "dbg.h"
#include "ssv.h"

typedef struct reader_data_t
{
	SceUID fd;
	long size;
} reader_data;

static int __end(void);

static reader_data data;

/**
 * ��ǰ��������״̬
 */
static int g_status;

/**
 * ����ǰ����״̬
 */
static int g_suspend_status;

#define WAVE_BUFFER_SIZE (1024 * 95)

/**
 * Wave���ֲ��Ż���
 */
static short *g_buff = NULL;

/**
 * Wave���ֲ��Ż����С����֡����
 */
static unsigned g_buff_frame_size;

/**
 * Wave���ֲ��Ż��嵱ǰλ�ã���֡����
 */
static int g_buff_frame_start;

/**
 * ��ǰ��������״̬д��
 */
static SceUID g_status_sema = -1;

/**
 * Wave�����ļ����ȣ�������
 */
static double g_duration;

/**
 * ��ǰ����ʱ�䣬��������
 */
static double g_play_time;

/**
 * Wave���ֿ����������
 */
static int g_seek_seconds;

/**
 * Wave����������
 */
static int g_wav_channels;

/**
 * Wave����������
 */
static int g_wav_sample_freq;

/**
 * Wave���ֱ�����
 */
static int g_wav_bitrate;

/**
 * Wave����ÿ֡�ֽ���
 */
static int g_wav_byte_per_frame = 0;

/**
 * Wave�����Ѳ���֡��
 */
static int g_wav_frames_decoded = 0;

/**
 * Wave������֡��
 */
static int g_wav_frames = 0;

/**
 * Wave��������ʱ����ʱ��
 */
static double g_suspend_playing_time;

/**
 * Wave�������ݿ�ʼλ��
 */
static uint32_t g_wav_data_offset = 0;

typedef struct _wav_taginfo_t
{
	char title[80];
	char artist[80];
	char album[80];
} wav_taginfo_t;

static wav_taginfo_t g_taginfo;

/**
 * ����
 */
static inline int wav_lock(void)
{
	return sceKernelWaitSemaCB(g_status_sema, 1, NULL);
}

/**
 * ����
 */
static inline int wav_unlock(void)
{
	return sceKernelSignalSema(g_status_sema, 1);
}

/**
 * ����Wave���ֲ���ѡ��
 *
 * @param key
 * @param value
 *
 * @return �ɹ�ʱ����0
 */
static int wav_set_opt(const char *key, const char *values)
{
	int argc, i;
	char **argv;

	dbg_printf(d, "%s: options are %s", __func__, values);

	build_args(values, &argc, &argv);

	for (i = 0; i < argc; ++i) {
		if (!strncasecmp
			(argv[i], "show_encoder_msg", sizeof("show_encoder_msg") - 1)) {
			if (opt_is_on(argv[i])) {
				show_encoder_msg = true;
			} else {
				show_encoder_msg = false;
			}
		}
	}

	clean_args(argc, argv);

	return 0;
}

/**
 * �������������
 *
 * @param buf ����������ָ��
 * @param frames ֡����С
 */
static void clear_snd_buf(void *buf, int frames)
{
	memset(buf, 0, frames * 2 * 2);
}

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
static void send_to_sndbuf(void *buf, short *srcbuf, int frames, int channels)
{
	unsigned n;
	signed short *p = buf;

	if (frames <= 0)
		return;

	for (n = 0; n < frames * channels; n++) {
		if (channels == 2)
			*p++ = srcbuf[n];
		else if (channels == 1) {
			*p++ = srcbuf[n];
			*p++ = srcbuf[n];
		}
	}
}

static int wav_seek_seconds(double seconds)
{
	int ret;

	ret =
		sceIoLseek(data.fd,
				   g_wav_data_offset +
				   (uint32_t) (seconds * g_wav_sample_freq) *
				   g_wav_byte_per_frame, SEEK_SET);

	if (ret >= 0) {
		g_buff_frame_size = g_buff_frame_start = 0;
		g_play_time = seconds;
		g_wav_frames_decoded = (uint32_t) (seconds * g_wav_sample_freq);
		return 0;
	}

	__end();
	return -1;
}

/**
 * Wave���ֲ��Żص�������
 * ���𽫽��������������������
 *
 * @note �����������ĸ�ʽΪ˫������16λ������
 *
 * @param buf ����������ָ��
 * @param reqn ������֡��С
 * @param pdata �û����ݣ�����
 */
static void wav_audiocallback(void *buf, unsigned int reqn, void *pdata)
{
	int avail_frame;
	int snd_buf_frame_size = (int) reqn;
	int ret;
	double incr;
	signed short *audio_buf = buf;

	UNUSED(pdata);

	if (g_status != ST_PLAYING) {
		if (g_status == ST_FFOWARD) {
			g_play_time += g_seek_seconds;
			if (g_play_time >= g_duration) {
				__end();
				return;
			}
			wav_lock();
			g_status = ST_PLAYING;
			scene_power_save(true);
			wav_unlock();
			wav_seek_seconds(g_play_time);
		} else if (g_status == ST_FBACKWARD) {
			g_play_time -= g_seek_seconds;
			if (g_play_time < 0.) {
				g_play_time = 0.;
			}
			wav_lock();
			g_status = ST_PLAYING;
			scene_power_save(true);
			wav_unlock();
			wav_seek_seconds(g_play_time);
		}
		clear_snd_buf(buf, snd_buf_frame_size);
		return;
	}

	while (snd_buf_frame_size > 0) {
		avail_frame = g_buff_frame_size - g_buff_frame_start;

		if (avail_frame >= snd_buf_frame_size) {
			send_to_sndbuf(audio_buf,
						   &g_buff[g_buff_frame_start * g_wav_channels],
						   snd_buf_frame_size, g_wav_channels);
			g_buff_frame_start += snd_buf_frame_size;
			audio_buf += snd_buf_frame_size * 2;
			incr = (double) (snd_buf_frame_size) / g_wav_sample_freq;
			g_play_time += incr;
			snd_buf_frame_size = 0;
		} else {
			send_to_sndbuf(audio_buf,
						   &g_buff[g_buff_frame_start * g_wav_channels],
						   avail_frame, g_wav_channels);
			snd_buf_frame_size -= avail_frame;
			audio_buf += avail_frame * 2;

			if (g_wav_frames_decoded >= g_wav_frames) {
				__end();
				return;
			}
			ret =
				sceIoRead(data.fd, g_buff, WAVE_BUFFER_SIZE * sizeof(*g_buff));
			if (ret <= 0) {
				__end();
				return;
			}

			g_buff_frame_size = ret / g_wav_byte_per_frame;
			g_buff_frame_start = 0;

			g_wav_frames_decoded += g_buff_frame_size;
		}
	}
}

/**
 * ��ʼ������������Դ��
 *
 * @return �ɹ�ʱ����0
 */
static int __init(void)
{
	g_status_sema = sceKernelCreateSema("wave Sema", 0, 1, 1, NULL);

	wav_lock();
	g_status = ST_UNKNOWN;
	wav_unlock();

	g_buff_frame_size = g_buff_frame_start = 0;
	g_seek_seconds = 0;

	g_duration = g_play_time = 0.;

	g_wav_frames_decoded = g_wav_frames = g_wav_data_offset = g_wav_bitrate =
		g_wav_sample_freq = g_wav_channels = 0;

	memset(&g_taginfo, 0, sizeof(g_taginfo));

	return 0;
}

static int wave_get_16(SceUID fd, uint16_t * buf)
{
	int ret = sceIoRead(fd, buf, sizeof(*buf));

	if (ret == 2) {
		return 0;
	}

	return -1;
}

static int wave_get_32(SceUID fd, uint32_t * buf)
{
	int ret = sceIoRead(fd, buf, sizeof(*buf));

	if (ret == 4) {
		return 0;
	}

	return -1;
}

static int wave_skip_n_bytes(SceUID fd, int n)
{
	return sceIoLseek(fd, n, SEEK_CUR) >= 0 ? 0 : -1;
}

/**
 * װ��Wave�����ļ� 
 *
 * @param spath ��·����
 * @param lpath ��·����
 *
 * @return �ɹ�ʱ����0
 */
static int wav_load(const char *spath, const char *lpath)
{
	__init();

	if (g_buff != NULL) {
		free(g_buff);
		g_buff = NULL;
	}
	g_buff = calloc(WAVE_BUFFER_SIZE, sizeof(*g_buff));
	if (g_buff == NULL) {
		__end();
		return -1;
	}

	APETag *tag = loadAPETag(spath);

	if (tag != NULL) {
		char *title = APETag_SimpleGet(tag, "Title");
		char *artist = APETag_SimpleGet(tag, "Artist");
		char *album = APETag_SimpleGet(tag, "Album");

		if (title) {
			STRCPY_S(g_taginfo.title, title);
			free(title);
			title = NULL;
		}
		if (artist) {
			STRCPY_S(g_taginfo.artist, artist);
			free(artist);
			artist = NULL;
		} else {
			artist = APETag_SimpleGet(tag, "Album artist");
			if (artist) {
				STRCPY_S(g_taginfo.artist, artist);
				free(artist);
				artist = NULL;
			}
		}
		if (album) {
			STRCPY_S(g_taginfo.album, album);
			free(album);
			album = NULL;
		}
		freeAPETag(tag);
	}

	data.fd = sceIoOpen(spath, PSP_O_RDONLY, 0777);

	if (data.fd < 0) {
		__end();
		return -1;
	}

	data.size = sceIoLseek(data.fd, 0, PSP_SEEK_END);
	sceIoLseek(data.fd, 0, PSP_SEEK_SET);

	uint32_t temp;

	// 'RIFF' keyword
	if (wave_get_32(data.fd, &temp) != 0) {
		__end();
		return -1;
	}
	if (temp != 0x46464952) {
		__end();
		return -1;
	}
	// chunk size: 36 + SubChunk2Size
	if (wave_get_32(data.fd, &temp) != 0) {
		__end();
		return -1;
	}
	// 'WAVE' keyword
	if (wave_get_32(data.fd, &temp) != 0) {
		__end();
		return -1;
	}
	if (temp != 0x45564157) {
		__end();
		return -1;
	}
	// 'fmt' keyword
	if (wave_get_32(data.fd, &temp) != 0) {
		__end();
		return -1;
	}
	if (temp != 0x20746d66) {
		__end();
		return -1;
	}
	uint32_t fmt_chunk_size;

	if (wave_get_32(data.fd, &fmt_chunk_size) != 0) {
		__end();
		return -1;
	}
	// Format tag: 1 if PCM
	temp = 0;
	if (wave_get_16(data.fd, (uint16_t *) & temp) != 0) {
		__end();
		return -1;
	}
	if (temp != 1) {
		__end();
		return -1;
	}
	// Channels
	if (wave_get_16(data.fd, (uint16_t *) & g_wav_channels) != 0) {
		__end();
		return -1;
	}
	// Sample freq
	if (wave_get_32(data.fd, (uint32_t *) & g_wav_sample_freq) != 0) {
		__end();
		return -1;
	}
	// byte rate
	if (wave_get_32(data.fd, (uint32_t *) & g_wav_bitrate) != 0) {
		__end();
		return -1;
	}
	g_wav_bitrate *= 8;
	// byte per sample
	if (wave_get_16(data.fd, (uint16_t *) & g_wav_byte_per_frame) != 0) {
		__end();
		return -1;
	}
	// bit per sample
	if (wave_get_16(data.fd, (uint16_t *) & temp) != 0) {
		__end();
		return -1;
	}
	// skip addition information
	if (fmt_chunk_size == 18) {
		uint16_t addition_size = 0;

		if (wave_get_16(data.fd, &addition_size) != 0) {
			__end();
			return -1;
		}
		wave_skip_n_bytes(data.fd, addition_size);
		g_wav_data_offset = 44 + 2 + addition_size;
	} else {
		g_wav_data_offset = 44;
	}
	if (wave_get_32(data.fd, (uint32_t *) & temp) != 0) {
		__end();
		return -1;
	}
	if (temp != 0x61746164) {
		__end();
		return -1;
	}
	// get size of data
	if (wave_get_32(data.fd, (uint32_t *) & temp) != 0) {
		__end();
		return -1;
	}
	if (g_wav_byte_per_frame == 0 || g_wav_sample_freq == 0
		|| g_wav_channels == 0) {
		__end();
		return -1;
	}
	g_wav_frames = temp / g_wav_byte_per_frame;
	g_duration = (double) (temp) / g_wav_byte_per_frame / g_wav_sample_freq;

	if (xMP3AudioInit() < 0) {
		__end();
		return -1;
	}

	if (xMP3AudioSetFrequency(g_wav_sample_freq) < 0) {
		__end();
		return -1;
	}

	xMP3AudioSetChannelCallback(0, wav_audiocallback, NULL);

	wav_lock();
	g_status = ST_LOADED;
	wav_unlock();

	return 0;
}

/**
 * ��ʼWave�����ļ��Ĳ���
 *
 * @return �ɹ�ʱ����0
 */
static int wav_play(void)
{
	wav_lock();
	scene_power_playing_music(true);
	g_status = ST_PLAYING;
	wav_unlock();

	return 0;
}

/**
 * ��ͣWave�����ļ��Ĳ���
 *
 * @return �ɹ�ʱ����0
 */
static int wav_pause(void)
{
	wav_lock();
	scene_power_playing_music(false);
	g_status = ST_PAUSED;
	wav_unlock();

	return 0;
}

/**
 * ֹͣWave�����ļ��Ĳ��ţ�������Դ��
 *
 * @note �����ڲ����߳��е���
 *
 * @return �ɹ�ʱ����0
 */
static int __end(void)
{
	xMP3AudioEndPre();

	wav_lock();
	g_status = ST_STOPPED;
	wav_unlock();

	if (g_status_sema >= 0) {
		sceKernelDeleteSema(g_status_sema);
		g_status_sema = -1;
	}

	if (data.fd >= 0) {
		sceIoClose(data.fd);
		data.fd = -1;
	}

	g_play_time = 0.;

	return 0;
}

/**
 * ֹͣWave�����ļ��Ĳ��ţ�������ռ�е��̡߳���Դ��
 *
 * @note �������ڲ����߳��е��ã������ܹ�����ظ����ö�������
 *
 * @return �ɹ�ʱ����0
 */
static int wav_end(void)
{
	__end();

	xMP3AudioEnd();

	if (g_buff != NULL) {
		free(g_buff);
		g_buff = NULL;
	}

	g_status = ST_STOPPED;

	return 0;
}

/**
 * �õ���ǰ�����Ĳ���״̬
 *
 * @return ״̬
 */
static int wav_get_status(void)
{
	return g_status;
}

/**
 * ���Wave�����ļ�
 *
 * @param sec ����
 *
 * @return �ɹ�ʱ����0
 */
static int wav_fforward(int sec)
{
	wav_lock();
	if (g_status == ST_PLAYING || g_status == ST_PAUSED)
		g_status = ST_FFOWARD;
	wav_unlock();

	g_seek_seconds = sec;

	return 0;
}

/**
 * ����Wave�����ļ�
 *
 * @param sec ����
 *
 * @return �ɹ�ʱ����0
 */
static int wav_fbackward(int sec)
{
	wav_lock();
	if (g_status == ST_PLAYING || g_status == ST_PAUSED)
		g_status = ST_FBACKWARD;
	wav_unlock();

	g_seek_seconds = sec;

	return 0;
}

/**
 * PSP׼������ʱWave�Ĳ���
 *
 * @return �ɹ�ʱ����0
 */
static int wav_suspend(void)
{
	g_suspend_status = g_status;
	g_suspend_playing_time = g_play_time;
	wav_end();

	return 0;
}

/**
 * PSP׼��������ʱ�ָ���Wave�Ĳ���
 *
 * @param spath ��ǰ������������8.3·����ʽ
 * @param lpath ��ǰ���������������ļ�����ʽ
 *
 * @return �ɹ�ʱ����0
 */
static int wav_resume(const char *spath, const char *lpath)
{
	int ret;

	ret = wav_load(spath, lpath);
	if (ret != 0) {
		dbg_printf(d, "%s: wav_load failed %d", __func__, ret);
		return -1;
	}

	g_play_time = g_suspend_playing_time;
	wav_seek_seconds(g_play_time);
	g_suspend_playing_time = 0;

	wav_lock();
	g_status = g_suspend_status;
	wav_unlock();
	g_suspend_status = ST_LOADED;

	return 0;
}

/**
 * �õ�Wave�����ļ������Ϣ
 *
 * @param pinfo ��Ϣ�ṹ��ָ��
 *
 * @return
 */
static int wav_get_info(struct music_info *pinfo)
{
	if (pinfo->type & MD_GET_TITLE) {
		STRCPY_S(pinfo->title, g_taginfo.title);
	}
	if (pinfo->type & MD_GET_ARTIST) {
		STRCPY_S(pinfo->artist, g_taginfo.artist);
	}
	if (pinfo->type & MD_GET_COMMENT) {
		STRCPY_S(pinfo->comment, "");
	}
	if (pinfo->type & MD_GET_CURTIME) {
		pinfo->cur_time = g_play_time;
	}
	if (pinfo->type & MD_GET_DURATION) {
		pinfo->duration = g_duration;
	}
	if (pinfo->type & MD_GET_CPUFREQ) {
		pinfo->psp_freq[0] = 33;
		pinfo->psp_freq[1] = 16;
	}
	if (pinfo->type & MD_GET_FREQ) {
		pinfo->freq = g_wav_sample_freq;
	}
	if (pinfo->type & MD_GET_CHANNELS) {
		pinfo->channels = g_wav_channels;
	}
	if (pinfo->type & MD_GET_AVGKBPS) {
		pinfo->avg_kbps = g_wav_bitrate / 1000;
	}
	if (pinfo->type & MD_GET_DECODERNAME) {
		STRCPY_S(pinfo->decoder_name, "wave");
	}
	if (pinfo->type & MD_GET_ENCODEMSG) {
		SPRINTF_S(pinfo->encode_msg, "");
	}

	return 0;
}

static struct music_ops wav_ops = {
	.name = "wave",
	.set_opt = wav_set_opt,
	.load = wav_load,
	.play = wav_play,
	.pause = wav_pause,
	.end = wav_end,
	.get_status = wav_get_status,
	.fforward = wav_fforward,
	.fbackward = wav_fbackward,
	.suspend = wav_suspend,
	.resume = wav_resume,
	.get_info = wav_get_info,
	.next = NULL
};

int wav_init(void)
{
	return register_musicdrv(&wav_ops);
}
