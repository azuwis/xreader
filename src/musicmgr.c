#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <pspsdk.h>
#include <pspkernel.h>
#include <psprtc.h>
#include <unistd.h>
#include <fcntl.h>
#include "musicdrv.h"
#include "musicmgr.h"
#include "lyric.h"
#include "fat.h"
#include "strsafe.h"
#include "common/utils.h"
#include "conf.h"
#include "mp3playerME.h"
#include "mpcplayer.h"
#include "madmp3drv.h"
#include "dbg.h"
#include "config.h"
#include "mad.h"
#include "scene.h"
#include "ctrl.h"

static struct music_file *g_music_files = NULL;
static int g_music_list_is_playing = 0;
static int g_thread_actived = 0;
static int g_thread_exited = 0;
static int g_current_pos = 0;
static int g_next_pos = 0;
static int g_cycle_mode = conf_cycle_single;
static t_lyric lyric;
bool g_bID3v1Hack = false;
static bool g_music_hprm_enable = false;
static SceUID music_sema = -1;

static int music_play(int i);
static int get_next_music(bool is_forwarding, bool prevnext_mode,
						  bool should_stop);

static void music_lock(void)
{
	if (music_sema >= 0)
		sceKernelWaitSema(music_sema, 1, NULL);
}

static void music_unlock(void)
{
	if (music_sema >= 0)
		sceKernelSignalSema(music_sema, 1);
}

static struct music_file *new_music_file(const char *spath, const char *lpath)
{
	struct music_file *p = calloc(1, sizeof(*p));

	if (p == NULL)
		return p;

	strcpy(p->shortpath, spath);
	strcpy(p->longpath, lpath);
	p->next = NULL;

	return p;
}

static void free_music_file(struct music_file *p)
{
	free(p);
}

static void get_next_pos(bool is_forwarding, bool prevnext_mode,
						 bool should_stop)
{
	if (g_cycle_mode == conf_cycle_repeat_one)
		g_next_pos = g_current_pos;
	else
		g_next_pos = get_next_music(is_forwarding, prevnext_mode, should_stop);
}

int music_add(const char *spath, const char *lpath)
{
	struct music_file *n;
	struct music_file **tmp;
	int count;

	if (spath == NULL || lpath == NULL)
		return -EINVAL;

	tmp = &g_music_files;
	count = 0;

	while (*tmp) {
		if (!strcmp((*tmp)->shortpath, spath) &&
			!strcmp((*tmp)->longpath, lpath)) {
			return -EBUSY;
		}
		tmp = &(*tmp)->next;
		count++;
	}

	n = new_music_file(spath, lpath);
	if (n != NULL)
		*tmp = n;
	else
		return -ENOMEM;

	return count;
}

int music_find(const char *spath, const char *lpath)
{
	struct music_file *tmp;
	int count;

	if (spath == NULL || lpath == NULL)
		return -EINVAL;

	count = 0;
	for (tmp = g_music_files; tmp; tmp = tmp->next) {
		if (!strcmp(tmp->shortpath, spath) && !strcmp(tmp->longpath, lpath)) {
			return count;
		}
		count++;
	}

	return -1;
}

int music_maxindex(void)
{
	struct music_file *tmp;
	int count;

	count = 0;
	for (tmp = g_music_files; tmp; tmp = tmp->next)
		count++;

	return count;
}

struct music_file *music_get(int i)
{
	struct music_file *tmp;

	if (i < 0)
		return NULL;
	for (tmp = g_music_files; tmp && i > 0; tmp = tmp->next)
		i--;

	return tmp;
}

int music_stop(void)
{
	int ret;

	ret = musicdrv_get_status();
	if (ret < 0)
		return ret;

	if (ret == ST_PLAYING || ret == ST_PAUSED || ret == ST_LOADED) {
		ret = musicdrv_end();
		scene_power_save(true);
	} else
		ret = 0;

	return ret;
}

int music_fforward(int sec)
{
	int ret;

	ret = musicdrv_get_status();
	if (ret < 0)
		return ret;

	if (ret == ST_PLAYING || ret == ST_PAUSED)
		ret = musicdrv_fforward(sec);
	else
		ret = 0;

	return ret;
}

