#include <stdlib.h>
#include <assert.h>
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
#include "musicdrv.h"

#define INPUT_BUFFER_SIZE	8*1152
#define OUTPUT_BUFFER_SIZE	4*1152	/* Must be an integer multiple of 4. */
#define MAXVOLUME 0x8000

static int status = ST_UNKNOWN;
static int file_size = 0;
static int file_pos = 0;
static int file_fd = -1;
static int samples_read = 0;
static SceUID decode_thread = -1;
static signed short output_buffer[4][OUTPUT_BUFFER_SIZE / 2];
static uint8_t input_buffer[INPUT_BUFFER_SIZE + MAD_BUFFER_GUARD],
	*output = (uint8_t *) & output_buffer[0][0], *guard = NULL,
	*output_end = (uint8_t *) & output_buffer[0][0] + OUTPUT_BUFFER_SIZE;
static int output_buffer_i = 0;
static struct mad_stream stream;
static struct mad_frame frame;
static struct mad_synth synth;
static mad_timer_t timer;
static int mp3_handle = -1;

static void _end(void);

#if 0
#define AUDIO_DEVICE1      "/dev/sound/dsp"
#define AUDIO_DEVICE2      "/dev/dsp"

static int oss_fd = -1;

static int audio_oss_init(void)
{
	oss_fd = sceIoOpen(AUDIO_DEVICE1, O_WRONLY);
	if (oss_fd == -1)
		oss_fd = sceIoOpen(AUDIO_DEVICE2, O_WRONLY);

	if (oss_fd < 0)
		return oss_fd;

	if (ioctl(oss_fd, SNDCTL_DSP_SYNC, 0) == -1) {
		return -EBUSY;
	}

	int channels = 2;

	if (ioctl(oss_fd, SNDCTL_DSP_CHANNELS, &channels) == -1) {
		return -EINVAL;
	}

	int format = AFMT_S16_LE;

	if (ioctl(oss_fd, SNDCTL_DSP_SETFMT, &format) == -1) {
		return -EINVAL;
	}

	int speed = 44100;

	if (ioctl(oss_fd, SNDCTL_DSP_SPEED, &speed) == -1) {
		return -EINVAL;
	}

	return 0;
}

static int audio_oss_output(unsigned char const *ptr, unsigned int len)
{
	while (len) {
		int wrote;

		wrote = write(oss_fd, ptr, len);
		if (wrote == -1) {
			if (errno == EINTR) {
				continue;
			} else {
				return -1;
			}
		}

		ptr += wrote;
		len -= wrote;
	}

	return 0;
}
#endif

static signed short MadFixedToSshort(mad_fixed_t Fixed)
{
	if (Fixed >= MAD_F_ONE)
		return (SHRT_MAX);
	if (Fixed <= -MAD_F_ONE)
		return (-SHRT_MAX);
	return ((signed short) (Fixed >> (MAD_F_FRACBITS - 15)));
}

static int madmp3_decoder_thread(SceSize args, void *argp)
{
	int ret, i;

	while (status != ST_UNKNOWN && status != ST_STOPPED) {
		if (status == ST_PLAYING) {
//          printf("%s: playing music. [%d frame decoded]\n", __func__, samples_read);
			if (stream.buffer == NULL || stream.error == MAD_ERROR_BUFLEN) {
				size_t read_size, remaining = 0, bufsize;
				uint8_t *read_start;

				if (stream.next_frame != NULL) {
					remaining = stream.bufend - stream.next_frame;
					memmove(input_buffer, stream.next_frame, remaining);
					read_start = input_buffer + remaining;
					read_size = INPUT_BUFFER_SIZE - remaining;
				} else {
					read_size = INPUT_BUFFER_SIZE;
					read_start = input_buffer;
					remaining = 0;
				}
				bufsize = sceIoRead(file_fd, read_start, read_size);
				if (bufsize == 0) {
					_end();
					continue;
				}
				if (bufsize < read_size) {
					guard = read_start + read_size;
					memset(guard, 0, MAD_BUFFER_GUARD);
					read_size += MAD_BUFFER_GUARD;
				}
				mad_stream_buffer(&stream, input_buffer, read_size + remaining);
				stream.error = 0;
			}

			ret = mad_frame_decode(&frame, &stream);
			samples_read++;
			if (ret == -1) {
				if (MAD_RECOVERABLE(stream.error)
					|| stream.error == MAD_ERROR_BUFLEN)
					continue;
				else {
					_end();
					continue;
				}
			}
			mad_timer_add(&timer, frame.header.duration);
			mad_synth_frame(&synth, &frame);
			for (i = 0; i < synth.pcm.length; i++) {
				signed short sample;

				/* Left channel */
				sample = MadFixedToSshort(synth.pcm.samples[0][i]);
				*(output++) = sample & 0xff;
				*(output++) = (sample >> 8);
				sample = MadFixedToSshort(synth.pcm.samples[1][i]);
				*(output++) = sample & 0xff;
				*(output++) = (sample >> 8);

				if (output == output_end) {
					sceAudioOutputPannedBlocking
						(mp3_handle, MAXVOLUME, MAXVOLUME, (char *)
						 output_buffer[output_buffer_i]
						);

					output_buffer_i = (output_buffer_i + 1) & 0x03;
					output = (uint8_t *) & output_buffer[output_buffer_i][0];
					output_end = output + OUTPUT_BUFFER_SIZE;
				}
			}
		} else {
			sceKernelDelayThread(1000000);
		}
	}

	return 0;
}

