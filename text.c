/* vim:set ts=4 sw=4 cindent ignorecase enc=gbk: */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pspuser.h>

#include <unzip.h>
#include <chm_lib.h>
#include <unrar.h>
#include "charsets.h"
#include "display.h"
#include "html.h"
#include "text.h"
#include "kubridge.h"
#include "dbg.h"

static void text_decode(p_text txt, t_conf_encode encode)
{
	if(txt->size < 2)
		return;
	if(*(word*)txt->buf == 0xFEFF)
	{
		txt->size = charsets_ucs_conv((const byte *)(txt->buf + 2), (byte *)txt->buf);
		txt->ucs = 1;
	}
	else if(*(word*)txt->buf == 0xFFEF)
	{
		txt->size = charsets_utf16be_conv((const byte *)(txt->buf + 2), (byte *)txt->buf);
		txt->ucs = 1;
	}
	else if(txt->size > 2 && (unsigned char)txt->buf[0] == 0xEF && (unsigned char)txt->buf[1] == 0xBB && (unsigned char)txt->buf[2] == 0xBF)
	{
		txt->size = charsets_utf8_conv((const byte *)(txt->buf + 3), (byte *)txt->buf);
		txt->ucs = 2;
	}
	else
	{
		switch(encode)
		{
		case conf_encode_big5:
			charsets_big5_conv((const byte *)txt->buf, (byte *)txt->buf);
			break;
		case conf_encode_sjis:
			{
				char * orgbuf = txt->buf;
				byte * newbuf;
				charsets_sjis_conv((const byte *)orgbuf, &newbuf, &txt->size);
				txt->buf = (char *)newbuf;
				if(txt->buf != NULL)
					free((void *)orgbuf);
				else
					txt->buf = orgbuf;
			}
			break;
		case conf_encode_utf8:
			txt->size = charsets_utf8_conv((const byte *)txt->buf, (byte *)txt->buf);
			break;
		default:;
		}
		txt->ucs = 0;
	}
}

static bool bytetable[256] = {
	1, 0, 0, 0, 0, 0, 0, 0, 0, 2, 1, 0, 0, 1, 0, 0,  // 0x00
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 0x10
	2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 0x20
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 0x30
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 0x40
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 0x50
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 0x60
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 0x70
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 0x80
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 0x90
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 0xA0
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 0xB0
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 0xC0
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 0xD0
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 0xE0
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0   // 0xF0
};
extern bool text_format(p_text txt, dword rowpixels, dword wordspace)
{
	char * pos = txt->buf, * posend = pos + txt->size;
	dword curs;
	txt->row_count = 0;
	for(curs = 0; curs < 1024; ++ curs)
		if(txt->rows[curs] != NULL)
		{
			free((void *)txt->rows[curs]);
			txt->rows[curs] = NULL;
		}
	curs = 0;
	while(txt->row_count < 1024 * 1024 && pos + 1 < posend)
	{
		if((txt->row_count % 1024) == 0)
		{
			curs = txt->row_count >> 10;
			if((txt->rows[curs] = (p_textrow)calloc(1024, sizeof(t_textrow))) == NULL)
				return false;
		}
		txt->rows[curs][txt->row_count & 0x3FF].start = pos;
		char * startp = pos;
		dword width = 0;
		while(pos < posend && bytetable[*(byte *)pos] != 1)
			if((*(byte *)pos) >= 0x80)
			{
				width += DISP_BOOK_FONTSIZE;
				if(width > rowpixels)
					break;
				width += wordspace * 2;
				pos += 2;
			}
			else
			{
				width += disp_ewidth[*(byte *)pos];
				if(width > rowpixels)
					break;
				width += wordspace;
				++ pos;
			}
/*		if(pos + 1 < posend && ((*pos >= 'A' && *pos <= 'Z') || (*pos >= 'a' && *pos <= 'z')))
		{
			char * curp = pos - 1;
			while(curp > startp)
			{
				if(*(byte *)(curp - 1) >= 0x80 || *curp == ' ' || * curp == '\t')
				{
					pos = curp + 1;
					break;
				}
				curp --;
			}
		}*/
		if(bytetable[*(byte *)pos] == 1)
		{
			if(*pos == '\r' && *(pos + 1) == '\n')
				pos += 2;
			else
				++ pos;
		}
		txt->rows[curs][txt->row_count & 0x3FF].count = pos - startp;
		txt->row_count ++;
	}
	return true;
}

