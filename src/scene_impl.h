/* vim:set ts=4 sw=4 cindent ignorecase enc=gbk: */

#ifndef SCENE_IMP_H
#define SCENE_IMP_H
#include "scene.h"

#ifdef ENABLE_PMPAVC
extern bool pmp_restart;
#endif
extern char appdir[PATH_MAX], copydir[PATH_MAX], cutdir[PATH_MAX];
extern dword drperpage, rowsperpage, pixelsperrow;
extern p_bookmark bm;
extern p_text fs;
extern t_conf config;
extern p_win_menuitem filelist, copylist, cutlist;
extern dword filecount, copycount, cutcount;

#ifdef ENABLE_BG
extern bool repaintbg;
#endif
extern bool imgreading, locreading;
extern int locaval[10];
extern t_fonts fonts[5], bookfonts[21];
extern int offset;
extern bool scene_readbook_in_raw_mode;

t_win_menu_op exit_confirm();
extern void scene_init();
extern void scene_exit();
extern void scene_power_save(bool save);
extern void scene_exception();
extern const char *scene_appdir();
dword scene_readbook(dword selidx);
dword scene_readbook_raw(const char *title, const unsigned char *data,
						 size_t size, t_fs_filetype ft);
extern void scene_mountrbkey(dword * ctlkey, dword * ctlkey2, dword * ku,
							 dword * kd, dword * kl, dword * kr);
extern bool scene_bookmark(dword * orgp);
extern dword scene_options(dword * selidx);
extern void scene_mp3bar();

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
	char tr[8], *trow;
	bool text_needrf, text_needrp, text_needrb;
	char filename[PATH_MAX], bookmarkname[PATH_MAX], archname[PATH_MAX];
	dword rrow;
} BookViewData, *PBookViewData;

extern BookViewData cur_book_view, prev_book_view;

#endif
