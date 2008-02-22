/* vim:set ts=4 sw=4 cindent ignorecase enc=gbk: */

#include "config.h"

#ifdef ENABLE_MUSIC

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <limits.h>
#include <fcntl.h>
#include <unistd.h>
#include <pspkernel.h>
#include <pspaudiolib.h>
#include <pspaudio.h>
#include <mad.h>
#ifdef ENABLE_WMA
#include <wmadec.h>
#endif
#include "conf.h"
#include "ctrl.h"
#include "charsets.h"
#include "fat.h"
#include "scene.h"
#ifdef ENABLE_LYRIC
#include "lyric.h"
#endif
#include "mp3info.h"
#include "common/utils.h"
#include "mp3.h"
#include "win.h"
#include "dbg.h"

#define MAXVOLUME 0x8000

// resample from madplay, thanks to nj-zero
#define MAX_RESAMPLEFACTOR 6
#define MAX_NSAMPLES (1152 * MAX_RESAMPLEFACTOR)

extern t_conf config;
extern int freq_list[][2];

struct resample_state
{
	mad_fixed_t ratio;
	mad_fixed_t step;
	mad_fixed_t last;
};

static mad_fixed_t(*Resampled)[2][MAX_NSAMPLES];
static struct resample_state Resample;

static struct OutputPCM
{
	unsigned int nsamples;
	mad_fixed_t const *samples[2];
} OutputPCM;

__inline int resample_init(struct resample_state *state, unsigned int oldrate,
						   unsigned int newrate)
{
	mad_fixed_t ratio;

	if (newrate == 0)
		return -1;

	ratio = mad_f_div(oldrate, newrate);
	if (ratio <= 0 || ratio > MAX_RESAMPLEFACTOR * MAD_F_ONE)
		return -1;

	state->ratio = ratio;

	state->step = 0;
	state->last = 0;

	return 0;
}

__inline unsigned int resample_block(struct resample_state *state,
									 unsigned int nsamples,
									 mad_fixed_t const *old0,
									 mad_fixed_t const *old1,
									 mad_fixed_t * new0, mad_fixed_t * new1)
{
	mad_fixed_t const *end, *begin;

	if (state->ratio == MAD_F_ONE) {
		memcpy(new0, old0, nsamples * sizeof(mad_fixed_t));
		memcpy(new1, old1, nsamples * sizeof(mad_fixed_t));
		return nsamples;
	}

	end = old0 + nsamples;
	begin = new0;

	if (state->step < 0) {
		state->step = mad_f_fracpart(-state->step);

		while (state->step < MAD_F_ONE) {
			if (state->step) {
				*new0++ =
					state->last + mad_f_mul(*old0 - state->last, state->step);
				*new1++ =
					state->last + mad_f_mul(*old1 - state->last, state->step);
			} else {
				*new0++ = state->last;
				*new1++ = state->last;
			}

			state->step += state->ratio;
			if (((state->step + 0x00000080L) & 0x0fffff00L) == 0)
				state->step = (state->step + 0x00000080L) & ~0x0fffffffL;
		}

		state->step -= MAD_F_ONE;
	}

	while (end - old0 > 1 + mad_f_intpart(state->step)) {
		old0 += mad_f_intpart(state->step);
		old1 += mad_f_intpart(state->step);
		state->step = mad_f_fracpart(state->step);

		if (state->step) {
			*new0++ = *old0 + mad_f_mul(old0[1] - old0[0], state->step);
			*new1++ = *old1 + mad_f_mul(old1[1] - old1[0], state->step);
		} else {
			*new0++ = *old0;
			*new1++ = *old1;
		}

		state->step += state->ratio;
		if (((state->step + 0x00000080L) & 0x0fffff00L) == 0)
			state->step = (state->step + 0x00000080L) & ~0x0fffffffL;
	}

	if (end - old0 == 1 + mad_f_intpart(state->step)) {
		state->last = end[-1];
		state->step = -state->step;
	} else
		state->step -= mad_f_fromint(end - old0);

	return new0 - begin;
}

// end of resample definitions & functions

static int mp3_handle = -1;
static int fd = -1;
static char mp3_tag[128], mp3_tag_encode[260], (*mp3_files)[512];
static int mp3_direct = -1, mp3_nfiles = 0, mp3_index = 0, mp3_jump = 0;
static t_conf_cycle mp3_cycle = conf_cycle_repeat;
static t_conf_encode mp3_encode = conf_encode_gbk;
static bool isPlaying = false, eos = true, isPause = true, manualSw = false;

#ifdef ENABLE_HPRM
static bool hprmEnabled = true;
#endif

#define INPUT_BUFFER_SIZE	8*1152
#define OUTPUT_BUFFER_SIZE	4*1152	/* Must be an integer multiple of 4. */
#ifdef ENABLE_WMA
#define WMA_BUFFER_SIZE WMA_MAX_BUF_SIZE
#endif

static mad_fixed_t Filter[32];
static int DoFilter = 0;
static SceUID mp3_thid = 0;

