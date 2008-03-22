/* vim:set ts=4 sw=4 cindent ignorecase enc=gbk: */

#ifndef _SCENE_H_
#define _SCENE_H_

#include "common/datatype.h"

extern void scene_init();
extern void scene_exit();
extern void scene_power_save(bool save);
extern void scene_exception();
extern const char *scene_appdir();

extern dword GetBGColorByTime(void);

enum
{
	scene_in_dir,
	scene_in_zip,
	scene_in_chm,
	scene_in_rar
} where;

typedef struct
{
	int size;
	bool zipped;
} t_fonts;

#endif