static dword text_reorder(char * string, dword size)
{
	int i;
	char * wtxt = string, * ctxt = string, * etxt = string + size;
	while(ctxt < etxt)
	{
		while(ctxt < etxt && bytetable[*(byte *)ctxt] == 0)
			* wtxt ++ = * ctxt ++;
		if(ctxt >= etxt)
			break;
		switch(*ctxt)
		{
		case '\t':
			* wtxt ++ = ' ';
			ctxt ++;
			for(i = 0; i < 3; i ++)
				if(* ctxt == ' ')
				{
					* wtxt ++ = ' ';
					ctxt ++;
				}
			while(ctxt < etxt && * ctxt == ' ')
				ctxt ++;
			break;
		case ' ':
			* wtxt ++ = ' ';
			ctxt ++;
			if(ctxt - 1 > string && *(ctxt - 2) == '\n')
				for(i = 0; i < 2; i ++)
					if(* ctxt == ' ')
					{
						* wtxt ++ = ' ';
						ctxt ++;
					}
			if(* ctxt == ' ')
			{
				* wtxt ++ = ' ';
				ctxt ++;
			}
			while(ctxt < etxt && * ctxt == ' ')
				ctxt ++;
			break;
		case '\r':case '\n':
			i = ((*ctxt == '\n') ? 1 : 0);
			* wtxt ++ = '\n';
			ctxt ++;
			while(ctxt < etxt && (* ctxt == '\r' || * ctxt == '\n'))
			{
				i += ((*ctxt == '\n') ? 1 : 0);
				ctxt ++;
			}
			if(i > 2)
				* wtxt ++ = '\n';
			break;
		case 0:
			ctxt ++;
		}
	}
	return wtxt - string;
}

extern p_text text_open(const char * filename, t_fs_filetype ft, dword rowpixels, dword wordspace, t_conf_encode encode, bool reorder)
{
	int fd;
	p_text txt = (p_text)calloc(1, sizeof(t_text));
	if(txt == NULL)
		return NULL;
	if((fd = sceIoOpen(filename, PSP_O_RDONLY, 0777)) < 0)
	{
		text_close(txt);
		return NULL;
	}
	strcpy(txt->filename, filename);
	txt->size = sceIoLseek32(fd, 0, PSP_SEEK_END);
	if((txt->buf = (char *)calloc(1, txt->size + 1)) == NULL)
	{
		sceIoClose(fd);
		text_close(txt);
		return NULL;
	}
	sceIoLseek32(fd, 0, PSP_SEEK_SET);
	sceIoRead(fd, txt->buf, txt->size);
	sceIoClose(fd);
	txt->buf[txt->size] = 0;
	text_decode(txt, encode);
	if(ft == fs_filetype_html)
		txt->size = html_to_text(txt->buf, txt->size, true);
	if(reorder)
		txt->size = text_reorder(txt->buf, txt->size);
	if(!text_format(txt, rowpixels, wordspace))
	{
		text_close(txt);
		return NULL;
	}
	return txt;
}

