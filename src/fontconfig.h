#ifndef XR_FONTCONFIG_H
#define XR_FONTCONFIG_H

#include "common/datatype.h"

typedef struct _font_config {
	const char* fontname;
	bool globaladvance;
	bool antialias;
	bool hinting;
	int hintstyle;
	bool autohint;
	int pixelsize;
	bool embolden;
	bool cleartype;
} font_config;

enum {
	TOKEN_IF,			/* if */
	TOKEN_THEN,			/* then*/
	TOKEN_ELSE,			/* else */
	TOKEN_END,			/* end */
	TOKEN_EQUAL,		/* = */
	TOKEN_COMP,			/* == */
	TOKEN_NOT,			/* ! */
	TOKEN_NOTEQUAL,		/* != */
	TOKEN_GT,			/* > */
	TOKEN_LT,			/* < */
	TOKEN_NOTLT,		/* >= */
	TOKEN_NOTGT,		/* <= */
	TOKEN_BRACKET,		/* () */
	TOKEN_AND,			/* AND */
	TOKEN_OR,			/* OR */
	TOKEN_VAR,			/* Variable name */
	TOKEN_BOOL,			/* true/false */
	TOKEN_NUM,			/* 0123 */
	TOKEN_STRING,		/* "Hello world" */
};

enum {
	TOKEN_BRACKET_LEFT = 0,			/* ( */
	TOKEN_BRACKET_RIGHT				/* ) */
};

enum {
	TOKEN_COMPARE_FALSE = 0,
	TOKEN_COMPARE_TRUE 
};

typedef struct _token {
	int type;

	void *value;
	size_t value_size;
} token;

int new_font_config(const char* fontname, font_config *p);
int get_font_config(font_config *p);
int report_font_config(font_config* p);

#endif
