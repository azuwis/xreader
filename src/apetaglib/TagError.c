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

#include <stdio.h>
#include "APETag.h"
#ifdef DMALLOC
#include "dmalloc.h"
#endif

char *APETag_errlist[] = {
	"OK",
	"Can't Open File",
	"Error File Format",
	"Unsupport Tag Version",
	"Bad Argument Error",
	"Function Not Implemented",
	"Memory Not Enough",
};

int APETag_nerr = (sizeof(APETag_errlist) / sizeof(APETag_errlist[0]));

const char *APETag_strerror(int errno)
{
	static char emsg[1024];

	if (errno < 0 || errno >= APETag_nerr)
		snprintf(emsg, sizeof(emsg), "Error %d occurred.", errno);
	else
		snprintf(emsg, sizeof(emsg), "%s", APETag_errlist[errno]);

	return emsg;
}
