#ifndef XREADER_HAL_H
#define XREADER_HAL_H

#include <pspsdk.h>
#include <pspkernel.h>
#include <psptypes.h>

static inline SceUID xrIoOpen(const char *file, int flags, SceMode mode)
{
	return sceIoOpen(file, flags, mode);
}

#endif