#ifdef ENABLE_WMA
static bool file_is_mp3 = false;
#endif

static struct mad_stream Stream;
static struct mad_frame Frame;
static struct mad_synth Synth;
static mad_timer_t Timer;
static signed short OutputBuffer[4][
#ifdef ENABLE_WMA
									   WMA_MAX_BUF_SIZE
#else
									   OUTPUT_BUFFER_SIZE / 2
#endif
	];
static byte InputBuffer[INPUT_BUFFER_SIZE + MAD_BUFFER_GUARD],
	*OutputPtr = (byte *) & OutputBuffer[0][0], *GuardPtr = NULL,
	*OutputBufferEnd = (byte *) & OutputBuffer[0][0] + OUTPUT_BUFFER_SIZE;
static int OutputBuffer_Index = 0;

#ifdef ENABLE_WMA
static int OutputBuffer_Pos = 0;
static WmaFile *wma = NULL;
#endif
static t_mp3info mp3info;

#ifdef ENABLE_LYRIC
static t_lyric lyric;
#endif

extern int dup(int fd1)
{
	return (fcntl(fd1, F_DUPFD, 0));
}

extern int dup2(int fd1, int fd2)
{
	close(fd2);
	return (fcntl(fd1, F_DUPFD, fd2));
}

static void apply_filter(struct mad_frame *Frame)
{
	int Channel, Sample, Samples, SubBand;

	Samples = MAD_NSBSAMPLES(&Frame->header);
	if (Frame->header.mode != MAD_MODE_SINGLE_CHANNEL)
		for (Channel = 0; Channel < 2; Channel++)
			for (Sample = 0; Sample < Samples; Sample++)
				for (SubBand = 0; SubBand < 32; SubBand++)
					Frame->sbsample[Channel][Sample][SubBand] =
						mad_f_mul(Frame->sbsample[Channel][Sample][SubBand],
								  Filter[SubBand]);
	else
		for (Sample = 0; Sample < Samples; Sample++)
			for (SubBand = 0; SubBand < 32; SubBand++)
				Frame->sbsample[0][Sample][SubBand] =
					mad_f_mul(Frame->sbsample[0][Sample][SubBand],
							  Filter[SubBand]);
}

static signed short MadFixedToSshort(mad_fixed_t Fixed)
{
	if (Fixed >= MAD_F_ONE)
		return (SHRT_MAX);
	if (Fixed <= -MAD_F_ONE)
		return (-SHRT_MAX);
	return ((signed short) (Fixed >> (MAD_F_FRACBITS - 15)));
}

