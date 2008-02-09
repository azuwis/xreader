/* vim:set ts=4 sw=4 cindent ignorecase enc=gbk: */

#include "config.h"

#if defined(ENABLE_IMAGE) || defined(ENABLE_BG)

#include <psptypes.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <png.h>
#include <gif_lib.h>
#include <jpeglib.h>
#include <bmplib.h>
#include <tga.h>
#include <chm_lib.h>
#include <unzip.h>
#include <unrar.h>
#include <setjmp.h>
#include <psprtc.h>
#include "common/utils.h"
#include "win.h"
#include "image.h"
#include "buffer.h"
#include "dbg.h"
#include "conf.h"

extern t_conf config;

ExifData *exif_data = NULL;
buffer_array *exif_array = 0;

/* 
#define PB  (1.0f/3.0f)
#define PC  (1.0f/3.0f)
#define P0  ((  6.0f- 2.0f*PB       )/6.0f)
#define P2  ((-18.0f+12.0f*PB+ 6.0f*PC)/6.0f)
#define P3  (( 12.0f- 9.0f*PB- 6.0f*PC)/6.0f)
#define Q0  ((       8.0f*PB+24.0f*PC)/6.0f)
#define Q1  ((     -12.0f*PB-48.0f*PC)/6.0f)
#define Q2  ((       6.0f*PB+30.0f*PC)/6.0f)
#define Q3  ((     - 1.0f*PB- 6.0f*PC)/6.0f)

__inline float sinc():
	if (x < -1.0f)
		return(Q0-x*(Q1-x*(Q2-x*Q3)));
	if (x < 0.0f)
		return(P0+x*x*(P2-x*P3));
	if (x < 1.0f)
		return(P0+x*x*(P2+x*P3));
	return(Q0+x*(Q1+x*(Q2+x*Q3)));
*/

/* You can replace default sinc() with following anyone for filter changing

__inline float sinc(float x)
{
	if (x < -1.0f)
		return(0.5f*(4.0f+x*(8.0f+x*(5.0f+x))));
	if (x < 0.0f)
		return(0.5f*(2.0f+x*x*(-5.0f-3.0f*x)));
	if (x < 1.0f)
		return(0.5f*(2.0f+x*x*(-5.0f+3.0f*x)));
	return(0.5f*(4.0f+x*(-8.0f+x*(5.0f-x))));
}

__inline float sinc(float x)
{
	if (x < 0.1f)
		return((2.0f+x)*(2.0f+x)*(2.0f+x)/6.0f);
	if (x < 0.0f)
		return((4.0f+x*x*(-6.0f-3.0f*x))/6.0f);
	if (x < 1.0f)
		return((4.0f+x*x*(-6.0f+3.0f*x))/6.0f);
	return((2.0f-x)*(2.0f-x)*(2.0f-x)/6.0f);
}

__inline float sinc(float x)
{
	float s = (x < 0) ? -x : x;
	if (s <= 1.0f)
		return 1.0f - 2.0f * x * x + x * x * s;
	else
		return 4.0f - 8.0f * s + 5.0f * x * x - x * x * s;
}
*/

__inline float sinc_n2(float x)
{
	return (2.0f + x) * (2.0f + x) * (1.0f + x);
}
__inline float sinc_n1(float x)
{
	return (1.0f - x - x * x) * (1.0f + x);
}
__inline float sinc_1(float x)
{
	return (1.0f + x - x * x) * (1.0f - x);
}
__inline float sinc_2(float x)
{
	return (2.0f - x) * (2.0f - x) * (1.0f - x);
}

__inline pixel bicubic(pixel i1, pixel i2, pixel i3, pixel i4, float u1, float u2, float u3, float u4)
{
	int r, g, b;
	r = u1 * RGB_R(i1) + u2 * RGB_R(i2) + u3 * RGB_R(i3) + u4 * RGB_R(i4) + 0.5f;
	if(r > COLOR_MAX) r = COLOR_MAX;
	else if(r < 0) r = 0;

	g = u1 * RGB_G(i1) + u2 * RGB_G(i2) + u3 * RGB_G(i3) + u4 * RGB_G(i4) + 0.5f;
	if(g > COLOR_MAX) g = COLOR_MAX;
	else if(g < 0) g = 0;

	b = u1 * RGB_B(i1) + u2 * RGB_B(i2) + u3 * RGB_B(i3) + u4 * RGB_B(i4) + 0.5f;
	if(b > COLOR_MAX) b = COLOR_MAX;
	else if(b < 0) b = 0;

	return RGB2(r, g, b);
}

extern void image_zoom_bicubic(pixel * src, int srcwidth, int srcheight, pixel * dest, int destwidth, int destheight)
{
	pixel * temp1, * temp2, * temp3, * temp4, * tempdst;
	float u, v, u1[destwidth], u2[destwidth], u3[destwidth], u4[destwidth];
	int x = 0, y, x2, y1, y2, y3, y4, i, j;

	for(i = 0; i < destheight; i ++)
	{
		x2 = x / destheight;
		temp2 = src + x2 * srcwidth;
		if(x2 == 0) temp1 = temp2; else temp1 = temp2 - srcwidth;
		if(x2 + 2 > srcheight)
		{
			temp3 = temp2;
			if(x2 + 3 > srcheight) temp4 = temp3; else temp4 = temp3 + srcwidth;
		} else {
			temp3 = temp2 + srcwidth;
			temp4 = temp3 + srcwidth;
		}
		tempdst = dest;

		v = ((float)x / destheight) - x2;
		float v1 = sinc_2(v + 1.0f), v2 = sinc_1(v), v3 = sinc_n1(v - 1.0f), v4 = sinc_n2(v - 2.0f);
		y = 0;

		if(i == 0)
		{
			for(j = 0; j < destwidth; j ++)
			{
				y2 = y / destwidth;
				y1 = y2 - 1;
				y3 = y2 + 1;
				y4 = y2 + 2;
				if(y1 < 0) y1 = 0;
				if(y3 > srcwidth - 1) y3 = srcwidth - 1;
				if(y4 > srcwidth - 1) y4 = srcwidth - 1;

				u = ((float)y / destwidth) - y2;
				u1[j] = sinc_2(u + 1.0f), u2[j] = sinc_1(u), u3[j] = sinc_n1(u - 1.0f), u4[j] = sinc_n2(u - 2.0f);
				tempdst[j] = bicubic(bicubic(temp1[y1], temp1[y2], temp1[y3], temp1[y4], u1[j], u2[j], u3[j], u4[j]), bicubic(temp2[y1], temp2[y2], temp2[y3], temp2[y4], u1[j], u2[j], u3[j], u4[j]), bicubic(temp3[y1], temp3[y2], temp3[y3], temp3[y4], u1[j], u2[j], u3[j], u4[j]), bicubic(temp4[y1], temp4[y2], temp4[y3], temp4[y4], u1[j], u2[j], u3[j], u4[j]), v1, v2, v3, v4);
				y += srcwidth;
			}
		}
		else
		{
			for(j = 0; j < destwidth; j ++)
			{
				y2 = y / destwidth;
				y1 = y2 - 1;
				y3 = y2 + 1;
				y4 = y2 + 2;
				if(y1 < 0) y1 = 0;
				if(y3 > srcwidth - 1) y3 = srcwidth - 1;
				if(y4 > srcwidth - 1) y4 = srcwidth - 1;

				tempdst[j] = bicubic(bicubic(temp1[y1], temp1[y2], temp1[y3], temp1[y4], u1[j], u2[j], u3[j], u4[j]), bicubic(temp2[y1], temp2[y2], temp2[y3], temp2[y4], u1[j], u2[j], u3[j], u4[j]), bicubic(temp3[y1], temp3[y2], temp3[y3], temp3[y4], u1[j], u2[j], u3[j], u4[j]), bicubic(temp4[y1], temp4[y2], temp4[y3], temp4[y4], u1[j], u2[j], u3[j], u4[j]), v1, v2, v3, v4);
				y += srcwidth;
			}
		}
		dest += destwidth;
		x += srcheight;
	}
} 

