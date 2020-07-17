/* html.c
 * (c) 2002 Mikulas Patocka
 * This file is a part of the Links program, released under GPL.
 */

#include <limits.h>
#include <string.h>

#include "links.h"

struct list_head html_stack = {&html_stack, &html_stack};

int html_format_changed = 0;

static inline int isA(unsigned char c)
{
	return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

static inline int atchr(unsigned char c)
{
	return /*isA(c) ||*/ (c > ' ' && c != '=' && c != '<' && c != '>');
}

/* accepts one html element */
/* e is pointer to the begining of the element (*e must be '<') */
/* eof is pointer to the end of scanned area */
/* parsed element name is stored in name, it's length is namelen */
/* first attribute is stored in attr */
/* end points to first character behind the html element */
/* returns: -1 fail (returned values in pointers are invalid) */
/*	    0 success */
int parse_element(unsigned char *e, unsigned char *eof, unsigned char **name, int *namelen, unsigned char **attr, unsigned char **end)
{
	if (eof - e < 3 || *(e++) != '<') return -1;
	if (name) *name = e;
	if (*e == '/') {
		e++;
		if (*e == '>' || *e == '<') goto xx;
	} else if (!isA(*e)) {
		return -1;
	}
	while (isA(*e) || (*e >= '0' && *e <= '9') || *e == '_' || *e == '-' || *e == '=') {
		e++;
		if (e >= eof) return -1;
	}
	xx:
	if (name && namelen) *namelen = (int)(e - *name);
	while ((WHITECHAR(*e) || *e == '/' || *e == ':')) {
		e++;
		if (e >= eof) return -1;
	}
	if ((!atchr(*e) && *e != '>' && *e != '<')) return -1;
	if (attr) *attr = e;
	nextattr:
	while (WHITECHAR(*e)) {
		e++;
		if (e >= eof) return -1;
	}
	if ((!atchr(*e) && *e != '>' && *e != '<')) return -1;
	if (*e == '>' || *e == '<') goto en;
	while (atchr(*e)) {
		e++;
		if (e >= eof) return -1;
	}
	while (WHITECHAR(*e)) {
		e++;
		if (e >= eof) return -1;
	}
	if (*e != '=') goto endattr;
	if (1) goto x2;
	while (WHITECHAR(*e)) {
		x2:
		e++;
		if (e >= eof) return -1;
	}
	if (U(*e)) {
		unsigned char uu = *e;
		/*u:*/
		if (1) goto x3;
		while (e < eof && *e != uu && *e /*(WHITECHAR(*e) || *e > ' ')*/) {
			x3:
			e++;
			if (e >= eof) return -1;
		}
		if (*e < ' ') return -1;
		e++;
		if (e >= eof /*|| (!WHITECHAR(*e) && *e != uu && *e != '>' && *e != '<')*/) return -1;
		/*if (*e == uu) goto u;*/
	} else {
		while (!WHITECHAR(*e) && *e != '>' && *e != '<') {
			e++;
			if (e >= eof) return -1;
		}
	}
	while (WHITECHAR(*e)) {
		e++;
		if (e >= eof) return -1;
	}
	endattr:
	if (*e != '>' && *e != '<') goto nextattr;
	en:
	if (e[-1] == '\\') return -1;
	if (end) *end = e + (*e == '>');
	return 0;
}

#define add_chr(s, l, c)						\
do {									\
	if (!((l) & (32 - 1))) {					\
		if ((unsigned)(l) > INT_MAX - 32) overalloc();		\
		(s) = xrealloc((s), (l) + 32);				\
	}								\
	(s)[(l)++] = (c);						\
} while (0)

int get_attr_val_nl = 0;

/* parses html element attributes */
/* e is attr pointer previously got from parse_element, DON'T PASS HERE ANY OTHER VALUE!!! */
/* name is searched attribute */
/* returns allocated string containing the attribute, or NULL on unsuccess */
unsigned char *get_attr_val(unsigned char *e, unsigned char *name)
{
	unsigned char *n;
	unsigned char *a = NULL;
	int l = 0;
	int f;
 aa:
	while (WHITECHAR(*e)) e++;
	if (*e == '>' || *e == '<') return NULL;
	n = name;
	while (*n && upcase(*e) == upcase(*n)) {
		e++;
		n++;
	}
	f = *n;
	while (atchr(*e)) {
		f = 1;
		e++;
	}
	while (WHITECHAR(*e)) e++;
	if (*e != '=') goto ea;
	e++;
	while (WHITECHAR(*e)) e++;
	if (!U(*e)) {
		while (!WHITECHAR(*e) && *e != '>' && *e != '<') {
			if (!f) add_chr(a, l, *e);
			e++;
		}
	} else {
		unsigned char uu = *e;
		e++;
		while (*e != uu) {
			if (!*e) {
				free(a);
				return NULL;
			}
			if (!f) {
				if (get_attr_val_nl == 2)
					goto exact;
				if (*e != 13) {
					if (*e != 9 && *e != 10)
 exact:
						add_chr(a, l, *e);
					else if (!get_attr_val_nl)
						add_chr(a, l, ' ');
				}
			}
			e++;
		}
		e++;
	}
 ea:
	if (!f) {
		unsigned char *b;
		add_chr(a, l, 0);
		if (strchr((char *)a, '&')) {
			unsigned char *aa = a;
			int c = d_opt->cp;
			d_opt->cp = d_opt->real_cp;
			a = convert_string(NULL, aa, strlen((char *)aa), d_opt);
			d_opt->cp = c;
			free(aa);
		}
		while ((b = (unsigned char *)strchr((char *)a, 1)))
			*b = ' ';
		if (get_attr_val_nl != 2) {
			for (b = a; *b == ' '; b++)
				;
			if (b != a)
				memmove(a, b, strlen((char *)b) + 1);
			for (b = a + strlen((char *)a) - 1; b >= a && *b == ' '; b--)
				*b = 0;
		}
		return a;
	}
	goto aa;
}

int has_attr(unsigned char *e, unsigned char *name)
{
	unsigned char *a;
	if (!(a = get_attr_val(e, name)))
		return 0;
	free(a);
	return 1;
}

static unsigned char *get_url_val(unsigned char *e, unsigned char *name)
{
	unsigned char *a, *p, *c;
	int l;
	get_attr_val_nl = 1;
	a = get_attr_val(e, name);
	get_attr_val_nl = 0;
	if (!a)
		return NULL;
	if (d_opt->real_cp) {
		if (url_non_ascii(a))
			goto need_convert;
	}
	return a;

	need_convert:
	c = init_str();
	l = 0;
	for (p = a; *p; p++)
		add_to_str(&c, &l, encode_utf_8(*p));
	free(a);
	return c;
}

static unsigned char *get_exact_attr_val(unsigned char *e, unsigned char *name)
{
	unsigned char *a;
	get_attr_val_nl = 2;
	a = get_attr_val(e, name);
	get_attr_val_nl = 0;
	if (a) {
		unsigned char *x1, *x2;
		for (x1 = x2 = a; *x1; x1++, x2++)
			if (x1[0] == '\r') {
				*x2 = '\n';
				if (x1[1] == '\n') x1++;
			} else
				*x2 = *x1;
		*x2 = 0;
	}
	return a;
}

static struct {
	const unsigned short int n;
	const char *s;
} roman_tbl[] = {
	{1000,	"m"},
	{999,	"im"},
	{990,	"xm"},
	{900,	"cm"},
	{500,	"d"},
	{499,	"id"},
	{490,	"xd"},
	{400,	"cd"},
	{100,	"c"},
	{99,	"ic"},
	{90,	"xc"},
	{50,	"l"},
	{49,	"il"},
	{40,	"xl"},
	{10,	"x"},
	{9,	"ix"},
	{5,	"v"},
	{4,	"iv"},
	{1,	"i"},
	{0,	NULL}
};

static void roman(char *p, unsigned int n, const size_t psz)
{
	int i = 0;
	if (!n) {
		if (strlcpy(p, "o", psz) >= psz)
			die("strlcpy(): dstsize too small\n");
		return;
	} else if (n >= 4000) {
		if (strlcpy(p, "---", psz) >= psz)
			die("strlcpy(): dstsize too small\n");
		return;
	}
	p[0] = 0;
	while (n) {
		while (roman_tbl[i].n <= n) {
			n -= roman_tbl[i].n;
			if (strlcat(p, roman_tbl[i].s, psz) >= psz)
				die("strlcat(): dstsize too small\n");
		}
		i++;
		if (n && !roman_tbl[i].n) {
			internal("BUG in roman number convertor");
			return;
		}
	}
}

struct color_spec {
	const char *name;
	const int rgb;
};

static const struct color_spec color_specs[] = {
	{"aliceblue",		0xF0F8FF},
	{"antiquewhite",	0xFAEBD7},
	{"aqua",		0x00FFFF},
	{"aquamarine",		0x7FFFD4},
	{"azure",		0xF0FFFF},
	{"beige",		0xF5F5DC},
	{"bisque",		0xFFE4C4},
	{"black",		0x000000},
	{"blanchedalmond",	0xFFEBCD},
	{"blue",		0x0000FF},
	{"blueviolet",		0x8A2BE2},
	{"brown",		0xA52A2A},
	{"burlywood",		0xDEB887},
	{"cadetblue",		0x5F9EA0},
	{"chartreuse",		0x7FFF00},
	{"chocolate",		0xD2691E},
	{"coral",		0xFF7F50},
	{"cornflowerblue",	0x6495ED},
	{"cornsilk",		0xFFF8DC},
	{"crimson",		0xDC143C},
	{"cyan",		0x00FFFF},
	{"darkblue",		0x00008B},
	{"darkcyan",		0x008B8B},
	{"darkgoldenrod",	0xB8860B},
	{"darkgray",		0xA9A9A9},
	{"darkgreen",		0x006400},
	{"darkkhaki",		0xBDB76B},
	{"darkmagenta",		0x8B008B},
	{"darkolivegreen",	0x556B2F},
	{"darkorange",		0xFF8C00},
	{"darkorchid",		0x9932CC},
	{"darkred",		0x8B0000},
	{"darksalmon",		0xE9967A},
	{"darkseagreen",	0x8FBC8F},
	{"darkslateblue",	0x483D8B},
	{"darkslategray",	0x2F4F4F},
	{"darkturquoise",	0x00CED1},
	{"darkviolet",		0x9400D3},
	{"deeppink",		0xFF1493},
	{"deepskyblue",		0x00BFFF},
	{"dimgray",		0x696969},
	{"dodgerblue",		0x1E90FF},
	{"firebrick",		0xB22222},
	{"floralwhite",		0xFFFAF0},
	{"forestgreen",		0x228B22},
	{"fuchsia",		0xFF00FF},
	{"gainsboro",		0xDCDCDC},
	{"ghostwhite",		0xF8F8FF},
	{"gold",		0xFFD700},
	{"goldenrod",		0xDAA520},
	{"gray",		0x808080},
	{"green",		0x008000},
	{"greenyellow",		0xADFF2F},
	{"honeydew",		0xF0FFF0},
	{"hotpink",		0xFF69B4},
	{"indianred",		0xCD5C5C},
	{"indigo",		0x4B0082},
	{"ivory",		0xFFFFF0},
	{"khaki",		0xF0E68C},
	{"lavender",		0xE6E6FA},
	{"lavenderblush",	0xFFF0F5},
	{"lawngreen",		0x7CFC00},
	{"lemonchiffon",	0xFFFACD},
	{"lightblue",		0xADD8E6},
	{"lightcoral",		0xF08080},
	{"lightcyan",		0xE0FFFF},
	{"lightgoldenrodyellow",	0xFAFAD2},
	{"lightgreen",		0x90EE90},
	{"lightgrey",		0xD3D3D3},
	{"lightpink",		0xFFB6C1},
	{"lightsalmon",		0xFFA07A},
	{"lightseagreen",	0x20B2AA},
	{"lightskyblue",	0x87CEFA},
	{"lightslategray",	0x778899},
	{"lightsteelblue",	0xB0C4DE},
	{"lightyellow",		0xFFFFE0},
	{"lime",		0x00FF00},
	{"limegreen",		0x32CD32},
	{"linen",		0xFAF0E6},
	{"magenta",		0xFF00FF},
	{"maroon",		0x800000},
	{"mediumaquamarine",	0x66CDAA},
	{"mediumblue",		0x0000CD},
	{"mediumorchid",	0xBA55D3},
	{"mediumpurple",	0x9370DB},
	{"mediumseagreen",	0x3CB371},
	{"mediumslateblue",	0x7B68EE},
	{"mediumspringgreen",	0x00FA9A},
	{"mediumturquoise",	0x48D1CC},
	{"mediumvioletred",	0xC71585},
	{"midnightblue",	0x191970},
	{"mintcream",		0xF5FFFA},
	{"mistyrose",		0xFFE4E1},
	{"moccasin",		0xFFE4B5},
	{"navajowhite",		0xFFDEAD},
	{"navy",		0x000080},
	{"oldlace",		0xFDF5E6},
	{"olive",		0x808000},
	{"olivedrab",		0x6B8E23},
	{"orange",		0xFFA500},
	{"orangered",		0xFF4500},
	{"orchid",		0xDA70D6},
	{"palegoldenrod",	0xEEE8AA},
	{"palegreen",		0x98FB98},
	{"paleturquoise",	0xAFEEEE},
	{"palevioletred",	0xDB7093},
	{"papayawhip",		0xFFEFD5},
	{"peachpuff",		0xFFDAB9},
	{"peru",		0xCD853F},
	{"pink",		0xFFC0CB},
	{"plum",		0xDDA0DD},
	{"powderblue",		0xB0E0E6},
	{"purple",		0x800080},
	{"red",			0xFF0000},
	{"rosybrown",		0xBC8F8F},
	{"royalblue",		0x4169E1},
	{"saddlebrown",		0x8B4513},
	{"salmon",		0xFA8072},
	{"sandybrown",		0xF4A460},
	{"seagreen",		0x2E8B57},
	{"seashell",		0xFFF5EE},
	{"sienna",		0xA0522D},
	{"silver",		0xC0C0C0},
	{"skyblue",		0x87CEEB},
	{"slateblue",		0x6A5ACD},
	{"slategray",		0x708090},
	{"snow",		0xFFFAFA},
	{"springgreen",		0x00FF7F},
	{"steelblue",		0x4682B4},
	{"tan",			0xD2B48C},
	{"teal",		0x008080},
	{"thistle",		0xD8BFD8},
	{"tomato",		0xFF6347},
	{"turquoise",		0x40E0D0},
	{"violet",		0xEE82EE},
	{"wheat",		0xF5DEB3},
	{"white",		0xFFFFFF},
	{"whitesmoke",		0xF5F5F5},
	{"yellow",		0xFFFF00},
	{"yellowgreen",		0x9ACD32},
};

#define endof(T) ((T) + array_elements(T))

int decode_color(unsigned char *str, struct rgb *col)
{
	unsigned long ch;
	char *end;
	if (*str != '#') {
		const struct color_spec *cs;
		for (cs = color_specs; cs < endof(color_specs); cs++)
			if (!casestrcmp(cast_uchar cs->name, str)) {
				ch = cs->rgb;
				goto found;
			}
	} else {
		str++;
	}
	if (strlen(cast_const_char str) == 6) {
		ch = strtoul(cast_const_char str, &end, 16);
		if (!*end && ch < 0x1000000) {
found:
			memset(col, 0, sizeof(struct rgb));
			col->r = (unsigned)ch / 0x10000;
			col->g = (unsigned)ch / 0x100 % 0x100;
			col->b = (unsigned)ch % 0x100;
			return 0;
		}
	}
	if (strlen(cast_const_char str) == 3) {
		ch = strtoul(cast_const_char str, &end, 16);
		if (!*end && ch < 0x1000) {
			memset(col, 0, sizeof(struct rgb));
			col->r = ((unsigned)ch / 0x100) * 0x11;
			col->g = ((unsigned)ch / 0x10 % 0x10) * 0x11;
			col->b = ((unsigned)ch % 0x10) * 0x11;
			return 0;
		}
	}
	return -1;
}

int get_color(unsigned char *a, unsigned char *c, struct rgb *rgb)
{
	unsigned char *at;
	int r = -1;
	if (d_opt->col >= 1) if ((at = get_attr_val(a, c))) {
		r = decode_color(at, rgb);
		free(at);
	}
	return r;
}

int get_bgcolor(unsigned char *a, struct rgb *rgb)
{
	if (d_opt->col < 2) return -1;
	return get_color(a, cast_uchar "bgcolor", rgb);
}

static unsigned char *get_target(unsigned char *a)
{
	return get_attr_val(a, cast_uchar "target");
}

void kill_html_stack_item(struct html_element *e)
{
	if (!e || (void *)e == &html_stack) {
		internal("trying to free bad html element");
		return;
	}
	if (e->dontkill == 2) {
		internal("trying to kill unkillable element");
		return;
	}
	html_format_changed = 1;
	free(e->attr.fontface);
	free(e->attr.link);
	free(e->attr.target);
	free(e->attr.image);
	free(e->attr.href_base);
	free(e->attr.target_base);
	free(e->attr.select);
	del_from_list(e);
	free(e);
}

void html_stack_dup(void)
{
	struct html_element *e;
	struct html_element *ep;
	html_format_changed = 1;
	ep = &html_top;
	e = xmalloc(sizeof(struct html_element));
	memcpy(e, ep, sizeof(struct html_element));
	e->attr.fontface = stracpy(ep->attr.fontface);
	e->attr.link = stracpy(ep->attr.link);
	e->attr.target = stracpy(ep->attr.target);
	e->attr.image = stracpy(ep->attr.image);
	e->attr.href_base = stracpy(ep->attr.href_base);
	e->attr.target_base = stracpy(ep->attr.target_base);
	e->attr.select = stracpy(ep->attr.select);
	e->name = e->options = NULL;
	e->namelen = 0;
	e->dontkill = 0;
	add_to_list(html_stack, e);
}

void *ff;
void (*put_chars_f)(void *, unsigned char *, int);
void (*line_break_f)(void *);
void *(*special_f)(void *, int, ...);

static unsigned char *eoff;
unsigned char *eofff;
unsigned char *startf;

int line_breax;
static int pos;
static int putsp;

static int was_br;
int table_level;
int empty_format;

static void ln_break(const int n)
{
	if (!n || html_top.invisible) return;
	while (n > line_breax) {
		line_breax++;
		line_break_f(ff);
	}
	pos = 0;
	putsp = -1;
}

#define CH_BUF		256
#define BUF_RESERVE	6

static int put_chars_conv(unsigned char *c, int l)
{
	static unsigned char buffer[CH_BUF];
	int bp = 0;
	int pp = 0;
	int total = 0;
	if (format_.attr & AT_GRAPHICS) {
		put_chars_f(ff, c, l);
		return l;
	}
	if (!l) put_chars_f(ff, NULL, 0);
	while (pp < l) {
		int sl;
		unsigned char *e = NULL;	/* against warning */
		if (c[pp] < 128 && c[pp] != '&') {
			put_c:
			if (bp > CH_BUF - BUF_RESERVE && c[pp] >= 0xc0) goto flush;
			if (!(buffer[bp++] = c[pp++])) buffer[bp - 1] = ' ';
			if ((buffer[bp - 1] != ' ' || par_format.align == AL_NO || par_format.align == AL_NO_BREAKABLE) && bp < CH_BUF) continue;
			goto flush;
		}
		if (c[pp] != '&') {
			struct conv_table *t;
			int i;
			if (l - pp >= 3 && c[pp] == 0xef && c[pp + 1] == 0xbb && c[pp + 2] == 0xbf && !d_opt->real_cp) {
				pp += 3;
				continue;
			}
			if ((d_opt->real_cp == d_opt->cp && !d_opt->real_cp) || !convert_table)
				goto put_c;
			t = convert_table;
			i = pp;
			decode:
			if (!t[c[i]].t) {
				e = t[c[i]].u.str;
			} else {
				t = t[c[i++]].u.tbl;
				if (i >= l) goto put_c;
				goto decode;
			}
			pp = i + 1;
		} else {
			int i = pp + 1;
			if (d_opt->plain & 1) goto put_c;
			while (i < l && !is_entity_terminator(c[i])) i++;
			if (!(e = get_entity_string(&c[pp + 1], i - pp - 1))) goto put_c;
			pp = i + (i < l && c[i] == ';');
		}
		if (!e[0]) continue;
		if (!e[1]) {
			buffer[bp++] = e[0];
			if (bp < CH_BUF) continue;
			flush:
			e = cast_uchar "";
			goto flush1;
		}
		sl = (int)strlen(cast_const_char e);
		if (sl > BUF_RESERVE) {
			e = cast_uchar "";
			sl = 0;
		}
		if (bp + sl > CH_BUF) {
			flush1:
			put_chars_f(ff, buffer, bp);
			if (!d_opt->cp) {
				while (bp) if ((buffer[--bp] & 0xc0) != 0x80) total++;
			} else {
				total += bp;
				bp = 0;
			}
		}
		while (*e) {
			buffer[bp++] = *(e++);
		}
		if (bp == CH_BUF) goto flush;
	}
	if (bp) put_chars_f(ff, buffer, bp);
	if (!d_opt->cp) {
		while (bp) if ((buffer[--bp] & 0xc0) != 0x80) total++;
	} else {
		total += bp;
	}
	return total;
}

static void put_chrs(unsigned char *start, int len)
{
	if (par_format.align == AL_NO || par_format.align == AL_NO_BREAKABLE) putsp = 0;
	if (!len || html_top.invisible) return;
	if (putsp == 1) {
		pos += put_chars_conv(cast_uchar " ", 1);
		putsp = -1;
	}
	if (putsp == -1) {
		if (start[0] == ' ') {
			start++;
			len--;
		}
		putsp = 0;
	}
	if (!len) {
		putsp = -1;
		if (par_format.align == AL_NO || par_format.align == AL_NO_BREAKABLE) putsp = 0;
		return;
	}
	if (start[len - 1] == ' ') putsp = -1;
	if (par_format.align == AL_NO || par_format.align == AL_NO_BREAKABLE) putsp = 0;
	was_br = 0;
	pos += put_chars_conv(start, len);
	line_breax = 0;
}

static void kill_until(int ls, ...)
{
	int l;
	struct list_head *e = &html_top.list_entry;
	if (ls) e = e->next;
	while (e != &html_stack) {
		struct html_element *he = list_struct(e, struct html_element);
		int sk = 0;
		va_list arg;
		va_start(arg, ls);
		while (1) {
			unsigned char *s = va_arg(arg, unsigned char *);
			if (!s) break;
			if (!*s) sk++;
			else if ((size_t)he->namelen == strlen(cast_const_char s) && !casecmp(he->name, s, strlen(cast_const_char s))) {
				if (!sk) {
					if (he->dontkill) break;
					va_end(arg);
					goto killll;
				}
				else if (sk == 1) {
					va_end(arg);
					goto killl;
				} else break;
			}
		}
		va_end(arg);
		if (he->dontkill || (he->namelen == 5 && !casecmp(he->name, cast_uchar "TABLE", 5))) break;
		if (he->namelen == 2 && upcase(he->name[0]) == 'T' && (upcase(he->name[1]) == 'D' || upcase(he->name[1]) == 'H' || upcase(he->name[1]) == 'R')) break;
		e = e->next;
	}
	return;
	killl:
	e = e->prev;
	killll:
	l = 0;
	while (e != &html_stack) {
		struct html_element *he = list_struct(e, struct html_element);
		if (ls && e == html_stack.next) break;
		if (he->linebreak > l) l = he->linebreak;
		e = e->prev;
		kill_html_stack_item(he);
	}
	ln_break(l);
}

static inline unsigned char *top_href_base(void)
{
	return list_struct(html_stack.prev, struct html_element)->attr.href_base;
}

int get_num(unsigned char *a, unsigned char *n)
{
	unsigned char *al;
	if ((al = get_attr_val(a, n))) {
		char *end;
		unsigned long s = strtoul(cast_const_char al, &end, 10);
		if (!*al || *end || s > 10000) s = -1;
		free(al);
		return (int)s;
	}
	return -1;
}

/* trunc somehow clips the maximum values. Use 0 to disable truncastion. */
static int parse_width(const char *w, const int trunc)
{
	char *end;
	int p = 0;
	long s;
	int l;
	int limit = par_format.width - (par_format.leftmargin + par_format.rightmargin) * gf_val(1, G_HTML_MARGIN);
	while (WHITECHAR(*w)) w++;
	for (l = 0; w[l] && w[l] != ','; l++)
		;
	while (l && WHITECHAR(w[l - 1])) l--;
	if (!l) return -1;
	if (w[l - 1] == '%') {
		l--;
		p = 1;
	}
	while (l && WHITECHAR(w[l - 1])) l--;
	if (!l) return -1;
	s = strtol(w, &end, 10);
	if (end - w < l || s < 0 || s > 10000) return -1;
	if (p) {
		if (trunc) {
			s = s * limit / 100;
		}
		else return -1;
	} else s = (s + (gf_val(HTML_CHAR_WIDTH, 1) - 1) / 2) / gf_val(HTML_CHAR_WIDTH, 1);
	if (trunc == 1 && s > limit) s = limit;
	if (s < 0) s = 0;
	return (int)s;
}

/* trunc somehow clips the maximum values. Use 0 to disable truncastion. */
int get_width(unsigned char *a, unsigned char *n, int trunc)
{
	int r;
	char *w;
	if (!(w = (char *)get_attr_val(a, n))) return -1;
	r = parse_width(w, trunc);
	free(w);
	return r;
}

static unsigned char *find_element_end(unsigned char *a)
{
	unsigned char *p;
	for (p = a - 1; *p != '<'; p--)
		;
	if (parse_element(p, eoff, NULL, NULL, NULL, &p)) {
		internal("parse element failed");
		return a;
	}
	return p;
}

struct form form = { NULL, NULL, NULL, NULL, 0, 0 };

unsigned char *last_form_tag;
unsigned char *last_form_attr;
unsigned char *last_input_tag;

static inline void set_link_attr(void)
{
	memcpy(!(format_.attr & AT_INVERT) ? &format_.fg : &format_.bg, &format_.clink, sizeof(struct rgb));
}

static void put_link_line(unsigned char *prefix, unsigned char *linkname, unsigned char *link, unsigned char *target)
{
	if (!casecmp(link, cast_uchar "android-app:", 12))
		return;
	html_stack_dup();
	ln_break(1);
	free(format_.link);
	format_.link = NULL;
	free(format_.target);
	format_.target = NULL;
	format_.form = NULL;
	put_chrs(prefix, (int)strlen(cast_const_char prefix));
	html_format_changed = 1;
	format_.link = join_urls(format_.href_base, link);
	format_.target = stracpy(target);
	set_link_attr();
	put_chrs(linkname, (int)strlen(cast_const_char linkname));
	ln_break(1);
	kill_html_stack_item(&html_top);
}

static void html_span(unsigned char *a)
{
	unsigned char *al;
	if ((al = get_attr_val(a, cast_uchar "class"))) {
		if (!strcmp(cast_const_char al, "line-number"))
			ln_break(1);
		if (!strcmp(cast_const_char al, "blob-code-inner")) {	/* github hack */
			ln_break(1);
			format_.attr |= AT_FIXED;
			par_format.align = AL_NO;
		}

		free(al);
	}
}

static void html_bold(unsigned char *a)
{
	format_.attr |= AT_BOLD;
}

static void html_italic(unsigned char *a)
{
	format_.attr |= AT_ITALIC;
}

static void html_underline(unsigned char *a)
{
	format_.attr |= AT_UNDERLINE;
}

static void html_fixed(unsigned char *a)
{
	format_.attr |= AT_FIXED;
}

static void html_invert(unsigned char *a)
{
	struct rgb rgb;
	memcpy(&rgb, &format_.fg, sizeof(struct rgb));
	memcpy(&format_.fg, &format_.bg, sizeof(struct rgb));
	memcpy(&format_.bg, &rgb, sizeof(struct rgb));
	format_.attr ^= AT_INVERT;
}

static void html_a(unsigned char *a)
{
	unsigned char *al;

	if ((al = get_url_val(a, cast_uchar "href"))) {
		unsigned char *all = al;
		while (all[0] == ' ') all++;
		while (all[0] && all[strlen(cast_const_char all) - 1] == ' ') all[strlen(cast_const_char all) - 1] = 0;
		free(format_.link);
		format_.link = join_urls(format_.href_base, all);
		free(al);
		if ((al = get_target(a))) {
			free(format_.target);
			format_.target = al;
		} else {
			free(format_.target);
			format_.target = stracpy(format_.target_base);
		}
		/*format_.attr ^= AT_BOLD;*/
		set_link_attr();
	} else
		kill_html_stack_item(&html_top);
	if ((al = get_attr_val(a, cast_uchar "name"))) {
		special_f(ff, SP_TAG, al);
		free(al);
	}
}

static void html_a_special(unsigned char *a, unsigned char *next, unsigned char *eof)
{
	unsigned char *t;
	if (!format_.link) return;
	while (next < eof && WHITECHAR(*next)) next++;
	if (eof - next >= 4 && next[0] == '<' && next[1] == '/' && upcase(next[2]) == 'A' && next[3] == '>')
		goto ok;
	if (strstr(cast_const_char format_.link, "/raw/"))      /* gitlab hack */
		goto ok;
	return;

 ok:
	if (!has_attr(a, cast_uchar "href")) return;
	t = get_attr_val(a, cast_uchar "title");
	if (!t) return;
	put_chrs(t, (int)strlen(cast_const_char t));
	free(t);
}

static void html_sub(unsigned char *a)
{
	if (!F) put_chrs(cast_uchar "_", 1);
	format_.fontsize = 1;
	format_.baseline = -1;
}

static void html_sup(unsigned char *a)
{
	if (!F) put_chrs(cast_uchar "^", 1);
	format_.fontsize = 1;
	if (format_.baseline <= 0) format_.baseline = format_.fontsize;
}


static void html_font(unsigned char *a)
{
	unsigned char *al;
	if ((al = get_attr_val(a, cast_uchar "size"))) {
		int p = 0;
		unsigned long s;
		unsigned char *nn = al;
		char *end;
		if (*al == '+') {
			p = 1;
			nn++;
		}
		if (*al == '-') {
			p = -1;
			nn++;
		}
		s = strtoul(cast_const_char nn, &end, 10);
		if (*nn && !*end) {
			if (s > 7) s = 7;
			if (!p) format_.fontsize = (int)s;
			else format_.fontsize += p * (int)s;
			if (format_.fontsize < 1) format_.fontsize = 1;
			if (format_.fontsize > 7) format_.fontsize = 7;
		}
		free(al);
	}
	get_color(a, cast_uchar "color", &format_.fg);
}

static unsigned char *get_url_val_img(unsigned char *a, unsigned char *name)
{
	unsigned char *v = get_url_val(a, name);
	if (v && !v[strcspn(cast_const_char v, "./")]) {
		free(v);
		v = NULL;
	}
	return v;
}

static void html_img(unsigned char *a)
{
	unsigned char *al;
	unsigned char *s;
	unsigned char *orig_link = NULL;
	int ismap, usemap = 0;
	/*put_chrs(cast_uchar " ", 1);*/
	if ((!F || !d_opt->display_images) && ((al = get_url_val(a, cast_uchar "usemap")))) {
		unsigned char *u;
		usemap = 1;
		html_stack_dup();
		free(format_.link);
		format_.form = NULL;
		u = join_urls(*al == '#' ? top_href_base() : format_.href_base, al);
		format_.link = stracpy(cast_uchar "MAP@");
		add_to_strn(&format_.link, u);
		format_.attr |= AT_BOLD;
		free(u);
		free(al);
	}
	ismap = format_.link && (F || !has_attr(a, cast_uchar "usemap")) && has_attr(a, cast_uchar "ismap");
	free(format_.image);
	format_.image = NULL;
	if ((s = get_url_val_img(a, cast_uchar "data-defer-src"))
	|| (s = get_url_val_img(a, cast_uchar "data-delay-url"))
	|| (s = get_url_val_img(a, cast_uchar "data-full"))
	|| (s = get_url_val_img(a, cast_uchar "data-lazy"))
	|| (s = get_url_val_img(a, cast_uchar "data-lazy-src"))
	|| (s = get_url_val_img(a, cast_uchar "data-li-src"))
	|| (s = get_url_val_img(a, cast_uchar "data-normal"))
	|| (s = get_url_val_img(a, cast_uchar "data-original"))
	|| (s = get_url_val_img(a, cast_uchar "data-small"))
	|| (s = get_url_val_img(a, cast_uchar "data-source"))
	|| (s = get_url_val_img(a, cast_uchar "data-src"))
	|| (s = get_url_val_img(a, cast_uchar "data-thumb"))
	|| (s = get_url_val_img(a, cast_uchar "src"))
	|| (s = get_url_val_img(a, cast_uchar "dynsrc"))
	|| (s = get_url_val_img(a, cast_uchar "data"))
	|| (s = get_url_val_img(a, cast_uchar "content"))
	|| (s = get_url_val(a, cast_uchar "src"))) {
		 if (!s[0]) goto skip_img;
		 format_.image = join_urls(format_.href_base, s);
 skip_img:
		 orig_link = s;
	}
	if (!F || !d_opt->display_images) {
		if ((!(al = get_attr_val(a, cast_uchar "alt"))
		&& !(al = get_attr_val(a, cast_uchar "title")))
		|| !*al) {
			free(al);
			if (!d_opt->images && !format_.link)
				goto ret;
			if (d_opt->image_names && s) {
				unsigned char *ss;
				al = stracpy(cast_uchar "[");
				if (!(ss = cast_uchar strrchr(cast_const_char s, '/')))
					ss = s;
				else
					ss++;
				add_to_strn(&al, ss);
				if ((ss = cast_uchar strchr(cast_const_char al, '?')))
					*ss = 0;
				if ((ss = cast_uchar strchr(cast_const_char al, '&')))
					*ss = 0;
				add_to_strn(&al, cast_uchar "]");
			} else if (usemap)
				al = stracpy(cast_uchar "[USEMAP]");
			else if
				(ismap) al = stracpy(cast_uchar "[ISMAP]");
			else
				al = stracpy(cast_uchar "[IMG]");
		}
		if (al) {
			if (ismap) {
				unsigned char *h;
				html_stack_dup();
				h = stracpy(format_.link);
				add_to_strn(&h, cast_uchar "?0,0");
				free(format_.link);
				format_.link = h;
			}
			html_format_changed = 1;
			put_chrs(al, (int)strlen(cast_const_char al));
			if (ismap) kill_html_stack_item(&html_top);
		}
		free(al);
	}
 ret:
	free(format_.image);
	format_.image = NULL;
	html_format_changed = 1;
	if (usemap) kill_html_stack_item(&html_top);
	free(orig_link);
}

static void html_obj(unsigned char *a, int obj)
{
	unsigned char *old_base = format_.href_base;
	unsigned char *url;
	unsigned char *type = get_attr_val(a, cast_uchar "type");
	unsigned char *base;
	if ((base = get_url_val(a, cast_uchar "codebase"))) format_.href_base = join_urls(format_.href_base, base);
	if (!type) {
		url = get_url_val(a, cast_uchar "src");
		if (!url) url = get_url_val(a, cast_uchar "data");
		if (url) {
			unsigned char *ju = join_urls(format_.href_base, url);
			type = get_content_type(NULL, ju);
			free(url);
			free(ju);
		}
	}
	url = get_url_val(a, cast_uchar "src");
	if (!url) url = get_url_val(a, cast_uchar "data");
	if (url) {
		put_link_line(cast_uchar "", !obj ? cast_uchar "[EMBED]" : cast_uchar "[OBJ]", url, cast_uchar "");
		free(url);
	}
	if (base) {
		free(format_.href_base);
		format_.href_base = old_base;
		free(base);
	}
	free(type);
}

static void html_embed(unsigned char *a)
{
	html_obj(a, 0);
}

static void html_object(unsigned char *a)
{
	html_obj(a, 1);
}

static void html_body(unsigned char *a)
{
	get_color(a, cast_uchar "text", &format_.fg);
	get_color(a, cast_uchar "link", &format_.clink);
	if (has_attr(a, cast_uchar "onload")) special_f(ff, SP_SCRIPT, NULL);
	/*
	get_bgcolor(a, &format_.bg);
	get_bgcolor(a, &par_format.bgcolor);
	*/
}

static void html_skip(unsigned char *a)
{
	html_top.dontkill = 1;
	html_top.invisible = INVISIBLE;
}

static void html_title(unsigned char *a)
{
	if (a[0] == '>' && a[-1] == '/') return;
	html_top.dontkill = 1;
	html_top.invisible = INVISIBLE;
}

int should_skip_script(unsigned char *a)
{
	return !has_attr(a, cast_uchar "/");
}

static void html_script(unsigned char *a)
{
	unsigned char *s;
	s = get_url_val(a, cast_uchar "src");
	special_f(ff, SP_SCRIPT, s);
	free(s);
	if (should_skip_script(a)) {
		html_top.dontkill = 1;
		html_top.invisible = INVISIBLE_SCRIPT;
	}
}

static void html_style(unsigned char *a)
{
	html_top.dontkill = 1;
	html_top.invisible = INVISIBLE_STYLE;
}

static void html_center(unsigned char *a)
{
	par_format.align = AL_CENTER;
	if (!table_level && !F) par_format.leftmargin = par_format.rightmargin = 0;
}

static void html_linebrk(unsigned char *a)
{
	unsigned char *al;
	if ((al = get_attr_val(a, cast_uchar "align"))) {
		if (!casestrcmp(al, cast_uchar "left")) par_format.align = AL_LEFT;
		if (!casestrcmp(al, cast_uchar "right")) par_format.align = AL_RIGHT;
		if (!casestrcmp(al, cast_uchar "center")) {
			par_format.align = AL_CENTER;
			if (!table_level && !F) par_format.leftmargin = par_format.rightmargin = 0;
		}
		if (!casestrcmp(al, cast_uchar "justify")) par_format.align = AL_BLOCK;
		free(al);
	}
}

static void html_br(unsigned char *a)
{
	html_linebrk(a);
	if (par_format.align != AL_NO && par_format.align != AL_NO_BREAKABLE) {
		if (was_br) ln_break(2);
		was_br = 1;
	}
}

static void html_form(unsigned char *a)
{
	was_br = 1;
}

static void html_p(unsigned char *a)
{
	if (par_format.leftmargin < margin) par_format.leftmargin = margin;
	if (par_format.rightmargin < margin) par_format.rightmargin = margin;
	/*par_format.align = AL_LEFT;*/
	html_linebrk(a);
}

static void html_address(unsigned char *a)
{
	par_format.leftmargin += 1;
	par_format.align = AL_LEFT;
}

static void html_blockquote(unsigned char *a)
{
	par_format.leftmargin += 2;
	par_format.align = AL_LEFT;
}

static void html_h(int h, unsigned char *a)
{
	par_format.align = AL_LEFT;
	if (h == 1)
		return;
	html_linebrk(a);
	switch (par_format.align) {
		case AL_LEFT:
			break;
		case AL_RIGHT:
			par_format.leftmargin = 0;
			par_format.rightmargin = (h - 2) * 2;
			break;
		case AL_CENTER:
			par_format.leftmargin = par_format.rightmargin = 0;
			break;
		case AL_BLOCK:
			par_format.leftmargin = par_format.rightmargin = (h - 2) * 2;
			break;
	}
}

static void html_h1(unsigned char *a) { html_h(1, a); }
static void html_h2(unsigned char *a) { html_h(2, a); }
static void html_h3(unsigned char *a) { html_h(3, a); }
static void html_h4(unsigned char *a) { html_h(4, a); }
static void html_h5(unsigned char *a) { html_h(5, a); }
static void html_h6(unsigned char *a) { html_h(6, a); }

static void html_pre(unsigned char *a)
{
	unsigned char *cl;
	format_.attr |= AT_FIXED;
	par_format.align = !par_format.implicit_pre_wrap ? AL_NO : AL_NO_BREAKABLE;
	par_format.leftmargin = par_format.leftmargin > 1;
	par_format.rightmargin = par_format.leftmargin;
	if ((cl = get_attr_val(a, cast_uchar "class"))) {
		if (strstr(cast_const_char cl, "bz_comment"))	/* hack */
			par_format.align = AL_NO_BREAKABLE;
		free(cl);
	}
}

static void html_div(unsigned char *a)
{
	unsigned char *al;
	if ((al = get_attr_val(a, cast_uchar "class"))) {
		if (!strcmp(cast_const_char al, "commit-msg") ||
		    !strcmp(cast_const_char al, "pre") /* sourceware hack */ ||
		    (!strncmp(cast_const_char al, "diff", 4) && casestrcmp(al, cast_uchar "diff-view") && strncmp(cast_const_char al, "diffbar", 7)) /* gitweb hack, github counter-hacks */ ||
		    0) {
			format_.attr |= AT_FIXED;
			par_format.align = AL_NO;
		} else if (strstr(cast_const_char al, "plain-text-white-space")) {
			format_.attr |= AT_FIXED;
			par_format.align = AL_NO_BREAKABLE;
		}
		free(al);
	}
	html_linebrk(a);
}

static void html_hr(unsigned char *a)
{
	int i;
	int q = get_num(a, cast_uchar "size");
	html_stack_dup();
	par_format.align = AL_CENTER;
	free(format_.link);
	format_.link = NULL;
	format_.form = NULL;
	html_linebrk(a);
	if (par_format.align == AL_BLOCK) par_format.align = AL_CENTER;
	par_format.leftmargin = margin;
	par_format.rightmargin = margin;
	i = get_width(a, cast_uchar "width", 1);
	if (!F) {
		unsigned char r = 205;
		if (q >= 0 && q < 2) r = 196;
		if (i < 0) i = par_format.width - 2 * margin - 4;
		format_.attr = AT_GRAPHICS;
		special_f(ff, SP_NOWRAP, 1);
		while (i-- > 0) put_chrs(&r, 1);
		special_f(ff, SP_NOWRAP, 0);
	}
	ln_break(2);
	kill_html_stack_item(&html_top);
}

static void html_table(unsigned char *a)
{
	par_format.leftmargin = margin;
	par_format.rightmargin = margin;
	par_format.align = AL_LEFT;
	html_linebrk(a);
	format_.attr = 0;
}

static void html_tr(unsigned char *a)
{
	html_linebrk(a);
}

static void html_th(unsigned char *a)
{
	/*html_linebrk(a);*/
	kill_until(1, cast_uchar "TD", cast_uchar "TH", cast_uchar "", cast_uchar "TR", cast_uchar "TABLE", NULL);
	format_.attr |= AT_BOLD;
	put_chrs(cast_uchar " ", 1);
}

static void html_td(unsigned char *a)
{
	/*html_linebrk(a);*/
	kill_until(1, cast_uchar "TD", cast_uchar "TH", cast_uchar "", cast_uchar "TR", cast_uchar "TABLE", NULL);
	format_.attr &= ~AT_BOLD;
	put_chrs(cast_uchar " ", 1);
}

static void html_base(unsigned char *a)
{
	unsigned char *al;
	if ((al = get_url_val(a, cast_uchar "href"))) {
		free(format_.href_base);
		format_.href_base = join_urls(top_href_base(), al);
		special_f(ff, SP_SET_BASE, format_.href_base);
		free(al);
	}
	if ((al = get_target(a))) {
		free(format_.target_base);
		format_.target_base = al;
	}
}

static void html_ul(unsigned char *a)
{
	unsigned char *al;
	/*debug_stack();*/
	par_format.list_level++;
	par_format.list_number = 0;
	par_format.flags = P_STAR;
	if ((al = get_attr_val(a, cast_uchar "type"))) {
		if (!casestrcmp(al, cast_uchar "disc") ||
		    !casestrcmp(al, cast_uchar "circle")) par_format.flags = P_O;
		if (!casestrcmp(al, cast_uchar "square")) par_format.flags = P_PLUS;
		free(al);
	}
	if ((par_format.leftmargin += 2 + (par_format.list_level > 1)) > par_format.width * 2 / 3 && !table_level)
		par_format.leftmargin = par_format.width * 2 / 3;
	par_format.align = AL_LEFT;
	html_top.dontkill = 1;
}

static void html_ol(unsigned char *a)
{
	unsigned char *al;
	int st;
	par_format.list_level++;
	st = get_num(a, cast_uchar "start");
	if (st == -1) st = 1;
	par_format.list_number = st;
	par_format.flags = P_NUMBER;
	if ((al = get_attr_val(a, cast_uchar "type"))) {
		if (!strcmp(cast_const_char al, "1")) par_format.flags = P_NUMBER;
		if (!strcmp(cast_const_char al, "a")) par_format.flags = P_alpha;
		if (!strcmp(cast_const_char al, "A")) par_format.flags = P_ALPHA;
		if (!strcmp(cast_const_char al, "r")) par_format.flags = P_roman;
		if (!strcmp(cast_const_char al, "R")) par_format.flags = P_ROMAN;
		if (!strcmp(cast_const_char al, "i")) par_format.flags = P_roman;
		if (!strcmp(cast_const_char al, "I")) par_format.flags = P_ROMAN;
		free(al);
	}
	if (!F) if ((par_format.leftmargin += (par_format.list_level > 1)) > par_format.width * 2 / 3 && !table_level)
		par_format.leftmargin = par_format.width * 2 / 3;
	par_format.align = AL_LEFT;
	html_top.dontkill = 1;
}

static void html_li(unsigned char *a)
{
	/*kill_until(0, cast_uchar "", cast_uchar "UL", cast_uchar "OL", NULL);*/
	if (!par_format.list_number) {
		unsigned char x[8] = "*&nbsp;";
		if ((par_format.flags & P_LISTMASK) == P_O) x[0] = 'o';
		if ((par_format.flags & P_LISTMASK) == P_PLUS) x[0] = '+';
		put_chrs(x, 7);
		if (!F) par_format.leftmargin += 2;
		par_format.align = AL_LEFT;
		putsp = -1;
	} else {
		unsigned char c = 0;
		char n[32];
		int t = par_format.flags & P_LISTMASK;
		int s = get_num(a, cast_uchar "value");
		if (s != -1) par_format.list_number = s;
		if ((t != P_roman && t != P_ROMAN
		&& par_format.list_number < 10)
		|| t == P_alpha || t == P_ALPHA) {
			put_chrs(cast_uchar "&nbsp;", 6);
			c = 1;
		}
		if (t == P_ALPHA || t == P_alpha) {
			n[0] = par_format.list_number ? (par_format.list_number - 1) % 26 + (t == P_ALPHA ? 'A' : 'a') : 0;
			n[1] = 0;
		} else if (t == P_ROMAN || t == P_roman) {
			roman(n, par_format.list_number, sizeof(n));
			if (t == P_ROMAN) {
				char *x;
				for (x = n; *x; x++)
					*x = upcase(*x);
			}
		} else sprintf(cast_char n, "%d", par_format.list_number);
		put_chrs((unsigned char *)n, strlen(n));
		put_chrs(cast_uchar ".&nbsp;", 7);
		if (!F) par_format.leftmargin += (int)strlen(cast_const_char n) + c + 2;
		par_format.align = AL_LEFT;
		list_struct(html_top.list_entry.next, struct html_element)->parattr.list_number = par_format.list_number + 1;
		par_format.list_number = 0;
		putsp = -1;
	}
	line_breax = 2;
}

static void html_dl(unsigned char *a)
{
	par_format.flags &= ~P_COMPACT;
	if (has_attr(a, cast_uchar "compact")) par_format.flags |= P_COMPACT;
	if (par_format.list_level) par_format.leftmargin += 5;
	par_format.list_level++;
	par_format.list_number = 0;
	par_format.align = AL_LEFT;
	par_format.dd_margin = par_format.leftmargin;
	html_top.dontkill = 1;
	if (!(par_format.flags & P_COMPACT)) {
		ln_break(2);
		html_top.linebreak = 2;
	}
}

static void html_dt(unsigned char *a)
{
	kill_until(0, cast_uchar "", cast_uchar "DL", NULL);
	par_format.align = AL_LEFT;
	par_format.leftmargin = par_format.dd_margin;
	if (!(par_format.flags & P_COMPACT) && !has_attr(a, cast_uchar "compact"))
		ln_break(2);
}

static void html_dd(unsigned char *a)
{
	kill_until(0, cast_uchar "", cast_uchar "DL", NULL);
	if ((par_format.leftmargin = par_format.dd_margin + (table_level ? 3 : 8)) > par_format.width * 2 / 3 && !table_level)
		par_format.leftmargin = par_format.width * 2 / 3;
	par_format.align = AL_LEFT;
}

static void get_html_form(unsigned char *a, struct form *form)
{
	unsigned char *al;
	unsigned char *ch;
	form->method = FM_GET;
	if ((al = get_attr_val(a, cast_uchar "method"))) {
		if (!casestrcmp(al, cast_uchar "post")) {
			unsigned char *ax;
			form->method = FM_POST;
			if ((ax = get_attr_val(a, cast_uchar "enctype"))) {
				if (!casestrcmp(ax, cast_uchar "multipart/form-data"))
					form->method = FM_POST_MP;
				free(ax);
			}
		}
		free(al);
	}
	if ((al = get_url_val(a, cast_uchar "action"))) {
		unsigned char *all = al;
		while (all[0] == ' ')
			all++;
		while (all[0] && all[strlen(cast_const_char all) - 1] == ' ')
			all[strlen(cast_const_char all) - 1] = 0;
		form->action = join_urls(format_.href_base, all);
		free(al);
	} else {
		if ((ch = cast_uchar strchr(cast_const_char(form->action = stracpy(format_.href_base)), POST_CHAR)))
			*ch = 0;
		if (form->method == FM_GET && (ch = cast_uchar strchr(cast_const_char form->action, '?')))
			*ch = 0;
	}
	if ((al = get_target(a))) {
		form->target = al;
	} else
		form->target = stracpy(format_.target_base);
	if ((al=get_attr_val(a,cast_uchar "name")))
		form->form_name = al;
	if ((al=get_attr_val(a,cast_uchar "onsubmit")))
		form->onsubmit = al;
	form->num = (int)(a - startf);
}

static void find_form_for_input(unsigned char *i)
{
	unsigned char *s, *ss, *name, *attr, *lf, *la;
	int namelen;
	free(form.action);
	free(form.target);
	free(form.form_name);
	free(form.onsubmit);
	memset(&form, 0, sizeof(struct form));
	if (!special_f(ff, SP_USED, NULL)) return;
	if (last_form_tag && last_input_tag && i <= last_input_tag && i > last_form_tag) {
		get_html_form(last_form_attr, &form);
		return;
	}
	if (last_form_tag && last_input_tag && i > last_input_tag) {
		if (parse_element(last_form_tag, i, &name, &namelen, &la, &s))
			internal("couldn't parse already parsed tag");
		lf = last_form_tag;
		s = last_input_tag;
	} else {
		lf = NULL;
		la = NULL;
		s = startf;
	}
	se:
	while (s < i && *s != '<') sp:s++;
	if (s >= i) goto end_parse;
	if (eofff - s >= 2 && (s[1] == '!' || s[1] == '?')) {
		s = skip_comment(s, i);
		goto se;
	}
	ss = s;
	if (parse_element(s, i, &name, &namelen, &attr, &s)) goto sp;
	if (namelen != 4 || casecmp(name, cast_uchar "FORM", 4)) goto se;
	lf = ss;
	la = attr;
	goto se;

	end_parse:
	if (lf) {
		last_form_tag = lf;
		last_form_attr = la;
		last_input_tag = i;
		get_html_form(la, &form);
	} else {
		last_form_tag = NULL;
	}
}

static void html_button(unsigned char *a)
{
	unsigned char *al;
	struct form_control *fc;
	find_form_for_input(a);
	fc = mem_calloc(sizeof(struct form_control));
	if (!(al = get_attr_val(a, cast_uchar "type"))) {
		fc->type = FC_SUBMIT;
		goto xxx;
	}
	if (!casestrcmp(al, cast_uchar "submit"))
		fc->type = FC_SUBMIT;
	else if (!casestrcmp(al, cast_uchar "reset"))
		fc->type = FC_RESET;
	else if (!casestrcmp(al, cast_uchar "button"))
		fc->type = FC_BUTTON;
	else {
		free(al);
		free(fc);
		return;
	}
	free(al);
	xxx:
	fc->form_num = last_form_tag ? (int)(last_form_tag - startf) : 0;
	fc->ctrl_num = last_form_tag ? (int)(a - last_form_tag) : (int)(a - startf);
	fc->position = (int)(a - startf);
	fc->method = form.method;
	fc->action = stracpy(form.action);
	fc->form_name = stracpy(form.form_name);
	fc->onsubmit = stracpy(form.onsubmit);
	fc->name = get_attr_val(a, cast_uchar "name");
	fc->default_value = get_exact_attr_val(a, cast_uchar "value");
	fc->ro = has_attr(a, cast_uchar "disabled") ? 2 : has_attr(a, cast_uchar "readonly") ? 1 : 0;
	if (fc->type == FC_SUBMIT && !fc->default_value)
		fc->default_value = stracpy(cast_uchar "Submit");
	if (fc->type == FC_RESET && !fc->default_value)
		fc->default_value = stracpy(cast_uchar "Reset");
	if (fc->type == FC_BUTTON && !fc->default_value)
		fc->default_value = stracpy(cast_uchar "BUTTON");
	if (!fc->default_value)
		fc->default_value = stracpy(cast_uchar "");
	special_f(ff, SP_CONTROL, fc);
	format_.form = fc;
	format_.attr |= AT_BOLD | AT_FIXED;

	if (fc->type != FC_BUTTON) {
		unsigned char *p, *name;
		int namelen;
		p = find_element_end(a);
p1:
		while (p < eoff && WHITECHAR(*p))
			p2:p++;
		if (p == eoff)
			goto put_text;
		if (*p != '<')
			return;
		if (parse_element(p, eoff, &name, &namelen, NULL, &p))
			goto p2;
		if (namelen == 6 && !casecmp(name, cast_uchar "BUTTON", 6))
			goto put_text;
		if (namelen == 7 && !casecmp(name, cast_uchar "/BUTTON", 7))
			goto put_text;
		if (namelen == 3 && !casecmp(name, cast_uchar "IMG", 3))
			return;
		goto p1;

put_text:
		put_chrs(cast_uchar "[&nbsp;", 7);
		put_chrs(fc->default_value, (int)strlen(cast_const_char fc->default_value));
		put_chrs(cast_uchar "&nbsp;]", 7);
		putsp = -1;
	}
}

static void set_max_textarea_width(int *w)
{
	int limit;
	if (!table_level) {
		limit = par_format.width - (par_format.leftmargin + par_format.rightmargin) * gf_val(1, G_HTML_MARGIN);
	} else {
		limit = gf_val(d_opt->xw - 2, d_opt->xw - G_SCROLL_BAR_WIDTH - 2 * G_HTML_MARGIN * d_opt->margin);
	}
	if (!F) {
		if (*w > limit) {
			*w = limit;
			if (*w < HTML_MINIMAL_TEXTAREA_WIDTH) *w = HTML_MINIMAL_TEXTAREA_WIDTH;
		}
	}
}

static void html_input(unsigned char *a)
{
	int i;
	int size;
	unsigned char *al;
	struct form_control *fc;
	find_form_for_input(a);
	fc = mem_calloc(sizeof(struct form_control));
	if (!(al = get_attr_val(a, cast_uchar "type"))) {
		if (has_attr(a, cast_uchar "onclick")) fc->type = FC_BUTTON;
		else fc->type = FC_TEXT;
		goto xxx;
	}
	if (!casestrcmp(al, cast_uchar "text")) fc->type = FC_TEXT;
	else if (!casestrcmp(al, cast_uchar "password")) fc->type = FC_PASSWORD;
	else if (!casestrcmp(al, cast_uchar "checkbox")) fc->type = FC_CHECKBOX;
	else if (!casestrcmp(al, cast_uchar "radio")) fc->type = FC_RADIO;
	else if (!casestrcmp(al, cast_uchar "submit")) fc->type = FC_SUBMIT;
	else if (!casestrcmp(al, cast_uchar "reset")) fc->type = FC_RESET;
	else if (!casestrcmp(al, cast_uchar "file")) fc->type = FC_FILE_UPLOAD;
	else if (!casestrcmp(al, cast_uchar "hidden")) fc->type = FC_HIDDEN;
	else if (!casestrcmp(al, cast_uchar "image")) fc->type = FC_IMAGE;
	else if (!casestrcmp(al, cast_uchar "button")) fc->type = FC_BUTTON;
	else fc->type = FC_TEXT;
	free(al);
	xxx:
	fc->form_num = last_form_tag ? (int)(last_form_tag - startf) : 0;
	fc->ctrl_num = last_form_tag ? (int)(a - last_form_tag) : (int)(a - startf);
	fc->position = (int)(a - startf);
	fc->method = form.method;
	fc->action = stracpy(form.action);
	fc->form_name = stracpy(form.form_name);
	fc->onsubmit = stracpy(form.onsubmit);
	fc->target = stracpy(form.target);
	fc->name = get_attr_val(a, cast_uchar "name");
	if (fc->type == FC_TEXT || fc->type == FC_PASSWORD) fc->default_value = get_attr_val(a, cast_uchar "value");
	else if (fc->type != FC_FILE_UPLOAD) fc->default_value = get_exact_attr_val(a, cast_uchar "value");
	if (fc->type == FC_CHECKBOX && !fc->default_value) fc->default_value = stracpy(cast_uchar "on");
	if ((size = get_num(a, cast_uchar "size")) <= 1) size = HTML_DEFAULT_INPUT_SIZE;
	size++;
	if (size > HTML_MINIMAL_TEXTAREA_WIDTH) {
		set_max_textarea_width(&size);
	}
	fc->size = size;
	if ((fc->maxlength = get_num(a, cast_uchar "maxlength")) == -1) fc->maxlength = INT_MAX / 4;
	if (fc->type == FC_CHECKBOX || fc->type == FC_RADIO) fc->default_state = has_attr(a, cast_uchar "checked");
	fc->ro = has_attr(a, cast_uchar "disabled") ? 2 : has_attr(a, cast_uchar "readonly") ? 1 : 0;
	if (fc->type == FC_IMAGE) {
		fc->alt = get_attr_val(a, cast_uchar "alt");
		if (!fc->alt) fc->alt = get_attr_val(a, cast_uchar "title");
		if (!fc->alt) fc->alt = get_attr_val(a, cast_uchar "name");
	}
	if (fc->type == FC_SUBMIT && !fc->default_value) fc->default_value = stracpy(cast_uchar "Submit");
	if (fc->type == FC_RESET && !fc->default_value) fc->default_value = stracpy(cast_uchar "Reset");
	if (!fc->default_value) fc->default_value = stracpy(cast_uchar "");
	if (fc->type == FC_HIDDEN) goto hid;
	put_chrs(cast_uchar " ", 1);
	html_stack_dup();
	format_.form = fc;
	switch (fc->type) {
		case FC_TEXT:
		case FC_PASSWORD:
		case FC_FILE_UPLOAD:
			format_.attr |= AT_BOLD | AT_FIXED;
			format_.fontsize = 3;
			for (i = 0; i < fc->size; i++) put_chrs(cast_uchar "_", 1);
			break;
		case FC_CHECKBOX:
			format_.attr |= AT_BOLD | AT_FIXED;
			format_.fontsize = 3;
			put_chrs(cast_uchar "[&nbsp;]", 8);
			break;
		case FC_RADIO:
			format_.attr |= AT_BOLD | AT_FIXED;
			format_.fontsize = 3;
			put_chrs(cast_uchar "[&nbsp;]", 8);
			break;
		case FC_IMAGE:
			if (!F || !d_opt->display_images) {
				free(format_.image);
				format_.image = NULL;
				if ((al = get_url_val(a, cast_uchar "src")) || (al = get_url_val(a, cast_uchar "dynsrc"))) {
					format_.image = join_urls(format_.href_base, al);
					free(al);
				}
				format_.attr |= AT_BOLD | AT_FIXED;
				put_chrs(cast_uchar "[&nbsp;", 7);
				if (fc->alt) put_chrs(fc->alt, (int)strlen(cast_const_char fc->alt));
				else put_chrs(cast_uchar "Submit", 6);
				put_chrs(cast_uchar "&nbsp;]", 7);
			} else html_img(a);
			break;
		case FC_SUBMIT:
		case FC_RESET:
			format_.attr |= AT_BOLD | AT_FIXED;
			format_.fontsize = 3;
			put_chrs(cast_uchar "[&nbsp;", 7);
			if (fc->default_value) put_chrs(fc->default_value, (int)strlen(cast_const_char fc->default_value));
			put_chrs(cast_uchar "&nbsp;]", 7);
			break;
		case FC_BUTTON:
			format_.attr |= AT_BOLD | AT_FIXED;
			format_.fontsize = 3;
			put_chrs(cast_uchar "[&nbsp;", 7);
			if (fc->default_value) put_chrs(fc->default_value, (int)strlen(cast_const_char fc->default_value));
			else put_chrs(cast_uchar "BUTTON", 6);
			put_chrs(cast_uchar "&nbsp;]", 7);
			break;
		default:
			internal("bad control type");
	}
	kill_html_stack_item(&html_top);
	put_chrs(cast_uchar " ", 1);

	hid:
	special_f(ff, SP_CONTROL, fc);
}

static void html_select(unsigned char *a)
{
	unsigned char *al;
	if (!(al = get_attr_val(a, cast_uchar "name"))) return;
	html_top.dontkill = 1;
	free(format_.select);
	format_.select = al;
	format_.select_disabled = 2 * has_attr(a, cast_uchar "disabled");
}

static void html_option(unsigned char *a)
{
	struct form_control *fc;
	unsigned char *val;
	find_form_for_input(a);
	if (!format_.select) return;
	fc = mem_calloc(sizeof(struct form_control));
	if (!(val = get_exact_attr_val(a, cast_uchar "value"))) {
		unsigned char *p, *r;
		unsigned char *name;
		int namelen;
		int l = 0;
		val = init_str();
		p = find_element_end(a);
		rrrr:
		while (p < eoff && WHITECHAR(*p)) p++;
		while (p < eoff && !WHITECHAR(*p) && *p != '<') {
			pppp:
			add_chr_to_str(&val, &l, *p);
			p++;
		}
		r = p;
		while (r < eoff && WHITECHAR(*r)) r++;
		if (r >= eoff) goto x;
		if (eoff - r >= 2 && (r[1] == '!' || r[1] == '?')) {
			p = skip_comment(r, eoff);
			goto rrrr;
		}
		if (parse_element(r, eoff, &name, &namelen, NULL, &p)) goto pppp;
		if (!((namelen == 6 && !casecmp(name, cast_uchar "OPTION", 6)) ||
		    (namelen == 7 && !casecmp(name, cast_uchar "/OPTION", 7)) ||
		    (namelen == 6 && !casecmp(name, cast_uchar "SELECT", 6)) ||
		    (namelen == 7 && !casecmp(name, cast_uchar "/SELECT", 7)) ||
		    (namelen == 8 && !casecmp(name, cast_uchar "OPTGROUP", 8)) ||
		    (namelen == 9 && !casecmp(name, cast_uchar "/OPTGROUP", 9)))) goto rrrr;
	}
	x:
	fc->form_num = last_form_tag ? (int)(last_form_tag - startf) : 0;
	fc->ctrl_num = last_form_tag ? (int)(a - last_form_tag) : (int)(a - startf);
	fc->position = (int)(a - startf);
	fc->method = form.method;
	fc->action = stracpy(form.action);
	fc->form_name = stracpy(form.form_name);
	fc->onsubmit = stracpy(form.onsubmit);
	fc->type = FC_CHECKBOX;
	fc->name = stracpy(format_.select);
	fc->default_value = val;
	fc->default_state = has_attr(a, cast_uchar "selected");
	fc->ro = format_.select_disabled;
	if (has_attr(a, cast_uchar "disabled")) fc->ro = 2;
	put_chrs(cast_uchar " ", 1);
	html_stack_dup();
	format_.form = fc;
	format_.attr |= AT_BOLD | AT_FIXED;
	format_.fontsize = 3;
	put_chrs(cast_uchar "[ ]", 3);
	kill_html_stack_item(&html_top);
	put_chrs(cast_uchar " ", 1);
	special_f(ff, SP_CONTROL, fc);
}

void clr_white(unsigned char *name)
{
	unsigned char *nm;
	for (nm = name; *nm; nm++)
		if (WHITECHAR(*nm) || *nm == 1) *nm = ' ';
}

void clr_spaces(unsigned char *name, int firstlast)
{
	unsigned char *n1, *n2;
	clr_white(name);
	if (!strchr(cast_const_char name, ' ')) return;
	for (n1 = name, n2 = name; *n1; n1++)
		if (!(n1[0] == ' ' && ((firstlast && n2 == name) || n1[1] == ' ' || (firstlast && !n1[1]))))
			*n2++ = *n1;
	*n2 = 0;
}

static int menu_stack_size;
static struct menu_item **menu_stack;

static void new_menu_item(unsigned char *name, long data, int fullname)
	/* name == NULL - up;	data == -1 - down */
{
	struct menu_item *top, *item, *nmenu = NULL; /* no uninitialized warnings */
	if (name) {
		clr_spaces(name, 1);
		if (!name[0]) {
			free(name);
			name = stracpy(cast_uchar " ");
		}
		if (name[0] == 1) name[0] = ' ';
	}
	if (name && data == -1) {
		nmenu = mem_calloc(sizeof(struct menu_item));
		/*nmenu->text = cast_uchar "";*/
	}
	if (menu_stack_size && name) {
		top = item = menu_stack[menu_stack_size - 1];
		while (item->text) item++;
		if ((size_t)((unsigned char *)(item + 2) - (unsigned char *)top) > INT_MAX)
			overalloc();
		top = xrealloc(top, (unsigned char *)(item + 2)
				- (unsigned char *)top);
		item = item - menu_stack[menu_stack_size - 1] + top;
		menu_stack[menu_stack_size - 1] = top;
		if (menu_stack_size >= 2) {
			struct menu_item *below = menu_stack[menu_stack_size - 2];
			while (below->text) below++;
			below[-1].data = top;
		}
		item->text = name;
		item->rtext = data == -1 ? cast_uchar ">" : cast_uchar "";
		item->hotkey = fullname ? cast_uchar "\000\001" : cast_uchar "\000\000"; /* dirty */
		item->func = data == -1 ? do_select_submenu : selected_item;
		item->data = data == -1 ? nmenu : (void *)data;
		item->in_m = data == -1 ? 1 : 0;
		item->free_i = 0;
		item++;
		memset(item, 0, sizeof(struct menu_item));
		/*item->text = cast_uchar "";*/
	} else
		free(name);
	if (name && data == -1) {
		if ((unsigned)menu_stack_size > INT_MAX / sizeof(struct menu_item *) - 1)
			overalloc();
		menu_stack = xrealloc(menu_stack, (menu_stack_size + 1) * sizeof(struct menu_item *));
		menu_stack[menu_stack_size++] = nmenu;
	}
	if (!name) menu_stack_size--;
}

static void init_menu(void)
{
	menu_stack_size = 0;
	menu_stack = NULL;
	new_menu_item(stracpy(cast_uchar ""), -1, 0);
}

void free_menu(struct menu_item *m) /* Grrr. Recursion */
{
	struct menu_item *mm;
	for (mm = m; mm->text; mm++) {
		free(mm->text);
		if (mm->func == do_select_submenu)
			free_menu(mm->data);
	}
	free(m);
}

static struct menu_item *detach_menu(void)
{
	struct menu_item *i = NULL;
	if (menu_stack) {
		if (menu_stack_size) i = menu_stack[0];
		free(menu_stack);
	}
	return i;
}

static void destroy_menu(void)
{
	if (menu_stack)
		free_menu(menu_stack[0]);
	detach_menu();
}

static void menu_labels(struct menu_item *m, unsigned char *base, unsigned char **lbls)
{
	unsigned char *bs;
	for (; m->text; m++) {
		if (m->func == do_select_submenu) {
			if ((bs = stracpy(base))) {
				add_to_strn(&bs, m->text);
				add_to_strn(&bs, cast_uchar " ");
				menu_labels(m->data, bs, lbls);
				free(bs);
			}
		} else {
			if ((bs = stracpy(m->hotkey[1] ? (unsigned char *)"" : base))) add_to_strn(&bs, m->text);
			lbls[(int)(long)m->data] = bs;
		}
	}
}

static int menu_contains(struct menu_item *m, int f)
{
	if (m->func != do_select_submenu)
		return (int)(long)m->data == f;
	for (m = m->data; m->text; m++)
		if (menu_contains(m, f))
			return 1;
	return 0;
}

void do_select_submenu(struct terminal *term, void *menu_, void *ses_)
{
	struct menu_item *menu = (struct menu_item *)menu_;
	struct session *ses = (struct session *)ses_;
	struct menu_item *m;
	int def = get_current_state(ses);
	int sel = 0;
	if (def < 0) def = 0;
	for (m = menu; m->text; m++, sel++) if (menu_contains(m, def)) goto f;
	sel = 0;
	f:
	do_menu_selected(term, menu, ses, sel, NULL, NULL);
}

static int do_html_select(unsigned char *attr, unsigned char *html, unsigned char *eof, unsigned char **end)
{
	struct form_control *fc;
	unsigned char *t_name, *t_attr, *en;
	int t_namelen;
	unsigned char *lbl;
	int lbl_l;
	unsigned char *vlbl;
	int vlbl_l;
	int nnmi = 0;
	struct conv_table *ct = special_f(ff, SP_TABLE, NULL);
	unsigned char **val, **lbls;
	int order, preselect, group;
	int i, mw;
	if (has_attr(attr, cast_uchar "multiple") || dmp) return 1;
	find_form_for_input(attr);
	lbl = NULL;
	lbl_l = 0;
	vlbl = NULL;
	vlbl_l = 0;
	val = NULL;
	order = 0;
	group = 0;
	preselect = -1;
	init_menu();
	se:
	en = html;
	see:
	html = en;
	while (html < eof && *html != '<') html++;
	if (html >= eof) {
		int i;
		abort:
		*end = html;
		free(lbl);
		free(vlbl);
		for (i = 0; i < order; i++)
			free(val[i]);
		free(val);
		destroy_menu();
		*end = en;
		return 0;
	}
	if (lbl) {
		unsigned char *q, *s = en;
		int l = (int)(html - en);
		while (l && WHITECHAR(s[0])) {
			s++;
			l--;
		}
		while (l && WHITECHAR(s[l-1])) l--;
		q = convert_string(ct, s, l, d_opt);
		if (q) {
			add_to_str(&lbl, &lbl_l, q);
			free(q);
		}
		add_bytes_to_str(&vlbl, &vlbl_l, s, l);
	}
	if (eof - html >= 2 && (html[1] == '!' || html[1] == '?')) {
		html = skip_comment(html, eof);
		goto se;
	}
	if (parse_element(html, eof, &t_name, &t_namelen, &t_attr, &en)) {
		html++;
		goto se;
	}
	if (t_namelen == 7 && !casecmp(t_name, cast_uchar "/SELECT", 7)) {
		if (lbl) {
			if (!val[order - 1]) val[order - 1] = stracpy(vlbl);
			if (!nnmi) {
				new_menu_item(lbl, order - 1, 1);
				lbl = NULL;
			} else {
				free(lbl);
				lbl = NULL;
			}
			free(vlbl);
			vlbl = NULL;
		}
		goto end_parse;
	}
	if (t_namelen == 7 && !casecmp(t_name, cast_uchar "/OPTION", 7)) {
		if (lbl) {
			if (!val[order - 1]) val[order - 1] = stracpy(vlbl);
			if (!nnmi) {
				new_menu_item(lbl, order - 1, 1);
				lbl = NULL;
			} else {
				free(lbl);
				lbl = NULL;
			}
			free(vlbl);
			vlbl = NULL;
		}
		goto see;
	}
	if (t_namelen == 6 && !casecmp(t_name, cast_uchar "OPTION", 6)) {
		unsigned char *v, *vx;
		if (lbl) {
			if (!val[order - 1]) val[order - 1] = stracpy(vlbl);
			if (!nnmi) {
				new_menu_item(lbl, order - 1, 1);
				lbl = NULL;
			} else {
				free(lbl);
				lbl = NULL;
			}
			free(vlbl);
			vlbl = NULL;
		}
		if (has_attr(t_attr, cast_uchar "disabled")) goto see;
		if (preselect == -1 && has_attr(t_attr, cast_uchar "selected")) preselect = order;
		v = get_exact_attr_val(t_attr, cast_uchar "value");
		if (!(order & (ALLOC_GR - 1))) {
			if ((unsigned)order > INT_MAX / sizeof(unsigned char *) - ALLOC_GR)
				overalloc();
			val = xrealloc(val, (order + ALLOC_GR)
					* sizeof(unsigned char *));
		}
		val[order++] = v;
		if ((vx = get_attr_val(t_attr, cast_uchar "label"))) {
			new_menu_item(convert_string(ct, vx, (int)strlen(cast_const_char vx), d_opt), order - 1, 0);
			free(vx);
		}
		if (!v || !vx) {
			lbl = init_str();
			lbl_l = 0;
			vlbl = init_str();
			vlbl_l = 0;
			nnmi = !!vx;
		}
		goto see;
	}
	if ((t_namelen == 8 && !casecmp(t_name, cast_uchar "OPTGROUP", 8)) || (t_namelen == 9 && !casecmp(t_name, cast_uchar "/OPTGROUP", 9))) {
		if (lbl) {
			if (!val[order - 1]) val[order - 1] = stracpy(vlbl);
			if (!nnmi) {
				new_menu_item(lbl, order - 1, 1);
				lbl = NULL;
			} else {
				free(lbl);
				lbl = NULL;
			}
			free(vlbl);
			vlbl = NULL;
		}
		if (group) {
			new_menu_item(NULL, -1, 0);
			group = 0;
		}
	}
	if (t_namelen == 8 && !casecmp(t_name, cast_uchar "OPTGROUP", 8)) {
		unsigned char *la;
		if (!(la = get_attr_val(t_attr, cast_uchar "label"))) la = stracpy(cast_uchar "");
		new_menu_item(convert_string(ct, la, (int)strlen(cast_const_char la), d_opt), -1, 0);
		free(la);
		group = 1;
	}
	goto see;

	end_parse:
	*end = en;
	if (!order) goto abort;
	fc = mem_calloc(sizeof(struct form_control));
	if ((unsigned)order > (unsigned)INT_MAX / sizeof(unsigned char *))
		overalloc();
	lbls = mem_calloc(order * sizeof(unsigned char *));
	fc->form_num = last_form_tag ? (int)(last_form_tag - startf) : 0;
	fc->ctrl_num = last_form_tag ? (int)(attr - last_form_tag) : (int)(attr - startf);
	fc->position = (int)(attr - startf);
	fc->method = form.method;
	fc->action = stracpy(form.action);
	fc->form_name= stracpy(form.form_name);
	fc->onsubmit= stracpy(form.onsubmit);
	fc->name = get_attr_val(attr, cast_uchar "name");
	fc->type = FC_SELECT;
	fc->default_state = preselect < 0 ? 0 : preselect;
	fc->default_value = order ? stracpy(val[fc->default_state]) : stracpy(cast_uchar "");
	fc->ro = has_attr(attr, cast_uchar "disabled") ? 2 : has_attr(attr, cast_uchar "readonly") ? 1 : 0;
	fc->nvalues = order;
	fc->values = val;
	fc->menu = detach_menu();
	fc->labels = lbls;
	menu_labels(fc->menu, cast_uchar "", lbls);
	html_stack_dup();
	format_.attr |= AT_FIXED;
	format_.fontsize = 3;
	put_chrs(cast_uchar "[", 1);
	html_stack_dup();
	format_.form = fc;
	format_.attr |= AT_BOLD | AT_FIXED;
	format_.fontsize = 3;
	mw = 0;
	for (i = 0; i < order; i++) if (lbls[i] && strlen((char *)lbls[i]) > mw) mw = strlen((char *)lbls[i]);
	for (i = 0; i < mw; i++) put_chrs(cast_uchar "_", 1);
	kill_html_stack_item(&html_top);
	put_chrs(cast_uchar "]", 1);
	kill_html_stack_item(&html_top);
	special_f(ff, SP_CONTROL, fc);
	return 0;
}

static void html_textarea(unsigned char *a)
{
	internal("This should be never called");
}

static void do_html_textarea(unsigned char *attr, unsigned char *html, unsigned char *eof, unsigned char **end)
{
	struct form_control *fc;
	unsigned char *p, *t_name, *w;
	int t_namelen;
	int cols, rows;
	int i;
	find_form_for_input(attr);
	while (html < eof && (*html == '\n' || *html == '\r')) html++;
	p = html;
	while (p < eof && *p != '<') {
		pp:
		p++;
	}
	if (p >= eof) {
		*end = eof;
		return;
	}
	if (parse_element(p, eof, &t_name, &t_namelen, NULL, end)) goto pp;
	if (t_namelen != 9 || casecmp(t_name, cast_uchar "/TEXTAREA", 9)) goto pp;
	fc = mem_calloc(sizeof(struct form_control));
	fc->form_num = last_form_tag ? (int)(last_form_tag - startf) : 0;
	fc->ctrl_num = last_form_tag ? (int)(attr - last_form_tag) : (int)(attr - startf);
	fc->position = (int)(attr - startf);
	fc->method = form.method;
	fc->action = stracpy(form.action);
	fc->form_name = stracpy(form.form_name);
	fc->onsubmit = stracpy(form.onsubmit);
	fc->name = get_attr_val(attr, cast_uchar "name");
	fc->type = FC_TEXTAREA;;
	fc->ro = has_attr(attr, cast_uchar "disabled") ? 2 : has_attr(attr, cast_uchar "readonly") ? 1 : 0;
	fc->default_value = memacpy(html, p - html);
	if ((cols = get_num(attr, cast_uchar "cols")) < HTML_MINIMAL_TEXTAREA_WIDTH) cols = HTML_DEFAULT_TEXTAREA_WIDTH;
	cols++;
	set_max_textarea_width(&cols);
	if ((rows = get_num(attr, cast_uchar "rows")) <= 0) rows = HTML_DEFAULT_TEXTAREA_HEIGHT;
	if (!F) {
		if (rows > d_opt->yw) {
			rows = d_opt->yw;
			if (rows <= 0) rows = 1;
		}
	}
	fc->cols = cols;
	fc->rows = rows;
	fc->wrap = 1;
	if ((w = get_attr_val(attr, cast_uchar "wrap"))) {
		if (!casestrcmp(w, cast_uchar "hard") || !casestrcmp(w, cast_uchar "physical")) fc->wrap = 2;
		else if (!casestrcmp(w, cast_uchar "off")) fc->wrap = 0;
		free(w);
	}
	if ((fc->maxlength = get_num(attr, cast_uchar "maxlength")) == -1) fc->maxlength = INT_MAX / 4;
	if (rows > 1) ln_break(1);
	else put_chrs(cast_uchar " ", 1);
	html_stack_dup();
	format_.form = fc;
	format_.attr = AT_BOLD | AT_FIXED;
	format_.fontsize = 3;
	for (i = 0; i < rows; i++) {
		int j;
		for (j = 0; j < cols; j++) put_chrs(cast_uchar "_", 1);
		if (i < rows - 1) ln_break(1);
	}
	kill_html_stack_item(&html_top);
	if (rows > 1) ln_break(1);
	else put_chrs(cast_uchar " ", 1);
	special_f(ff, SP_CONTROL, fc);
}

static void html_iframe(unsigned char *a)
{
	unsigned char *name, *url;
	if (!(url = get_url_val(a, cast_uchar "src"))) return;
	if (!*url) goto free_url_ret;
	if (!(name = get_attr_val(a, cast_uchar "name"))) name = stracpy(cast_uchar "");
	if (*name) put_link_line(cast_uchar "IFrame: ", name, url, d_opt->framename);
	else put_link_line(cast_uchar "", cast_uchar "IFrame", url, d_opt->framename);
	free(name);
	free_url_ret:
	free(url);
}

static void html_noframes(unsigned char *a)
{
	if (d_opt->frames) html_skip(a);
}

static void html_frame(unsigned char *a)
{
	unsigned char *name, *u2, *url;
	if (!(u2 = get_url_val(a, cast_uchar "src"))) {
		url = stracpy(cast_uchar "");
	} else {
		url = join_urls(format_.href_base, u2);
		free(u2);
	}
	name = get_attr_val(a, cast_uchar "name");
	if (!name[0]) {
		free(name);
		name = NULL;
	}
	if (!name) {
		name = get_attr_val(a, cast_uchar "src");
		if (!name)
			name = stracpy(cast_uchar "Frame");
	}
	if (!d_opt->frames || !html_top.frameset) put_link_line(cast_uchar "Frame: ", name, url, cast_uchar "");
	else {
		struct frame_param fp;
		unsigned char *scroll = get_attr_val(a, cast_uchar "scrolling");
		fp.name = name;
		fp.url = url;
		fp.parent = html_top.frameset;
		fp.marginwidth = get_num(a, cast_uchar "marginwidth");
		fp.marginheight = get_num(a, cast_uchar "marginheight");
		fp.scrolling = SCROLLING_AUTO;
		if (scroll) {
			if (!casestrcmp(scroll, cast_uchar "no"))
				fp.scrolling = SCROLLING_NO;
			else if (!casestrcmp(scroll, cast_uchar "yes"))
				fp.scrolling = SCROLLING_YES;
			free(scroll);
		}
		if (special_f(ff, SP_USED, NULL)) special_f(ff, SP_FRAME, &fp);
	}
	free(name);
	free(url);
}

static void parse_frame_widths(unsigned char *a, int ww, int www, int **op, int *olp)
{
	unsigned char *aa;
	char *end;
	int q, qq, i, d, nn;
	unsigned long n;
	int *oo, *o;
	int ol;
	ol = 0;
	o = NULL;
	new_ch:
	while (WHITECHAR(*a)) a++;
	n = strtoul(cast_const_char a, &end, 10);
	a = cast_uchar end;
	if (n > 10000) n = 10000;
	q = (int)n;
	if (*a == '%') q = q * ww / 100;
	else if (*a != '*') q = (q + (www - 1) / 2) / (www ? www : 1);
	else if (!(q = -q)) q = -1;
	if ((unsigned)ol > INT_MAX / sizeof(int) - 1)
		overalloc();
	o = xrealloc(o, (ol + 1) * sizeof(int));
	o[ol++] = q;
	if ((aa = cast_uchar strchr(cast_const_char a, ','))) {
		a = aa + 1;
		goto new_ch;
	}
	*op = o;
	*olp = ol;
	q = gf_val(2 * ol - 1, ol);
	for (i = 0; i < ol; i++) if (o[i] > 0) q += o[i] - 1;
	if (q >= ww) {
		distribute:
		for (i = 0; i < ol; i++) if (o[i] < 1) o[i] = 1;
		q -= ww;
		d = 0;
		for (i = 0; i < ol; i++) d += o[i];
		qq = q;
		for (i = 0; i < ol; i++) {
			q -= o[i] - o[i] * (d - qq) / (d ? d : 1);
			o[i] = o[i] * (d - qq) / (d ? d : 1);
		}
		while (q) {
			nn = 0;
			for (i = 0; i < ol; i++) {
				if (q < 0) {
					o[i]++;
					q++;
					nn = 1;
				}
				if (q > 0 && o[i] > 1) {
					o[i]--;
					q--;
					nn = 1;
				}
				if (!q) break;
			}
			if (!nn) break;
		}
	} else {
		int nn = 0;
		for (i = 0; i < ol; i++) if (o[i] < 0) nn = 1;
		if (!nn) goto distribute;
		if ((unsigned)ol > INT_MAX / sizeof(int))
			overalloc();
		oo = xmalloc(ol * sizeof(int));
		memcpy(oo, o, ol * sizeof(int));
		for (i = 0; i < ol; i++) if (o[i] < 1) o[i] = 1;
		q = ww - q;
		d = 0;
		for (i = 0; i < ol; i++) if (oo[i] < 0) d += -oo[i];
		qq = q;
		for (i = 0; i < ol; i++) if (oo[i] < 0) {
			o[i] += (-oo[i] * qq / (d ? d : 1));
			q -= (-oo[i] * qq / (d ? d : 1));
		}
		if (q < 0) {
			q = 0;
			/*internal("parse_frame_widths: q < 0"); may happen when page contains too big values */
		}
		for (i = 0; i < ol; i++) if (oo[i] < 0) {
			if (q) {
				o[i]++;
				q--;
			}
		}
		free(oo);
	}
	for (i = 0; i < ol; i++) if (!o[i]) {
		int j;
		int m = 0;
		int mj = 0;
		for (j = 0; j < ol; j++)
			if (o[j] > m) {
				m = o[j];
				mj = j;
			}
		if (m) {
			o[i] = 1;
			o[mj]--;
		}
	}
}

static void html_frameset(unsigned char *a)
{
	int x = 0, y = 0;	/* against warning */
	struct frameset_param fp;
	unsigned char *c, *d;
	if (!d_opt->frames || !special_f(ff, SP_USED, NULL)) return;
	if (!(c = get_attr_val(a, cast_uchar "cols"))) c = stracpy(cast_uchar "100%");
	if (!(d = get_attr_val(a, cast_uchar "rows"))) d = stracpy(cast_uchar "100%");
	if (!html_top.frameset) {
		x = d_opt->xw;
		y = d_opt->yw;
	} else {
		struct frameset_desc *f = html_top.frameset;
		if (f->yp >= f->y) goto free_cd;
		x = f->f[f->xp + f->yp * f->x].xw;
		y = f->f[f->xp + f->yp * f->x].yw;
	}
	parse_frame_widths(c, x, gf_val(HTML_FRAME_CHAR_WIDTH, 1), &fp.xw, &fp.x);
	parse_frame_widths(d, y, gf_val(HTML_FRAME_CHAR_HEIGHT, 1), &fp.yw, &fp.y);
	fp.parent = html_top.frameset;
	if (fp.x && fp.y) {
		html_top.frameset = special_f(ff, SP_FRAMESET, &fp);
	}
	free(fp.xw);
	free(fp.yw);
	free_cd:
	free(c);
	free(d);
}

/*static void html_frameset(unsigned char *a)
{
	int w;
	int horiz = 0;
	struct frameset_param *fp;
	unsigned char *c, *d;
	if (!d_opt->frames || !special_f(ff, SP_USED, NULL)) return;
	if (!(c = get_attr_val(a, cast_uchar "cols"))) {
		horiz = 1;
		if (!(c = get_attr_val(a, cast_uchar "rows"))) return;
	}
	fp = xmalloc(sizeof(struct frameset_param));
	fp->n = 0;
	fp->horiz = horiz;
	par_format.leftmargin = par_format.rightmargin = 0;
	d = c;
	while (1) {
		while (WHITECHAR(*d)) d++;
		if (!*d) break;
		if (*d == ',') {
			d++;
			continue;
		}
		if ((w = parse_width(d, 1)) != -1) {
			if ((unsigned)fp->n > (INT_MAX - sizeof(struct frameset_param)) / sizeof(int) - 1)
				overalloc();
			fp = xrealloc(fp, sizeof(struct frameset_param)
					+ (fp->n + 1) * sizeof(int));
			fp->width[fp->n++] = w;
		}
		if (!(d = cast_uchar strchr(cast_const_char d, ','))) break;
		d++;
	}
	fp->parent = html_top.frameset;
	if (fp->n) html_top.frameset = special_f(ff, SP_FRAMESET, fp);
	free(fp);
	f:
	free(c);
}*/

static void html_meta(unsigned char *a)
{
	unsigned char *prop;
	if ((prop = get_attr_val(a, cast_uchar "property"))) {
		if (!strcmp(cast_const_char prop, "og:image")) {
			unsigned char *host = get_host_name(format_.href_base);
			if (host) {
				if (strstr(cast_const_char host, "facebook.")
				|| strstr(cast_const_char host, "flickr.")
				|| strstr(cast_const_char host, "imgur.")
				|| strstr(cast_const_char host, "instagram.")
				|| strstr(cast_const_char host, "mastadon.")
				|| strstr(cast_const_char host, "pinterest.")
				|| strstr(cast_const_char host, "twitter."))
					html_img(a);
				free(host);
			}
		}
		free(prop);
	}
}

static void html_link(unsigned char *a)
{
	unsigned char *name, *url, *title;
	if ((name = get_attr_val(a, cast_uchar "type"))) {
		if (casestrcmp(name, cast_uchar "text/html")) {
			free(name);
			return;
		}
		free(name);
	}
	if (!(url = get_url_val(a, cast_uchar "href"))) return;
	if (!(name = get_attr_val(a, cast_uchar "rel")))
		if (!(name = get_attr_val(a, cast_uchar "rev")))
			name = get_attr_val(a, cast_uchar "ref");
	if (name) {
		unsigned char *lang;
		if ((lang = get_attr_val(a, cast_uchar "hreflang"))) {
			add_to_strn(&name, cast_uchar " ");
			add_to_strn(&name, lang);
			free(lang);
		}
	}
	if (!name) {
		if (has_attr(a, cast_uchar "title"))
			name = stracpy(cast_uchar "");
		else {
			name = get_attr_val(a, cast_uchar "href");
			if (!name)
				name = stracpy(cast_uchar "Link");
		}
	}
	if (
	    !casecmp(name, cast_uchar "schema", 6) ||
	    !casecmp(name, cast_uchar "mw-", 3) ||
	    !casestrcmp(name, cast_uchar "Edit-Time-Data") ||
	    !casestrcmp(name, cast_uchar "File-List") ||
	    !casestrcmp(name, cast_uchar "alternate stylesheet") ||
	    !casestrcmp(name, cast_uchar "generator-home") ||
	    !casestrcmp(name, cast_uchar "https://api.w.org/") ||
	    !casestrcmp(name, cast_uchar "https://github.com/WP-API/WP-API") ||
	    !casestrcmp(name, cast_uchar "made") ||
	    !casestrcmp(name, cast_uchar "manifest") ||
	    !casestrcmp(name, cast_uchar "meta") ||
	    !casestrcmp(name, cast_uchar "pingback") ||
	    !casestrcmp(name, cast_uchar "preconnect") ||
	    !casestrcmp(name, cast_uchar "stylesheet") ||
	    casestrstr(name, cast_uchar "icon") ||
	    0) goto skip;
	if (!casestrcmp(name, cast_uchar "prefetch") ||
	    !casestrcmp(name, cast_uchar "prerender") ||
	    !casestrcmp(name, cast_uchar "preload")) {
		unsigned char *pre_url = join_urls(format_.href_base, url);
		if (!dmp) load_url(pre_url, format_.href_base, NULL, PRI_PRELOAD, NC_ALWAYS_CACHE, 0, 0, 0);
		free(pre_url);
		goto skip;
	}
	if (!casestrcmp(name, cast_uchar "dns-prefetch")) {
		unsigned char *pre_url, *host;
		if (dmp || *proxies.socks_proxy || proxies.only_proxies)
			goto skip;
		pre_url = join_urls(format_.href_base, url);
		if (get_proxy_string(pre_url) || is_noproxy_url(pre_url)) {
			free(pre_url);
			goto skip;
		}
		host = get_host_name(pre_url);
		free(pre_url);
		free(host);
		goto skip;
	}
	if ((title = get_attr_val(a, cast_uchar "title"))) {
		if (*name) add_to_strn(&name, cast_uchar ": ");
		add_to_strn(&name, title);
		free(title);
	}
	put_link_line(cast_uchar "Link: ", name, url, format_.target_base);
	skip:
	free(name);
	free(url);
}

struct element_info {
	char *name;
	void (*func)(unsigned char *);
	int linebreak;
	int nopair; /* Somehow relates to paired elements */
};

static struct element_info elements[] = {
	{"SPAN",	html_span,	0, 0},
	{"B",		html_bold,	0, 0},
	{"STRONG",	html_bold,	0, 0},
	{"DFN",		html_bold,	0, 0},
	{"I",		html_italic,	0, 0},
	{"Q",		html_italic,	0, 0},
	{"CITE",	html_italic,	0, 0},
	{"EM",		html_italic,	0, 0},
	{"ABBR",	html_italic,	0, 0},
	{"U",		html_underline,	0, 0},
	{"S",		html_underline,	0, 0},
	{"STRIKE",	html_underline,	0, 0},
	{"FIXED",	html_fixed,	0, 0},
	{"CODE",	html_fixed,	0, 0},
	{"TT",		html_fixed,	0, 0},
	{"SAMP",	html_fixed,	0, 0},
	{"SUB",		html_sub,	0, 0},
	{"SUP",		html_sup,	0, 0},
	{"FONT",	html_font,	0, 0},
	{"INVERT",	html_invert,	0, 0},
	{"A",		html_a,		0, 2},
	{"IMG",		html_img,	0, 1},
	{"IMAGE",	html_img,	0, 1},
	{"OBJECT",	html_object,	0, 0},
	{"EMBED",	html_embed,	0, 1},

	{"BASE",	html_base,	0, 1},
	{"BASEFONT",	html_font,	0, 1},

	{"BODY",	html_body,	0, 0},

/*	{"HEAD",	html_skip,	0, 0},*/
	{"TITLE",	html_title,	0, 0},
	{"SCRIPT",	html_script,	0, 0},
	{"STYLE",	html_style,	0, 0},
	{"NOEMBED",	html_skip,	0, 0},

	{"BR",		html_br,	1, 1},
	{"DIV",		html_div,	1, 0},
	{"CENTER",	html_center,	1, 0},
	{"CAPTION",	html_center,	1, 0},
	{"P",		html_p,		2, 2},
	{"HR",		html_hr,	2, 1},
	{"H1",		html_h1,	2, 2},
	{"H2",		html_h2,	2, 2},
	{"H3",		html_h3,	2, 2},
	{"H4",		html_h4,	2, 2},
	{"H5",		html_h5,	2, 2},
	{"H6",		html_h6,	2, 2},
	{"BLOCKQUOTE",	html_blockquote,2, 0},
	{"ADDRESS",	html_address,	2, 0},
	{"PRE",		html_pre,	2, 0},
	{"LISTING",	html_pre,	2, 0},

	{"UL",		html_ul,	1, 0},
	{"DIR",		html_ul,	1, 0},
	{"MENU",	html_ul,	1, 0},
	{"OL",		html_ol,	1, 0},
	{"LI",		html_li,	1, 3},
	{"DL",		html_dl,	1, 0},
	{"DT",		html_dt,	1, 1},
	{"DD",		html_dd,	1, 1},

	{"TABLE",	html_table,	2, 0},
	{"TR",		html_tr,	1, 0},
	{"TD",		html_td,	0, 0},
	{"TH",		html_th,	0, 0},

	{"FORM",	html_form,	1, 0},
	{"INPUT",	html_input,	0, 1},
	{"TEXTAREA",	html_textarea,	0, 1},
	{"SELECT",	html_select,	0, 0},
	{"OPTION",	html_option,	1, 1},
	{"BUTTON",	html_button,	0, 0},

	{"META",	html_meta,	0, 1},
	{"LINK",	html_link,	0, 1},
	{"IFRAME",	html_iframe,	1, 1},
	{"FRAME",	html_frame,	1, 1},
	{"FRAMESET",	html_frameset,	1, 0},
	{"NOFRAMES",	html_noframes,	0, 0},
};

unsigned char *skip_comment(unsigned char *html, unsigned char *eof)
{
	int comm = eof - html >= 4 && html[2] == '-' && html[3] == '-';
	html += comm ? 4 : 2;
	while (html < eof) {
		if (!comm && html[0] == '>') return html + 1;
		if (comm && eof - html >= 2 && html[0] == '-' && html[1] == '-') {
			html += 2;
			while (html < eof && (*html == '-' || *html == '!')) html++;
			while (html < eof && WHITECHAR(*html)) html++;
			if (html >= eof) return eof;
			if (*html == '>') return html + 1;
			continue;
		}
		html++;
	}
	return eof;
}

static void process_head(unsigned char *head)
{
	unsigned char *r, *p;
	struct refresh_param rp;
	if ((r = parse_http_header(head, cast_uchar "Refresh", NULL))) {
		if (!d_opt->auto_refresh) {
			if ((p = parse_header_param(r, cast_uchar "URL", 0)) || (p = parse_header_param(r, cast_uchar "", 0))) {
				put_link_line(cast_uchar "Refresh: ", p, p, d_opt->framename);
				free(p);
			}
		} else {
			rp.url = parse_header_param(r, cast_uchar "URL", 0);
			if (!rp.url) rp.url = parse_header_param(r, cast_uchar "", 0);
			rp.time = atoi(cast_const_char r);
			if (rp.time < 1) rp.time = 1;
			special_f(ff, SP_REFRESH, &rp);
			free(rp.url);
		}
		free(r);
	}
}

static int qd(unsigned char *html, unsigned char *eof, int *len)
{
	int l;
	*len = 1;
	if (html >= eof) {
		internal("qd: out of data, html == %p, eof == %p", html, eof);
		return -1;
	}
	if (html[0] != '&' || d_opt->plain & 1) return html[0];
	if (eof - html >= 5 && !memcmp(html + 1, "Tab;", 4)) {
		*len = 5;
		return 9;
	}
	if (eof - html <= 1) return -1;
	if (html[1] != '#') return -1;
	for (l = 2; l < 10 && eof - html > l; l++) if (html[l] == ';') {
		int n = get_entity_number(html + 2, l - 2);
		if (n >= 0) {
			*len = l + 1;
			return n;
		}
		break;
	}
	return -1;
}

void parse_html(unsigned char *html, unsigned char *eof, void (*put_chars)(void *, unsigned char *, int), void (*line_break)(void *), void *(*special)(void *, int, ...), void *f, unsigned char *head)
{
	unsigned char *lt;

	html_format_changed = 1;
	putsp = -1;
	line_breax = table_level ? 2 : 1;
	pos = 0;
	was_br = 0;

#define set_globals			\
do {					\
	put_chars_f = put_chars;	\
	line_break_f = line_break;	\
	special_f = special;		\
	ff = f;				\
	eoff = eof;			\
} while (0)

	set_globals;

	if (head) process_head(head);

	set_lt:

	/*set_globals;*/

	lt = html;
	while (html < eof) {
		unsigned char *name, *attr, *end;
		unsigned char *a;
		int namelen;
		struct element_info *ei;
		int inv;
		if (par_format.align != AL_NO && par_format.align != AL_NO_BREAKABLE && WHITECHAR(*html)) {
			unsigned char *h = html;
			/*if (putsp == -1) {
				while (html < eof && WHITECHAR(*html)) html++;
				goto set_lt;
			}
			putsp = 0;*/
			while (h < eof && WHITECHAR(*h)) h++;
			if (eof - h > 1 && h[0] == '<' && h[1] == '/') {
				if (!parse_element(h, eof, &name, &namelen, &attr, &end)) {
					put_chrs(lt, (int)(html - lt));
					lt = html = h;
					if (!html_top.invisible) putsp = 1;
					goto element;
				}
			}
			html++;
			if (!(pos + (html-lt-1))) goto skip_w; /* ??? */
			if (*(html - 1) == ' ') {
				if (html < eof && !WHITECHAR(*html)) continue;	/* BIG performance win; not sure if it doesn't cause any bug */
				put_chrs(lt, (int)(html - lt));
			} else {
				put_chrs(lt, (int)(html - 1 - lt));
				put_chrs(cast_uchar " ", 1);
			}
			skip_w:
			while (html < eof && WHITECHAR(*html)) html++;
			/*putsp = -1;*/
			goto set_lt;
		}
		if (0) {
			put_sp:
			put_chrs(cast_uchar " ", 1);
			/*putsp = -1;*/
		}
		if ((par_format.align == AL_NO || par_format.align == AL_NO_BREAKABLE) && (*html < 32 || *html == '&')) {
			int l;
			int q = qd(html, eof, &l);
			putsp = 0;
			if (q == 9) {
				put_chrs(lt, (int)(html - lt));
				put_chrs(cast_uchar "        ", 8 - pos % 8);
				html += l;
				goto set_lt;
			} else if (q == 13 || q == 10) {
				put_chrs(lt, (int)(html - lt));
				next_break:
				html += l;
				if (q == 13 && eof - html > 1 && qd(html, eof, &l) == 10) html += l;
				ln_break(1);
				if (html >= eof) goto set_lt;
				q = qd(html, eof, &l);
				if (q == 13 || q == 10) {
					line_breax = 0;
					goto next_break;
				}
				goto set_lt;
			}
		}
		if (*html < ' ') {
			int xl;
			put_chrs(lt, (int)(html - lt));
			xl = 1;
			while (xl < 240 && eof - html > xl + 1 && html[xl + 1] < ' ' && html[xl + 1] != 9 && html[xl + 1] != 10 && html[xl + 1] != 13) xl++;
			put_chrs(cast_uchar "................................................................................................................................................................................................................................................", xl);
			html += xl;
			goto set_lt;
		}
		if (eof - html >= 2 && html[0] == '<' && (html[1] == '!' || html[1] == '?') && !(d_opt->plain & 1) && html_top.invisible != INVISIBLE_STYLE) {
			/*if (putsp == 1) goto put_sp;
			putsp = 0;*/
			put_chrs(lt, (int)(html - lt));
			html = skip_comment(html, eof);
			goto set_lt;
		}
		if (*html != '<' || d_opt->plain & 1 || parse_element(html, eof, &name, &namelen, &attr, &end)) {
			/*if (putsp == 1) goto put_sp;
			putsp = 0;*/
			html++;
			continue;
		}
		element:
		html_format_changed = 1;
		inv = *name == '/'; name += inv; namelen -= inv;
		if (html_top.invisible == INVISIBLE_SCRIPT && !(inv && namelen == 6 && !casecmp(name, cast_uchar "SCRIPT", 6))) {
			html++;
			continue;
		}
		if (html_top.invisible == INVISIBLE_STYLE && !(inv && namelen == 5 && !casecmp(name, cast_uchar "STYLE", 5))) {
			html++;
			continue;
		}
		if (!inv && putsp == 1 && !html_top.invisible) goto put_sp;
		put_chrs(lt, (int)(html - lt));
		if (par_format.align != AL_NO && par_format.align != AL_NO_BREAKABLE) if (!inv && !putsp) {
			unsigned char *ee = end;
			unsigned char *nm;
			while (!parse_element(ee, eof, &nm, NULL, NULL, &ee))
				if (*nm == '/') goto ng;
			if (ee < eof && WHITECHAR(*ee)) {
				/*putsp = -1;*/
				put_chrs(cast_uchar " ", 1);
			}
			ng:;
		}
		html = end;
		for (ei = elements; ei != endof(elements); ei++) {
			if (strlen(cast_const_char ei->name) != (size_t)namelen || casecmp(cast_uchar ei->name, name, namelen))
				continue;
			if (ei - elements > 4) {
				struct element_info e = *ei;
				memmove(elements + 1, elements, (ei - elements) * sizeof(struct element_info));
				elements[0] = e;
				ei = &elements[0];
			}
			if (!inv) {
				int display_none = 0;
				int noskip = 0;
	/* treat <br> literally in <pre> (fixes source code viewer on github) */
				if ((par_format.align == AL_NO || par_format.align == AL_NO_BREAKABLE) && !casestrcmp(cast_uchar ei->name, cast_uchar "BR"))
					line_breax = 0;
				ln_break(ei->linebreak);
				if ((a = get_attr_val(attr, cast_uchar "id"))) {
					special(f, SP_TAG, a);
					free(a);
				}
				if ((a = get_attr_val(attr, cast_uchar "style"))) {
					unsigned char *d, *s;

					if (!casestrcmp(cast_uchar ei->name, cast_uchar "INPUT")) {
						unsigned char *aa = get_attr_val(attr, cast_uchar "type");
						if (aa) {
							if (!casestrcmp(aa, cast_uchar "hidden"))
								noskip = 1;
							free(aa);
						}
					}

					for (d = s = a; *s; s++) if (*s > ' ') *d++ = *s;
					*d = 0;
					display_none |= !casecmp(a, cast_uchar "display:none", 12) && !noskip;
					free(a);
				}
				if (display_none) {
					if (ei->nopair == 1) goto set_lt;
					html_stack_dup();
					html_top.name = name;
					html_top.namelen = namelen;
					html_top.options = attr;
					html_top.linebreak = 0;
					html_top.invisible = INVISIBLE;
					goto set_lt;
				}
				if (!html_top.invisible) {
					int a = par_format.align == AL_NO || par_format.align == AL_NO_BREAKABLE;
					struct par_attrib pa = par_format;
					if (ei->func == html_table && d_opt->tables && table_level < HTML_MAX_TABLE_LEVEL) {
						format_table(attr, html, eof, &html, f);
						set_globals;
						ln_break(2);
						goto set_lt;
					}
					if (ei->func == html_select) {
						if (!do_html_select(attr, html, eof, &html))
							goto set_lt;
					}
					if (ei->func == html_textarea) {
						do_html_textarea(attr, html, eof, &html);
						goto set_lt;
					}
					if (ei->nopair == 2 || ei->nopair == 3) {
						struct html_element *e = NULL;	/* against warning */
						struct list_head *le;
						if (ei->nopair == 2) {
							foreach(struct html_element, e, le, html_stack) {
								if (e->dontkill) break;
								if (e->linebreak || !ei->linebreak) break;
							}
						} else foreach(struct html_element, e, le, html_stack) {
							if (e->linebreak && !ei->linebreak) break;
							if (e->dontkill) break;
							if (e->namelen == namelen && !casecmp(e->name, name, e->namelen)) break;
						}
						if (e->namelen == namelen && !casecmp(e->name, name, e->namelen)) {
							while (e->list_entry.prev != &html_stack) kill_html_stack_item(list_struct(e->list_entry.prev, struct html_element));
							if (e->dontkill != 2) kill_html_stack_item(e);
						}
					}
					if (ei->nopair != 1) {
						html_stack_dup();
						html_top.name = name;
						html_top.namelen = namelen;
						html_top.options = attr;
						html_top.linebreak = ei->linebreak;
					}
					if (ei->func) ei->func(attr);
					if (ei->func == html_a) html_a_special(attr, html, eof);
					if (ei->func != html_br) was_br = 0;
					if (a) par_format = pa;
				} else {
					if (!ei->nopair) {
						html_stack_dup();
						html_top.name = name;
						html_top.namelen = namelen;
						html_top.options = attr;
						html_top.linebreak = 0;
						html_top.invisible = INVISIBLE;
					}
				}
			} else {
				struct html_element *e = NULL, *fx = NULL;
				struct list_head *le, *lfx;
				int lnb = 0;
				int xxx = 0;
				was_br = 0;
				if (ei->nopair == 1 || ei->nopair == 3) break;
				/*debug_stack();*/
				foreach(struct html_element, e, le, html_stack) {
					if (e->linebreak && !ei->linebreak) xxx = 1;
					if (e->namelen != namelen || casecmp(e->name, name, e->namelen)) {
						if (e->dontkill) break;
						else continue;
					}
					if (xxx) {
						kill_html_stack_item(e);
						break;
					}
					foreachbackfrom(struct html_element, fx, lfx, html_stack, le)
						if (fx->linebreak > lnb) lnb = fx->linebreak;
					format_.fontsize = list_struct(e->list_entry.next, struct html_element)->attr.fontsize;
					ln_break(lnb);
					while (e->list_entry.prev != &html_stack) kill_html_stack_item(list_struct(e->list_entry.prev, struct html_element));
					kill_html_stack_item(e);
					break;
				}
				/*debug_stack();*/
			}
			goto set_lt;
		}
		if (!inv) {
			if ((a = get_attr_val(attr, cast_uchar "id"))) {
				special(f, SP_TAG, a);
				free(a);
			}
		}
		goto set_lt;
	}
	put_chrs(lt, (int)(html - lt));
	ln_break(1);
	putsp = -1;
	pos = 0;
	/*line_breax = 1;*/
	was_br = 0;
#undef set_globals
}

static void scan_area_tag(unsigned char *attr, unsigned char *name, unsigned char **ptr, struct memory_list **ml)
{
	unsigned char *v;
	if ((v = get_attr_val(attr, name))) {
		*ptr = v;
		add_to_ml(ml, v, NULL);
	}
}

int get_image_map(unsigned char *head, unsigned char *s, unsigned char *eof, unsigned char *tag, struct menu_item **menu, struct memory_list **ml, unsigned char *href_base, unsigned char *target_base, int to, int def, int hdef, int gfx)
{
	unsigned char *name, *attr, *al, *label, *href, *target;
	int namelen, lblen;
	struct link_def *ld;
	struct menu_item *nm;
	int nmenu = 0;
	int i;
	unsigned char *hd = init_str();
	int hdl = 0;
	struct conv_table *ct;
	if (head) add_to_str(&hd, &hdl, head);
	scan_http_equiv(s, eof, &hd, &hdl, NULL, NULL, NULL, NULL);
	if (!gfx) ct = get_convert_table(hd, to, def, NULL, NULL, hdef);
	else ct = convert_table;
	free(hd);
	*menu = mem_calloc(sizeof(struct menu_item));
	se:
	while (s < eof && *s != '<') {
		sp:
		s++;
	}
	if (s >= eof) {
		free(*menu);
		return -1;
	}
	if (eof - s >= 2 && (s[1] == '!' || s[1] == '?')) {
		s = skip_comment(s, eof);
		goto se;
	}
	if (parse_element(s, eof, &name, &namelen, &attr, &s)) goto sp;
	if (namelen != 3 || casecmp(name, cast_uchar "MAP", 3)) goto se;
	if (tag && *tag) {
		if (!(al = get_attr_val(attr, cast_uchar "name"))) goto se;
		if (casestrcmp(al, tag)) {
			free(al);
			goto se;
		}
		free(al);
	}
	*ml = getml(NULL);
	se2:
	while (s < eof && *s != '<') {
		sp2:
		s++;
	}
	if (s >= eof) {
		freeml(*ml);
		free(*menu);
		return -1;
	}
	if (eof - s >= 2 && (s[1] == '!' || s[1] == '?')) {
		s = skip_comment(s, eof);
		goto se2;
	}
	if (parse_element(s, eof, &name, &namelen, &attr, &s)) goto sp2;
	if (namelen == 1 && !casecmp(name, cast_uchar "A", 1)) {
		unsigned char *ss;
		label = init_str();
		lblen = 0;
		se3:
		ss = s;
		se4:
		while (ss < eof && *ss != '<') ss++;
		if (ss >= eof) {
			free(label);
			freeml(*ml);
			free(*menu);
			return -1;
		}
		add_bytes_to_str(&label, &lblen, s, ss - s);
		s = ss;
		if (eof - s >= 2 && (s[1] == '!' || s[1] == '?')) {
			s = skip_comment(s, eof);
			goto se3;
		}
		if (parse_element(s, eof, NULL, NULL, NULL, &ss)) {
			ss = s + 1;
			goto se4;
		}
		if (!((namelen == 1 && !casecmp(name, cast_uchar "A", 1)) ||
		      (namelen == 2 && !casecmp(name, cast_uchar "/A", 2)) ||
		      (namelen == 3 && !casecmp(name, cast_uchar "MAP", 3)) ||
		      (namelen == 4 && !casecmp(name, cast_uchar "/MAP", 4)) ||
		      (namelen == 4 && !casecmp(name, cast_uchar "AREA", 4)) ||
		      (namelen == 5 && !casecmp(name, cast_uchar "/AREA", 5)))) {
				s = ss;
				goto se3;
		}
	} else if (namelen == 4 && !casecmp(name, cast_uchar "AREA", 4)) {
		unsigned char *l = get_attr_val(attr, cast_uchar "alt");
		if (l) {
			label = !gfx ? convert_string(ct, l, (int)strlen(cast_const_char l), d_opt) : stracpy(l);
			free(l);
		} else
			label = NULL;
	} else if (namelen == 4 && !casecmp(name, cast_uchar "/MAP", 4)) goto done;
	else goto se2;
	href = get_url_val(attr, cast_uchar "href");
	if (!(target = get_target(attr)) && !(target = stracpy(target_base)))
		target = stracpy(cast_uchar "");
	ld = mem_calloc(sizeof(struct link_def));
	if (href) {
		ld->link = join_urls(href_base, href);
		free(href);
	}
	ld->target = target;

	add_to_ml(ml, ld, ld->target, NULL);
	if (ld->link)
		add_to_ml(ml, ld->link, NULL);
	scan_area_tag(attr, cast_uchar "shape", &ld->shape, ml);
	scan_area_tag(attr, cast_uchar "coords", &ld->coords, ml);
	scan_area_tag(attr, cast_uchar "onclick", &ld->onclick, ml);
	scan_area_tag(attr, cast_uchar "ondblclick", &ld->ondblclick, ml);
	scan_area_tag(attr, cast_uchar "onmousedown", &ld->onmousedown, ml);
	scan_area_tag(attr, cast_uchar "onmouseup", &ld->onmouseup, ml);
	scan_area_tag(attr, cast_uchar "onmouseover", &ld->onmouseover, ml);
	scan_area_tag(attr, cast_uchar "onmouseout", &ld->onmouseout, ml);
	scan_area_tag(attr, cast_uchar "onmousemove", &ld->onmousemove, ml);

	if (label)
		clr_spaces(label, 1);
	if (!*label) {
		free(label);
		label = NULL;
	}
	ld->label = label;
	if (!label)
		label = stracpy(ld->link);
	if (!*label) {
		free(label);
		label = NULL;
	}
	if (!label)
		label = stracpy(ld->onclick);
	if (!*label) {
		free(label);
		label = NULL;
	}
	if (!label && !gfx)
		goto se2;
	if (!label)
		label = stracpy(ld->onmousedown);
	if (!*label) {
		free(label);
		label = NULL;
	}
	if (!label)
		label = stracpy(ld->onmouseup);
	if (!*label) {
		free(label);
		label = NULL;
	}
	if (!label)
		label = stracpy(ld->ondblclick);
	if (!*label) {
		free(label);
		label = NULL;
	}
	if (!label)
		label = stracpy(ld->onmouseover);
	if (!*label) {
		free(label);
		label = NULL;
	}
	if (!label)
		label = stracpy(ld->onmouseout);
	if (!*label) {
		free(label);
		label = NULL;
	}
	if (!label)
		label = stracpy(ld->onmousemove);
	if (!*label) {
		free(label);
		label = NULL;
	}
	if (!label)
		goto se2;
	add_to_ml(ml, label, NULL);

	if (!gfx) for (i = 0; i < nmenu; i++) {
		struct link_def *ll = (*menu)[i].data;
		if (!xstrcmp(ll->link, ld->link)
		&& !xstrcmp(ll->target, ld->target)
		&& !xstrcmp(ll->onclick, ld->onclick))
			goto se2;
	}
	if ((unsigned)nmenu > INT_MAX / sizeof(struct menu_item) - 2)
		overalloc();
	nm = xrealloc(*menu, (nmenu + 2) * sizeof(struct menu_item));
	*menu = nm;
	memset(&nm[nmenu], 0, 2 * sizeof(struct menu_item));
	nm[nmenu].text = label;
	nm[nmenu].rtext = cast_uchar "";
	nm[nmenu].hotkey = cast_uchar "";
	nm[nmenu].func = map_selected;
	nm[nmenu].data = ld;
	nm[++nmenu].text = NULL;
	goto se2;
	done:
	add_to_ml(ml, *menu, NULL);
	return 0;
}

void scan_http_equiv(unsigned char *s, unsigned char *eof, unsigned char **head, int *hdl, unsigned char **title, unsigned char **background, unsigned char **bgcolor, int *pre_wrap)
{
	unsigned char *name, *attr, *he, *c;
	int namelen;
	int tlen = 0;
	if (background) *background = NULL;
	if (bgcolor) *bgcolor = NULL;
	if (pre_wrap) *pre_wrap = 0;
	if (title) *title = init_str();
	add_chr_to_str(head, hdl, '\n');
	se:
	while (s < eof && *s != '<') sp:s++;
	if (s >= eof) return;
	if (eof - s >= 2 && (s[1] == '!' || s[1] == '?')) {
		s = skip_comment(s, eof);
		goto se;
	}
	if (parse_element(s, eof, &name, &namelen, &attr, &s)) goto sp;
	ps:
	if (namelen == 6 && !casecmp(name, cast_uchar "SCRIPT", 6)) {
		if (should_skip_script(attr)) {
			s = skip_element(s, eof, cast_uchar "SCRIPT", 0);
			goto se;
		}
	}
	if (namelen == 4 && !casecmp(name, cast_uchar "BODY", 4)) {
		if (background) {
			*background = get_attr_val(attr, cast_uchar "background");
			background = NULL;
		}
		if (bgcolor) {
			*bgcolor = get_attr_val(attr, cast_uchar "bgcolor");
			bgcolor = NULL;
		}
		/*return;*/
	}
	if (title && !tlen && namelen == 5 && !casecmp(name, cast_uchar "TITLE", 5)) {
		unsigned char *s1;
		xse:
		s1 = s;
		while (s < eof && *s != '<') xsp:s++;
		add_bytes_to_str(title, &tlen, s1, s - s1);
		if (s >= eof) goto se;
		if (eof - s >= 2 && (s[1] == '!' || s[1] == '?')) {
			s = skip_comment(s, eof);
			goto xse;
		}
		if (parse_element(s, eof, &name, &namelen, &attr, &s)) {
			s1 = s;
			goto xsp;
		}
		clr_spaces(*title, 1);
		goto ps;
	}
	if (namelen == 5 && !casecmp(name, cast_uchar "STYLE", 5)) {
		while (s < eof && *s != '<') {
			if (*s == 'p' && eof - s >= 8 && !strncmp(cast_const_char s, "pre-wrap", 8)) {
				if (pre_wrap) *pre_wrap = 1;
			}
			s++;
		}
		goto se;
	}
	if (namelen != 4 || casecmp(name, cast_uchar "META", 4)) goto se;
	if ((he = get_attr_val(attr, cast_uchar "charset"))) {
		add_to_str(head, hdl, cast_uchar "Charset: ");
		add_to_str(head, hdl, he);
		add_to_str(head, hdl, cast_uchar "\r\n");
		free(he);
	}
	if (!(he = get_attr_val(attr, cast_uchar "http-equiv"))) goto se;
	c = get_attr_val(attr, cast_uchar "content");
	add_to_str(head, hdl, he);
	if (c) {
		add_to_str(head, hdl, cast_uchar ": ");
		add_to_str(head, hdl, c); 
		free(c);
	}
	free(he);
	add_to_str(head, hdl, cast_uchar "\r\n");
	goto se;
}
