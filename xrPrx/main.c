#include <pspkernel.h>
#include <pspdebug.h>
#include <pspsdk.h>
#include <psputilsforkernel.h>
#include <pspdisplay_kernel.h>
#include <psploadexec_kernel.h>
#include <pspsysmem_kernel.h>
#include <string.h>
#include "xrPrx.h"

typedef unsigned short bool;

enum
{
	false,
	true
};

PSP_MODULE_INFO("xrPrx", 0x1007, 1, 1);
PSP_MAIN_THREAD_ATTR(0);

PspDebugErrorHandler curr_handler;
PspDebugRegBlock *exception_regs;

void _pspDebugExceptionHandler(void);
int sceKernelRegisterDefaultExceptionHandler(void *func);
int sceKernelRegisterDefaultExceptionHandler371(void *func);

unsigned long FindProc(const char *szMod, const char *szLib, unsigned long nid)
{
	unsigned int k = pspSdkSetK1(0);
	struct SceLibraryEntryTable *entry;
	SceModule *pMod;
	void *entTab;
	int entLen;

	pMod = sceKernelFindModuleByName(szMod);

	if (!pMod) {
		Kprintf("Cannot find module %s\n", szMod);
		pspSdkSetK1(k);
		return 0;
	}

	int i = 0;

	entTab = pMod->ent_top;
	entLen = pMod->ent_size;

	while (i < entLen) {
		int count;
		int total;
		unsigned int *vars;

		entry = (struct SceLibraryEntryTable *) (entTab + i);

		if (entry->libname && !strcmp(entry->libname, szLib)) {
			total = entry->stubcount + entry->vstubcount;
			vars = entry->entrytable;

			if (entry->stubcount > 0) {
				for (count = 0; count < entry->stubcount; count++) {
					if (vars[count] == nid) {
						pspSdkSetK1(k);
						return vars[count + total];
					}
				}
			}
		}

		i += (entry->len * 4);
	}

	pspSdkSetK1(k);
	return 0;
}

int (*scePowerSetClockFrequency2) (int cpufreq, int ramfreq, int busfreq) = 0;
int (*scePowerIsBatteryCharging) (void) = 0;
void (*scePower_driver_A09FC577) (int) = 0;
void (*scePower_driver_191A3848) (int) = 0;

void xrPlayerSetSpeed(int cpu, int bus)
{
	unsigned int k = pspSdkSetK1(0);
	static bool inited = false;

	if (!inited) {
		scePowerSetClockFrequency2 =
			(void *) FindProc("scePower_Service", "scePower", 0x545A7F3C);
		scePowerIsBatteryCharging =
			(void *) FindProc("scePower_Service", "scePower", 0x1E490401);
		scePower_driver_A09FC577 =
			(void *) FindProc("scePower_Service", "scePower_driver",
							  0xA09FC577);
		scePower_driver_191A3848 =
			(void *) FindProc("scePower_Service", "scePower_driver",
							  0x191A3848);
		inited = true;
	}

	if (scePowerSetClockFrequency2 == 0 || scePowerIsBatteryCharging == 0
		|| scePower_driver_A09FC577 == 0 || scePower_driver_191A3848 == 0) {
		inited = false;
		return;
	}

	scePowerSetClockFrequency2(cpu, cpu, bus);
	if (scePowerIsBatteryCharging() != 0) {
		pspSdkSetK1(k);
		return;
	}
	static int ps1 = 0;

	if (ps1 == 0) {
		scePower_driver_A09FC577(1);
		ps1 = 1;
		pspSdkSetK1(k);
		return;
	}

	static int ps2 = 0;

	if (ps2 == 0) {
		ps1 = 0;
		ps2 = 1;
		pspSdkSetK1(k);
		return;
	}
	scePower_driver_191A3848(0);
	ps1 = 0;
	ps2 = 0;
	pspSdkSetK1(k);
}

/*
void xrPlayerSetSpeed(int cpu, int bus)
{
	unsigned int k = pspSdkSetK1(0);
	static bool inited = false;

	if(!inited)
	{
		scePowerSetClockFrequency2 = (void *)FindProc("scePower_Service", "scePower", 0x545A7F3C);
		inited = true;
	}

	if(scePowerSetClockFrequency2)
		scePowerSetClockFrequency2(cpu, cpu, bus);
	pspSdkSetK1(k);
}
*/

int xrGetBrightness(void)
{
	unsigned int k = pspSdkSetK1(0);

	int level, unk1 = 0;

	sceDisplayGetBrightness(&level, &unk1);

	pspSdkSetK1(k);

	return level;
}

void xrSetBrightness(int bright)
{
	unsigned int k = pspSdkSetK1(0);

	sceDisplaySetBrightness(bright, 0);

	pspSdkSetK1(k);
}

int xrKernelGetModel()
{
	unsigned int k = pspSdkSetK1(0);

	int ret;

	ret = sceKernelGetModel();

	pspSdkSetK1(k);

	return ret;
}

int xrKernelLoadExecVSHMsX(int method, const char *exec,
						   struct SceKernelLoadExecVSHParam *param)
{
	unsigned int k = pspSdkSetK1(0);
	int r = 0;

	if (!exec || !param)
		goto ret;

	switch (method) {
		case 4:
			r = sceKernelLoadExecVSHMs4(exec, param);
			break;
		case 3:
			r = sceKernelLoadExecVSHMs3(exec, param);
			break;
		case 2:
			r = sceKernelLoadExecVSHMs2(exec, param);
			break;
		case 1:
			r = sceKernelLoadExecVSHMs1(exec, param);
			break;
		default:
			r = -1;
			break;
	}

  ret:
	pspSdkSetK1(k);

	return r;
}

/* Entry point */
int module_start(SceSize args, void *argp)
{
	if(args != 8) return -1;
	curr_handler = (PspDebugErrorHandler)((int *)argp)[0];
	exception_regs = (PspDebugRegBlock *)((int *)argp)[1];
	if(!curr_handler || !exception_regs) return -1;

	if(sceKernelDevkitVersion() < 0x03070110)
		return sceKernelRegisterDefaultExceptionHandler((void *)_pspDebugExceptionHandler);
	else
		return sceKernelRegisterDefaultExceptionHandler371((void *)_pspDebugExceptionHandler);
}

int module_stop(SceSize args, void *argp)
{
	return 0;
}