static bool mp3_load()
{
#ifdef ENABLE_LYRIC
	lyric_close(&lyric);
#endif
	if (mp3_handle >= 0)
		sceAudioChRelease(mp3_handle);
	do {
		while (isPlaying && (mp3_nfiles == 0 || mp3_files == NULL))
			sceKernelDelayThread(500000);

		if (!isPlaying)
			return false;

		if (manualSw || mp3_cycle == conf_cycle_repeat
			|| mp3_cycle == conf_cycle_single) {
			if (mp3_direct == 0) {
				mp3_index--;
				if (mp3_index < 0)
					mp3_index = mp3_nfiles - 1;
			} else if (mp3_direct > 0) {
				mp3_index++;
				if (mp3_index >= mp3_nfiles) {
					if (manualSw || mp3_cycle == conf_cycle_repeat)
						mp3_index = 0;
					else {
						mp3_index = mp3_nfiles - 1;
						isPause = true;
					}
				}
			}
			mp3_direct = 1;
		} else if (mp3_cycle == conf_cycle_random)
			mp3_index = rand() % mp3_nfiles;

#ifdef ENABLE_WMA
		if (file_is_mp3) {
#endif
			if (fd >= 0) {
				sceIoClose(fd);
				fd = -1;
			}
#ifdef ENABLE_WMA
		} else {
			if (wma != NULL) {
				wma_close(wma);
				wma = NULL;
			}
		}
#endif

		mad_timer_reset(&Timer);
		while (mp3_nfiles > 0
			   && (fd =
				   sceIoOpen(mp3_files[mp3_index], PSP_O_RDONLY, 0777)) < 0) {
			if (mp3_index < mp3_nfiles - 1)
				memmove(&mp3_files[mp3_index], &mp3_files[mp3_index + 1],
						512 * (mp3_nfiles - 1 - mp3_index));
			mp3_nfiles--;
			if (mp3_nfiles == 0) {
				mp3_tag_encode[0] = 0;
				free((void *) mp3_files);
				mp3_files = NULL;
			}
		}
	} while (mp3_nfiles == 0);
#ifdef ENABLE_WMA
	if (file_is_mp3) {
#endif
		mad_synth_finish(&Synth);
		mad_frame_finish(&Frame);
		mad_stream_finish(&Stream);
		mp3info_free(&mp3info);
#ifdef ENABLE_WMA
	}
	const char *ext = utils_fileext(mp3_files[mp3_index]);

	file_is_mp3 = (ext && stricmp(ext, "mp3") == 0);
	if (file_is_mp3) {
#endif
		mp3info_read(&mp3info, fd);
		mad_stream_init(&Stream);
		mad_frame_init(&Frame);
		mad_synth_init(&Synth);
		memset(&OutputBuffer[0][0], 0, 4 * OUTPUT_BUFFER_SIZE);
		OutputPtr = (byte *) & OutputBuffer[0][0];
		OutputBufferEnd = OutputPtr + OUTPUT_BUFFER_SIZE * 2;
		if (mp3info.title[0] != 0) {
			if (mp3info.artist[0] != 0)
				SPRINTF_S(mp3_tag, "%s - %s", mp3info.title, mp3info.artist);
			else
				SPRINTF_S(mp3_tag, "? - %s", mp3info.title);
		} else {
			char *mp3_tag2 = strrchr(mp3_files[mp3_index], '/');

			if (mp3_tag2 != NULL)
				mp3_tag2++;
			else
				mp3_tag2 = "";
			strncpy_s(mp3_tag, 128, mp3_tag2, 128);
		}
		mp3_handle =
			sceAudioChReserve(PSP_AUDIO_NEXT_CHANNEL, OUTPUT_BUFFER_SIZE / 4,
							  0);
#ifdef ENABLE_WMA
	} else {
		sceIoClose(fd);
		fd = -1;
		wma = wma_open(mp3_files[mp3_index]);
		if (wma == NULL)
			return false;
		// the wma tag library is buggy!!!
//      charsets_utf8_conv((const byte *)wma->title, (byte *)wma->title);
//      charsets_utf8_conv((const byte *)wma->author, (byte *)wma->author);
//      dbg_hexdump_ascii(d, (const unsigned char*)wma->title, 512);
//      dbg_hexdump_ascii(d, (const unsigned char*)wma->author, 512);
		if (wma->title[0] != 0) {
			if (wma->author[0] != 0)
				SPRINTF_S(mp3_tag, "%s - %s", (const char *) wma->author,
						  (const char *) wma->title);
			else
				SPRINTF_S(mp3_tag, "? - %s", (const char *) wma->title);
		} else {
			char *mp3_tag2 = strrchr(mp3_files[mp3_index], '/');

			if (mp3_tag2 != NULL)
				mp3_tag2++;
			else
				mp3_tag2 = "";
			strncpy_s(mp3_tag, 128, mp3_tag2, 128);
		}
		mp3_handle =
			sceAudioChReserve(PSP_AUDIO_NEXT_CHANNEL,
							  WMA_MAX_BUF_SIZE / 2 / wma->channels,
							  (wma->channels == 2) ? 0 : 1);
	}
#endif
	mp3_set_encode(mp3_encode);
	eos = false;
	manualSw = false;
	mp3_jump = 0;
#ifdef ENABLE_LYRIC
	char lyricname[256];

	strncpy_s(lyricname, NELEMS(lyricname), mp3_files[mp3_index], 256);
	int lsize = strlen(lyricname);

	lyricname[lsize - 3] = 'l';
	lyricname[lsize - 2] = 'r';
	lyricname[lsize - 1] = 'c';
	lyric_open(&lyric, lyricname);
#endif

	return true;
}

static void hprm_fb(int flag)
{
	if (flag != PSP_HPRM_FORWARD && flag != PSP_HPRM_BACK)
		return;
	sceKernelDelayThread(500000);
	if (ctrl_hprm_raw() != flag) {
		if (flag == PSP_HPRM_FORWARD)
			mp3_next();
		else
			mp3_prev();
		return;
	}
}

static void hprm_forward()
{
	hprm_fb(PSP_HPRM_FORWARD);
}

static void hprm_backward()
{
	hprm_fb(PSP_HPRM_BACK);
}

