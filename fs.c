/* vim:set ts=4 sw=4 cindent ignorecase enc=gbk: */

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

typedef struct {
	const char * ext;
	t_fs_filetype ft;
} t_fs_filetype_entry;

typedef struct {
	const char* fname;
	t_fs_filetype ft;
} t_fs_specfiletype_entry;

t_fs_filetype_entry ft_table[] =
{
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
#ifdef ENABLE_PMPAVC
	{"pmp", fs_filetype_pmp},
#endif
#ifdef ENABLE_IMAGE
	{"png", fs_filetype_png},
	{"gif", fs_filetype_gif},
	{"jpg", fs_filetype_jpg},
	{"jpeg", fs_filetype_jpg},
	{"bmp", fs_filetype_bmp},
	{"tga", fs_filetype_tga},
#endif
	{"zip", fs_filetype_zip},
	{"gz", fs_filetype_gz},
	{"chm", fs_filetype_chm},
	{"rar", fs_filetype_rar},
	{"prx", fs_filetype_prog},
	{"pbp", fs_filetype_prog},
#ifdef ENABLE_MUSIC
	{"mp3", fs_filetype_mp3},
	{"aa3", fs_filetype_aa3},
#ifdef ENABLE_WMA
	{"wma", fs_filetype_wma},
#endif
#endif
	{"ebm", fs_filetype_ebm},
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
#ifdef ENABLE_MUSIC
  {"mpeg", fs_filetype_mp3},
  {"mpea", fs_filetype_mp3},
#endif
  {NULL, fs_filetype_unknown}
};

int MAX_ITEM_NAME_LEN = 40;

void filename_to_itemname(p_win_menuitem item, int cur_count, const char* filename)
{
	if((item[cur_count].width = strlen(filename)) > MAX_ITEM_NAME_LEN)
	{
		strncpy(item[cur_count].name, filename, MAX_ITEM_NAME_LEN - 3);
		item[cur_count].name[MAX_ITEM_NAME_LEN - 3] = item[cur_count].name[MAX_ITEM_NAME_LEN - 2] = item[cur_count].name[MAX_ITEM_NAME_LEN - 1] = '.';
		item[cur_count].name[MAX_ITEM_NAME_LEN] = 0;
		item[cur_count].width = MAX_ITEM_NAME_LEN;
	}
	else {
		strncpy(item[cur_count].name, filename, 64);
		item[cur_count].name[63] = '\0';
	}
}

extern dword fs_list_device(const char * dir, const char * sdir, p_win_menuitem * mitem, dword icolor, dword selicolor, dword selrcolor, dword selbcolor)
{
	strcpy((char *)sdir, dir);
	dword cur_count = 0;
	p_win_menuitem item = NULL;
	cur_count = 1;
	* mitem = (p_win_menuitem)malloc(sizeof(t_win_menuitem) * 3);
	if(* mitem == NULL)
		return 0;
	item = * mitem;
	strcpy(item[0].name, "<MemoryStick>");
	strcpy(item[0].compname, "ms0:");
	item[0].data = (void *)fs_filetype_dir;
	item[0].width = 13;
	item[0].selected = false;
	item[0].icolor = icolor;
	item[0].selicolor = selicolor;
	item[0].selrcolor = selrcolor;
	item[0].selbcolor = selbcolor;
	return cur_count;
}

