/* error.c
 * (c) 2002 Mikulas Patocka
 * This file is a part of the Links program, released under GPL.
 */

#include "links.h"

void *
mem_calloc(size_t size)
{
	void *p;
	if (!size)
		return NULL;
	if (!(p = calloc(1, size)))
		die("calloc: %s\n", strerror(errno));
	return p;
}

unsigned char *
memacpy(const unsigned char *src, size_t len)
{
	unsigned char *m;
	if (!(len + 1))
		overalloc();
	m = xmalloc(len + 1);
	if (len)
		memcpy(m, src, len);
	m[len] = 0;
	return m;
}

unsigned char *
stracpy(const unsigned char *src)
{
	return src ? memacpy(src, src != DUMMY ? strlen((char *)src) : 0)
	           : NULL;
}
