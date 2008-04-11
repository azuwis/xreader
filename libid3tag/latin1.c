/*
 * libid3tag - ID3 tag manipulation library
 * Copyright (C) 2000-2004 Underbit Technologies, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * $Id: latin1.c,v 1.10 2004/01/23 09:41:32 rob Exp $
 */

# ifdef HAVE_CONFIG_H
#  include "config.h"
# endif

# include "global.h"

# include <stdlib.h>

# include "id3tag.h"
# include "latin1.h"
# include "ucs4.h"
# include "utf8.h"
# ifdef PSP
# include "config.h"
# include "../common/datatype.h"
# include "../charsets.h"
# include <string.h>
# else
# include <windows.h>
# endif

static id3_byte_t *ConvertLocalStringToUTF8(const char *pData, int size)
{
#ifndef PSP
	size_t n = MultiByteToWideChar(CP_OEMCP, 0, pData, (int) size, NULL, 0);
	WCHAR *pChar = (WCHAR *) malloc((n + 1) * sizeof(WCHAR));

	n = MultiByteToWideChar(CP_OEMCP, 0, pData, (int) size, pChar, n);
	pChar[n] = 0;
	n = WideCharToMultiByte(CP_UTF8, 0, pChar, -1, 0, 0, 0, 0);
	char *p = (char *) malloc((n + 1) * sizeof(char));

	n = WideCharToMultiByte(CP_UTF8, 0, pChar, -1, p, n, 0, 0);
	free(pChar);
	return (id3_byte_t *) p;
#else
	/*
	   typedef unsigned long dword;
	   dword *t = (dword*) malloc(sizeof(dword) * size);
	   charsets_gbk_to_ucs_conv(pData, t);
	   return (id3_byte_t*) t;
	 */

	return (id3_byte_t *) strndup(pData, size);
#endif
}

/*
 * NAME:	latin1->length()
 * DESCRIPTION:	return the number of ucs4 chars represented by a latin1 string
 */
id3_length_t id3_latin1_length(id3_latin1_t const *latin1)
{
	id3_latin1_t const *ptr = latin1;

	while (*ptr)
		++ptr;

	return ptr - latin1;
}

/*
 * NAME:	latin1->size()
 * DESCRIPTION:	return the encoding size of a latin1 string
 */
id3_length_t id3_latin1_size(id3_latin1_t const *latin1)
{
	return id3_latin1_length(latin1) + 1;
}

/*
 * NAME:	latin1->copy()
 * DESCRIPTION:	copy a latin1 string
 */
void id3_latin1_copy(id3_latin1_t * dest, id3_latin1_t const *src)
{
	while ((*dest++ = *src++));
}

/*
 * NAME:	latin1->duplicate()
 * DESCRIPTION:	duplicate a latin1 string
 */
id3_latin1_t *id3_latin1_duplicate(id3_latin1_t const *src)
{
	id3_latin1_t *latin1;

	latin1 = malloc(id3_latin1_size(src) * sizeof(*latin1));
	if (latin1)
		id3_latin1_copy(latin1, src);

	return latin1;
}

/*
 * NAME:	latin1->ucs4duplicate()
 * DESCRIPTION:	duplicate and decode a latin1 string into ucs4
 */
id3_ucs4_t *id3_latin1_ucs4duplicate(id3_latin1_t const *latin1)
{
	id3_ucs4_t *ucs4;

	ucs4 = malloc((id3_latin1_length(latin1) + 1) * sizeof(*ucs4));
	if (ucs4)
		id3_latin1_decode(latin1, ucs4);

	return release(ucs4);
}

/*
 * NAME:	latin1->decodechar()
 * DESCRIPTION:	decode a (single) latin1 char into a single ucs4 char
 */
id3_length_t id3_latin1_decodechar(id3_latin1_t const *latin1,
								   id3_ucs4_t * ucs4)
{
	*ucs4 = *latin1;

	return 1;
}

/*
 * NAME:	latin1->encodechar()
 * DESCRIPTION:	encode a single ucs4 char into a (single) latin1 char
 */
