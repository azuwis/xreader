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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pspuser.h>
#include <unzip.h>
#include <chm_lib.h>
#include <unrar.h>
#include "common/utils.h"
#include "html.h"
#include "display.h"
#include "charsets.h"
#include "fat.h"
#include "fs.h"
#include "scene.h"
#include "conf.h"
#include "win.h"
#include "passwdmgr.h"
#include "osk.h"
#include "bg.h"
#include "image.h"
#include "archive.h"
#include "freq_lock.h"
#include "musicdrv.h"
#include "dbg.h"
#include "xrhal.h"
#ifdef DMALLOC
#include "dmalloc.h"
#endif

typedef struct
{
	const char *ext;
	t_fs_filetype ft;
} t_fs_filetype_entry;

typedef struct
{
	const char *fname;
	t_fs_filetype ft;
} t_fs_specfiletype_entry;

t_fs_filetype_entry ft_table[] = {
	{"lrc", fs_filetype_txt},
	{"txt", fs_filetype_txt},
	{"log", fs_filetype_txt},
	{"ini", fs_filetype_txt},
	{"cfg", fs_filetype_txt},
	{"conf", fs_filetype_txt},
	{"inf", fs_filetype_txt},
	{"xml", fs_filetype_txt},
	{"cpp", fs_filetype_txt},
	{"in", fs_filetype_txt},
	{"ac", fs_filetype_txt},
	{"m4", fs_filetype_txt},
	{"am", fs_filetype_txt},
	{"mak", fs_filetype_txt},
	{"exp", fs_filetype_txt},
	{"sh", fs_filetype_txt},
	{"asm", fs_filetype_txt},
	{"s", fs_filetype_txt},
	{"patch", fs_filetype_txt},
	{"c", fs_filetype_txt},
	{"h", fs_filetype_txt},
	{"hpp", fs_filetype_txt},
	{"cc", fs_filetype_txt},
	{"cxx", fs_filetype_txt},
	{"pas", fs_filetype_txt},
	{"bas", fs_filetype_txt},
	{"py", fs_filetype_txt},
	{"mk", fs_filetype_txt},
	{"rc", fs_filetype_txt},
	{"pl", fs_filetype_txt},
	{"cgi", fs_filetype_txt},
	{"bat", fs_filetype_txt},
	{"js", fs_filetype_txt},
	{"vbs", fs_filetype_txt},
	{"vb", fs_filetype_txt},
	{"cs", fs_filetype_txt},
	{"css", fs_filetype_txt},
	{"csv", fs_filetype_txt},
	{"php", fs_filetype_txt},
	{"php3", fs_filetype_txt},
	{"asp", fs_filetype_txt},
	{"aspx", fs_filetype_txt},
	{"java", fs_filetype_txt},
	{"jsp", fs_filetype_txt},
	{"awk", fs_filetype_txt},
	{"tcl", fs_filetype_txt},
	{"y", fs_filetype_txt},
	{"html", fs_filetype_html},
	{"htm", fs_filetype_html},
	{"shtml", fs_filetype_html},
#ifdef ENABLE_IMAGE
	{"png", fs_filetype_png},
	{"gif", fs_filetype_gif},
	{"jpg", fs_filetype_jpg},
	{"jpeg", fs_filetype_jpg},
	{"thm", fs_filetype_jpg},
	{"bmp", fs_filetype_bmp},
	{"tga", fs_filetype_tga},
#endif
	{"zip", fs_filetype_zip},
	{"cbz", fs_filetype_zip},
	{"gz", fs_filetype_gz},
	{"umd", fs_filetype_umd},
	{"pdb", fs_filetype_pdb},
	{"chm", fs_filetype_chm},
	{"rar", fs_filetype_rar},
	{"cbr", fs_filetype_rar},
	{"pbp", fs_filetype_prog},
	{"ebm", fs_filetype_ebm},
	{"iso", fs_filetype_iso},
	{"cso", fs_filetype_iso},
#ifdef ENABLE_TTF
	{"ttf", fs_filetype_font},
	{"ttc", fs_filetype_font},
#endif
	{NULL, fs_filetype_unknown}
};

t_fs_specfiletype_entry ft_spec_table[] = {
	{"Makefile", fs_filetype_txt},
	{"LICENSE", fs_filetype_txt},
	{"TODO", fs_filetype_txt},
	{"Configure", fs_filetype_txt},
	{"Changelog", fs_filetype_txt},
	{"Readme", fs_filetype_txt},
	{"Version", fs_filetype_txt},
	{"INSTALL", fs_filetype_txt},
	{"CREDITS", fs_filetype_txt},
	{NULL, fs_filetype_unknown}
};

