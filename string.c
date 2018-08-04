/* string.c
 * (c) 2002 Mikulas Patocka
 * This file is a part of the Links program, released under GPL.
 */

#include "links.h"

int snprint(unsigned char *s, int n, int num)
{
	int q = 1;
	while (q <= num / 10) q *= 10;
	while (n-- > 1 && q) {
		*(s++) = (unsigned char)(num / q + '0');
		num %= q;
		q /= 10;
	}
	*s = 0;
	return !!q;
}

int snzprint(unsigned char *s, int n, off_t num)
{
	off_t q = 1;
	if (n > 1 && num < 0) {
		*(s++) = '-';
		num = -num;
		n--;
	}
	while (q <= num / 10) q *= 10;
	while (n-- > 1 && q) {
		*(s++) = (unsigned char)(num / q + '0');
		num %= q;
		q /= 10;
	}
	*s = 0;
	return !!q;
}

void add_to_strn(unsigned char **s, unsigned char *a)
{
	unsigned char *p;
	size_t l1 = strlen(cast_const_char *s), l2 = strlen(cast_const_char a);
	if (((l1 | l2) | (l1 + l2 + 1)) > MAXINT)
		overalloc();
	p = xrealloc(*s, l1 + l2 + 1);
	strcat(cast_char p, cast_const_char a);
	*s = p;
}

void extend_str(unsigned char **s, int n)
{
	size_t l = strlen(cast_const_char *s);
	if (((l | n) | (l + n + 1)) > MAXINT)
		overalloc();
	*s = xrealloc(*s, l + n + 1);
}

void add_bytes_to_str(unsigned char **s, int *l, unsigned char *a, size_t ll)
{
	unsigned char *p;
	size_t old_length;
	size_t new_length;
	size_t x;

	if (!ll)
		return;

	p = *s;
	old_length = (unsigned)*l;
	if (ll + old_length >= (unsigned)MAXINT / 2 || ll + old_length < ll) overalloc();
	new_length = old_length + ll;
	*l = (int)new_length;
	x = old_length ^ new_length;
	if (x >= old_length) {
		/* Need to realloc */
		{
			new_length |= new_length >> 1;
			new_length |= new_length >> 2;
			new_length |= new_length >> 4;
			new_length |= new_length >> 8;
			new_length |= new_length >> 16;
			new_length++;
		}
		p = xrealloc(p, new_length);
		*s = p;
	}
	p[*l] = 0;
	memcpy(p + old_length, a, ll);
}

void add_to_str(unsigned char **s, int *l, unsigned char *a)
{
	add_bytes_to_str(s, l, a, strlen(cast_const_char a));
}

void add_chr_to_str(unsigned char **s, int *l, unsigned char a)
{
	add_bytes_to_str(s, l, &a, 1);
}

void add_unsigned_long_num_to_str(unsigned char **s, int *l, long n)
{
	unsigned char a[64];
	snprint(a, 64, n);
	add_to_str(s, l, a);
}

void add_num_to_str(unsigned char **s, int *l, off_t n)
{
	unsigned char a[64];
	snzprint(a, 64, n);
	add_to_str(s, l, a);
}

void add_knum_to_str(unsigned char **s, int *l, off_t n)
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

long strtolx(unsigned char *c, unsigned char **end)
{
	long l;
	if (c[0] == '0' && upcase(c[1]) == 'X' && c[2]) l = strtol(cast_const_char(c + 2), (char **)(void *)end, 16);
	else l = strtol(cast_const_char c, (char **)(void *)end, 10);
	if (!*end) return l;
	if (upcase(**end) == 'K') {
		(*end)++;
		if (l < -MAXINT / 1024) return -MAXINT;
		if (l > MAXINT / 1024) return MAXINT;
		return l * 1024;
	}
	if (upcase(**end) == 'M') {
		(*end)++;
		if (l < -MAXINT / (1024 * 1024)) return -MAXINT;
		if (l > MAXINT / (1024 * 1024)) return MAXINT;
		return l * (1024 * 1024);
	}
	return l;
}

my_strtoll_t my_strtoll(unsigned char *string, unsigned char **end)
{
	my_strtoll_t f;
	errno = 0;
	f = strtoll(cast_const_char string, (char **)(void *)end, 10);
	if (f < 0 || errno) return -1;
	return f;
}

/* Copies at most dst_size chars into dst. Ensures null termination of dst. */
void safe_strncpy(unsigned char *dst, const unsigned char *src, size_t dst_size)
{
	dst[dst_size - 1] = 0;
	strncpy(cast_char dst, cast_const_char src, dst_size - 1);
}

/* don't use strcasecmp because it depends on locale */
int casestrcmp(const unsigned char *s1, const unsigned char *s2)
{
	while (1) {
		unsigned char c1 = *s1;
		unsigned char c2 = *s2;
		c1 = locase(c1);
		c2 = locase(c2);
		if (c1 != c2) {
			return (int)c1 - (int)c2;
		}
		if (!*s1) break;
		s1++;
		s2++;
	}
	return 0;
}

/* case insensitive compare of 2 strings */
/* comparison ends after len (or less) characters */
/* return value: 1=strings differ, 0=strings are same */
int casecmp(const unsigned char *c1, const unsigned char *c2, size_t len)
{
	size_t i;
	for (i = 0; i < len; i++) if (srch_cmp(c1[i], c2[i])) return 1;
	return 0;
}

int casestrstr(const unsigned char *h, const unsigned char *n)
{
	const unsigned char *p;

	for (p=h;*p;p++)
	{
		if (!srch_cmp(*p,*n))  /* same */
		{
			const unsigned char *q, *r;
			for (q=n, r=p;*r&&*q;)
			{
				if (!srch_cmp(*q,*r)) {
					r++;
					q++;    /* same */
				} else
					break;
			}
			if (!*q) return 1;
		}
	}

	return 0;
}


