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

#include "config.h"

#include <stdlib.h>
#include <string.h>
#include <pspkernel.h>
#include "common/utils.h"
#include "charsets.h"
#include "fat.h"
#include <stdio.h>
#include "dbg.h"
#include "xrhal.h"
#ifdef DMALLOC
#include "dmalloc.h"
#endif

static int fatfd = -1;
static t_fat_dbr dbr;
static t_fat_mbr mbr;
static u32 *fat_table = NULL;
static u64 dbr_pos = 0;
static u64 root_pos = 0;
static u64 data_pos = 0;
static u64 bytes_per_clus = 0;
static u32 loadcount = 0;
static u32 clus_max = 0;
static enum
{
	fat12,
	fat16,
	fat32
} fat_type = fat16;
static SceUID fat_sema = -1;

void fat_powerdown(void)
{
	fat_lock();
	xrIoClose(fatfd);
	fatfd = -1;
}

void fat_powerup(void)
{
	fatfd = xrIoOpen("msstor:", PSP_O_RDONLY, 0777);
	fat_unlock();
}

void fat_lock(void)
{
	xrKernelWaitSema(fat_sema, 1, NULL);
}

void fat_unlock(void)
{
	xrKernelSignalSema(fat_sema, 1);
}

extern bool fat_init(void)
{
	fat_sema = xrKernelCreateSema("FAT Sema", 0, 1, 1, NULL);
	if (fat_sema < 0)
		return false;
	fat_lock();
	fatfd = xrIoOpen("msstor:", PSP_O_RDONLY, 0777);
	if (fatfd < 0) {
		fat_unlock();
		return false;
	}
	if (xrIoRead(fatfd, &mbr, sizeof(mbr)) != sizeof(mbr)) {
		xrIoClose(fatfd);
		fatfd = -1;
		fat_unlock();
		return false;
	}
	dbr_pos = mbr.dpt[0].start_sec * 0x200;
	if (xrIoLseek(fatfd, dbr_pos, PSP_SEEK_SET) != dbr_pos
		|| xrIoRead(fatfd, &dbr, sizeof(dbr)) < sizeof(dbr)) {
		dbr_pos = 0;
		if (xrIoLseek(fatfd, dbr_pos, PSP_SEEK_SET) != dbr_pos
			|| xrIoRead(fatfd, &dbr, sizeof(dbr)) < sizeof(dbr)) {
			xrIoClose(fatfd);
			fatfd = -1;
			fat_unlock();
			return false;
		}
	}

	u64 total_sec = (dbr.total_sec == 0) ? dbr.big_total_sec : dbr.total_sec;
	u64 fat_sec =
		(dbr.sec_per_fat == 0) ? dbr.ufat.fat32.sec_per_fat : dbr.sec_per_fat;
	u64 root_sec =
		(dbr.root_entry * 32 + dbr.bytes_per_sec - 1) / dbr.bytes_per_sec;
	u64 data_sec =
		total_sec - dbr.reserved_sec - (dbr.num_fats * fat_sec) - root_sec;
	u64 data_clus = data_sec / dbr.sec_per_clus;

	if (data_clus < 4085) {
		fat_type = fat12;
		clus_max = 0x0FF0;
	} else if (data_clus < 65525) {
		fat_type = fat16;
		clus_max = 0xFFF0;
	} else {
		fat_type = fat32;
		clus_max = 0x0FFFFFF0;
	}

	bytes_per_clus = 1ull * dbr.sec_per_clus * dbr.bytes_per_sec;
	if (fat_type == fat32) {
		data_pos = 1ull * dbr_pos;
		data_pos +=
			1ull * (dbr.ufat.fat32.sec_per_fat * dbr.num_fats +
					dbr.reserved_sec) * dbr.bytes_per_sec;
		root_pos = data_pos;
		root_pos += 1ull * bytes_per_clus * dbr.ufat.fat32.root_clus;
	} else {
		root_pos = 1ull * dbr_pos;
		root_pos +=
			1ull * (dbr.num_fats * dbr.sec_per_fat +
					dbr.reserved_sec) * dbr.bytes_per_sec;
		data_pos = root_pos;
		data_pos += 1ull * dbr.root_entry * sizeof(t_fat_entry);
	}
	xrIoClose(fatfd);
	fat_unlock();
	return true;
}

