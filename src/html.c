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

#include "config.h"

#include <stdio.h>
#include <string.h>
#include "common/utils.h"
#include "html.h"

static char *html_skip_spaces(char *string)
{
	char *res = string;

	while (*res == '\r' || *res == '\n' || *res == ' ' || *res == '\t')
		res++;
	return res;
}

static char *html_find_next_tag(char *string, char **tag, char **property,
								bool * istag, bool stripeol)
{
	char *res = string;
	bool inquote = false;
	char quote = ' ';

	*property = NULL;
	while (*res != 0 && *res != '<' && *res != '&') {
		if (stripeol
			&& (*res == ' ' || *res == '\t' || *res == '\r' || *res == '\n')) {
			if (res > string && *(res - 1) == ' ')
				*res = 0x1F;
			else
				*res = ' ';
		}
		res++;
	}
	if (*res == 0)
		return NULL;
	if (*res == '<') {
		*res++ = 0;
		res = html_skip_spaces(res);
		*tag = res;
		while (inquote || (*res != 0 && *res != '>')) {
			if (*res == '"' || *res == '\'') {
				if (!inquote) {
					inquote = true;
					quote = *res;
				} else if (quote == *res)
					inquote = false;
				res++;
			} else if (!inquote
					   && (*res == ' ' || *res == '\t'
						   || *res == '\r' || *res == '\n')) {
				*res = 0;
				res++;
				res = html_skip_spaces(res);
				if (*res != '>')
					*property = res;
			} else
				res++;
		}
		if (*res == 0)
			return NULL;
		*res++ = 0;
		res = html_skip_spaces(res);
		*istag = true;
	} else {
		*res++ = 0;
		*tag = res;
		while (*res != 0 && *res != ' ' && *res != ';' && *res != '<'
			   && *res != '&')
			res++;
		if (*res == 0)
			return NULL;
		if (*res == ' ' || *res == ';')
			*res++ = 0;
		else
			*(res - 1) = 0;
		*istag = false;
	}
	return res;
}

static char *html_skip_close_tag(char *string, char *tag)
{
	char *res = string, *sstart;
	int len = strlen(tag) + 1;
	char ttag[len + 1];

	snprintf_s(ttag, len + 1, "/%s", tag);
	do {
		while (*res != 0 && *res != '<')
			res++;
		if (*res == 0)
			return NULL;
		sstart = res;
		res++;
		res = html_skip_spaces(res);
		if (strncmp(ttag, res, len) == 0)
			res += len;
		else
			continue;
		res = html_skip_spaces(res);
		if (*res == 0)
			return NULL;
	} while (*res != '>');
	*sstart = *res++ = 0;
	return res;
}

extern dword html_to_text(char *string, dword size, bool stripeol)
{
	char *nstr, *ntag, *cstr = string, *str = string, *prop;
	bool istag;

	if (size > 0) {
		char *cur, *end;

		for (cur = string, end = cur + size; cur < end; cur++)
			if (*cur == 0)
				*cur = 1;
	}
	while (str != NULL
		   && (nstr =
			   html_find_next_tag(str, &ntag, &prop, &istag,
								  stripeol)) != NULL) {
		int len = (int) ntag - (int) str - 1;

		if (len > 0) {
			if (str != cstr) {
				strncpy(cstr, str, len);
				cstr[len] = '\0';
			}
			cstr += len;
		}
		if (istag) {
			if (stricmp(ntag, "head") == 0
				|| stricmp(ntag, "style") == 0 || stricmp(ntag, "title") == 0) {
				char *pstr = html_skip_close_tag(nstr, ntag);

				if (pstr != NULL)
					str = pstr;
				else
					str = nstr;
			} else if (stricmp(ntag, "script") == 0) {
				char *kstr = nstr;

				str = html_skip_close_tag(nstr, ntag);
				while ((kstr = strstr(kstr, "document.write(")) != NULL) {
					kstr += 15;
					while (*kstr == ' ' || *kstr == '\t')
						kstr++;
					if (*kstr == '\'' || *kstr == '"') {
						char qt = *kstr;
						char *dstr;

						kstr++;
						dstr = kstr;
						while (*dstr != 0
							   && (*dstr != qt || *(dstr - 1) == '\\'))
							dstr++;
						if (*dstr == qt) {
							strncpy(cstr, kstr, dstr - kstr);
							cstr[dstr - kstr] = '\0';
							cstr += html_to_text(cstr, 0, true);
						}
						kstr = dstr;
					}
				}
			} else if (stricmp(ntag, "pre") == 0) {
				char *pstr = html_skip_close_tag(nstr, "pre");

				if (pstr != NULL) {
					len = pstr - nstr - 6;
					strncpy(cstr, nstr, len);
					cstr[len] = '\0';
					cstr += html_to_text(cstr, 0, false);
				}
				str = pstr;
			} else {
				if (stricmp(ntag, "br") == 0
					|| stricmp(ntag, "li") == 0
					|| stricmp(ntag, "p") == 0
					|| stricmp(ntag, "/p") == 0
					|| stricmp(ntag, "tr") == 0
					|| stricmp(ntag, "/table") == 0
					|| stricmp(ntag, "div") == 0
					|| stricmp(ntag, "/div") == 0) {
					if (prop != NULL) {
						char *v = strchr(prop, '=');

						if (v == NULL)
							*cstr++ = '\n';
						else {
							char *t = v - 1;

							*v = 0;
							while (t > prop
								   && (*t == ' '
									   || *t == '\t'
									   || *t == '\r' || *t == '\n'))
								t--;
							*(t + 1) = 0;
							if (stricmp(prop, "style") == 0) {
								v++;
								v = html_skip_spaces(v);
								if (*v == '"' || *v == '\'') {
									char *rq = strchr(v + 1,
													  *v);

									if (rq == NULL) {
										*cstr++ = '\n';
										v = NULL;
									} else {
										*rq = 0;
										v++;
									}
								}
								if (v != NULL) {
									if (stricmp(v, "display:none") != 0)
										*cstr++ = '\n';
									else {
										char *pstr =
											html_skip_close_tag(nstr, ntag);
										if (pstr != NULL)
											nstr = pstr;
									}
								}
							}
						}
					} else
						*cstr++ = '\n';
				} else if (stricmp(ntag, "th") == 0 || stricmp(ntag, "td") == 0)
					*cstr++ = ' ';
				str = nstr;
			}
		} else {
			if (stricmp(ntag, "nbsp") == 0)
				*cstr++ = ' ';
			else if (stricmp(ntag, "gt") == 0)
				*cstr++ = '>';
			else if (stricmp(ntag, "lt") == 0)
				*cstr++ = '<';
			else if (stricmp(ntag, "amp") == 0)
				*cstr++ = '&';
			else if (stricmp(ntag, "quote") == 0)
				*cstr++ = '\'';
			else if (stricmp(ntag, "copy") == 0) {
				*cstr++ = '(';
				*cstr++ = 'C';
				*cstr++ = ')';
			}
			str = nstr;
		}
	}
	int slen = strlen(str);

	strncpy(cstr, str, slen);
	cstr[slen] = '\0';
	cstr += slen;
	return cstr - string;
}
