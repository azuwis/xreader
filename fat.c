/* vim:set ts=4 sw=4 cindent ignorecase enc=gbk: */

#include "config.h"

#include <stdlib.h>
#include <string.h>
#include <pspkernel.h>
#include "common/utils.h"
#include "charsets.h"
#include "fat.h"

typedef unsigned long long dquad;

static int fatfd = -1;
static t_fat_dbr dbr;
static t_fat_mbr mbr;
static dword * fat_table = NULL;
static dquad root_pos = 0;
static dquad data_pos = 0;
static dquad bytes_per_clus = 0;
static dquad loadcount = 0;
static dquad clus_max = 0;
static enum {
	fat12,
	fat16,
	fat32
} fat_type = fat16;
static SceUID fat_sema = -1;

void fat_powerdown()
{
	fat_lock();
	sceIoClose(fatfd);
	fatfd = -1;
}

void fat_powerup()
{
	fatfd = sceIoOpen("msstor:", PSP_O_RDONLY, 0777);
	fat_unlock();
}

void fat_lock()
{
	sceKernelWaitSema(fat_sema, 1, NULL);
}

void fat_unlock()
{
	sceKernelSignalSema(fat_sema, 1);
}

extern bool fat_init()
{
	fat_sema = sceKernelCreateSema("FAT Sema", 0, 1, 1, NULL);
	if(fat_sema < 0)
		return false;
	fat_lock();
	fatfd = sceIoOpen("msstor:", PSP_O_RDONLY, 0777);
	if(fatfd < 0) {
		fat_unlock();
		return false;
	}
	if(sceIoRead(fatfd, &mbr, sizeof(mbr)) != sizeof(mbr))
	{
		sceIoClose(fatfd);
		fatfd = -1;
		fat_unlock();
		return false;
	}
	if(sceIoLseek(fatfd, mbr.dpt[0].start_sec * 0x200, PSP_SEEK_SET) != mbr.dpt[0].start_sec * 0x200 || sceIoRead(fatfd, &dbr, sizeof(dbr)) < sizeof(dbr))
	{
		sceIoClose(fatfd);
		fatfd = -1;
		fat_unlock();
		return false;
	}
	// Add Vista Format fat32 support
	if ( dbr.root_entry == 0 )
	{
		fat_type = fat32;
		clus_max = 0x0FFFFFF0;
	}
	else if(mbr.dpt[0].id == 0x1)
	{
		fat_type = fat12;
		clus_max = 0x0FF0;
	}
	else //if(mbr.dpt[0].id == 0x4 || mbr.dpt[0].id == 0x6 || mbr.dpt[0].id == 0xE || mbr.dpt[0].id == 0xB )
	{
		fat_type = fat16;
		clus_max = 0xFFF0;
	}
	bytes_per_clus = dbr.sec_per_clus * dbr.bytes_per_sec;
	if(fat_type == fat32)
	{
		data_pos = mbr.dpt[0].start_sec * 0x200 + (dbr.ufat.fat32.sec_per_fat * dbr.num_fats + dbr.reserved_sec) * dbr.bytes_per_sec;
		root_pos = data_pos + bytes_per_clus * dbr.ufat.fat32.root_clus;
	}
	else
	{
		root_pos = mbr.dpt[0].start_sec * 0x200 + (dbr.num_fats * dbr.sec_per_fat + dbr.reserved_sec) * dbr.bytes_per_sec;
		data_pos = root_pos + dbr.root_entry * sizeof(t_fat_entry);
	}
	sceIoClose(fatfd);
	fat_unlock();
	return true;
}

static bool convert_table_fat12()
{
	word * otable = (word *)malloc(dbr.sec_per_fat * dbr.bytes_per_sec);
	if(otable == NULL)
		return false;
	memcpy(otable, fat_table, dbr.sec_per_fat * dbr.bytes_per_sec);
	int i, j, entrycount = dbr.sec_per_fat * dbr.bytes_per_sec / sizeof(word) * 4 / 3;
	free((void *)fat_table);
	fat_table = (dword *)malloc(sizeof(dword) * entrycount);
	if(fat_table == NULL)
	{
		free((void *)otable);
		return false;
	}
	for(i = 0, j = 0; i < entrycount; i += 4, j += 3)
	{
		fat_table[i] = otable[j] & 0x0FFF;
		fat_table[i + 1] = ((otable[j] >> 12) & 0x000F) | ((otable[j + 1] & 0x00FF) << 4);
		fat_table[i + 2] = ((otable[j + 1] >> 8) & 0x00FF) | ((otable[j + 2] & 0x000F) << 8);
		fat_table[i + 3] = otable[j + 2] >> 4;
	}
	free((void *)otable);
	return true;
}