int music_fbackward(int sec)
{
	int ret;

	ret = musicdrv_get_status();
	if (ret < 0)
		return ret;

	if (ret == ST_PLAYING || ret == ST_PAUSED)
		ret = musicdrv_fbackward(sec);
	else
		ret = 0;

	return ret;
}

static const char *get_file_ext(const char *path)
{
	if (path == NULL)
		return "";

	const char *p = strrchr(path, '.');

	if (p == NULL)
		return "";

	return p + 1;
}

static int music_setupdriver(const char *spath, const char *lpath)
{
	const char *ext = get_file_ext(lpath);
	struct music_ops *dev = 0;

	if (!stricmp(ext, "mp3"))
		dev = set_musicdrv("mp3");
	else if (!stricmp(ext, "mpc"))
		dev = set_musicdrv("musepack");

	return dev ? 0 : -ENODEV;
}

static int music_load_config(void)
{
	const struct music_ops *drv = get_musicdrv(NULL);

	if (!strcmp(drv->name, "mp3")) {
		musicdrv_set_opt("use_media_engine", "1");
		return 0;
	}

	return 0;
}

static int music_play(int i)
{
	struct music_file *file = music_get(i);
	int ret;

	if (file == NULL)
		return -EINVAL;
	ret = music_stop();
	if (ret < 0)
		return ret;
	ret = music_setupdriver(file->shortpath, file->longpath);
	if (ret < 0)
		return ret;
	ret = music_load_config();
	if (ret < 0)
		return ret;
	ret = musicdrv_load(file->shortpath, file->longpath);
	if (ret < 0)
		return ret;
	lyric_close(&lyric);
#ifdef ENABLE_LYRIC
	char lyricname[PATH_MAX];
	char lyricshortname[PATH_MAX];

	strncpy_s(lyricname, NELEMS(lyricname), file->longpath, PATH_MAX);
	int lsize = strlen(lyricname);

	lyricname[lsize - 3] = 'l';
	lyricname[lsize - 2] = 'r';
	lyricname[lsize - 1] = 'c';
	if (fat_longnametoshortname(lyricshortname, lyricname, PATH_MAX))
		lyric_open(&lyric, lyricshortname);
#endif
	scene_power_save(false);
	ret = musicdrv_play();
	scene_power_save(true);

	return ret;
}

int music_directplay(const char *spath, const char *lpath)
{
	int pos;
	int ret;

	music_lock();
	music_add(spath, lpath);
	pos = music_find(spath, lpath);
	g_current_pos = pos;
	get_next_pos(true, false, true);
	ret = music_play(pos);
	g_music_list_is_playing = 1;
	music_unlock();
	return 0;
}

int music_del(int i)
{
	int n;
	struct music_file **tmp;

	if (i == g_current_pos) {
		music_next();
	}
	if (g_current_pos == 0) {
		music_stop();
	}

	tmp = &g_music_files;
	n = i;
	while (*tmp && n > 0) {
		n--;
		tmp = &(*tmp)->next;
	}

	struct music_file *p;

	p = (*tmp);
	*tmp = p->next;
	free_music_file(p);

	return music_maxindex();
}

int music_moveup(int i)
{
	struct music_file **tmp;
	struct music_file *a, *b, *c;
	int n;

	if (i < 1 || i >= music_maxindex())
		return -EINVAL;

	tmp = &g_music_files;
	n = i - 1;

	while (*tmp && n > 0) {
		n--;
		tmp = &(*tmp)->next;
	}

	a = *tmp;
	b = a->next;
	c = b->next;
	b->next = a;
	a->next = c;
	*tmp = b;

	return i - 1;
}

int music_movedown(int i)
{
	struct music_file **tmp;
	struct music_file *a, *b, *c;
	int n;

	if (i < 0 || i >= music_maxindex() - 1)
		return -EINVAL;

	tmp = &g_music_files;
	n = i;

	while (*tmp && n > 0) {
		n--;
		tmp = &(*tmp)->next;
	}

	b = (*tmp);
	a = b->next;
	c = a->next;
	a->next = b;
	b->next = c;
	*tmp = a;
	return i + 1;
}