int MAX_ITEM_NAME_LEN = 40;
int DIR_INC_SIZE = 256;
p_umd_chapter p_umdchapter = NULL;

void filename_to_itemname(p_win_menuitem item, int cur_count,
						  const char *filename)
{
	if ((item[cur_count].width = strlen(filename)) > MAX_ITEM_NAME_LEN) {
		mbcsncpy_s(((unsigned char *) item[cur_count].name),
				   MAX_ITEM_NAME_LEN - 2,
				   ((const unsigned char *) filename), -1);
		if (strlen(item[cur_count].name) < MAX_ITEM_NAME_LEN - 3) {
			mbcsncpy_s(((unsigned char *) item[cur_count].name),
					   MAX_ITEM_NAME_LEN,
					   ((const unsigned char *) filename), -1);
			STRCAT_S(item[cur_count].name, "..");
		} else
			STRCAT_S(item[cur_count].name, "...");
		item[cur_count].width = MAX_ITEM_NAME_LEN;
	} else {
		STRCPY_S(item[cur_count].name, filename);
	}
}

extern dword fs_list_device(const char *dir, const char *sdir,
							p_win_menuitem * mitem, dword icolor,
							dword selicolor, dword selrcolor, dword selbcolor)
{
	extern dword filecount;

	win_item_destroy(mitem, &filecount);

	strcpy_s((char *) sdir, 256, dir);
	dword cur_count = 0;
	p_win_menuitem item = NULL;

	cur_count = config.hide_flash ? 1 : 3;
	*mitem = win_realloc_items(NULL, 0, cur_count);
	if (*mitem == NULL)
		return 0;
	item = *mitem;
	STRCPY_S(item[0].name, "<MemoryStick>");
	buffer_copy_string(item[0].compname, "ms0:");
	buffer_copy_string(item[0].shortname, "ms0:");
	item[0].data = (void *) fs_filetype_dir;
	item[0].width = 13;
	item[0].selected = false;
	item[0].icolor = icolor;
	item[0].selicolor = selicolor;
	item[0].selrcolor = selrcolor;
	item[0].selbcolor = selbcolor;

	if (config.hide_flash == false) {
		STRCPY_S(item[1].name, "<NandFlash 0>");
		buffer_copy_string(item[1].compname, "flash0:");
		item[1].data = (void *) fs_filetype_dir;
		item[1].width = 13;
		item[1].selected = false;
		item[1].icolor = icolor;
		item[1].selicolor = selicolor;
		item[1].selrcolor = selrcolor;
		item[1].selbcolor = selbcolor;
		STRCPY_S(item[2].name, "<NandFlash 1>");
		buffer_copy_string(item[2].compname, "flash1:");
		item[2].data = (void *) fs_filetype_dir;
		item[2].width = 13;
		item[2].selected = false;
		item[2].icolor = icolor;
		item[2].selicolor = selicolor;
		item[2].selrcolor = selrcolor;
		item[2].selbcolor = selbcolor;
	}

	return cur_count;
}

