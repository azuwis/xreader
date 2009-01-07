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
 * 音乐快进、退秒数
 */
int g_seek_seconds = 0;

/**
 * 当前驱动播放状态
 */
int g_status = 0;

/**
 * 当前驱动播放状态写锁
 */
SceUID g_status_sema = -1;

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
	if (g_status == ST_PLAYING || g_status == ST_PAUSED)
		g_status = ST_FFOWARD;
	generic_unlock();

	g_seek_seconds = sec;

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
	if (g_status == ST_PLAYING || g_status == ST_PAUSED)
		g_status = ST_FBACKWARD;
	generic_unlock();

	g_seek_seconds = sec;

	return 0;
}
