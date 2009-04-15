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
#include <wmadec.h>
#include "config.h"
#include "ssv.h"
#include "scene.h"
#include "xmp3audiolib.h"
#include "musicmgr.h"
#include "musicdrv.h"
#include "strsafe.h"
#include "common/utils.h"
#include "genericplayer.h"
#include "musicinfo.h"
#include "dbg.h"

static int __end(void);

/**
 * WMA���ֲ��Ż���
 */
static uint16_t *g_buff = NULL;

/**
 * WMA���ֲ��Ż����ܴ�С, ��֡����
 */
static unsigned g_buff_size;

/**
 * WMA���ֲ��Ż����С����֡����
 */
static unsigned g_buff_frame_size;

/**
 * WMA���ֲ��Ż��嵱ǰλ�ã���֡����
 */
static int g_buff_frame_start;

/**
 * WMA������
 */
static WmaFile *decoder = NULL;

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
	if (frames <= 0)
		return;

	memcpy(buf, srcbuf, frames * channels * sizeof(*srcbuf));
}

static void wma_seek_seconds(WmaFile *decoder, double npt)
{
	if (decoder)
		wma_seek(decoder, (long long)npt);
}

/**
 * WMA���ֲ��Żص�������
 * ���𽫽��������������������
 *
 * @note �����������ĸ�ʽΪ˫������16λ������
 *
 * @param buf ����������ָ��
 * @param reqn ������֡��С
 * @param pdata �û����ݣ�����
 */
static int wma_audiocallback(void *buf, unsigned int reqn, void *pdata)
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
			wma_seek_seconds(decoder, g_play_time);
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
			wma_seek_seconds(decoder, g_play_time);
			g_buff_frame_size = g_buff_frame_start = 0;
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

			uint8_t *buf = (uint8_t *) wma_decode_frame(decoder, &ret);

			if (buf == NULL) {
				__end();

				return -1;
			}

			int samplesdecoded = ret / 2 / g_info.channels;

			if (decoder->channels == 2) {
				memcpy((uint8_t*) g_buff, buf, samplesdecoded * 4);
			} else {
				int i;
				uint8_t *output = (uint8_t*) g_buff;

				for(i=0; i<samplesdecoded; ++i) {
					*output++ = buf[i * 2];
					*output++ = buf[i * 2 + 1];
					*output++ = buf[i * 2];
					*output++ = buf[i * 2 + 1];
				}
			}

			g_buff_frame_size = samplesdecoded;
			g_buff_frame_start = 0;

			incr = (double) samplesdecoded / g_info.sample_freq;
			g_play_time += incr;
#if 0
			add_bitrate(&g_inst_br,
						ret * 8 * g_info.sample_freq / WMA_MAX_BUF_SIZE,
						incr);
#endif
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
	g_buff = NULL;
	g_buff_size = 0;

	return 0;
}

#if 1
static void get_wma_tag(void)
{
	g_info.tag.type = WMATAG;
	g_info.tag.encode = config.mp3encode;

	dbg_printf(d, "Dump title:");
	dbg_hexdump_ascii(d, decoder->title, sizeof(decoder->title));

	if (decoder->title)
		STRCPY_S(g_info.tag.title, decoder->title);
	else
		STRCPY_S(g_info.tag.title, "");
	
	STRCPY_S(g_info.tag.album, "");
	
	dbg_printf(d, "Dump author:");
	dbg_hexdump_ascii(d, decoder->author, sizeof(decoder->author));
	
	if (decoder->author)
		STRCPY_S(g_info.tag.artist, decoder->author);
	else
		STRCPY_S(g_info.tag.artist, "");
}
#endif

/**
 * װ��WMA�����ļ� 
 *
 * @param spath ��·����
 * @param lpath ��·����
 *
 * @return �ɹ�ʱ����0
 */
