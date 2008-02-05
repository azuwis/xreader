/* vim:set ts=4 sw=4 cindent ignorecase enc=gbk: */

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

#ifdef HOMEBREW_2XX
PSP_MODULE_INFO("XREADER", 0x0000, 1, 6);
PSP_MAIN_THREAD_PARAMS(45, 256, PSP_THREAD_ATTR_USER);
PSP_HEAP_SIZE_MAX();
#else
PSP_MODULE_INFO("XREADER", 0x1000, 1, 0);
PSP_MAIN_THREAD_PARAMS(45, 256, 0);

static void exception_handler(PspDebugRegBlock *regs)
{
	/* Do normal initial dump, setup screen etc */
	pspDebugScreenInit();

	/* I always felt BSODs were more interesting that white on black */
	pspDebugScreenSetBackColor(0x00FF0000);
	pspDebugScreenSetTextColor(0xFFFFFFFF);
	pspDebugScreenClear();

	pspDebugScreenPrintf("The application has just crashed\n");
	pspDebugScreenPrintf("Please contact aeolusc @ pspchina.com or post comments at www.pspsp.org\n\n");
	pspDebugScreenPrintf("Exception Details:\n");
	pspDebugDumpException(regs);
	pspDebugScreenPrintf("\nThe offending routine may be identified with:\n\n"
		"\tpsp-addr2line -e target.elf -f -C 0x%x 0x%x 0x%x\n",
		(unsigned int)regs->epc, (unsigned int)regs->badvaddr, (unsigned int)regs->r[31]);
	pspDebugScreenPrintf("\nThanks for support!\n");
}

__attribute__ ((constructor))
void loaderInit()
{
	pspKernelSetKernelPC();
	pspSdkInstallNoPlainModuleCheckPatch();
	pspDebugInstallErrorHandler(exception_handler);
/*	pspKernelSetKernelPC();
	pspSdkInstallNoDeviceCheckPatch();
	pspSdkInstallNoPlainModuleCheckPatch();
	pspSdkInstallKernelLoadModulePatch();
	pspDebugInstallKprintfHandler(NULL);
	pspDebugInstallErrorHandler(exception_handler);*/
}
#endif

static int power_callback(int arg1, int powerInfo, void * arg)
{
#ifdef ENABLE_MUSIC
	if((powerInfo & (PSP_POWER_CB_POWER_SWITCH | PSP_POWER_CB_STANDBY)) > 0)
	{
		mp3_powerdown();
		fat_powerdown();
	}
	else if((powerInfo & PSP_POWER_CB_RESUME_COMPLETE) > 0)
	{
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
	int thid = sceKernelCreateThread("Callback Thread", CallbackThread, 0x11, 0xFA0, PSP_THREAD_ATTR_USER, 0);
	if(thid >= 0)
	{
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

	int thid = sceKernelCreateThread("User Thread", main_thread, 45, 0x40000, PSP_THREAD_ATTR_USER, NULL);
	if(thid < 0)
		sceKernelSleepThread();
	sceKernelStartThread(thid, 0, NULL);

	sceKernelSleepThread();
	return 0;
}
