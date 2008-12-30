#ifndef MP3INFO_MACRO_H
#define MP3INFO_MACRO_H

#define FFMIN(a,b) ((a) > (b) ? (b) : (a))

#define PUT_UTF8(val, tmp, PUT_BYTE)\
do {\
	int bytes, shift;\
	uint32_t in = val;\
	if (in < 0x80) {\
		tmp = in;\
		PUT_BYTE\
	} else {\
		bytes = (av_log2(in) + 4) / 5;\
		shift = (bytes - 1) * 6;\
		tmp = (256 - (256 >> bytes)) | (in >> shift);\
		PUT_BYTE\
		while (shift >= 6) {\
			shift -= 6;\
			tmp = 0x80 | ((in >> shift) & 0x3f);\
			PUT_BYTE\
		}\
	}\
} while ( 0 )

#endif
