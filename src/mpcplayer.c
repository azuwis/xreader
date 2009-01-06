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

static int __end(void);

/*
  The data bundle we pass around with our reader to store file
  position and size etc. 
*/
typedef struct reader_data_t
{
	SceUID fd;
	long size;
	mpc_bool_t seekable;
} reader_data;

static reader_data data;
static mpc_decoder decoder;
static mpc_reader reader;
static mpc_streaminfo info;

/*
  Our implementations of the mpc_reader callback functions.
*/
static mpc_int32_t read_impl(void *data, void *ptr, mpc_int32_t size)
{
	reader_data *d = (reader_data *) data;

	return sceIoRead(d->fd, ptr, size);
}

static mpc_bool_t seek_impl(void *data, mpc_int32_t offset)
{
	reader_data *d = (reader_data *) data;

	return d->seekable ? (sceIoLseek(d->fd, offset, SEEK_SET), 1) : 0;
}

static mpc_int32_t tell_impl(void *data)
{
	reader_data *d = (reader_data *) data;

	return sceIoLseek(d->fd, 0, SEEK_CUR);
}

static mpc_int32_t get_size_impl(void *data)
{
	reader_data *d = (reader_data *) data;

	return d->size;
}

static mpc_bool_t canseek_impl(void *data)
{
	reader_data *d = (reader_data *) data;

	return d->seekable;
}

/**
 * 当前驱动播放状态
 */
static int g_status;

/**
 * 休眠前播放状态
 */
static int g_suspend_status;

/**
 * Musepack音乐播放缓冲
 */
static MPC_SAMPLE_FORMAT g_buff[MPC_DECODER_BUFFER_LENGTH];

/**
 * Musepack音乐播放缓冲大小，以帧数计
 */
static unsigned g_buff_frame_size;

/**
 * Musepack音乐播放缓冲当前位置，以帧数计
 */
static int g_buff_frame_start;

/**
 * 当前驱动播放状态写锁
 */
static SceUID g_status_sema = -1;

/**
 * Musepack音乐文件长度，以秒数
 */
static double g_duration;

/**
 * 当前播放时间，以秒数计
 */
static double g_play_time;

/**
 * Musepack音乐快进、退秒数
 */
static int g_seek_seconds;

/**
 * Musepack音乐休眠时播放时间
 */
static double g_suspend_playing_time;

typedef struct _MPC_taginfo_t
{
	char title[80];
	char artist[80];
	char album[80];
} MPC_taginfo_t;

static MPC_taginfo_t g_taginfo;

/**
 * 加锁
 */
static inline int mpc_lock(void)
{
	return sceKernelWaitSemaCB(g_status_sema, 1, NULL);
}

/**
 * 解锁
 */
static inline int mpc_unlock(void)
{
	return sceKernelSignalSema(g_status_sema, 1);
}

/**
 * 设置Musepack音乐播放选项
 *
 * @param key
 * @param value
 *
 * @return 成功时返回0
 */
static int mpc_set_opt(const char *unused, const char *values)
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

/**
 * 清空声音缓冲区
 *
 * @param buf 声音缓冲区指针
 * @param frames 帧数大小
 */
static void clear_snd_buf(void *buf, int frames)
{
	memset(buf, 0, frames * 2 * 2);
}

#ifdef MPC_FIXED_POINT
static int shift_signed(MPC_SAMPLE_FORMAT val, int shift)
{
	if (shift > 0)
		val <<= shift;
	else if (shift < 0)
		val >>= -shift;
	return (int) val;
}
#endif

/**
 * 复制数据到声音缓冲区
 *
 * @note 声音缓存区的格式为双声道，16位低字序
 *
 * @param buf 声音缓冲区指针
 * @param srcbuf 解码数据缓冲区指针
 * @param frames 复制帧数
 * @param channels 声道数
 */
static void send_to_sndbuf(void *buf, MPC_SAMPLE_FORMAT * srcbuf, int frames,
						   int channels)
{
	unsigned n;
	unsigned bps = 16;
	signed short *p = buf;
	int clip_min = -1 << (bps - 1), clip_max = (1 << (bps - 1)) - 1;

	if (frames <= 0)
		return;

#ifndef MPC_FIXED_POINT
	int float_scale = 1 << (bps - 1);
#endif
	for (n = 0; n < frames * channels; n++) {
		int val;

#ifdef MPC_FIXED_POINT
		val = shift_signed(srcbuf[n], bps - MPC_FIXED_POINT_SCALE_SHIFT);
#else
		val = (int) (srcbuf[n] * float_scale);
#endif
		if (val < clip_min)
			val = clip_min;
		else if (val > clip_max)
			val = clip_max;
		if (channels == 2)
			*p++ = (signed short) val;
		else if (channels == 1) {
			*p++ = (signed short) val;
			*p++ = (signed short) val;
		}
	}
}

/**
 * Musepack音乐播放回调函数，
 * 负责将解码数据填充声音缓存区
 *
 * @note 声音缓存区的格式为双声道，16位低字序
 *
 * @param buf 声音缓冲区指针
 * @param reqn 缓冲区帧大小
 * @param pdata 用户数据，无用
 */
