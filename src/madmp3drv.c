#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <mad.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <pspkernel.h>
#include <pspaudio.h>
#include <pspaudio.h>
#include <limits.h>
#include "strsafe.h"
#include "musicdrv.h"
#include "xmp3audiolib.h"
#include "dbg.h"
#include "power.h"

#define LB_CONV(x)	\
    (((x) & 0xff)<<24) |  \
    (((x>>8) & 0xff)<<16) |  \
    (((x>>16) & 0xff)<< 8) |  \
    (((x>>24) & 0xff)    )

#define UNUSED(x) ((void)(x))
#define BUFF_SIZE	8*1152
#define MODE_EXT_I_STEREO   1
#define MODE_EXT_MS_STEREO   2
#define MPA_MONO   3

#define open sceIoOpen
#define read sceIoRead
#define close sceIoClose
#define lseek sceIoLseek

#define SEEK_CUR PSP_SEEK_CUR
#define SEEK_SET PSP_SEEK_SET
#define SEEK_END PSP_SEEK_END

#define O_RDONLY PSP_O_RDONLY

typedef struct reader_data_t
{
	SceUID fd;
	long size;
} reader_data;

static reader_data data;

static int __end(void);

static struct mad_stream stream;
static struct mad_frame frame;
static struct mad_synth synth;

/**
 * 当前驱动播放状态
 */
static int g_status;

/**
 * MP3音乐播放缓冲
 */
static uint16_t g_buff[BUFF_SIZE / 2];

/**
 * MP3音乐解码缓冲
 */
static uint8_t g_input_buff[BUFF_SIZE + MAD_BUFFER_GUARD];

/**
 * MP3音乐播放缓冲大小，以帧数计
 */
static unsigned g_buff_frame_size;

/**
 * MP3音乐播放缓冲当前位置，以帧数计
 */
static int g_buff_frame_start;

/**
 * 当前驱动播放状态写锁
 */
static SceUID g_status_sema;;

/**
 * MP3音乐文件长度，以秒数
 */
static double g_duration;

/**
 * 当前播放时间，以秒数计
 */
static double g_play_time;

/**
 * MP3音乐快进、退秒数
 */
static int g_seek_seconds;

/**
 * MP3音乐声道数
 */
static int g_madmp3_channels;

/**
 * MP3音乐声道数
 */
static int g_madmp3_sample_freq;

/**
 * MP3音乐比特率
 */
static int g_madmp3_bitrate;

/**
 * MP3音乐休眠时播放时间
 */
static double g_suspend_playing_time;

/**
 * MP3平均比特率
 */
static double g_average_bitrate;

/**
 * 休眠前播放状态
 */
static int g_suspend_status;

/**
 * 是MPEG1or2层?
 */
static bool g_is_mpeg1or2;

/**
 * 加锁
 */
static inline int madmp3_lock(void)
{
	return sceKernelWaitSemaCB(g_status_sema, 1, NULL);
}

/**
 * 解锁
 */
static inline int madmp3_unlock(void)
{
	return sceKernelSignalSema(g_status_sema, 1);
}

static signed short MadFixedToSshort(mad_fixed_t Fixed)
{
	if (Fixed >= MAD_F_ONE)
		return (SHRT_MAX);
	if (Fixed <= -MAD_F_ONE)
		return (-SHRT_MAX);
	return ((signed short) (Fixed >> (MAD_F_FRACBITS - 15)));
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
	if (frames <= 0)
		return;

	memcpy(buf, srcbuf, frames * channels * sizeof(*srcbuf));
}

