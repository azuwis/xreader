#ifndef XREADER_HAL_H
#define XREADER_HAL_H

#include <pspsdk.h>
#include <pspkernel.h>
#include <psptypes.h>

static inline
SceUID xrIoOpen(const char *file, int flags, SceMode mode)
{
	return sceIoOpen(file, flags, mode);
}

static inline 
int xrIoClose(SceUID fd)
{
	return sceIoClose(fd);
}

static inline
int xrIoRead(SceUID fd, void *data, SceSize size)
{
	return sceIoRead(fd, data, size);
}

static inline
int xrIoWrite(SceUID fd, const void *data, SceSize size)
{
	return sceIoWrite(fd, data, size);
}

static inline
SceOff xrIoLseek(SceUID fd, SceOff offset, int whence)
{
	return sceIoLseek(fd, offset, whence);
}

static inline
SceOff xrIoLseek32(SceUID fd, SceOff offset, int whence)
{
	return sceIoLseek32(fd, offset, whence);
}

static inline
int xrIoGetstat(const char *file, SceIoStat *stat)
{
	return sceIoGetstat(file, stat);
}

static inline
int xrIoMkdir(const char *dir, SceMode mode)
{
	return sceIoMkdir(dir, mode);
}

static inline
int xrIoRmdir(const char *path)
{
	return xrIoRmdir(path);
}

static inline
SceUID xrIoDopen(const char *dirname)
{
	return sceIoDopen(dirname);
}

static inline
int xrIoDread(SceUID fd, SceIoDirent *dir)
{
	return sceIoDread(fd, dir);
}

static inline
int xrIoDclose(SceUID fd)
{
	return sceIoDclose(fd);
}

static inline
int xrIoChangeAsyncPriority(SceUID fd, int pri)
{
	return sceIoChangeAsyncPriority(fd, pri);
}

static inline
int xrIoReadAsync(SceUID fd, void *data, SceSize size)
{
	return sceIoReadAsync(fd, data, size);
}

static inline
int xrIoWriteAsync(SceUID fd, const void *data, SceSize size)
{
	return sceIoWriteAsync(fd, data, size);
}

static inline
int xrIoWaitAsync(SceUID fd, SceInt64 *res)
{
	return sceIoWaitAsync(fd, res);
}

static inline
int xrIoChstat(const char *file, SceIoStat *stat, int bits)
{
	return sceIoChstat(file, stat, bits);
}

static inline
int xrIoRemove(const char *file)
{
	return sceIoRemove(file);
}

#endif