extern dword fs_flashdir_to_menu(const char *dir, const char *sdir,
								 p_win_menuitem * mitem, dword icolor,
								 dword selicolor, dword selrcolor,
								 dword selbcolor)
{
	extern dword filecount;
	int fid;
	dword itemcount;

	win_item_destroy(mitem, &filecount);

	fid = freq_enter_hotzone();
	strcpy_s((char *) sdir, 256, dir);
	SceIoDirent info;
	dword cur_count = 0;
	p_win_menuitem item = NULL;
	int fd = xrIoDopen(dir);

	if (fd < 0) {
		freq_leave(fid);
		return 0;
	}
	//  if(stricmp(dir, "ms0:/") == 0)
	{
		itemcount = DIR_INC_SIZE;
		*mitem = win_realloc_items(NULL, 0, itemcount);
		if (*mitem == NULL) {
			xrIoDclose(fd);
			freq_leave(fid);
			return 0;
		}
		cur_count = 1;
		item = *mitem;
		STRCPY_S(item[0].name, "<..>");
		buffer_copy_string(item[0].compname, "..");
		buffer_copy_string(item[0].shortname, "..");
		item[0].data = (void *) fs_filetype_dir;
		item[0].width = 4;
		item[0].selected = false;
		item[0].icolor = icolor;
		item[0].selicolor = selicolor;
		item[0].selrcolor = selrcolor;
		item[0].selbcolor = selbcolor;
	}

	memset(&info, 0, sizeof(SceIoDirent));

	while (xrIoDread(fd, &info) > 0) {
		if ((info.d_stat.st_mode & FIO_S_IFMT) == FIO_S_IFDIR) {
			if (info.d_name[0] == '.' && info.d_name[1] == 0)
				continue;
			if (strcmp(info.d_name, "..") == 0)
				continue;
			if (cur_count % DIR_INC_SIZE == 0) {
				itemcount = cur_count + DIR_INC_SIZE;

				*mitem = win_realloc_items(*mitem, cur_count, itemcount);

				if (*mitem == NULL) {
					freq_leave(fid);
					return 0;
				}

				item = *mitem;
			}
			item[cur_count].data = (void *) fs_filetype_dir;
			buffer_copy_string(item[cur_count].compname, info.d_name);
			item[cur_count].name[0] = '<';
			if ((item[cur_count].width =
				 strlen(info.d_name) + 2) > MAX_ITEM_NAME_LEN) {
				mbcsncpy_s((unsigned char *) &item[cur_count].name[1],
						   MAX_ITEM_NAME_LEN - 4,
						   (const unsigned char *) info.d_name, -1);
				STRCAT_S(item[cur_count].name, "...>");
				item[cur_count].width = MAX_ITEM_NAME_LEN;
			} else {
				mbcsncpy_s((unsigned char *) &item[cur_count].name[1],
						   MAX_ITEM_NAME_LEN - 1,
						   (const unsigned char *) info.d_name, -1);
				STRCAT_S(item[cur_count].name, ">");
			}
		} else {
			t_fs_filetype ft = fs_file_get_type(info.d_name);

			if (cur_count % DIR_INC_SIZE == 0) {
				itemcount = cur_count + DIR_INC_SIZE;

				*mitem = win_realloc_items(*mitem, cur_count, itemcount);

				if (*mitem == NULL) {
					freq_leave(fid);
					return 0;
				}

				item = *mitem;
			}
			item[cur_count].data = (void *) ft;
			buffer_copy_string(item[cur_count].compname, info.d_name);
			buffer_copy_string(item[cur_count].shortname, info.d_name);
			filename_to_itemname(item, cur_count, info.d_name);
		}
		item[cur_count].icolor = icolor;
		item[cur_count].selicolor = selicolor;
		item[cur_count].selrcolor = selrcolor;
		item[cur_count].selbcolor = selbcolor;
		item[cur_count].selected = false;
		item[cur_count].data2[0] =
			((info.d_stat.st_ctime.year - 1980) << 9) +
			(info.d_stat.st_ctime.month << 5) + info.d_stat.st_ctime.day;
		item[cur_count].data2[1] =
			(info.d_stat.st_ctime.hour << 11) +
			(info.d_stat.st_ctime.minute << 5) +
			info.d_stat.st_ctime.second / 2;
		item[cur_count].data2[2] =
			((info.d_stat.st_mtime.year - 1980) << 9) +
			(info.d_stat.st_mtime.month << 5) + info.d_stat.st_mtime.day;
		item[cur_count].data2[3] =
			(info.d_stat.st_mtime.hour << 11) +
			(info.d_stat.st_mtime.minute << 5) +
			info.d_stat.st_mtime.second / 2;
		item[cur_count].data3 = info.d_stat.st_size;
		cur_count++;
	}

	xrIoDclose(fd);
	freq_leave(fid);

	// Remove unused item
	*mitem = win_realloc_items(*mitem, itemcount, cur_count);

	return cur_count;
}

