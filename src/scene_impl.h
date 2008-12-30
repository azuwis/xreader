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

#ifndef SCENE_IMP_H
#define SCENE_IMP_H
#include "scene.h"

extern char appdir[PATH_MAX], copydir[PATH_MAX], cutdir[PATH_MAX];
extern dword drperpage, rowsperpage, pixelsperrow;
extern p_bookmark bm;
extern p_text fs;
extern p_win_menuitem filelist, copylist, cutlist;
extern dword filecount, copycount, cutcount;

#ifdef ENABLE_BG
extern bool repaintbg;
#endif
extern bool imgreading, locreading;
extern int locaval[10];
extern t_fonts fonts[5], bookfonts[21];
extern bool scene_readbook_in_raw_mode;

t_win_menu_op exit_confirm(void);
extern void scene_init(void);
extern void scene_exit(void);
extern void scene_power_save(bool save);
extern const char *scene_appdir(void);
dword scene_readbook(dword selidx);
dword scene_readbook_raw(const char *title, const unsigned char *data,
						 size_t size, t_fs_filetype ft);
extern void scene_mountrbkey(dword * ctlkey, dword * ctlkey2, dword * ku,
							 dword * kd, dword * kl, dword * kr);
extern bool scene_bookmark(dword * orgp);
extern dword scene_options(dword * selidx);
extern void scene_mp3bar(void);
extern int scene_get_infobar_height(void);
extern int default_predraw(const win_menu_predraw_data * pData, const char *str,
						   int max_height, int *left, int *right, int *upper,
						   int *bottom, int width_fixup);
extern int prompt_press_any_key(void);
extern int get_center_pos(int left, int right, const char *str);

dword scene_txtkey(dword * selidx);

#ifdef ENABLE_IMAGE
dword scene_readimage(dword selidx);
dword scene_imgkey(dword * selidx);
#endif
extern dword scene_readbook(dword selidx);

#ifdef ENABLE_IMAGE
extern dword scene_readimage(dword selidx);
#endif

typedef struct _BookViewData
{
	int rowtop;
	bool text_needrf, text_needrp, text_needrb;
	char filename[PATH_MAX], bookmarkname[PATH_MAX], archname[PATH_MAX];
	dword rrow;
} BookViewData, *PBookViewData;

extern BookViewData cur_book_view, prev_book_view;

extern char prev_path[PATH_MAX], prev_shortpath[PATH_MAX];
extern char prev_lastfile[PATH_MAX];
extern int prev_where;

#endif
