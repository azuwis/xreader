/*
 * $Id: Filesys.cpp 77 2007-11-05 09:21:58Z soarchin $

 * Copyright 2007 aeolusc

 * This file is part of eReader2.

 * eReader2 is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.

 * eReader2 is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "Filesys.h"
#include <string.h>
#include <pspkernel.h>
#include <stdio.h>
#include "Charsets.h"
#include "Log.h"
	
struct _FatMbrDpt {
	BYTE active;
	BYTE start[3];
	BYTE id;
	BYTE ending[3];
	DWORD start_sec;
	DWORD total_sec;
} __attribute__((packed));

struct _FatMbr {
	BYTE mb_data[0x1BE];
	struct _FatMbrDpt dpt[4];
	WORD ending_flag;
} __attribute__((packed));

struct _FatDbr {
	BYTE jmp_boot[3];
	CHAR oem_name[8];
	WORD bytes_per_sec;
	BYTE sec_per_clus;
	WORD reserved_sec;
	BYTE num_fats;
	WORD root_entry;
	WORD total_sec;
	BYTE media_type;
	WORD sec_per_fat;
	WORD sec_per_track;
	WORD heads;
	DWORD hidd_sec;
	DWORD big_total_sec;
	union {
		struct {
			BYTE drv_num;
			BYTE reserved;
			BYTE boot_sig;
			BYTE vol_id[4];
			CHAR vol_lab[11];
			CHAR file_sys_type[8];
		} __attribute__((packed)) fat16;
		struct {
			DWORD sec_per_fat;
			WORD extend_flag;
			WORD sys_ver;
			DWORD root_clus;
			WORD info_sec;
			WORD back_sec;
			BYTE reserved[10];
		} __attribute__((packed)) fat32;
	} __attribute__((packed)) ufat;
	BYTE exe_code[448];
	WORD ending_flag;
} __attribute__((packed));

struct _FatFileEntry {
	CHAR filename[8];
	CHAR fileext[3];
	BYTE attr;
	BYTE flag;
	BYTE cr_time_msec;
	WORD cr_time;
	WORD cr_date;
	WORD last_visit;
	WORD clus_high;
	WORD last_mod_time;
	WORD last_mod_date;
	WORD clus;
	DWORD filesize;
} __attribute__((packed));

struct _FatLongFile {
	BYTE order;
	WORD uni_name[5];
	BYTE sig;
	BYTE reserved;
	BYTE checksum;
	WORD uni_name2[6];
	WORD clus;
	WORD uni_name3[2];
} __attribute__((packed));

union _FatEntry {
	struct _FatFileEntry file;
	struct _FatLongFile longfile;
} __attribute__((packed));


class FAT
{
public:
	FAT(_FatMbr& mbr, _FatDbr& dbr)
	{
		memcpy(&m_mbr, &mbr, sizeof(mbr));
		memcpy(&m_dbr, &dbr, sizeof(dbr));
		m_fatfd = sceIoOpen("msstor:", PSP_O_RDONLY, 0777);
		if(m_fatfd < 0)
			return;
		m_bpc = m_dbr.sec_per_clus * m_dbr.bytes_per_sec;
		m_fattable = NULL;
		m_fatloaded = FALSE;
		m_sema = sceKernelCreateSema("FAT sema", 0, 1, 1, NULL);
	}
	virtual ~FAT()
	{
		if(m_fatfd >= 0)
			sceIoClose(m_fatfd);
		FreeFatTable();
		sceKernelDeleteSema(m_sema);
	}
	STATIC BOOL Init();
	DWORD FatTable(DWORD index)
	{
		if(!m_fatloaded)
		{
			m_fatloaded = TRUE;
			LoadFatTable();
		}
		return _FatTable(index);
	}
	BOOL Lock()
	{
		return sceKernelWaitSema(m_sema, 1, NULL) >= 0;
	}
	VOID Unlock()
	{
		sceKernelSignalSema(m_sema, 1);
	}
	DWORD LocateDir(CONST CHAR * dir)
	{
		CHAR rdir[256];
		strcpy(rdir, dir);
		CHAR * pdir = strtok(rdir, "/");
		if(pdir == NULL || (strncmp(pdir, "ms", 2) != 0 && strncmp(pdir, "fatms", 5) != 0))
			return 0;
		DWORD clus = 1;
		while(TRUE)
		{
			pdir = strtok(NULL, "/");
			if(pdir == NULL || pdir[0] == 0)
				break;
			if(!FindEntry(clus, pdir))
				return 0;
		}
		return clus;
	}
	BOOL LocateFile(CONST CHAR * filename, _FatEntry& entry)
	{
		CHAR dir[256];
		CHAR * pstr = strrchr(filename, '/');
		if(pstr == NULL)
			return FALSE;
		strncpy(dir, filename, pstr - filename);
		dir[pstr - filename] = 0;
		pstr ++;
		DWORD clus = LocateDir(dir);
		
		DWORD count;
		_FatEntry * elist;
		count = ReadEntryList(clus, elist);
		if(count <= 0)
			return FALSE;
		CHAR shortname[11];
		WORD longname[256];
		BOOL testshort = FALSE;
		CONST CHAR * dot = strchr(pstr, '.');
		if(dot == NULL)
		{
			if(strlen(pstr) < 9)
				testshort = TRUE;
		}
		else
		{
			if(dot - pstr < 9 && strlen(dot + 1) < 4)
				testshort = TRUE;
		}
		if(testshort)
		{
			memset(shortname, ' ', 11);
			if(dot != NULL)
			{
				strncpy(shortname, pstr, dot - pstr);
				strcpy(&shortname[8], dot + 1);
			}
			else
				strncpy(shortname, pstr, strlen(pstr));
			if((BYTE)shortname[0] == 0xE5)
				shortname[0] = 0x05;
		}
		WORD * lname = NULL;
		GBK.ProcessStr(pstr, lname);
		for(DWORD i = 0; i < count; i ++)
		{
			if((elist[i].file.attr & 0x0F) == 0x0F || (elist[i].file.attr & 0x18) > 0)
				continue;
			if(testshort && strncasecmp((CONST CHAR *)&elist[i].file.filename[0], shortname, 11) == 0)
			{
				memcpy(&entry, &elist[i], sizeof(_FatEntry));
				delete[] elist;
				delete[] lname;
				return TRUE;
			}
			if(GetLongname(elist, i, longname) && UCS.ICompare((CONST CHAR *)longname, (CONST CHAR *)lname) == 0)
			{
				memcpy(&entry, &elist[i], sizeof(_FatEntry));
				delete[] elist;
				delete[] lname;
				return TRUE;
			}
		}
		delete[] elist;
		delete[] lname;
		return FALSE;
	}
	BOOL ReadClus(DWORD index, BYTE * data, INT handle = -1)
	{
		if(handle >= 0)
		{
			sceIoLseek(handle, m_dataoff + m_bpc * ((UINT64)index - 2ull), PSP_SEEK_SET);
			sceIoRead(handle, data, m_bpc);
		}
		else
		{
			sceIoLseek(m_fatfd, m_dataoff + m_bpc * ((UINT64)index - 2ull), PSP_SEEK_SET);
			sceIoRead(m_fatfd, data, m_bpc);
		}
		return TRUE;
	}
protected:
	friend class Dir;
	friend class FATFile;
	_FatMbr m_mbr;
	_FatDbr m_dbr;
	DWORD m_clusmax;
	UINT64 m_dataoff, m_rootoff, m_bpc;
	INT m_fatfd;
	BYTE * m_fattable;
	BOOL m_fatloaded;
	INT m_sema;
	virtual DWORD _FatTable(DWORD index) = 0;
	virtual VOID LoadFatTable() = 0;
	virtual DWORD GetRootEntry(_FatEntry *& elist) = 0;
	virtual DWORD GetEntryClus(_FatEntry& entry) = 0;
	virtual VOID FreeFatTable()
	{
		if(m_fattable != NULL)
		{
			delete[] m_fattable;
			m_fattable = NULL;
			m_fatloaded = FALSE;
		}
	}
	__inline BYTE _CalcChksum(_FatEntry * entry)
	{
		BYTE * n = (BYTE *)&entry->file.filename[0];
		BYTE chksum = 0;
		for(INT i = 0; i < 11; i ++)
			chksum = ((chksum & 1) ? 0x80 : 0) + (chksum >> 1) + n[i];
		return chksum;
	}
	BOOL GetLongname(_FatEntry * entrys, DWORD cur, WORD * longname)
	{
		WORD chksum = _CalcChksum(&entrys[cur]);
		DWORD j = cur;
		memset(longname, 0, 256 * sizeof(WORD));
		while(j > 0)
		{
			j --;
			if((entrys[j].file.attr & 0x0F) != 0x0F || entrys[j].longfile.checksum != chksum || entrys[j].file.filename[0] == 0 || (BYTE)entrys[j].file.filename[0] == 0xE5)
				return FALSE;
			BYTE order = entrys[j].longfile.order & 0x3F;
			if(order > 20)
				return FALSE;
			DWORD ppos = ((DWORD)(order - 1)) * 13;
			INT k;
			for(k = 0; k < 5; k ++)
				longname[ppos ++] = entrys[j].longfile.uni_name[k];
			for(k = 0; k < 6; k ++)
				longname[ppos ++] = entrys[j].longfile.uni_name2[k];
			for(k = 0; k < 2; k ++)
				longname[ppos ++] = entrys[j].longfile.uni_name3[k];
			if((entrys[j].longfile.order & 0x40) > 0)
				break;
		}
		if((entrys[j].file.attr & 0x0F) != 0x0F || (entrys[j].longfile.order & 0x40) == 0)
			return FALSE;
		return TRUE;
	}
	VOID GetFilename(_FatEntry * entrys, DWORD cur, WORD * longname)
	{
		if(GetLongname(entrys, cur, longname))
			return;
		DWORD pos = 0;
		for(DWORD i = 0; i < 8 && entrys[cur].file.filename[i] != ' '; i ++)
			longname[pos ++] = entrys[cur].file.filename[i];
		if(entrys[cur].file.filename[8] != ' ' || entrys[cur].file.filename[9] != ' ' || entrys[cur].file.filename[10] != ' ')
		{
			longname[pos ++] = '.';
			for(DWORD i = 8; i < 11 && entrys[cur].file.filename[i] != ' '; i ++)
				longname[pos ++] = entrys[cur].file.filename[i];
		}
		longname[pos] = 0;
		if(longname[0] == 5) longname[0] = 0xE5;
	}
	VOID GetShortname(_FatEntry * entrys, DWORD cur, CHAR * longname)
	{
		DWORD pos = 0;
		for(DWORD i = 0; i < 8 && entrys[cur].file.filename[i] != ' '; i ++)
			longname[pos ++] = entrys[cur].file.filename[i];
		if(entrys[cur].file.filename[8] != ' ' || entrys[cur].file.filename[9] != ' ' || entrys[cur].file.filename[10] != ' ')
		{
			longname[pos ++] = '.';
			for(DWORD i = 8; i < 11 && entrys[cur].file.filename[i] != ' '; i ++)
				longname[pos ++] = entrys[cur].file.filename[i];
		}
		longname[pos] = 0;
		if(longname[0] == 5) longname[0] = 0xE5;
	}
	DWORD ReadEntryList(DWORD clus, _FatEntry *& ent)
	{
		if(clus == 1)
			return GetRootEntry(ent);
		DWORD c2 = clus;
		if(c2 < 1 || c2 >= m_clusmax)
			return 0;
		DWORD count = 1;
		while(1)
		{
			c2 = FatTable(c2);
			if(c2 < 2 || c2 >= m_clusmax)
				break;
			count ++;
		}
		DWORD ces = m_bpc / sizeof(_FatEntry);
		ent = new _FatEntry[ces * count];
		c2 = clus;
		for(DWORD i = 0; i < count; i ++)
		{
			sceIoLseek(m_fatfd, m_dataoff + m_bpc * ((UINT64)c2 - 2ull), PSP_SEEK_SET);
			sceIoRead(m_fatfd, &ent[ces * i], m_bpc);
			c2 = FatTable(c2);
		}
		return count * ces;
	}
	virtual BOOL FindEntry(DWORD& clus, CONST CHAR * dir)
	{
		DWORD count;
		_FatEntry * elist;
		count = ReadEntryList(clus, elist);
		if(count <= 0)
			return FALSE;
		CHAR shortname[11];
		WORD longname[256];
		BOOL testshort = FALSE;
		CONST CHAR * dot = strchr(dir, '.');
		if(dot == NULL)
		{
			if(strlen(dir) < 9)
				testshort = TRUE;
		}
		else
		{
			if(dot - dir < 9 && strlen(dot + 1) < 4)
				testshort = TRUE;
		}
		if(testshort)
		{
			memset(shortname, ' ', 11);
			if(dot != NULL)
			{
				strncpy(shortname, dir, dot - dir);
				strcpy(&shortname[8], dot + 1);
			}
			else
				strncpy(shortname, dir, strlen(dir));
			if((BYTE)shortname[0] == 0xE5)
				shortname[0] = 0x05;
		}
		WORD * lname = NULL;
		GBK.ProcessStr(dir, lname);
		for(DWORD i = 0; i < count; i ++)
		{
			if((elist[i].file.attr & 0x0F) == 0x0F || (elist[i].file.attr & 0x10) == 0)
				continue;
			if(testshort && strncasecmp((CONST CHAR *)&elist[i].file.filename[0], shortname, 11) == 0)
			{
				clus = GetEntryClus(elist[i]);
				delete[] elist;
				delete[] lname;
				return TRUE;
			}
			if(GetLongname(elist, i, longname) && UCS.ICompare((CONST CHAR *)longname, (CONST CHAR *)lname) == 0)
			{
				clus = GetEntryClus(elist[i]);
				delete[] elist;
				delete[] lname;
				return TRUE;
			}
		}
		delete[] elist;
		delete[] lname;
		return FALSE;
	}
};

class FAT12: public FAT
{
public:
	FAT12(_FatMbr& mbr, _FatDbr& dbr):FAT(mbr, dbr)
	{
		m_clusmax = 0x0FF0;
		m_rootoff = m_mbr.dpt[0].start_sec * 0x200 + (m_dbr.num_fats * m_dbr.sec_per_fat + m_dbr.reserved_sec) * m_dbr.bytes_per_sec;
		m_dataoff = m_rootoff + ((m_dbr.root_entry * sizeof(_FatEntry) + m_dbr.bytes_per_sec - 1) / m_dbr.bytes_per_sec) * m_dbr.bytes_per_sec;
	}
	virtual ~FAT12()
	{
	}
protected:
	virtual VOID LoadFatTable()
	{
		DWORD size = m_dbr.sec_per_fat * m_dbr.bytes_per_sec;
		sceIoLseek(m_fatfd, (UINT64)m_mbr.dpt[0].start_sec * 0x200ull + (UINT64)m_dbr.reserved_sec * (UINT64)m_dbr.bytes_per_sec, PSP_SEEK_SET);
		m_fattable = new BYTE[size];
		sceIoRead(m_fatfd, m_fattable, size);
	}
	virtual DWORD _FatTable(DWORD index)
	{
		if((index & 1) == 0)
		{
			return (*(WORD *)&m_fattable[index * 3 / 2]) & 0x0FFF;
		}
		else
		{
			return (*(WORD *)&m_fattable[index * 3 / 2]) >> 8;
		}
	}
	virtual DWORD GetRootEntry(_FatEntry *& elist)
	{
		DWORD count = m_dbr.root_entry;
		if (count > 0)
		{
			elist = new _FatEntry[count];
			sceIoLseek(m_fatfd, m_rootoff, PSP_SEEK_SET);
			sceIoRead(m_fatfd, elist, sizeof(_FatEntry) * count);
		}
		return count;
	}
	virtual DWORD GetEntryClus(_FatEntry& entry)
	{
		return entry.file.clus;
	}
};

class FAT16: public FAT12
{
public:
	FAT16(_FatMbr& mbr, _FatDbr& dbr):FAT12(mbr, dbr)
	{
		m_clusmax = 0xFFF0;
		m_rootoff = m_mbr.dpt[0].start_sec * 0x200 + (m_dbr.num_fats * m_dbr.sec_per_fat + m_dbr.reserved_sec) * m_dbr.bytes_per_sec;
		m_dataoff = m_rootoff + ((m_dbr.root_entry * sizeof(_FatEntry) + m_dbr.bytes_per_sec - 1) / m_dbr.bytes_per_sec) * m_dbr.bytes_per_sec;
	}
	virtual ~FAT16()
	{
	}
protected:
	virtual DWORD _FatTable(DWORD index)
	{
		return *(WORD *)&m_fattable[index * 2];
	}
};

class FAT32: public FAT
{
public:
	FAT32(_FatMbr& mbr, _FatDbr& dbr):FAT(mbr, dbr)
	{
		m_clusmax = 0x0FFFFFF0;
		m_dataoff = m_mbr.dpt[0].start_sec * 0x200 + (m_dbr.ufat.fat32.sec_per_fat * m_dbr.num_fats + m_dbr.reserved_sec) * m_dbr.bytes_per_sec;
		m_rootoff = m_dataoff + m_bpc * m_dbr.ufat.fat32.root_clus;
	}
	virtual ~FAT32()
	{
	}
protected:
	virtual VOID LoadFatTable()
	{
		DWORD size = m_dbr.ufat.fat32.sec_per_fat * m_dbr.bytes_per_sec;
		sceIoLseek(m_fatfd, (UINT64)m_mbr.dpt[0].start_sec * 0x200ull + (UINT64)m_dbr.reserved_sec * (UINT64)m_dbr.bytes_per_sec, PSP_SEEK_SET);
		m_fattable = new BYTE[size];
		sceIoRead(m_fatfd, m_fattable, size);
	}
	virtual DWORD _FatTable(DWORD index)
	{
		return *(DWORD *)&m_fattable[index * 4];
	}
	virtual DWORD GetRootEntry(_FatEntry *& elist)
	{
		return ReadEntryList(m_dbr.ufat.fat32.root_clus, elist);
	}
	virtual DWORD GetEntryClus(_FatEntry& entry)
	{
		return ((DWORD)entry.file.clus) + (((DWORD)entry.file.clus_high) << 16);
	}
};

FAT * sFat = NULL;

BOOL FAT::Init()
{
	_FatMbr mbr;
	_FatDbr dbr;
	INT fatfd = sceIoOpen("msstor:", PSP_O_RDONLY, 0777);
	if(fatfd < 0)
		return FALSE;
	sceIoRead(fatfd, &mbr, sizeof(mbr));
	sceIoLseek(fatfd, (UINT64)mbr.dpt[0].start_sec * 0x200ull, PSP_SEEK_SET);
	sceIoRead(fatfd, &dbr, sizeof(dbr));
	sceIoClose(fatfd);
	if(dbr.root_entry == 0)
	{
		sFat = new FAT32(mbr, dbr);
	}
	else if(mbr.dpt[0].id == 1)
	{
		sFat = new FAT12(mbr, dbr);
	}
	else
	{
		sFat = new FAT16(mbr, dbr);
	}
	return TRUE;
}

Dir::Dir()
{
}

Dir::~Dir()
{
}

BOOL Dir::Init()
{
	return FAT::Init();
}

VOID Dir::Release()
{
	sFat->Lock();
	delete sFat;
	sFat = NULL;
}

VOID Dir::PowerDown()
{
	sFat->Lock();
	sceIoClose(sFat->m_fatfd);
}

VOID Dir::PowerUp()
{
	sFat->m_fatfd = sceIoOpen("msstor:", PSP_O_RDONLY, 0777);
	sFat->Unlock();
}

DWORD Dir::ReadDir(CONST CHAR * path, FileInfo *& info)
{
	if(!sFat->Lock())
		return 0;
	sFat->FreeFatTable();
	DWORD clus;
	if((clus = sFat->LocateDir(path)) < 1)
	{
		sFat->Unlock();
		return 0;
	}
	_FatEntry * elist = NULL;
	DWORD size = sFat->ReadEntryList(clus, elist), count = 0;
	info = new FileInfo[size];
	for(DWORD i = 0; i < size; i ++)
	{
		if(elist[i].file.filename[0] != 0 && (BYTE)elist[i].file.filename[0] != 0xE5 && (elist[i].file.attr & 0x0F) != 0x0F)
		{
			sFat->GetFilename(elist, i, info[count].filename);
			sFat->GetShortname(elist, i, info[count].shortname);
			info[count].clus = sFat->GetEntryClus(elist[i]);
			info[count].attr = elist[i].file.attr;
			count ++;
		}
	}
	delete[] elist;
	sFat->Unlock();
	return count;
}

namespace eReader2space 
{
	File::File()
	{
		m_handle = -1;
	}

	BOOL File::Open(CONST CHAR * filename, FileOpenMode mode)
	{
		switch(mode)
		{
			case FO_READ:
				m_handle = sceIoOpen(filename, PSP_O_RDONLY, 0777);
				break;
			case FO_WRITE:
				if((m_handle = sceIoOpen(filename, PSP_O_RDWR, 0777)) < 0)
					m_handle = sceIoOpen(filename, PSP_O_CREAT | PSP_O_RDWR, 0777);
				break;
			case FO_CREATE:
				m_handle = sceIoOpen(filename, PSP_O_CREAT | PSP_O_RDWR, 0777);
				break;
			case FO_APPEND:
				m_handle = sceIoOpen(filename, PSP_O_APPEND | PSP_O_RDWR, 0777);
				break;
		}
		return (m_handle >= 0);
	}

	File::~File()
	{
		Close();
	}

	DWORD File::Read(VOID * data, DWORD size)
	{
		return sceIoRead(m_handle, data, size);
	}

	DWORD File::Write(VOID * data, DWORD size)
	{
		return sceIoWrite(m_handle, data, size);
	}

	DWORD File::GetSize()
	{
		DWORD orgidx = sceIoLseek(m_handle, 0, PSP_SEEK_CUR);
		DWORD res = sceIoLseek(m_handle, 0, PSP_SEEK_END);
		sceIoLseek(m_handle, orgidx, PSP_SEEK_SET);
		return res;
	}

	DWORD File::Tell()
	{
		return sceIoLseek(m_handle, 0, PSP_SEEK_CUR);
	}

	DWORD File::Seek(INT pos, FileSeekMode seek)
	{
		switch(seek)
		{
			case FS_BEGIN:
				return sceIoLseek(m_handle, pos, PSP_SEEK_SET);
			case FS_CURRENT:
				return sceIoLseek(m_handle, pos, PSP_SEEK_CUR);
			case FS_END:
				return sceIoLseek(m_handle, pos, PSP_SEEK_END);
		}
		return 0;
	}

	VOID File::Close()
	{
		if(m_handle >= 0)
		{
			sceIoClose(m_handle);
			m_handle = -1;
		}
	}
}

FATFile::FATFile()
{
	if(!sFat->Lock())
		return;
	m_cluslist.clear();
	m_size = 0;
	m_pos = 0;
	m_cached = FALSE;
	m_cache = new BYTE[sFat->m_bpc];
	m_handle = -1;
	sFat->Unlock();
}

FATFile::~FATFile()
{
	delete[] m_cache;
	Close();
}

BOOL FATFile::Open(CONST CHAR * filename)
{
	if(!sFat->Lock())
		return FALSE;
	m_cached = FALSE;
	sFat->FreeFatTable();

	_FatEntry entry;

	if(!sFat->LocateFile(filename, entry))
	{
		sFat->Unlock();
		return FALSE;
	}
	m_handle = sceIoOpen("msstor:", PSP_O_RDONLY, 0777);
	if(m_handle < 0)
	{
		sFat->Unlock();
		return FALSE;
	}
	DWORD clus = sFat->GetEntryClus(entry);
	while(clus > 1 && clus < sFat->m_clusmax)
	{
		m_cluslist.push_back(clus);
		clus = sFat->FatTable(clus);
	}
	m_size = entry.file.filesize;
	m_pos = 0;
	sFat->Unlock();
	return TRUE;
}

VOID FATFile::Close()
{
	sceIoClose(m_handle);
	m_handle = -1;
	m_size = m_pos = 0;
	m_cluslist.clear();
	m_cached = FALSE;
}

DWORD FATFile::Read(VOID * data, DWORD size)
{
	if(m_size == 0)
	{
		m_pos = 0;
		m_cached = FALSE;
		return 0;
	}
	if(!sFat->Lock())
		return 0;
	if(m_handle < 0)
		return 0;
	if(m_pos + size > m_size)
		size = m_size - m_pos;
	DWORD bpc = sFat->m_bpc;
	DWORD clus = m_pos / bpc;
	DWORD copied = (clus + 1) * bpc - m_pos;

	if(copied > size)
		copied = size;
	if(!m_cached)
	{
		sFat->ReadClus(m_cluslist.at(m_pos / bpc), m_cache, m_handle);
		m_cached = TRUE;
	}
	memcpy(data, &m_cache[m_pos - clus * bpc], copied);
	m_pos += copied;
	while(copied + bpc < size)
	{
		sFat->ReadClus(m_cluslist.at(m_pos / bpc), ((BYTE *)data) + copied);
		copied += bpc;
		m_pos += bpc;
	}
	if(copied < size)
	{
		sFat->ReadClus(m_cluslist.at(m_pos / bpc), m_cache, m_handle);
		memcpy(((BYTE *)data) + copied, m_cache, size - copied);
		m_pos += size - copied;
	}
	if((m_pos % sFat->m_bpc) == 0)
		m_cached = FALSE;
	sFat->Unlock();
	return size;
}

DWORD FATFile::GetSize()
{
	return m_size;
}

DWORD FATFile::Tell()
{
	return m_pos;
}

DWORD FATFile::Seek(INT pos, FileSeekMode seek)
{
	INT temppos = 0;
	switch(seek)
	{
		case FS_BEGIN:
			temppos = pos;
			break;
		case FS_CURRENT:
			temppos = (INT)m_pos;
			temppos += pos;
			break;
		case FS_END:
			temppos = m_size;
			temppos += pos;
			break;
	}
	if(temppos < 0)
		temppos = 0;
	else if(temppos > (INT)m_size)
		temppos = m_size;
	if(!sFat->Lock())
		return 0;
	if(m_cached && (temppos / sFat->m_bpc) != ((INT)m_pos / sFat->m_bpc))
		m_cached = FALSE;
	sFat->Unlock();
	m_pos = temppos;
	return m_pos;
}