static int madmp3_seek_seconds_offset(double offset)
{
	uint32_t target_frame = abs(offset) * g_average_bitrate / 8;
	int is_forward = offset > 0 ? 1 : -1;
	int orig_pos = lseek(data.fd, 0, SEEK_CUR);
	int new_pos = orig_pos + is_forward * (int) target_frame;

	if (new_pos < 0) {
		new_pos = 0;
	}
	int ret = lseek(data.fd, new_pos, SEEK_SET);
	int bufsize;

	if (ret >= 0) {
		bufsize = read(data.fd, g_input_buff, BUFF_SIZE);

		if (bufsize <= 0) {
			__end();
			return -1;
		}

		g_play_time += offset;
		if (g_play_time < 0) {
			g_play_time = 0;
		}
		mad_stream_buffer(&stream, g_input_buff, BUFF_SIZE);
		mad_stream_sync(&stream);
		ret = mad_frame_decode(&frame, &stream);
		if (ret == MAD_ERROR_LOSTSYNC) {
			if (stream.next_frame != NULL
				&& stream.next_frame != stream.this_frame) {
				mad_stream_skip(&stream, stream.next_frame - stream.this_frame);
			} else {
				mad_stream_skip(&stream, stream.bufend - stream.this_frame);
			}
		}
		return 0;
	}

	__end();
	return -1;
}

static int madmp3_seek_seconds(double npt)
{
	return madmp3_seek_seconds_offset(npt - g_play_time);
}

/**
 * MP3音乐播放回调函数，
 * 负责将解码数据填充声音缓存区
 *
 * @note 声音缓存区的格式为双声道，16位低字序
 *
 * @param buf 声音缓冲区指针
 * @param reqn 缓冲区帧大小
 * @param pdata 用户数据，无用
 */
static void madmp3_audiocallback(void *buf, unsigned int reqn, void *pdata)
{
	int avail_frame;
	int snd_buf_frame_size = (int) reqn;
	int ret;
	double incr;
	signed short *audio_buf = buf;

	UNUSED(pdata);

	if (g_status != ST_PLAYING) {
		if (g_status == ST_FFOWARD) {
			madmp3_lock();
			g_status = ST_PLAYING;
			madmp3_unlock();
			scene_power_save(true);
			madmp3_seek_seconds(g_play_time + g_seek_seconds);
		} else if (g_status == ST_FBACKWARD) {
			madmp3_lock();
			g_status = ST_PLAYING;
			madmp3_unlock();
			scene_power_save(true);
			madmp3_seek_seconds(g_play_time - g_seek_seconds);
		}
		clear_snd_buf(buf, snd_buf_frame_size);
		return;
	}

	while (snd_buf_frame_size > 0) {
		avail_frame = g_buff_frame_size - g_buff_frame_start;

		if (avail_frame >= snd_buf_frame_size) {
			send_to_sndbuf(audio_buf,
						   &g_buff[g_buff_frame_start * 2],
						   snd_buf_frame_size, 2);
			g_buff_frame_start += snd_buf_frame_size;
			audio_buf += snd_buf_frame_size * 2;
			snd_buf_frame_size = 0;
		} else {
			send_to_sndbuf(audio_buf,
						   &g_buff[g_buff_frame_start * 2], avail_frame, 2);
			snd_buf_frame_size -= avail_frame;
			audio_buf += avail_frame * 2;

			if (stream.buffer == NULL || stream.error == MAD_ERROR_BUFLEN) {
				size_t read_size, remaining = 0;
				uint8_t *read_start;
				int bufsize;

				if (stream.next_frame != NULL) {
					remaining = stream.bufend - stream.next_frame;
					memmove(g_input_buff, stream.next_frame, remaining);
					read_start = g_input_buff + remaining;
					read_size = BUFF_SIZE - remaining;
				} else {
					read_size = BUFF_SIZE;
					read_start = g_input_buff;
					remaining = 0;
				}
				bufsize = read(data.fd, read_start, read_size);
				if (bufsize <= 0) {
					__end();
					return;
				}
				if (bufsize < read_size) {
					uint8_t *guard = read_start + read_size;

					memset(guard, 0, MAD_BUFFER_GUARD);
					read_size += MAD_BUFFER_GUARD;
				}
				mad_stream_buffer(&stream, g_input_buff, read_size + remaining);
				stream.error = 0;
			}

			ret = mad_frame_decode(&frame, &stream);

			if (ret == -1) {
				if (MAD_RECOVERABLE(stream.error)
					|| stream.error == MAD_ERROR_BUFLEN) {
					if (stream.error == MAD_ERROR_LOSTSYNC) {
						// likely to be ID3 Tags, skip it
						mad_stream_skip(&stream,
										stream.bufend - stream.this_frame);
					}
					g_buff_frame_size = 0;
					g_buff_frame_start = 0;
					continue;
				} else {
					__end();
					return;
				}
			}

			unsigned i;
			uint16_t *output = &g_buff[0];

			mad_synth_frame(&synth, &frame);
			for (i = 0; i < synth.pcm.length; i++) {
				signed short sample;

				if (MAD_NCHANNELS(&frame.header) == 2) {
					/* Left channel */
					sample = MadFixedToSshort(synth.pcm.samples[0][i]);
					*(output++) = sample;
					sample = MadFixedToSshort(synth.pcm.samples[1][i]);
					*(output++) = sample;
				} else {
					sample = MadFixedToSshort(synth.pcm.samples[0][i]);
					*(output++) = sample;
					*(output++) = sample;
				}
			}

			g_buff_frame_size = synth.pcm.length;
			g_buff_frame_start = 0;
			incr = frame.header.duration.seconds;
			incr +=
				mad_timer_fraction(frame.header.duration,
								   MAD_UNITS_MILLISECONDS) / 1000.0;
			g_play_time += incr;
		}
	}
}

