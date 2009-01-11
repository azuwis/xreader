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
#define ENABLE_MUSIC
#include "scene.h"
#include "xmp3audiolib.h"
#include "musicmgr.h"
#include "musicdrv.h"
#include "All.h"
#include "CharacterHelper.h"
#include "APEDecompress.h"
#include "APEInfo.h"
#include "strsafe.h"
#include "common/utils.h"
#include "apetaglib/APETag.h"
#include "genericplayer.h"
#include "dbg.h"
#include "ssv.h"

#define BLOCKS_PER_DECODE (4096 / 4)
#define NUM_AUDIO_SAMPLES (BLOCKS_PER_DECODE * 4)

static int __end(void);

/**
 * APE���ֲ��Ż���
 */
static short *g_buff = NULL;

/**
 * APE���ֲ��Ż����С����֡����
 */
static unsigned g_buff_frame_size;

/**
 * APE���ֲ��Ż��嵱ǰλ�ã���֡����
 */
static int g_buff_frame_start;

/**
 * APE�����ļ����ȣ�������
 */
static double g_duration;

/**
 * APE����������
 */
static int g_ape_channels;

/**
 * APE����������
 */
static int g_ape_sample_freq;

/**
 * APE���ֱ�����
 */
static float g_ape_bitrate;

/**
 * APE��֡��
 */
static int g_ape_total_samples = 0;

/**
 * APE����ÿ����λ��
 */
static int g_ape_bits_per_sample = 0;

/**
 * APE�ļ���С
 */
static uint32_t g_ape_file_size = 0;

typedef struct _ape_taginfo_t
{
	char title[80];
	char artist[80];
	char album[80];
} ape_taginfo_t;

static ape_taginfo_t g_taginfo;

/**
 * APE����������
 */
static char g_encode_name[80];

/**
 * APE������
 */
static CAPEDecompress *g_decoder = NULL;

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
	int n;
	signed short *p = (signed short *) buf;

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

static int ape_seek_seconds(double seconds)
{
	uint32_t sample;

	if (g_duration == 0)
		return -1;

	if (seconds >= g_duration) {
		__end();
		return 0;
	}

	if (seconds < 0)
		seconds = 0;

	free_bitrate(&g_inst_br);
	sample = g_ape_total_samples * seconds / g_duration;

	dbg_printf(d, "Seeking to sample %d.", sample);

	int ret = g_decoder->Seek(sample);

	if (ret == ERROR_SUCCESS) {
		g_play_time = seconds;

		return 0;
	}

	return -1;
}

/**
 * ����������
 *
 * @return
 * - -1 should exit
 * - 0 OK
 */
static int handle_seek(void)
{
	u64 timer_end;

	if (1) {

		if (g_status == ST_FFOWARD) {
			sceRtcGetCurrentTick(&timer_end);

			generic_lock();
			if (g_last_seek_is_forward) {
				generic_unlock();

				if (pspDiffTime(&timer_end, &g_last_seek_tick) <= 1.0) {
					generic_lock();

					if (g_seek_count > 0) {
						g_play_time += g_seek_seconds;
						g_seek_count--;
					}

					generic_unlock();

					if (g_play_time >= g_duration) {
						return -1;
					}

					sceKernelDelayThread(100000);
				} else {
					generic_lock();

					if (g_seek_count == 0) {
						scene_power_playing_music(true);

						if (ape_seek_seconds(g_play_time) < 0) {
							generic_unlock();
							return -1;
						}

						g_status = ST_PLAYING;
					}

					generic_unlock();
				}
			} else {
				generic_unlock();
			}
		} else if (g_status == ST_FBACKWARD) {
			sceRtcGetCurrentTick(&timer_end);

			generic_lock();
			if (!g_last_seek_is_forward) {
				generic_unlock();

				if (pspDiffTime(&timer_end, &g_last_seek_tick) <= 1.0) {
					generic_lock();

					if (g_seek_count > 0) {
						g_play_time -= g_seek_seconds;
						g_seek_count--;
					}

					generic_unlock();

					if (g_play_time < 0) {
						g_play_time = 0;
					}

					sceKernelDelayThread(100000);
				} else {
					generic_lock();

					if (g_seek_count == 0) {
						scene_power_playing_music(true);

						if (ape_seek_seconds(g_play_time) < 0) {
							generic_unlock();
							return -1;
						}

						g_status = ST_PLAYING;
					}

					generic_unlock();
				}
			} else {
				generic_unlock();
			}
		}

		return 0;
	} else {
		if (g_status == ST_FFOWARD) {
			generic_lock();
			g_status = ST_PLAYING;
			generic_unlock();
			scene_power_playing_music(true);
			ape_seek_seconds(g_play_time + g_seek_seconds);
		} else if (g_status == ST_FBACKWARD) {
			generic_lock();
			g_status = ST_PLAYING;
			generic_unlock();
			scene_power_playing_music(true);
			ape_seek_seconds(g_play_time - g_seek_seconds);
		}
	}

	return 0;
}