static bool convert_table_fat16()
{
	word * otable = (word *)malloc(dbr.sec_per_fat * dbr.bytes_per_sec);
	if(otable == NULL)
		return false;
	memcpy(otable, fat_table, dbr.sec_per_fat * dbr.bytes_per_sec);
	int i, entrycount = dbr.sec_per_fat * dbr.bytes_per_sec / sizeof(word);
	free((void *)fat_table);
	fat_table = (dword *)malloc(sizeof(dword) * entrycount);
	if(fat_table == NULL)
	{
		free((void *)otable);
		return false;
	}
	for(i = 0; i < entrycount; i ++)
		fat_table[i] = otable[i];
	free((void *)otable);
	return true;
}

static bool fat_load_table()
{
	if(loadcount > 0)
	{
		loadcount ++;
		return true;
	}
	fatfd = sceIoOpen("msstor:", PSP_O_RDONLY, 0777);
	if(fatfd < 0) {
		return false;
	}
	if(sceIoLseek(fatfd, mbr.dpt[0].start_sec * 0x200 + dbr.reserved_sec * dbr.bytes_per_sec, PSP_SEEK_SET) != mbr.dpt[0].start_sec * 0x200 + dbr.reserved_sec * dbr.bytes_per_sec || (fat_table = (dword *)malloc(((fat_type == fat32) ? dbr.ufat.fat32.sec_per_fat : dbr.sec_per_fat) * dbr.bytes_per_sec)) == NULL)
	{
		sceIoClose(fatfd);
		fatfd = -1;
		return false;
	}
	if(sceIoRead(fatfd, fat_table, ((fat_type == fat32) ? dbr.ufat.fat32.sec_per_fat : dbr.sec_per_fat) * dbr.bytes_per_sec) != ((fat_type == fat32) ? dbr.ufat.fat32.sec_per_fat : dbr.sec_per_fat) * dbr.bytes_per_sec || (mbr.dpt[0].id == 1 && !convert_table_fat12()) || ((mbr.dpt[0].id == 0x04 || mbr.dpt[0].id == 0x06 || mbr.dpt[0].id == 0x0E) && !convert_table_fat16()))
	{
		sceIoClose(fatfd);
		fatfd = -1;
		free((void *)fat_table);
		fat_table = NULL;
		return false;
	}
	loadcount = 1;
	return true;
}

static void fat_free_table()
{
	if(loadcount > 0)
	{
		loadcount --;
		if(loadcount > 0)
			return;
		if(fat_table != NULL)
		{
			free((void *)fat_table);
			fat_table = NULL;
		}
		sceIoClose(fatfd);
	}
}

static byte fat_calc_chksum(p_fat_entry info)
{
	byte * n = (byte *)&info->norm.filename[0];
	byte chksum = 0;
	dword i;
	for(i = 0; i < 11; i ++)
		chksum = ((chksum & 1) ? 0x80 : 0) + (chksum >> 1) + n[i];
	return chksum;
}

static bool fat_dir_list(dword clus, dword * count, p_fat_entry * entrys)
{
	if(clus == 0)
		return false;
	if(clus < 2)
	{
		* count = dbr.root_entry;
		if((* entrys = (p_fat_entry)malloc(* count * sizeof(t_fat_entry))) == NULL)
			return false;
		if(sceIoLseek(fatfd, root_pos, PSP_SEEK_SET) != root_pos || sceIoRead(fatfd, * entrys, * count * sizeof(t_fat_entry)) != * count * sizeof(t_fat_entry))
		{
			free((void *)* entrys);
			return false;
		}
	}
	else
	{
		if(fat_table[clus] < 2)
			return false;
		dword c2 = clus;
		* count = 1;
		while(1)
		{
			c2 = fat_table[c2];
			if(c2 >= clus_max || c2 < 2)
				break;
			(* count) ++;
		}
		dword epc = (bytes_per_clus / sizeof(t_fat_entry));
		if((* entrys = (p_fat_entry)malloc(* count * epc * sizeof(t_fat_entry))) == NULL)
			return false;
		c2 = clus;
		dword i;
		for(i = 0; i < *count; i ++) {
			sceIoLseek(fatfd, data_pos + (c2 - 2) * bytes_per_clus, PSP_SEEK_SET);
			sceIoRead(fatfd, &((*entrys)[i * epc]), bytes_per_clus);
			c2 = fat_table[c2];
		}
		(* count) *= epc;
	}
	return true;
}