// New style fat system custom reading
extern dword fs_dir_to_menu(const char *dir, char *sdir, p_win_menuitem * mitem,
							dword icolor, dword selicolor, dword selrcolor,
							dword selbcolor, bool showhidden, bool showunknown)
{
	extern dword filecount;

	win_item_destroy(mitem, &filecount);

	int fid = freq_enter_hotzone();
	p_win_menuitem item = NULL;
	p_fat_info info;

	dword itemcount;
	dword count = fat_readdir(dir, sdir, &info);
	dword i, cur_count = 0;

	if (count == INVALID) {
		freq_leave(fid);
		return 0;
	}

	if (stricmp(dir, "ms0:/") == 0) {
		itemcount = DIR_INC_SIZE;
		*mitem = win_realloc_items(NULL, 0, itemcount);

		if (*mitem == NULL) {
			free(info);
			freq_leave(fid);
			return 0;
		}

		cur_count = 1;
		item = *mitem;
		STRCPY_S(item[0].name, "<..>");
		buffer_copy_string(item[0].compname, "..");
		buffer_copy_string(item[0].shortname, "..");
		item[0].data = (void *) fs_filetype_dir;
		item[0].width = 4;
		item[0].selected = false;
		item[0].icolor = icolor;
		item[0].selicolor = selicolor;
		item[0].selrcolor = selrcolor;
		item[0].selbcolor = selbcolor;
	}

	for (i = 0; i < count; i++) {
		if (!showhidden && (info[i].attr & FAT_FILEATTR_HIDDEN) > 0)
			continue;
		if (cur_count % DIR_INC_SIZE == 0) {
			itemcount = cur_count + DIR_INC_SIZE;

			*mitem = win_realloc_items(*mitem, cur_count, itemcount);

			if (*mitem == NULL) {
				free(info);
				freq_leave(fid);
				return 0;
			}

			item = *mitem;
		}
		if (info[i].attr & FAT_FILEATTR_DIRECTORY) {
			item[cur_count].data = (void *) fs_filetype_dir;
			buffer_copy_string(item[cur_count].shortname, info[i].filename);
			buffer_copy_string(item[cur_count].compname, info[i].longname);
			item[cur_count].name[0] = '<';
			if ((item[cur_count].width =
				 strlen(info[i].longname) + 2) > MAX_ITEM_NAME_LEN) {
				strncpy_s(&item[cur_count].name[1],
						  NELEMS(item[cur_count].name) - 1,
						  info[i].longname, MAX_ITEM_NAME_LEN - 5);
				item[cur_count].name[MAX_ITEM_NAME_LEN - 4] =
					item[cur_count].name[MAX_ITEM_NAME_LEN -
										 3] =
					item[cur_count].name[MAX_ITEM_NAME_LEN - 2] = '.';
				item[cur_count].name[MAX_ITEM_NAME_LEN - 1] = '>';
				item[cur_count].name[MAX_ITEM_NAME_LEN] = 0;
				item[cur_count].width = MAX_ITEM_NAME_LEN;
			} else {
				strncpy_s(&item[cur_count].name[1],
						  NELEMS(item[cur_count].name) - 1,
						  info[i].longname, MAX_ITEM_NAME_LEN);
				item[cur_count].name[item[cur_count].width - 1] = '>';
				item[cur_count].name[item[cur_count].width] = 0;
			}
		} else {
			if (info[i].filesize == 0)
				continue;
			t_fs_filetype ft = fs_file_get_type(info[i].longname);

			if (!showunknown && ft == fs_filetype_unknown)
				continue;
			item[cur_count].data = (void *) ft;
			buffer_copy_string(item[cur_count].shortname, info[i].filename);
			buffer_copy_string(item[cur_count].compname, info[i].longname);
			filename_to_itemname(item, cur_count, info[i].longname);
		}
		item[cur_count].icolor = icolor;
		item[cur_count].selicolor = selicolor;
		item[cur_count].selrcolor = selrcolor;
		item[cur_count].selbcolor = selbcolor;
		item[cur_count].selected = false;
		item[cur_count].data2[0] = info[i].cdate;
		item[cur_count].data2[1] = info[i].ctime;
		item[cur_count].data2[2] = info[i].mdate;
		item[cur_count].data2[3] = info[i].mtime;
		item[cur_count].data3 = info[i].filesize;
		cur_count++;
	}
	free(info);
	freq_leave(fid);

	// Remove unused item
	*mitem = win_realloc_items(*mitem, itemcount, cur_count);

	return cur_count;
}