extern p_text text_open_binary(const char * filename, bool vert)
{
	int fd;
	p_text txt = (p_text)calloc(1, sizeof(t_text));
	if(txt == NULL)
		return NULL;
	if((fd = sceIoOpen(filename, PSP_O_RDONLY, 0777)) < 0)
	{
		text_close(txt);
		return NULL;
	}
	strcpy(txt->filename, filename);
	txt->size = sceIoLseek32(fd, 0, PSP_SEEK_END);
	if(txt->size > 256 * 1024) 
	{
		if (kuKernelGetModel() != PSP_MODEL_SLIM_AND_LITE)
		{
			txt->size = 256 * 1024;
		}
		else {
			if(txt->size > 4 * 1024 * 1024)
				txt->size = 4 * 1024 * 1024;
		}
	}
	byte * tmpbuf;
	dword bpr = (vert ? 43 : 66);
	if((txt->buf = (char *)calloc(1, (txt->size + 15) / 16 * bpr)) == NULL || (tmpbuf = (byte *)calloc(1, txt->size)) == NULL)
	{
		sceIoClose(fd);
		text_close(txt);
		return NULL;
	}
	sceIoLseek32(fd, 0, PSP_SEEK_SET);
	sceIoRead(fd, tmpbuf, txt->size);
	sceIoClose(fd);

	dword curs = 0;
	txt->row_count = (txt->size + 15) / 16;
	byte * cbuf = tmpbuf;
	dword i;
	for(i = 0; i < txt->row_count; i ++)
	{
		if((i % 1024) == 0)
		{
			curs = i >> 10;
			if((txt->rows[curs] = (p_textrow)calloc(1024, sizeof(t_textrow))) == NULL)
			{
				free((void *)tmpbuf);
				text_close(txt);
				return NULL;
			}
		}
		txt->rows[curs][i & 0x3FF].start = &txt->buf[bpr * i];
		txt->rows[curs][i & 0x3FF].count = bpr;
		if(vert)
		{
			sprintf(&txt->buf[bpr * i], "%08X: %02X%02X%02X%02X%02X%02X%02X%02X %02X%02X%02X%02X%02X%02X%02X%02X", (unsigned int)i * 0x10, cbuf[0], cbuf[1], cbuf[2], cbuf[3], cbuf[4], cbuf[5], cbuf[6], cbuf[7], cbuf[8], cbuf[9], cbuf[10], cbuf[11], cbuf[12], cbuf[13], cbuf[14], cbuf[15]);
			if((i + 1) * 16 > txt->size)
			{
				dword padding = (i + 1) * 16 - txt->size;
				if(padding < 9)
					memset(&txt->buf[bpr * i + bpr - padding * 2], 0x20, padding * 2);
				else
					memset(&txt->buf[bpr * i + bpr - 1 - padding * 2], 0x20, padding * 2 + 1);
			}
		}
		else
		{
			sprintf(&txt->buf[bpr * i], "%08X: %02X%02X %02X%02X %02X%02X %02X%02X %02X%02X %02X%02X %02X%02X %02X%02X ", (unsigned int)i * 0x10, cbuf[0], cbuf[1], cbuf[2], cbuf[3], cbuf[4], cbuf[5], cbuf[6], cbuf[7], cbuf[8], cbuf[9], cbuf[10], cbuf[11], cbuf[12], cbuf[13], cbuf[14], cbuf[15]);
			dword j;
			for(j = 0; j < 16; j ++)
				txt->buf[bpr * i + 40 + 10 + j] = (cbuf[j] > 0x1F && cbuf[j] < 0x7F) ? cbuf[j] : '.';
			if((i + 1) * 16 > txt->size)
			{
				dword padding = (i + 1) * 16 - txt->size;
				memset(&txt->buf[bpr * i + bpr - padding], 0x20, padding);
				if((padding & 1) > 0)
					memset(&txt->buf[bpr * i + 40 + 10 - padding / 2 * 5 - 3], 0x20, padding / 2 * 5 + 3);
				else
					memset(&txt->buf[bpr * i + 40 + 10 - padding / 2 * 5], 0x20, padding / 2 * 5);
			}
		}
		cbuf += 16;
	}
	free((void *)tmpbuf);
	return txt;
}