__inline pixel bilinear(pixel i1, pixel i2, int u, int s)
{
	int r1, g1, b1, r2, g2, b2;
	r1 = RGB_R(i1); g1 = RGB_G(i1); b1 = RGB_B(i1);
	r2 = RGB_R(i2); g2 = RGB_G(i2); b2 = RGB_B(i2);
	return RGB2((r1 + u * (r2 - r1) / s), (g1 + u * (g2 - g1) / s), (b1 + u * (b2 - b1) / s));
}

extern void image_zoom_bilinear(pixel * src, int srcwidth, int srcheight, pixel * dest, int destwidth, int destheight)
{
	pixel * temp1, * temp2, * tempdst;
	int x = 0, y, u, v;
	int x2, y1, y2, i, j;

	for(i = 0; i < destheight; i ++)
	{
		x2 = x / destheight;
		if(x2 < srcheight - 1)
		{
			temp1 = src + x2 * srcwidth;
			temp2 = temp1 + srcwidth;
		}
		else
			temp1 = temp2 = src + x2 * (srcheight - 1);
		tempdst = dest;

		v = x - x2 * destheight;
		y = 0;

		for(j = 0; j < destwidth; j ++)
		{
			y1 = y / destwidth;
			if(y1 < srcwidth - 1)
				y2 = y1 + 1;
			else
				y2 = y1 = srcwidth - 1;

			u = y - y1 * destwidth;
			tempdst[j] = bilinear(bilinear(temp1[y1], temp1[y2], u, destwidth), bilinear(temp2[y1], temp2[y2], u, destwidth), v, destheight);
			y += srcwidth;
		}
		dest += destwidth;
		x += srcheight;
	}
} 

extern void image_rotate(pixel * imgdata, dword * pwidth, dword * pheight, dword organgle, dword newangle)
{
	dword ca;
	int temp;
	if(newangle < organgle)
	{
		ca = newangle + 360 - organgle;
	}
	else
		ca = newangle - organgle;
	if(ca == 0)
		return;
	pixel * newdata = memalign(16, sizeof(pixel) * *pwidth * *pheight);
	if(newdata == NULL) {
			win_msg("内存不足无法完成旋转!", COLOR_WHITE, COLOR_WHITE, config.msgbcolor);
		return;
	}
	dword i, j;
	switch(ca)
	{
	case 90:
		for(j = 0; j < *pheight; j ++)
			for(i = 0; i < *pwidth; i ++)
				newdata[i * *pheight + (*pheight - j - 1)] = imgdata[j * *pwidth + i];
		temp = *pheight; *pheight = *pwidth; *pwidth = temp;
		break;
	case 180:
		for(j = 0; j < *pheight; j ++)
			for(i = 0; i < *pwidth; i ++)
				newdata[(*pheight - j - 1) * *pwidth + (*pwidth - i - 1)] = imgdata[j * *pwidth + i];
		break;
	case 270:
		for(j = 0; j < *pheight; j ++)
			for(i = 0; i < *pwidth; i ++)
				newdata[(*pwidth - i - 1) * *pheight + j] = imgdata[j * *pwidth + i];
		temp = *pheight; *pheight = *pwidth; *pwidth = temp;
		break;
	default:
		return;
	}
	memcpy(imgdata, newdata, sizeof(pixel) * *pwidth * *pheight);
	free((void *)newdata);
}

static unsigned image_zip_fread(void *buf, unsigned r, unsigned n, void *stream)
{
	int size = unzReadCurrentFile((unzFile)stream, buf, r * n);
	if (size < 0)
		return 0;
	else
		return n;
}

static int image_zip_fseek(void *stream, long offset, int origin)
{
	if(origin != SEEK_SET)
		return 0;
	int size = offset - unztell((unzFile)stream);
	if(size <= 0)
		return 0;
	byte buf[size];
	unzReadCurrentFile((unzFile)stream, buf, size);
	return 0;
}

static long image_zip_ftell(void *stream)
{
	return unztell((unzFile)stream);
}

typedef struct {
	struct chmFile * chm;
	struct chmUnitInfo ui;
	LONGUINT64 readpos;
} t_image_chm, * p_image_chm;

static unsigned image_chm_fread(void *buf, unsigned r, unsigned n, void *stream)
{
	LONGINT64 readsize = chm_retrieve_object(((p_image_chm)stream)->chm, &((p_image_chm)stream)->ui, buf, ((p_image_chm)stream)->readpos, r * n);
	if(readsize < 0)
		readsize = 0;
	((p_image_chm)stream)->readpos += readsize;
	return n;
}

static int image_chm_fseek(void *stream, long offset, int origin)
{
	if(origin != SEEK_SET)
		return 0;
	((p_image_chm)stream)->readpos = offset;
	return 0;
}

static long image_chm_ftell(void *stream)
{
	return ((p_image_chm)stream)->readpos;
}

typedef struct {
	int idx, size;
	byte * buf;
} t_image_rar, * p_image_rar;

static int imagerarcbproc(UINT msg,LONG UserData,LONG P1,LONG P2)
{
	if(msg == UCM_PROCESSDATA)
	{
		p_image_rar irar = (p_image_rar)UserData;
		if(P2 > irar->size - irar->idx)
		{
			memcpy(&irar->buf[irar->idx], (void *)P1, irar->size - irar->idx);
			return -1;
		}
		memcpy(&irar->buf[irar->idx], (void *)P1, P2);
		irar->idx += P2;
	}
	return 0;
}

static unsigned image_rar_fread(void *buf, unsigned r, unsigned n, void *stream)
{
	p_image_rar irar = (p_image_rar)stream;
	if(r * n > irar->size - irar->idx)
	{
		memcpy(buf, &irar->buf[irar->idx], irar->size - irar->idx);
		irar->idx = irar->size;
		return n;
	}
	memcpy(buf, &irar->buf[irar->idx], r * n);
	irar->idx += (r * n);
	return n;
}