id3_length_t id3_latin1_encodechar(id3_latin1_t * latin1, id3_ucs4_t ucs4)
{
	*latin1 = ucs4;
	if (ucs4 > 0x000000ffL)
		*latin1 = ID3_UCS4_REPLACEMENTCHAR;

	return 1;
}

/*
 * NAME:	latin1->decode()
 * DESCRIPTION:	decode a complete latin1 string into a ucs4 string
 * @Hacked BY Hrimfaxi: treat ISO_8859_1 as LOCAL SYSTEM ENCODING
 */
void id3_latin1_decode(id3_latin1_t const *latin1, id3_ucs4_t * ucs4)
{
	/*
	   id3_byte_t *utf8 = ConvertLocalStringToUTF8((const char*)latin1, id3_latin1_size(latin1));

	   if (utf8 == NULL)
	   return;

	   id3_ucs4_t *ucs4t = id3_utf8_deserialize((id3_byte_t const **)&utf8, id3_utf8_size((id3_utf8_t*)utf8));

	   id3_ucs4_ncopy(ucs4, ucs4t, 31);

	   free(utf8);
	 */
	do
		latin1 += id3_latin1_decodechar(latin1, ucs4);
	while (*ucs4++);
}

/*
 * NAME:	latin1->encode()
 * DESCRIPTION:	encode a complete ucs4 string into a latin1 string
 */
void id3_latin1_encode(id3_latin1_t * latin1, id3_ucs4_t const *ucs4)
{
	do
		latin1 += id3_latin1_encodechar(latin1, *ucs4);
	while (*ucs4++);
}

/*
 * NAME:	latin1->put()
 * DESCRIPTION:	serialize a single latin1 character
 */
id3_length_t id3_latin1_put(id3_byte_t ** ptr, id3_latin1_t latin1)
{
	if (ptr)
		*(*ptr)++ = latin1;

	return 1;
}

/*
 * NAME:	latin1->get()
 * DESCRIPTION:	deserialize a single latin1 character
 */
id3_latin1_t id3_latin1_get(id3_byte_t const **ptr)
{
	return *(*ptr)++;
}

/*
 * NAME:	latin1->serialize()
 * DESCRIPTION:	serialize a ucs4 string using latin1 encoding
 */
id3_length_t id3_latin1_serialize(id3_byte_t ** ptr, id3_ucs4_t const *ucs4,
								  int terminate)
{
	id3_length_t size = 0;
	id3_latin1_t latin1[1], *out;

	while (*ucs4) {
		switch (id3_latin1_encodechar(out = latin1, *ucs4++)) {
			case 1:
				size += id3_latin1_put(ptr, *out++);
			case 0:
				break;
		}
	}

	if (terminate)
		size += id3_latin1_put(ptr, 0);

	return size;
}

/*
 * NAME:	latin1->deserialize()
 * DESCRIPTION:	deserialize a ucs4 string using latin1 encoding
 * @Hacked BY Hrimfaxi: treat ISO_8859_1 as LOCAL SYSTEM ENCODING
 */
id3_ucs4_t *id3_latin1_deserialize(id3_byte_t const **ptr, id3_length_t length)
{
	/*
	   id3_byte_t *utf8 = ConvertLocalStringToUTF8((const char*)*ptr, length);

	   if (utf8 == NULL)
	   return NULL;

	   id3_ucs4_t *ucs4 = id3_utf8_deserialize((id3_byte_t const **)&utf8, id3_utf8_size((id3_utf8_t*)utf8));

	   free(utf8);

	   return ucs4;
	 */
	id3_byte_t const *end;
	id3_latin1_t *latin1ptr, *latin1;
	id3_ucs4_t *ucs4;

	end = *ptr + length;

	latin1 = malloc((length + 1) * sizeof(*latin1));
	if (latin1 == 0)
		return 0;

	latin1ptr = latin1;
	while (end - *ptr > 0 && (*latin1ptr = id3_latin1_get(ptr)))
		++latin1ptr;

	*latin1ptr = 0;

	ucs4 = malloc((id3_latin1_length(latin1) + 1) * sizeof(*ucs4));
	if (ucs4)
		id3_latin1_decode(latin1, ucs4);

	free(latin1);

	return ucs4;
}