static bool convert_table_fat12(void)
{
	u16 *otable = malloc(dbr.sec_per_fat * dbr.bytes_per_sec);

	if (otable == NULL)
		return false;
	memcpy(otable, fat_table, dbr.sec_per_fat * dbr.bytes_per_sec);
	int i, j, entrycount =
		dbr.sec_per_fat * dbr.bytes_per_sec / sizeof(u16) * 4 / 3;
	free(fat_table);
	fat_table = malloc(sizeof(*fat_table) * entrycount);
	if (fat_table == NULL) {
		free(otable);
		return false;
	}
	for (i = 0, j = 0; i < entrycount; i += 4, j += 3) {
		fat_table[i] = otable[j] & 0x0FFF;
		fat_table[i + 1] =
			((otable[j] >> 12) & 0x000F) | ((otable[j + 1] & 0x00FF) << 4);
		fat_table[i + 2] =
			((otable[j + 1] >> 8) & 0x00FF) | ((otable[j + 2] & 0x000F)
											   << 8);
		fat_table[i + 3] = otable[j + 2] >> 4;
	}
	free(otable);
	return true;
}

static bool convert_table_fat16(void)
{
	u16 *otable = malloc(dbr.sec_per_fat * dbr.bytes_per_sec);

	if (otable == NULL)
		return false;
	memcpy(otable, fat_table, dbr.sec_per_fat * dbr.bytes_per_sec);
	int i, entrycount = dbr.sec_per_fat * dbr.bytes_per_sec / sizeof(u16);

	free(fat_table);
	fat_table = malloc(sizeof(*fat_table) * entrycount);
	if (fat_table == NULL) {
		free(otable);
		return false;
	}
	for (i = 0; i < entrycount; i++)
		fat_table[i] = otable[i];
	free(otable);
	return true;
}

static bool fat_load_table(void)
{
	if (loadcount > 0) {
		loadcount++;
		return true;
	}
	fatfd = xrIoOpen("msstor:", PSP_O_RDONLY, 0777);
	if (fatfd < 0)
		return false;

	u32 fat_table_size =
		((fat_type ==
		  fat32) ? dbr.ufat.fat32.sec_per_fat : dbr.sec_per_fat) *
		dbr.bytes_per_sec;
	if (xrIoLseek
		(fatfd, dbr_pos + dbr.reserved_sec * dbr.bytes_per_sec,
		 PSP_SEEK_SET) != dbr_pos + dbr.reserved_sec * dbr.bytes_per_sec
		|| (fat_table = malloc(fat_table_size)) == NULL) {
		xrIoClose(fatfd);
		fatfd = -1;
		return false;
	}
	if ((xrIoRead(fatfd, fat_table, fat_table_size) != fat_table_size)
		|| (fat_type == fat12 && !convert_table_fat12())
		|| (fat_type == fat16 && !convert_table_fat16())) {
		xrIoClose(fatfd);
		fatfd = -1;
		free(fat_table);
		fat_table = NULL;
		return false;
	}
	loadcount = 1;
	return true;
}

static void fat_free_table(void)
{
	if (loadcount > 0) {
		loadcount--;
		if (loadcount > 0)
			return;
		if (fat_table != NULL) {
			free(fat_table);
			fat_table = NULL;
		}
		xrIoClose(fatfd);
	}
}

static byte fat_calc_chksum(p_fat_entry info)
{
	byte *n = (byte *) & info->norm.filename[0];
	byte chksum = 0;

	// Fix upset warning in psp-gcc 4.3.3
#if 0
	u32 i;

	for (i = 0; i < 11; i++)
		chksum = ((chksum & 1) ? 0x80 : 0) + (chksum >> 1) + n[i];
#endif

	chksum = ((chksum & 1) ? 0x80 : 0) + (chksum >> 1) + n[0];
	chksum = ((chksum & 1) ? 0x80 : 0) + (chksum >> 1) + n[1];
	chksum = ((chksum & 1) ? 0x80 : 0) + (chksum >> 1) + n[2];
	chksum = ((chksum & 1) ? 0x80 : 0) + (chksum >> 1) + n[3];
	chksum = ((chksum & 1) ? 0x80 : 0) + (chksum >> 1) + n[4];
	chksum = ((chksum & 1) ? 0x80 : 0) + (chksum >> 1) + n[5];
	chksum = ((chksum & 1) ? 0x80 : 0) + (chksum >> 1) + n[6];
	chksum = ((chksum & 1) ? 0x80 : 0) + (chksum >> 1) + n[7];
	chksum = ((chksum & 1) ? 0x80 : 0) + (chksum >> 1) + n[8];
	chksum = ((chksum & 1) ? 0x80 : 0) + (chksum >> 1) + n[9];
	chksum = ((chksum & 1) ? 0x80 : 0) + (chksum >> 1) + n[10];

	return chksum;
}

