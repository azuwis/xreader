#ifndef _UTILS_H_
#define _UTILS_H_

#include "datatype.h"

extern dword utils_dword2string(dword dw, char * dest, dword width);
extern bool utils_string2dword(const char * src, dword * dw);
extern bool utils_string2double(const char * src, double * db);
extern const char * utils_fileext(const char * filename);
extern void utils_del_file(const char * file);
extern void utils_del_dir(const char * dir);

size_t strncpy_s( char *strDest, size_t numberOfElements, const char *strSource, size_t count);
size_t strcpy_s( char *strDestination, size_t numberOfElements, const char *strSource );
size_t strncat_s ( char *strDest, size_t numberOfElements, const char *strSource, size_t count);
size_t strcat_s( char *strDestination, size_t numberOfElements, const char *strSource );
int snprintf_s( char *buffer, size_t sizeOfBuffer, const char *format, ...);
size_t mbcslen(const unsigned char* str);
size_t mbcsncpy_s(unsigned char* dst, size_t nBytes, const unsigned char* src, size_t n);

bool utils_is_file_exists(const char *filename);

#define NELEMS(n) (sizeof(n) / sizeof(n[0]))

#define STRCPY_S(d, s) strcpy_s((d), (sizeof(d) / sizeof(d[0])), (s))
#define STRCAT_S(d, s) strcat_s((d), (sizeof(d) / sizeof(d[0])), (s))
#define SPRINTF_S(str, fmt...) snprintf_s((str), \
							  ((sizeof(str) / sizeof(str[0]))), fmt)
#define MBCSCPY_S(dst, src, n) mbcsncpy_s((dst), (sizeof(dst) / sizeof(dst[0])), (src), (n))

#endif