extern p_text text_open_in_gz(const char * gzfile, const char * filename, t_fs_filetype ft, dword rowpixels, dword wordspace, t_conf_encode encode, bool reorder)
{
	p_text txt = (p_text)calloc(1, sizeof(t_text));
	if(txt == NULL)
		return NULL;
	gzFile unzf = gzopen(gzfile, "rb");
	if(unzf == NULL)
	{
		text_close(txt);
		return NULL;
	}
	strcpy(txt->filename, filename);
	int len, cap = BUFSIZ;
	if((txt->buf = (char *)calloc(1, BUFSIZ)) == NULL)
	{
		text_close(txt);
		gzclose(unzf);
		return NULL;
	}
	txt->size = 0;
	while((len = gzread(unzf, txt->buf + txt->size, cap - txt->size)) > 0) {
		txt->size += len;
		if(txt->size >= cap) {
			cap *= 2 + 16;
			txt->buf = realloc(txt->buf, cap);
			if(txt->buf == NULL) {
				text_close(txt);
				gzclose(unzf);
				return NULL;
			}
		}
	}
	if(len < 0) {
		text_close(txt);
		gzclose(unzf);
		return NULL;
	}
	gzclose(unzf);
	text_decode(txt, encode);
	if(ft == fs_filetype_html)
		txt->size = html_to_text(txt->buf, txt->size, true);
	if(reorder)
		txt->size = text_reorder(txt->buf, txt->size);
	if(!text_format(txt, rowpixels, wordspace))
	{
		text_close(txt);
		return NULL;
	}
	return txt;
}

extern p_text text_open_binary_in_zip(const char * zipfile, const char * filename, t_fs_filetype ft, dword rowpixels, dword wordspace, t_conf_encode encode, bool reorder, bool vert)
{
	p_text txt = (p_text)calloc(1, sizeof(t_text));
	if(txt == NULL)
		return NULL;
	unzFile unzf = unzOpen(zipfile);
	if(unzf == NULL)
	{
		text_close(txt);
		return NULL;
	}
	if(unzLocateFile(unzf, filename, 0) != UNZ_OK || unzOpenCurrentFile(unzf) != UNZ_OK)
	{
		text_close(txt);
		unzClose(unzf);
		return NULL;
	}
	strcpy(txt->filename, filename);
	unz_file_info info;
	if(unzGetCurrentFileInfo(unzf, &info, NULL, 0, NULL, 0, NULL, 0) != UNZ_OK)
	{
		text_close(txt);
		unzCloseCurrentFile(unzf);
		unzClose(unzf);
		return NULL;
	}
	txt->size = info.uncompressed_size;
	if((txt->buf = (char *)calloc(1, txt->size)) == NULL)
	{
		text_close(txt);
		unzCloseCurrentFile(unzf);
		unzClose(unzf);
		return NULL;
	}
	txt->size = unzReadCurrentFile(unzf, txt->buf, txt->size);
	unzCloseCurrentFile(unzf);
	unzClose(unzf);

	byte * tmpbuf = (byte*) txt->buf;
	dword bpr = (vert ? 43 : 66);
	if((txt->buf = (char *)calloc(1, (txt->size + 15) / 16 * bpr)) == NULL)
	{
		free(tmpbuf);
		text_close(txt);
		return NULL;
	}
	dword curs = 0;
	txt->row_count = (txt->size + 15) / 16;
	byte * cbuf = tmpbuf;
	dword i;
	for(i = 0; i < txt->row_count; i ++)
	{
		if((i % 1024) == 0)
		{
			curs = i >> 10;
			if((txt->rows[curs] = (p_textrow)calloc(1024, sizeof(t_textrow))) == NULL)
			{
				free((void *)tmpbuf);
				text_close(txt);
				return NULL;
			}
		}
		txt->rows[curs][i & 0x3FF].start = &txt->buf[bpr * i];
		txt->rows[curs][i & 0x3FF].count = bpr;
		if(vert)
		{
			sprintf(&txt->buf[bpr * i], "%08X: %02X%02X%02X%02X%02X%02X%02X%02X %02X%02X%02X%02X%02X%02X%02X%02X", (unsigned int)i * 0x10, cbuf[0], cbuf[1], cbuf[2], cbuf[3], cbuf[4], cbuf[5], cbuf[6], cbuf[7], cbuf[8], cbuf[9], cbuf[10], cbuf[11], cbuf[12], cbuf[13], cbuf[14], cbuf[15]);
			if((i + 1) * 16 > txt->size)
			{
				dword padding = (i + 1) * 16 - txt->size;
				if(padding < 9)
					memset(&txt->buf[bpr * i + bpr - padding * 2], 0x20, padding * 2);
				else
					memset(&txt->buf[bpr * i + bpr - 1 - padding * 2], 0x20, padding * 2 + 1);
			}
		}
		else
		{
			sprintf(&txt->buf[bpr * i], "%08X: %02X%02X %02X%02X %02X%02X %02X%02X %02X%02X %02X%02X %02X%02X %02X%02X ", (unsigned int)i * 0x10, cbuf[0], cbuf[1], cbuf[2], cbuf[3], cbuf[4], cbuf[5], cbuf[6], cbuf[7], cbuf[8], cbuf[9], cbuf[10], cbuf[11], cbuf[12], cbuf[13], cbuf[14], cbuf[15]);
			dword j;
			for(j = 0; j < 16; j ++)
				txt->buf[bpr * i + 40 + 10 + j] = (cbuf[j] > 0x1F && cbuf[j] < 0x7F) ? cbuf[j] : '.';
			if((i + 1) * 16 > txt->size)
			{
				dword padding = (i + 1) * 16 - txt->size;
				memset(&txt->buf[bpr * i + bpr - padding], 0x20, padding);
				if((padding & 1) > 0)
					memset(&txt->buf[bpr * i + 40 + 10 - padding / 2 * 5 - 3], 0x20, padding / 2 * 5 + 3);
				else
					memset(&txt->buf[bpr * i + 40 + 10 - padding / 2 * 5], 0x20, padding / 2 * 5);
			}
		}
		cbuf += 16;
	}
	free((void *)tmpbuf);
	return txt;
}

