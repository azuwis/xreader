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

#ifndef _FAT_H_
#define _FAT_H_

#include "common/datatype.h"

struct _fat_mbr_dpt
{
	byte active;
	byte start[3];
	byte id;
	byte ending[3];
	dword start_sec;
	dword total_sec;
} __attribute__ ((packed));

struct _fat_mbr
{
	byte mb_data[0x1BE];
	struct _fat_mbr_dpt dpt[4];
	word ending_flag;
} __attribute__ ((packed));
typedef struct _fat_mbr t_fat_mbr, *p_fat_mbr;

struct _fat_dbr
{
	byte jmp_boot[3];
	char oem_name[8];
	word bytes_per_sec;
	byte sec_per_clus;
	word reserved_sec;
	byte num_fats;
	word root_entry;
	word total_sec;
	byte media_type;
	word sec_per_fat;
	word sec_per_track;
	word heads;
	dword hidd_sec;
	dword big_total_sec;
	union
	{
		struct
		{
			byte drv_num;
			byte reserved;
			byte boot_sig;
			byte vol_id[4];
			char vol_lab[11];
			char file_sys_type[8];
		} __attribute__ ((packed)) fat16;
		struct
		{
			dword sec_per_fat;
			word extend_flag;
			word sys_ver;
			dword root_clus;
			word info_sec;
			word back_sec;
			byte reserved[10];
		} __attribute__ ((packed)) fat32;
	} __attribute__ ((packed)) ufat;
	byte exe_code[448];
	word ending_flag;
} __attribute__ ((packed));
typedef struct _fat_dbr t_fat_dbr, *p_fat_dbr;

struct _fat_normentry
{
	char filename[8];
	char fileext[3];
	byte attr;
	byte flag;
	byte cr_time_msec;
	word cr_time;
	word cr_date;
	word last_visit;
	word clus_high;
	word last_mod_time;
	word last_mod_date;
	word clus;
	dword filesize;
} __attribute__ ((packed));

struct _fat_longfile
{
	byte order;
	word uni_name[5];
	byte sig;
	byte reserved;
	byte checksum;
	word uni_name2[6];
	word clus;
	word uni_name3[2];
} __attribute__ ((packed));

union _fat_entry
{
	struct _fat_normentry norm;
	struct _fat_longfile longfile;
} __attribute__ ((packed));
typedef union _fat_entry t_fat_entry, *p_fat_entry;

#define FAT_FILEATTR_READONLY	0x01
#define FAT_FILEATTR_HIDDEN		0x02
#define FAT_FILEATTR_SYSTEM		0x04
#define FAT_FILEATTR_VOLUME		0x08
#define FAT_FILEATTR_DIRECTORY	0x10
#define FAT_FILEATTR_ARCHIVE	0x20

typedef struct
{
	char filename[256];
	char longname[256];
	dword filesize;
	word cdate;
	word ctime;
	word mdate;
	word mtime;
	dword clus;
	byte attr;
} __attribute__ ((aligned(16))) t_fat_info, *p_fat_info;

void fat_lock(void);
void fat_unlock(void);
extern void fat_powerup(void);
extern void fat_powerdown(void);
extern bool fat_init(void);
extern bool fat_locate(const char *name, char *sname, dword clus,
					   p_fat_entry info);
extern dword fat_readdir(const char *dir, char *sdir, p_fat_info * info);
extern void fat_free(void);
extern bool fat_longnametoshortname(char *shortname, const char *longname,
									dword size);

#endif
