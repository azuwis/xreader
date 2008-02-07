#include <stdio.h>
#include <string.h>
#include <pspkernel.h>
#include <stdarg.h>
#include "config.h"
#include "utils.h"
#include "dbg.h"

extern dword utils_dword2string(dword dw, char * dest, dword width)
{
	dest[width] = 0;
	if(dw == 0)
	{
		dest[--width] = '0';
		return width;
	}
	while(dw > 0 && width > 0)
	{
		dest[-- width] = '0' + (dw % 10);
		dw /= 10;
	}
	return width;
}

extern bool utils_string2dword(const char * src, dword * dw)
{
	* dw = 0;
	while(* src >= '0' && * src <= '9')
	{
		* dw = * dw * 10 + (* src - '0');
		src ++;
	}
	return (* src == 0);
}

extern bool utils_string2double(const char * src, double * db)
{
	* db = 0.0;
	bool doted = false;
	double p = 0.1;
	while((* src >= '0' && * src <= '9') || * src == '.')
	{
		if(* src == '.')
		{
			if(doted)
				return false;
			else
				doted = true;
		}
		else
		{
			if(doted)
			{
				* db = * db + p * (* src - '0');
				p = p / 10;
			}
			else
				* db = * db * 10 + (* src - '0');
		}
		src ++;
	}
	return (* src == 0);
}

extern const char * utils_fileext(const char * filename)
{
	dword len = strlen(filename);
	const char * p = filename + len;
	while(p > filename && *p != '.' && *p != '/') p --;
	if(*p == '.')
		return p + 1;
	else
		return NULL;
}

extern void utils_del_file(const char * file)
{
	SceIoStat stat;
	memset(&stat, 0, sizeof(SceIoStat));
	sceIoGetstat(file, &stat);
	stat.st_attr &= ~0x0F;
	sceIoChstat(file, &stat, 3);
	sceIoRemove(file);
}

extern void utils_del_dir(const char * dir)
{
	int dl = sceIoDopen(dir);
	if(dl < 0)
		return;
	SceIoDirent sid;
	memset(&sid, 0, sizeof(SceIoDirent));
	while(sceIoDread(dl, &sid))
	{
		if(sid.d_name[0] == '.') continue;
		char compPath[260];
		sprintf(compPath, "%s/%s", dir, sid.d_name);
		if(FIO_S_ISDIR(sid.d_stat.st_mode))
		{
			utils_del_dir(compPath);
			continue;
		}
		utils_del_file(compPath);
		memset(&sid, 0, sizeof(SceIoDirent));
	}
	sceIoDclose(dl);
	sceIoRmdir(dir);
}

extern DBG *d;

size_t
strncpy_s(
	char *strDest,
	size_t numberOfElements,
	const char *strSource,
	size_t count
)
{
	if(!strDest || !strSource || numberOfElements == 0)
		return 0;
#ifdef _DEBUG
	if(numberOfElements == 4) {
		dbg_printf(d, "strncpy_s: strDest可能退化为指针: %s", strSource);
	}
#endif
	strncpy(strDest, strSource, numberOfElements < count ? 
			numberOfElements : count);
	strDest[numberOfElements - 1] = '\0';
	return strnlen(strDest, numberOfElements);
}

size_t
strcpy_s(
	char *strDestination,
	size_t numberOfElements,
	const char *strSource 
)
{
	return strncpy_s(strDestination, numberOfElements, strSource, -1);
}

size_t strncat_s ( 
	char *strDest,
	size_t numberOfElements,
	const char *strSource,
	size_t count
)
{
	size_t rest;
	if(!strDest || !strSource || numberOfElements == 0)
		return 0;

#ifdef _DEBUG
	if(numberOfElements == 4) {
		dbg_printf(d, "strncat_s: strDest可能退化为指针: %s", strSource);
	}
#endif

	rest = numberOfElements - strnlen(strDest, numberOfElements) - 1;
	strncat(strDest, strSource, rest < count ? rest: count);

	return strnlen(strDest, numberOfElements);
}

size_t
strcat_s(
	char *strDestination,
	size_t numberOfElements,
	const char *strSource 
)
{
	return strncat_s(strDestination, numberOfElements, strSource, -1);
}

int snprintf_s(
		char *buffer,
		size_t sizeOfBuffer,
		const char *format, ...
)
{
	if(!buffer || sizeOfBuffer == 0)
		return -1;

#ifdef _DEBUG
	if(sizeOfBuffer == 4) {
		dbg_printf(d, "snprintf_s: strDest可能退化为指针: %s", format);
	}
#endif

	va_list va;

	va_start(va, format);

	int ret = vsnprintf(buffer, sizeOfBuffer, format, va);
	buffer[sizeOfBuffer - 1] = '\0';

	va_end(va);

	return ret;
}

size_t mbcslen(const unsigned char* str)
{
	size_t s=0;

	if(!str)
		return 0;

	while(*str != '\0') {
		if(*str > 0x80 && *(str+1) != '\0') {
			s++;
			str+=2;
		}
		else {
			s++;
			str++;
		}
	}

	return s;
}

size_t mbcsncpy_s(unsigned char* dst, size_t nBytes, const unsigned char* src, size_t n)
{
	unsigned char* start = dst;

	if(!dst || !src || nBytes == 0 || n == 0)
		return 0;

#ifdef _DEBUG
	if(nBytes == 4) {
		dbg_printf(d, "mbcsncpy_s: dst可能退化为指针: %s", src);
	}
#endif

	while(nBytes > 0 && n > 0 && *src != '\0') {
		if(*src > 0x80 && *(src+1) != '\0') {
			if(nBytes > 2) {
				nBytes-=2, n--;
				*dst++ = *src++;
				*dst++ = *src++;
			}
			else 
			{
				break;
			}
		}
		else {
			if(nBytes > 1) {
				nBytes--, n--;
				*dst++ = *src++;
			}
			else {
				break;
			}
		}
	}

	*dst = '\0';

	return mbcslen(start);
}

