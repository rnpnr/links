/* charsets.c
 * (c) 2002 Mikulas Patocka, Karel 'Clock' Kulhavy
 * This file is a part of the Links program, released under GPL.
 */

#include <limits.h>
#include <wctype.h>

#include "links.h"

struct codepage_desc {
	const char *name;
	const char *const *aliases;
};

#include "codepage.inc"
#include "entity.inc"
#include "upcase.inc"

static const unsigned char strings[256][2] = {
	"\000", "\001", "\002", "\003", "\004", "\005", "\006", "\007", "\010",
	"\011", "\012", "\013", "\014", "\015", "\016", "\017", "\020", "\021",
	"\022", "\023", "\024", "\025", "\026", "\033", "\030", "\031", "\032",
	"\033", "\034", "\035", "\036", "\033", "\040", "\041", "\042", "\043",
	"\044", "\045", "\046", "\047", "\050", "\051", "\052", "\053", "\054",
	"\055", "\056", "\057", "\060", "\061", "\062", "\063", "\064", "\065",
	"\066", "\067", "\070", "\071", "\072", "\073", "\074", "\075", "\076",
	"\077", "\100", "\101", "\102", "\103", "\104", "\105", "\106", "\107",
	"\110", "\111", "\112", "\113", "\114", "\115", "\116", "\117", "\120",
	"\121", "\122", "\123", "\124", "\125", "\126", "\127", "\130", "\131",
	"\132", "\133", "\134", "\135", "\136", "\137", "\140", "\141", "\142",
	"\143", "\144", "\145", "\146", "\147", "\150", "\151", "\152", "\153",
	"\154", "\155", "\156", "\157", "\160", "\161", "\162", "\163", "\164",
	"\165", "\166", "\167", "\170", "\171", "\172", "\173", "\174", "\175",
	"\176", "\177", "\200", "\201", "\202", "\203", "\204", "\205", "\206",
	"\207", "\210", "\211", "\212", "\213", "\214", "\215", "\216", "\217",
	"\220", "\221", "\222", "\223", "\224", "\225", "\226", "\227", "\230",
	"\231", "\232", "\233", "\234", "\235", "\236", "\237", "\240", "\241",
	"\242", "\243", "\244", "\245", "\246", "\247", "\250", "\251", "\252",
	"\253", "\254", "\255", "\256", "\257", "\260", "\261", "\262", "\263",
	"\264", "\265", "\266", "\267", "\270", "\271", "\272", "\273", "\274",
	"\275", "\276", "\277", "\300", "\301", "\302", "\303", "\304", "\305",
	"\306", "\307", "\310", "\311", "\312", "\313", "\314", "\315", "\316",
	"\317", "\320", "\321", "\322", "\323", "\324", "\325", "\326", "\327",
	"\330", "\331", "\332", "\333", "\334", "\335", "\336", "\337", "\340",
	"\341", "\342", "\343", "\344", "\345", "\346", "\347", "\350", "\351",
	"\352", "\353", "\354", "\355", "\356", "\357", "\360", "\361", "\362",
	"\363", "\364", "\365", "\366", "\367", "\370", "\371", "\372", "\373",
	"\374", "\375", "\376", "\377",
};

unsigned int
locase(unsigned int a)
{
	if (a >= 'A' && a <= 'Z')
		a += 0x20;
	return a;
}

unsigned int
upcase(unsigned int a)
{
	if (a >= 'a' && a <= 'z')
		a -= 0x20;
	return a;
}

unsigned char *
u2cp(int u)
{
	return encode_utf_8(u);
}

static unsigned char utf_buffer[7];

