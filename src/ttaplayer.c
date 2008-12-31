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
#include "tta/ttalib.h"
#include "ssv.h"
#include "simple_gettext.h"
#include "dbg.h"

static int __end(void);

/**
 * ��ǰ��������״̬
 */
static int g_status;

/**
 * ����ǰ����״̬
 */
static int g_suspend_status;

#define TTA_BUFFER_SIZE (PCM_BUFFER_LENGTH * MAX_NCH)

/**
 * TTA���ֲ��Ż���
 */
static short *g_buff = NULL;

/**
 * TTA���ֲ��Ż����С����֡����
 */
static unsigned g_buff_frame_size;

/**
 * TTA���ֲ��Ż��嵱ǰλ�ã���֡����
 */
static int g_buff_frame_start;

/**
 * ��ǰ��������״̬д��
 */
static SceUID g_status_sema = -1;

/**
 * TTA�����ļ����ȣ�������
 */
static double g_duration;

/**
 * ��ǰ����ʱ�䣬��������
 */
static double g_play_time;

/**
 * TTA���ֿ����������
 */
static int g_seek_seconds;

/**
 * TTA������Ϣ
 */
static tta_info g_info;

/**
 * TTA�����Ѳ���֡��
 */
static int g_tta_frames_decoded = 0;

/**
 * TTA������֡��
 */
static int g_tta_frames = 0;

/**
 * TTA��������ʱ����ʱ��
 */
static double g_suspend_playing_time;

/**
 * TTA�������ݿ�ʼλ��
 */
static uint32_t g_tta_data_offset = 0;

/**
 * ����
 */
static inline int tta_lock(void)
{
	return sceKernelWaitSemaCB(g_status_sema, 1, NULL);
}

/**
 * ����
 */
static inline int tta_unlock(void)
{
	return sceKernelSignalSema(g_status_sema, 1);
}

/**
 * ����TTA���ֲ���ѡ��
 *
 * @param key
 * @param value
 *
 * @return �ɹ�ʱ����0
 */
static int tta_set_opt(const char *key, const char *values)
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

static int tta_seek_seconds(double seconds)
{
	if (set_position(seconds * 1000 / SEEK_STEP) == 0) {
		g_buff_frame_size = g_buff_frame_start = 0;
		g_play_time = seconds;
		g_tta_frames_decoded = (uint32_t) (seconds * g_info.SAMPLERATE);
		return 0;
	}

	__end();
	return -1;
}

/**
 * TTA���ֲ��Żص�������
 * ���𽫽��������������������
 *
 * @note �����������ĸ�ʽΪ˫������16λ������
 *
 * @param buf ����������ָ��
 * @param reqn ������֡��С
 * @param pdata �û����ݣ�����
 */
