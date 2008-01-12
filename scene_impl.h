/* vim:set ts=4 sw=4 cindent ignorecase enc=gbk: */

#ifndef SCENE_IMP_H
#define SCENE_IMP_H
#include "scene.h"

#ifdef ENABLE_PMPAVC
extern bool pmp_restart;
#endif
extern char appdir[256], copydir[256], cutdir[256];
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
extern int fontcount, fontindex, bookfontcount, bookfontindex, ttfsize;
extern int offset;

t_win_menu_op exit_confirm();
extern void scene_init();
extern void scene_exit();
extern void scene_power_save(bool save);
extern void scene_exception();
extern const char * scene_appdir();
dword scene_readbook(dword selidx);
extern void scene_mountrbkey(dword * ctlkey, dword * ctlkey2, dword * ku, dword * kd, dword * kl, dword * kr);
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

#endif