static int mp3_thread(unsigned int args, void *argp)
{
#ifdef ENABLE_HPRM
	dword key;
#endif
	while (isPlaying && (!eos || mp3_load())) {
		if (isPause) {
			while (isPause) {
				sceKernelDelayThread(500000);
#ifdef ENABLE_HPRM
				if (!hprmEnabled)
					continue;
				key = ctrl_hprm();
				switch (key) {
					case PSP_HPRM_PLAYPAUSE:
						if (config.freqs[1] < 4)
							config.freqs[1] = 4;
						scene_power_save(false);
						if (mp3_nfiles == 0 || mp3_files == NULL)
							break;
						isPause = false;
						break;
					case PSP_HPRM_FORWARD:
						mp3_next();
						break;
					case PSP_HPRM_BACK:
						mp3_prev();
						break;
				}
#endif
			}
			continue;
		}
		if (config.freqs[1] < 4)
			config.freqs[1] = 4;
		scene_power_save(false);
#ifdef ENABLE_WMA
		if (file_is_mp3) {
#endif
			if (Stream.buffer == NULL || Stream.error == MAD_ERROR_BUFLEN) {
				size_t ReadSize, Remaining = 0, BufSize;
				byte *ReadStart;

				if (Stream.next_frame != NULL) {
					Remaining = Stream.bufend - Stream.next_frame;
					memmove(InputBuffer, Stream.next_frame, Remaining);
					ReadStart = InputBuffer + Remaining;
					ReadSize = INPUT_BUFFER_SIZE - Remaining;
				} else {
					ReadSize = INPUT_BUFFER_SIZE;
					ReadStart = InputBuffer;
					Remaining = 0;
				}
				BufSize = sceIoRead(fd, ReadStart, ReadSize);
				if (BufSize == 0) {
					eos = true;
					manualSw = false;
					continue;
				}
				if (BufSize < ReadSize) {
					GuardPtr = ReadStart + ReadSize;
					memset(GuardPtr, 0, MAD_BUFFER_GUARD);
					ReadSize += MAD_BUFFER_GUARD;
				}
				mad_stream_buffer(&Stream, InputBuffer, ReadSize + Remaining);
				Stream.error = 0;
			}

			if (mad_frame_decode(&Frame, &Stream) == -1) {
				if (MAD_RECOVERABLE(Stream.error)
					|| Stream.error == MAD_ERROR_BUFLEN)
					continue;
				else {
					eos = true;
					manualSw = false;
					continue;
				}
			}
			mad_timer_add(&Timer, Frame.header.duration);
			if (mp3_jump != 0) {
				if (mp3info.framecount > 0) {
					int pos = 0;

					if (mp3_jump == -2) {
						pos = 0;
						mp3_pause();
					} else if (mp3_jump == -1) {
						if (Timer.seconds > 5) {
							pos =
								(int) ((double) (Timer.seconds - 5) /
									   mp3info.framelen);
							if (pos >= mp3info.framecount)
								pos = mp3info.framecount - 1;
						} else {
							pos = 0;
						}
					} else {
						pos =
							(int) ((double) (Timer.seconds + 5) /
								   mp3info.framelen);
						if (pos >= mp3info.framecount) {
							eos = true;
							pos = mp3info.framecount - 1;
						}
					}
					sceIoLseek32(fd, mp3info.frameoff[pos], PSP_SEEK_SET);
					Timer.seconds = (long) (mp3info.framelen * (double) pos);
					Timer.fraction =
						(unsigned
						 long) ((mp3info.framelen * (double) pos -
								 (double) Timer.seconds) *
								MAD_TIMER_RESOLUTION);
					mad_synth_finish(&Synth);
					mad_frame_finish(&Frame);
					mad_stream_finish(&Stream);
					mad_stream_init(&Stream);
					mad_frame_init(&Frame);
					mad_synth_init(&Synth);
					mp3_jump = 0;
#ifdef ENABLE_LYRIC
					lyric_update_pos(&lyric, (void *) &Timer);
#endif
					continue;
				}
				mp3_jump = 0;
			}
#ifdef ENABLE_LYRIC
			lyric_update_pos(&lyric, (void *) &Timer);
#endif

			if (DoFilter)
				apply_filter(&Frame);

			mad_synth_frame(&Synth, &Frame);

			if (Synth.pcm.samplerate == 44100) {
				OutputPCM.nsamples = Synth.pcm.length;
				OutputPCM.samples[0] = Synth.pcm.samples[0];
				OutputPCM.samples[1] = Synth.pcm.samples[1];
			} else {
				if (resample_init(&Resample, Synth.pcm.samplerate, 44100) == -1)
					continue;
				OutputPCM.nsamples =
					resample_block(&Resample, Synth.pcm.length,
								   Synth.pcm.samples[0], Synth.pcm.samples[1],
								   (*Resampled)[0], (*Resampled)[1]);

				OutputPCM.samples[0] = (*Resampled)[0];
				OutputPCM.samples[1] = (*Resampled)[1];
			}

			if (OutputPCM.nsamples > 0) {
				int i;

				if (MAD_NCHANNELS(&Frame.header) == 2)
					for (i = 0; i < OutputPCM.nsamples; i++) {
						signed short Sample;

						/* Left channel */
						Sample = MadFixedToSshort(OutputPCM.samples[0][i]);
						*(OutputPtr++) = Sample & 0xff;
						*(OutputPtr++) = (Sample >> 8);
						Sample = MadFixedToSshort(OutputPCM.samples[1][i]);
						*(OutputPtr++) = Sample & 0xff;
						*(OutputPtr++) = (Sample >> 8);

						if (OutputPtr == OutputBufferEnd) {
							sceAudioOutputPannedBlocking(mp3_handle, MAXVOLUME,
														 MAXVOLUME, (char *)
														 OutputBuffer
														 [OutputBuffer_Index]);
							OutputBuffer_Index =
								(OutputBuffer_Index + 1) & 0x03;
							OutputPtr =
								(byte *) & OutputBuffer[OutputBuffer_Index][0];
							OutputBufferEnd = OutputPtr + OUTPUT_BUFFER_SIZE;
						}
				} else
					for (i = 0; i < OutputPCM.nsamples; i++) {
						signed short Sample;

						/* Left channel */
						Sample = MadFixedToSshort(OutputPCM.samples[0][i]);
						*(OutputPtr++) = Sample & 0xff;
						*(OutputPtr++) = (Sample >> 8);
						*(OutputPtr++) = Sample & 0xff;
						*(OutputPtr++) = (Sample >> 8);

						if (OutputPtr >= OutputBufferEnd) {
							sceAudioOutputPannedBlocking(mp3_handle, MAXVOLUME,
														 MAXVOLUME, (char *)
														 OutputBuffer
														 [OutputBuffer_Index]);
							OutputBuffer_Index =
								(OutputBuffer_Index + 1) & 0x03;
							OutputPtr =
								(byte *) & OutputBuffer[OutputBuffer_Index][0];
							OutputBufferEnd = OutputPtr + OUTPUT_BUFFER_SIZE;
						}
					}
			} else {
				eos = true;
				manualSw = false;
			}
#ifdef ENABLE_HPRM
			if (!hprmEnabled)
				continue;
			key = ctrl_hprm();
			if (key > 0) {
				switch (key) {
					case PSP_HPRM_PLAYPAUSE:
						isPause = true;
						scene_power_save(true);
						break;
					case PSP_HPRM_FORWARD:
						hprm_forward();
						break;
					case PSP_HPRM_BACK:
						hprm_backward();
						break;
				}
			}
#endif
#ifdef ENABLE_WMA
		} else {
			mad_timer_t incr;
			double tt =
				(double) (WMA_MAX_BUF_SIZE / 2 / wma->channels) /
				wma->samplerate;
			incr.seconds = (long) tt;
			incr.fraction =
				(unsigned long) ((tt - (long) tt) * MAD_TIMER_RESOLUTION);
			int size, p = 0;

			if (mp3_jump != 0) {
				if (mp3_jump == -2) {
					OutputBuffer_Pos = 0;
					eos = true;
					mp3_direct = -1;
					isPlaying = true;
					mad_timer_reset(&Timer);
					break;
				} else if (mp3_jump == -1) {
					if (Timer.seconds > 5) {
						wma_seek(wma, Timer.seconds - 5);
						Timer.seconds -= 5;
						Timer.fraction = 0;
					} else {
						wma_seek(wma, 0);
						mad_timer_reset(&Timer);
					}
					mp3_jump = 0;
					break;
				} else {
					int newp = Timer.seconds + 5;

					if (newp < wma->duration) {
						wma_seek(wma, newp);
						Timer.seconds = newp;
						Timer.fraction = 0;
					}
					mp3_jump = 0;
					break;
				}
			}
			unsigned char *buf = (unsigned char *) wma_decode_frame(wma, &size);

			if (buf == NULL) {
				eos = true;
				manualSw = false;
				continue;
			}

			while (size > 0) {
				if (OutputBuffer_Pos + size >= WMA_MAX_BUF_SIZE) {
					memcpy(&OutputBuffer[OutputBuffer_Index]
						   [OutputBuffer_Pos / 2], &buf[p],
						   WMA_MAX_BUF_SIZE - OutputBuffer_Pos);
					sceAudioOutputPannedBlocking(mp3_handle, MAXVOLUME,
												 MAXVOLUME,
												 OutputBuffer
												 [OutputBuffer_Index]);
					mad_timer_add(&Timer, incr);
#ifdef ENABLE_LYRIC
					lyric_update_pos(&lyric, (void *) &Timer);
#endif
					OutputBuffer_Index = (OutputBuffer_Index + 1) & 3;
					memset(&OutputBuffer[OutputBuffer_Index][0], 0,
						   WMA_MAX_BUF_SIZE);
					p += WMA_MAX_BUF_SIZE - OutputBuffer_Pos;
					size -= WMA_MAX_BUF_SIZE - OutputBuffer_Pos;
					OutputBuffer_Pos = 0;
				} else {
					memcpy(&OutputBuffer[OutputBuffer_Index]
						   [OutputBuffer_Pos / 2], &buf[p], size);
					OutputBuffer_Pos += size;
					p += size;
					size = 0;
				}
#ifdef ENABLE_HPRM
				if (!hprmEnabled)
					continue;
				key = ctrl_hprm();
				if (key > 0) {
					switch (key) {
						case PSP_HPRM_PLAYPAUSE:
							isPause = true;
							scene_power_save(true);
							break;
						case PSP_HPRM_FORWARD:
							hprm_forward();
							break;
						case PSP_HPRM_BACK:
							hprm_backward();
							break;
					}
				}
#endif
			}
		}
#endif
	}
	return 0;
}

