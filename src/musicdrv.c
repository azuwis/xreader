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
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "musicdrv.h"

static struct music_ops *music_drivers = NULL;
static struct music_ops *cur_musicdrv = NULL;

bool show_encoder_msg = false;

int register_musicdrv(struct music_ops *drv)
{
	struct music_ops **tmp;

	if (drv == NULL) {
		return -EINVAL;
	}
	if (drv->next != NULL) {
		return -EBUSY;
	}
	tmp = &music_drivers;
	while (*tmp) {
		if (strcmp((*tmp)->name, drv->name) == 0)
			return -EBUSY;
		tmp = &(*tmp)->next;
	}
	*tmp = drv;
	return 0;
}

int unregister_musicdrv(struct music_ops *drv)
{
	struct music_ops **tmp;

	tmp = &music_drivers;
	while (*tmp) {
		if (drv == *tmp) {
			*tmp = drv->next;
			drv->next = NULL;
			return 0;
		}
		tmp = &(*tmp)->next;
	}
	return -EINVAL;
}

int musicdrv_maxindex(void)
{
	struct music_ops *tmp;
	int count;

	count = 0;
	for (tmp = music_drivers; tmp; tmp = tmp->next)
		count++;

	return count;
}

struct music_ops *get_musicdrv(const char *name)
{
	struct music_ops *tmp;

	if (name == NULL)
		return cur_musicdrv;

	for (tmp = music_drivers; tmp && strcmp(name, tmp->name); tmp = tmp->next);

	return tmp;
}

struct music_ops *set_musicdrv(const char *name)
{
	cur_musicdrv = get_musicdrv(name);

	return cur_musicdrv;
}

int musicdrv_set_opt(const char *key, const char *value)
{
	if (key == NULL || value == NULL)
		return -EINVAL;
	if (cur_musicdrv == NULL)
		return -EBUSY;
	if (cur_musicdrv->set_opt)
		return cur_musicdrv->set_opt(key, value);
	else
		return -ENOSYS;
}

int musicdrv_load(const char *spath, const char *lpath)
{
	if (spath == NULL || lpath == NULL)
		return -EINVAL;
	if (cur_musicdrv == NULL)
		return -EBUSY;
	if (cur_musicdrv->load)
		return cur_musicdrv->load(spath, lpath);
	else
		return -ENOSYS;
}

int musicdrv_play(void)
{
	if (cur_musicdrv == NULL)
		return -EBUSY;
	if (cur_musicdrv->play)
		return cur_musicdrv->play();
	else
		return -ENOSYS;
}

int musicdrv_pause(void)
{
	if (cur_musicdrv == NULL)
		return -EBUSY;
	if (cur_musicdrv->pause)
		return cur_musicdrv->pause();
	else
		return -ENOSYS;
}

int musicdrv_end(void)
{
	if (cur_musicdrv == NULL)
		return -EBUSY;
	if (cur_musicdrv->end)
		return cur_musicdrv->end();
	else
		return -ENOSYS;
}

int musicdrv_fforward(int second)
{
	if (cur_musicdrv == NULL)
		return -EBUSY;
	if (cur_musicdrv->fforward)
		return cur_musicdrv->fforward(second);
	else
		return -ENOSYS;
}

int musicdrv_fbackward(int second)
{
	if (cur_musicdrv == NULL)
		return -EBUSY;
	if (cur_musicdrv->fbackward)
		return cur_musicdrv->fbackward(second);
	else
		return -ENOSYS;
}

int musicdrv_get_status(void)
{
	if (cur_musicdrv == NULL)
		return -EBUSY;
	if (cur_musicdrv->get_status)
		return cur_musicdrv->get_status();
	else
		return -ENOSYS;
}

int musicdrv_suspend(void)
{
	if (cur_musicdrv == NULL)
		return -EBUSY;
	if (cur_musicdrv->suspend)
		return cur_musicdrv->suspend();
	else
		return -ENOSYS;
}

int musicdrv_resume(const char *spath, const char *lpath)
{
	if (cur_musicdrv == NULL)
		return -EBUSY;
	if (cur_musicdrv->resume)
		return cur_musicdrv->resume(spath, lpath);
	else
		return -ENOSYS;
}

int musicdrv_get_info(struct music_info *info)
{
	if (info == NULL)
		return -EINVAL;
	if (cur_musicdrv == NULL)
		return -EBUSY;
	if (cur_musicdrv->get_info)
		return cur_musicdrv->get_info(info);
	else
		return -ENOSYS;
}

int free_musicinfo(struct music_info *info)
{
	return 0;
}

bool opt_is_on(const char *str)
{
	size_t slen;

	if (str == NULL)
		return false;

	slen = strlen(str);

#define CHECK_TAIL(tailstr) \
	do { \
		if (slen > (sizeof(tailstr) - 1) \
				&& !strncmp(&str[slen- (sizeof(tailstr) - 1)], tailstr, (sizeof(tailstr) - 1))) \
		return true; }\
	while ( 0 )

	CHECK_TAIL("=y");
	CHECK_TAIL("=yes");
	CHECK_TAIL("=1");
	CHECK_TAIL("=on");

#undef CHECK_TAIL

	return false;
}