extern dword fs_zip_to_menu(const char *zipfile, p_win_menuitem * mitem,
							dword icolor, dword selicolor, dword selrcolor,
							dword selbcolor)
{
	extern dword filecount;
	dword itemcount;
	dword cur_count = 1;

	win_item_destroy(mitem, &filecount);

	int fid = freq_enter_hotzone();
	unzFile unzf = unzOpen(zipfile);
	p_win_menuitem item = NULL;

	if (unzf == NULL) {
		freq_leave(fid);
		return 0;
	}

	itemcount = DIR_INC_SIZE;
	*mitem = win_realloc_items(NULL, 0, itemcount);

	if (*mitem == NULL) {
		unzClose(unzf);
		freq_leave(fid);
		return 0;
	}

	item = *mitem;
	STRCPY_S(item[0].name, "<..>");
	buffer_copy_string(item[0].compname, "..");
	buffer_copy_string(item[0].shortname, "..");
	item[0].data = (void *) fs_filetype_dir;
	item[0].width = 4;
	item[0].selected = false;
	item[0].icolor = icolor;
	item[0].selicolor = selicolor;
	item[0].selrcolor = selrcolor;
	item[0].selbcolor = selbcolor;

	if (unzGoToFirstFile(unzf) != UNZ_OK) {
		unzClose(unzf);
		freq_leave(fid);
		*mitem = win_realloc_items(*mitem, itemcount, cur_count);
		return cur_count;
	}

	do {
		char fname[PATH_MAX];
		unz_file_info file_info;

		if (unzGetCurrentFileInfo
			(unzf, &file_info, fname, PATH_MAX, NULL, 0, NULL, 0) != UNZ_OK)
			break;
		if (file_info.uncompressed_size == 0)
			continue;
		t_fs_filetype ft = fs_file_get_type(fname);

		if (ft == fs_filetype_chm || ft == fs_filetype_zip
			|| ft == fs_filetype_rar)
			continue;
		if (cur_count % DIR_INC_SIZE == 0) {
			itemcount = cur_count + DIR_INC_SIZE;

			*mitem = win_realloc_items(*mitem, cur_count, itemcount);

			if (*mitem == NULL) {
				unzClose(unzf);
				freq_leave(fid);
				return 0;
			}

			item = *mitem;
		}
		item[cur_count].data = (void *) ft;
		buffer_copy_string(item[cur_count].compname, fname);
		char t[20];

		SPRINTF_S(t, "%u", (unsigned int) file_info.uncompressed_size);
		buffer_copy_string(item[cur_count].shortname, t);
		filename_to_itemname(item, cur_count, fname);
		item[cur_count].selected = false;
		item[cur_count].icolor = icolor;
		item[cur_count].selicolor = selicolor;
		item[cur_count].selrcolor = selrcolor;
		item[cur_count].selbcolor = selbcolor;
		item[cur_count].data3 = file_info.uncompressed_size;
		cur_count++;
	} while (unzGoToNextFile(unzf) == UNZ_OK);
	unzClose(unzf);
	freq_leave(fid);

	*mitem = win_realloc_items(*mitem, itemcount, cur_count);

	return cur_count;
}

extern dword fs_rar_to_menu(const char *rarfile, p_win_menuitem * mitem,
							dword icolor, dword selicolor, dword selrcolor,
							dword selbcolor)
{
	extern dword filecount;
	dword itemcount;

	win_item_destroy(mitem, &filecount);

	int fid = freq_enter_hotzone();
	p_win_menuitem item = NULL;

	struct RAROpenArchiveData arcdata;

	arcdata.ArcName = (char *) rarfile;
	arcdata.OpenMode = RAR_OM_LIST;
	arcdata.CmtBuf = NULL;
	arcdata.CmtBufSize = 0;
	HANDLE hrar = RAROpenArchive(&arcdata);

	if (hrar == 0) {
		freq_leave(fid);
		return 0;
	}

	dword cur_count = 1;

	itemcount = DIR_INC_SIZE;
	*mitem = win_realloc_items(NULL, 0, itemcount);
	if (*mitem == NULL) {
		RARCloseArchive(hrar);
		freq_leave(fid);
		return 0;
	}

	item = *mitem;
	STRCPY_S(item[0].name, "<..>");
	buffer_copy_string(item[0].compname, "..");
	buffer_copy_string(item[0].shortname, "..");
	item[0].data = (void *) fs_filetype_dir;
	item[0].width = 4;
	item[0].selected = false;
	item[0].selected = false;
	item[0].icolor = icolor;
	item[0].selicolor = selicolor;
	item[0].selrcolor = selrcolor;
	item[0].selbcolor = selbcolor;
	struct RARHeaderDataEx header;
	int ret;

	do {
		if ((ret = RARReadHeaderEx(hrar, &header)) != 0) {
			if (ret != ERAR_UNKNOWN)
				break;
			RARCloseArchive(hrar);
			if ((hrar = reopen_rar_with_passwords(&arcdata)) == 0)
				break;
			if (RARReadHeaderEx(hrar, &header) != 0)
				break;
		}
		if (header.UnpSize == 0)
			continue;
		t_fs_filetype ft = fs_file_get_type(header.FileName);

		if (ft == fs_filetype_chm || ft == fs_filetype_zip
			|| ft == fs_filetype_rar)
			continue;
		if (cur_count % DIR_INC_SIZE == 0) {
			itemcount = cur_count + DIR_INC_SIZE;

			*mitem = win_realloc_items(*mitem, cur_count, itemcount);

			if (*mitem == NULL) {
				RARCloseArchive(hrar);
				freq_leave(fid);
				return 0;
			}

			item = *mitem;
		}
		item[cur_count].data = (void *) ft;
		if (header.Flags & 0x200) {
			char str[1024];

			memset(str, 0, 1024);
			const byte *uni = (byte *) header.FileNameW;

			charsets_utf32_conv(uni, sizeof(header.FileNameW), (byte *) str,
								sizeof(str));
			buffer_copy_string_len(item[cur_count].compname, header.FileName,
								   256);
			filename_to_itemname(item, cur_count, str);
		} else {
			buffer_copy_string_len(item[cur_count].compname,
								   header.FileName, 256);
			filename_to_itemname(item, cur_count, header.FileName);
		}
		char t[20];

		SPRINTF_S(t, "%u", (unsigned int) header.UnpSize);
		buffer_copy_string(item[cur_count].shortname, t);
		item[cur_count].selected = false;
		item[cur_count].icolor = icolor;
		item[cur_count].selicolor = selicolor;
		item[cur_count].selrcolor = selrcolor;
		item[cur_count].selbcolor = selbcolor;
		item[cur_count].data3 = header.UnpSize;
		cur_count++;
	} while (RARProcessFile(hrar, RAR_SKIP, NULL, NULL) == 0);

	RARCloseArchive(hrar);
	freq_leave(fid);

	// Remove unused item
	*mitem = win_realloc_items(*mitem, itemcount, cur_count);

	return cur_count;
}

