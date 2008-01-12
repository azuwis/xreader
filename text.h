/* vim:set ts=4 sw=4 cindent ignorecase enc=gbk: */

#ifndef _TEXT_H_
#define _TEXT_H_

#include "common/datatype.h"
#include "fs.h"

typedef struct {
	char * start;
	dword count;
} t_textrow, * p_textrow;

typedef struct {
	char filename[256];
	dword crow;
	dword size;
	char * buf;
	int ucs;
	dword row_count;
	p_textrow rows[1024];
} t_text, * p_text;

extern bool text_format(p_text txt, dword rowpixels, dword wordspace);
extern p_text text_open(const char * filename, t_fs_filetype ft, dword rowpixels, dword wordspace, t_conf_encode encode, bool reorder);
extern p_text text_open_binary(const char * filename, bool vert);
extern p_text text_open_in_zip(const char * zipfile, const char * filename, t_fs_filetype ft, dword rowpixels, dword wordspace, t_conf_encode encode, bool reorder);
extern p_text text_open_in_gz(const char * gzfile, const char * filename, t_fs_filetype ft, dword rowpixels, dword wordspace, t_conf_encode encode, bool reorder);
extern p_text text_open_in_rar(const char * rarfile, const char * filename, t_fs_filetype ft, dword rowpixels, dword wordspace, t_conf_encode encode, bool reorder);
extern p_text text_open_in_chm(const char * chmfile, const char * filename, t_fs_filetype ft, dword rowpixels, dword wordspace, t_conf_encode encode, bool reorder);
extern p_text text_open_binary_in_rar(const char * rarfile, const char * filename, t_fs_filetype ft, dword rowpixels, dword wordspace, t_conf_encode encode, bool reorder, bool vert);
extern p_text text_open_binary_in_zip(const char * zipfile, const char * filename, t_fs_filetype ft, dword rowpixels, dword wordspace, t_conf_encode encode, bool reorder, bool vert);
extern void text_close(p_text fstext);

#endif
