/* language.c
 * (c) 2002 Mikulas Patocka
 * This file is a part of the Links program, released under GPL.
 */

#include "links.h"
#include "language.h"

struct translation {
	int code;
	const char *name;
};

#include "language.inc"

unsigned char dummyarray[T__N_TEXTS];

int get_commandline_charset(void)
{
	return dump_codepage == -1 ? 0 : dump_codepage;
}

static inline int is_direct_text(unsigned char *text)
{
	return !(text >= dummyarray && text < (dummyarray + T__N_TEXTS));
}

unsigned char *get_text_translation(unsigned char *text, struct terminal *term)
{
	if (is_direct_text(text))
		return text;
	return (unsigned char *)translation[text - dummyarray].name;
}

unsigned char *get_english_translation(unsigned char *text)
{
	if (is_direct_text(text))
		return text;
	return (unsigned char *)translation[text - dummyarray].name;
}
