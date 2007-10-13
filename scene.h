#ifndef _SCENE_H_
#define _SCENE_H_

#include "common/datatype.h"

extern void scene_init();
extern void scene_exit();
extern void scene_power_save(bool save);
extern void scene_exception();
extern const char * scene_appdir();

#endif
