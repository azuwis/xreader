#ifndef _UNUMD_
#define _UNUMD_
#include "common/datatype.h"
#include "buffer.h"
#include <stdio.h>

struct UMDHeaderData
{
	char Mark;
	unsigned short hdType;
	byte Flag;
	byte Length;
} __attribute__ ((packed));

struct UMDHeaderDataEx
{
	char Mark;
	unsigned int CheckNum;
	unsigned int Length;
} __attribute__ ((packed));

struct t_chapter
{
	size_t chunk_pos;
	size_t chunk_offset;
	size_t length;
	buffer *name;
} __attribute__ ((packed));

struct _t_umd_chapter
{
	buffer *umdfile;
	unsigned short umd_type;
	unsigned short umd_mode;
	unsigned int chapter_count;
	size_t filesize;
	size_t content_pos;
	struct t_chapter *pchapters;
	//buffer*           pcontents;
} __attribute__ ((packed));
typedef struct _t_umd_chapter t_umd_chapter, *p_umd_chapter;

//extern p_umd_chapter p_umdchapter;
extern p_umd_chapter umd_chapter_init();
extern void umd_chapter_reset(p_umd_chapter pchap);
extern void umd_chapter_free(p_umd_chapter pchap);
extern int umd_readdata(char **pf, buffer ** buf);
extern int umd_getchapter(char **pf, p_umd_chapter * pchapter);
extern int parse_umd_chapters(const char *umdfile, p_umd_chapter * pchapter);
extern int locate_umd_img(const char *umdfile, size_t file_offset, FILE ** fp);
extern int read_umd_chapter_content(const char *umdfile, u_int index,
									p_umd_chapter pchapter
									/*,size_t file_offset,size_t length */ ,
									buffer ** pbuf);
#endif