static int mpc_audiocallback(void *buf, unsigned int reqn, void *pdata)
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
				return -1;
			}
			mpc_lock();
			g_status = ST_PLAYING;
			scene_power_save(true);
			mpc_unlock();
			mpc_decoder_seek_seconds(&decoder, g_play_time);
			g_buff_frame_size = g_buff_frame_start = 0;
		} else if (g_status == ST_FBACKWARD) {
			g_play_time -= g_seek_seconds;
			if (g_play_time < 0.) {
				g_play_time = 0.;
			}
			mpc_lock();
			g_status = ST_PLAYING;
			scene_power_save(true);
			mpc_unlock();
			mpc_decoder_seek_seconds(&decoder, g_play_time);
			g_buff_frame_size = g_buff_frame_start = 0;
		}
		clear_snd_buf(buf, snd_buf_frame_size);
		return 0;
	}

	while (snd_buf_frame_size > 0) {
		avail_frame = g_buff_frame_size - g_buff_frame_start;

		if (avail_frame >= snd_buf_frame_size) {
			send_to_sndbuf(audio_buf,
						   &g_buff[g_buff_frame_start * info.channels],
						   snd_buf_frame_size, info.channels);
			g_buff_frame_start += snd_buf_frame_size;
			audio_buf += snd_buf_frame_size * 2;
			snd_buf_frame_size = 0;
		} else {
			send_to_sndbuf(audio_buf,
						   &g_buff[g_buff_frame_start * info.channels],
						   avail_frame, info.channels);
			snd_buf_frame_size -= avail_frame;
			audio_buf += avail_frame * 2;

			ret = mpc_decoder_decode(&decoder, g_buff, 0, 0);
			if (ret == -1 || ret == 0) {
				__end();
				return -1;
			}

			g_buff_frame_size = ret;
			g_buff_frame_start = 0;

			incr =
				(double) (MPC_DECODER_BUFFER_LENGTH / 2 / info.channels) /
				info.sample_freq;
			g_play_time += incr;
		}
	}

	return 0;
}

/**
 * 初始化驱动变量资源等
 *
 * @return 成功时返回0
 */
static int __init(void)
{
	g_status_sema = sceKernelCreateSema("Musepack Sema", 0, 1, 1, NULL);

	mpc_lock();
	g_status = ST_UNKNOWN;
	mpc_unlock();

	memset(g_buff, 0, sizeof(g_buff));
	g_buff_frame_size = g_buff_frame_start = 0;
	g_seek_seconds = 0;

	g_duration = g_play_time = 0.;

	memset(&g_taginfo, 0, sizeof(g_taginfo));

	return 0;
}

/**
 * 装载Musepack音乐文件 
 *
 * @param spath 短路径名
 * @param lpath 长路径名
 *
 * @return 成功时返回0
 */
static int mpc_load(const char *spath, const char *lpath)
{
	__init();

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

	data.seekable = TRUE;
	data.size = sceIoLseek(data.fd, 0, PSP_SEEK_END);
	sceIoLseek(data.fd, 0, PSP_SEEK_SET);

	reader.read = read_impl;
	reader.seek = seek_impl;
	reader.tell = tell_impl;
	reader.get_size = get_size_impl;
	reader.canseek = canseek_impl;
	reader.data = &data;

	mpc_streaminfo_init(&info);
	if (mpc_streaminfo_read(&info, &reader) != ERROR_CODE_OK) {
		__end();
		return -1;
	}

	if (info.average_bitrate != 0) {
		g_duration = (double) info.total_file_length * 8 / info.average_bitrate;
	}

	mpc_decoder_setup(&decoder, &reader);
	mpc_decoder_set_seeking(&decoder, &info, 0);

	if (!mpc_decoder_initialize(&decoder, &info)) {
		__end();
		return -1;
	}

	if (xMP3AudioInit() < 0) {
		__end();
		return -1;
	}

	if (xMP3AudioSetFrequency(info.sample_freq) < 0) {
		__end();
		return -1;
	}

	xMP3AudioSetChannelCallback(0, mpc_audiocallback, NULL);

	mpc_lock();
	g_status = ST_LOADED;
	mpc_unlock();

	return 0;
}

/**
 * 开始Musepack音乐文件的播放
 *
 * @return 成功时返回0
 */
static int mpc_play(void)
{
	mpc_lock();
	scene_power_playing_music(true);
	g_status = ST_PLAYING;
	mpc_unlock();

	return 0;
}

/**
 * 暂停Musepack音乐文件的播放
 *
 * @return 成功时返回0
 */
static int mpc_pause(void)
{
	mpc_lock();
	scene_power_playing_music(false);
	g_status = ST_PAUSED;
	mpc_unlock();

	return 0;
}

/**
 * 停止Musepack音乐文件的播放，销毁资源等
 *
 * @note 可以在播放线程中调用
 *
 * @return 成功时返回0
 */