static int wma_load(const char *spath, const char *lpath)
{
	int fd;

	__init();

	wma_init();
	g_buff_size = WMA_MAX_BUF_SIZE / 2;
	g_buff = calloc(1, g_buff_size * sizeof(g_buff[0]));

	if (g_buff == NULL) {
		__end();
		return -1;
	}

	fd = sceIoOpen(spath, PSP_O_RDONLY, 0777);

	if (fd < 0) {
		__end();
		return -1;
	}

	g_info.filesize = sceIoLseek32(fd, 0, PSP_SEEK_END);
	sceIoClose(fd);

	decoder = wma_open(spath);

	if (decoder == NULL) {
		__end();
		return -1;
	}

	if (decoder->channels > 2 || decoder->channels <= 0) {
		__end();
		return -1;
	}

	g_info.sample_freq = decoder->samplerate;
	g_info.channels = decoder->channels;
	g_info.duration = (double) decoder->duration / 1000;
	g_info.avg_bps = decoder->bitrate;

	get_wma_tag();

	if (xMP3AudioInit() < 0) {
		__end();
		return -1;
	}

	if (xMP3AudioSetFrequency(g_info.sample_freq) < 0) {
		__end();
		return -1;
	}

	xMP3AudioSetChannelCallback(0, wma_audiocallback, NULL);

	generic_lock();
	g_status = ST_LOADED;
	generic_unlock();

	return 0;
}

/**
 * ֹͣWMA�����ļ��Ĳ��ţ�������Դ��
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
 * ֹͣWMA�����ļ��Ĳ��ţ�������ռ�е��̡߳���Դ��
 *
 * @note �������ڲ����߳��е��ã������ܹ�����ظ����ö�������
 *
 * @return �ɹ�ʱ����0
 */
static int wma_end(void)
{
	__end();

	xMP3AudioEnd();

	g_status = ST_STOPPED;

	if (decoder != NULL) {
		wma_close(decoder);
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
 * PSP׼������ʱWMA�Ĳ���
 *
 * @return �ɹ�ʱ����0
 */
static int wma_suspend(void)
{
	generic_suspend();
	wma_end();

	return 0;
}

/**
 * PSP׼��������ʱ�ָ���WMA�Ĳ���
 *
 * @param spath ��ǰ������������8.3·����ʽ
 * @param lpath ��ǰ���������������ļ�����ʽ
 *
 * @return �ɹ�ʱ����0
 */
static int wma_resume(const char *spath, const char *lpath)
{
	int ret;

	ret = wma_load(spath, lpath);
	if (ret != 0) {
		dbg_printf(d, "%s: wma_load failed %d", __func__, ret);
		return -1;
	}

	g_play_time = g_suspend_playing_time;
	free_bitrate(&g_inst_br);
	wma_seek_seconds(decoder, g_play_time);
	g_suspend_playing_time = 0;

	generic_resume(spath, lpath);

	return 0;
}

/**
 * �õ�WMA�����ļ������Ϣ
 *
 * @param pinfo ��Ϣ�ṹ��ָ��
 *
 * @return
 */
static int wma_get_info(struct music_info *pinfo)
{
	if (pinfo->type & MD_GET_CURTIME) {
		pinfo->cur_time = g_play_time;
	}
	if (pinfo->type & MD_GET_CPUFREQ) {
#if 0
		pinfo->psp_freq[0] =
			66 + (120 - 66) * g_info.avg_bps / 1000 / 320;
#else
		pinfo->psp_freq[0] = 333;
#endif
		pinfo->psp_freq[1] = 166;
	}
	if (pinfo->type & MD_GET_INSKBPS) {
		pinfo->ins_kbps = get_inst_bitrate(&g_inst_br) / 1000;
	}
	if (pinfo->type & MD_GET_DECODERNAME) {
		STRCPY_S(pinfo->decoder_name, "WMA");
	}
	if (pinfo->type & MD_GET_ENCODEMSG) {
		if (show_encoder_msg) {
#if 0
			SPRINTF_S(pinfo->encode_msg,
					  "SV %lu.%lu, Profile %s (%s)", info.stream_version & 15,
					  info.stream_version >> 4, info.profile_name,
					  info.encoder);
#endif
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
static int wma_probe(const char* spath)
{
	const char *p;

	p = utils_fileext(spath);

	if (p) {
		if (stricmp(p, "wma") == 0) {
			return 1;
		}
		if (stricmp(p, "wmv") == 0) {
			return 1;
		}
	}

	return 0;
}

static struct music_ops wma_ops = {
	.name = "wma",
	.set_opt = NULL,
	.load = wma_load,
	.play = NULL,
	.pause = NULL,
	.end = wma_end,
	.get_status = NULL,
	.fforward = NULL,
	.fbackward = NULL,
	.suspend = wma_suspend,
	.resume = wma_resume,
	.get_info = wma_get_info,
	.probe = wma_probe,
	.next = NULL
};

int wmadrv_init(void)
{
	return register_musicdrv(&wma_ops);
}

