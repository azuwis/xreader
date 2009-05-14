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
#include <psprtc.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include "config.h"
#include "ssv.h"
#include "scene.h"
#include "xmp3audiolib.h"
#include "musicmgr.h"
#include "musicdrv.h"
#include "strsafe.h"
#include "common/utils.h"
#include "apetaglib/APETag.h"
#include "freq_lock.h"
#include "freq_lock.h"
#include "musicinfo.h"
#include "dbg.h"
#include "buffered_reader.h"
#include "genericplayer.h"
#include "xrhal.h"

/**
 * �ϴΰ�����˼�����
 */
bool g_last_seek_is_forward = false;

/**
 * �ϴΰ�����˼���ʼʱ��
 */
volatile u64 g_last_seek_tick = 0;

/**
 *  ������˼�����
 */
volatile dword g_seek_count = 0;

/**
 * ����ǰ����״̬
 */
int g_suspend_status = ST_UNKNOWN;

/**
 * ��ǰ����ʱ�䣬��������
 */
double g_play_time = 0;

/**
 * Wave��������ʱ����ʱ��
 */
double g_suspend_playing_time = 0;

/**
 * ���ֿ����������
 */
int g_seek_seconds = 0;

/**
 * ��ǰ��������״̬
 */
int g_status = ST_UNKNOWN;

/**
 * ��ǰ��������״̬д��
 */
static SceUID g_status_sema = -1;

/**
 * freq_locker ID
 */
int g_fid = -1;

/**
 * ��ǰ���������ļ���Ϣ
 */
MusicInfo g_info = { 0 };

/**
 * ʹ�û���IO
 */
bool g_use_buffer = true;

/**
 * Ĭ�ϻ���IO�����ֽڴ�С����Ͳ�С��8192
 */
int g_io_buffer_size = BUFFERED_READER_BUFFER_SIZE;

reader_data data;

/**
 * ����
 */
int generic_lock(void)
{
	return xrKernelWaitSemaCB(g_status_sema, 1, NULL);
}

/**
 * ����
 */
int generic_unlock(void)
{
	return xrKernelSignalSema(g_status_sema, 1);
}

int generic_set_opt(const char *unused, const char *values)
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

	dbg_printf(d, "%s: %d", __func__, show_encoder_msg);

	clean_args(argc, argv);

	return 0;
}

void generic_set_playback(bool playing)
{
	if (playing) {
		if (g_fid < 0) {
			struct music_info info = { 0 };

			info.type = MD_GET_CPUFREQ;

			if (musicdrv_get_info(&info) == 0) {
				g_fid = freq_enter(info.psp_freq[0], info.psp_freq[1]);
			}
		}
	} else {
		if (g_fid >= 0) {
			freq_leave(g_fid);
			g_fid = -1;
		}
	}
}

int generic_play(void)
{
	generic_lock();
	generic_set_playback(true);
	g_status = ST_PLAYING;
	generic_unlock();

	return 0;
}

int generic_pause(void)
{
	generic_lock();
	generic_set_playback(false);
	g_status = ST_PAUSED;
	generic_unlock();

	return 0;
}

/**
 * �õ���ǰ�����Ĳ���״̬
 *
 * @return ״̬
 */
int generic_get_status(void)
{
	return g_status;
}

/**
 * ��������ļ�
 *
 * @param sec ����
 *
 * @return �ɹ�ʱ����0
 */
int generic_fforward(int sec)
{
	generic_lock();
	if (g_status == ST_PLAYING || g_status == ST_PAUSED
		|| g_status == ST_FBACKWARD)
		g_status = ST_FFORWARD;

	g_seek_seconds = sec;

	xrRtcGetCurrentTick((u64 *) & g_last_seek_tick);
	g_last_seek_is_forward = true;
	g_seek_count++;
	generic_unlock();

	return 0;
}

/**
 * ���������ļ�
 *
 * @param sec ����
 *
 * @return �ɹ�ʱ����0
 */
int generic_fbackward(int sec)
{
	generic_lock();
	if (g_status == ST_PLAYING || g_status == ST_PAUSED
		|| g_status == ST_FFORWARD)
		g_status = ST_FBACKWARD;

	g_seek_seconds = sec;

	xrRtcGetCurrentTick((u64 *) & g_last_seek_tick);
	g_last_seek_is_forward = false;
	g_seek_count++;
	generic_unlock();

	return 0;
}

int generic_end(void)
{
	if (g_status_sema >= 0) {
		xrKernelDeleteSema(g_status_sema);
		g_status_sema = -1;
	}

	generic_set_playback(false);

	return 0;
}

int generic_init(void)
{
	g_status_sema = xrKernelCreateSema("Music Sema", 0, 1, 1, NULL);
	g_seek_count = 0;

	return 0;
}

int generic_resume(const char *spath, const char *lpath)
{
	generic_lock();
	g_status = g_suspend_status;

	if (g_status == ST_PLAYING)
		generic_set_playback(true);

	generic_unlock();
	g_suspend_status = ST_LOADED;

	return 0;
}

int generic_suspend(void)
{
	g_suspend_status = g_status;
	g_suspend_playing_time = g_play_time;

	return 0;
}

int generic_get_info(struct music_info *info)
{
	if (info->type & MD_GET_TITLE) {
		info->encode = g_info.tag.encode;

		if (info->encode != conf_encode_ucs)
			STRCPY_S(info->title, g_info.tag.title);
		else
			memcpy(info->title, g_info.tag.title, sizeof(info->title));
	}
	if (info->type & MD_GET_ALBUM) {
		info->encode = g_info.tag.encode;

		if (info->encode != conf_encode_ucs)
			STRCPY_S(info->album, g_info.tag.album);
		else
			memcpy(info->album, g_info.tag.album, sizeof(info->album));
	}
	if (info->type & MD_GET_ARTIST) {
		info->encode = g_info.tag.encode;

		if (info->encode != conf_encode_ucs)
			STRCPY_S(info->artist, g_info.tag.artist);
		else
			memcpy(info->artist, g_info.tag.artist, sizeof(info->artist));
	}
	if (info->type & MD_GET_COMMENT) {
		info->encode = g_info.tag.encode;

		if (info->encode != conf_encode_ucs)
			STRCPY_S(info->comment, g_info.tag.comment);
		else
			memcpy(info->comment, g_info.tag.comment, sizeof(info->comment));
	}
	if (info->type & MD_GET_DURATION) {
		info->duration = g_info.duration;
	}
	if (info->type & MD_GET_FREQ) {
		info->freq = g_info.sample_freq;
	}
	if (info->type & MD_GET_CHANNELS) {
		info->channels = g_info.channels;
	}
	if (info->type & MD_GET_AVGKBPS) {
		info->avg_kbps = g_info.avg_bps / 1000;
	}

	return 0;
}