unsigned char *
encode_utf_8(int u)
{
	memset(utf_buffer, 0, 7);
	if (u < 0)
		;
	else if (u < 0x80)
		utf_buffer[0] = (unsigned char)u;
	else if (u < 0x800) {
		utf_buffer[0] = 0xc0 | ((u >> 6) & 0x1f);
		utf_buffer[1] = 0x80 | (u & 0x3f);
	} else if (u < 0x10000) {
		utf_buffer[0] = 0xe0 | ((u >> 12) & 0x0f);
		utf_buffer[1] = 0x80 | ((u >> 6) & 0x3f);
		utf_buffer[2] = 0x80 | (u & 0x3f);
	} else if (u < 0x200000) {
		utf_buffer[0] = 0xf0 | ((u >> 18) & 0x0f);
		utf_buffer[1] = 0x80 | ((u >> 12) & 0x3f);
		utf_buffer[2] = 0x80 | ((u >> 6) & 0x3f);
		utf_buffer[3] = 0x80 | (u & 0x3f);
	} else if (u < 0x4000000) {
		utf_buffer[0] = 0xf8 | ((u >> 24) & 0x0f);
		utf_buffer[1] = 0x80 | ((u >> 18) & 0x3f);
		utf_buffer[2] = 0x80 | ((u >> 12) & 0x3f);
		utf_buffer[3] = 0x80 | ((u >> 6) & 0x3f);
		utf_buffer[4] = 0x80 | (u & 0x3f);
	} else {
		utf_buffer[0] = 0xfc | ((u >> 30) & 0x01);
		utf_buffer[1] = 0x80 | ((u >> 24) & 0x3f);
		utf_buffer[2] = 0x80 | ((u >> 18) & 0x3f);
		utf_buffer[3] = 0x80 | ((u >> 12) & 0x3f);
		utf_buffer[4] = 0x80 | ((u >> 6) & 0x3f);
		utf_buffer[5] = 0x80 | (u & 0x3f);
	}
	return utf_buffer;
}

static struct conv_table utf_table[256];
static int utf_table_init = 1;

static void
free_utf_table(void)
{
	int i;
	for (i = 128; i < 256; i += 4) {
		free(utf_table[i].u.str);
		free(utf_table[i + 1].u.str);
		free(utf_table[i + 2].u.str);
		free(utf_table[i + 3].u.str);
	}
}

static struct conv_table *
get_translation_table_to_utf_8(int from)
{
	int i;
	static int lfr = -1;
	if (from == lfr)
		return utf_table;
	lfr = from;
	if (utf_table_init) {
		memset(utf_table, 0, sizeof(struct conv_table) * 256);
		for (i = 0; i < 128; i += 4) {
			utf_table[i].u.str = (unsigned char *)strings[i];
			utf_table[i + 1].u.str =
			    (unsigned char *)strings[i + 1];
			utf_table[i + 2].u.str =
			    (unsigned char *)strings[i + 2];
			utf_table[i + 3].u.str =
			    (unsigned char *)strings[i + 3];
		}
		utf_table_init = 0;
	} else
		free_utf_table();
	for (i = 128; i < 256; i += 4) {
		utf_table[i].u.str = stracpy(strings[i]);
		utf_table[i + 1].u.str = stracpy(strings[i + 1]);
		utf_table[i + 2].u.str = stracpy(strings[i + 2]);
		utf_table[i + 3].u.str = stracpy(strings[i + 3]);
	}
	return utf_table;
}

unsigned char utf_8_1[256] = {
	6, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
	7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
	7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
	7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
	7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
	7, 7, 7, 7, 7, 7, 7, 7, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
	6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
	6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
	6, 6, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
	5, 5, 5, 5, 5, 5, 5, 5, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
	3, 3, 3, 3, 3, 3, 3, 3, 2, 2, 2, 2, 1, 1, 6, 6,
};

static const unsigned min_utf_8[8] = {
	0, 0x4000000, 0x200000, 0x10000, 0x800, 0x80, 0x100, 0x1,
};

