/* vim:set ts=4 sw=4 cindent ignorecase enc=gbk: */

#ifndef _LOCATION_H_
#define _LOCATION_H_

#include "common/datatype.h"

typedef void (*t_location_enum_func)(dword index, char * comppath, char * shortpath, char * compname, bool isreading, void * data);

extern void location_init(const char * filename, int * slotaval);
extern bool location_get(dword index, char * comppath, char * shortpath, char * compname, bool * isreading);
extern bool location_set(dword index, char * comppath, char * shortpath, char * compname, bool isreading);
extern bool location_enum(t_location_enum_func func, void * data);

#endif
