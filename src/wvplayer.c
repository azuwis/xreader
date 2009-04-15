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
#include "dbg.h"
#include "ssv.h"
#include "genericplayer.h"
#include "wavpack/wavpack.h"
#include "musicinfo.h"

static int __end(void);

#define MAX_BLOCK_SIZE (1024 * 8)

/**
 * WvPack音乐播放缓冲
 */
static uint16_t *g_buff = NULL;

/**
 * WvPack音乐播放缓冲总大小, 以帧数计
 */
static unsigned g_buff_size;

/**
 * WvPack音乐播放缓冲大小，以帧数计
 */
static unsigned g_buff_frame_size;

/**
 * WvPack音乐播放缓冲当前位置，以帧数计
 */
static int g_buff_frame_start;

/**
 * WvPack音乐每样本位数
 */
static int g_wv_bits_per_sample = 0;

/**
 * WvPack编码器名字
 */
static char g_encode_name[80];

/**
 * Wvpack解码器
 */
static WavpackContext *g_decoder = NULL;

/**
 * WvPack解码32位样本指针
 */
static int32_t *wv_buffer = NULL;

/**
 * WvPack解码模式
 */
static int g_mode = 0;

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

static int wv_seek_seconds(double seconds)
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

	if (WavpackSeekSample(g_decoder, seconds * g_info.sample_freq) != 1) {
		__end();
		return -1;
	}

	g_play_time = seconds;

	return 0;
}

/**
 * WvPack音乐播放回调函数，
 * 负责将解码数据填充声音缓存区
 *
 * @note 声音缓存区的格式为双声道，16位低字序
 *
 * @param buf 声音缓冲区指针
 * @param reqn 缓冲区帧大小
 * @param pdata 用户数据，无用
 */
static int wv_audiocallback(void *buf, unsigned int reqn, void *pdata)
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
			wv_seek_seconds(g_play_time + g_seek_seconds);
		} else if (g_status == ST_FBACKWARD) {
			generic_lock();
			g_status = ST_PLAYING;
			generic_set_playback(true);
			generic_unlock();
			wv_seek_seconds(g_play_time - g_seek_seconds);
		}
		xMP3ClearSndBuf(buf, snd_buf_frame_size);
		sceKernelDelayThread(100000);
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

			int ret;

			ret = WavpackUnpackSamples(g_decoder, wv_buffer, MAX_BLOCK_SIZE);

			if (ret > 0) {

				if (ret > g_buff_size) {
					g_buff_size = ret;
					g_buff = safe_realloc(g_buff, g_buff_size * g_info.channels * sizeof(*g_buff));

					if (g_buff == NULL) {
						__end();
						return -1;
					}
				}

				int i;
				uint8_t *output = (uint8_t*) g_buff;

				for(i=0; i<ret; ++i) {
					*output++ = wv_buffer[2 * i];
					*output++ = wv_buffer[2 * i] >> 8;
					*output++ = wv_buffer[2 * i + 1];
					*output++ = wv_buffer[2 * i + 1] >> 8;
				}
			}

			g_buff_frame_size = ret;
			g_buff_frame_start = 0;

			incr = 1.0 * g_buff_frame_size / g_info.sample_freq;

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
	generic_init();
	generic_lock();
	g_status = ST_UNKNOWN;
	generic_unlock();

	g_buff_frame_size = g_buff_frame_start = 0;
	g_seek_seconds = 0;
	g_play_time = 0.;
	memset(&g_info, 0, sizeof(g_info));
	wv_buffer = NULL;
	g_decoder = NULL;
	g_encode_name[0] = '\0';
	g_mode = 0;

	return 0;
}

