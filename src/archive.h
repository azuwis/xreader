#pragma once

#include "unrar.h"

typedef struct
{
	int idx, size;
	byte *buf;
} t_image_rar, *p_image_rar;

extern void extract_archive_file_into_buffer(buffer ** buf,
											 const char *archname,
											 const char *archpath,
											 t_fs_filetype filetype);

extern void extract_rar_file_into_image(t_image_rar * image,
										const char *archname,
										const char *archpath);

extern HANDLE reopen_rar_with_passwords(struct RAROpenArchiveData *arcdata);