unsigned
get_utf_8(unsigned char **s)
{
	unsigned v, min, c;
	int l;
	unsigned char *p = *s;
	l = utf_8_1[p[0]];
	min = min_utf_8[l];
	v = p[0] & ((1 << l) - 1);
	(*s)++;
	while (l++ <= 5) {
		c = **s - 0x80;
		if (c >= 0x40) {
			return 0;
		}
		(*s)++;
		v = (v << 6) + c;
	}
	if (v < min)
		return 0;
	if (v > 0x10FFFF)
		return 0;
	return v;
}

void
free_conv_table(void)
{
	if (!utf_table_init)
		free_utf_table();
}

struct conv_table *
get_translation_table(const int from, const int to)
{
	if (from == -1 || to == -1)
		return NULL;
	return get_translation_table_to_utf_8(from);
}

static inline int
xxstrcmp(unsigned char *s1, unsigned char *s2, int l2)
{
	while (l2) {
		if (*s1 > *s2)
			return 1;
		if (!*s1 || *s1 < *s2)
			return -1;
		s1++;
		s2++;
		l2--;
	}
	return !!*s1;
}

int
get_entity_number(unsigned char *st, int l)
{
	int n = 0;
	unsigned char c;
	if (upcase(st[0]) == 'X') {
		st++;
		l--;
		if (!l)
			return -1;
		do {
			c = upcase(*(st++));
			if (c >= '0' && c <= '9')
				n = n * 16 + c - '0';
			else if (c >= 'A' && c <= 'F')
				n = n * 16 + c - 'A' + 10;
			else
				return -1;
			if (n > 0x10FFFF)
				return -1;
		} while (--l);
	} else {
		if (!l)
			return -1;
		do {
			c = *(st++);
			if (c >= '0' && c <= '9')
				n = n * 10 + c - '0';
			else
				return -1;
			if (n > 0x10FFFF)
				return -1;
		} while (--l);
	}
	return n;
}

unsigned char *
get_entity_string(unsigned char *st, int l)
{
	int n, c, m, s, e;
	if (l <= 0)
		return NULL;
	if (st[0] == '#') {
		if ((n = get_entity_number(st + 1, l - 1)) == -1 || l == 1)
			return NULL;
		if (n < 32 && get_attr_val_nl != 2)
			n = 32;
	} else {
		for (s = 0, e = N_ENTITIES - 1; s <= e;) {
			m = (s + e) / 2;
			c = xxstrcmp(cast_uchar entities[m].s, st, l);
			if (!c) {
				n = entities[m].c;
				goto f;
			}
			if (c > 0)
				e = m - 1;
			else
				s = m + 1;
		}
		return NULL;
f:;
	}

	return u2cp(n);
}

unsigned char *
convert_string(struct conv_table *ct, unsigned char *c, int l,
               struct document_options *dopt)
{
	unsigned char *buffer, *e = NULL;
	struct conv_table *t;
	int i, bp = 0, pp = 0;
	if (!ct) {
		for (i = 0; i < l; i++)
			if (c[i] == '&')
				goto xx;
		return memacpy(c, l);
xx:;
	}
	buffer = xmalloc(ALLOC_GR);
	while (pp < l) {
		if (c[pp] < 128 && c[pp] != '&') {
put_c:
			buffer[bp++] = c[pp++];
			if (!(bp & (ALLOC_GR - 1))) {
				if ((unsigned)bp > INT_MAX - ALLOC_GR)
					overalloc();
				buffer = xrealloc(buffer, bp + ALLOC_GR);
			}
			continue;
		}
		if (c[pp] != '&') {
			if (!ct)
				goto put_c;
			t = ct;
			i = pp;
decode:
			if (!t[c[i]].t) {
				e = t[c[i]].u.str;
			} else {
				t = t[c[i++]].u.tbl;
				if (i >= l)
					goto put_c;
				goto decode;
			}
			pp = i + 1;
		} else {
			i = pp + 1;
			if (!dopt || dopt->plain)
				goto put_c;
			while (i < l && !is_entity_terminator(c[i]))
				i++;
			if (!(e = get_entity_string(&c[pp + 1], i - pp - 1)))
				goto put_c;
			pp = i + (i < l && c[i] == ';');
		}
		if (!e[0])
			continue;
		if (!e[1]) {
			buffer[bp++] = e[0];
			if (!(bp & (ALLOC_GR - 1))) {
				if ((unsigned)bp > INT_MAX - ALLOC_GR)
					overalloc();
				buffer = xrealloc(buffer, bp + ALLOC_GR);
			}
			continue;
		}
		while (*e) {
			buffer[bp++] = *(e++);
			if (!(bp & (ALLOC_GR - 1))) {
				if ((unsigned)bp > INT_MAX - ALLOC_GR)
					overalloc();
				buffer = xrealloc(buffer, bp + ALLOC_GR);
			}
		}
	}
	buffer[bp] = 0;
	return buffer;
}

