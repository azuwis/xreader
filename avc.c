#include "config.h"

#ifdef ENABLE_PMPAVC

#include <pspsdk.h>
#include <pspdisplay.h>
#include <pspge.h>
#include <psppower.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "display.h"
#include "bg.h"
#include "avc.h"

static int pmp_inited = 0, id1 = -1, id2 = -1, id3 = -1, id4 = -1;
static void *m1 = NULL, *m2 = NULL;
char* avc_static_init();
void* valloc(size_t size);
void vfree(void* ptr);
void av_register_all();
char* gu_font_init();
char* gu_font_load(char* name);
void gu_font_close();

extern bool avc_init()
{
	if(pmp_inited)
		return true;

	m1 = valloc(4 * 512 * 272);
	m2 = valloc(4 * 512 * 272);

	av_register_all();
	int id1 = pspSdkLoadStartModule("flash0:/kd/audiocodec_260.prx", PSP_MEMORY_PARTITION_KERNEL);
	if (id1 < 0)
		return false;

	id2 = pspSdkLoadStartModule("flash0:/kd/videocodec_260.prx", PSP_MEMORY_PARTITION_KERNEL);
	if (id2 < 0)
		return false;

	id3 = pspSdkLoadStartModule("flash0:/kd/mpegbase_260.prx", PSP_MEMORY_PARTITION_KERNEL);
	if (id3 < 0)
		return false;

	id4 = pspSdkLoadStartModule("flash0:/kd/mpeg_vsh.prx", PSP_MEMORY_PARTITION_USER);
	if (id4 < 0)
		return false;

	pspSdkFixupImports(id4);

	gu_font_init();
	char ftn[256];
	getcwd(ftn, 256);
	strcat(ftn, "/font10.f");
	gu_font_load(ftn);

	pmp_inited = 1;
	return true;
}

extern void avc_free()
{
	gu_font_close();
	if (m1 != NULL)
	{
		vfree(m1);
	}
	if (m2 != NULL)
	{
		vfree(m2);
	}
	if (id1 >= 0)
	{
		sceKernelStopModule(id1, 0, 0, 0, 0);
		sceKernelUnloadModule(id1);
		id1 = -1;
	}
	if (id2 >= 0)
	{
		sceKernelStopModule(id2, 0, 0, 0, 0);
		sceKernelUnloadModule(id2);
		id2 = -1;
	}
	if (id3 >= 0)
	{
		sceKernelStopModule(id3, 0, 0, 0, 0);
		sceKernelUnloadModule(id3);
		id3 = -1;
	}
	if (id4 >= 0)
	{
		sceKernelStopModule(id4, 0, 0, 0, 0);
		sceKernelUnloadModule(id4);
		id4 = -1;
	}
}

extern void avc_start()
{
	bg_cache();
	void * vram = (void*) (0x40000000 | (u32) sceGeEdramGetAddr());
	sceDisplaySetMode(0, 480, 272);
	sceDisplaySetFrameBuf(vram, 512, PSP_DISPLAY_PIXEL_FORMAT_8888, 1);
	memset(vram, 0, 4 * 512 * 272);
	scePowerLock(0);
}

extern void avc_end()
{
	scePowerUnlock(0);
	disp_init();
	bg_restore();
}

#endif
