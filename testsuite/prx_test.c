#include <pspdebug.h>
#include <pspkernel.h>
#include <pspsdk.h>
#include <psprtc.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include "kubridge.h"
#include "config.h"
#include "win.h"
#include "common/datatype.h"
#include "common/utils.h"
#include "fs.h"
#include "rar_speed_test.h"
#include "dbg.h"
#include "archive.h"
#include "buffer.h"
#include "freq_lock.h"
#include "power.h"
#include "image.h"
#include "scene.h"
#include "commons.h"
#include "prx_test.h"
#include "testprx/testprx.h"

int prx_test(void)
{
	int ret;
	char cwd[PATH_MAX];
	struct TestStruct test;

	memset(&test, 0, sizeof(test));

	getcwd(cwd, PATH_MAX);
	STRCAT_S(cwd, "/testprx.prx");

	dbg_printf(d, "cwd is %s", cwd);
#if 1
	SceUID mod = sceKernelLoadModuleMs(cwd, 0, NULL);
#else
	SceUID mod = pspSdkLoadStartModule(cwd, PSP_MEMORY_PARTITION_USER);
#endif

	if (mod >= 0) {
		dbg_printf(d, "sceKernelLoadModuleMs ok...");

		mod = sceKernelStartModule(mod, 0, NULL, NULL, NULL);

		if (mod < 0) {
			dbg_printf(d, 
					"%s: sceKernelStartModule failed %08x", __func__, mod);
		} else {
			do {
				ret = test_get_version();
				dbg_printf(d, "get version 0x%08x", ret);
				test_modify_struct(&test);
				dbg_printf(d, "test cnt is %d now.", test.cnt);
				sceKernelDelayThread(1000000);
			} while ( 1 );
		}
	} else {
		if (mod == SCE_KERNEL_ERROR_EXCLUSIVE_LOAD) {
		} else {
			dbg_printf(d, "%s: kuKernelLoadModule failed %08x", __func__, mod);
		}
	}
	
	return 0;
}
