#pragma once

#include "lyric.h"

struct music_file
{
	char shortpath[PATH_MAX];
	char longpath[PATH_MAX];

	struct music_file *next;
};

int music_add(const char *spath, const char *lpath);
int music_add_dir(const char *spath, const char *lpath);
int music_find(const char *spath, const char *lpath);
int music_directplay(const char *spath, const char *lpath);
struct music_file *music_get(int i);
int music_maxindex(void);
int music_del(int i);
int music_moveup(int i);
int music_movedown(int i);
int music_list_play(void);
int music_list_stop(void);
int music_list_playorpause(void);
int music_ispaused(void);
int music_init(void);
int music_free(void);
int music_resume(void);
int music_suspend(void);
int music_prev(void);
int music_next(void);
int music_set_cycle_mode(int);
int music_list_save(const char *path);
int music_list_load(const char *path);
p_lyric music_get_lyric(void);
struct music_info;
int music_get_info(struct music_info *info);
int music_loadonly(int i);
bool music_curr_playing();
int music_get_current_pos(void);
int music_set_hprm(bool enable);