extern bool mp3_init()
{
	srand((unsigned) time(NULL));
#ifdef ENABLE_LYRIC
	lyric_init(&lyric);
#endif
	mp3info_init(&mp3info);
#ifdef ENABLE_WMA
	wma_init();
#endif
	STRCPY_S(mp3_tag, "");
	isPlaying = true;
	isPause = true;
	eos = true;
	manualSw = false;
	mp3_direct = -1;
	Resampled = (mad_fixed_t(*)[2][MAX_NSAMPLES]) malloc(sizeof(*Resampled));
	if (mp3_thid <= 0)
		mp3_thid =
			sceKernelCreateThread("mp3 thread", mp3_thread, 0x08, 0x10000,
								  PSP_THREAD_ATTR_USER, NULL);
	if (mp3_thid < 0)
		return false;
	return true;
}

static void mp3_freetune()
{
	mad_synth_finish(&Synth);
	mad_frame_finish(&Frame);
	mad_stream_finish(&Stream);
	free((void *) Resampled);
}

extern void mp3_start()
{
	mp3_direct = -1;
	eos = true;
	manualSw = false;
	isPlaying = true;
	sceKernelStartThread(mp3_thid, 0, NULL);
	sceKernelWakeupThread(mp3_thid);
	sceKernelResumeThread(mp3_thid);
}

