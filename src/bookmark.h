#ifndef _BOOKMARK_H_
#define _BOOKMARK_H_

#include "common/datatype.h"

struct _bookmark
{
	dword row[10];
	dword index, hash;
} __attribute__ ((packed));
typedef struct _bookmark t_bookmark, *p_bookmark;

extern void bookmark_init(const char *fn);
extern p_bookmark bookmark_open(const char *filename);
extern void bookmark_save(p_bookmark bm);
extern void bookmark_delete(p_bookmark bm);
extern void bookmark_close(p_bookmark bm);
extern dword bookmark_autoload(const char *filename);
extern void bookmark_autosave(const char *filename, dword row);
extern bool bookmark_export(p_bookmark bm, const char *filename);
extern bool bookmark_import(const char *filename);

#endif
