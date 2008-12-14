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

#ifndef MUSICTAG_H
#define MUSICTAG_H

#include "conf.h"

enum
{
	NONE,
	ID3V1,
	ID3V2,
	APETAG
};

struct MusicTag
{
	int type;
	t_conf_encode encode;

	char title[512];
	char author[512];
	char copyright[512];
	char comment[512];
	char album[512];
	int year;
	int track;
	char genre[32];
};

#endif