static int image_rar_fseek(void *stream, long offset, int origin)
{
	p_image_rar irar = (p_image_rar)stream;
	if(origin == SEEK_SET)
	{
		if(offset < 0)
			offset = 0;
		irar->idx = min(offset, irar->size);
	}
	else if(origin == SEEK_CUR)
	{
		if(offset + irar->idx < 0)
			offset = -irar->idx;
		irar->idx = min(offset + irar->idx, irar->size);
	}
	else
	{
		if(offset + irar->idx < 0)
			offset = -irar->idx;
		irar->idx = min(offset + irar->size, irar->size);
	}
	return 0;
}

static long image_rar_ftell(void *stream)
{
	return ((p_image_rar)stream)->idx;
}

/* PNG processing */

/* return value = 0 for success, 1 for bad sig/hdr, 4 for no mem
display_exponent == LUT_exponent * CRT_exponent */

static int image_readpng2(void *infile, dword *pwidth, dword *pheight, pixel ** image_data, pixel * bgcolor, png_rw_ptr read_fn)
{
	png_structp png_ptr = NULL;
	png_infop info_ptr = NULL;

	byte sig[8];

	png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (!png_ptr)
		return 4;

	info_ptr = png_create_info_struct(png_ptr);
	if (!info_ptr) {
		png_destroy_read_struct(&png_ptr, NULL, NULL);
		return 4;
	}

	if (setjmp(png_ptr->jmpbuf)) {
		png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
		return 1;
	}

	png_set_read_fn(png_ptr, infile, read_fn);

	read_fn(png_ptr, sig, 8);
	if (!png_check_sig(sig, 8))
	{
		png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
		return 1;
	}
	png_set_sig_bytes(png_ptr, 8);

	png_read_png(png_ptr, info_ptr, PNG_TRANSFORM_STRIP_16 | PNG_TRANSFORM_PACKING | PNG_TRANSFORM_EXPAND | PNG_TRANSFORM_BGR, NULL);

	*pwidth = info_ptr->width;
	*pheight = info_ptr->height;

	if (info_ptr->bit_depth == 16) {
		*bgcolor = RGB(info_ptr->background.red >> 8, info_ptr->background.green >> 8, info_ptr->background.blue >> 8);
	} else if (info_ptr->color_type == PNG_COLOR_TYPE_GRAY && info_ptr->bit_depth < 8) {
		if (info_ptr->bit_depth == 1)
			*bgcolor = info_ptr->background.gray ? 0xFFFFFFFF : 0;
		else if (info_ptr->bit_depth == 2)
			*bgcolor = RGB((255/3) * info_ptr->background.gray, (255/3) * info_ptr->background.gray, (255/3) * info_ptr->background.gray);
		else
			*bgcolor = RGB((255/15) * info_ptr->background.gray, (255/15) * info_ptr->background.gray, (255/15) * info_ptr->background.gray);
	} else {
		*bgcolor = RGB(info_ptr->background.red, info_ptr->background.green, info_ptr->background.blue);
	}

	if((*image_data = (pixel *)memalign(16, sizeof(pixel) * info_ptr->width * info_ptr->height)) == NULL)
	{
		png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
		return 4;
	}

	png_byte **prowtable = info_ptr->row_pointers;
	dword x, y;
	byte r=0, g=0, b=0;
	pixel * imgdata = * image_data;
	switch(info_ptr->color_type){
	case PNG_COLOR_TYPE_GRAY:
		for (y = 0; y < info_ptr->height; y ++){
			png_byte *prow = prowtable[y];
			for (x = 0; x < info_ptr->width; x ++){
				r = g = b = *prow++;
				*imgdata++ = RGB(r,g,b);
			}
		}
		break;
	case PNG_COLOR_TYPE_GRAY_ALPHA:
		for (y = 0; y < info_ptr->height; y ++){
			png_byte *prow = prowtable[y];
			for (x = 0; x < info_ptr->width; x ++){
				r = g = b = *prow++;
				prow++;
				*imgdata++ = RGB(r,g,b);
			}
		}
		break;
	case PNG_COLOR_TYPE_RGB:
		for (y = 0; y < info_ptr->height; y ++){
			png_byte *prow = prowtable[y];
			for (x = 0; x < info_ptr->width; x ++){
				b = *prow++;
				g = *prow++;
				r = *prow++;
				*imgdata++ = RGB(r,g,b);
			}
		}
		break;
	case PNG_COLOR_TYPE_RGB_ALPHA:
		for (y = 0; y < info_ptr->height; y ++){
			png_byte *prow = prowtable[y];
			for (x = 0; x < info_ptr->width; x ++){
				b = *prow++;
				g = *prow++;
				r = *prow++;
				prow++;
				*imgdata++ = RGB(r,g,b);
			}
		}
		break;
	}

	png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
	png_ptr = NULL;
	info_ptr = NULL;

	return 0;
}

static void image_png_read(png_structp png, png_bytep buf, png_size_t size)
{
	png_size_t check = fread(buf, 1, size, png->io_ptr);
	if (check != size)
		png_error(png, "Read Error");
}

static void image_png_zip_read(png_structp png, png_bytep buf, png_size_t size)
{
	png_size_t check = image_zip_fread(buf, 1, size, png->io_ptr);
	if (check != size)
		png_error(png, "Read Error");
}

static void image_png_chm_read(png_structp png, png_bytep buf, png_size_t size)
{
	png_size_t check = image_chm_fread(buf, 1, size, png->io_ptr);
	if (check != size)
		png_error(png, "Read Error");
}

static void image_png_rar_read(png_structp png, png_bytep buf, png_size_t size)
{
	png_size_t check = image_rar_fread(buf, 1, size, png->io_ptr);
	if (check != size)
		png_error(png, "Read Error");
}

extern int image_readpng(const char * filename, dword *pwidth, dword *pheight, pixel ** image_data, pixel * bgcolor)
{
	FILE * fp = fopen(filename, "rb");
	if(fp == NULL)
		return -1;
	int result = image_readpng2(fp, pwidth, pheight, image_data, bgcolor, image_png_read);
	fclose(fp);
	return result;
}

extern int image_readpng_in_zip(const char * zipfile, const char * filename, dword *pwidth, dword *pheight, pixel ** image_data, pixel * bgcolor)
{
	unzFile unzf = unzOpen(zipfile);
	if(unzf == NULL)
		return -1;
	if(unzLocateFile(unzf, filename, 0) != UNZ_OK || unzOpenCurrentFile(unzf) != UNZ_OK)
	{
		unzClose(unzf);
		return -1;
	}
	int result = image_readpng2((void *)unzf, pwidth, pheight, image_data, bgcolor, image_png_zip_read);
	unzCloseCurrentFile(unzf);
	unzClose(unzf);
	return result;
}