static bool fat_dir_list(u32 clus, u32 * count, p_fat_entry * entrys)
{
	if (clus == 0)
		return false;
	if (clus < 2) {
		*count = dbr.root_entry;
		if ((*entrys = malloc(*count * sizeof(**entrys))) == NULL)
			return false;
		if (xrIoLseek(fatfd, root_pos, PSP_SEEK_SET) != root_pos
			|| xrIoRead(fatfd, *entrys,
						*count * sizeof(t_fat_entry)) !=
			*count * sizeof(t_fat_entry)) {
			free(*entrys);
			return false;
		}
	} else {
		if (fat_table[clus] < 2)
			return false;
		u32 c2 = clus;

		*count = 1;
		while (fat_table[c2] < clus_max && fat_table[c2] > 1) {
			c2 = fat_table[c2];
			(*count)++;
		}
		c2 = clus;
		u32 epc = (bytes_per_clus / sizeof(t_fat_entry)), ep = 0;

		(*count) *= epc;
		if ((*entrys = malloc(*count * sizeof(**entrys))) == NULL)
			return false;
		do {
			u64 epos = data_pos + 1ull * (c2 - 2) * bytes_per_clus;

			if (xrIoLseek(fatfd, epos, PSP_SEEK_SET) != epos
				|| xrIoRead(fatfd, &(*entrys)[ep],
							bytes_per_clus) != bytes_per_clus) {
				free(*entrys);
				return false;
			}
			ep += epc;
			c2 = fat_table[c2];
		} while (c2 < clus_max && c2 > 1);
	}
	return true;
}

static bool fat_get_longname(p_fat_entry entrys, u32 cur, char *longnamestr)
{
	u16 chksum = fat_calc_chksum(&entrys[cur]);
	u32 j = cur;
	u16 longname[260];

	memset(longname, 0, 260 * sizeof(u16));
	while (j > 0) {
		j--;
		if (entrys[j].norm.attr != 0x0F
			|| entrys[j].longfile.checksum != chksum
			|| entrys[j].norm.filename[0] == 0
			|| (u8) entrys[j].norm.filename[0] == 0xE5)
			return false;
		u32 order = entrys[j].longfile.order & 0x3F;

		if (order > 20)
			return false;
		u32 ppos = (order - 1) * 13;
		u32 k;

		for (k = 0; k < 5; k++)
			longname[ppos++] = entrys[j].longfile.uni_name[k];
		for (k = 0; k < 6; k++)
			longname[ppos++] = entrys[j].longfile.uni_name2[k];
		for (k = 0; k < 2; k++)
			longname[ppos++] = entrys[j].longfile.uni_name3[k];
		if ((entrys[j].longfile.order & 0x40) > 0)
			break;
	}
	if (entrys[j].norm.attr != 0x0F || (entrys[j].longfile.order & 0x40) == 0)
		return false;
	longname[255] = 0;
	memset(longnamestr, 0, 256);
	charsets_ucs_conv((const byte *) longname, sizeof(longname),
					  (byte *) longnamestr, 256);
	return true;
}

static void fat_get_shortname(p_fat_entry entry, char *shortnamestr)
{
	static bool chartable[256] = {
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	// 0x00
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	// 0x00
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	// 0x00
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	// 0x30
		0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,	// 0x40
		1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0,	// 0x50
		0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,	// 0x60
		1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0,	// 0x70
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	// 0x80
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	// 0x90
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	// 0xA0
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	// 0xB0
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	// 0xC0
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	// 0xD0
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	// 0xE0
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0	// 0xF0
	};
	u32 i = 0;
	u8 abit = 0;

	if ((entry->norm.flag & 0x08) > 0)
		abit = 0x20;
	while (i < 8 && entry->norm.filename[i] != 0x20) {
		*shortnamestr++ =
			entry->norm.
			filename[i] | ((chartable[(u8) entry->norm.filename[i]]) ? abit :
						   0);
		i++;
	}
	if (entry->norm.fileext[0] != 0x20) {
		*shortnamestr++ = '.';
		i = 0;
		while (i < 3 && entry->norm.fileext[i] != 0x20)
			*shortnamestr++ = entry->norm.fileext[i++] | abit;
	}
	*shortnamestr = 0;
}

