/* error.c
 * (c) 2002 Mikulas Patocka
 * This file is a part of the Links program, released under GPL.
 */

#include "links.h"

volatile char dummy_val;
volatile char * volatile dummy_ptr = &dummy_val;
volatile char * volatile x_ptr;

void *do_not_optimize_here(void *p)
{
	/* break ANSI aliasing */
	x_ptr = p;
	*dummy_ptr = 0;
	return p;
}

static void er(char *m, va_list l)
{
	vfprintf(stderr, cast_const_char m, l);
	fprintf(stderr, ANSI_BELL);
	fprintf(stderr, "\n");
	fflush(stderr);
	portable_sleep(1000);
}

void error(char *m, ...)
{
	va_list l;
	va_start(l, m);
	er(m, l);
	va_end(l);
}

void fatal_exit(char *m, ...)
{
	va_list l;
	fatal_tty_exit();
	va_start(l, m);
	er(m, l);
	va_end(l);
	fflush(stdout);
	fflush(stderr);
	exit(RET_FATAL);
}

int errline;
unsigned char *errfile;

static unsigned char errbuf[4096];

void int_error(char *m, ...)
{
#ifdef NO_IE
	return;
#else
	va_list l;
	fatal_tty_exit();
	va_start(l, m);
	sprintf((char *)errbuf, "\n"ANSI_SET_BOLD"INTERNAL ERROR"ANSI_CLEAR_BOLD" at %s:%d: %s", errfile, errline, m);
	er((char *)errbuf, l);
	va_end(l);
	exit(RET_INTERNAL);
#endif
}

void *mem_calloc(size_t size)
{
	void *p;
	if (!size)
		return NULL;
	if (!(p = calloc(1, size)))
		die("calloc: %s\n", strerror(errno));
	return p;
}

unsigned char *memacpy(const unsigned char *src, size_t len)
{
	unsigned char *m;
	if (!(len + 1)) overalloc();
	m = xmalloc(len + 1);
	if (len)
		memcpy(m, src, len);
	m[len] = 0;
	return m;
}

unsigned char *stracpy(const unsigned char *src)
{
	return src ? memacpy(src, src != DUMMY ? strlen((const char *)src) : 0) : NULL;
}