static void tta_audiocallback(void *buf, unsigned int reqn, void *pdata)
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
			tta_lock();
			g_status = ST_PLAYING;
			scene_power_save(true);
			tta_unlock();
			tta_seek_seconds(g_play_time);
		} else if (g_status == ST_FBACKWARD) {
			g_play_time -= g_seek_seconds;
			if (g_play_time < 0.) {
				g_play_time = 0.;
			}
			tta_lock();
			g_status = ST_PLAYING;
			scene_power_save(true);
			tta_unlock();
			tta_seek_seconds(g_play_time);
		}
		clear_snd_buf(buf, snd_buf_frame_size);
		return;
	}

	while (snd_buf_frame_size > 0) {
		avail_frame = g_buff_frame_size - g_buff_frame_start;

		if (avail_frame >= snd_buf_frame_size) {
			send_to_sndbuf(audio_buf,
						   &g_buff[g_buff_frame_start * g_info.NCH],
						   snd_buf_frame_size, g_info.NCH);
			g_buff_frame_start += snd_buf_frame_size;
			audio_buf += snd_buf_frame_size * 2;
			snd_buf_frame_size = 0;
		} else {
			send_to_sndbuf(audio_buf,
						   &g_buff[g_buff_frame_start * g_info.NCH],
						   avail_frame, g_info.NCH);
			snd_buf_frame_size -= avail_frame;
			audio_buf += avail_frame * 2;

			if (g_tta_frames_decoded >= g_tta_frames) {
				__end();
				return;
			}
			ret = get_samples((byte *) g_buff);
			if (ret <= 0) {
				__end();
				return;
			}

			g_buff_frame_size = ret;
			g_buff_frame_start = 0;

			incr = (double) (g_buff_frame_size) / g_info.SAMPLERATE;
			g_play_time += incr;

			g_tta_frames_decoded += g_buff_frame_size;
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
	g_status_sema = sceKernelCreateSema("tta Sema", 0, 1, 1, NULL);

	tta_lock();
	g_status = ST_UNKNOWN;
	tta_unlock();

	g_buff_frame_size = g_buff_frame_start = 0;
	g_seek_seconds = 0;

	g_duration = g_play_time = 0.;

	g_tta_frames_decoded = g_tta_frames = g_tta_data_offset = 0;

	return 0;
}

/**
 * װ��TTA�����ļ� 
 *
 * @param spath ��·����
 * @param lpath ��·����
 *
 * @return �ɹ�ʱ����0
 */
static int tta_load(const char *spath, const char *lpath)
{
	__init();

	if (g_buff != NULL) {
		free(g_buff);
		g_buff = NULL;
	}
	g_buff = calloc(TTA_BUFFER_SIZE, sizeof(*g_buff));
	if (g_buff == NULL) {
		__end();
		return -1;
	}

	if (open_tta_file(spath, &g_info, 0) < 0) {
		dbg_printf(d, "TTA Decoder Error - %s", get_error_str(g_info.STATE));
		close_tta_file(&g_info);
		return -1;
	}

	if (player_init(&g_info) != 0) {
		__end();
		return -1;
	}

	if (g_info.BPS == 0) {
		__end();
		return -1;
	}

	g_tta_frames = g_info.DATALENGTH;
	g_duration = g_info.LENGTH;

	if (xMP3AudioInit() < 0) {
		__end();
		return -1;
	}

	if (xMP3AudioSetFrequency(g_info.SAMPLERATE) < 0) {
		__end();
		return -1;
	}

	xMP3AudioSetChannelCallback(0, tta_audiocallback, NULL);

	tta_lock();
	g_status = ST_LOADED;
	tta_unlock();

	return 0;
}

/**
 * ��ʼTTA�����ļ��Ĳ���
 *
 * @return �ɹ�ʱ����0
 */
static int tta_play(void)
{
	tta_lock();
	scene_power_playing_music(true);
	g_status = ST_PLAYING;
	tta_unlock();

	return 0;
}

/**
 * ��ͣTTA�����ļ��Ĳ���
 *
 * @return �ɹ�ʱ����0
 */
static int tta_pause(void)
{
	tta_lock();
	scene_power_playing_music(false);
	g_status = ST_PAUSED;
	tta_unlock();

	return 0;
}

/**
 * ֹͣTTA�����ļ��Ĳ��ţ�������Դ��
 *
 * @note �����ڲ����߳��е���
 *
 * @return �ɹ�ʱ����0
 */
static int __end(void)
{
	xMP3AudioEndPre();

	tta_lock();
	g_status = ST_STOPPED;
	tta_unlock();

	if (g_status_sema >= 0) {
		sceKernelDeleteSema(g_status_sema);
		g_status_sema = -1;
	}

	g_play_time = 0.;

	return 0;
}

/**
 * ֹͣTTA�����ļ��Ĳ��ţ�������ռ�е��̡߳���Դ��
 *
 * @note �������ڲ����߳��е��ã������ܹ�����ظ����ö�������
 *
 * @return �ɹ�ʱ����0
 */
static int tta_end(void)
{
	__end();

	xMP3AudioEnd();

	if (g_buff != NULL) {
		free(g_buff);
		g_buff = NULL;
	}

	player_stop();
	close_tta_file(&g_info);

	g_status = ST_STOPPED;

	return 0;
}

/**
 * �õ���ǰ�����Ĳ���״̬
 *
 * @return ״̬
 */
static int tta_get_status(void)
{
	return g_status;
}

/**
 * ���TTA�����ļ�
 *
 * @param sec ����
 *
 * @return �ɹ�ʱ����0
 */
static int tta_fforward(int sec)
{
	tta_lock();
	if (g_status == ST_PLAYING || g_status == ST_PAUSED)
		g_status = ST_FFOWARD;
	tta_unlock();

	g_seek_seconds = sec;

	return 0;
}

/**
 * ����TTA�����ļ�
 *
 * @param sec ����
 *
 * @return �ɹ�ʱ����0
 */
static int tta_fbackward(int sec)
{
	tta_lock();
	if (g_status == ST_PLAYING || g_status == ST_PAUSED)
		g_status = ST_FBACKWARD;
	tta_unlock();

	g_seek_seconds = sec;

	return 0;
}

/**
 * PSP׼������ʱTTA�Ĳ���
 *
 * @return �ɹ�ʱ����0
 */
static int tta_suspend(void)
{
	g_suspend_status = g_status;
	g_suspend_playing_time = g_play_time;
	tta_end();

	return 0;
}

/**
 * PSP׼��������ʱ�ָ���TTA�Ĳ���
 *
 * @param spath ��ǰ������������8.3·����ʽ
 * @param lpath ��ǰ���������������ļ�����ʽ
 *
 * @return �ɹ�ʱ����0
 */
static int tta_resume(const char *spath, const char *lpath)
{
	int ret;

	ret = tta_load(spath, lpath);
	if (ret != 0) {
		dbg_printf(d, "%s: tta_load failed %d", __func__, ret);
		return -1;
	}

	g_play_time = g_suspend_playing_time;
	tta_seek_seconds(g_play_time);
	g_suspend_playing_time = 0;

	tta_lock();
	g_status = g_suspend_status;
	tta_unlock();
	g_suspend_status = ST_LOADED;

	return 0;
}

/**
 * �õ�TTA�����ļ������Ϣ
 *
 * @param pinfo ��Ϣ�ṹ��ָ��
 *
 * @return
 */
static int tta_get_info(struct music_info *pinfo)
{
	if (pinfo->type & MD_GET_TITLE) {
		STRCPY_S(pinfo->title, (const char *) g_info.ID3.title);
	}
	if (pinfo->type & MD_GET_ARTIST) {
		STRCPY_S(pinfo->artist, (const char *) g_info.ID3.artist);
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
		pinfo->psp_freq[0] = 222;
		pinfo->psp_freq[1] = 111;
	}
	if (pinfo->type & MD_GET_FREQ) {
		pinfo->freq = g_info.SAMPLERATE;
	}
	if (pinfo->type & MD_GET_CHANNELS) {
		pinfo->channels = g_info.NCH;
	}
	if (pinfo->type & MD_GET_AVGKBPS) {
		pinfo->avg_kbps = g_info.BITRATE;
	}
	if (pinfo->type & MD_GET_DECODERNAME) {
		STRCPY_S(pinfo->decoder_name, "tta");
	}
	if (pinfo->type & MD_GET_ENCODEMSG) {
		if (show_encoder_msg) {
			SPRINTF_S(pinfo->encode_msg, "%s: %.2f", _("ѹ����"),
					  g_info.COMPRESS);
		} else {
			pinfo->encode_msg[0] = '\0';
		}
	}

	return 0;
}

static struct music_ops tta_ops = {
	.name = "tta",
	.set_opt = tta_set_opt,
	.load = tta_load,
	.play = tta_play,
	.pause = tta_pause,
	.end = tta_end,
	.get_status = tta_get_status,
	.fforward = tta_fforward,
	.fbackward = tta_fbackward,
	.suspend = tta_suspend,
	.resume = tta_resume,
	.get_info = tta_get_info,
	.next = NULL
};

int tta_init(void)
{
	return register_musicdrv(&tta_ops);
}