/**
 * APE���ֲ��Żص�������
 * ���𽫽��������������������
 *
 * @note �����������ĸ�ʽΪ˫������16λ������
 *
 * @param buf ����������ָ��
 * @param reqn ������֡��С
 * @param pdata �û����ݣ�����
 */
static int ape_audiocallback(void *buf, unsigned int reqn, void *pdata)
{
	int avail_frame;
	int snd_buf_frame_size = (int) reqn;
	int ret;
	double incr;
	signed short *audio_buf = (signed short *) buf;

	UNUSED(pdata);

	if (g_status != ST_PLAYING) {
		if (handle_seek() == -1) {
			__end();
			return -1;
		}

		clear_snd_buf(buf, snd_buf_frame_size);
		return 0;
	}

	while (snd_buf_frame_size > 0) {
		avail_frame = g_buff_frame_size - g_buff_frame_start;

		if (avail_frame >= snd_buf_frame_size) {
			send_to_sndbuf(audio_buf,
						   &g_buff[g_buff_frame_start * g_ape_channels],
						   snd_buf_frame_size, g_ape_channels);
			g_buff_frame_start += snd_buf_frame_size;
			audio_buf += snd_buf_frame_size * 2;
			snd_buf_frame_size = 0;
		} else {
			send_to_sndbuf(audio_buf,
						   &g_buff[g_buff_frame_start * g_ape_channels],
						   avail_frame, g_ape_channels);
			snd_buf_frame_size -= avail_frame;
			audio_buf += avail_frame * 2;
			int block = -1;

			ret =
				g_decoder->GetData((char *) g_buff, BLOCKS_PER_DECODE, &block);

			if (ret != ERROR_SUCCESS || block == 0) {
				__end();
				return -1;
			}

			g_buff_frame_size = block;
			g_buff_frame_start = 0;

			incr = 1.0 * g_buff_frame_size / g_ape_sample_freq;
			g_play_time += incr;

			add_bitrate(&g_inst_br,
						g_decoder->GetInfo(APE_DECOMPRESS_CURRENT_BITRATE) *
						1000, incr);
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
	g_duration = g_play_time = 0.;
	g_ape_bitrate = g_ape_sample_freq = g_ape_channels = 0;;
	memset(&g_taginfo, 0, sizeof(g_taginfo));
	g_decoder = NULL;
	g_encode_name[0] = '\0';

	return 0;
}

/**
 * װ��APE�����ļ� 
 *
 * @param spath ��·����
 * @param lpath ��·����
 *
 * @return �ɹ�ʱ����0
 */
static int ape_load(const char *spath, const char *lpath)
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

	if (g_buff != NULL) {
		free(g_buff);
		g_buff = NULL;
	}

	g_buff = (short int *) calloc(NUM_AUDIO_SAMPLES, sizeof(*g_buff));

	if (g_buff == NULL) {
		__end();
		return -1;
	}

	CSmartPtr < str_utf16 > path(GetUTF16FromANSI(spath));
	int err;
	CAPEInfo ape_info(&err, path);

	if (err == ERROR_SUCCESS) {
		g_ape_sample_freq = ape_info.GetInfo(APE_INFO_SAMPLE_RATE);
		g_ape_channels = ape_info.GetInfo(APE_INFO_CHANNELS);
		g_ape_total_samples = ape_info.GetInfo(APE_INFO_TOTAL_BLOCKS);
		g_ape_file_size = ape_info.GetInfo(APE_INFO_APE_TOTAL_BYTES);
		g_ape_bitrate = ape_info.GetInfo(APE_INFO_AVERAGE_BITRATE) * 1000;
		g_ape_bits_per_sample = ape_info.GetInfo(APE_INFO_BITS_PER_SAMPLE);

		if (g_ape_total_samples > 0) {
			g_duration = 1.0 * g_ape_total_samples / g_ape_sample_freq;
		} else {
			g_duration = 0;
		}
	} else {
		__end();
		return -1;
	}

	if (xMP3AudioInit() < 0) {
		__end();
		return -1;
	}

	if (xMP3AudioSetFrequency(g_ape_sample_freq) < 0) {
		__end();
		return -1;
	}

	generic_lock();
	g_status = ST_LOADED;
	generic_unlock();

	dbg_printf(d,
			   "[%d channel(s), %d Hz, %.2f kbps, %02d:%02d, encoder: %s, Ratio: %.3f]",
			   g_ape_channels, g_ape_sample_freq, g_ape_bitrate / 1000,
			   (int) (g_duration / 60), (int) g_duration % 60, g_encode_name,
			   1.0 * g_ape_file_size / (g_ape_total_samples *
										g_ape_channels *
										(g_ape_bits_per_sample / 8))
		);

	dbg_printf(d, "[%s - %s - %s, ape tag]", g_taginfo.artist, g_taginfo.album,
			   g_taginfo.title);

	g_decoder = (CAPEDecompress *) CreateIAPEDecompress(path, &err);

	if (err != ERROR_SUCCESS) {
		__end();
		return -1;
	}

	xMP3AudioSetChannelCallback(0, ape_audiocallback, NULL);

	return 0;
}

/**
 * ֹͣAPE�����ļ��Ĳ��ţ�������Դ��
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
 * ֹͣAPE�����ļ��Ĳ��ţ�������ռ�е��̡߳���Դ��
 *
 * @note �������ڲ����߳��е��ã������ܹ�����ظ����ö�������
 *
 * @return �ɹ�ʱ����0
 */
static int ape_end(void)
{
	__end();

	xMP3AudioEnd();

	if (g_buff != NULL) {
		free(g_buff);
		g_buff = NULL;
	}

	g_status = ST_STOPPED;

	if (g_decoder) {
		delete g_decoder;

		g_decoder = NULL;
	}

	free_bitrate(&g_inst_br);
	generic_end();

	return 0;
}

/**
 * PSP׼������ʱAPE�Ĳ���
 *
 * @return �ɹ�ʱ����0
 */
static int ape_suspend(void)
{
	generic_suspend();
	ape_end();

	return 0;
}

/**
 * PSP׼��������ʱ�ָ���APE�Ĳ���
 *
 * @param spath ��ǰ������������8.3·����ʽ
 * @param lpath ��ǰ���������������ļ�����ʽ
 *
 * @return �ɹ�ʱ����0
 */
static int ape_resume(const char *spath, const char *lpath)
{
	int ret;

	ret = ape_load(spath, lpath);
	if (ret != 0) {
		dbg_printf(d, "%s: ape_load failed %d", __func__, ret);
		return -1;
	}

	g_play_time = g_suspend_playing_time;
	ape_seek_seconds(g_play_time);
	g_suspend_playing_time = 0;

	generic_resume(spath, lpath);

	return 0;
}

/**
 * �õ�APE�����ļ������Ϣ
 *
 * @param pinfo ��Ϣ�ṹ��ָ��
 *
 * @return
 */
static int ape_get_info(struct music_info *pinfo)
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
		pinfo->psp_freq[0] = 233;
		pinfo->psp_freq[1] = 116;
	}
	if (pinfo->type & MD_GET_FREQ) {
		pinfo->freq = g_ape_sample_freq;
	}
	if (pinfo->type & MD_GET_CHANNELS) {
		pinfo->channels = g_ape_channels;
	}
	if (pinfo->type & MD_GET_AVGKBPS) {
		pinfo->avg_kbps = g_ape_bitrate / 1000;
	}
	if (pinfo->type & MD_GET_INSKBPS) {
		pinfo->ins_kbps = get_inst_bitrate(&g_inst_br) / 1000;
	}
	if (pinfo->type & MD_GET_DECODERNAME) {
		STRCPY_S(pinfo->decoder_name, "ape");
	}
	if (pinfo->type & MD_GET_ENCODEMSG) {
		if (show_encoder_msg) {
			SPRINTF_S(pinfo->encode_msg, "%s Ratio: %.3f", g_encode_name,
					  1.0 * g_ape_file_size / (g_ape_total_samples *
											   g_ape_channels *
											   (g_ape_bits_per_sample / 8)));
		} else {
			pinfo->encode_msg[0] = '\0';
		}
	}

	return 0;
}

static struct music_ops ape_ops = {
	"ape",
	NULL,
	ape_load,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	ape_get_info,
	ape_suspend,
	ape_resume,
	ape_end,
	NULL
};

extern "C" int ape_init(void)
{
	return register_musicdrv(&ape_ops);
}
