#ifndef _UTILS_H_
#define _UTILS_H_

#include "strsafe.h"

#include "datatype.h"

extern dword utils_dword2string(dword dw, char *dest, dword width);
extern bool utils_string2dword(const char *src, dword * dw);
extern bool utils_string2double(const char *src, double *db);
extern const char *utils_fileext(const char *filename);
extern bool utils_del_file(const char *file);
extern dword utils_del_dir(const char *dir);
bool utils_is_file_exists(const char *filename);
void *safe_realloc(void *ptr, size_t size);

#endif
