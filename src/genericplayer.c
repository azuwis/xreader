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
#include <mpcdec/mpcdec.h>
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
#include "dbg.h"

/**
 * 上次按快进退键类型
 */
bool g_last_seek_is_forward = false;

/**
 * 上次按快进退键时间
 */
u64 g_last_seek_tick;

/**
 * 休眠前播放状态
 */
int g_suspend_status;

/**
 * 当前播放时间，以秒数计
 */
double g_play_time;

/**
 * Wave音乐休眠时播放时间
 */
double g_suspend_playing_time;

/**
 * 音乐快进、退秒数
 */
int g_seek_seconds = 0;

/**
 * 当前驱动播放状态
 */
int g_status = ST_UNKNOWN;

/**
 * 当前驱动播放状态写锁
 */
static SceUID g_status_sema = -1;

/**
 * 加锁
 */
int generic_lock(void)
{
	return sceKernelWaitSemaCB(g_status_sema, 1, NULL);
}

/**
 * 解锁
 */
int generic_unlock(void)
{
	return sceKernelSignalSema(g_status_sema, 1);
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

int generic_play(void)
{
	generic_lock();
	scene_power_playing_music(true);
	g_status = ST_PLAYING;
	generic_unlock();

	return 0;
}

int generic_pause(void)
{
	generic_lock();
	scene_power_playing_music(false);
	g_status = ST_PAUSED;
	generic_unlock();

	return 0;
}

/**
 * 得到当前驱动的播放状态
 *
 * @return 状态
 */
int generic_get_status(void)
{
	return g_status;
}

/**
 * 快进音乐文件
 *
 * @param sec 秒数
 *
 * @return 成功时返回0
 */
int generic_fforward(int sec)
{
	generic_lock();
	if (g_status == ST_PLAYING || g_status == ST_PAUSED
		|| g_status == ST_FBACKWARD)
		g_status = ST_FFOWARD;

	g_seek_seconds = sec;

	sceRtcGetCurrentTick(&g_last_seek_tick);
	g_last_seek_is_forward = true;
	generic_unlock();

	return 0;
}

/**
 * 快退音乐文件
 *
 * @param sec 秒数
 *
 * @return 成功时返回0
 */
int generic_fbackward(int sec)
{
	generic_lock();
	if (g_status == ST_PLAYING || g_status == ST_PAUSED
		|| g_status == ST_FFOWARD)
		g_status = ST_FBACKWARD;

	g_seek_seconds = sec;

	sceRtcGetCurrentTick(&g_last_seek_tick);
	g_last_seek_is_forward = false;
	generic_unlock();

	return 0;
}

int generic_end(void)
{
	if (g_status_sema >= 0) {
		sceKernelDeleteSema(g_status_sema);
		g_status_sema = -1;
	}

	return 0;
}

int generic_init(void)
{
	g_status_sema = sceKernelCreateSema("Music Sema", 0, 1, 1, NULL);

	return 0;
}

int generic_resume(const char *spath, const char *lpath)
{
	generic_lock();
	g_status = g_suspend_status;
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