static bool fat_get_longname(p_fat_entry entrys, dword cur, char * longnamestr)
{
	word chksum = fat_calc_chksum(&entrys[cur]);
	dword j = cur;
	word longname[260];
	memset(longname, 0, 260 * sizeof(word));
	while(j > 0)
	{
		j --;
		if(entrys[j].norm.attr != 0x0F || entrys[j].longfile.checksum != chksum || entrys[j].norm.filename[0] == 0 || (byte)entrys[j].norm.filename[0] == 0xE5)
			return false;
		dword order = entrys[j].longfile.order & 0x3F;
		if(order > 20)
			return false;
		dword ppos = (order - 1) * 13;
		dword k;
		for(k = 0; k < 5; k ++)
			longname[ppos ++] = entrys[j].longfile.uni_name[k];
		for(k = 0; k < 6; k ++)
			longname[ppos ++] = entrys[j].longfile.uni_name2[k];
		for(k = 0; k < 2; k ++)
			longname[ppos ++] = entrys[j].longfile.uni_name3[k];
		if((entrys[j].longfile.order & 0x40) > 0)
			break;
	}
	if(entrys[j].norm.attr != 0x0F || (entrys[j].longfile.order & 0x40) == 0)
		return false;
	longname[255] = 0;
	memset(longnamestr, 0, 256);
	charsets_ucs_conv((const byte *)longname, (byte *)longnamestr);
	return true;
}

static void fat_get_shortname(p_fat_entry entry, char * shortnamestr)
{
	static bool chartable[256] = {
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 0x00
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 0x00
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 0x00
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 0x30
		0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  // 0x40
		1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0,  // 0x50
		0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  // 0x60
		1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0,  // 0x70
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 0x80
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 0x90
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 0xA0
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 0xB0
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 0xC0
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 0xD0
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 0xE0
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0   // 0xF0
	};
	dword i = 0;
	byte abit = 0;
	if((entry->norm.flag & 0x08) > 0)
		abit = 0x20;
	while(i < 8 && entry->norm.filename[i] != 0x20)
	{
		*shortnamestr ++ = entry->norm.filename[i] | ((chartable[(byte)entry->norm.filename[i]]) ? abit : 0);
		i ++;
	}
	if(entry->norm.fileext[0] != 0x20)
	{
		*shortnamestr ++ = '.';
		i = 0;
		while(i < 3 && entry->norm.fileext[i] != 0x20)
			*shortnamestr ++ = entry->norm.fileext[i ++] | abit;
	}
	*shortnamestr = 0;
}

extern bool fat_locate(const char * name, char * sname, dword clus, p_fat_entry info)
{
	dword count;
	p_fat_entry entrys;
	if(!fat_dir_list(clus, &count, &entrys))
		return false;
	SceUID dl = sceIoDopen(sname);
	if(dl < 0)
		return false;
	char shortname[11];
	bool onlylong = false;
	dword nlen = strlen(name);
	if(nlen > 12)
		onlylong = true;
	else
	{
		char * dot = strrchr(name, '.');
		if((dot == NULL && nlen < 9) || (dot - name < 9 && nlen - 1 - (dot - name) < 4))
		{
			memset(&shortname[0], 0x20, 11);
			memcpy(&shortname[0], name, (dot == NULL) ? nlen : (dot - name));
			if(dot != NULL)
				memcpy(&shortname[8], dot + 1, nlen - 1 - (dot - name));
		}
		else
			onlylong = true;
	}
	SceIoDirent sid;
	dword i;
	for(i = 0; i < count; i ++) {
		if((entrys[i].norm.attr & FAT_FILEATTR_VOLUME) == 0 && entrys[i].norm.filename[0] != 0 && (byte)entrys[i].norm.filename[0] != 0xE5)
		{
			memset(&sid, 0, sizeof(SceIoDirent));
			int result;
			while((result = sceIoDread(dl, &sid)) > 0 && (sid.d_stat.st_attr & 0x08) > 0);
			if(result == 0)
				break;
			if(entrys[i].norm.filename[0] == 0x05)
				entrys[i].norm.filename[0] = 0xE5;
			if(!onlylong && strnicmp(shortname, &entrys[i].norm.filename[0], 11) == 0)
			{
				if((byte)entrys[i].norm.filename[0] == 0xE5)
					entrys[i].norm.filename[0] = 0x05;
				memcpy(info, &entrys[i], sizeof(t_fat_entry));
				free((void *)entrys);
				strcat_s(sname, 256, sid.d_name);
				if((entrys[i].norm.attr & FAT_FILEATTR_DIRECTORY) > 0)
					strcat_s(sname, 256, "/");
				sceIoDclose(dl);
				return true;
			}
			if((byte)entrys[i].norm.filename[0] == 0xE5)
				entrys[i].norm.filename[0] = 0x05;
			char longnames[256];
			if(!fat_get_longname(entrys, i, longnames))
				continue;
			if(stricmp(name, longnames) == 0)
			{
				memcpy(info, &entrys[i], sizeof(t_fat_entry));
				free((void *)entrys);
				strcat_s(sname, 256, sid.d_name);
				if((entrys[i].norm.attr & FAT_FILEATTR_DIRECTORY) > 0)
					strcat_s(sname, 256, "/");
				sceIoDclose(dl);
				return true;
			}
		}
	}
	free((void *)entrys);
	sceIoDclose(dl);
	return false;
}