/**
 * 初始化驱动变量资源等
 *
 * @return 成功时返回0
 */
static int __init(void)
{
	g_status_sema = sceKernelCreateSema("wave Sema", 0, 1, 1, NULL);

	madmp3_lock();
	g_status = ST_UNKNOWN;
	madmp3_unlock();

	memset(g_buff, 0, sizeof(g_buff));
	memset(g_input_buff, 0, sizeof(g_input_buff));
	g_buff_frame_size = g_buff_frame_start = 0;
	g_seek_seconds = 0;

	g_duration = g_play_time = 0.;

	g_madmp3_bitrate = g_madmp3_sample_freq = g_madmp3_channels = 0;
	g_average_bitrate = 0;
	g_is_mpeg1or2 = false;

//  memset(&g_taginfo, 0, sizeof(g_taginfo));

	return 0;
}

/* fast header check for resync */
static inline int ff_mpa_check_header(uint32_t header)
{
	/* header */
	if ((header & 0xffe00000) != 0xffe00000)
		return -1;
	/* layer check */
	if ((header & (3 << 17)) == 0)
		return -1;
	/* bit rate */
	if ((header & (0xf << 12)) == 0xf << 12)
		return -1;
	/* frequency */
	if ((header & (3 << 10)) == 3 << 10)
		return -1;
	return 0;
}

const uint16_t ff_mpa_freq_tab[3] = { 44100, 48000, 32000 };

const uint16_t ff_mpa_bitrate_tab[2][3][15] = {
	{{0, 32, 64, 96, 128, 160, 192, 224, 256, 288, 320, 352, 384, 416, 448},
	 {0, 32, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 384},
	 {0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320}},
	{{0, 32, 48, 56, 64, 80, 96, 112, 128, 144, 160, 176, 192, 224, 256},
	 {0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160},
	 {0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160}
	 }
};

