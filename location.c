/* vim:set ts=4 sw=4 cindent ignorecase enc=gbk: */

#include "config.h"

#include <string.h>
#include <pspkernel.h>
#include "location.h"

static char fn[256];
static bool slot[10];

struct _location
{
	char comppath[256];
	char shortpath[256];
	char compname[256];
	bool isreading;
} __attribute__((packed));
typedef struct _location t_location;

extern void location_init(const char * filename, int * slotaval)
{
	strcpy(fn, filename);
	memset(slot, 0, sizeof(bool) * 10);
	int fd = sceIoOpen(fn, PSP_O_RDONLY, 0777);
	if(fd < 0)
	{
		if((fd = sceIoOpen(fn, PSP_O_CREAT | PSP_O_RDWR, 0777)) < 0)
			return;
		byte tempdata[sizeof(t_location) * 10 + sizeof(bool) * 10];
		memset(tempdata, 0, sizeof(t_location) * 10 + sizeof(bool) * 10);
		sceIoWrite(fd, tempdata, sizeof(t_location) * 10 + sizeof(bool) * 10);
	}
	sceIoRead(fd, slot, sizeof(bool) * 10);
	memcpy(slotaval, slot, sizeof(bool) * 10);
	sceIoClose(fd);
}

extern bool location_enum(t_location_enum_func func, void * data)
{
	int fd = sceIoOpen(fn, PSP_O_RDONLY, 0777);
	if(fd < 0)
		return false;
	int i;
	if(sceIoLseek32(fd, sizeof(bool) * 10, PSP_SEEK_SET) != sizeof(bool) * 10)
	{
		sceIoClose(fd);
		return false;
	}
	for(i = 0; i < 10; i ++)
	{
		t_location l;
		memset(&l, 0, sizeof(t_location));
		if(sceIoRead(fd, &l, sizeof(t_location)) != sizeof(t_location))
			break;
		if(slot[i])
			func(i, l.comppath, l.shortpath, l.compname, l.isreading, data);
	}
	sceIoClose(fd);
	return true;
}

extern bool location_get(dword index, char * comppath, char * shortpath, char * compname, bool * isreading)
{
	if(!slot[index])
		return false;
	int fd = sceIoOpen(fn, PSP_O_RDONLY, 0777);
	if(fd < 0)
		return false;
	if(sceIoLseek32(fd, sizeof(t_location) * index + sizeof(bool) * 10, PSP_SEEK_SET) != sizeof(t_location) * index + sizeof(bool) * 10)
	{
		sceIoClose(fd);
		return false;
	}
	t_location l;
	memset(&l, 0, sizeof(t_location));
	sceIoRead(fd, &l, sizeof(t_location));
	sceIoClose(fd);
	strcpy(comppath, l.comppath);
	strcpy(shortpath, l.shortpath);
	strcpy(compname, l.compname);
	* isreading = l.isreading;
	return true;
}

extern bool location_set(dword index, char * comppath, char * shortpath, char * compname, bool isreading)
{
	int fd;
	if((fd = sceIoOpen(fn, PSP_O_RDWR, 0777)) < 0 && (fd = sceIoOpen(fn, PSP_O_CREAT | PSP_O_RDWR, 0777)) < 0)
		return false;
	dword pos = sceIoLseek32(fd, sizeof(t_location) * index + sizeof(bool) * 10, PSP_SEEK_SET);
	if(pos < sizeof(t_location) * index + sizeof(bool) * 10)
	{
		byte tempdata[sizeof(t_location) * 10 + sizeof(bool) * 10 - pos];
		memset(tempdata, 0, sizeof(t_location) * 10 + sizeof(bool) * 10 - pos);
		sceIoWrite(fd, tempdata, sizeof(t_location) * 10 + sizeof(bool) * 10 - pos);
		if(sceIoLseek32(fd, sizeof(t_location) * index + sizeof(bool) * 10, PSP_SEEK_SET) < sizeof(t_location) * index + sizeof(bool) * 10)
		{
			sceIoClose(fd);
			return false;
		}
	}
	t_location t;
	memset(&t, 0, sizeof(t_location));
	strcpy(t.comppath, comppath);
	strcpy(t.shortpath, shortpath);
	strcpy(t.compname, compname);
	t.isreading = isreading;
	sceIoWrite(fd, &t, sizeof(t_location));
	sceIoLseek32(fd, sizeof(bool) * index, PSP_SEEK_SET);
	slot[index] = true;
	sceIoWrite(fd, &slot[index], sizeof(bool));
	sceIoClose(fd);
	return true;
}
