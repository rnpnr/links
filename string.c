/* string.c
 * (c) 2002 Mikulas Patocka
 * This file is a part of the Links program, released under GPL.
 */

#include <limits.h>

#include "links.h"

/* case insensitive compare of 2 strings */
/* comparison ends after len (or less) characters */
/* return value: 1=strings differ, 0=strings are same */
static inline int
srch_cmp(unsigned char c1, unsigned char c2)
{
	return upcase(c1) != upcase(c2);
}

int
cmpbeg(const unsigned char *str, const unsigned char *b)
{
	while (*str && upcase(*str) == upcase(*b)) {
		str++;
		b++;
	}
	return !!*b;
}

int
xstrcmp(const unsigned char *s1, const unsigned char *s2)
{
	if (!s1 && !s2)
		return 0;
	if (!s1)
		return -1;
	if (!s2)
		return 1;
	return strcmp(cast_const_char s1, cast_const_char s2);
}

int
snprint(unsigned char *s, int n, int num)
{
	int q = 1;
	while (q <= num / 10)
		q *= 10;
	while (n-- > 1 && q) {
		*(s++) = (unsigned char)(num / q + '0');
		num %= q;
		q /= 10;
	}
	*s = 0;
	return !!q;
}

int
snzprint(unsigned char *s, int n, off_t num)
{
	off_t q = 1;
	if (n > 1 && num < 0) {
		*(s++) = '-';
		num = -num;
		n--;
	}
	while (q <= num / 10)
		q *= 10;
	while (n-- > 1 && q) {
		*(s++) = (unsigned char)(num / q + '0');
		num %= q;
		q /= 10;
	}
	*s = 0;
	return !!q;
}

void
add_to_strn(unsigned char **s, unsigned char *a)
{
	unsigned char *p;
	size_t l1 = strlen(cast_const_char * s), l2 = strlen(cast_const_char a);
	if (((l1 | l2) | (l1 + l2 + 1)) > INT_MAX)
		overalloc();
	p = xrealloc(*s, l1 + l2 + 1);
	strcat(cast_char p, cast_const_char a);
	*s = p;
}

void
extend_str(unsigned char **s, int n)
{
	size_t l = strlen(cast_const_char * s);
	if (((l | n) | (l + n + 1)) > INT_MAX)
		overalloc();
	*s = xrealloc(*s, l + n + 1);
}

void
add_bytes_to_str(unsigned char **s, int *l, unsigned char *a, size_t ll)
{
	unsigned char *p = *s;
	size_t ol = *l;

	if (!ll)
		return;

	*l = *l + ll;

	/* FIXME: Hack, behaves like init_str() fn */
	if (ol == 0)
		p = xreallocarray(p, *l + 1, sizeof(unsigned char));
	else
		p = xreallocarray(p, *l, sizeof(unsigned char));
	p[*l] = 0;
	memcpy(p + ol, a, ll);

	*s = p;
}

void
add_to_str(unsigned char **s, int *l, unsigned char *a)
{
	add_bytes_to_str(s, l, a, strlen(cast_const_char a));
}

void
add_chr_to_str(unsigned char **s, int *l, unsigned char a)
{
	add_bytes_to_str(s, l, &a, 1);
}

void
add_unsigned_long_num_to_str(unsigned char **s, int *l, unsigned long n)
{
	unsigned char a[64];
	snprint(a, 64, n);
	add_to_str(s, l, a);
}

void
add_num_to_str(unsigned char **s, int *l, off_t n)
{
	unsigned char a[64];
	if (n >= 0 && n < 1000) {
		unsigned sn = (unsigned)n;
		unsigned char *p = a;
		if (sn >= 100) {
			*p++ = '0' + sn / 100;
			sn %= 100;
			goto d10;
		}
		if (sn >= 10) {
d10:
			*p++ = '0' + sn / 10;
			sn %= 10;
		}
		*p++ = '0' + sn;
		add_bytes_to_str(s, l, a, p - a);
	} else {
		snzprint(a, 64, n);
		add_to_str(s, l, a);
	}
}

void
add_knum_to_str(unsigned char **s, int *l, off_t n)
{
	unsigned char a[13];
	if (n && n / (1024 * 1024) * (1024 * 1024) == n) {
		snzprint(a, 12, n / (1024 * 1024));
		a[strlen(cast_const_char a) + 1] = 0;
		a[strlen(cast_const_char a)] = 'M';
	} else if (n && n / 1024 * 1024 == n) {
		snzprint(a, 12, n / 1024);
		a[strlen(cast_const_char a) + 1] = 0;
		a[strlen(cast_const_char a)] = 'k';
	} else
		snzprint(a, 13, n);
	add_to_str(s, l, a);
}

long
strtolx(unsigned char *c, unsigned char **end)
{
	char *end_c;
	long l;
	if (c[0] == '0' && upcase(c[1]) == 'X' && c[2])
		l = strtol(cast_const_char(c + 2), &end_c, 16);
	else
		l = strtol(cast_const_char c, &end_c, 10);
	*end = cast_uchar end_c;
	if (upcase(**end) == 'K') {
		(*end)++;
		if (l < -INT_MAX / 1024)
			return -INT_MAX;
		if (l > INT_MAX / 1024)
			return INT_MAX;
		return l * 1024;
	}
	if (upcase(**end) == 'M') {
		(*end)++;
		if (l < -INT_MAX / (1024 * 1024))
			return -INT_MAX;
		if (l > INT_MAX / (1024 * 1024))
			return INT_MAX;
		return l * (1024 * 1024);
	}
	return l;
}

/* Copies at most dst_size chars into dst. Ensures null termination of dst. */
void
safe_strncpy(unsigned char *dst, const unsigned char *src, size_t dst_size)
{
	dst[dst_size - 1] = 0;
	strncpy(cast_char dst, cast_const_char src, dst_size - 1);
}

/* don't use strcasecmp because it depends on locale */
int
casestrcmp(const unsigned char *s1, const unsigned char *s2)
{
	while (1) {
		unsigned char c1 = *s1;
		unsigned char c2 = *s2;
		c1 = locase(c1);
		c2 = locase(c2);
		if (c1 != c2) {
			return (int)c1 - (int)c2;
		}
		if (!*s1)
			break;
		s1++;
		s2++;
	}
	return 0;
}

/* case insensitive compare of 2 strings */
/* comparison ends after len (or less) characters */
/* return value: 1=strings differ, 0=strings are same */
int
casecmp(const unsigned char *c1, const unsigned char *c2, size_t len)
{
	size_t i;
	for (i = 0; i < len; i++)
		if (srch_cmp(c1[i], c2[i]))
			return 1;
	return 0;
}

int
casestrstr(const unsigned char *h, const unsigned char *n)
{
	const unsigned char *p;

	for (p = h; *p; p++) {
		if (!srch_cmp(*p, *n)) /* same */
		{
			const unsigned char *q, *r;
			for (q = n, r = p; *r && *q;) {
				if (!srch_cmp(*q, *r)) {
					r++;
					q++; /* same */
				} else
					break;
			}
			if (!*q)
				return 1;
		}
	}

	return 0;
}