extern bool fat_locate(const char *name, char *sname, dword clus,
					   p_fat_entry info)
{
	u32 count;
	p_fat_entry entrys;

	if (!fat_dir_list(clus, &count, &entrys))
		return false;
	SceUID dl = xrIoDopen(sname);

	if (dl < 0)
		return false;
	char shortname[11];
	bool onlylong = false;
	u32 nlen = strlen(name);

	if (nlen > 12)
		onlylong = true;
	else {
		char *dot = strrchr(name, '.');

		if ((dot == NULL && nlen < 9)
			|| (dot - name < 9 && nlen - 1 - (dot - name) < 4)) {
			memset(&shortname[0], 0x20, 11);
			memcpy(&shortname[0], name, (dot == NULL) ? nlen : (dot - name));
			if (dot != NULL)
				memcpy(&shortname[8], dot + 1, nlen - 1 - (dot - name));
		} else
			onlylong = true;
	}
	SceIoDirent sid;
	u32 i;

	for (i = 0; i < count; i++) {
		if ((entrys[i].norm.attr & FAT_FILEATTR_VOLUME) == 0
			&& entrys[i].norm.filename[0] != 0
			&& (u8) entrys[i].norm.filename[0] != 0xE5) {
			memset(&sid, 0, sizeof(SceIoDirent));
			int result;

			while ((result = xrIoDread(dl, &sid)) > 0
				   && (sid.d_stat.st_attr & 0x08) > 0);
			if (result == 0)
				break;
			if (entrys[i].norm.filename[0] == 0x05)
				entrys[i].norm.filename[0] = 0xE5;
			if (!onlylong
				&& strnicmp(shortname, &entrys[i].norm.filename[0], 11) == 0) {
				if ((u8) entrys[i].norm.filename[0] == 0xE5)
					entrys[i].norm.filename[0] = 0x05;
				memcpy(info, &entrys[i], sizeof(t_fat_entry));
				free(entrys);
				if (xrKernelDevkitVersion() <= 0x03070110) {
					strcat_s(sname, 256, sid.d_name);
				} else {
					char short_name[256];

					fat_get_shortname(info, short_name);
					strcat_s(sname, 256, short_name);
				}
				if ((info->norm.attr & FAT_FILEATTR_DIRECTORY) > 0)
					strcat_s(sname, 256, "/");
				xrIoDclose(dl);
				return true;
			}
			if ((u8) entrys[i].norm.filename[0] == 0xE5)
				entrys[i].norm.filename[0] = 0x05;
			char longnames[256];

			if (!fat_get_longname(entrys, i, longnames))
				continue;
			if (stricmp(name, longnames) == 0) {
				memcpy(info, &entrys[i], sizeof(t_fat_entry));
				free(entrys);
				if (xrKernelDevkitVersion() <= 0x03070110) {
					strcat_s(sname, 256, sid.d_name);
				} else {
					char short_name[256];

					fat_get_shortname(info, short_name);
					strcat_s(sname, 256, short_name);
				}
				if ((info->norm.attr & FAT_FILEATTR_DIRECTORY) > 0)
					strcat_s(sname, 256, "/");
				xrIoDclose(dl);
				return true;
			}
		}
	}
	free(entrys);
	xrIoDclose(dl);
	return false;
}

static u32 fat_dir_clus(const char *dir, char *shortdir)
{
	if (!fat_load_table() || fatfd < 0)
		return 0;
	char rdir[256];

	STRCPY_S(rdir, dir);
	char *partname = strtok(rdir, "/\\");

	if (partname == NULL) {
		fat_free_table();
		return 0;
	}
	if (strcmp(partname, "ms0:") != 0 && strcmp(partname, "fatms:") != 0
		&& strcmp(partname, "fatms0:") != 0) {
		fat_free_table();
		return 0;
	}
	strcpy_s(shortdir, 256, partname);
	strcat_s(shortdir, 256, "/");
	partname = strtok(NULL, "/\\");
	u32 clus = (fat_type == fat32) ? dbr.ufat.fat32.root_clus : 1;
	t_fat_entry entry;

	while (partname != NULL) {
		if (partname[0] != 0) {
			if (fat_locate(partname, shortdir, clus, &entry)) {
				clus = entry.norm.clus_high;
				clus =
					((fat_type == fat32) ? (clus << 16) : 0) + entry.norm.clus;
			} else {
				fat_free_table();
				return 0;
			}
		}
		partname = strtok(NULL, "/\\");
	}
	fat_free_table();
	return clus;
}