extern int image_readpng_in_chm(const char * chmfile, const char * filename, dword *pwidth, dword *pheight, pixel ** image_data, pixel * bgcolor)
{
	t_image_chm chm;
	chm.chm = chm_open(chmfile);
	if(chm.chm == NULL)
		return -1;
    if (chm_resolve_object(chm.chm, filename, &chm.ui) != CHM_RESOLVE_SUCCESS)
	{
		chm_close(chm.chm);
		return -1;
	}
	chm.readpos = 0;
	int result = image_readpng2((void *)&chm, pwidth, pheight, image_data, bgcolor, image_png_chm_read);
	chm_close(chm.chm);
	return result;
}

extern int image_readpng_in_rar(const char * rarfile, const char * filename, dword *pwidth, dword *pheight, pixel ** image_data, pixel * bgcolor)
{
	t_image_rar rar;
	struct RAROpenArchiveData arcdata;
	arcdata.ArcName = (char *)rarfile;
	arcdata.OpenMode = RAR_OM_EXTRACT;
	arcdata.CmtBuf = NULL;
	arcdata.CmtBufSize = 0;
	HANDLE hrar = RAROpenArchive(&arcdata);
	if(hrar == NULL)
		return -1;
	RARSetCallback(hrar, imagerarcbproc, (LONG)&rar);
	do {
		struct RARHeaderData header;
		if(RARReadHeader(hrar, &header) != 0)
			break;
		if(stricmp(header.FileName, filename) == 0)
		{
			rar.size = header.UnpSize;
			rar.idx = 0;
			int e;
			if((rar.buf = (byte *)calloc(1, rar.size)) == NULL || (e = RARProcessFile(hrar, RAR_TEST, NULL, NULL)) != 0)
			{
				RARCloseArchive(hrar);
				return -1;
			}
			RARCloseArchive(hrar);
			rar.idx = 0;
			int result = image_readpng2((void *)&rar, pwidth, pheight, image_data, bgcolor, image_png_rar_read);
			free((void *)rar.buf);
			return result;
		}
	} while(RARProcessFile(hrar, RAR_SKIP, NULL, NULL) == 0);
	RARCloseArchive(hrar);
	return -1;
}

static int image_readgif2(void * handle, dword *pwidth, dword *pheight, pixel ** image_data, pixel * bgcolor, InputFunc readFunc)
{
#define gif_color(c) RGB(palette->Colors[c].Red, palette->Colors[c].Green, palette->Colors[c].Blue)
	GifRecordType RecordType;
	GifByteType *Extension;
	GifRowType LineIn = NULL;
	GifFileType *GifFileIn = NULL;
	ColorMapObject *palette;
	int ExtCode;
	dword i, j;
	if ((GifFileIn = DGifOpen(handle, readFunc)) == NULL)
		return 1;
	*bgcolor = 0;
	*pwidth = 0;
	*pheight = 0;
	*image_data = NULL;

	do {
		if (DGifGetRecordType(GifFileIn, &RecordType) == GIF_ERROR)
		{
			DGifCloseFile(GifFileIn);
			return 1;
		}

		switch (RecordType) {
			case IMAGE_DESC_RECORD_TYPE:
				if (DGifGetImageDesc(GifFileIn) == GIF_ERROR)
				{
					DGifCloseFile(GifFileIn);
					return 1;
				}
				if((palette = (GifFileIn->SColorMap != NULL) ? GifFileIn->SColorMap : GifFileIn->Image.ColorMap) == NULL)
				{
					DGifCloseFile(GifFileIn);
					return 1;
				}
				*pwidth = GifFileIn->Image.Width;
				*pheight = GifFileIn->Image.Height;
				*bgcolor = gif_color(GifFileIn->SBackGroundColor);
				if((LineIn = (GifRowType) malloc(GifFileIn->Image.Width * sizeof(GifPixelType))) == NULL)
				{
					DGifCloseFile(GifFileIn);
					return 1;
				}
				if((*image_data = (pixel *)memalign(16, sizeof(pixel) * GifFileIn->Image.Width * GifFileIn->Image.Height)) == NULL)
				{
					free((void *)LineIn);
					DGifCloseFile(GifFileIn);
					return 1;
				}
				pixel * imgdata = *image_data;
				for (i = 0; i < GifFileIn->Image.Height; i ++) {
					if (DGifGetLine(GifFileIn, LineIn, GifFileIn->Image.Width) == GIF_ERROR)
					{
						free((void *)*image_data);
						free((void *)LineIn);
						DGifCloseFile(GifFileIn);
						return 1;
					}
					for(j = 0; j < GifFileIn->Image.Width; j ++)
						*imgdata++ = gif_color(LineIn[j]);
				}
				break;
			case EXTENSION_RECORD_TYPE:
				if (DGifGetExtension(GifFileIn, &ExtCode, &Extension) == GIF_ERROR)
				{
					if(*image_data != NULL)
						free((void *)*image_data);
					if(LineIn != NULL)
						free((void *)LineIn);
					DGifCloseFile(GifFileIn);
					return 1;
				}
				while (Extension != NULL) {
					if (DGifGetExtensionNext(GifFileIn, &Extension) == GIF_ERROR)
					{
						if(*image_data != NULL)
							free((void *)*image_data);
						if(LineIn != NULL)
							free((void *)LineIn);
						DGifCloseFile(GifFileIn);
						return 1;
					}
				}
				break;
			case TERMINATE_RECORD_TYPE:
				break;
			default:
				break;
		}
	}
	while (RecordType != TERMINATE_RECORD_TYPE);

	if(LineIn != NULL)
		free((void *)LineIn);
	DGifCloseFile(GifFileIn);

	return 0;
}

static int image_gif_read(GifFileType * ft, GifByteType * buf, int size)
{
	return fread(buf, 1, size, (FILE *)ft->UserData);
}

static int image_gif_zip_read(GifFileType * ft, GifByteType * buf, int size)
{
	return image_zip_fread(buf, 1, size, (FILE *)ft->UserData);
}

static int image_gif_chm_read(GifFileType * ft, GifByteType * buf, int size)
{
	return image_chm_fread(buf, 1, size, (FILE *)ft->UserData);
}

static int image_gif_rar_read(GifFileType * ft, GifByteType * buf, int size)
{
	return image_rar_fread(buf, 1, size, (FILE *)ft->UserData);
}

extern int image_readgif(const char * filename, dword *pwidth, dword *pheight, pixel ** image_data, pixel * bgcolor)
{
	FILE * fp = fopen(filename, "rb");
	if(fp == NULL)
		return -1;
	int result = image_readgif2(fp, pwidth, pheight, image_data, bgcolor, image_gif_read);
	fclose(fp);
	return result;
}

extern int image_readgif_in_zip(const char * zipfile, const char * filename, dword *pwidth, dword *pheight, pixel ** image_data, pixel * bgcolor)
{
	unzFile unzf = unzOpen(zipfile);
	if(unzf == NULL)
		return -1;
	if(unzLocateFile(unzf, filename, 0) != UNZ_OK || unzOpenCurrentFile(unzf) != UNZ_OK)
	{
		unzClose(unzf);
		return -1;
	}
	int result = image_readgif2((void *)unzf, pwidth, pheight, image_data, bgcolor, image_gif_zip_read);
	unzCloseCurrentFile(unzf);
	unzClose(unzf);
	return result;
}