typedef struct MPADecodeContext
{
//    DECLARE_ALIGNED_8(uint8_t, last_buf[2*BACKSTEP_SIZE + EXTRABYTES]);
	int last_buf_size;
	int frame_size;
	/* next header (used in free format parsing) */
	uint32_t free_format_next_header;
	int error_protection;
	int layer;
	int sample_rate;
	int sample_rate_index;		/* between 0 and 8 */
	int bit_rate;
//    GetBitContext gb;
//    GetBitContext in_gb;
	int nb_channels;
	int mode;
	int mode_ext;
	int lsf;
//    DECLARE_ALIGNED_16(MPA_INT, synth_buf[MPA_MAX_CHANNELS][512 * 2]);
//    int synth_buf_offset[MPA_MAX_CHANNELS];
//    DECLARE_ALIGNED_16(int32_t, sb_samples[MPA_MAX_CHANNELS][36][SBLIMIT]);
//    int32_t mdct_buf[MPA_MAX_CHANNELS][SBLIMIT * 18]; /* previous samples, for layer 3 MDCT */
#ifdef DEBUG
	int frame_count;
#endif
//    void (*compute_antialias)(struct MPADecodeContext *s, struct GranuleDef *g);
	int adu_mode;
	int dither_state;
	int error_recognition;
//    AVCodecContext* avctx;
} MPADecodeContext;

int ff_mpegaudio_decode_header(MPADecodeContext * s, uint32_t header)
{
	int sample_rate, frame_size, mpeg25, padding;
	int sample_rate_index, bitrate_index;

	if (header & (1 << 20)) {
		s->lsf = (header & (1 << 19)) ? 0 : 1;
		mpeg25 = 0;
	} else {
		s->lsf = 1;
		mpeg25 = 1;
	}

	s->layer = 4 - ((header >> 17) & 3);
	/* extract frequency */
	sample_rate_index = (header >> 10) & 3;
	sample_rate = ff_mpa_freq_tab[sample_rate_index] >> (s->lsf + mpeg25);
	sample_rate_index += 3 * (s->lsf + mpeg25);
	s->sample_rate_index = sample_rate_index;
	s->error_protection = ((header >> 16) & 1) ^ 1;
	s->sample_rate = sample_rate;

	bitrate_index = (header >> 12) & 0xf;
	padding = (header >> 9) & 1;
	//extension = (header >> 8) & 1;
	s->mode = (header >> 6) & 3;
	s->mode_ext = (header >> 4) & 3;
	//copyright = (header >> 3) & 1;
	//original = (header >> 2) & 1;
	//emphasis = header & 3;

	if (s->mode == MPA_MONO)
		s->nb_channels = 1;
	else
		s->nb_channels = 2;

	if (bitrate_index != 0) {
		frame_size = ff_mpa_bitrate_tab[s->lsf][s->layer - 1][bitrate_index];
		s->bit_rate = frame_size * 1000;
		switch (s->layer) {
			case 1:
				frame_size = (frame_size * 12000) / sample_rate;
				frame_size = (frame_size + padding) * 4;
				break;
			case 2:
				frame_size = (frame_size * 144000) / sample_rate;
				frame_size += padding;
				break;
			default:
			case 3:
				frame_size = (frame_size * 144000) / (sample_rate << s->lsf);
				frame_size += padding;
				break;
		}
		s->frame_size = frame_size;
	} else {
		/* if no frame size computed, signal it */
		return 1;
	}

#if 0
	printf("layer%d, %d Hz, %d kbits/s, ",
		   s->layer, s->sample_rate, s->bit_rate);
	if (s->nb_channels == 2) {
		if (s->layer == 3) {
			if (s->mode_ext & MODE_EXT_MS_STEREO)
				printf("ms-");
			if (s->mode_ext & MODE_EXT_I_STEREO)
				printf("i-");
		}
		printf("stereo");
	} else {
		printf("mono");
	}
	printf("\n");
#endif
	return 0;
}

#define MKBETAG(a,b,c,d) (d | (c << 8) | (b << 16) | (a << 24))

