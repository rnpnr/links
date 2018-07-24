/* error.c
 * (c) 2002 Mikulas Patocka
 * This file is a part of the Links program, released under GPL.
 */

#include "links.h"

#ifdef RED_ZONE
#define RED_ZONE_INC	1
#else
#define RED_ZONE_INC	0
#endif

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

#define heap_malloc	malloc
#define heap_realloc	realloc
#define heap_calloc(x) calloc(1, (x))
void init_heap(void)
{
}
#define exit_heap()	do { } while (0)

static inline void force_dump(void)
{
	int rs;
	fprintf(stderr, "\n"ANSI_SET_BOLD"Forcing core dump"ANSI_CLEAR_BOLD"\n");
	fflush(stdout);
	fflush(stderr);
	EINTRLOOP(rs, raise(SIGSEGV));
}

void check_memory_leaks(void)
{
	exit_heap();
}

static void er(int b, char *m, va_list l)
{
	vfprintf(stderr, cast_const_char m, l);
	if (b) fprintf(stderr, ANSI_BELL);
	fprintf(stderr, "\n");
	fflush(stderr);
	portable_sleep(1000);
}

void error(char *m, ...)
{
	va_list l;
	va_start(l, m);
	fprintf(stderr, "\n");
	er(1, m, l);
	va_end(l);
}

void fatal_exit(char *m, ...)
{
	va_list l;
	fatal_tty_exit();
	va_start(l, m);
	fprintf(stderr, "\n");
	er(1, m, l);
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
	sprintf(cast_char errbuf, "\n"ANSI_SET_BOLD"INTERNAL ERROR"ANSI_CLEAR_BOLD" at %s:%d: %s", errfile, errline, m);
	er(1, cast_char errbuf, l);
	va_end(l);
	force_dump();
	exit(RET_INTERNAL);
#endif
}

void debug_msg(char *m, ...)
{
	va_list l;
	va_start(l, m);
	sprintf(cast_char errbuf, "\nDEBUG MESSAGE at %s:%d: %s", errfile, errline, m);
	er(0, cast_char errbuf, l);
	va_end(l);
}

void *mem_calloc_(size_t size, int mayfail)
{
	void *p;
	debug_test_free(NULL, 0);
	if (!size)
		return NULL;
	retry:
	if (!(p = heap_calloc(size))) {
		if (out_of_memory_fl(0, !mayfail ? cast_uchar "calloc" : NULL, size, NULL, 0)) goto retry;
		return NULL;
	}
	return p;
}

void *mem_realloc_(void *p, size_t size, int mayfail)
{
	void *np;
	if (!p)
		return xmalloc(size);
	debug_test_free(NULL, 0);
	if (!size) {
		free(p);
		return NULL;
	}
	retry:
	if (!(np = heap_realloc(p, size))) {
		if (out_of_memory_fl(0, !mayfail ? cast_uchar "realloc" : NULL, size, NULL, 0)) goto retry;
		return NULL;
	}
	return np;
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
	return src ? memacpy(src, src != DUMMY ? strlen(cast_const_char src) : 0) : NULL;
}
