#ifndef _IMAGE_C_
#define _IAMGE_C_

#include "buffer.h"
#include "libexif/exif-data.h"
#include "common/datatype.h"
#include "display.h"
#include "fs.h"

extern void image_zoom_bicubic(pixel * src, int srcwidth, int srcheight,
							   pixel * dest, int destwidth, int destheight);
extern void image_zoom_bilinear(pixel * src, int srcwidth, int srcheight,
								pixel * dest, int destwidth, int destheight);
extern void image_rotate(pixel * imgdata, dword * pwidth, dword * pheight,
						 dword organgle, dword newangle);
extern int image_readpng(const char *filename, dword * pwidth, dword * pheight,
						 pixel ** image_data, pixel * bgcolor);
extern int image_readpng_in_zip(const char *zipfile, const char *filename,
								dword * pwidth, dword * pheight,
								pixel ** image_data, pixel * bgcolor);
extern int image_readpng_in_chm(const char *chmfile, const char *filename,
								dword * pwidth, dword * pheight,
								pixel ** image_data, pixel * bgcolor);
extern int image_readpng_in_rar(const char *rarfile, const char *filename,
								dword * pwidth, dword * pheight,
								pixel ** image_data, pixel * bgcolor);
extern int image_readgif(const char *filename, dword * pwidth, dword * pheight,
						 pixel ** image_data, pixel * bgcolor);
extern int image_readgif_in_zip(const char *zipfile, const char *filename,
								dword * pwidth, dword * pheight,
								pixel ** image_data, pixel * bgcolor);
extern int image_readgif_in_chm(const char *chmfile, const char *filename,
								dword * pwidth, dword * pheight,
								pixel ** image_data, pixel * bgcolor);
extern int image_readgif_in_umd(const char *umdfile, size_t file_pos,
								size_t length, dword * pwidth, dword * pheight,
								pixel ** image_data, pixel * bgcolor);
extern int image_readgif_in_rar(const char *rarfile, const char *filename,
								dword * pwidth, dword * pheight,
								pixel ** image_data, pixel * bgcolor);
extern int image_readjpg(const char *filename, dword * pwidth, dword * pheight,
						 pixel ** image_data, pixel * bgcolor);
extern int image_readjpg_in_zip(const char *zipfile, const char *filename,
								dword * pwidth, dword * pheight,
								pixel ** image_data, pixel * bgcolor);
extern int image_readjpg_in_chm(const char *chmfile, const char *filename,
								dword * pwidth, dword * pheight,
								pixel ** image_data, pixel * bgcolor);
extern int image_readjpg_in_umd(const char *umdfile, size_t file_pos,
								size_t length, dword * pwidth, dword * pheight,
								pixel ** image_data, pixel * bgcolor);
extern int image_readjpg_in_rar(const char *rarfile, const char *filename,
								dword * pwidth, dword * pheight,
								pixel ** image_data, pixel * bgcolor);
extern int exif_readjpg(const char *filename, dword * pwidth, dword * pheight,
						pixel ** image_data, pixel * bgcolor);
extern int exif_readjpg_in_zip(const char *zipfile, const char *filename,
							   dword * pwidth, dword * pheight,
							   pixel ** image_data, pixel * bgcolor);
extern int exif_readjpg_in_chm(const char *chmfile, const char *filename,
							   dword * pwidth, dword * pheight,
							   pixel ** image_data, pixel * bgcolor);
extern int exif_readjpg_in_umd(const char *umdfile, size_t file_pos,
							   size_t length, dword * pwidth, dword * pheight,
							   pixel ** image_data, pixel * bgcolor);
extern int exif_readjpg_in_rar(const char *rarfile, const char *filename,
							   dword * pwidth, dword * pheight,
							   pixel ** image_data, pixel * bgcolor);
extern int image_readbmp(const char *filename, dword * pwidth, dword * pheight,
						 pixel ** image_data, pixel * bgcolor);
extern int image_readbmp_in_zip(const char *zipfile, const char *filename,
								dword * pwidth, dword * pheight,
								pixel ** image_data, pixel * bgcolor);
extern int image_readbmp_in_chm(const char *chmfile, const char *filename,
								dword * pwidth, dword * pheight,
								pixel ** image_data, pixel * bgcolor);
extern int image_readbmp_in_umd(const char *umdfile, size_t file_pos,
								size_t length, dword * pwidth, dword * pheight,
								pixel ** image_data, pixel * bgcolor);
extern int image_readbmp_in_rar(const char *rarfile, const char *filename,
								dword * pwidth, dword * pheight,
								pixel ** image_data, pixel * bgcolor);
extern int image_readtga(const char *filename, dword * pwidth, dword * pheight,
						 pixel ** image_data, pixel * bgcolor);
extern int image_readtga_in_zip(const char *zipfile, const char *filename,
								dword * pwidth, dword * pheight,
								pixel ** image_data, pixel * bgcolor);
extern int image_readtga_in_chm(const char *chmfile, const char *filename,
								dword * pwidth, dword * pheight,
								pixel ** image_data, pixel * bgcolor);
extern int image_readtga_in_rar(const char *rarfile, const char *filename,
								dword * pwidth, dword * pheight,
								pixel ** image_data, pixel * bgcolor);
extern int image_open_normal(const char *filename, t_fs_filetype ft,
							 dword * pWidth, dword * pHeight,
							 pixel ** ppImageData, pixel * pBgColor);
extern int image_open_umd(const char *chaptername, const char *umdfile,
						  t_fs_filetype ft, size_t file_pos, size_t length,
						  dword * pWidth, dword * pHeight, pixel ** ppImageData,
						  pixel * pBgColor);
int image_open_archive(const char *filename, const char *archname,
					   t_fs_filetype ft, dword * pWidth, dword * pHeight,
					   pixel ** ppImageData, pixel * pBgColor, int where);
extern void exif_entry_viewer(ExifEntry * pentry, void *user_data);
extern void exif_context_viewer(ExifContent * pcontext, void *user_data);
extern void exif_viewer(ExifData * data);

extern buffer_array *exif_array;

#endif
