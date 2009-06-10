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
#include "WavPack/wavpack.h"
#include "musicinfo.h"
#include "xrhal.h"
#include "simple_gettext.h"
#ifdef DMALLOC
#include "dmalloc.h"
#endif

#ifdef ENABLE_WAVPACK

#define WVPACK_BUFFERED_READER_BUFFER_SIZE (256 * 1024)

static int __end(void);

#define MAX_BLOCK_SIZE (1024 * 8)

/**
 * WvPack���ֲ��Ż���
 */
static uint16_t *g_buff = NULL;

/**
 * WvPack���ֲ��Ż����ܴ�С, ��֡����
 */
static unsigned g_buff_size;

/**
 * WvPack���ֲ��Ż����С����֡����
 */
static unsigned g_buff_frame_size;

/**
 * WvPack���ֲ��Ż��嵱ǰλ�ã���֡����
 */
static int g_buff_frame_start;

/**
 * WvPack����ÿ����λ��
 */
static int g_wv_bits_per_sample = 0;

/**
 * WvPack����������
 */
static char g_encode_name[80];

/**
 * Wvpack������
 */
static WavpackContext *g_decoder = NULL;

/**
 * WvPack����32λ����ָ��
 */
static int32_t *wv_buffer = NULL;

/**
 * WvPack����ģʽ
 */
static int g_mode = 0;

static buffered_reader_t *wv = NULL, *wvc = NULL;

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
 * ����������
 *
 * @return
 * - -1 should exit
 * - 0 OK
 */
static int handle_seek(void)
{
	u64 timer_end;

	if (g_status == ST_FFORWARD) {
		xrRtcGetCurrentTick(&timer_end);

		generic_lock();
		if (g_last_seek_is_forward) {
			generic_unlock();

			if (pspDiffTime(&timer_end, (u64 *) & g_last_seek_tick) <= 1.0) {
				generic_lock();

				if (g_seek_count > 0) {
					g_play_time += g_seek_seconds;
					g_seek_count--;
				}

				generic_unlock();

				if (g_play_time >= g_info.duration) {
					return -1;
				}

				xrKernelDelayThread(100000);
			} else {
				generic_lock();

				g_seek_count = 0;
				generic_set_playback(true);

				if (wv_seek_seconds(g_play_time) < 0) {
					generic_unlock();
					return -1;
				}

				g_status = ST_PLAYING;

				generic_unlock();
				xrKernelDelayThread(100000);
			}
		} else {
			generic_unlock();
		}
	} else if (g_status == ST_FBACKWARD) {
		xrRtcGetCurrentTick(&timer_end);

		generic_lock();
		if (!g_last_seek_is_forward) {
			generic_unlock();

			if (pspDiffTime(&timer_end, (u64 *) & g_last_seek_tick) <= 1.0) {
				generic_lock();

				if (g_seek_count > 0) {
					g_play_time -= g_seek_seconds;
					g_seek_count--;
				}

				generic_unlock();

				if (g_play_time < 0) {
					g_play_time = 0;
				}

				xrKernelDelayThread(100000);
			} else {
				generic_lock();

				g_seek_count = 0;
				generic_set_playback(true);

				if (wv_seek_seconds(g_play_time) < 0) {
					generic_unlock();
					return -1;
				}

				g_status = ST_PLAYING;

				generic_unlock();
				xrKernelDelayThread(100000);
			}
		} else {
			generic_unlock();
		}
	}

	return 0;
}

/**
 * WvPack���ֲ��Żص�������
 * ���𽫽��������������������
 *
 * @note �����������ĸ�ʽΪ˫������16λ������
 *
 * @param buf ����������ָ��
 * @param reqn ������֡��С
 * @param pdata �û����ݣ�����
 */