extern p_text text_open_in_zip(const char * zipfile, const char * filename, t_fs_filetype ft, dword rowpixels, dword wordspace, t_conf_encode encode, bool reorder)
{
	p_text txt = (p_text)calloc(1, sizeof(t_text));
	if(txt == NULL)
		return NULL;
	unzFile unzf = unzOpen(zipfile);
	if(unzf == NULL)
	{
		text_close(txt);
		return NULL;
	}
	if(unzLocateFile(unzf, filename, 0) != UNZ_OK || unzOpenCurrentFile(unzf) != UNZ_OK)
	{
		text_close(txt);
		unzClose(unzf);
		return NULL;
	}
	strcpy(txt->filename, filename);
	unz_file_info info;
	if(unzGetCurrentFileInfo(unzf, &info, NULL, 0, NULL, 0, NULL, 0) != UNZ_OK)
	{
		text_close(txt);
		unzCloseCurrentFile(unzf);
		unzClose(unzf);
		return NULL;
	}
	txt->size = info.uncompressed_size;
	if((txt->buf = (char *)calloc(1, txt->size)) == NULL)
	{
		text_close(txt);
		unzCloseCurrentFile(unzf);
		unzClose(unzf);
		return NULL;
	}
	txt->size = unzReadCurrentFile(unzf, txt->buf, txt->size);
	unzCloseCurrentFile(unzf);
	unzClose(unzf);
	text_decode(txt, encode);
	if(ft == fs_filetype_html)
		txt->size = html_to_text(txt->buf, txt->size, true);
	if(reorder)
		txt->size = text_reorder(txt->buf, txt->size);
	if(!text_format(txt, rowpixels, wordspace))
	{
		text_close(txt);
		return NULL;
	}
	return txt;
}

static int curidx = 0;

static int rarcbproc(UINT msg,LONG UserData,LONG P1,LONG P2)
{
	if(msg == UCM_PROCESSDATA)
	{
		p_text txt = (p_text)UserData;
		if(P2 > (long)txt->size - curidx)
		{
			memcpy(&txt->buf[curidx], (void *)P1, (long)txt->size - curidx);
			return -1;
		}
		memcpy(&txt->buf[curidx], (void *)P1, P2);
		curidx += P2;
	}
	return 0;
}