extern int image_readgif_in_chm(const char * chmfile, const char * filename, dword *pwidth, dword *pheight, pixel ** image_data, pixel * bgcolor)
{
	t_image_chm chm;
	chm.chm = chm_open(chmfile);
	if(chm.chm == NULL)
		return -1;
    if (chm_resolve_object(chm.chm, filename, &chm.ui) != CHM_RESOLVE_SUCCESS)
	{
		chm_close(chm.chm);
		return -1;
	}
	chm.readpos = 0;
	int result = image_readgif2((void *)&chm, pwidth, pheight, image_data, bgcolor, image_gif_chm_read);
	chm_close(chm.chm);
	return result;
}

extern int image_readgif_in_rar(const char * rarfile, const char * filename, dword *pwidth, dword *pheight, pixel ** image_data, pixel * bgcolor)
{
	t_image_rar rar;
	struct RAROpenArchiveData arcdata;
	arcdata.ArcName = (char *)rarfile;
	arcdata.OpenMode = RAR_OM_EXTRACT;
	arcdata.CmtBuf = NULL;
	arcdata.CmtBufSize = 0;
	HANDLE hrar = RAROpenArchive(&arcdata);
	if(hrar == NULL)
		return -1;
	RARSetCallback(hrar, imagerarcbproc, (LONG)&rar);
	do {
		struct RARHeaderData header;
		if(RARReadHeader(hrar, &header) != 0)
			break;
		if(stricmp(header.FileName, filename) == 0)
		{
			rar.size = header.UnpSize;
			rar.idx = 0;
			int e;
			if((rar.buf = (byte *)calloc(1, rar.size)) == NULL || (e = RARProcessFile(hrar, RAR_TEST, NULL, NULL)) != 0)
			{
				RARCloseArchive(hrar);
				return -1;
			}
			RARCloseArchive(hrar);
			rar.idx = 0;
			int result = image_readgif2((void *)&rar, pwidth, pheight, image_data, bgcolor, image_gif_rar_read);
			free((void *)rar.buf);
			return result;
		}
	} while(RARProcessFile(hrar, RAR_SKIP, NULL, NULL) == 0);
	RARCloseArchive(hrar);
	return -1;
}

static jmp_buf jmp;

static void my_error_exit(j_common_ptr cinfo)
{
	longjmp(jmp, 1);
}

static void output_no_message(j_common_ptr cinfo)
{
}

static int image_readjpg2(FILE *infile, dword *pwidth, dword *pheight, pixel ** image_data, pixel * bgcolor, jpeg_fread jfread)
{
	u64 dbglasttick, dbgnow;
	struct jpeg_decompress_struct cinfo;
	struct jpeg_error_mgr jerr;
	JSAMPROW sline;
	memset(&cinfo, 0, sizeof(struct jpeg_decompress_struct));
	cinfo.err = jpeg_std_error(&jerr);
	jerr.error_exit = my_error_exit;
	jerr.output_message = output_no_message;
	jpeg_create_decompress(&cinfo);
	jpeg_stdio_src(&cinfo, infile, jfread);
	if(setjmp(jmp)) {
		jpeg_destroy_decompress(&cinfo);
		return 1;
	}
	if(jpeg_read_header(&cinfo, TRUE) != JPEG_HEADER_OK)
	{
		jpeg_destroy_decompress(&cinfo);
		return 2;
	}
	cinfo.out_color_space = JCS_RGB;
	cinfo.quantize_colors = FALSE;
	cinfo.scale_num   = 1;
	cinfo.scale_denom = 1;
	cinfo.dct_method = JDCT_FASTEST;
	cinfo.do_fancy_upsampling = FALSE;
	if(!jpeg_start_decompress(&cinfo))
	{
		jpeg_destroy_decompress(&cinfo);
		return 3;
	}
	*bgcolor = 0;
	*pwidth = cinfo.output_width;
	*pheight = cinfo.output_height;
	if((sline = (JSAMPROW)malloc(sizeof(JSAMPLE) * 3 * cinfo.output_width)) == NULL)
	{
		jpeg_abort_decompress(&cinfo);
		jpeg_destroy_decompress(&cinfo);
		return 4;
	}
	if((*image_data = (pixel *)memalign(16, sizeof(pixel) * cinfo.output_width * cinfo.output_height)) == NULL)
	{
		free((void *)sline);
		jpeg_abort_decompress(&cinfo);
		jpeg_destroy_decompress(&cinfo);
		return 5;
	}
	sceRtcGetCurrentTick(&dbglasttick);
	pixel * imgdata = *image_data;
	while(cinfo.output_scanline < cinfo.output_height)
	{
		int i;
		jpeg_read_scanlines(&cinfo, &sline, (JDIMENSION) 1);
		for(i = 0; i < cinfo.output_width; i ++)
			*imgdata++ = RGB(sline[i * 3], sline[i * 3 + 1], sline[i * 3 + 2]);
	}
	sceRtcGetCurrentTick(&dbgnow);
	dbg_printf(d, "提取扫描线完成耗时:%.2f秒", pspDiffTime(&dbgnow, &dbglasttick));
	free((void *)sline);
	jpeg_finish_decompress(&cinfo);
	jpeg_destroy_decompress(&cinfo);
	return 0;
}

extern int exif_readjpg(const char * filename, dword *pwidth, dword *pheight, pixel ** image_data, pixel * bgcolor)
{
	if(!config.load_exif) {
		return -1;
	}

	exif_data = exif_data_new_from_file(filename);
	exif_viewer(exif_data);

	return 0;
}

extern int image_readjpg(const char * filename, dword *pwidth, dword *pheight, pixel ** image_data, pixel * bgcolor)
{
	FILE * fp = fopen(filename, "rb");
	if(fp == NULL)
		return -1;
	int result = image_readjpg2(fp, pwidth, pheight, image_data, bgcolor, NULL);
	fclose(fp);

	if(config.load_exif) {
		exif_data = exif_data_new_from_file(filename);
		exif_viewer(exif_data);
	}

	return result;
}

extern int exif_readjpg_in_zip(const char * zipfile, const char * filename, dword *pwidth, dword *pheight, pixel ** image_data, pixel * bgcolor)
{
	if(!config.load_exif)
		return -1;

	unzFile unzf = unzOpen(zipfile);
	if(unzf == NULL)
		return -1;
	if(unzLocateFile(unzf, filename, 0) != UNZ_OK || unzOpenCurrentFile(unzf) != UNZ_OK)
	{
		unzClose(unzf);
		return -1;
	}

	exif_data = exif_data_new_from_stream(image_zip_fread, unzf);
	exif_viewer(exif_data);

	unzCloseCurrentFile(unzf);
	unzClose(unzf);
	return 0;
}