static void wv_get_tag(void)
{
	int n;
	
	n = WavpackGetTagItem(g_decoder, "Title", g_info.tag.title, sizeof(g_info.tag.title));

	if (n != 0) {
		g_info.tag.type = APETAG;
		g_info.tag.encode = conf_encode_utf8;
	} else {
		WavpackGetTagItem(g_decoder, "title", g_info.tag.title, sizeof(g_info.tag.title));
		g_info.tag.type = ID3V1;
		g_info.tag.encode = config.mp3encode;
	}

	n = WavpackGetTagItem(g_decoder, "Artist", g_info.tag.artist, sizeof(g_info.tag.artist));

	if (n != 0) {
		g_info.tag.type = APETAG;
		g_info.tag.encode = conf_encode_utf8;
	} else {
		WavpackGetTagItem(g_decoder, "artist", g_info.tag.artist, sizeof(g_info.tag.artist));
		g_info.tag.type = ID3V1;
		g_info.tag.encode = config.mp3encode;
	}

	n = WavpackGetTagItem(g_decoder, "Album", g_info.tag.album, sizeof(g_info.tag.album));

	if (n != 0) {
		g_info.tag.type = APETAG;
		g_info.tag.encode = conf_encode_utf8;
	} else {
		WavpackGetTagItem(g_decoder, "album", g_info.tag.album, sizeof(g_info.tag.album));
		g_info.tag.type = ID3V1;
		g_info.tag.encode = config.mp3encode;
	}
}

/**
 * 装载WvPack音乐文件 
 *
 * @param spath 短路径名
 * @param lpath 长路径名
 *
 * @return 成功时返回0
 */
static int wv_load(const char *spath, const char *lpath)
{
	int fd;

	__init();

	if (g_buff != NULL) {
		free(g_buff);
		g_buff = NULL;
	}

	g_buff_size = MAX_BLOCK_SIZE * 2;
	g_buff = calloc(g_buff_size, sizeof(*g_buff));

	if (g_buff == NULL) {
		__end();
		return -1;
	}

	fd = sceIoOpen(spath, PSP_O_RDONLY, 0777);

	if (fd < 0) {
		__end();
		return -1;
	}

	g_info.filesize = sceIoLseek(fd, 0, PSP_SEEK_END);
	sceIoClose(fd);

	char error[80];

	g_decoder = WavpackOpenFileInput(spath, error, OPEN_WVC | OPEN_TAGS | OPEN_2CH_MAX | OPEN_NORMALIZE, 23);

	if (g_decoder == NULL) {
		__end();
		return -1;
	}

	g_wv_bits_per_sample = WavpackGetBitsPerSample(g_decoder);

	if ( g_wv_bits_per_sample != 16 ) {
		__end();
		return -1;
	}

	g_info.sample_freq = WavpackGetSampleRate(g_decoder);
	g_info.channels = WavpackGetNumChannels(g_decoder);

	if (g_info.channels != 1 && g_info.channels != 2) {
		__end();
		return -1;
	}

	wv_buffer = calloc(MAX_BLOCK_SIZE * g_info.channels, sizeof(*wv_buffer));

	if (wv_buffer == NULL) {
		__end();
		return -1;
	}

	if (g_info.sample_freq != 0) {
		g_info.samples = WavpackGetNumSamples(g_decoder);
		g_info.duration = (double) g_info.samples / g_info.sample_freq;
	} else {
		__end();
		return -1;
	}

	g_info.avg_bps = WavpackGetAverageBitrate(g_decoder, 1);

	g_mode = WavpackGetMode(g_decoder);

	if (g_mode & MODE_VALID_TAG) {
		wv_get_tag();
	}

	g_encode_name[0] = '\0';

	if (g_mode & MODE_WVC) {
		STRCAT_S(g_encode_name, " WVC");
	}

	if (g_mode & MODE_HYBRID) {
		STRCAT_S(g_encode_name, " HYBRID");
	}

	if (g_mode & MODE_LOSSLESS) {
		STRCAT_S(g_encode_name, " LOSSLESS");
	} else {
		STRCAT_S(g_encode_name, " LOSSY");
	}

	if (g_mode & MODE_HIGH) {
		STRCAT_S(g_encode_name, " HIGH");
	}

	if (g_mode & MODE_FAST) {
		STRCAT_S(g_encode_name, " FAST");
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
										 (g_wv_bits_per_sample / 8))
		);

	dbg_printf(d, "[%s - %s - %s, wv tag]", g_info.tag.artist, g_info.tag.album,
			   g_info.tag.title);

	xMP3AudioSetChannelCallback(0, wv_audiocallback, NULL);

	return 0;
}