static int get_next_music(bool is_forwarding, bool prevnext_mode,
						  bool should_stop)
{
	int pos = g_current_pos;

	switch (g_cycle_mode) {
		case conf_cycle_repeat:
			if (is_forwarding) {
				pos++;
				if (pos >= music_maxindex())
					pos = 0;
			} else {
				if (pos != 0)
					pos--;
				else
					pos = music_maxindex() - 1;
			}
			break;
		case conf_cycle_single:
			if (is_forwarding) {
				pos++;
				if (pos >= music_maxindex()) {
					pos = 0;
					if (should_stop)
						pos = -1;
				}
			} else {
				if (pos != 0)
					pos--;
				else
					pos = music_maxindex() - 1;
			}
			break;
		case conf_cycle_repeat_one:
			if (prevnext_mode) {
				if (is_forwarding) {
					pos++;
					if (pos >= music_maxindex())
						pos = 0;
				} else {
					if (pos != 0)
						pos--;
					else
						pos = music_maxindex() - 1;
				}
			} else
				pos = pos;
			break;
		case conf_cycle_random:
			pos = rand() % music_maxindex();
			break;
	}

	return pos;
}

int music_list_play(void)
{
	music_lock();
	int ret;

	ret = musicdrv_get_status();
	if (ret == ST_LOADED) {
		music_unlock();
		ret = musicdrv_play();
		g_music_list_is_playing = 1;

		return ret;
	}
	ret = music_play(g_current_pos);
	g_music_list_is_playing = 1;

	music_unlock();
	return ret;
}

int music_list_stop(void)
{
	music_lock();
	if (g_music_list_is_playing) {
		int ret = music_stop();

		if (ret == 0) {
			g_music_list_is_playing = 0;
		} else {
			music_unlock();
			return -1;
		}
	}

	music_unlock();
	return 0;
}

int music_list_playorpause(void)
{
	int ret;

	music_lock();
	if (!g_music_list_is_playing) {
		get_next_pos(true, false, true);
		ret = music_play(g_current_pos);
		g_music_list_is_playing = 1;
		if (ret < 0)
			return ret;
	} else {
		int ret = musicdrv_get_status();

		if (ret < 0) {
			music_unlock();
			return ret;
		}
		if (ret == ST_PLAYING)
			musicdrv_pause();
		else if (ret == ST_PAUSED)
			musicdrv_play();
		else if (ret == ST_STOPPED) {
			music_play(g_current_pos);
		}
		scene_power_save(true);
	}
	music_unlock();

	return 0;
}

int music_ispaused(void)
{
	int ret = musicdrv_get_status();

	if (ret == ST_PAUSED)
		return 1;
	else
		return 0;
}

static SceUID g_music_thread = -1;

static int music_thread(SceSize arg, void *argp)
{
	g_thread_actived = 1;
	g_thread_exited = 0;

	while (g_thread_actived) {
		music_lock();
		if (g_music_list_is_playing) {
			int ret = musicdrv_get_status();

			if (ret == ST_STOPPED || ret == ST_UNKNOWN) {
				g_current_pos = g_next_pos;
				ret = music_play(g_current_pos);
				int i = get_next_music(true, false, true);

				if (i >= 0) {
					get_next_pos(true, false, true);
					g_music_list_is_playing = 1;
				} else {
					g_music_list_is_playing = 0;
				}
			}
			music_unlock();
			sceKernelDelayThread(100000);
		} else {
			music_unlock();
			sceKernelDelayThread(500000);
		}
		if (g_music_hprm_enable) {
			dword key = ctrl_hprm();

			if (key > 0) {
				switch (key) {
					case PSP_HPRM_PLAYPAUSE:
						music_list_playorpause();
						break;
					case PSP_HPRM_FORWARD:
						music_next();
						break;
					case PSP_HPRM_BACK:
						music_prev();
						break;
				}
			}
		}
	}

	g_thread_actived = 0;
	g_thread_exited = 1;

	return 0;
}

int music_init(void)
{
	pspTime tm;

	sceRtcGetCurrentClockLocalTime(&tm);

	srand(tm.microseconds);

	music_sema = sceKernelCreateSema("Music Sema", 0, 1, 1, NULL);

	mp3_init();
	mpc_init();
	madmp3_init();

	// set default drv to mp3
	set_musicdrv("mp3");

	g_music_thread = sceKernelCreateThread("Music Thread",
										   music_thread,
										   0x12, 0x10000, 0, NULL);
	if (g_music_thread < 0) {
		return -EBUSY;
	}

	sceKernelStartThread(g_music_thread, 0, 0);

	return 0;
}

