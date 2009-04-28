/*
 * This file is part of xReader.
 *
 * Copyright (C) 2008 hrimfaxi (outmatch@gmail.com)
 *
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <pspsdk.h>
#include <pspkernel.h>
#include <pspaudio.h>
#include <pspaudiocodec.h>
#include "config.h"
#include "xrhal.h"
#include "mediaengine.h"
#include "common/datatype.h"

#ifdef ENABLE_MUSIC

static bool me_prx_loaded = false;

int load_me_prx(void)
{
	int result;

	if (me_prx_loaded)
		return 0;

#if (PSP_FW_VERSION >= 300)
	result = xrUtilityLoadAvModule(PSP_AV_MODULE_AVCODEC);
#else
	result =
		pspSdkLoadStartModule("flash0:/kd/avcodec.prx",
							  PSP_MEMORY_PARTITION_KERNEL);
#endif

	if (result < 0)
		return -1;

#if (PSP_FW_VERSION >= 300)
	result = xrUtilityLoadAvModule(PSP_AV_MODULE_ATRAC3PLUS);
	if (result < 0)
		return -2;
#endif

	me_prx_loaded = true;

	return 0;
}

#endif
