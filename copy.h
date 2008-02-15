/* vim:set ts=4 sw=4 cindent ignorecase enc=gbk: */

#ifndef _COPY_H_
#define _COPY_H_

#include "common/datatype.h"

typedef void (*t_copy_cb) (const char *src, const char *dest, bool succ,
						   void *data);
typedef bool(*t_copy_overwritecb) (const char *filename, void *data);

extern bool copy_file(const char *src, const char *dest, t_copy_cb cb,
					  t_copy_overwritecb ocb, void *data);
extern dword copy_dir(const char *src, const char *dest, t_copy_cb cb,
					  t_copy_overwritecb ocb, void *data);
extern bool move_file(const char *src, const char *dest, t_copy_cb cb,
					  t_copy_overwritecb ocb, void *data);
extern dword move_dir(const char *src, const char *dest, t_copy_cb cb,
					  t_copy_overwritecb ocb, void *data);

#endif