p_win_menuitem fs_empty_dir(dword * filecount, dword icolor,
							dword selicolor, dword selrcolor, dword selbcolor)
{
	p_win_menuitem p;

	p = win_realloc_items(NULL, 0, 1);

	if (p == NULL) {
		return NULL;
	}

	STRCPY_S(p->name, "<..>");
	buffer_copy_string(p->compname, "..");
	buffer_copy_string(p->shortname, "..");
	p->data = (void *) fs_filetype_dir;
	p->width = 4;
	p->selected = false;
	p->icolor = icolor;
	p->selicolor = selicolor;
	p->selrcolor = selrcolor;
	p->selbcolor = selbcolor;

	*filecount = 1;
	return p;
}

typedef struct
{
	p_win_menuitem *mitem;
	dword cur_count;
	dword icolor;
	dword selicolor;
	dword selrcolor;
	dword selbcolor;
} t_fs_chm_enum, *p_fs_chm_enum;

static dword *chm_itemcount = NULL;

static int chmEnum(struct chmFile *h, struct chmUnitInfo *ui, void *context)
{
	p_win_menuitem *mitem = ((p_fs_chm_enum) context)->mitem;
	p_win_menuitem item = *mitem;

	int size = strlen(ui->path);

	if (size == 0 || ui->path[size - 1] == '/')
		return CHM_ENUMERATOR_CONTINUE;

	t_fs_filetype ft = fs_file_get_type(ui->path);

	if (ft == fs_filetype_chm || ft == fs_filetype_zip || ft == fs_filetype_rar
		|| ft == fs_filetype_umd || ft == fs_filetype_pdb)
		return CHM_ENUMERATOR_CONTINUE;

	dword cur_count = ((p_fs_chm_enum) context)->cur_count;

	if (cur_count % DIR_INC_SIZE == 0) {
		*chm_itemcount = cur_count + DIR_INC_SIZE;
		*mitem = win_realloc_items(*mitem, cur_count, *chm_itemcount);

		if (*mitem == NULL) {
			((p_fs_chm_enum) context)->cur_count = 0;
			return CHM_ENUMERATOR_FAILURE;
		}

		item = *mitem;
	}

	char fname[PATH_MAX] = "";

	buffer_copy_string(item[cur_count].compname, ui->path);
	char t[20];

	SPRINTF_S(t, "%u", (unsigned int) ui->length);
	buffer_copy_string(item[cur_count].shortname, t);
	if (ui->path[0] == '/') {
		strncpy_s(fname, NELEMS(fname), ui->path + 1, 256);
	} else {
		strncpy_s(fname, NELEMS(fname), ui->path, 256);
	}

	charsets_utf8_conv((unsigned char *) fname, sizeof(fname),
					   (unsigned char *) fname, sizeof(fname));

	item[cur_count].data = (void *) ft;
	filename_to_itemname(item, cur_count, fname);
	item[cur_count].selected = false;
	item[cur_count].icolor = ((p_fs_chm_enum) context)->icolor;
	item[cur_count].selicolor = ((p_fs_chm_enum) context)->selicolor;
	item[cur_count].selrcolor = ((p_fs_chm_enum) context)->selrcolor;
	item[cur_count].selbcolor = ((p_fs_chm_enum) context)->selbcolor;
	item[cur_count].data3 = ui->length;
	((p_fs_chm_enum) context)->cur_count++;
	return CHM_ENUMERATOR_CONTINUE;
}

