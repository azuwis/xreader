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