extern int image_readjpg_in_zip(const char * zipfile, const char * filename, dword *pwidth, dword *pheight, pixel ** image_data, pixel * bgcolor)
{
	unzFile unzf = unzOpen(zipfile);
	if(unzf == NULL)
		return -1;
	if(unzLocateFile(unzf, filename, 0) != UNZ_OK || unzOpenCurrentFile(unzf) != UNZ_OK)
	{
		unzClose(unzf);
		return -1;
	}
	int result = image_readjpg2((FILE *)unzf, pwidth, pheight, image_data, bgcolor, image_zip_fread);

	if(config.load_exif) {
		unzCloseCurrentFile(unzf);
		unzClose(unzf);

		unzf = unzOpen(zipfile);
		if(unzf == NULL)
			return -1;
		if(unzLocateFile(unzf, filename, 0) != UNZ_OK || unzOpenCurrentFile(unzf) != UNZ_OK)
		{
			unzClose(unzf);
			return -1;
		}

		exif_data = exif_data_new_from_stream(image_zip_fread, unzf);
		exif_viewer(exif_data);
	}

	unzCloseCurrentFile(unzf);
	unzClose(unzf);
	return result;
}

extern int exif_readjpg_in_chm(const char * chmfile, const char * filename, dword *pwidth, dword *pheight, pixel ** image_data, pixel * bgcolor)
{
	if(!config.load_exif) {
		return -1;
	}

	t_image_chm chm;
	chm.chm = chm_open(chmfile);
	if(chm.chm == NULL)
		return -1;
	if (chm_resolve_object(chm.chm, filename, &chm.ui) != CHM_RESOLVE_SUCCESS)
	{
		chm_close(chm.chm);
		return -1;
	}
	chm.readpos = 0;
	exif_data = exif_data_new_from_stream(image_chm_fread, &chm);
	exif_viewer(exif_data);
	chm_close(chm.chm);
	return 0;
}

extern int image_readjpg_in_chm(const char * chmfile, const char * filename, dword *pwidth, dword *pheight, pixel ** image_data, pixel * bgcolor)
{
	t_image_chm chm;
	chm.chm = chm_open(chmfile);
	if(chm.chm == NULL)
		return -1;
    if (chm_resolve_object(chm.chm, filename, &chm.ui) != CHM_RESOLVE_SUCCESS)
	{
		chm_close(chm.chm);
		return -1;
	}
	chm.readpos = 0;
	int result = image_readjpg2((FILE *)&chm, pwidth, pheight, image_data, bgcolor, image_chm_fread);
	if(config.load_exif) {
		chm.readpos = 0;
		exif_data = exif_data_new_from_stream(image_chm_fread, &chm);
		exif_viewer(exif_data);
	}
	chm_close(chm.chm);
	return result;
}

extern int exif_readjpg_in_rar(const char * rarfile, const char * filename, dword *pwidth, dword *pheight, pixel ** image_data, pixel * bgcolor)
{
	if(!config.load_exif) {
		return -1;
	}
	
	u64 dbglasttick, dbgnow;
	t_image_rar rar;
	struct RAROpenArchiveData arcdata;
	arcdata.ArcName = (char *)rarfile;
	arcdata.OpenMode = RAR_OM_EXTRACT;
	arcdata.CmtBuf = NULL;
	arcdata.CmtBufSize = 0;
	sceRtcGetCurrentTick(&dbglasttick);
	HANDLE hrar = RAROpenArchive(&arcdata);
	if(hrar == NULL)
		return -1;
	RARSetCallback(hrar, imagerarcbproc, (LONG)&rar);
	do {
		struct RARHeaderData header;
		if(RARReadHeader(hrar, &header) != 0)
			break;
		if(stricmp(header.FileName, filename) == 0)
		{
			rar.size = header.UnpSize;
			rar.idx = 0;
			int e;
			if((rar.buf = (byte *)calloc(1, rar.size)) == NULL || (e = RARProcessFile(hrar, RAR_TEST, NULL, NULL)) != 0)
			{
				RARCloseArchive(hrar);
				return -1;
			}
			RARCloseArchive(hrar);
			rar.idx = 0;
			sceRtcGetCurrentTick(&dbgnow);
			dbg_printf(d, "找到RAR中JPG文件耗时%.2f秒", pspDiffTime(&dbgnow, &dbglasttick));
			exif_data = exif_data_new_from_data(rar.buf, rar.size);
			exif_viewer(exif_data);
			free((void *)rar.buf);
			return 0;
		}
	} while(RARProcessFile(hrar, RAR_SKIP, NULL, NULL) == 0);

	RARCloseArchive(hrar);
	return -1;
}

extern int image_readjpg_in_rar(const char * rarfile, const char * filename, dword *pwidth, dword *pheight, pixel ** image_data, pixel * bgcolor)
{
	u64 dbglasttick, dbgnow;
	t_image_rar rar;
	struct RAROpenArchiveData arcdata;
	arcdata.ArcName = (char *)rarfile;
	arcdata.OpenMode = RAR_OM_EXTRACT;
	arcdata.CmtBuf = NULL;
	arcdata.CmtBufSize = 0;
	sceRtcGetCurrentTick(&dbglasttick);
	HANDLE hrar = RAROpenArchive(&arcdata);
	if(hrar == NULL)
		return -1;
	RARSetCallback(hrar, imagerarcbproc, (LONG)&rar);
	do {
		struct RARHeaderData header;
		if(RARReadHeader(hrar, &header) != 0)
			break;
		if(stricmp(header.FileName, filename) == 0)
		{
			rar.size = header.UnpSize;
			rar.idx = 0;
			int e;
			if((rar.buf = (byte *)calloc(1, rar.size)) == NULL || (e = RARProcessFile(hrar, RAR_TEST, NULL, NULL)) != 0)
			{
				RARCloseArchive(hrar);
				return -1;
			}
			RARCloseArchive(hrar);
			rar.idx = 0;
			sceRtcGetCurrentTick(&dbgnow);
			dbg_printf(d, "找到RAR中JPG文件耗时%.2f秒", pspDiffTime(&dbgnow, &dbglasttick));
			int result = image_readjpg2((FILE *)&rar, pwidth, pheight, image_data, bgcolor, image_rar_fread);
			if(config.load_exif) {
				exif_data = exif_data_new_from_data(rar.buf, rar.size);
				exif_viewer(exif_data);
			}
			free((void *)rar.buf);
			return result;
		}
	} while(RARProcessFile(hrar, RAR_SKIP, NULL, NULL) == 0);

	RARCloseArchive(hrar);
	return -1;
}