extern void mp3_end()
{
#ifdef ENABLE_LYRIC
	lyric_close(&lyric);
#endif
	mp3_stop();
#ifdef ENABLE_WMA
	if (file_is_mp3) {
#endif
		if (fd >= 0) {
			sceIoClose(fd);
			fd = -1;
		}
#ifdef ENABLE_WMA
	} else {
		if (wma != NULL) {
			wma_close(wma);
			wma = NULL;
		}
	}
#endif
	mp3info_free(&mp3info);
	mp3_freetune();
	sceAudioChRelease(mp3_handle);
}

extern void mp3_pause()
{
	isPause = true;
	scene_power_save(true);
}

extern void mp3_resume()
{
	if (mp3_nfiles == 0 || mp3_files == NULL)
		return;
	if (config.freqs[1] < 4)
		config.freqs[1] = 4;
	scene_power_save(false);
	isPause = false;
}

extern void mp3_stop()
{
	isPlaying = false;
	isPause = false;
	eos = true;
	manualSw = false;
	mp3_direct = -1;
	sceKernelWakeupThread(mp3_thid);
	sceKernelResumeThread(mp3_thid);
	sceKernelWaitThreadEnd(mp3_thid, NULL);
	scene_power_save(true);
}

static bool lastpause;
static dword lastpos, lastindex;

extern void mp3_powerdown()
{
	lastpause = isPause;
	isPause = true;
#ifdef ENABLE_WMA
	if (file_is_mp3) {
#endif
		lastpos = sceIoLseek32(fd, 0, PSP_SEEK_CUR);
		if (fd >= 0) {
			sceIoClose(fd);
			fd = -1;
		}
#ifdef ENABLE_WMA
	} else {
		wma_close(wma);
		wma = NULL;
	}
#endif
	lastindex = mp3_index;
	if (config.freqs[1] < 4)
		config.freqs[1] = 4;
	scene_power_save(false);
}

extern void mp3_powerup()
{
	mp3_index = lastindex;
#ifdef ENABLE_WMA
	if (file_is_mp3) {
#endif
		if (mp3_nfiles == 0 || mp3_files == NULL
			|| (fd = sceIoOpen(mp3_files[mp3_index], PSP_O_RDONLY, 0777)) < 0)
			return;
		sceIoLseek32(fd, lastpos, PSP_SEEK_SET);
#ifdef ENABLE_WMA
	} else {
		if (mp3_nfiles == 0 || mp3_files == NULL
			|| (wma = wma_open(mp3_files[mp3_index])) == NULL)
			return;
	}
#endif
	isPause = lastpause;
	scene_power_save(isPause);
}

extern void mp3_list_add_dir(const char *comppath)
{
	p_fat_info info;
	char path[256];
	dword i, count = fat_readdir(comppath, path, &info);

	if (count == INVALID)
		return;
	for (i = 0; i < count; i++) {
		if ((info[i].attr & FAT_FILEATTR_DIRECTORY) > 0) {
			if (info[i].filename[0] == '.')
				continue;
			char subCompPath[260];

			SPRINTF_S(subCompPath, "%s%s/", comppath, info[i].longname);
			mp3_list_add_dir(subCompPath);
			continue;
		}
		const char *ext = utils_fileext(info[i].filename);

		if (ext == NULL || (stricmp(ext, "mp3") != 0
#ifdef ENABLE_WMA
							&& stricmp(ext, "wma") != 0
#endif
			))
			continue;
		if ((mp3_nfiles % 256) == 0) {
			if (mp3_nfiles > 0)
				mp3_files =
					(char (*)[512]) realloc(mp3_files,
											512 * (mp3_nfiles + 256));
			else
				mp3_files = (char (*)[512]) malloc(512 * (mp3_nfiles + 256));
			if (mp3_files == NULL) {
				mp3_nfiles = 0;
				break;
			}
		}
		snprintf_s(mp3_files[mp3_nfiles], 256, "%s%s", path, info[i].filename);
		snprintf_s(&mp3_files[mp3_nfiles][256], 256, "%s%s", comppath,
				   info[i].longname);
		dword j;

		for (j = 0; j < mp3_nfiles; j++)
			if (stricmp(mp3_files[j], mp3_files[mp3_nfiles]) == 0)
				break;
		if (j < mp3_nfiles)
			continue;
		mp3_nfiles++;
	}
	free((void *) info);
}

