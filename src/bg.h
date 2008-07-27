#ifndef _BG_H_
#define _BG_H_

#include "common/datatype.h"
#include "fs.h"

extern void bg_load(const char *filename, const char *archname, pixel bgcolor,
					t_fs_filetype ft, dword grayscale, int where);
extern bool bg_display(void);
extern void bg_cancel(void);
extern void bg_cache(void);
extern void bg_restore(void);

#endif