static int image_bmp_to_32color( DIB dib, dword * width, dword * height, pixel ** imgdata)  {
    BITMAPINFOHEADER *bi;
    int row_bytes, i, j;
    unsigned char *src, *pixel_data;
	pixel *dest;
    DIB tmp_dib = NULL;
    RGBQUAD *palette;

    bi = (BITMAPINFOHEADER *)dib;
	*imgdata = (pixel *)memalign(16, sizeof(pixel) * bi->biWidth * bi->biHeight);
	if(*imgdata == NULL)
		return 1;

    if( bi->biCompression == BI_RLE8 ||
        bi->biCompression == BI_RLE4
    ) {
        tmp_dib = bmp_expand_dib_rle( dib );
        if( tmp_dib == NULL ) {
			free(*imgdata);
			*imgdata = NULL;
            return 1;
        }
        dib = tmp_dib;
    }
	*width = bi->biWidth;
	*height = bi->biHeight;

    palette = (RGBQUAD *)(dib + sizeof(BITMAPINFOHEADER));
    pixel_data = (unsigned char *)palette + bi->biClrUsed * sizeof(RGBQUAD);
    row_bytes = (bi->biBitCount * bi->biWidth + 31 ) / 32 * 4;

    for( i = 0; i < bi->biHeight; i++ ) {
        int   pos;
        src  = &pixel_data[ (bi->biHeight - i - 1) * row_bytes ];
        dest = (*imgdata) + i * bi->biWidth;
        switch( bi->biBitCount ) {
          case 1:
            for( j = 0; j < bi->biWidth; j++ ) {
                if( *src & ( 0x80 >> (i % 8) ) ) pos = 1;
                else pos = 0;
                if( ( i % 8 ) == 7) src++;
                *dest++ = RGB(palette[ pos ].rgbRed, palette[ pos ].rgbGreen, palette[ pos ].rgbBlue);
            }
            break;
          case 4:
            for( j = 0; j < bi->biWidth; j++ ) {
                pos = *src;
                if( (j % 2) == 0 ) pos >>= 4;
                else  { pos &= 0x0F; src++; }
                *dest++ = RGB(palette[ pos ].rgbRed, palette[ pos ].rgbGreen, palette[ pos ].rgbBlue);
            }
            break;
          case 8:
            for( j = 0; j < bi->biWidth; j++ ) {
                pos = *src++;
                *dest++ = RGB(palette[ pos ].rgbRed, palette[ pos ].rgbGreen, palette[ pos ].rgbBlue);
            }
            break;
		  case 15:
          case 16:
            for( j = 0; j < bi->biWidth; j++ ) {
                ushort rgb = *(ushort *)src;
                src += 2;
                *dest++ = RGB_16to32(rgb);
            }
            break;
          case 24:
            for( j = 0; j < bi->biWidth; j++ ) {
                *dest++ = RGB(*(src + 2), *(src + 1), *src);
                src += 3;
            }
			break;
          case 32:
            for( j = 0; j < bi->biWidth; j++ ) {
                *dest++ = RGB(*(src + 2), *(src + 1), *src);
                src += 4;
            }
            break;
          default:
            for( j = 0; j < bi->biWidth; j++ )
			  *dest++ = 0;
            break;
        }
    }

    if( tmp_dib ) free( tmp_dib );

	return 0;
}

static int image_readbmp2(void * handle, dword *pwidth, dword *pheight, pixel ** image_data, pixel * bgcolor, t_bmp_fread readfn)
{
	DIB bmp;
	bmp = bmp_read_dib_file(handle, readfn);
	if(bmp == NULL)
		return 1;
	*bgcolor = 0;
	int result = image_bmp_to_32color(bmp, pwidth, pheight, image_data);
	free((void *)bmp);
	return result;
}

extern int image_readbmp(const char * filename, dword *pwidth, dword *pheight, pixel ** image_data, pixel * bgcolor)
{
	FILE * fp = fopen(filename, "rb");
	if(fp == NULL)
		return -1;
	int result = image_readbmp2(fp, pwidth, pheight, image_data, bgcolor, NULL);
	fclose(fp);
	return result;
}

extern int image_readbmp_in_zip(const char * zipfile, const char * filename, dword *pwidth, dword *pheight, pixel ** image_data, pixel * bgcolor)
{
	unzFile unzf = unzOpen(zipfile);
	if(unzf == NULL)
		return -1;
	if(unzLocateFile(unzf, filename, 0) != UNZ_OK || unzOpenCurrentFile(unzf) != UNZ_OK)
	{
		unzClose(unzf);
		return -1;
	}
	int result = image_readbmp2((FILE *)unzf, pwidth, pheight, image_data, bgcolor, image_zip_fread);
	unzCloseCurrentFile(unzf);
	unzClose(unzf);
	return result;
}

extern int image_readbmp_in_chm(const char * chmfile, const char * filename, dword *pwidth, dword *pheight, pixel ** image_data, pixel * bgcolor)
{
	t_image_chm chm;
	chm.chm = chm_open(chmfile);
	if(chm.chm == NULL)
		return -1;
    if (chm_resolve_object(chm.chm, filename, &chm.ui) != CHM_RESOLVE_SUCCESS)
	{
		chm_close(chm.chm);
		return -1;
	}
	chm.readpos = 0;
	int result = image_readbmp2((FILE *)&chm, pwidth, pheight, image_data, bgcolor, image_chm_fread);
	chm_close(chm.chm);
	return result;
}

extern int image_readbmp_in_rar(const char * rarfile, const char * filename, dword *pwidth, dword *pheight, pixel ** image_data, pixel * bgcolor)
{
	t_image_rar rar;
	struct RAROpenArchiveData arcdata;
	arcdata.ArcName = (char *)rarfile;
	arcdata.OpenMode = RAR_OM_EXTRACT;
	arcdata.CmtBuf = NULL;
	arcdata.CmtBufSize = 0;
	HANDLE hrar = RAROpenArchive(&arcdata);
	if(hrar == NULL)
		return -1;
	RARSetCallback(hrar, imagerarcbproc, (LONG)&rar);
	do {
		struct RARHeaderData header;
		if(RARReadHeader(hrar, &header) != 0)
			break;
		if(stricmp(header.FileName, filename) == 0)
		{
			rar.size = header.UnpSize;
			rar.idx = 0;
			int e;
			if((rar.buf = (byte *)calloc(1, rar.size)) == NULL || (e = RARProcessFile(hrar, RAR_TEST, NULL, NULL)) != 0)
			{
				RARCloseArchive(hrar);
				return -1;
			}
			RARCloseArchive(hrar);
			rar.idx = 0;
			int result = image_readbmp2((FILE *)&rar, pwidth, pheight, image_data, bgcolor, image_rar_fread);
			free((void *)rar.buf);
			return result;
		}
	} while(RARProcessFile(hrar, RAR_SKIP, NULL, NULL) == 0);
	RARCloseArchive(hrar);
	return -1;
}

static void image_freetgadata(TGAData * data)
{
	if(data->img_id != NULL)
		free((void *)data->img_id);
	if(data->cmap != NULL)
		free((void *)data->cmap);
	if(data->img_data != NULL);
		free((void *)data->img_data);
	free((void *)data);
}

