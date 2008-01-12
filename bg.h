/* vim:set ts=4 sw=4 cindent ignorecase enc=gbk: */

#ifndef _BG_H_
#define _BG_H_

#include "common/datatype.h"
#include "fs.h"

extern void bg_load(const char * filename, pixel bgcolor, t_fs_filetype ft, dword grayscale);
extern bool bg_display();
extern void bg_cancel();
extern void bg_cache();
extern void bg_restore();

#endif
