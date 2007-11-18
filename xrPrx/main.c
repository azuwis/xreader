#include <pspkernel.h>
#include <pspsdk.h>
#include <psputilsforkernel.h>
#include <string.h>
#include "xrPrx.h"

typedef unsigned short bool;

enum {
	false,
	true
};

PSP_MODULE_INFO("xrPrx", PSP_MODULE_KERNEL, 1, 1);
PSP_MAIN_THREAD_ATTR(0);

int (* scePowerSetClockFrequency2)(int cpufreq, int ramfreq, int busfreq);

unsigned long FindProc(const char* szMod, const char* szLib, unsigned long nid)
{
	unsigned int k = pspSdkSetK1(0);
	struct SceLibraryEntryTable *entry;
	SceModule *pMod;
	void *entTab;
	int entLen;

	pMod = sceKernelFindModuleByName(szMod);

	if (!pMod)
	{
		Kprintf("Cannot find module %s\n", szMod);
		pspSdkSetK1(k);
		return 0;
	}
	
	int i = 0;

	entTab = pMod->ent_top;
	entLen = pMod->ent_size;

	while(i < entLen)
    {
		int count;
		int total;
		unsigned int *vars;

		entry = (struct SceLibraryEntryTable *) (entTab + i);

        if(entry->libname && !strcmp(entry->libname, szLib))
		{
			total = entry->stubcount + entry->vstubcount;
			vars = entry->entrytable;

			if(entry->stubcount > 0)
			{
				for(count = 0; count < entry->stubcount; count++)
				{
					if (vars[count] == nid) {
						pspSdkSetK1(k);
						return vars[count+total];					
					}
				}
			}
		}

		i += (entry->len * 4);
	}

	pspSdkSetK1(k);
	return 0;
}

void xrPlayerSetSpeed(int cpu, int bus)
{
	unsigned int k = pspSdkSetK1(0);
	static bool inited = false;

	if(!inited)
	{
		scePowerSetClockFrequency2 = (void *)FindProc("scePower_Service", "scePower", 0x545A7F3C);
		inited = true;
	}

	scePowerSetClockFrequency2(cpu, cpu, bus);
	pspSdkSetK1(k);
}


/* Entry point */
int module_start(SceSize args, void *argp)
{
	return 0;
}

/* Module stop entry */
int module_stop(SceSize args, void *argp)
{
	return 0;
}
