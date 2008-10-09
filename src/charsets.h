/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#ifndef _CHARSETS_H_
#define _CHARSETS_H_

#include "common/datatype.h"

typedef unsigned long ucs4_t;

extern dword charsets_utf32_conv(const byte * ucs, size_t inputlen, byte * cjk,
								 size_t outputlen);
extern dword charsets_ucs_conv(const byte * ucs, size_t inputlen, byte * cjk,
							   size_t outputlen);
extern dword charsets_big5_conv(const byte * big5, size_t inputlen, byte * cjk,
								size_t outputlen);
extern void charsets_sjis_conv(const byte * jis, byte ** cjk, dword * newsize);
extern dword charsets_utf8_conv(const byte * ucs, size_t inputlen, byte * cjk,
								size_t outputlen);
extern dword charsets_utf16_conv(const byte * ucs, size_t inputlen, byte * cjk,
								 size_t outputlen);
extern dword charsets_utf16be_conv(const byte * ucs, size_t inputlen,
								   byte * cjk, size_t outputlen);
extern int gbk_mbtowc(ucs4_t * pwc, const byte * cjk, int n);
extern int gbk_wctomb(byte * r, ucs4_t wc, int n);
extern int utf8_mbtowc(ucs4_t * pwc, const byte * s, int n);

extern word charsets_gbk_to_ucs(const byte * cjk);
extern dword charsets_bg5hk2cjk(const byte * big5hk, size_t inputlen,
								byte * cjk, size_t outputlen);

#endif
