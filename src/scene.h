#ifndef _SCENE_H_
#define _SCENE_H_

#include "common/datatype.h"

extern void scene_init(void);
extern void scene_exit(void);
extern void scene_power_save(bool save);
extern const char *scene_appdir(void);

extern dword get_bgcolor_by_time(void);

enum
{
	scene_in_dir,
	scene_in_zip,
	scene_in_umd,
	scene_in_chm,
	scene_in_rar
} where;

typedef struct
{
	int size;
	bool zipped;
} t_fonts;

#endif