extern dword fs_flashdir_to_menu(const char * dir, const char * sdir, p_win_menuitem * mitem, dword icolor, dword selicolor, dword selrcolor, dword selbcolor)
{
	strcpy((char *)sdir, dir);
	SceIoDirent info;
	dword cur_count = 0;
	p_win_menuitem item = NULL;
	int fd = sceIoDopen(dir);
	if(fd < 0)
		return 0;
	if(stricmp(dir, "ms0:/") == 0)
	{
		cur_count = 1;
		* mitem = (p_win_menuitem)malloc(sizeof(t_win_menuitem) * 256);
		if(* mitem == NULL)
		{
			sceIoDclose(fd);
			return 0;
		}
		item = * mitem;
		strcpy(item[0].name, "<..>");
		strcpy(item[0].compname, "..");
		item[0].data = (void *)fs_filetype_dir;
		item[0].width = 4;
		item[0].selected = false;
		item[0].icolor = icolor;
		item[0].selicolor = selicolor;
		item[0].selrcolor = selrcolor;
		item[0].selbcolor = selbcolor;
	}
	memset(&info, 0, sizeof(SceIoDirent));
	while(sceIoDread(fd, &info) > 0)
	{
		if(info.d_stat.st_attr & FIO_SO_IFDIR)
		{
			if(info.d_name[0] == '.' && info.d_name[1] == 0)
				continue;
			if(cur_count % 256 == 0)
			{
				if(cur_count == 0)
					* mitem = (p_win_menuitem)malloc(sizeof(t_win_menuitem) * 256);
				else
					* mitem = (p_win_menuitem)realloc(*mitem, sizeof(t_win_menuitem) * (cur_count + 256));
				if(* mitem == NULL)
					return 0;
				item = *mitem;
			}
			item[cur_count].data = (void *)fs_filetype_dir;
			strcpy(item[cur_count].compname, info.d_name);
			item[cur_count].name[0] = '<';
			if((item[cur_count].width = strlen(info.d_name) + 2) > MAX_ITEM_NAME_LEN)
			{
				strncpy(&item[cur_count].name[1], info.d_name, MAX_ITEM_NAME_LEN - 5);
				item[cur_count].name[MAX_ITEM_NAME_LEN - 4] = item[cur_count].name[MAX_ITEM_NAME_LEN - 3] = item[cur_count].name[MAX_ITEM_NAME_LEN - 2] = '.';
				item[cur_count].name[MAX_ITEM_NAME_LEN - 1] = '>';
				item[cur_count].name[MAX_ITEM_NAME_LEN] = 0;
				item[cur_count].width = MAX_ITEM_NAME_LEN;
			}
			else
			{
				strncpy(&item[cur_count].name[1], info.d_name, MAX_ITEM_NAME_LEN);
				item[cur_count].name[item[cur_count].width - 1] = '>';
				item[cur_count].name[item[cur_count].width] = 0;
			}
		}
		else
		{
			t_fs_filetype ft = fs_file_get_type(info.d_name);
			if(cur_count % 256 == 0)
			{
				if(cur_count == 0)
					* mitem = (p_win_menuitem)malloc(sizeof(t_win_menuitem) * 256);
				else
					* mitem = (p_win_menuitem)realloc(*mitem, sizeof(t_win_menuitem) * (cur_count + 256));
				if(* mitem == NULL)
					return 0;
				item = *mitem;
			}
			item[cur_count].data = (void *)ft;
			strcpy(item[cur_count].compname, info.d_name);
			strcpy(item[cur_count].shortname, info.d_name);
			filename_to_itemname(item, cur_count, info.d_name);
		}
		item[cur_count].icolor = icolor;
		item[cur_count].selicolor = selicolor;
		item[cur_count].selrcolor = selrcolor;
		item[cur_count].selbcolor = selbcolor;
		item[cur_count].selected = false;
		item[cur_count].data2[0] = ((info.d_stat.st_ctime.year - 1980) << 9) + (info.d_stat.st_ctime.month << 5) + info.d_stat.st_ctime.day;
		item[cur_count].data2[1] = (info.d_stat.st_ctime.hour << 11) + (info.d_stat.st_ctime.minute << 5) + info.d_stat.st_ctime.second / 2;
		item[cur_count].data2[2] = ((info.d_stat.st_mtime.year - 1980) << 9) + (info.d_stat.st_mtime.month << 5) + info.d_stat.st_mtime.day;
		item[cur_count].data2[3] = (info.d_stat.st_mtime.hour << 11) + (info.d_stat.st_mtime.minute << 5) + info.d_stat.st_mtime.second / 2;
		item[cur_count].data3 = info.d_stat.st_size;
		cur_count ++;
	}
	sceIoDclose(fd);
	return cur_count;
}

