#ifndef _MP3_H_
#define _MP3_H_

#include "common/datatype.h"

extern bool mp3_init(void);
extern void mp3_end(void);
extern void mp3_start(void);
extern void mp3_pause(void);
extern void mp3_resume(void);
extern void mp3_stop(void);
extern void mp3_powerdown(void);
extern void mp3_powerup(void);
extern void mp3_list_add_dir(const char *comppath);
extern void mp3_list_free(void);
extern bool mp3_list_load(const char *filename);
extern void mp3_list_save(const char *filename);
extern bool mp3_list_add(const char *filename, const char *longname);
extern void mp3_list_delete(dword index);
extern void mp3_list_moveup(dword index);
extern void mp3_list_movedown(dword index);
extern dword mp3_list_count(void);
extern const char *mp3_list_get(dword index);
extern const char *mp3_list_get_path(dword index);
extern void mp3_prev(void);
extern void mp3_next(void);
extern void mp3_directplay(const char *filename, const char *longname);
extern void mp3_forward(void);
extern void mp3_backward(void);
extern void mp3_restart(void);
extern bool mp3_paused(void);
extern char *mp3_get_tag(void);
extern bool mp3_get_info(int *bitrate, int *sample, int *curlength,
						 int *totallength);
extern void mp3_set_cycle(t_conf_cycle cycle);
extern void mp3_set_encode(t_conf_encode encode);

#ifdef ENABLE_HPRM
extern void mp3_set_hprm(bool enable);
#endif
#ifdef ENABLE_LYRIC
extern void *mp3_get_lyric(void);
#endif

#endif