extern void mp3_list_free()
{
	if (mp3_files != NULL) {
		free((void *) mp3_files);
		mp3_files = NULL;
	}
	mp3_nfiles = 0;
}

bool mp3_is_file_exist(const char *filename)
{
	SceUID uid;

	uid = sceIoOpen(filename, PSP_O_RDONLY, 0777);
	if (uid < 0)
		return false;
	sceIoClose(uid);
	return true;
}

extern bool mp3_list_load(const char *filename)
{
	FILE *fp = fopen(filename, "rt");

	if (fp == NULL)
		return false;
	mp3_nfiles = 0;
	char fname[256], cname[256];

	while (fgets(fname, 256, fp) != NULL && fgets(cname, 256, fp) != NULL) {
		dword len1 = strlen(fname), len2 = strlen(cname);

		if (len1 > 1 && len2 > 1) {
			if (fname[len1 - 1] == '\n')
				fname[len1 - 1] = 0;
			if (cname[len2 - 1] == '\n')
				cname[len2 - 1] = 0;
		} else
			continue;
		if (!mp3_is_file_exist(fname))
			continue;
		if (mp3_nfiles % 256 == 0) {
			if (mp3_nfiles == 0)
				mp3_files = (char (*)[512]) malloc(512 * 256);
			else
				mp3_files =
					(char (*)[512]) realloc(mp3_files,
											512 * (mp3_nfiles + 256));
			if (mp3_files == NULL) {
				mp3_nfiles = 0;
				return false;
			}
		}
		strcpy_s(mp3_files[mp3_nfiles], 256, fname);
		strcpy_s(&mp3_files[mp3_nfiles][256], 256, cname);
		mp3_nfiles++;
	}
	fclose(fp);
	return true;
}

extern void mp3_list_save(const char *filename)
{
	FILE *fp;
	dword i;

	if (mp3_nfiles == 0 || mp3_files == NULL) {
		FILE *fp = fopen(filename, "wt");

		if (fp == NULL)
			return;
		fclose(fp);
		return;
	}
	fp = fopen(filename, "wt");
	if (fp == NULL)
		return;
	for (i = 0; i < mp3_nfiles; i++) {
		fprintf(fp, "%s\n", mp3_files[i]);
		fprintf(fp, "%s\n", &mp3_files[i][256]);
	}
	fclose(fp);
}

extern bool mp3_list_add(const char *filename, const char *longname)
{
	dword i;

	for (i = 0; i < mp3_nfiles; i++)
		if (stricmp(mp3_files[i], filename) == 0)
			return false;
	if (mp3_nfiles % 256 == 0) {
		if (mp3_nfiles > 0)
			mp3_files =
				(char (*)[512]) realloc(mp3_files, 512 * (mp3_nfiles + 256));
		else
			mp3_files = (char (*)[512]) malloc(512 * 256);
		if (mp3_files == NULL) {
			win_msg("ÄÚ´æ²»×ã", COLOR_WHITE, COLOR_WHITE, config.msgbcolor);
			mp3_nfiles = 0;
			mp3_stop();
			return false;
		}
	}
	strcpy_s(mp3_files[mp3_nfiles], 256, filename);
	strcpy_s(&mp3_files[mp3_nfiles][256], 256, longname);
	mp3_nfiles++;
	return true;
}

extern void mp3_list_delete(dword index)
{
	if (index >= mp3_nfiles)
		return;
	bool orgPause = isPause;

	if (mp3_index == index) {
		isPause = true;
		sceKernelDelayThread(50000);
		mp3_next();
	}
	if (index < mp3_nfiles - 1)
		memmove(&mp3_files[index], &mp3_files[index + 1],
				512 * (mp3_nfiles - 1 - index));
	mp3_nfiles--;
	if (mp3_nfiles == 0) {
		mp3_index = 0;
		mp3_tag_encode[0] = 0;
		free((void *) mp3_files);
		mp3_files = NULL;
		mp3_end();
		mp3_init();
		mp3_start();
		return;
	}
	if (mp3_index >= index) {
		mp3_index--;
		if (mp3_index == index - 1)
			isPause = orgPause;
	}
}

extern void mp3_list_moveup(dword index)
{
	if (index == 0)
		return;
	char tempname[512];

	memcpy(tempname, mp3_files[index], 512);
	memcpy(mp3_files[index], mp3_files[index - 1], 512);
	memcpy(mp3_files[index - 1], tempname, 512);
	if (mp3_index == index)
		mp3_index = index - 1;
	else if (mp3_index == index - 1)
		mp3_index = index;
}