extern dword fs_chm_to_menu(const char *chmfile, p_win_menuitem * mitem,
							dword icolor, dword selicolor, dword selrcolor,
							dword selbcolor)
{
	extern dword filecount;
	dword itemcount;

	win_item_destroy(mitem, &filecount);

	int fid = freq_enter_hotzone();
	struct chmFile *chm = chm_open(chmfile);
	p_win_menuitem item = NULL;

	if (chm == NULL) {
		freq_leave(fid);
		return 0;
	}

	itemcount = DIR_INC_SIZE;
	*mitem = win_realloc_items(NULL, 0, itemcount);

	if (*mitem == NULL) {
		chm_close(chm);
		freq_leave(fid);
		return 0;
	}

	item = *mitem;
	STRCPY_S(item[0].name, "<..>");
	buffer_copy_string(item[0].compname, "..");
	buffer_copy_string(item[0].shortname, "..");
	item[0].data = (void *) fs_filetype_dir;
	item[0].width = 4;
	item[0].selected = false;
	item[0].icolor = icolor;
	item[0].selicolor = selicolor;
	item[0].selrcolor = selrcolor;
	item[0].selbcolor = selbcolor;

	t_fs_chm_enum cenum;

	cenum.mitem = mitem;
	cenum.cur_count = 1;
	cenum.icolor = icolor;
	cenum.selicolor = selicolor;
	cenum.selrcolor = selrcolor;
	cenum.selbcolor = selbcolor;

	chm_itemcount = &itemcount;
	chm_enumerate(chm, CHM_ENUMERATE_NORMAL | CHM_ENUMERATE_FILES, chmEnum,
				  (void *) &cenum);

	chm_close(chm);
	freq_leave(fid);

	*mitem = win_realloc_items(*mitem, itemcount, cenum.cur_count);

	return cenum.cur_count;
}

