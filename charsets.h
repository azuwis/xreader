/* vim:set ts=4 sw=4 cindent ignorecase enc=gbk: */

#ifndef _CHARSETS_H_
#define _CHARSETS_HH

#include "common/datatype.h"

extern dword charsets_utf32_conv(const byte * ucs, byte * cjk);
extern dword charsets_ucs_conv(const byte * uni, byte * cjk);
extern void charsets_big5_conv(const byte * big5, byte * cjk);
extern void charsets_sjis_conv(const byte * jis, byte ** cjk, dword * newsize);
extern dword charsets_utf8_conv(const byte * ucs, byte * cjk);
extern dword charsets_utf16_conv(const byte * ucs, byte * cjk);
extern dword charsets_utf16be_conv(const byte * ucs, byte * cjk);
extern word charsets_gbk_to_ucs(const byte * cjk);

#endif