extern p_text text_open_binary_in_rar(const char * rarfile, const char * filename, t_fs_filetype ft, dword rowpixels, dword wordspace, t_conf_encode encode, bool reorder, bool vert)
{
	byte * tmpbuf = 0;
	p_text txt = (p_text)calloc(1, sizeof(t_text));
	DBG_ASSERT(d, "txt != null", txt != NULL);
	if(txt == NULL)
		return NULL;
	struct RAROpenArchiveData arcdata;
	arcdata.ArcName = (char *)rarfile;
	arcdata.OpenMode = RAR_OM_EXTRACT;
	arcdata.CmtBuf = NULL;
	arcdata.CmtBufSize = 0;
	HANDLE hrar = RAROpenArchive(&arcdata);
	DBG_ASSERT(d, "hrar != null", hrar != NULL);
	if(hrar == NULL)
	{
		text_close(txt);
		return NULL;
	}
	do {
		struct RARHeaderData header;
		if(RARReadHeader(hrar, &header) != 0)
			break;
		if(stricmp(header.FileName, filename) == 0)
		{
			curidx = 0;
			strcpy(txt->filename, filename);
			txt->size = header.UnpSize;
			if((tmpbuf = (byte*)calloc(1, txt->size)) == NULL) 
				return NULL;
			txt->buf = (char*)tmpbuf;
			RARSetCallback(hrar, rarcbproc, (LONG)txt);
			dword bpr = (vert ? 43 : 66);
			if( RARProcessFile(hrar, RAR_TEST, NULL, NULL) != 0 || (txt->buf = (char *)calloc(1, (txt->size + 15) / 16 * bpr)) == NULL)
			{
				DBG_ASSERT(d, "RARProcessFile", 0);
				free(tmpbuf);
				text_close(txt);
				RARCloseArchive(hrar);
				return NULL;
			}
			RARCloseArchive(hrar);
			dword curs = 0;
			txt->row_count = (txt->size + 15) / 16;
			byte * cbuf = tmpbuf;
			dword i;
			for(i = 0; i < txt->row_count; i ++)
			{
				if((i % 1024) == 0)
				{
					curs = i >> 10;
					if((txt->rows[curs] = (p_textrow)calloc(1024, sizeof(t_textrow))) == NULL)
					{
						free((void *)tmpbuf);
						text_close(txt);
						return NULL;
					}
				}
				txt->rows[curs][i & 0x3FF].start = &txt->buf[bpr * i];
				txt->rows[curs][i & 0x3FF].count = bpr;
				if(vert)
				{
					sprintf(&txt->buf[bpr * i], "%08X: %02X%02X%02X%02X%02X%02X%02X%02X %02X%02X%02X%02X%02X%02X%02X%02X", (unsigned int)i * 0x10, cbuf[0], cbuf[1], cbuf[2], cbuf[3], cbuf[4], cbuf[5], cbuf[6], cbuf[7], cbuf[8], cbuf[9], cbuf[10], cbuf[11], cbuf[12], cbuf[13], cbuf[14], cbuf[15]);
					if((i + 1) * 16 > txt->size)
					{
						dword padding = (i + 1) * 16 - txt->size;
						if(padding < 9)
							memset(&txt->buf[bpr * i + bpr - padding * 2], 0x20, padding * 2);
						else
							memset(&txt->buf[bpr * i + bpr - 1 - padding * 2], 0x20, padding * 2 + 1);
					}
				}
				else
				{
					sprintf(&txt->buf[bpr * i], "%08X: %02X%02X %02X%02X %02X%02X %02X%02X %02X%02X %02X%02X %02X%02X %02X%02X ", (unsigned int)i * 0x10, cbuf[0], cbuf[1], cbuf[2], cbuf[3], cbuf[4], cbuf[5], cbuf[6], cbuf[7], cbuf[8], cbuf[9], cbuf[10], cbuf[11], cbuf[12], cbuf[13], cbuf[14], cbuf[15]);
					dword j;
					for(j = 0; j < 16; j ++)
						txt->buf[bpr * i + 40 + 10 + j] = (cbuf[j] > 0x1F && cbuf[j] < 0x7F) ? cbuf[j] : '.';
					if((i + 1) * 16 > txt->size)
					{
						dword padding = (i + 1) * 16 - txt->size;
						memset(&txt->buf[bpr * i + bpr - padding], 0x20, padding);
						if((padding & 1) > 0)
							memset(&txt->buf[bpr * i + 40 + 10 - padding / 2 * 5 - 3], 0x20, padding / 2 * 5 + 3);
						else
							memset(&txt->buf[bpr * i + 40 + 10 - padding / 2 * 5], 0x20, padding / 2 * 5);
					}
				}
				cbuf += 16;
			}
			free((void *)tmpbuf);

			DBG_ASSERT(d, "txt != NULL", txt != NULL);
			return txt;
		}
	} while(RARProcessFile(hrar, RAR_SKIP, NULL, NULL) == 0);
	text_close(txt);
	RARCloseArchive(hrar);
	return NULL;
}

