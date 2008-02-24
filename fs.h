/* vim:set ts=4 sw=4 cindent ignorecase enc=gbk: */

#ifndef _FS_H_
#define _FS_H_

#include "common/datatype.h"
#include "conf.h"
#include "win.h"
#include "buffer.h"

typedef enum
{
	fs_filetype_dir = 0,
	fs_filetype_chm,
	fs_filetype_gz,
	fs_filetype_zip,
	fs_filetype_rar,
	fs_filetype_txt,
	fs_filetype_html,
	fs_filetype_prog,
#ifdef ENABLE_PMPAVC
	fs_filetype_pmp,
#endif
#if defined(ENABLE_IMAGE) || defined(ENABLE_BG)
	fs_filetype_bmp,
	fs_filetype_gif,
	fs_filetype_jpg,
	fs_filetype_png,
	fs_filetype_tga,
#endif
#ifdef ENABLE_MUSIC
	fs_filetype_mp3,
	fs_filetype_aa3,
#ifdef ENABLE_WMA
	fs_filetype_wma,
#endif
#endif
	fs_filetype_ebm,
	fs_filetype_unknown
} t_fs_filetype;

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
extern t_fs_filetype fs_file_get_type(const char *filename);
extern bool fs_is_image(t_fs_filetype ft);
extern bool fs_is_txtbook(t_fs_filetype ft);

extern void extract_archive_file_into_buffer(buffer **buf,
											 const char* archname,
											 const char* archpath,
											 t_fs_filetype filetype);

#endif