int music_free(void)
{
	music_lock();
	int ret = music_stop();

	g_music_list_is_playing = 0;
	if (ret < 0) {
		music_unlock();
		return ret;
	}
	g_thread_actived = 0;
	music_unlock();
	ret = sceKernelDeleteSema(music_sema);
	if (ret < 0)
		return ret;
	return 0;
}

int music_prev(void)
{
	music_lock();
	int ret = music_stop();

	if (ret < 0) {
		music_unlock();
		return ret;
	}
	int i = get_next_music(false, true, false);

	if (i >= 0) {
		g_current_pos = i;
		get_next_pos(false, true, false);
		music_play(g_current_pos);
		g_music_list_is_playing = 1;
	}
	music_unlock();
	return 0;
}

int music_next(void)
{
	music_lock();
	int ret = music_stop();

	if (ret < 0) {
		music_unlock();
		return ret;
	}
	int i = get_next_music(true, true, false);

	if (i >= 0) {
		g_current_pos = i;
		get_next_pos(true, true, false);
		music_play(g_current_pos);
		g_music_list_is_playing = 1;
	}
	music_unlock();
	return 0;
}

bool is_file_music(const char *filename)
{
	static char musicext[][5] = { "MP3", "MPC" };
//      { "MP3", "OGG", "AA3", "OMA", "OMG", "FLAC", "WMA", "MPC" };
	const char *ext = utils_fileext(filename);

	if (ext == NULL)
		return false;

	size_t i;

	for (i = 0; i < NELEMS(musicext); ++i) {
		if (stricmp(ext, musicext[i]) == 0) {
			return true;
		}
	}

	return false;
}

int music_add_dir(const char *spath, const char *lpath)
{
	p_fat_info info;

	if (spath == NULL || lpath == NULL)
		return -EINVAL;

	dword i, count = fat_readdir(lpath, (char *) spath, &info);

	if (count == INVALID)
		return -EBADF;
	for (i = 0; i < count; i++) {
		if ((info[i].attr & FAT_FILEATTR_DIRECTORY) > 0) {
			if (info[i].filename[0] == '.')
				continue;
			char lpath2[PATH_MAX], spath2[PATH_MAX];

			SPRINTF_S(lpath2, "%s%s/", lpath, info[i].longname);
			SPRINTF_S(spath2, "%s%s/", spath, info[i].filename);
			music_add_dir(spath2, lpath2);
			continue;
		}
		if (is_file_music(info[i].longname) == false)
			continue;
		char sfn[PATH_MAX];
		char lfn[PATH_MAX];

		SPRINTF_S(sfn, "%s%s", spath, info[i].filename);
		SPRINTF_S(lfn, "%s%s", lpath, info[i].longname);
		music_add(sfn, lfn);
	}
	free((void *) info);

	return 0;
}

p_lyric music_get_lyric(void)
{
	struct music_info info;

	info.type = MD_GET_CURTIME;
	if (music_get_info(&info) == 0) {
		mad_timer_t timer;

		timer.seconds = (int) info.cur_time;
		timer.fraction =
			(unsigned long) ((info.cur_time - (long) info.cur_time) *
							 MAD_TIMER_RESOLUTION);
		lyric_update_pos(&lyric, &timer);
	}

	return &lyric;
}

int music_list_save(const char *path)
{
	if (path == NULL)
		return -EINVAL;

	FILE *fp;
	int i;

	fp = fopen(path, "wt");
	if (fp == NULL)
		return -EBADFD;
	for (i = 0; i < music_maxindex(); i++) {
		struct music_file *file;

		file = music_get(i);
		if (file == NULL)
			continue;
		fprintf(fp, "%s\n", file->shortpath);
		fprintf(fp, "%s\n", file->longpath);
	}
	fclose(fp);

	return true;
}

static bool music_is_file_exist(const char *filename)
{
	SceUID uid;

	uid = sceIoOpen(filename, PSP_O_RDONLY, 0777);
	if (uid < 0)
		return false;
	sceIoClose(uid);
	return true;
}

