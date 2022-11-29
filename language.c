/* language.c
 * (c) 2002 Mikulas Patocka
 * This file is a part of the Links program, released under GPL.
 */
#include "links.h"

#include "language.h"
#include "language.inc"

unsigned char dummyarray[T__N_TEXTS];

unsigned char *
get_text_translation(unsigned char *text, struct terminal *term)
{
	if (!(text >= dummyarray && text < (dummyarray + T__N_TEXTS)))
		return text;
	return (unsigned char *)translation[text - dummyarray].name;
}
