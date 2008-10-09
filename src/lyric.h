/*
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

#ifndef _LYRIC_H_
#define _LYRIC_H_

#include <mad.h>
#include "common/datatype.h"

typedef struct _lyricline
{
	mad_timer_t t;
	const char *line;
	dword size;
} t_lyricline, *p_lyricline;

typedef struct _lyric
{
	bool succ;
	SceUID sema;
	bool changed;
	char *whole;
	dword size;
	const char *ar;
	const char *ti;
	const char *al;
	const char *by;
	double off;
	int count;
	p_lyricline lines;
	int idx;
} t_lyric, *p_lyric;

extern void lyric_init(p_lyric l);
extern bool lyric_open(p_lyric l, const char *filename);
extern void lyric_close(p_lyric l);
extern void lyric_update_pos(p_lyric l, void *tm);
extern bool lyric_get_cur_lines(p_lyric l, int extralines, const char **lines,
								dword * sizes);
void lyric_decode(const char *lrcsrc, char *lrcdst, dword * size);
extern bool lyric_check_changed(p_lyric l);

#endif