extern dword fat_readdir(const char *dir, char *sdir, p_fat_info * info)
{
	fat_lock();
	if (!fat_load_table() || fatfd < 0) {
		fat_unlock();
		return INVALID;
	}
	u32 clus = fat_dir_clus(dir, sdir);
	SceUID dl = 0;

	if (clus == 0 || (dl = xrIoDopen(sdir)) < 0) {
		fat_free_table();
		xrIoDclose(dl);
		fat_unlock();
		return INVALID;
	}
	u32 ecount = 0;
	p_fat_entry entrys;

	if (!fat_dir_list(clus, &ecount, &entrys)) {
		fat_free_table();
		xrIoDclose(dl);
		fat_unlock();
		return INVALID;
	}
	u32 count = 0, cur = 0, i;

	for (i = 0; i < ecount; i++) {
		if ((entrys[i].norm.attr & FAT_FILEATTR_VOLUME) != 0
			|| entrys[i].norm.filename[0] == 0
			|| (u8) entrys[i].norm.filename[0] == 0xE5
			|| (entrys[i].norm.filename[0] == '.'
				&& entrys[i].norm.filename[1] == 0x20))
			continue;
		count++;
	}
	if (count == 0 || (*info = malloc(count * sizeof(**info))) == NULL) {
		free(entrys);
		fat_free_table();
		xrIoDclose(dl);
		fat_unlock();
		return INVALID;
	}
	SceIoDirent sid;

	for (i = 0; i < ecount; i++) {
		if ((entrys[i].norm.attr & FAT_FILEATTR_VOLUME) != 0
			|| entrys[i].norm.filename[0] == 0
			|| (u8) entrys[i].norm.filename[0] == 0xE5
			|| (entrys[i].norm.filename[0] == '.'
				&& entrys[i].norm.filename[1] == 0x20))
			continue;
		memset(&sid, 0, sizeof(SceIoDirent));
		int result;

		while ((result = xrIoDread(dl, &sid)) > 0
			   && ((sid.d_stat.st_attr & 0x08) > 0
				   || (sid.d_name[0] == '.' && sid.d_name[1] == 0)));
		if (result == 0)
			break;
		p_fat_info inf = &((*info)[cur]);

		fat_get_shortname(&entrys[i], inf->filename);
		if (inf->filename[0] == 0x05)
			inf->filename[0] = 0xE5;
		if (!fat_get_longname(entrys, i, inf->longname))
			STRCPY_S(inf->longname, inf->filename);
		if (xrKernelDevkitVersion() <= 0x03070110) {
			STRCPY_S(inf->filename, sid.d_name);
		}
		inf->filesize = entrys[i].norm.filesize;
		inf->cdate = entrys[i].norm.cr_date;
		inf->ctime = entrys[i].norm.cr_time;
		inf->mdate = entrys[i].norm.last_mod_date;
		inf->mtime = entrys[i].norm.last_mod_time;
		inf->clus =
			((fat_type ==
			  fat32) ? (((u32) entrys[i].norm.clus_high) << 16) : 0) +
			entrys[i].norm.clus;
		inf->attr = entrys[i].norm.attr;
		cur++;
	}
	free(entrys);
	fat_free_table();
	xrIoDclose(dl);
	fat_unlock();
	return cur;
}

extern void fat_free(void)
{
	if (fatfd >= 0) {
		xrIoClose(fatfd);
		fatfd = -1;
	}
	memset(&dbr, 0, sizeof(dbr));
	memset(&mbr, 0, sizeof(mbr));
	root_pos = 0;
	data_pos = 0;
	bytes_per_clus = 0;
	loadcount = 0;
	clus_max = 0;
	fat_type = fat16;
	xrKernelDeleteSema(fat_sema);
}

/**
 * 将长文件名转换为8.3文件名
 * @param shortname 短文件名
 * @param longname 长文件名
 * @param size 短文件名缓冲字节大小
 * @return 是否成功
 * @note 参考: http://support.microsoft.com/kb/142982/zh-cn
 */
extern bool fat_longnametoshortname(char *shortname, const char *longname,
									dword size)
{
	p_fat_info info = NULL;
	char dirname[PATH_MAX], spath[PATH_MAX], longfilename[PATH_MAX];
	char *p = NULL;
	bool manual_init = false;

	STRCPY_S(dirname, longname);
	if ((p = strrchr(dirname, '/')) != NULL) {
		*p = '\0';
		STRCPY_S(longfilename, p + 1);
	} else {
		STRCPY_S(longfilename, dirname);
	}

	if (fatfd < 0) {
		manual_init = true;
		fat_init();
	}
	dword i, count = fat_readdir(dirname, spath, &info);

	if (manual_init)
		fat_free();

	if (count == INVALID || info == NULL) {
		return false;
	}

	for (i = 0; i < count; ++i) {
		if (stricmp(info[i].longname, longfilename) == 0) {
			if (spath[strlen(spath) - 1] != '/')
				STRCAT_S(spath, "/");
			STRCAT_S(spath, info[i].filename);
			strcpy_s(shortname, size, spath);
			free(info);
			return true;
		}
	}

	free(info);
	return false;
}
