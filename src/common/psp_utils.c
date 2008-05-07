#include <pspkernel.h>
#include <pspusb.h>
#include "psp_utils.h"

extern int psp_load_start_module(const char *path)
{
	int mid;
	int result;

	mid = sceKernelLoadModule(path, 0, 0);

	if (mid == 0x80020139) {
		return 0;				// already loaded 
	}

	if (mid < 0) {
		return mid;
	}

	result = sceKernelStartModule(mid, 0, 0, 0, 0);
	if (result < 0) {
		return result;
	}

	return mid;
}