static int madmp3_load(const char *spath, const char *lpath)
{
//  printf("%s: loading %s\n", __func__, spath);
	status = ST_UNKNOWN;

	file_fd = sceIoOpen(spath, PSP_O_RDONLY, 777);

	if (file_fd < 0) {
		return file_fd;
	}
	// TODO: read tag

	file_size = sceIoLseek32(file_fd, 0, PSP_SEEK_END);
	if (file_size < 0)
		return file_size;
	file_pos = sceIoLseek32(file_fd, 0, PSP_SEEK_SET);
	if (file_pos < 0)
		return file_pos;

	mad_stream_init(&stream);
	mad_frame_init(&frame);
	mad_synth_init(&synth);
	mad_timer_reset(&timer);
	memset(&output_buffer[0][0], 0, 4 * OUTPUT_BUFFER_SIZE);
	output = (uint8_t *) & output_buffer[0][0];
	output_end = output + OUTPUT_BUFFER_SIZE * 2;
	output_buffer_i = 0;
	samples_read = 0;

	mp3_handle =
		sceAudioChReserve(PSP_AUDIO_NEXT_CHANNEL, OUTPUT_BUFFER_SIZE / 4, 0);

	if (mp3_handle < 0) {
		sceIoClose(file_fd);
		file_fd = -1;
		return mp3_handle;
	}

	status = ST_LOADED;

	decode_thread =
		sceKernelCreateThread("madmp3 decoder", madmp3_decoder_thread, 0x08,
							  0x100000, PSP_THREAD_ATTR_USER, NULL);

	if (decode_thread < 0) {
		sceIoClose(file_fd);
		status = ST_UNKNOWN;
		return -1;
	}

	sceKernelStartThread(decode_thread, 0, NULL);

	// if load fail return -1

	return 0;
}

static int madmp3_set_opt(const char *key, const char *value)
{
//  printf("%s: setting %s to %s\n", __func__, key, value);

	return 0;
}

static int madmp3_get_info(struct music_info *info)
{
//  printf("%s: type: %d\n", __func__, info->type);

	if (status == ST_UNKNOWN) {
		return -1;
	}

	if (info->type & MD_GET_CURTIME) {
		info->cur_time = timer.seconds;
	}

	if (info->type & MD_GET_CPUFREQ) {
		info->psp_freq[0] = 33;
		info->psp_freq[1] = 111;
	}
/*
	if (info->type & MD_GET_TITLE) {
		strcpy(info->title, "default title");
	}
	if (info->type & MD_GET_ARTIST) {
		strcpy(info->artist, "default artist");
	}
	if (info->type & MD_GET_COMMENT) {
		strcpy(info->comment, "default comment");
	}
	if (info->type & MD_GET_CURTIME) {
		info->cur_time = 0.;
	}
	if (info->type & MD_GET_DURATION) {
		info->duration = 60.;
	}
	if (info->type & MD_GET_CPUFREQ) {
		info->psp_freq[0] = 33;
		info->psp_freq[1] = 111;
	}
	if (info->type & MD_GET_FREQ) {
		info->freq = 44100;
	}
	if (info->type & MD_GET_CHANNELS) {
		info->channels = 2;
	}
	if (info->type & MD_GET_DECODERNAME) {
		strcpy(info->decoder_name, "default mp3 decoder");
	}
	if (info->type & MD_GET_ENCODEMSG) {
		strcpy(info->encode_msg, "320CBR lame 3.90 etc");
	}
	if (info->type & MD_GET_AVGKBPS) {
		info->avg_kbps = 320;
	}
	if (info->type & MD_GET_INSKBPS) {
		info->ins_kbps = 320;
	}
	if (info->type & MD_GET_FILEFD) {
		info->file_handle = -1;
	}
	if (info->type & MD_GET_SNDCHL) {
		info->channel_handle = -1;
	}
	*/

	// if fail return -1

	return 0;
}

static int madmp3_play(void)
{
//  printf("%s\n", __func__);

	status = ST_PLAYING;

	/* if return error value won't play */

	return 0;
}

static int madmp3_pause(void)
{
//  printf("%s\n", __func__);

	status = ST_PAUSED;

	return 0;
}

static void _end(void)
{
	status = ST_STOPPED;
	if (file_fd >= 0)
		close(file_fd);
	if (mp3_handle >= 0) {
		sceAudioChRelease(mp3_handle);
		mp3_handle = -1;
	}
}

static int madmp3_end(void)
{
//  printf("%s\n", __func__);

	status = ST_STOPPED;

//  pthread_join(decode_thread, NULL);

	_end();

	return 0;
}

/**
 * Returning <0 value will result in freeze
 * Return ST_UNKNOWN or ST_STOPPED when load failed
 * Return ST_STOPPED when decode error, or freeze
 */
static int madmp3_get_status(void)
{
	return status;
}

static int madmp3_fforward(int i)
{
//  printf("%s: sec: %d\n", __func__, i);

	return 0;
}

static int madmp3_fbackward(int i)
{
//  printf("%s: sec: %d\n", __func__, i);

	return 0;
}

static int madmp3_suspend(void)
{
//  printf("%s\n", __func__);

	/* return 0 when successed suspend */
	return 0;
}

static int madmp3_resume(const char *filename)
{
//  printf("%s\n", __func__);

	/* return 0 when successed resume */
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
