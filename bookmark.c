/* vim:set ts=4 sw=4 cindent ignorecase enc=gbk: */

#include "config.h"

#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <pspkernel.h>
#include "common/utils.h"
#include "bookmark.h"

struct _bm_index
{
	dword flag;
	dword hash[32];
} __attribute__ ((packed));
typedef struct _bm_index t_bm_index, *p_bm_index;
static dword flagbits[32] = {
	0x00000001, 0x00000002, 0x00000004, 0x00000008, 0x00000010, 0x00000020,
	0x00000040, 0x00000080,
	0x00000100, 0x00000200, 0x00000400, 0x00000800, 0x00001000, 0x00002000,
	0x00004000, 0x00008000,
	0x00010000, 0x00020000, 0x00040000, 0x00080000, 0x00100000, 0x00200000,
	0x00400000, 0x00800000,
	0x01000000, 0x02000000, 0x04000000, 0x08000000, 0x10000000, 0x20000000,
	0x40000000, 0x80000000,
};

static char bmfile[256];

extern dword bookmark_encode(const char *filename)
{
	register dword h;

	for (h = 5381; *filename != 0; ++filename) {
		h += h << 5;
		h ^= *filename;
	}

	return h;
}

extern void bookmark_init(const char *fn)
{
	STRCPY_S(bmfile, fn);
}

static p_bookmark bookmark_open_hash(dword hash)
{
	int fd;
	p_bookmark bm = (p_bookmark) calloc(1, sizeof(t_bookmark));

	if (bm == NULL)
		return NULL;
	memset(bm->row, 0xFF, 10 * sizeof(dword));
	bm->index = INVALID;
	bm->hash = hash;
	fd = sceIoOpen(bmfile, PSP_O_RDONLY, 0777);
	if (fd < 0)
		return bm;
	dword count;

	if (sceIoRead(fd, &count, sizeof(dword)) < sizeof(dword)) {
		sceIoClose(fd);
		return bm;
	}
	t_bm_index bi;
	dword i, j;

	for (i = 0; i < count; i++) {
		if (sceIoRead(fd, &bi, sizeof(t_bm_index)) < sizeof(t_bm_index)) {
			sceIoClose(fd);
			return bm;
		}
		for (j = 0; j < 32; j++)
			if ((bi.flag & flagbits[j]) > 0 && bi.hash[j] == bm->hash) {
				bm->index = i * 32 + j;
				sceIoLseek(fd, j * 10 * sizeof(dword), PSP_SEEK_CUR);
				sceIoRead(fd, &bm->row[0], 10 * sizeof(dword));
				sceIoClose(fd);
				return bm;
			}
		sceIoLseek(fd, 32 * 10 * sizeof(dword), PSP_SEEK_CUR);
	}
	sceIoClose(fd);
	return bm;
}

extern p_bookmark bookmark_open(const char *filename)
{
	if (!filename)
		return NULL;
	return bookmark_open_hash(bookmark_encode(filename));
}

extern void bookmark_save(p_bookmark bm)
{
	if (!bm)
		return;
	int fd = sceIoOpen(bmfile, PSP_O_CREAT | PSP_O_RDWR, 0777);

	if (fd < 0)
		return;
	dword count;
	t_bm_index bi;

	if (sceIoRead(fd, &count, sizeof(dword)) < sizeof(dword)) {
		count = 1;
		sceIoLseek(fd, 0, PSP_SEEK_SET);
		sceIoWrite(fd, &count, sizeof(dword));
		memset(&bi, 0, sizeof(t_bm_index));
		sceIoWrite(fd, &bi, sizeof(t_bm_index));
		dword temp[32 * 10];

		memset(temp, 0, 32 * 10 * sizeof(dword));
		sceIoWrite(fd, temp, 32 * 10 * sizeof(dword));
	}
	if (bm->index == INVALID) {
		sceIoLseek(fd,
				   sizeof(dword) + (count - 1) * (sizeof(t_bm_index) +
												  32 * 10 * sizeof(dword)),
				   PSP_SEEK_SET);
		sceIoRead(fd, &bi, sizeof(t_bm_index));
		if (bi.flag != 0xFFFFFFFF) {
			dword j;

			for (j = 0; j < 32; j++)
				if ((bi.flag & flagbits[j]) == 0) {
					bi.flag |= flagbits[j];
					bi.hash[j] = bm->hash;
					bm->index = (count - 1) * 32 + j;
					sceIoLseek(fd,
							   sizeof(dword) + (count -
												1) * (sizeof(t_bm_index) +
													  32 * 10 * sizeof(dword)),
							   PSP_SEEK_SET);
					sceIoWrite(fd, &bi, sizeof(t_bm_index));
					sceIoLseek(fd, j * 10 * sizeof(dword), PSP_SEEK_CUR);
					sceIoWrite(fd, &bm->row[0], 10 * sizeof(dword));
					break;
				}
		} else {
			sceIoLseek(fd,
					   sizeof(dword) + count * (sizeof(t_bm_index) +
												32 * 10 * sizeof(dword)),
					   PSP_SEEK_SET);
			memset(&bi, 0, sizeof(t_bm_index));
			bi.flag = 1;
			bi.hash[0] = bm->hash;
			bm->index = count * 32;
			sceIoWrite(fd, &bi, sizeof(t_bm_index));
			sceIoWrite(fd, &bm->row[0], 10 * sizeof(dword));
			dword temp[31 * 10];

			memset(temp, 0, sizeof(temp));
			sceIoWrite(fd, temp, 31 * 10 * sizeof(dword));
			sceIoLseek(fd, 0, PSP_SEEK_SET);
			count++;
			sceIoWrite(fd, &count, sizeof(dword));
		}
	} else {
		sceIoLseek(fd,
				   sizeof(dword) + (bm->index / 32) * (sizeof(t_bm_index) +
													   32 * 10 *
													   sizeof(dword)) +
				   sizeof(t_bm_index) + ((bm->index % 32) * 10 * sizeof(dword)),
				   PSP_SEEK_SET);
		sceIoWrite(fd, &bm->row[0], 10 * sizeof(dword));
	}
	sceIoClose(fd);
}

