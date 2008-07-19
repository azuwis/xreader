#include "config.h"

#include <pspdebug.h>
#include <pspkernel.h>
#include <pspsdk.h>
#include <psppower.h>
#include <pspdisplay.h>
#include "fat.h"
#include "conf.h"
#ifdef ENABLE_PMPAVC
#include "avc.h"
#endif
#ifdef ENABLE_MUSIC
#include "mp3.h"
#endif
#include "scene.h"
#include "conf.h"
#include "display.h"
#include "ctrl.h"

PSP_MODULE_INFO("XREADER", 0x0200, 1, 6);
PSP_MAIN_THREAD_PARAMS(45, 256, PSP_THREAD_ATTR_USER);
PSP_HEAP_SIZE_MAX();

extern t_conf config;

static int power_callback(int arg1, int powerInfo, void *arg)
{
#ifdef ENABLE_TTF
	if ((powerInfo & (PSP_POWER_CB_POWER_SWITCH | PSP_POWER_CB_STANDBY)) > 0) {
		if (config.usettf && !config.ttf_load_to_memory) {
			sceKernelDelayThread(500000);
			disp_assign_book_font();
		}
	} else if ((powerInfo & PSP_POWER_CB_RESUME_COMPLETE) > 0) {
		if (config.usettf && !config.ttf_load_to_memory) {
			disp_load_zipped_truetype_book_font(config.ettfarch,
												config.cttfarch,
												config.ettfpath,
												config.cttfpath,
												config.bookfontsize);
		}
	}
#endif
#ifdef ENABLE_MUSIC
	if ((powerInfo & (PSP_POWER_CB_POWER_SWITCH | PSP_POWER_CB_STANDBY)) > 0) {
		mp3_powerdown();
		fat_powerdown();
	} else if ((powerInfo & PSP_POWER_CB_RESUME_COMPLETE) > 0) {
		sceKernelDelayThread(1000000);
		fat_powerup();
		mp3_powerup();
	}
#endif
	scene_power_save(true);
	return 0;
}

static int exit_callback(int arg1, int arg2, void *arg)
{
	extern bool xreader_scene_inited;

	while (xreader_scene_inited == false) {
		sceKernelDelayThread(1);
	}
#ifdef ENABLE_MUSIC
	mp3_stop();
#endif
	scene_exit();
	sceKernelExitGame();

	return 0;
}

static int CallbackThread(unsigned int args, void *argp)
{
	int cbid;

	cbid = sceKernelCreateCallback("Exit Callback", exit_callback, NULL);
	sceKernelRegisterExitCallback(cbid);
	cbid = sceKernelCreateCallback("Power Callback", power_callback, NULL);
	scePowerRegisterCallback(0, cbid);

	sceKernelSleepThreadCB();

	return 0;
}

/* Sets up the callback thread and returns its thread id */
static int SetupCallbacks(void)
{
	int thid = sceKernelCreateThread("Callback Thread", CallbackThread, 0x11,
									 0x3F40,
									 PSP_THREAD_ATTR_USER, 0);

	if (thid >= 0) {
		sceKernelStartThread(thid, 0, 0);
	}
	return thid;
}

static int main_thread(unsigned int args, void *argp)
{
	scene_init();
	return 0;
}

int main(int argc, char *argv[])
{
	SetupCallbacks();
#ifdef ENABLE_PMPAVC
	avc_init();
#endif

	int thid = sceKernelCreateThread("User Thread", main_thread, 45, 0x40000,
									 PSP_THREAD_ATTR_USER, NULL);

	if (thid < 0)
		sceKernelSleepThread();
	sceKernelStartThread(thid, 0, NULL);

	sceKernelSleepThread();
	return 0;
}
