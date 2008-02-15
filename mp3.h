/* vim:set ts=4 sw=4 cindent ignorecase enc=gbk: */

#ifndef _MP3_H_
#define _MP3_H_

#include "common/datatype.h"

extern bool mp3_init();
extern void mp3_end();
extern void mp3_start();
extern void mp3_pause();
extern void mp3_resume();
extern void mp3_stop();
extern void mp3_powerdown();
extern void mp3_powerup();
extern void mp3_list_add_dir(const char *comppath);
extern void mp3_list_free();
extern bool mp3_list_load(const char *filename);
extern void mp3_list_save(const char *filename);
extern bool mp3_list_add(const char *filename, const char *longname);
extern void mp3_list_delete(dword index);
extern void mp3_list_moveup(dword index);
extern void mp3_list_movedown(dword index);
extern dword mp3_list_count();
extern const char *mp3_list_get(dword index);
extern void mp3_prev();
extern void mp3_next();
extern void mp3_directplay(const char *filename, const char *longname);
extern void mp3_forward();
extern void mp3_backward();
extern void mp3_restart();
extern bool mp3_paused();
extern char *mp3_get_tag();
extern bool mp3_get_info(int *bitrate, int *sample, int *curlength,
						 int *totallength);
extern void mp3_set_cycle(t_conf_cycle cycle);
extern void mp3_set_encode(t_conf_encode encode);

#ifdef ENABLE_HPRM
extern void mp3_set_hprm(bool enable);
#endif
#ifdef ENABLE_LYRIC
extern void *mp3_get_lyric();
#endif

#endif
