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

	return ret;
}

int module_stop(SceSize args, void *argp)
{
	return 0;
}
