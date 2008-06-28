#include <pspkernel.h>
#include <pspdebug.h>
#include <pspsdk.h>
#include <psputilsforkernel.h>
#include <pspdisplay_kernel.h>
#include <psploadexec_kernel.h>
#include <pspsysmem_kernel.h>
#include <pspimpose_driver.h>
#include <string.h>
#include "config.h"
#include "xrPrx.h"

typedef unsigned short bool;

enum
{
	false = 0,
	true = 1
};

// null the 6 MSB bits (the opcode bits) 
// and divide x by 4 to get the J-Instr parameter. 
#define MIPS_J_ADDRESS(x) (((u32)((x)) & 0x3FFFFFFF) >> 2)

#define NOP       (0x00000000)
#define JAL_TO(x) (0x0E000000 | MIPS_J_ADDRESS(x))
#define J_TO(x)   (0x08000000 | MIPS_J_ADDRESS(x))
#define LUI(x,y)  (0x3C000000 | ((x & 0x1F) << 0x10) | (y & 0xFFFF))

PSP_MODULE_INFO("xrPrx", 0x1007, 1, 1);
PSP_MAIN_THREAD_ATTR(0);

PspDebugErrorHandler curr_handler;
PspDebugRegBlock *exception_regs;

void _pspDebugExceptionHandler(void);
int sceKernelRegisterDefaultExceptionHandler(void *func);
int sceKernelRegisterDefaultExceptionHandler371(void *func);

static void (*orig_sceDisplaySetBrightess) (int, int) = NULL;
static void (*orig_sceDisplay_driver_E55F0D50) (int) = NULL;
static int max_brightness = 100;
static void new_sceDisplaySetBrightness(int level, int unk1);

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
		u32 *vars;

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

static void new_sceDisplay_driver_E55F0D50(int bright)
{
	bright = xrGetBrightness() + 10;
	if (bright <= max_brightness && bright < 101 && bright >= 28) {
		xrSetBrightness(bright);
	} else {
		xrSetBrightness(28);
	}
}

int xrSetBrightness(int bright)
{
	unsigned int k = pspSdkSetK1(0);

	sceDisplaySetBrightness(bright, 0);
	sceImposeSetParam(PSP_IMPOSE_BACKLIGHT_BRIGHTNESS, bright);

	pspSdkSetK1(k);

	return bright;
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

extern int xrSetMaxBrightness(int newlevel)
{
	if (newlevel >= 28)
		max_brightness = newlevel;
	return max_brightness;
}

static void new_sceDisplaySetBrightness(int level, int unk1)
{
	unsigned int k = pspSdkSetK1(0);

	int t = level;

	if (t >= max_brightness)
		t = max_brightness;

	if (orig_sceDisplaySetBrightess != NULL)
		(*orig_sceDisplaySetBrightess) (t, unk1);

	pspSdkSetK1(k);
}

struct PspModuleImport
{
	const char *name;
	unsigned short version;
	unsigned short attribute;
	unsigned char entLen;
	unsigned char varCount;
	unsigned short funcCount;
	unsigned int *fnids;
	unsigned int *funcs;
	unsigned int *vnids;
	unsigned int *vars;
} __attribute__ ((packed));

static struct PspModuleImport *_libsFindImport(SceUID uid, const char *library)
{
	SceModule *pMod;
	void *stubTab;
	int stubLen;

	pMod = sceKernelFindModuleByUID(uid);
	if (pMod != NULL) {
		int i = 0;

		stubTab = pMod->stub_top;
		stubLen = pMod->stub_size;
		while (i < stubLen) {
			struct PspModuleImport *pImp =
				(struct PspModuleImport *) (stubTab + i);

			if ((pImp->name) && (strcmp(pImp->name, library) == 0)) {
				return pImp;
			}

			i += (pImp->entLen * 4);
		}
	}

	return NULL;
}

unsigned int libsFindImportAddrByNid(SceUID uid, const char *library,
									 unsigned int nid)
{
	struct PspModuleImport *pImp;

	pImp = _libsFindImport(uid, library);
	if (pImp) {
		int i;

		for (i = 0; i < pImp->funcCount; i++) {
			if (pImp->fnids[i] == nid) {
				return (unsigned int) &pImp->funcs[i * 2];
			}
		}
	}

	return 0;
}

static void patch_setbrightness(void)
{
	int intc;

	intc = pspSdkDisableInterrupts();

	if (sceKernelDevkitVersion() < 0x03070110)
		orig_sceDisplaySetBrightess = (void (*)(int, int))
			FindProc("sceDisplay_Service", "sceDisplay_driver", 0x9E3C6DC6);
	else if (sceKernelDevkitVersion() == 0x03070110)
		orig_sceDisplaySetBrightess = (void (*)(int, int))
			FindProc("sceDisplay_Service", "sceDisplay_driver", 0x776ADFDB);
	else
		orig_sceDisplaySetBrightess = (void (*)(int, int))
			FindProc("sceDisplay_Service", "sceDisplay_driver", 0x267BF9F7);

	orig_sceDisplay_driver_E55F0D50 = (void (*)(int))
		FindProc("sceDisplay_Service", "sceDisplay_driver", 0xE55F0D50);

	if (orig_sceDisplaySetBrightess != NULL) {
		u32 *t = NULL;
		SceModule *pMod = sceKernelFindModuleByName("xrPrx");

		if (pMod != NULL) {
			if (sceKernelDevkitVersion() <= 0x03070110)
				t = (u32 *) libsFindImportAddrByNid(pMod->modid,
													"sceDisplay_driver",
													0x9E3C6DC6);
			else if (sceKernelDevkitVersion() > 0x03070110)
				t = (u32 *) libsFindImportAddrByNid(pMod->modid,
													"sceDisplay_driver",
													0x267BF9F7);
		}
		if (t != NULL) {
			*t = J_TO(&new_sceDisplaySetBrightness);
		}

		pMod = sceKernelFindModuleByName("sceImpose_Driver");
		if (pMod != NULL) {
			t = (u32 *) libsFindImportAddrByNid(pMod->modid,
												"sceDisplay_driver",
												0xE55F0D50);
		}
		if (t != NULL) {
			*t = J_TO(&new_sceDisplay_driver_E55F0D50);
		}
		sceKernelDcacheWritebackInvalidateRange(t, sizeof(*t));
		sceKernelIcacheInvalidateRange(t, sizeof(*t));
	}

	pspSdkEnableInterrupts(intc);
}

/* Entry point */
int module_start(SceSize args, void *argp)
{
	int ret = 0;

#ifndef _DEBUG
	if (args != 8)
		return -1;
	curr_handler = (PspDebugErrorHandler) ((int *) argp)[0];
	exception_regs = (PspDebugRegBlock *) ((int *) argp)[1];
	if (!curr_handler || !exception_regs)
		return -1;

	if (sceKernelDevkitVersion() < 0x03070110)
		ret = sceKernelRegisterDefaultExceptionHandler((void *)
													   _pspDebugExceptionHandler);
	else
		ret = sceKernelRegisterDefaultExceptionHandler371((void *)
														  _pspDebugExceptionHandler);
#endif

	if (sceKernelDevkitVersion() >= 0x03070110)
		patch_setbrightness();

	return ret;
}

int module_stop(SceSize args, void *argp)
{
	return 0;
}