static int mp3_parse_vbr_tags(uint32_t off)
{
	const uint32_t xing_offtbl[2][2] = { {32, 17}, {17, 9} };
	uint32_t b, frames;
	MPADecodeContext ctx;

	if (read(data.fd, &b, sizeof(b)) != sizeof(b)) {
		return -1;
	}

	b = LB_CONV(b);

	if (ff_mpa_check_header(b) < 0)
		return -1;

	ff_mpegaudio_decode_header(&ctx, b);
	if (ctx.layer != 3)
		return -1;

	lseek(data.fd, xing_offtbl[ctx.lsf == 1][ctx.nb_channels == 1], SEEK_CUR);

	if (read(data.fd, &b, sizeof(b)) != sizeof(b)) {
		return -1;
	}
	b = LB_CONV(b);

	if (b == MKBETAG('X', 'i', 'n', 'g') || b == MKBETAG('I', 'n', 'f', 'o')) {
		if (read(data.fd, &b, sizeof(b)) != sizeof(b)) {
			return -1;
		}
		b = LB_CONV(b);
		if (b & 0x1) {
			if (read(data.fd, &frames, sizeof(frames)) != sizeof(frames)) {
				return -1;
			}
			frames = LB_CONV(frames);
		}
	}

	/* Check for VBRI tag (always 32 bytes after end of mpegaudio header) */
	lseek(data.fd, off + 4 + 32, SEEK_SET);
	if (read(data.fd, &b, sizeof(b)) != sizeof(b)) {
		return -1;
	}
	b = LB_CONV(b);
	if (b == MKBETAG('V', 'B', 'R', 'I')) {
		uint16_t t;

		/* Check tag version */
		if (read(data.fd, &t, sizeof(t)) != sizeof(t)) {
			return -1;
		}
		t = ((t & 0xff) << 8) | (t >> 8);

		if (t == 1) {
			/* skip delay, quality and total bytes */
			lseek(data.fd, 8, SEEK_CUR);
			if (read(data.fd, &frames, sizeof(frames)) != sizeof(frames)) {
				return -1;
			}
			frames = LB_CONV(frames);
		}
	}

	if ((int) frames < 0)
		return -1;

	return frames;
}

static int id3v2_match(const uint8_t * buf)
{
	return buf[0] == 'I' &&
		buf[1] == 'D' &&
		buf[2] == '3' &&
		buf[3] != 0xff &&
		buf[4] != 0xff &&
		(buf[6] & 0x80) == 0 &&
		(buf[7] & 0x80) == 0 && (buf[8] & 0x80) == 0 && (buf[9] & 0x80) == 0;
}

static int read_mp3_info(void)
{
	uint32_t b;

	lseek(data.fd, 0, SEEK_SET);

	if (read(data.fd, &b, sizeof(b)) < 4) {
		return -1;
	}

	b = LB_CONV(b);

	if (ff_mpa_check_header(b) < 0)
		return -1;

	MPADecodeContext ctx;

	ff_mpegaudio_decode_header(&ctx, b);

	g_madmp3_channels = ctx.nb_channels;
	g_madmp3_sample_freq = ctx.sample_rate;

	lseek(data.fd, 0, SEEK_SET);

	if (ctx.layer != 3) {
		g_is_mpeg1or2 = true;
		return 0;
	}

	/* skip ID3v2 header if exists */
#define ID3v2_HEADER_SIZE 10
	uint8_t buf[ID3v2_HEADER_SIZE];

	if (read(data.fd, buf, sizeof(buf)) != sizeof(buf)) {
		return -1;
	}

	int len;
	uint32_t off;

	if (id3v2_match(buf)) {
		/* parse ID3v2 header */
		len = ((buf[6] & 0x7f) << 21) |
			((buf[7] & 0x7f) << 14) | ((buf[8] & 0x7f) << 7) | (buf[9] & 0x7f);

		/* TODO: read ID3v2 tag */

		lseek(data.fd, len, SEEK_CUR);
	} else {
		lseek(data.fd, 0, SEEK_SET);
	}

	off = lseek(data.fd, 0, SEEK_CUR);
	int frames = mp3_parse_vbr_tags(off);
	int spf;

	spf = ctx.lsf ? 576 : 1152;	/* Samples per frame, layer 3 */

	g_duration = (double) frames *spf / g_madmp3_sample_freq;

	g_average_bitrate = (double) data.size * 8 / g_duration;

	lseek(data.fd, 0, SEEK_SET);

	return 0;
}

