#include <pspsdk.h>
#include <pspkernel.h>
#include <pspdebug.h>
#include <pspctrl.h>
#include <psploadexec_kernel.h>
#include <systemctrl.h>
#include <systemctrl_se.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include "kubridge.h"
#include "xr_rdriver/xr_rdriver.h"
#include "m33boot.h"
#include "dbg.h"
#include "xrPrx/xrPrx.h"

static struct SceKernelLoadExecVSHParam param;
static int apitype = 0;
static const char *program = NULL;
static char *mode = NULL;

static int lanuch_api(void)
{
	memset(&param, 0, sizeof(param));
	param.size = sizeof(param);
	param.args = strlen(program) + 1;
	param.argp = (char *) program;
	param.key = mode;

	dbg_printf(d, "%s: program %s", __func__, program);

	return sctrlKernelLoadExecVSHWithApitype(apitype, program, &param);
}

extern int run_homebrew(const char *path)
{
	apitype = 0x141;
	program = path;
	mode = "game";

	return lanuch_api();
}

extern int run_umd(void)
{
	apitype = 0x120;
	program = "disc0:/PSP_GAME/SYSDIR/EBOOT.BIN";
	mode = "game";

	return lanuch_api();
}

extern int run_iso(const char *iso)
{
	SEConfig config;

	apitype = 0x120;
	program = "disc0:/PSP_GAME/SYSDIR/EBOOT.BIN";
	mode = "game";

	SetUmdFile((char *) iso);
	int r = sctrlSEGetConfigEx(&config, sizeof(config));

	dbg_printf(d, "%s: sctrlSEGetConfigEx %08x", __func__, r);

	if (config.umdmode == MODE_MARCH33) {
		SetConfFile(1);
	} else if (config.umdmode == MODE_NP9660) {
		SetConfFile(2);
	} else {
		// Assume this is is normal umd mode, as isofs will be deleted soon
		SetConfFile(0);
	}

	return lanuch_api();
}

int run_psx_game(const char *path)
{
	apitype = 0x143;
	program = path;
	mode = "pops";

	return lanuch_api();
}