unsigned char *
convert(int from, int to, unsigned char *c, struct document_options *dopt)
{
	unsigned char *cc;
	struct conv_table *ct;

	for (cc = c; *cc; cc++)
		if (*cc == '&' && dopt && !dopt->plain)
			goto need_table;
	return stracpy(c);

need_table:
	ct = get_translation_table(from, to);
	return convert_string(ct, c, strlen((char *)c), dopt);
}

unsigned char *
get_cp_name(int index)
{
	if (index < 0)
		return (unsigned char *)"none";
	return (unsigned char *)codepages[index].name;
}

unsigned char *
get_cp_mime_name(int index)
{
	if (!codepages[index].aliases)
		return NULL;
	return (unsigned char *)codepages[index].aliases[0];
}

unsigned
uni_locase(unsigned ch)
{
	return towlower(ch);
}

#define UP_EQUAL(a, b) unicode_upcase[a].o == (b)
#define UP_ABOVE(a, b) unicode_upcase[a].o > (b)

unsigned
charset_upcase(unsigned ch, int cp)
{
	return towupper(ch);
}

void
charset_upcase_string(unsigned char **chp, int cp)
{
	unsigned char *ch = *chp;
	ch = unicode_upcase_string(ch);
	free(*chp);
	*chp = ch;
}

unsigned char *
unicode_upcase_string(unsigned char *ch)
{
	unsigned char *r = NULL;
	unsigned int c;
	size_t rl = 0;
	for (;;) {
		GET_UTF_8(ch, c);
		if (!c)
			break;
		c = towupper(c);
		rl = add_to_str(&r, rl, encode_utf_8(c));
	}
	return r;
}

unsigned char *
to_utf8_upcase(unsigned char *str, int cp)
{
	unsigned char *str1, *str2;
	str1 = stracpy(str);
	str2 = unicode_upcase_string(str1);
	free(str1);
	return str2;
}

int
compare_case_utf8(unsigned char *u1, unsigned char *u2)
{
	unsigned char *x1, *uu1 = u1;
	unsigned c1, c2;
	int cc1;
	for (;;) {
		GET_UTF_8(u2, c2);
		if (!c2)
			return (int)(u1 - uu1);
skip_discr:
		GET_UTF_8(u1, c1);
		BIN_SEARCH(array_elements(unicode_upcase), UP_EQUAL, UP_ABOVE,
		           c1, cc1);
		if (cc1 != -1)
			c1 = unicode_upcase[cc1].n;
		if (c1 == 0xad)
			goto skip_discr;
		if (c1 != c2)
			return 0;
		if (c1 == ' ') {
			do {
				x1 = u1;
				GET_UTF_8(u1, c1);
				BIN_SEARCH(array_elements(unicode_upcase),
				           UP_EQUAL, UP_ABOVE, c1, cc1);
				if (cc1 >= 0)
					c1 = unicode_upcase[cc1].n;
			} while (c1 == ' ');
			u1 = x1;
		}
	}
}