extern void bookmark_delete(p_bookmark bm)
{
	int fd = sceIoOpen(bmfile, PSP_O_RDWR, 0777);

	if (fd < 0)
		return;
	sceIoLseek(fd,
			   sizeof(dword) + (bm->index / 32) * (sizeof(t_bm_index) +
												   32 * 10 * sizeof(dword)),
			   PSP_SEEK_SET);
	t_bm_index bi;

	memset(&bi, 0, sizeof(t_bm_index));
	sceIoRead(fd, &bi, sizeof(t_bm_index));
	bi.flag &= ~flagbits[bm->index % 32];
	bi.hash[bm->index % 32] = 0;
	sceIoLseek(fd,
			   sizeof(dword) + (bm->index / 32) * (sizeof(t_bm_index) +
												   32 * 10 * sizeof(dword)),
			   PSP_SEEK_SET);
	sceIoWrite(fd, &bi, sizeof(t_bm_index));
	sceIoClose(fd);
}

extern void bookmark_close(p_bookmark bm)
{
	if (bm != NULL)
		free((void *) bm);
}

extern dword bookmark_autoload(const char *filename)
{
	dword row;
	p_bookmark bm = bookmark_open(filename);

	if (bm == NULL)
		return 0;
	if (bm->row[0] == INVALID)
		row = 0;
	else
		row = bm->row[0];
	bookmark_close(bm);
	return row;
}

extern void bookmark_autosave(const char *filename, dword row)
{
	if (!filename)
		return;
	extern bool scene_readbook_in_raw_mode;

	if (scene_readbook_in_raw_mode)
		return;
	p_bookmark bm = bookmark_open(filename);

	if (bm == NULL)
		return;
	bm->row[0] = row;
	bookmark_save(bm);
	bookmark_close(bm);
}

extern bool bookmark_export(p_bookmark bm, const char *filename)
{
	if (!bm || !filename)
		return false;
	int fd = sceIoOpen(filename, PSP_O_CREAT | PSP_O_WRONLY, 0777);

	if (fd < 0)
		return false;
	sceIoWrite(fd, &bm->hash, sizeof(dword));
	sceIoWrite(fd, &bm->row[0], 10 * sizeof(dword));
	sceIoClose(fd);
	return true;
}

extern bool bookmark_import(const char *filename)
{
	if (!filename)
		return false;
	int fd = sceIoOpen(filename, PSP_O_RDONLY, 0777);

	if (fd < 0)
		return false;
	dword hash;

	if (sceIoRead(fd, &hash, sizeof(dword)) < sizeof(dword)) {
		sceIoClose(fd);
		return false;
	}
	p_bookmark bm = NULL;

	if ((bm = bookmark_open_hash(hash)) == NULL) {
		sceIoClose(fd);
		return false;
	}
	memset(bm->row, 0xFF, 10 * sizeof(dword));
	sceIoRead(fd, &bm->row[0], 10 * sizeof(dword));
	bookmark_save(bm);
	bookmark_close(bm);
	sceIoClose(fd);
	return true;
}
