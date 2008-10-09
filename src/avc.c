/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include "config.h"

#ifdef ENABLE_PMPAVC

#include <psputility_avmodules.h>
#include <pspdisplay.h>
#include <pspge.h>
#include <psppower.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "display.h"
#include "bg.h"
#include "avc.h"

static int pmp_inited = 0;
static void *m1 = NULL, *m2 = NULL;
char *avc_static_init();
void *valloc(size_t size);
void vfree(void *ptr);
char *gu_font_init();
char *gu_font_load(char *name);
void gu_font_close();

extern bool avc_init(void)
{
	if (pmp_inited)
		return true;

	m1 = valloc(4 * 512 * 272);
	m2 = valloc(4 * 512 * 272);

	av_register_all();
	sceUtilityLoadAvModule(PSP_AV_MODULE_AVCODEC);
	sceUtilityLoadAvModule(PSP_AV_MODULE_ATRAC3PLUS);
	sceUtilityLoadAvModule(PSP_AV_MODULE_MPEGBASE);
	sceUtilityLoadAvModule(PSP_AV_MODULE_SASCORE);

	gu_font_init();
	char ftn[PATH_MAX];

	getcwd(ftn, PATH_MAX);
	STRCAT_S(ftn, "/font10.f");
	gu_font_load(ftn);

	pmp_inited = 1;
	return true;
}

extern void avc_free(void)
{
	gu_font_close();
	if (m1 != NULL) {
		vfree(m1);
	}
	if (m2 != NULL) {
		vfree(m2);
	}
	sceUtilityUnloadAvModule(PSP_AV_MODULE_ATRAC3PLUS);
	sceUtilityUnloadAvModule(PSP_AV_MODULE_MPEGBASE);
	sceUtilityUnloadAvModule(PSP_AV_MODULE_AVCODEC);
	sceUtilityUnloadAvModule(PSP_AV_MODULE_SASCORE);
}

extern void avc_start(void)
{
	bg_cache();
	void *vram = (void *) (0x40000000 | (u32) sceGeEdramGetAddr());

	sceDisplaySetMode(0, 480, 272);
	sceDisplaySetFrameBuf(vram, 512, PSP_DISPLAY_PIXEL_FORMAT_8888, 1);
	memset(vram, 0, 4 * 512 * 272);
	scePowerLock(0);
}

extern void avc_end(void)
{
	scePowerUnlock(0);
	disp_init();
	bg_restore();
}

#endif