int music_list_load(const char *path)
{
	if (path == NULL)
		return -EINVAL;

	FILE *fp = fopen(path, "rt");

	if (fp == NULL)
		return -EBADFD;

	char fname[PATH_MAX];
	char lfname[PATH_MAX];

	while (fgets(fname, PATH_MAX, fp) != NULL) {
		if (fgets(lfname, PATH_MAX, fp) == NULL)
			break;
		dword len1 = strlen(fname), len2 = strlen(lfname);

		if (len1 > 1 && len2 > 1) {
			if (fname[len1 - 1] == '\n')
				fname[len1 - 1] = 0;
			if (lfname[len2 - 1] == '\n')
				lfname[len2 - 1] = 0;
		} else
			continue;
		if (!music_is_file_exist(fname))
			continue;
		music_add(fname, lfname);
	}
	fclose(fp);
	return 0;
}

int music_set_cycle_mode(int mode)
{
	printf("%s\n", __func__);
	if (mode >= 0 && mode <= conf_cycle_random)
		g_cycle_mode = mode;

	if (g_music_list_is_playing)
		get_next_pos(true, false, true);

	return g_cycle_mode;
}

int music_get_info(struct music_info *info)
{
	int ret;

	if (info == NULL)
		return -EINVAL;

	ret = musicdrv_get_status();

	if (ret != ST_UNKNOWN) {
		return musicdrv_get_info(info);
	}

	return -EBUSY;
}

// for libid3tag
extern int dup(int fd1)
{
	return (fcntl(fd1, F_DUPFD, 0));
}

extern int dup2(int fd1, int fd2)
{
	close(fd2);
	return (fcntl(fd1, F_DUPFD, fd2));
}

int music_loadonly(int i)
{
	struct music_file *file = music_get(i);
	int ret;

	if (file == NULL)
		return -EINVAL;
	ret = music_stop();
	if (ret < 0)
		return ret;
	ret = music_setupdriver(file->shortpath, file->longpath);
	if (ret < 0)
		return ret;
	ret = music_load_config();
	if (ret < 0)
		return ret;
	ret = musicdrv_load(file->shortpath, file->longpath);
	if (ret < 0)
		return ret;
	lyric_close(&lyric);
#ifdef ENABLE_LYRIC
	char lyricname[PATH_MAX];
	char lyricshortname[PATH_MAX];

	strncpy_s(lyricname, NELEMS(lyricname), file->longpath, PATH_MAX);
	int lsize = strlen(lyricname);

	lyricname[lsize - 3] = 'l';
	lyricname[lsize - 2] = 'r';
	lyricname[lsize - 1] = 'c';
	if (fat_longnametoshortname(lyricshortname, lyricname, PATH_MAX))
		lyric_open(&lyric, lyricshortname);
#endif

	return ret;
}

bool music_curr_playing()
{
	int ret = musicdrv_get_status();

	if (ret == ST_PLAYING)
		return true;
	return false;
}

int music_get_current_pos(void)
{
	return g_current_pos;
}

int music_set_hprm(bool enable)
{
	g_music_hprm_enable = enable;

	return 0;
}

static int prev_is_playing = 0;

int music_suspend(void)
{
	int ret;

	music_lock();
	ret = musicdrv_get_status();
	if (ret != ST_STOPPED && ret != ST_UNKNOWN) {
		ret = musicdrv_suspend();
		if (ret < 0) {
			music_unlock();
			return ret;
		}
	}

	prev_is_playing = g_music_list_is_playing;
	g_music_list_is_playing = 0;
	music_unlock();
	return 0;
}

int music_resume(void)
{
	int ret;

	music_lock();
	ret = musicdrv_get_status();
	if (ret != ST_STOPPED && ret != ST_UNKNOWN) {
		struct music_file *fl = music_get(g_current_pos);

		if (fl == NULL) {
			music_unlock();
			return -1;
		}
		ret = musicdrv_resume(fl->shortpath);
		if (ret < 0) {
			music_unlock();
			return ret;
		}
	}

	g_music_list_is_playing = prev_is_playing;
	music_unlock();
	return 0;
}
