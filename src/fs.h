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

#ifndef _FS_H_
#define _FS_H_

#include "common/datatype.h"
#include "conf.h"
#include "win.h"
#include "buffer.h"
#include "unumd.h"
#include "depdb.h"

typedef enum
{
	fs_filetype_dir = 0,
	fs_filetype_umd,
	fs_filetype_pdb,
	fs_filetype_chm,
	fs_filetype_gz,
	fs_filetype_zip,
	fs_filetype_rar,
	fs_filetype_txt,
	fs_filetype_html,
	fs_filetype_prog,
#if defined(ENABLE_IMAGE) || defined(ENABLE_BG)
	fs_filetype_bmp,
	fs_filetype_gif,
	fs_filetype_jpg,
	fs_filetype_png,
	fs_filetype_tga,
#endif
#ifdef ENABLE_MUSIC
	fs_filetype_music,
#endif
	fs_filetype_ebm,
	fs_filetype_unknown,
	fs_filetype_iso
#ifdef ENABLE_TTF
		, fs_filetype_font
#endif
} t_fs_filetype;
extern p_umd_chapter p_umdchapter;
extern dword fs_list_device(const char *dir, const char *sdir,
							p_win_menuitem * mitem, dword icolor,
							dword selicolor, dword selrcolor, dword selbcolor);
extern dword fs_flashdir_to_menu(const char *dir, const char *sdir,
								 p_win_menuitem * mitem, dword icolor,
								 dword selicolor, dword selrcolor,
								 dword selbcolor);
extern dword fs_dir_to_menu(const char *dir, char *sdir, p_win_menuitem * mitem,
							dword icolor, dword selicolor, dword selrcolor,
							dword selbcolor, bool showhidden, bool showunknown);
extern dword fs_zip_to_menu(const char *zipfile, p_win_menuitem * mitem,
							dword icolor, dword selicolor, dword selrcolor,
							dword selbcolor);
extern dword fs_rar_to_menu(const char *rarfile, p_win_menuitem * mitem,
							dword icolor, dword selicolor, dword selrcolor,
							dword selbcolor);
extern dword fs_chm_to_menu(const char *chmfile, p_win_menuitem * mitem,
							dword icolor, dword selicolor, dword selrcolor,
							dword selbcolor);
extern dword fs_umd_to_menu(const char *umdfile, p_win_menuitem * mitem,
							dword icolor, dword selicolor, dword selrcolor,
							dword selbcolor);
p_win_menuitem fs_empty_dir(dword * filecount, dword icolor,
							dword selicolor, dword selrcolor, dword selbcolor);
extern t_fs_filetype fs_file_get_type(const char *filename);
extern bool fs_is_image(t_fs_filetype ft);
extern bool fs_is_txtbook(t_fs_filetype ft);
extern bool fs_is_music(const char* spath, const char* lpath);

#endif
