#ifndef XR_PRX
#define XR_PRX

#include <psploadexec_kernel.h>

unsigned long FindProc(const char *szMod, const char *szLib, unsigned long nid);
void xrPlayerSetSpeed(int cpu, int bus);
int xrGetBrightness(void);
void xrSetBrightness(int bright);
int xrKernelLoadExecVSHMsX(int method, const char *exec,
						   struct SceKernelLoadExecVSHParam *param);

#endif