// New style fat system custom reading
extern dword fs_dir_to_menu(const char * dir, char * sdir, p_win_menuitem * mitem, dword icolor, dword selicolor, dword selrcolor, dword selbcolor, bool showhidden, bool showunknown)
{
	p_win_menuitem item = NULL;
	p_fat_info info;
	if(* mitem != NULL)
	{
		free((void *)* mitem);
		* mitem = NULL;
	}
	dword count = fat_readdir(dir, sdir, &info);
	if(count == INVALID)
		return 0;
	if(*mitem != NULL)
	{
		free((void *)* mitem);
		* mitem = NULL;
	}
	dword i, cur_count = 0;
	if(stricmp(dir, "ms0:/") == 0)
	{
		cur_count = 1;
		* mitem = (p_win_menuitem)malloc(sizeof(t_win_menuitem) * 256);
		if(* mitem == NULL)
		{
			free((void *)info);
			return 0;
		}
		item = * mitem;
		strcpy(item[0].name, "<..>");
		strcpy(item[0].compname, "..");
		item[0].data = (void *)fs_filetype_dir;
		item[0].width = 4;
		item[0].selected = false;
		item[0].icolor = icolor;
		item[0].selicolor = selicolor;
		item[0].selrcolor = selrcolor;
		item[0].selbcolor = selbcolor;
	}
	for(i = 0; i < count; i ++)
	{
		if(!showhidden && (info[i].attr & FAT_FILEATTR_HIDDEN) > 0)
			continue;
		if(cur_count % 256 == 0)
		{
			if(cur_count == 0)
				* mitem = (p_win_menuitem)malloc(sizeof(t_win_menuitem) * 256);
			else
				* mitem = (p_win_menuitem)realloc(*mitem, sizeof(t_win_menuitem) * (cur_count + 256));
			if(* mitem == NULL)
			{
				free((void *)info);
				return 0;
			}
			item = *mitem;
		}
		if(info[i].attr & FAT_FILEATTR_DIRECTORY)
		{
			item[cur_count].data = (void *)fs_filetype_dir;
			strcpy(item[cur_count].shortname, info[i].filename);
			strcpy(item[cur_count].compname, info[i].longname);
			item[cur_count].name[0] = '<';
			if((item[cur_count].width = strlen(info[i].longname) + 2) > MAX_ITEM_NAME_LEN)
			{
				strncpy(&item[cur_count].name[1], info[i].longname, MAX_ITEM_NAME_LEN - 5);
				item[cur_count].name[MAX_ITEM_NAME_LEN - 4] = item[cur_count].name[MAX_ITEM_NAME_LEN - 3] = item[cur_count].name[MAX_ITEM_NAME_LEN - 2] = '.';
				item[cur_count].name[MAX_ITEM_NAME_LEN - 1] = '>';
				item[cur_count].name[MAX_ITEM_NAME_LEN] = 0;
				item[cur_count].width = MAX_ITEM_NAME_LEN;
			}
			else
			{
				strncpy(&item[cur_count].name[1], info[i].longname, MAX_ITEM_NAME_LEN);
				item[cur_count].name[item[cur_count].width - 1] = '>';
				item[cur_count].name[item[cur_count].width] = 0;
			}
		}
		else
		{
			if(info[i].filesize == 0)
				continue;
			t_fs_filetype ft = fs_file_get_type(info[i].longname);
			if(!showunknown && ft == fs_filetype_unknown)
				continue;
			item[cur_count].data = (void *)ft;
			strcpy(item[cur_count].shortname, info[i].filename);
			strcpy(item[cur_count].compname, info[i].longname);
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
		cur_count ++;
	}
	free((void *)info);
	return cur_count;
}

extern dword fs_zip_to_menu(const char * zipfile, p_win_menuitem * mitem, dword icolor, dword selicolor, dword selrcolor, dword selbcolor)
{
	unzFile unzf = unzOpen(zipfile);
	p_win_menuitem item = NULL;
	if(* mitem != NULL)
	{
		free((void *)* mitem);
		* mitem = NULL;
	}
	if(unzf == NULL)
		return 0;
	dword cur_count = 1;
	* mitem = (p_win_menuitem)malloc(sizeof(t_win_menuitem) * 256);
	if(* mitem == NULL)
	{
		unzClose(unzf);
		return 0;
	}
	item = * mitem;
	strcpy(item[0].name, "<..>");
	strcpy(item[0].compname, "..");
	item[0].data = (void *)fs_filetype_dir;
	item[0].width = 4;
	item[0].selected = false;
	item[0].icolor = icolor;
	item[0].selicolor = selicolor;
	item[0].selrcolor = selrcolor;
	item[0].selbcolor = selbcolor;
	if(unzGoToFirstFile(unzf) != UNZ_OK)
	{
		unzClose(unzf);
		return 1;
	}
	do {
		char fname[256];
		unz_file_info file_info;
		if(unzGetCurrentFileInfo(unzf, &file_info, fname, 256, NULL, 0, NULL, 0) != UNZ_OK)
			break;
		if(file_info.uncompressed_size == 0)
			continue;
		t_fs_filetype ft = fs_file_get_type(fname);
		if(ft == fs_filetype_chm || ft == fs_filetype_zip || ft == fs_filetype_rar)
			continue;
		if(cur_count % 256 == 0)
		{
			* mitem = (p_win_menuitem)realloc(*mitem, sizeof(t_win_menuitem) * (cur_count + 256));
			if(* mitem == NULL)
				return 0;
			item = *mitem;
		}
		item[cur_count].data = (void *)ft;
		strcpy(item[cur_count].compname, fname);
		sprintf(item[cur_count].shortname, "%u", (unsigned int)file_info.uncompressed_size);
		filename_to_itemname(item, cur_count, fname);
		item[cur_count].selected = false;
		item[cur_count].icolor = icolor;
		item[cur_count].selicolor = selicolor;
		item[cur_count].selrcolor = selrcolor;
		item[cur_count].selbcolor = selbcolor;
		cur_count ++;
	} while(unzGoToNextFile(unzf) == UNZ_OK);
	unzClose(unzf);
	return cur_count;
}

extern dword fs_rar_to_menu(const char * rarfile, p_win_menuitem * mitem, dword icolor, dword selicolor, dword selrcolor, dword selbcolor)
{
	p_win_menuitem item = NULL;
	if(* mitem != NULL)
	{
		free((void *)* mitem);
		* mitem = NULL;
	}
	struct RAROpenArchiveData arcdata;
	arcdata.ArcName = (char *)rarfile;
	arcdata.OpenMode = RAR_OM_LIST;
	arcdata.CmtBuf = NULL;
	arcdata.CmtBufSize = 0;
	HANDLE hrar = RAROpenArchive(&arcdata);
	if(hrar == 0)
		return 0;
	dword cur_count = 1;
	* mitem = (p_win_menuitem)malloc(sizeof(t_win_menuitem) * 256);
	if(* mitem == NULL)
	{
		RARCloseArchive(hrar);
		return 0;
	}
	item = * mitem;
	strcpy(item[0].name, "<..>");
	strcpy(item[0].compname, "..");
	item[0].data = (void *)fs_filetype_dir;
	item[0].width = 4;
	item[0].selected = false;
	item[0].selected = false;
	item[0].icolor = icolor;
	item[0].selicolor = selicolor;
	item[0].selrcolor = selrcolor;
	item[0].selbcolor = selbcolor;
	struct RARHeaderDataEx header;
	do
	{
		if(RARReadHeaderEx(hrar, &header) != 0)
			break;
		if(header.UnpSize == 0)
			continue;
		t_fs_filetype ft = fs_file_get_type(header.FileName);
		if(ft == fs_filetype_chm || ft == fs_filetype_zip || ft == fs_filetype_rar)
			continue;
		if(cur_count % 256 == 0)
		{
			* mitem = (p_win_menuitem)realloc(*mitem, sizeof(t_win_menuitem) * (cur_count + 256));
			if(* mitem == NULL)
				return 0;
			item = *mitem;
		}
		item[cur_count].data = (void *)ft;
		if(header.Flags & 0x200) {
			char str[1024];
			memset(str, 0, 1024);
			const byte *uni = (byte*) header.FileNameW;
			charsets_utf32_conv(uni, (byte*)str);
			strncpy(item[cur_count].compname, header.FileName, 256);
			item[cur_count].compname[255] = '\0';
			filename_to_itemname(item, cur_count, str);
		}
		else {
			strncpy(item[cur_count].compname, header.FileName, 256);
			item[cur_count].compname[255] = '\0';
			filename_to_itemname(item, cur_count, header.FileName);
		}
		sprintf(item[cur_count].shortname, "%u", (unsigned int)header.UnpSize);
		item[cur_count].selected = false;
		item[cur_count].icolor = icolor;
		item[cur_count].selicolor = selicolor;
		item[cur_count].selrcolor = selrcolor;
		item[cur_count].selbcolor = selbcolor;
		cur_count ++;
	}
	while(RARProcessFile(hrar, RAR_SKIP, NULL, NULL) == 0);

	RARCloseArchive(hrar);
	return cur_count;
}

typedef struct {
	p_win_menuitem * mitem;
	dword cur_count;
	dword icolor;
	dword selicolor;
	dword selrcolor;
	dword selbcolor;
} t_fs_chm_enum, *p_fs_chm_enum;

static int chmEnum(struct chmFile *h, struct chmUnitInfo *ui, void *context)
{
	p_win_menuitem * mitem = ((p_fs_chm_enum)context)->mitem;
	p_win_menuitem item = *mitem;

	int size = strlen(ui->path);
	if(size == 0 || ui->path[size - 1] == '/')
		return CHM_ENUMERATOR_CONTINUE;

	t_fs_filetype ft = fs_file_get_type(ui->path);
	if(ft == fs_filetype_chm || ft == fs_filetype_zip || ft == fs_filetype_rar)
		return CHM_ENUMERATOR_CONTINUE;

	dword cur_count = ((p_fs_chm_enum)context)->cur_count;
	if(cur_count % 256 == 0)
	{
		* mitem = (p_win_menuitem)realloc(*mitem, sizeof(t_win_menuitem) * (cur_count + 256));
		if(* mitem == NULL)
		{
			((p_fs_chm_enum)context)->cur_count = 0;
			return CHM_ENUMERATOR_FAILURE;
		}
		item = *mitem;
	}

	char fname[256] = "";

	strncpy(item[cur_count].compname, ui->path, 256);
	item[cur_count].compname[255] = '\0';
	sprintf(item[cur_count].shortname, "%u", (unsigned int)ui->length);
	if(ui->path[0] == '/') {
		strncpy(fname, ui->path + 1, 256);
		fname[255] = '\0';
	}
	else {
		strncpy(fname, ui->path, 256);
		fname[255] = '\0';
	}
	
	// CHM Dir in UTF-8 Format???
	char fname_temp[256] = "";
	strcpy(fname_temp, fname);
	charsets_utf8_conv((unsigned char*)fname_temp, (unsigned char*)fname);

	item[cur_count].data = (void *)ft;
	filename_to_itemname(item, cur_count, fname);
	item[cur_count].selected = false;
	item[cur_count].icolor = ((p_fs_chm_enum)context)->icolor;
	item[cur_count].selicolor = ((p_fs_chm_enum)context)->selicolor;
	item[cur_count].selrcolor = ((p_fs_chm_enum)context)->selrcolor;
	item[cur_count].selbcolor = ((p_fs_chm_enum)context)->selbcolor;
	((p_fs_chm_enum)context)->cur_count ++;
	return CHM_ENUMERATOR_CONTINUE;
}

extern dword fs_chm_to_menu(const char * chmfile, p_win_menuitem * mitem, dword icolor, dword selicolor, dword selrcolor, dword selbcolor)
{
	struct chmFile * chm = chm_open(chmfile);
	p_win_menuitem item = NULL;
	if(* mitem != NULL)
	{
		free((void *)* mitem);
		* mitem = NULL;
	}
	if(chm == NULL)
		return 0;
	* mitem = (p_win_menuitem)malloc(sizeof(t_win_menuitem) * 256);
	if(* mitem == NULL)
	{
		chm_close(chm);
		return 0;
	}
	item = * mitem;
	strcpy(item[0].name, "<..>");
	strcpy(item[0].compname, "..");
	item[0].data = (void *)fs_filetype_dir;
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
	chm_enumerate(chm, CHM_ENUMERATE_NORMAL | CHM_ENUMERATE_FILES, chmEnum, (void *)&cenum);

	chm_close(chm);
	return cenum.cur_count;
}

extern t_fs_filetype fs_file_get_type(const char * filename)
{
	const char * ext = utils_fileext(filename);
	t_fs_filetype_entry * entry = ft_table;
	t_fs_specfiletype_entry * entry2 = ft_spec_table;
	if(ext)
	{
		while(entry->ext != NULL)
		{
			if(stricmp(ext, entry->ext) == 0)
				return entry->ft;
			entry ++;
		}
	}
	while(entry2->fname != NULL)
	{
		const char* shortname = strrchr(filename, '/');
		if(!shortname)
			shortname = filename;
		else
			shortname++;
		if(stricmp(shortname, entry2->fname) == 0)
			return entry2->ft;
		entry2 ++;
	}
	return fs_filetype_unknown;
}

#ifdef ENABLE_IMAGE
extern bool fs_is_image(t_fs_filetype ft)
{
	return ft == fs_filetype_jpg || ft == fs_filetype_gif || ft == fs_filetype_png || ft == fs_filetype_tga || ft == fs_filetype_bmp;
}
#endif

extern bool fs_is_txtbook(t_fs_filetype ft)
{
	return ft == fs_filetype_txt || ft == fs_filetype_html || ft == fs_filetype_gz;
}