static int __end(void)
{
	xMP3AudioEndPre();

	mpc_lock();
	g_status = ST_STOPPED;
	mpc_unlock();

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
 * 停止Musepack音乐文件的播放，销毁所占有的线程、资源等
 *
 * @note 不可以在播放线程中调用，必须能够多次重复调用而不死机
 *
 * @return 成功时返回0
 */
static int mpc_end(void)
{
	__end();

	xMP3AudioEnd();

	g_status = ST_STOPPED;

	return 0;
}

/**
 * 得到当前驱动的播放状态
 *
 * @return 状态
 */
static int mpc_get_status(void)
{
	return g_status;
}

/**
 * 快进Musepack音乐文件
 *
 * @param sec 秒数
 *
 * @return 成功时返回0
 */
static int mpc_fforward(int sec)
{
	mpc_lock();
	if (g_status == ST_PLAYING || g_status == ST_PAUSED)
		g_status = ST_FFOWARD;
	mpc_unlock();

	g_seek_seconds = sec;

	return 0;
}

/**
 * 快退Musepack音乐文件
 *
 * @param sec 秒数
 *
 * @return 成功时返回0
 */
static int mpc_fbackward(int sec)
{
	mpc_lock();
	if (g_status == ST_PLAYING || g_status == ST_PAUSED)
		g_status = ST_FBACKWARD;
	mpc_unlock();

	g_seek_seconds = sec;

	return 0;
}

/**
 * PSP准备休眠时Musepack的操作
 *
 * @return 成功时返回0
 */
static int mpc_suspend(void)
{
	g_suspend_status = g_status;
	g_suspend_playing_time = g_play_time;
	mpc_end();

	return 0;
}

/**
 * PSP准备从休眠时恢复的Musepack的操作
 *
 * @param spath 当前播放音乐名，8.3路径形式
 * @param lpath 当前播放音乐名，长文件名形式
 *
 * @return 成功时返回0
 */
static int mpc_resume(const char *spath, const char *lpath)
{
	int ret;

	ret = mpc_load(spath, lpath);
	if (ret != 0) {
		dbg_printf(d, "%s: mpc_load failed %d", __func__, ret);
		return -1;
	}

	g_play_time = g_suspend_playing_time;
	mpc_decoder_seek_seconds(&decoder, g_play_time);
	g_suspend_playing_time = 0;

	mpc_lock();
	g_status = g_suspend_status;
	mpc_unlock();
	g_suspend_status = ST_LOADED;

	return 0;
}

/**
 * 得到Musepack音乐文件相关信息
 *
 * @param pinfo 信息结构体指针
 *
 * @return
 */
static int mpc_get_info(struct music_info *pinfo)
{
	if (pinfo->type & MD_GET_TITLE) {
		pinfo->encode = conf_encode_utf8;
		STRCPY_S(pinfo->title, g_taginfo.title);
	}
	if (pinfo->type & MD_GET_ALBUM) {
		pinfo->encode = conf_encode_utf8;
		STRCPY_S(pinfo->album, g_taginfo.album);
	}
	if (pinfo->type & MD_GET_ARTIST) {
		pinfo->encode = conf_encode_utf8;
		STRCPY_S(pinfo->artist, g_taginfo.artist);
	}
	if (pinfo->type & MD_GET_COMMENT) {
		pinfo->encode = conf_encode_utf8;
		STRCPY_S(pinfo->comment, "");
	}
	if (pinfo->type & MD_GET_CURTIME) {
		pinfo->cur_time = g_play_time;
	}
	if (pinfo->type & MD_GET_DURATION) {
		pinfo->duration = g_duration;
	}
	if (pinfo->type & MD_GET_CPUFREQ) {
		pinfo->psp_freq[0] =
			66 + (120 - 66) * info.average_bitrate / 1000 / 320;
		pinfo->psp_freq[1] = 111;
	}
	if (pinfo->type & MD_GET_FREQ) {
		pinfo->freq = info.sample_freq;
	}
	if (pinfo->type & MD_GET_CHANNELS) {
		pinfo->channels = info.channels;
	}
	if (pinfo->type & MD_GET_AVGKBPS) {
		pinfo->avg_kbps = info.average_bitrate / 1000;
	}
	if (pinfo->type & MD_GET_DECODERNAME) {
		STRCPY_S(pinfo->decoder_name, "musepack");
	}
	if (pinfo->type & MD_GET_ENCODEMSG) {
		if (show_encoder_msg) {
			SPRINTF_S(pinfo->encode_msg,
					  "SV %lu.%lu, Profile %s (%s)", info.stream_version & 15,
					  info.stream_version >> 4, info.profile_name,
					  info.encoder);
		} else {
			pinfo->encode_msg[0] = '\0';
		}
	}

	return 0;
}

static struct music_ops mpc_ops = {
	.name = "musepack",
	.set_opt = mpc_set_opt,
	.load = mpc_load,
	.play = mpc_play,
	.pause = mpc_pause,
	.end = mpc_end,
	.get_status = mpc_get_status,
	.fforward = mpc_fforward,
	.fbackward = mpc_fbackward,
	.suspend = mpc_suspend,
	.resume = mpc_resume,
	.get_info = mpc_get_info,
	.next = NULL
};

int mpc_init(void)
{
	return register_musicdrv(&mpc_ops);
}