static inline void broadcast_output()
{
}

static int madmp3_load(const char *spath, const char *lpath)
{
	int ret;

	__init();

	broadcast_output("%s: loading %s\n", __func__, spath);
	g_status = ST_UNKNOWN;

	data.fd = open(lpath, O_RDONLY, 777);

	if (data.fd < 0) {
		return data.fd;
	}
	// TODO: read tag

	data.size = lseek(data.fd, 0, SEEK_END);
	if (data.size < 0)
		return data.size;
	lseek(data.fd, 0, SEEK_SET);

	mad_stream_init(&stream);
	mad_frame_init(&frame);
	mad_synth_init(&synth);

	if (read_mp3_info() < 0) {
		__end();
		return -1;
	}

	broadcast_output("[%d channel(s), %d Hz, %.2f kbps, %02d:%02d]\n",
					 g_madmp3_channels, g_madmp3_sample_freq,
					 g_average_bitrate / 1000, (int) (g_duration / 60),
					 (int) g_duration % 60);
	ret = xMP3AudioInit();

	if (ret < 0) {
		__end();
		return -1;
	}

	ret = xMP3AudioSetFrequency(g_madmp3_sample_freq);
	if (ret < 0) {
		__end();
		return -1;
	}

	xMP3AudioSetChannelCallback(0, madmp3_audiocallback, NULL);

	if (ret < 0) {
		close(data.fd);
		g_status = ST_UNKNOWN;
		return -1;
	}

	madmp3_lock();
	g_status = ST_LOADED;
	madmp3_unlock();

	return 0;
}

static int madmp3_set_opt(const char *key, const char *value)
{
	broadcast_output("%s: setting %s to %s\n", __func__, key, value);

	return 0;
}

static int madmp3_get_info(struct music_info *info)
{
	broadcast_output("%s: type: %d\n", __func__, info->type);

	if (g_status == ST_UNKNOWN) {
		return -1;
	}

/*
	if (info->type & MD_GET_TITLE) {
		STRCPY_S(info->title, "default title");
	}
	if (info->type & MD_GET_ARTIST) {
		STRCPY_S(info->artist, "default artist");
	}
	if (info->type & MD_GET_COMMENT) {
		STRCPY_S(info->comment, "default comment");
	}
	*/
	if (info->type & MD_GET_CURTIME) {
		info->cur_time = g_play_time;
	}
	if (info->type & MD_GET_DURATION) {
		info->duration = g_duration;
	}
	if (info->type & MD_GET_CPUFREQ) {
		info->psp_freq[0] = 66 + (133 - 66) * g_average_bitrate / 1000 / 320;
		info->psp_freq[1] = 111;
	}
	if (info->type & MD_GET_FREQ) {
		info->freq = g_madmp3_sample_freq;
	}
	if (info->type & MD_GET_CHANNELS) {
		info->channels = g_madmp3_channels;
	}
	if (info->type & MD_GET_DECODERNAME) {
		STRCPY_S(info->decoder_name, "madmp3");
	}
	/*
	   if (info->type & MD_GET_ENCODEMSG) {
	   STRCPY_S(info->encode_msg, "320CBR lame 3.90 etc");
	   }
	 */
	if (info->type & MD_GET_AVGKBPS) {
		info->avg_kbps = g_average_bitrate / 1000;
	}
	if (info->type & MD_GET_INSKBPS) {
		info->ins_kbps = frame.header.bitrate / 1000;
	}
	/*
	   if (info->type & MD_GET_FILEFD) {
	   info->file_handle = -1;
	   }
	   if (info->type & MD_GET_SNDCHL) {
	   info->channel_handle = -1;
	   }
	 */

	return 0;
}

static int madmp3_play(void)
{
	broadcast_output("%s\n", __func__);

	g_status = ST_PLAYING;

	/* if return error value won't play */

	return 0;
}