extern dword fs_umd_to_menu(const char *umdfile, p_win_menuitem * mitem,
							dword icolor, dword selicolor, dword selrcolor,
							dword selbcolor)
{
	buffer *pbuf = NULL;
	extern dword filecount;
	p_win_menuitem item = NULL;
	dword cur_count = 1;
	dword itemcount;
	int fid;

	win_item_destroy(mitem, &filecount);

	fid = freq_enter_hotzone();

	itemcount = DIR_INC_SIZE;
	*mitem = win_realloc_items(NULL, 0, itemcount);

	if (*mitem == NULL) {
		freq_leave(fid);
		return 0;
	}

	item = *mitem;

	STRCPY_S(item[0].name, "<..>");
	buffer_copy_string(item[0].compname, "..");
	buffer_copy_string(item[0].shortname, "..");
	item[0].data = (void *) fs_filetype_dir;
	item[0].width = 4;
	item[0].selected = false;
	item[0].icolor = icolor;
	item[0].selicolor = selicolor;
	item[0].selrcolor = selrcolor;
	item[0].selbcolor = selbcolor;

	do {
		if (!p_umdchapter
			|| (p_umdchapter->umdfile->ptr
				&& strcmp(p_umdchapter->umdfile->ptr, umdfile))) {
			if (p_umdchapter)
				umd_chapter_reset(p_umdchapter);
			p_umdchapter = umd_chapter_init();
			if (!p_umdchapter)
				break;
			buffer_copy_string(p_umdchapter->umdfile, umdfile);
			cur_count = parse_umd_chapters(umdfile, &p_umdchapter);
		} else
			cur_count = p_umdchapter->chapter_count + 1;

		if (cur_count > 1) {
			if (cur_count > DIR_INC_SIZE) {
				itemcount = cur_count;
				*mitem = win_realloc_items(*mitem, DIR_INC_SIZE, itemcount);

				if (*mitem == NULL)
					break;

				item = *mitem;
			}

			u_int i = 1;
			struct t_chapter *p = p_umdchapter->pchapters;
			size_t stlen = 0;

			pbuf = buffer_init();
			if (!pbuf || buffer_prepare_copy(pbuf, 256) < 0)
				break;
			char pos[20] = { 0 };
			for (i = 1; i < cur_count; i++) {
				stlen = p[i - 1].name->used - 1;
				stlen =
					charsets_ucs_conv((const byte *) p[i - 1].name->ptr, stlen,
									  (byte *) pbuf->ptr, pbuf->size);
				SPRINTF_S(pos, "%d", p[i - 1].length);
				buffer_copy_string_len(item[i].shortname, pos, 20);
				buffer_copy_string_len(item[i].compname, pbuf->ptr,
									   (stlen > 256) ? 256 : stlen);
				filename_to_itemname(item, i, item[i].compname->ptr);
				if (1 != p_umdchapter->umd_type) {
					if (0 == p_umdchapter->umd_mode)
						item[i].data = (void *) fs_filetype_bmp;
					else if (1 == p_umdchapter->umd_mode)
						item[i].data = (void *) fs_filetype_jpg;
					else
						item[i].data = (void *) fs_filetype_gif;
				} else
					item[i].data = (void *) fs_filetype_txt;
				item[i].data2[0] = p[i - 1].chunk_pos & 0xFFFF;
				item[i].data2[1] = (p[i - 1].chunk_pos >> 16) & 0xFFFF;
				item[i].data2[2] = p[i - 1].chunk_offset & 0xFFFF;
				item[i].data2[3] = (p[i - 1].chunk_offset >> 16) & 0xFFFF;
				item[i].data3 = p[i - 1].length;
#if 0
				printf("%d pos:%d,%d,%d-%d,%d\n", i, p[i - 1].chunk_pos,
					   item[i].data2[0], item[i].data2[1], item[i].data2[2],
					   item[i].data2[3]);
#endif
				item[i].selected = false;
				item[i].icolor = icolor;
				item[i].selicolor = selicolor;
				item[i].selrcolor = selrcolor;
				item[i].selbcolor = selbcolor;
				//buffer_free(p[i - 1].name);
			}
#if 0
			printf("%s umd file:%s type:%d,mode:%d,chapter count:%ld\n",
				   __func__, umdfile, p_umdchapter->umd_type,
				   p_umdchapter->umd_mode, cur_count);
#endif
		}
	} while (false);

	if (pbuf)
		buffer_free(pbuf);

	freq_leave(fid);
#define MAX_CHAP_LEN 500
	cur_count = (cur_count > 1 && cur_count < MAX_CHAP_LEN) ? cur_count : 1;
	*mitem = win_realloc_items(*mitem, itemcount, cur_count);

	return cur_count;
}

extern t_fs_filetype fs_file_get_type(const char *filename)
{
	const char *ext = utils_fileext(filename);
	t_fs_filetype_entry *entry = ft_table;
	t_fs_specfiletype_entry *entry2 = ft_spec_table;

	if (ext) {
		while (entry->ext != NULL) {
			if (stricmp(ext, entry->ext) == 0)
				return entry->ft;
			entry++;
		}
	}

	while (entry2->fname != NULL) {
		const char *shortname = strrchr(filename, '/');

		if (!shortname)
			shortname = filename;
		else
			shortname++;
		if (stricmp(shortname, entry2->fname) == 0)
			return entry2->ft;
		entry2++;
	}

#ifdef ENABLE_MUSIC
	if (fs_is_music(filename, filename)) {
		return fs_filetype_music;
	}
#endif

	return fs_filetype_unknown;
}

#ifdef ENABLE_IMAGE
extern bool fs_is_image(t_fs_filetype ft)
{
	return ft == fs_filetype_jpg || ft == fs_filetype_gif
		|| ft == fs_filetype_png || ft == fs_filetype_tga
		|| ft == fs_filetype_bmp;
}
#endif

extern bool fs_is_txtbook(t_fs_filetype ft)
{
	return ft == fs_filetype_txt || ft == fs_filetype_html
		|| ft == fs_filetype_gz;
}

#ifdef ENABLE_MUSIC
extern bool fs_is_music(const char *spath, const char *lpath)
{
	bool res;

	res = musicdrv_chk_file(spath) != NULL ? true : false;

	if (res)
		return res;

	res = musicdrv_chk_file(lpath) != NULL ? true : false;

	return res;
}
#endif