/**
 * 停止WvPack音乐文件的播放，销毁资源等
 *
 * @note 可以在播放线程中调用
 *
 * @return 成功时返回0
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
 * 停止WvPack音乐文件的播放，销毁所占有的线程、资源等
 *
 * @note 不可以在播放线程中调用，必须能够多次重复调用而不死机
 *
 * @return 成功时返回0
 */
static int wv_end(void)
{
	__end();

	xMP3AudioEnd();

	if (wv_buffer != NULL) {
		free(wv_buffer);
		wv_buffer = NULL;
	}

	if (g_buff != NULL) {
		free(g_buff);
		g_buff = NULL;
	}

	g_status = ST_STOPPED;

	if (g_decoder) {
		WavpackCloseFile(g_decoder);
		g_decoder = NULL;
	}

	free_bitrate(&g_inst_br);
	generic_end();

	return 0;
}

/**
 * PSP准备休眠时WvPack的操作
 *
 * @return 成功时返回0
 */
static int wv_suspend(void)
{
	generic_suspend();
	wv_end();

	return 0;
}

/**
 * PSP准备从休眠时恢复的WvPack的操作
 *
 * @param spath 当前播放音乐名，8.3路径形式
 * @param lpath 当前播放音乐名，长文件或形式
 *
 * @return 成功时返回0
 */
static int wv_resume(const char *spath, const char *lpath)
{
	int ret;

	ret = wv_load(spath, lpath);
	if (ret != 0) {
		dbg_printf(d, "%s: wv_load failed %d", __func__, ret);
		return -1;
	}

	g_play_time = g_suspend_playing_time;
	wv_seek_seconds(g_play_time);
	g_suspend_playing_time = 0;

	generic_resume(spath, lpath);

	return 0;
}

/**
 * 得到WvPack音乐文件相关信息
 *
 * @param pinfo 信息结构体指针
 *
 * @return
 */
static int wv_get_info(struct music_info *pinfo)
{
	if (pinfo->type & MD_GET_CURTIME) {
		pinfo->cur_time = g_play_time;
	}
	if (pinfo->type & MD_GET_CPUFREQ) {
		if (g_mode & MODE_WVC)
			pinfo->psp_freq[0] = 266;
		else
			pinfo->psp_freq[0] = 222;

		pinfo->psp_freq[1] = 111;
	}
	if (pinfo->type & MD_GET_INSKBPS) {
		if (g_decoder != NULL)
			pinfo->ins_kbps = WavpackGetInstantBitrate(g_decoder) / 1000;
		else
			pinfo->ins_kbps = g_info.avg_bps / 1000;
	}
	if (pinfo->type & MD_GET_DECODERNAME) {
		STRCPY_S(pinfo->decoder_name, "wv");
	}
	if (pinfo->type & MD_GET_ENCODEMSG) {
		if (show_encoder_msg) {
			SPRINTF_S(pinfo->encode_msg, "%s Ratio: %.3f", g_encode_name,
					  1.0 * g_info.filesize / (g_info.samples *
												g_info.channels *
												(g_wv_bits_per_sample / 8)));
		} else {
			pinfo->encode_msg[0] = '\0';
		}
	}

	return generic_get_info(pinfo);
}

/**
 * 检测是否为WvPack文件，目前只检查文件后缀名
 *
 * @param spath 当前播放音乐名，8.3路径形式
 *
 * @return 是WvPack文件返回1，否则返回0
 */
static int wv_probe(const char* spath)
{
	const char *p;

	p = utils_fileext(spath);

	if (p) {
		if (stricmp(p, "wv") == 0) {
			return 1;
		}
	}

	return 0;
}

static struct music_ops wv_ops = {
	.name = "wv",
	.set_opt = NULL,
	.load = wv_load,
	.play = NULL,
	.pause = NULL,
	.end = wv_end,
	.get_status = NULL,
	.fforward = NULL,
	.fbackward = NULL,
	.suspend = wv_suspend,
	.resume = wv_resume,
	.get_info = wv_get_info,
	.probe = wv_probe,
	.next = NULL
};

int wv_init(void)
{
	return register_musicdrv(&wv_ops);
}