static int wv_audiocallback(void *buf, unsigned int reqn, void *pdata)
{
	int avail_frame;
	int snd_buf_frame_size = (int) reqn;
	double incr;
	signed short *audio_buf = buf;

	UNUSED(pdata);

	if (g_status != ST_PLAYING) {
		if (handle_seek() == -1) {
			__end();
			return -1;
		}

		xMP3ClearSndBuf(buf, snd_buf_frame_size);
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

			int ret;

			ret = WavpackUnpackSamples(g_decoder, wv_buffer, MAX_BLOCK_SIZE);

			if (ret > 0) {

				if (ret > g_buff_size) {
					g_buff_size = ret;
					g_buff =
						safe_realloc(g_buff,
									 g_buff_size * g_info.channels *
									 sizeof(*g_buff));

					if (g_buff == NULL) {
						__end();
						return -1;
					}
				}

				int i;
				uint8_t *output = (uint8_t *) g_buff;

				for (i = 0; i < ret; ++i) {
					*output++ = wv_buffer[2 * i];
					*output++ = wv_buffer[2 * i] >> 8;
					*output++ = wv_buffer[2 * i + 1];
					*output++ = wv_buffer[2 * i + 1] >> 8;
				}
			} else {
				__end();
				return -1;
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
	wv_buffer = NULL;
	g_decoder = NULL;
	g_encode_name[0] = '\0';
	g_mode = 0;

	return 0;
}

static void wv_get_tag(void)
{
	int n;

	n = WavpackGetTagItem(g_decoder, "Title", g_info.tag.title,
						  sizeof(g_info.tag.title));

	if (n != 0) {
		g_info.tag.type = APETAG;
		g_info.tag.encode = conf_encode_utf8;
	} else {
		WavpackGetTagItem(g_decoder, "title", g_info.tag.title,
						  sizeof(g_info.tag.title));
		g_info.tag.type = ID3V1;
		g_info.tag.encode = config.mp3encode;
	}

	n = WavpackGetTagItem(g_decoder, "Artist", g_info.tag.artist,
						  sizeof(g_info.tag.artist));

	if (n != 0) {
		g_info.tag.type = APETAG;
		g_info.tag.encode = conf_encode_utf8;
	} else {
		WavpackGetTagItem(g_decoder, "artist", g_info.tag.artist,
						  sizeof(g_info.tag.artist));
		g_info.tag.type = ID3V1;
		g_info.tag.encode = config.mp3encode;
	}

	n = WavpackGetTagItem(g_decoder, "Album", g_info.tag.album,
						  sizeof(g_info.tag.album));

	if (n != 0) {
		g_info.tag.type = APETAG;
		g_info.tag.encode = conf_encode_utf8;
	} else {
		WavpackGetTagItem(g_decoder, "album", g_info.tag.album,
						  sizeof(g_info.tag.album));
		g_info.tag.type = ID3V1;
		g_info.tag.encode = config.mp3encode;
	}
}

static int32_t read_bytes(void *id, void *data, int32_t bcount)
{
	buffered_reader_t *pbreader = (buffered_reader_t *) id;

	return buffered_reader_read(pbreader, data, bcount);
}

static uint32_t get_pos(void *id)
{
	buffered_reader_t *pbreader = (buffered_reader_t *) id;

	return buffered_reader_position(pbreader);
}

static int set_pos_abs(void *id, uint32_t pos)
{
	buffered_reader_t *pbreader = (buffered_reader_t *) id;

	buffered_reader_seek(pbreader, pos);

	return 0;
}

static uint32_t get_length(void *id);

static int set_pos_rel(void *id, int32_t delta, int mode)
{
	uint32_t cur_pos;

	switch (mode) {
		case SEEK_SET:
			set_pos_abs(id, delta);
			break;
		case SEEK_CUR:
			cur_pos = get_pos(id);
			set_pos_abs(id, delta + cur_pos);
			break;
		case SEEK_END:
			cur_pos = get_length(id);
			set_pos_abs(id, delta + cur_pos);
			break;
		default:
			break;
	}

	return 0;
}

/// TODO: temp hack
static int push_back_byte(void *id, int c)
{
	set_pos_rel(id, -1, SEEK_CUR);

	return c;
//    return ungetc (c, id);
}

static uint32_t get_length(void *id)
{
	buffered_reader_t *pbreader = (buffered_reader_t *) id;

	return buffered_reader_length(pbreader);
}

static int can_seek(void *id)
{
	return 1;
}

static int32_t write_bytes(void *id, void *data, int32_t bcount)
{
	return -1;
}

static WavpackStreamReader breader = {
	read_bytes, get_pos, set_pos_abs, set_pos_rel, push_back_byte, get_length,
	can_seek,
	write_bytes
};

static WavpackContext *open_wvfile(const char *spath, int flags,
								   int norm_offset)
{
	if (spath == NULL) {
		return NULL;
	}

	if (flags & OPEN_WVC) {
		char *in2filename = malloc(strlen(spath) + 10);

		strcpy(in2filename, spath);
		strcat(in2filename, "c");

		// Lookup for corrent file
		wvc = buffered_reader_open(in2filename, g_io_buffer_size, 0);
		free(in2filename);
	}

	wv = buffered_reader_open(spath, g_io_buffer_size, 0);

	if (wv == NULL) {
		goto failed;
	}

	char error[80];

	return WavpackOpenFileInputEx(&breader, (void *) wv, (void *) wvc, error,
								  flags, norm_offset);

  failed:
	if (wv != NULL) {
		buffered_reader_close(wv);
		wv = NULL;
	}

	if (wvc != NULL) {
		buffered_reader_close(wvc);
		wvc = NULL;
	}

	return NULL;
}

/**
 * װ��WvPack�����ļ� 
 *
 * @param spath ��·����
 * @param lpath ��·����
 *
 * @return �ɹ�ʱ����0
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

	fd = xrIoOpen(spath, PSP_O_RDONLY, 0777);

	if (fd < 0) {
		__end();
		return -1;
	}

	g_info.filesize = xrIoLseek(fd, 0, PSP_SEEK_END);
	xrIoClose(fd);

	g_decoder =
		open_wvfile(spath, OPEN_WVC | OPEN_TAGS | OPEN_2CH_MAX | OPEN_NORMALIZE,
					23);

	if (g_decoder == NULL) {
		__end();
		return -1;
	}

	g_wv_bits_per_sample = WavpackGetBitsPerSample(g_decoder);

	if (g_wv_bits_per_sample != 16) {
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
			   (int) (g_info.duration / 60), (int) g_info.duration % 60,
			   g_encode_name,
			   1.0 * g_info.filesize / (g_info.samples * g_info.channels *
										(g_wv_bits_per_sample / 8))
		);

	dbg_printf(d, "[%s - %s - %s, wv tag]", g_info.tag.artist, g_info.tag.album,
			   g_info.tag.title);

	xMP3AudioSetChannelCallback(0, wv_audiocallback, NULL);

	return 0;
}

/**
 * ֹͣWvPack�����ļ��Ĳ��ţ�������Դ��
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
 * ֹͣWvPack�����ļ��Ĳ��ţ�������ռ�е��̡߳���Դ��
 *
 * @note �������ڲ����߳��е��ã������ܹ�����ظ����ö�������
 *
 * @return �ɹ�ʱ����0
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

	if (wv != NULL) {
		buffered_reader_close(wv);
		wv = NULL;
	}

	if (wvc != NULL) {
		buffered_reader_close(wvc);
		wvc = NULL;
	}

	free_bitrate(&g_inst_br);
	generic_end();

	return 0;
}

/**
 * PSP׼������ʱWvPack�Ĳ���
 *
 * @return �ɹ�ʱ����0
 */
static int wv_suspend(void)
{
	generic_suspend();
	wv_end();

	return 0;
}

/**
 * PSP׼��������ʱ�ָ���WvPack�Ĳ���
 *
 * @param spath ��ǰ������������8.3·����ʽ
 * @param lpath ��ǰ���������������ļ�����ʽ
 *
 * @return �ɹ�ʱ����0
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
 * �õ�WvPack�����ļ������Ϣ
 *
 * @param pinfo ��Ϣ�ṹ��ָ��
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
			pinfo->psp_freq[0] = 222;
		else
			pinfo->psp_freq[0] = 166;

		pinfo->psp_freq[1] = pinfo->psp_freq[0] / 2;
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
		if (show_encoder_msg && g_status != ST_UNKNOWN) {
			SPRINTF_S(pinfo->encode_msg, "%s %s: %.3f", g_encode_name,
					  _("ѹ����"),
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
 * ����Ƿ�ΪWvPack�ļ���Ŀǰֻ����ļ���׺��
 *
 * @param spath ��ǰ������������8.3·����ʽ
 *
 * @return ��WvPack�ļ�����1�����򷵻�0
 */
static int wv_probe(const char *spath)
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

static int wv_set_opt(const char *unused, const char *values)
{
	int argc, i;
	char **argv;

	g_io_buffer_size = WVPACK_BUFFERED_READER_BUFFER_SIZE;

	dbg_printf(d, "%s: options are %s", __func__, values);

	build_args(values, &argc, &argv);

	for (i = 0; i < argc; ++i) {
		if (!strncasecmp
			(argv[i], "wv_buffer_size", sizeof("wv_buffer_size") - 1)) {
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

	return 0;
}

static struct music_ops wv_ops = {
	.name = "wv",
	.set_opt = wv_set_opt,
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

#endif