extern void mp3_list_movedown(dword index)
{
	if (index >= mp3_nfiles - 1)
		return;
	char tempname[512];

	memcpy(tempname, mp3_files[index], 512);
	memcpy(mp3_files[index], mp3_files[index + 1], 512);
	memcpy(mp3_files[index + 1], tempname, 512);
	if (mp3_index == index)
		mp3_index = index + 1;
	else if (mp3_index == index + 1)
		mp3_index = index;
}

extern dword mp3_list_count()
{
	return mp3_nfiles;
}

extern const char *mp3_list_get_path(dword index)
{
	if (index >= mp3_nfiles)
		return NULL;
	return mp3_files[index];
}

extern const char *mp3_list_get(dword index)
{
	if (index >= mp3_nfiles)
		return NULL;
	return &mp3_files[index][256];
}

extern void mp3_prev()
{
	mp3_direct = 0;
	manualSw = true;
	if (isPause) {
		if (mp3_nfiles > 0 && mp3_files != NULL)
			mp3_load();
	} else
		eos = true;
	isPlaying = true;
}

extern void mp3_next()
{
	mp3_direct = 1;
	manualSw = true;
	if (isPause) {
		if (mp3_nfiles > 0 && mp3_files != NULL)
			mp3_load();
	} else
		eos = true;
	isPlaying = true;
}

extern void mp3_directplay(const char *filename, const char *longname)
{
	dword i;

	for (i = 0; i < mp3_nfiles; i++)
		if (stricmp(mp3_files[i], filename) == 0)
			break;
	if (i >= mp3_nfiles) {
		if (mp3_nfiles % 256 == 0) {
			if (mp3_nfiles > 0)
				mp3_files =
					(char (*)[512]) realloc(mp3_files,
											512 * (mp3_nfiles + 256));
			else
				mp3_files = (char (*)[512]) malloc(512 * 256);
			if (mp3_files == NULL) {
				mp3_nfiles = 0;
				mp3_stop();
				return;
			}
		}
		strcpy_s(mp3_files[mp3_nfiles], 256, filename);
		strcpy_s(&mp3_files[mp3_nfiles][256], 256, longname);
		i = mp3_nfiles;
		mp3_nfiles++;
	}
	mp3_index = i;
	mp3_direct = -1;
	eos = true;
	manualSw = false;
	isPause = false;
	isPlaying = true;
}

extern void mp3_forward()
{
	mp3_jump = 1;
}

extern void mp3_backward()
{
	mp3_jump = -1;
}

extern void mp3_restart()
{
	mp3_jump = -2;
}

extern bool mp3_paused()
{
	return isPause;
}

extern char *mp3_get_tag()
{
	return mp3_tag_encode;
}

extern bool mp3_get_info(int *bitrate, int *sample, int *curlength,
						 int *totallength)
{
#ifdef ENABLE_WMA
	if (file_is_mp3) {
#endif
		if (mp3info.bitrate == 0)
			return false;
		*bitrate = mp3info.bitrate;
		*sample = mp3info.samplerate;
		*curlength = Timer.seconds;
		*totallength = mp3info.length;
#ifdef ENABLE_WMA
	} else {
		if (wma == NULL)
			return false;
		*bitrate = wma->bitrate;
		*sample = wma->samplerate;
		*curlength = Timer.seconds;
		*totallength = wma->duration / 1000;
	}
#endif
	return true;
}

extern void mp3_set_cycle(t_conf_cycle cycle)
{
	mp3_cycle = cycle;
}

extern void mp3_set_encode(t_conf_encode encode)
{
	if (mp3_nfiles == 0 || mp3_files == NULL) {
		STRCPY_S(mp3_tag_encode, "");
		return;
	}
	mp3_encode = encode;
	switch (mp3_encode) {
		case conf_encode_gbk:
			STRCPY_S(mp3_tag_encode, mp3_tag);
			break;
		case conf_encode_big5:
			charsets_big5_conv((const byte *) mp3_tag, (byte *) mp3_tag_encode);
			break;
		case conf_encode_sjis:
			{
				byte *targ = NULL;
				dword size = strlen(mp3_tag);

				charsets_sjis_conv((const byte *) mp3_tag, (byte **) & targ,
								   &size);
				if (targ != NULL) {
					STRCPY_S(mp3_tag_encode, (char *) targ);
					free(targ);
				} else
					STRCPY_S(mp3_tag_encode, mp3_files[mp3_index]);
			}
			break;
		default:
			STRCPY_S(mp3_tag_encode, &mp3_files[mp3_index][256]);
			break;
	}
}

#ifdef ENABLE_HPRM
extern void mp3_set_hprm(bool enable)
{
	hprmEnabled = enable;
}
#endif

#ifdef ENABLE_LYRIC
extern void *mp3_get_lyric()
{
	return (void *) &lyric;
}
#endif

#endif