static int image_readtga2(void * handle, dword *pwidth, dword *pheight, pixel ** image_data, pixel * bgcolor, TGAfread nfread, TGAfseek nfseek, TGAftell nftell)
{
	TGA *in;
	TGAData *data;

	if((data = (TGAData*)calloc(1, sizeof(TGAData))) == NULL)
		return 1;
	in = TGAOpenFd(handle, nfread, nfseek, nftell);
	if(in == NULL)
	{
		image_freetgadata(data);
		return 1;
	}
	data->flags = TGA_IMAGE_ID | TGA_IMAGE_DATA | TGA_RGB;
	if(in->last != TGA_OK || TGAReadImage(in, data) != TGA_OK)
	{
		TGAClose(in);
		image_freetgadata(data);
		return 1;
	}

	*pwidth = in->hdr.width;
	*pheight = in->hdr.height;
	*bgcolor = 0;
	if((*image_data = (pixel *)memalign(16, sizeof(pixel) * *pwidth * *pheight)) == NULL)
	{
		TGAClose(in);
		image_freetgadata(data);
		return 1;
	}

	byte * srcdata = data->img_data;
	pixel * imgdata = (*image_data) + in->hdr.x + in->hdr.y * *pwidth;
	if(in->hdr.x == 0)
		in->hdr.horz = TGA_LEFT;
	if(in->hdr.y == 0)
		in->hdr.vert = TGA_TOP;
	int i, j;
	for(j = 0; j < *pheight; j ++)
	{
		for(i = 0; i < *pwidth; i ++)
		{
			switch(in->hdr.depth)
			{
			case 32:
				*imgdata = RGB(*srcdata, *(srcdata + 1), *(srcdata + 2));
				srcdata += 4;
				break;
			case 8:
				*imgdata = (*srcdata > 0) ? 0xFFFFFFFF : 0;
				srcdata ++;
				break;
			default:
				*imgdata = RGB(*srcdata * 255 / 31, *(srcdata + 1) * 255 / 31, *(srcdata + 2) * 255 / 31);
				srcdata += 3;
			}
			if(in->hdr.horz == TGA_LEFT)
				imgdata ++;
			else
				imgdata --;
		}
		if(in->hdr.horz == TGA_LEFT && in->hdr.vert == TGA_BOTTOM)
			imgdata -= 2 * *pwidth;
		else if(in->hdr.horz == TGA_RIGHT && in->hdr.vert == TGA_TOP)
			imgdata += 2 * *pwidth;
	}

	free(in);
	image_freetgadata(data);
	return 0;
}

extern int image_readtga(const char * filename, dword *pwidth, dword *pheight, pixel ** image_data, pixel * bgcolor)
{
	FILE * fp = fopen(filename, "rb");
	if(fp == NULL)
		return -1;
	int result = image_readtga2(fp, pwidth, pheight, image_data, bgcolor, NULL, NULL, NULL);
	fclose(fp);
	return result;
}

extern int image_readtga_in_zip(const char * zipfile, const char * filename, dword *pwidth, dword *pheight, pixel ** image_data, pixel * bgcolor)
{
	unzFile unzf = unzOpen(zipfile);
	if(unzf == NULL)
		return -1;
	if(unzLocateFile(unzf, filename, 0) != UNZ_OK || unzOpenCurrentFile(unzf) != UNZ_OK)
	{
		unzClose(unzf);
		return -1;
	}
	int result = image_readtga2((FILE *)unzf, pwidth, pheight, image_data, bgcolor, image_zip_fread, image_zip_fseek, image_zip_ftell);
	unzCloseCurrentFile(unzf);
	unzClose(unzf);
	return result;
}

extern int image_readtga_in_chm(const char * chmfile, const char * filename, dword *pwidth, dword *pheight, pixel ** image_data, pixel * bgcolor)
{
	t_image_chm chm;
	chm.chm = chm_open(chmfile);
	if(chm.chm == NULL)
		return -1;
    if (chm_resolve_object(chm.chm, filename, &chm.ui) != CHM_RESOLVE_SUCCESS)
	{
		chm_close(chm.chm);
		return -1;
	}
	chm.readpos = 0;
	int result = image_readtga2((FILE *)&chm, pwidth, pheight, image_data, bgcolor, image_chm_fread, image_chm_fseek, image_chm_ftell);
	chm_close(chm.chm);
	return result;
}

extern int image_readtga_in_rar(const char * rarfile, const char * filename, dword *pwidth, dword *pheight, pixel ** image_data, pixel * bgcolor)
{
	t_image_rar rar;
	struct RAROpenArchiveData arcdata;
	arcdata.ArcName = (char *)rarfile;
	arcdata.OpenMode = RAR_OM_EXTRACT;
	arcdata.CmtBuf = NULL;
	arcdata.CmtBufSize = 0;
	HANDLE hrar = RAROpenArchive(&arcdata);
	if(hrar == NULL)
		return -1;
	RARSetCallback(hrar, imagerarcbproc, (LONG)&rar);
	do {
		struct RARHeaderData header;
		if(RARReadHeader(hrar, &header) != 0)
			break;
		if(stricmp(header.FileName, filename) == 0)
		{
			rar.size = header.UnpSize;
			rar.idx = 0;
			int e;
			if((rar.buf = (byte *)calloc(1, rar.size)) == NULL || (e = RARProcessFile(hrar, RAR_TEST, NULL, NULL)) != 0)
			{
				RARCloseArchive(hrar);
				return -1;
			}
			RARCloseArchive(hrar);
			rar.idx = 0;
			int result = image_readtga2((FILE *)&rar, pwidth, pheight, image_data, bgcolor, image_rar_fread, image_rar_fseek, image_rar_ftell);
			free((void *)rar.buf);
			return result;
		}
	} while(RARProcessFile(hrar, RAR_SKIP, NULL, NULL) == 0);
	RARCloseArchive(hrar);
	return -1;
}

void exif_entry_viewer (ExifEntry *pentry, void *user_data)
{
	if(pentry == 0)
		return;
;
	char exif_str[512];
	exif_entry_get_value(pentry, exif_str, 512);
	exif_str[511] = '\0';

	if(exif_str[0] == '\0')
		return;

	ExifIfd ifd = exif_entry_get_ifd(pentry);
	char msg[512];
	STRCPY_S(msg, exif_tag_get_title_in_ifd(pentry->tag, ifd));
	strncat(msg, ": ", 512 - strlen(msg) - 1);
	strncat(msg, exif_str, 512 - strlen(msg) - 1);

	buffer *b = buffer_array_append_get_buffer(exif_array);
	buffer_copy_string(b, msg);

//	win_msg(msg, COLOR_WHITE, COLOR_WHITE, config.msgbcolor);
}

void exif_context_viewer(ExifContent *pcontext, void *user_data)
{
	if(pcontext == NULL)
		return;

	exif_content_foreach_entry(pcontext, exif_entry_viewer, 0);
}

void exif_viewer(ExifData *data)
{
	if(exif_array) {
		buffer_array_reset(exif_array);
	}
	else {
		exif_array = buffer_array_init();
	}
	if(data) {
//		win_msg("得到JPEG exif数据!", COLOR_WHITE, COLOR_WHITE, config.msgbcolor);
		// 打印所有EXIF数据
		exif_data_foreach_content(data, exif_context_viewer, 0);
		exif_data_free(data);
	}
}

#endif
