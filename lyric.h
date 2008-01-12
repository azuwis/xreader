/* vim:set ts=4 sw=4 cindent ignorecase enc=gbk: */

#ifndef _LYRIC_H_
#define _LYRIC_H_

#include <mad.h>
#include "common/datatype.h"

typedef struct _lyricline{
	mad_timer_t t;
	const char * line;
	dword size;
} t_lyricline, * p_lyricline;

typedef struct _lyric {
	bool succ;
	SceUID sema;
	bool changed;
	char * whole;
	dword size;
	const char * ar;
	const char * ti;
	const char * al;
	const char * by;
	double off;
	int count;
	p_lyricline lines;
	int idx;
} t_lyric, * p_lyric;

extern void lyric_init(p_lyric l);
extern bool lyric_open(p_lyric l, const char * filename);
extern void lyric_close(p_lyric l);
extern void lyric_update_pos(p_lyric l, void * tm);
extern bool lyric_get_cur_lines(p_lyric l, int extralines, const char ** lines, dword * sizes);
void lyric_decode(const char* lrcsrc, char *lrcdst, dword *size);
extern bool lyric_check_changed(p_lyric l);

#endif