extern p_text text_open_in_rar(const char * rarfile, const char * filename, t_fs_filetype ft, dword rowpixels, dword wordspace, t_conf_encode encode, bool reorder)
{
	p_text txt = (p_text)calloc(1, sizeof(t_text));
	if(txt == NULL)
		return NULL;
	struct RAROpenArchiveData arcdata;
	arcdata.ArcName = (char *)rarfile;
	arcdata.OpenMode = RAR_OM_EXTRACT;
	arcdata.CmtBuf = NULL;
	arcdata.CmtBufSize = 0;
	HANDLE hrar = RAROpenArchive(&arcdata);
	if(hrar == NULL)
	{
		text_close(txt);
		return NULL;
	}
	RARSetCallback(hrar, rarcbproc, (LONG)txt);
	do {
		struct RARHeaderData header;
		if(RARReadHeader(hrar, &header) != 0)
			break;
		if(stricmp(header.FileName, filename) == 0)
		{
			curidx = 0;
			strcpy(txt->filename, filename);
			txt->size = header.UnpSize;
			if((txt->buf = (char *)calloc(1, txt->size)) == NULL || RARProcessFile(hrar, RAR_TEST, NULL, NULL) != 0)
			{
				text_close(txt);
				RARCloseArchive(hrar);
				return NULL;
			}
			RARCloseArchive(hrar);
			text_decode(txt, encode);
			if(ft == fs_filetype_html)
				txt->size = html_to_text(txt->buf, txt->size, true);
			if(reorder)
				txt->size = text_reorder(txt->buf, txt->size);
			if(!text_format(txt, rowpixels, wordspace))
			{
				text_close(txt);
				return NULL;
			}
			return txt;
		}
	} while(RARProcessFile(hrar, RAR_SKIP, NULL, NULL) == 0);
	text_close(txt);
	RARCloseArchive(hrar);
	return NULL;
}

extern p_text text_open_in_chm(const char * chmfile, const char * filename, t_fs_filetype ft, dword rowpixels, dword wordspace, t_conf_encode encode, bool reorder)
{
	p_text txt = (p_text)calloc(1, sizeof(t_text));
	if(txt == NULL)
		return NULL;
	struct chmFile * chm = chm_open(chmfile);
	if(chm == NULL)
	{
		text_close(txt);
		return NULL;
	}
	struct chmUnitInfo ui;
    if (chm_resolve_object(chm, filename, &ui) != CHM_RESOLVE_SUCCESS)
	{
		text_close(txt);
		chm_close(chm);
		return NULL;
	}
	strcpy(txt->filename, filename);
	txt->size = ui.length;
	if((txt->buf = (char *)calloc(1, txt->size)) == NULL)
	{
		text_close(txt);
		chm_close(chm);
		return NULL;
	}
	txt->size = chm_retrieve_object(chm, &ui, (byte *)txt->buf, 0, txt->size);
	chm_close(chm);
	text_decode(txt, encode);
	if(ft == fs_filetype_html)
		txt->size = html_to_text(txt->buf, txt->size, true);
	if(reorder)
		txt->size = text_reorder(txt->buf, txt->size);
	if(!text_format(txt, rowpixels, wordspace))
	{
		text_close(txt);
		return NULL;
	}
	return txt;
}

extern void text_close(p_text fstext)
{
	if(fstext != NULL)
	{
		dword i;
		if(fstext->buf != NULL)
			free((void *)fstext->buf);
		for(i = 0; i < 1024; ++ i)
			if(fstext->rows[i] != NULL)
				free((void *)fstext->rows[i]);
	}
}