static int madmp3_pause(void)
{
	broadcast_output("%s\n", __func__);

	g_status = ST_PAUSED;

	return 0;
}

/**
 * 停止MP3音乐文件的播放，销毁所占有的线程、资源等
 *
 * @note 不可以在播放线程中调用，必须能够多次重复调用而不死机
 *
 * @return 成功时返回0
 */
static int madmp3_end(void)
{
	broadcast_output("%s\n", __func__);

	__end();

	xMP3AudioEnd();

	g_status = ST_STOPPED;

	return 0;
}

/**
 * Returning <0 value will result in freeze
 * Return ST_UNKNOWN or ST_STOPPED when load failed
 * Return ST_STOPPED when decode error, or freeze
 */
static int madmp3_get_status(void)
{
	return g_status;
}

static int madmp3_fforward(int sec)
{
	if (g_is_mpeg1or2)
		return -1;

	broadcast_output("%s: sec: %d\n", __func__, sec);

	madmp3_lock();
	if (g_status == ST_PLAYING || g_status == ST_PAUSED)
		g_status = ST_FFOWARD;
	madmp3_unlock();

	g_seek_seconds = sec;

	return 0;
}

static int madmp3_fbackward(int sec)
{
	if (g_is_mpeg1or2)
		return -1;

	broadcast_output("%s: sec: %d\n", __func__, sec);

	madmp3_lock();
	if (g_status == ST_PLAYING || g_status == ST_PAUSED)
		g_status = ST_FBACKWARD;
	madmp3_unlock();

	g_seek_seconds = sec;

	return 0;
}

/**
 * PSP准备休眠时MP3的操作
 *
 * @return 成功时返回0
 */
static int madmp3_suspend(void)
{
	broadcast_output("%s\n", __func__);

	g_suspend_status = g_status;
	g_suspend_playing_time = g_play_time;
	madmp3_end();

	return 0;
}

/**
 * PSP准备从休眠时恢复的MP3的操作
 *
 * @param spath 当前播放音乐名，8.3路径形式
 * @param lpath 当前播放音乐名，长文件名形式
 *
 * @return 成功时返回0
 */
static int madmp3_resume(const char *spath, const char *lpath)
{
	int ret;

	broadcast_output("%s\n", __func__);
	ret = madmp3_load(spath, lpath);

	if (ret != 0) {
		dbg_printf(d, "%s: madmp3_load failed %d", __func__, ret);
		return -1;
	}

	madmp3_seek_seconds(g_suspend_playing_time);
	g_suspend_playing_time = 0;
	madmp3_lock();
	g_status = g_suspend_status;
	madmp3_unlock();
	g_suspend_status = ST_LOADED;

	return 0;
}

static struct music_ops madmp3_ops = {
	.name = "madmp3",
	.set_opt = madmp3_set_opt,
	.load = madmp3_load,
	.play = madmp3_play,
	.pause = madmp3_pause,
	.end = madmp3_end,
	.get_status = madmp3_get_status,
	.fforward = madmp3_fforward,
	.fbackward = madmp3_fbackward,
	.suspend = madmp3_suspend,
	.resume = madmp3_resume,
	.get_info = madmp3_get_info,
	.next = NULL,
};

int madmp3_init()
{
	return register_musicdrv(&madmp3_ops);
}

/**
 * 停止MP3音乐文件的播放，销毁资源等
 *
 * @note 可以在播放线程中调用
 *
 * @return 成功时返回0
 */
static int __end(void)
{
	xMP3AudioEndPre();

	if (data.fd >= 0) {
		close(data.fd);
		data.fd = -1;
	}

	mad_stream_finish(&stream);
	mad_synth_finish(&synth);
	mad_frame_finish(&frame);

	g_play_time = 0.;
	madmp3_lock();
	g_status = ST_STOPPED;
	madmp3_unlock();

	return 0;
}
