#include <stdio.h>
#include <string.h>
#include <pspkernel.h>
#include <stdarg.h>
#include "config.h"
#include "utils.h"
#include "strsafe.h"
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

extern bool utils_del_file(const char * file)
{
	int result;
	SceIoStat stat;
	memset(&stat, 0, sizeof(SceIoStat));
	result = sceIoGetstat(file, &stat);
	if (result < 0)
		return false;
	stat.st_attr &= ~0x0F;
	result = sceIoChstat(file, &stat, 3);
	if (result < 0)
		return false;
	result = sceIoRemove(file);
	if (result < 0)
		return false;

	return true;
}

extern dword utils_del_dir(const char * dir)
{
	dword count = 0;
	int dl = sceIoDopen(dir);
	if(dl < 0)
		return count;
	SceIoDirent sid;
	memset(&sid, 0, sizeof(SceIoDirent));
	while(sceIoDread(dl, &sid))
	{
		if(sid.d_name[0] == '.') continue;
		char compPath[260];
		SPRINTF_S(compPath, "%s/%s", dir, sid.d_name);
		if(FIO_S_ISDIR(sid.d_stat.st_mode))
		{
			if (utils_del_dir(compPath)) {
				count++;
			}
			continue;
		}
		if (utils_del_file(compPath)) {
			count++;
		}
		memset(&sid, 0, sizeof(SceIoDirent));
	}
	sceIoDclose(dl);
	sceIoRmdir(dir);

	return count;
}

bool utils_is_file_exists(const char *filename)
{
	if(!filename)
		return false;

	SceUID uid;
	uid = sceIoOpen(filename, PSP_O_RDONLY, 0777);
	if(uid < 0)
		return false;
	sceIoClose(uid);
	return true;
}

void *realloc_free_when_fail(void *ptr, size_t size)
{
	void *p = realloc(ptr, size);
	if (p == NULL) {
		if (ptr)
			free(ptr);
		ptr = NULL;
	}

	return p;
}