static dword fat_dir_clus(const char * dir, char * shortdir)
{
	if(!fat_load_table() || fatfd < 0)
		return 0;
	char rdir[256];
	STRCPY_S(rdir, dir);
	char * partname = strtok(rdir, "/\\");
	if(partname == NULL)
	{
		fat_free_table();
		return 0;
	}
	if(strcmp(partname, "ms0:") != 0 && strcmp(partname, "fatms:") != 0 && strcmp(partname, "fatms0:") != 0)
	{
		fat_free_table();
		return 0;
	}
	strcpy_s(shortdir, 256, partname);
	strcat_s(shortdir, 256, "/");
	partname = strtok(NULL, "/\\");
	dword clus = (fat_type == fat32) ? dbr.ufat.fat32.root_clus : 1;
	t_fat_entry entry;
	while(partname != NULL) {
		if(partname[0] != 0)
		{
			if(fat_locate(partname, shortdir, clus, &entry))
				clus = ((fat_type == fat32) ? (((dword)entry.norm.clus_high) << 16) : 0) + entry.norm.clus;
			else
			{
				fat_free_table();
				return 0;
			}
		}
		partname = strtok(NULL, "/\\");
	}
	fat_free_table();
	return clus;
}

extern dword fat_readdir(const char * dir, char * sdir, p_fat_info * info)
{
	if(!fat_load_table() || fatfd < 0)
		return INVALID;
	dword clus = fat_dir_clus(dir, sdir);
	SceUID dl = 0;
	if(clus == 0 || (dl = sceIoDopen(sdir)) < 0)
	{
		fat_free_table();
		sceIoDclose(dl);
		return INVALID;
	}
	dword ecount = 0;
	p_fat_entry entrys;
	if(!fat_dir_list(clus, &ecount, &entrys))
	{
		fat_free_table();
		sceIoDclose(dl);
		return INVALID;
	}
	dword count = 0, cur = 0, i;
	for(i = 0; i < ecount; i ++)
	{
		if((entrys[i].norm.attr & FAT_FILEATTR_VOLUME) != 0 || entrys[i].norm.filename[0] == 0 || (byte)entrys[i].norm.filename[0] == 0xE5 || (entrys[i].norm.filename[0] == '.' && entrys[i].norm.filename[1] == 0x20))
			continue;
		count ++;
	}
	if(count == 0 || (*info = (p_fat_info)malloc(count * sizeof(t_fat_info))) == NULL)
	{
		free(entrys);
		fat_free_table();
		sceIoDclose(dl);
		return INVALID;
	}
	SceIoDirent sid;
	for(i = 0; i < ecount; i ++)
	{
		if((entrys[i].norm.attr & FAT_FILEATTR_VOLUME) != 0 || entrys[i].norm.filename[0] == 0 || (byte)entrys[i].norm.filename[0] == 0xE5 || (entrys[i].norm.filename[0] == '.' && entrys[i].norm.filename[1] == 0x20))
			continue;
		memset(&sid, 0, sizeof(SceIoDirent));
		int result;
		while((result = sceIoDread(dl, &sid)) > 0 && ((sid.d_stat.st_attr & 0x08) > 0 || (sid.d_name[0] == '.' && sid.d_name[1] == 0)));
		if(result == 0)
			break;
		p_fat_info inf = &((*info)[cur]);
		fat_get_shortname(&entrys[i], inf->filename);
		if(inf->filename[0] == 0x05)
			inf->filename[0] = 0xE5;
		if(!fat_get_longname(entrys, i, inf->longname))
			STRCPY_S(inf->longname, inf->filename);
		STRCPY_S(inf->filename, sid.d_name);
		inf->filesize = entrys[i].norm.filesize;
		inf->cdate = entrys[i].norm.cr_date;
		inf->ctime = entrys[i].norm.cr_time;
		inf->mdate = entrys[i].norm.last_mod_date;
		inf->mtime = entrys[i].norm.last_mod_time;
		inf->clus = ((fat_type == fat32) ? (((dword)entrys[i].norm.clus_high) << 16) : 0) + entrys[i].norm.clus;
		inf->attr = entrys[i].norm.attr;
		cur ++;
	}
	free(entrys);
	fat_free_table();
	sceIoDclose(dl);
	return cur;
}

extern void fat_free()
{
	if(fatfd >= 0)
	{
		sceIoClose(fatfd);
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
	sceKernelDeleteSema(fat_sema);
}
