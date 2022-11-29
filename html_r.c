/* html_r.c
 * (c) 2002 Mikulas Patocka
 * This file is a part of the Links program, released under GPL.
 */

#include <limits.h>

#include "links.h"

struct f_data *
init_formatted(struct document_options *opt)
{
	struct f_data *scr;
	scr = mem_calloc(sizeof(struct f_data));
	copy_opt(&scr->opt, opt);
	scr->data = NULL;
	scr->nlinks = 0;
	scr->links = NULL;
	init_list(scr->forms);
	init_list(scr->tags);
	init_list(scr->nodes);
	return scr;
}

void
destroy_fc(struct form_control *fc)
{
	int i;
	free(fc->action);
	free(fc->target);
	free(fc->form_name);
	free(fc->onsubmit);
	free(fc->name);
	free(fc->alt);
	free(fc->default_value);
	for (i = 0; i < fc->nvalues; i++) {
		free(fc->values[i]);
		free(fc->labels[i]);
	}
	free(fc->values);
	free(fc->labels);
	if (fc->menu)
		free_menu(fc->menu);
}

void
free_frameset_desc(struct frameset_desc *fd)
{
	int i;
	for (i = 0; i < fd->n; i++) {
		if (fd->f[i].subframe)
			free_frameset_desc(fd->f[i].subframe);
		free(fd->f[i].name);
		free(fd->f[i].url);
	}
	free(fd);
}

struct frameset_desc *
copy_frameset_desc(struct frameset_desc *fd)
{
	int i;
	struct frameset_desc *neww;
	if ((unsigned)fd->n > INT_MAX / sizeof(struct frame_desc))
		overalloc();
	neww = xmalloc(sizeof(struct frameset_desc)
	               + fd->n * sizeof(struct frame_desc));
	memcpy(neww, fd,
	       sizeof(struct frameset_desc)
	           + fd->n * sizeof(struct frame_desc));
	for (i = 0; i < neww->n; i++) {
		if (neww->f[i].subframe)
			neww->f[i].subframe =
			    copy_frameset_desc(neww->f[i].subframe);
		if (neww->f[i].name)
			neww->f[i].name = stracpy(neww->f[i].name);
		if (neww->f[i].url)
			neww->f[i].url = stracpy(neww->f[i].url);
	}
	return neww;
}

void
free_additional_files(struct additional_files **a)
{
	struct additional_file *af = NULL;
	struct list_head *laf;
	if (!*a)
		return;
	if (--(*a)->refcount) {
		*a = NULL;
		return;
	}
	foreach (struct additional_file, af, laf, (*a)->af)
		release_object(&af->rq);
	free_list(struct additional_file, (*a)->af);
	free(*a);
	*a = NULL;
}

static void
clear_formatted(struct f_data *scr)
{
	int n;
	int y;
	struct form_control *fc = NULL;
	struct list_head *lfc;
	if (!scr)
		return;

	free(scr->search_chr);
	free(scr->search_pos);
	free(scr->slines1);
	free(scr->slines2);

	release_object(&scr->rq);
	free_additional_files(&scr->af);
	free(scr->title);
	if (scr->frame_desc) {
		free_frameset_desc(scr->frame_desc);
	}
	for (n = 0; n < scr->nlinks; n++) {
		struct link *l = &scr->links[n];
		free(l->where);
		free(l->target);
		free(l->where_img);
		free(l->img_alt);
		free(l->pos);
	}
	free(scr->links);
	for (y = 0; y < scr->y; y++)
		free(scr->data[y].d);
	free(scr->data);
	free(scr->lines1);
	free(scr->lines2);
	free(scr->opt.framename);
	foreach (struct form_control, fc, lfc, scr->forms) {
		destroy_fc(fc);
	}
	free_list(struct form_control, scr->forms);
	free_list(struct tag, scr->tags);
	free_list(struct node, scr->nodes);
	free(scr->refresh);
}

void
destroy_formatted(struct f_data *scr)
{
	if (scr->fd) {
		internal("trying to free locked formatted data");
		return;
	}
	clear_formatted(scr);
	free(scr);
}

static inline int
color_distance(struct rgb *c1, struct rgb *c2)
{
	return 3 * (c1->r - c2->r) * (c1->r - c2->r)
	       + 4 * (c1->g - c2->g) * (c1->g - c2->g)
	       + 2 * (c1->b - c2->b) * (c1->b - c2->b);
}

struct rgb palette_16_colors[16] = {
	{0x00,  0x00, 0x00, 0},
        { 0x80, 0x00, 0x00, 0},
	{ 0x00, 0x80, 0x00, 0},
        { 0xaa, 0x55, 0x00, 0},
	{ 0x00, 0x00, 0x80, 0},
        { 0x80, 0x00, 0x80, 0},
	{ 0x00, 0x80, 0x80, 0},
        { 0xaa, 0xaa, 0xaa, 0},
	{ 0x55, 0x55, 0x55, 0},
        { 0xff, 0x55, 0x55, 0},
	{ 0x55, 0xff, 0x55, 0},
        { 0xff, 0xff, 0x55, 0},
	{ 0x55, 0x55, 0xff, 0},
        { 0xff, 0x55, 0xff, 0},
	{ 0x55, 0xff, 0xff, 0},
        { 0xff, 0xff, 0xff, 0},
};

struct rgb_cache_entry {
	int color;
	int l;
	struct rgb rgb;
};

#define RGB_HASH_SIZE 4096

#define HASH_RGB(r, l)                                                         \
	((((r)->r << 3) + ((r)->g << 2) + (r)->b + (l)) & (RGB_HASH_SIZE - 1))

int
find_nearest_color(struct rgb *r, int l)
{
	int dist, dst, min, i;
	static struct rgb_cache_entry rgb_cache[RGB_HASH_SIZE];
	static int cache_init = 0;
	int h;
	if ((size_t)l > array_elements(palette_16_colors))
		internal("invalid length %d", l);
	if (!cache_init)
		goto initialize;
back:
	h = HASH_RGB(r, l);
	if (rgb_cache[h].color != -1 && rgb_cache[h].l == l
	    && rgb_cache[h].rgb.r == r->r && rgb_cache[h].rgb.g == r->g
	    && rgb_cache[h].rgb.b == r->b)
		return rgb_cache[h].color;
	dist = 0xffffff;
	min = 0;
	for (i = 0; i < l; i++)
		if ((dst = color_distance(r, &palette_16_colors[i])) < dist) {
			dist = dst;
			min = i;
		}
	rgb_cache[h].color = min;
	rgb_cache[h].l = l;
	rgb_cache[h].rgb.r = r->r;
	rgb_cache[h].rgb.g = r->g;
	rgb_cache[h].rgb.b = r->b;
	return min;

initialize:
	for (h = 0; h < RGB_HASH_SIZE; h++)
		rgb_cache[h].color = -1;
	cache_init = 1;
	goto back;
}

int
fg_color(int fg, int bg)
{
	int l = bg < fg ? bg : fg;
	int h = bg < fg ? fg : bg;
	if (l == h || (!l && (h == 4 || h == 8 || h == 12))
	    || (l == 1 && (h == 3 || h == 5 || h == 8 || h == 12))
	    || (l == 2 && h == 6) || (l == 3 && (h == 5 || h == 12))
	    || (l == 4 && (h == 8 || h == 12))
	    || (l == 5 && (h == 8 || h == 12)))
		return (fg == 4 || fg == 12) && (bg == 0 || bg == 8)
		           ? 6
		           : (7 - 7 * (bg == 2 || bg == 6 || bg == 7));
	return fg;
}

static int nowrap = 0;

static void
xpand_lines(struct part *p, int y)
{
	if (!p->data)
		return;
	if (y < 0)
		return;
	y = safe_add(y, safe_add(p->yp, 1));
	if (y > p->data->y) {
		int i;
		if ((y ^ p->data->y) > p->data->y) {
			unsigned s;
			for (s = 1; s < (unsigned)y; s = s * 2 + 1) {
				if (s > INT_MAX / sizeof(struct line))
					overalloc();
			}
			p->data->data =
			    xrealloc(p->data->data, s * sizeof(struct line));
		}
		for (i = p->data->y; i < y; i++) {
			p->data->data[i].l = 0;
			p->data->data[i].allocated = 0;
			p->data->data[i].d = NULL;
		}
		p->data->y = y;
	}
}

static void
xpand_line(struct part *p, int y, int x)
{
	struct line *ln;
	if (!p->data)
		return;
	x = safe_add(x, p->xp);
	y = safe_add(y, p->yp);
	ln = &p->data->data[y];
	if (x >= ln->l) {
		int i;
		if (x >= ln->allocated) {
			if (x >= 0x4000)
				ln->allocated = safe_add(x, x);
			else
				ln->allocated = safe_add(x, 0x10) & ~0xf;
			if ((unsigned)ln->allocated > INT_MAX / sizeof(chr))
				overalloc();
			ln->d = xrealloc(ln->d, ln->allocated * sizeof(chr));
		}
		for (i = ln->l; i <= x; i++) {
			ln->d[i].at = p->attribute;
			ln->d[i].ch = ' ';
		}
		ln->l = i;
	}
}

static void
r_xpand_spaces(struct part *p, int l)
{
	unsigned char *c;
	if ((unsigned)l >= INT_MAX)
		overalloc();
	c = xrealloc(p->spaces, l + 1);
	memset(c + p->spl, 0, l - p->spl + 1);
	p->spl = l + 1;
	p->spaces = c;
}

static inline void
xpand_spaces(struct part *p, int l)
{
	if ((unsigned)l >= (unsigned)p->spl)
		r_xpand_spaces(p, l);
}

#define POS(x, y) (p->data->data[safe_add(p->yp, (y))].d[safe_add(p->xp, (x))])
#define LEN(y)                                                                 \
	(p->data->data[safe_add(p->yp, (y))].l - p->xp < 0                     \
	     ? 0                                                               \
	     : p->data->data[p->yp + (y)].l - p->xp)
#define SLEN(y, x) p->data->data[safe_add(p->yp, (y))].l = safe_add(p->xp, x)
#define X(x)       safe_add(p->xp, (x))
#define Y(y)       safe_add(p->yp, (y))

static inline void
set_hchar(struct part *p, int x, int y, unsigned ch, unsigned char at)
{
	chr *cc;
	xpand_lines(p, y);
	xpand_line(p, y, x);
	cc = &POS(x, y);
	cc->ch = ch;
	cc->at = at;
}

static inline void
set_hchars(struct part *p, int x, int y, int xl, unsigned ch, unsigned char at)
{
	chr *cc;
	xpand_lines(p, y);
	xpand_line(p, y, safe_add(x, xl) - 1);
	cc = &POS(x, y);
	for (; xl; xl--) {
		cc->ch = ch;
		cc->at = at;
		cc++;
	}
}

void
xset_hchar(struct part *p, int x, int y, unsigned ch, unsigned char at)
{
	set_hchar(p, x, y, ch, at);
}

void
xset_hchars(struct part *p, int x, int y, int xl, unsigned ch, unsigned char at)
{
	set_hchars(p, x, y, xl, ch, at);
}

void
xxpand_lines(struct part *p, int y)
{
	xpand_lines(p, y);
}

void
xxpand_line(struct part *p, int y, int x)
{
	xpand_line(p, y, x);
}

static inline void
set_hline(struct part *p, int x, int y, int xl, unsigned char *d,
          unsigned char at)
{
	chr *cc;
	int xp;
	xpand_lines(p, y);
	xpand_line(p, y, safe_add(x, xl) - 1);
	xp = par_format.align != AL_NO;
	if (xp)
		xpand_spaces(p, safe_add(x, xl) - 1);
	cc = NULL;
	if (p->data)
		cc = &POS(x, y);
	for (; xl; xl--, x++, d++) {
		if (xp)
			p->spaces[x] = *d == ' ';
		if (p->data) {
			cc->ch = *d;
			cc->at = at;
			cc++;
		}
	}
}

static inline void
set_hline_uni(struct part *p, int x, int y, int xl, char_t *d, unsigned char at)
{
	chr *cc;
	int xp;
	xpand_lines(p, y);
	xpand_line(p, y, safe_add(x, xl) - 1);
	xp = par_format.align != AL_NO;
	if (xp)
		xpand_spaces(p, safe_add(x, xl) - 1);
	cc = NULL;
	if (p->data)
		cc = &POS(x, y);
	for (; xl; xl--, x++, d++) {
		if (xp)
			p->spaces[x] = *d == ' ';
		if (p->data) {
			cc->ch = *d;
			cc->at = at;
			cc++;
		}
	}
}

static int last_link_to_move;
static struct list_head *last_tag_to_move;
static struct list_head *last_tag_for_newline;

static inline void
move_links(struct part *p, int xf, int yf, int xt, int yt)
{
	int n;
	struct tag *t = NULL;
	struct list_head *lt;
	int w = 0;
	if (!p->data)
		return;
	xpand_lines(p, yt);
	for (n = last_link_to_move; n < p->data->nlinks; n++) {
		int i;
		struct link *link = &p->data->links[n];
		/*printf("ml: %d %d %d
		 * %d",link->pos[0].x,link->pos[0].y,X(xf),Y(yf));fflush(stdout);portable_sleep(1000);*/
		for (i = link->first_point_to_move; i < link->n; i++)
			if (link->pos[i].y >= Y(yf)) {
				w = 1;
				if (link->pos[i].y == Y(yf)
				    && link->pos[i].x >= X(xf)) {
					if (yt >= 0) {
						link->pos[i].y = Y(yt);
						if (xt > xf)
							link->pos[i].x =
							    safe_add(
								link->pos[i].x,
								-xf + xt);
						else
							link->pos[i].x =
							    link->pos[i].x - xf
							    + xt;
					} else {
						memmove(
						    &link->pos[i],
						    &link->pos[i + 1],
						    (link->n - i - 1)
							* sizeof(struct point));
						link->n--;
						i--;
					}
				}
			} else {
				link->first_point_to_move = safe_add(i, 1);
			}
		if (!w)
			last_link_to_move = n;
	}
	w = 0;
	if (yt >= 0)
		foreachfrom (struct tag, t, lt, p->data->tags,
		             last_tag_to_move->next) {
			if (t->y == Y(yf)) {
				w = 1;
				if (t->x >= X(xf)) {
					t->y = Y(yt);
					if (xt > xf)
						t->x = safe_add(t->x, -xf + xt);
					else
						t->x += -xf + xt;
				}
			}
			if (!w)
				last_tag_to_move = &t->list_entry;
		}
}

static inline void
copy_chars(struct part *p, int x, int y, int xl, chr *d)
{
	if (xl <= 0)
		return;
	xpand_lines(p, y);
	xpand_line(p, y, safe_add(x, xl) - 1);
	for (; xl; xl--, x++, d++)
		POS(x, y) = *d;
}

static inline void
move_chars(struct part *p, int x, int y, int nx, int ny)
{
	if (LEN(y) - x <= 0)
		return;
	copy_chars(p, nx, ny, LEN(y) - x, &POS(x, y));
	SLEN(y, x);
	move_links(p, x, y, nx, ny);
}

static inline void
shift_chars(struct part *p, int y, int s)
{
	chr *a;
	int l = LEN(y);
	if ((unsigned)l > INT_MAX / sizeof(chr))
		overalloc();
	a = xmalloc(l * sizeof(chr));
	memcpy(a, &POS(0, y), l * sizeof(chr));
	set_hchars(p, 0, y, s, ' ', p->attribute);
	copy_chars(p, s, y, l, a);
	free(a);
	move_links(p, 0, y, s, y);
}

static inline void
del_chars(struct part *p, int x, int y)
{
	SLEN(y, x);
	move_links(p, x, y, -1, -1);
}

#define rm(x)                                                                  \
	((x).width - (x).rightmargin > 0 ? (x).width - (x).rightmargin : 0)

static void line_break(void *);

static int
split_line(struct part *p)
{
	int i;
	if (rm(par_format) >= p->z_spaces) {
		for (i = rm(par_format); i >= par_format.leftmargin; i--)
			if (i < p->spl && p->spaces[i])
				goto split;
	}
	i = rm(par_format) + 1;
	if (i < p->z_spaces)
		i = p->z_spaces;
	if (i < par_format.leftmargin)
		i = par_format.leftmargin;
	for (; i < p->cx; i++)
		if (i < p->spl && p->spaces[i])
			goto split;
	p->z_spaces = i;
	if (p->cx >= 0 && safe_add(p->cx, par_format.rightmargin) > p->x)
		p->x = p->cx + par_format.rightmargin;
	return 0;
split:
	if (safe_add(i, par_format.rightmargin) > p->x)
		p->x = i + par_format.rightmargin;
	if (p->data) {
		move_chars(p, safe_add(i, 1), p->cy, par_format.leftmargin,
		           safe_add(p->cy, 1));
		del_chars(p, i, p->cy);
	}
	memmove(p->spaces, p->spaces + safe_add(i, 1), p->spl - i - 1);
	memset(p->spaces + p->spl - i - 1, 0, i + 1);
	memmove(p->spaces + par_format.leftmargin, p->spaces,
	        p->spl - par_format.leftmargin);
	p->z_spaces = 0;
	p->cy = safe_add(p->cy, 1);
	p->cx -= i - par_format.leftmargin + 1;
	if (p->cx == par_format.leftmargin)
		p->cx = -1;
	if (p->y < safe_add(p->cy, (p->cx != -1)))
		p->y = p->cy + (p->cx != -1);
	return 1 + (p->cx == -1);
}

static void
align_line(struct part *p, int y)
{
	int na;
	if (!p->data)
		return;
	if (!LEN(y) || par_format.align == AL_LEFT || par_format.align == AL_NO
	    || par_format.align == AL_NO_BREAKABLE
	    || par_format.align == AL_BLOCK /* !!! fixme! */)
		return;
	na = rm(par_format) - LEN(y);
	if (par_format.align == AL_CENTER)
		na /= 2;
	if (na > 0)
		shift_chars(p, y, na);
}

struct link *
new_link(struct f_data *f)
{
	if (!f)
		return NULL;
	if (!(f->nlinks & (ALLOC_GR - 1))) {
		if ((unsigned)f->nlinks
		    > INT_MAX / sizeof(struct link) - ALLOC_GR)
			overalloc();
		f->links = xrealloc(f->links, (f->nlinks + ALLOC_GR)
		                                  * sizeof(struct link));
	}
	memset(&f->links[f->nlinks], 0, sizeof(struct link));
	return &f->links[f->nlinks++];
}

void
html_tag(struct f_data *f, unsigned char *t, int x, int y)
{
	struct tag *tag;
	size_t sl;
	unsigned char *tt;
	int ll;
	if (!f)
		return;
	tt = init_str();
	ll = 0;
	add_conv_str(&tt, &ll, t, (int)strlen(cast_const_char t), -2);
	sl = strlen(cast_const_char tt);
	if (sl > INT_MAX - sizeof(struct tag))
		overalloc();
	tag = xmalloc(sizeof(struct tag) + sl);
	tag->x = x;
	tag->y = y;
	strcpy(cast_char tag->name, cast_const_char tt);
	add_to_list(f->tags, tag);
	if (last_tag_for_newline == &f->tags)
		last_tag_for_newline = &tag->list_entry;
	free(tt);
}

unsigned char *last_link = NULL;
unsigned char *last_target = NULL;
unsigned char *last_image = NULL;
struct form_control *last_form = NULL;

static int nobreak;

struct conv_table *convert_table;

static void
put_chars(void *p_, unsigned char *c, int l)
{
	struct part *p = p_;

	static struct text_attrib_beginning ta_cache = {
		-1, {0,  0, 0, 0},
                 { 0, 0, 0, 0},
                 0, 0
	};
	static int bg_cache;
	static int fg_cache;
	char_t *uni_c = NULL;

	int bg, fg;
	int i;
	struct link *link;
	struct point *pt;
	int ll;

	if (l < 0)
		overalloc();

	/*printf("%d-", p->cx);for (i=0; i<l; i++) printf("%c", c[i]);
	 * printf("-\n");portable_sleep(1000);*/
	while (p->cx <= par_format.leftmargin && l && *c == ' '
	       && par_format.align != AL_NO
	       && par_format.align != AL_NO_BREAKABLE) {
		c++;
		l--;
	}
	if (!l)
		return;
	if (p->cx < par_format.leftmargin)
		p->cx = par_format.leftmargin;
	if (c[0] != ' ' || (c[1] && c[1] != ' ')) {
		if (p->data)
			last_tag_for_newline = &p->data->tags;
	}
	if (!d_opt->cp && !(format_.attr & AT_GRAPHICS)) {
		int pl;
		unsigned char *cc;
		if (p->utf8_part_len) {
			unsigned char new_part[7];
			unsigned char *q;
next_utf_byte:
			if ((*c & 0xc0) != 0x80)
				goto bad_utf;
			p->utf8_part[p->utf8_part_len++] = *c;
			p->utf8_part[p->utf8_part_len] = 0;
			c++;
			l--;
			q = p->utf8_part;
			if (!get_utf_8(&q)) {
				if (p->utf8_part_len
				    == sizeof(p->utf8_part) - 1)
					goto bad_utf;
				if (l)
					goto next_utf_byte;
				return;
			}
			pl = p->utf8_part_len;
			p->utf8_part_len = 0;
			strcpy(cast_char new_part,
			       cast_const_char p->utf8_part);
			put_chars(p, new_part, pl);
		}
bad_utf:
		p->utf8_part_len = 0;
		if (!l)
			return;
		if ((unsigned)l > (unsigned)INT_MAX / sizeof(char_t))
			overalloc();
		uni_c = xmalloc(l * sizeof(char_t));
		ll = 0;
		cc = c;
next_utf_char:
		pl = utf8chrlen(*cc);
		if (safe_add((int)(cc - c), pl) > l) {
			memcpy(p->utf8_part, cc,
			       p->utf8_part_len = l - (int)(cc - c));
			goto utf_done;
		} else {
			if (!pl) {
				cc++;
			} else {
				unsigned un;
				GET_UTF_8(cc, un);
				if (un != 0xad) {
					uni_c[ll] = un;
					ll = safe_add(ll, 1);
				}
			}
			if (cc < c + l)
				goto next_utf_char;
		}
utf_done:
		if (!ll) {
			free(uni_c);
			return;
		}
	} else {
		ll = l;
	}
	if (last_link || last_image || last_form || format_.link
	    || format_.image || format_.form)
		goto process_link;
no_l:
	/*printf("%d %d\n",p->cx, p->cy);*/
	if (memcmp(&ta_cache, &format_, sizeof(struct text_attrib_beginning)))
		goto format_change;
	bg = bg_cache;
	fg = fg_cache;
end_format_change:
	if (p->y < safe_add(p->cy, 1))
		p->y = p->cy + 1;
	if (nowrap && safe_add(p->cx, ll) > rm(par_format)) {
		free(uni_c);
		return;
	}
	if (!d_opt->cp && !(format_.attr & AT_GRAPHICS)) {
		set_hline_uni(p, p->cx, p->cy, ll, uni_c,
		              ((fg & 0x08) << 3) | (bg << 3) | (fg & 0x07));
	} else
		set_hline(p, p->cx, p->cy, l, c,
		          ((fg & 0x08) << 3) | (bg << 3) | (fg & 0x07));
	p->cx += ll;
	nobreak = 0;
	if (par_format.align != AL_NO)
		while (p->cx > rm(par_format)
		       && p->cx > par_format.leftmargin) {
			int x;
			if (!(x = split_line(p)))
				break;
			align_line(p, p->cy - 1);
			nobreak = x - 1;
		}
	p->xa = safe_add(p->xa, ll);
	if (safe_add(p->xa
	                 - (c[l - 1] == ' ' && par_format.align != AL_NO
	                    && par_format.align != AL_NO_BREAKABLE),
	             safe_add(par_format.leftmargin, par_format.rightmargin))
	    > p->xmax)
		p->xmax = p->xa
		          - (c[l - 1] == ' ' && par_format.align != AL_NO
		             && par_format.align != AL_NO_BREAKABLE)
		          + par_format.leftmargin + par_format.rightmargin;
	free(uni_c);
	return;

	/* !!! WARNING: THE FOLLOWING CODE IS SHADOWED IN HTML_GR.C */

process_link:
	if ((last_link || last_image || last_form)
	    && !xstrcmp(format_.link, last_link)
	    && !xstrcmp(format_.target, last_target)
	    && !xstrcmp(format_.image, last_image)
	    && format_.form == last_form) {
		if (!p->data)
			goto x;
		link = &p->data->links[p->data->nlinks - 1];
		if (!p->data->nlinks) {
			internal("no link");
			goto no_l;
		}
		goto set_link;
x:;
	} else {
		free(last_link);
		free(last_target);
		free(last_image);
		last_link = last_target = last_image = NULL;
		last_form = NULL;
		if (!(format_.link || format_.image || format_.form))
			goto no_l;
		if (d_opt->num_links) {
			unsigned char s[64];
			unsigned char *fl = format_.link, *ft = format_.target,
				      *fi = format_.image;
			struct form_control *ff = format_.form;
			format_.link = format_.target = format_.image = NULL;
			format_.form = NULL;
			if (d_opt->num_links) {
				s[0] = '[';
				snzprint(s + 1, 62, p->link_num);
				strcat(cast_char s, "]");
			} else {
				if (ff
				    && (ff->type == FC_TEXT
				        || ff->type == FC_PASSWORD
				        || ff->type == FC_FILE_UPLOAD
				        || ff->type == FC_TEXTAREA)) {
					strcpy(cast_char s, ">");
				} else if (ff
				           && (ff->type == FC_CHECKBOX
				               || ff->type == FC_RADIO
				               || ff->type == FC_SELECT)) {
					strcpy(cast_char s, "");
				} else {
					strcpy(cast_char s, "~");
				}
			}
			put_chars(p, s, (int)strlen(cast_const_char s));
			if (ff && ff->type == FC_TEXTAREA)
				line_break(p);
			if (p->cx < par_format.leftmargin)
				p->cx = par_format.leftmargin;
			format_.link = fl;
			format_.target = ft;
			format_.image = fi;
			format_.form = ff;
		}
		p->link_num++;
		last_link = stracpy(format_.link);
		last_target = stracpy(format_.target);
		last_image = stracpy(format_.image);
		last_form = format_.form;
		if (!p->data)
			goto no_l;
		if (!(link = new_link(p->data)))
			goto no_l;
		link->num = p->link_num - 1;
		link->pos = NULL;
		if (!last_form) {
			link->type = L_LINK;
			link->where = stracpy(last_link);
			link->target = stracpy(last_target);
		} else {
			link->type =
			    last_form->type == FC_TEXT
				    || last_form->type == FC_PASSWORD
				    || last_form->type == FC_FILE_UPLOAD
				? L_FIELD
			    : last_form->type == FC_TEXTAREA ? L_AREA
			    : last_form->type == FC_CHECKBOX
				    || last_form->type == FC_RADIO
				? L_CHECKBOX
			    : last_form->type == FC_SELECT ? L_SELECT
							   : L_BUTTON;
			link->form = last_form;
			link->target = stracpy(last_form->target);
		}
		link->where_img = stracpy(last_image);
		if (link->type != L_FIELD && link->type != L_AREA) {
			bg = find_nearest_color(&format_.clink, 8);
			fg = find_nearest_color(&format_.bg, 8);
			fg = fg_color(fg, bg);
		} else {
			fg = find_nearest_color(&format_.fg, 8);
			bg = find_nearest_color(&format_.bg, 8);
			fg = fg_color(fg, bg);
		}
		link->sel_color = get_attribute(fg, bg);
		link->n = 0;
set_link:
		if ((unsigned)link->n + (unsigned)ll
		    > INT_MAX / sizeof(struct point))
			overalloc();
		pt = xrealloc(link->pos, (link->n + ll) * sizeof(struct point));
		link->pos = pt;
		for (i = 0; i < ll; i++) {
			pt[link->n + i].x = X(p->cx) + i;
			pt[link->n + i].y = Y(p->cy);
		}
		link->n += ll;
	}
	goto no_l;

format_change:
	bg = find_nearest_color(&format_.bg, 8);
	fg = find_nearest_color(&format_.fg, 16);
	fg = fg_color(fg, bg);
	if (format_.attr & AT_ITALIC)
		fg = fg ^ 0x01;
	if (format_.attr & AT_UNDERLINE)
		fg = (fg ^ 0x04) | 0x08;
	if (format_.attr & AT_BOLD)
		fg = fg | 0x08;
	fg = fg_color(fg, bg);
	if (format_.attr & AT_GRAPHICS)
		bg = bg | 0x10;
	memcpy(&ta_cache, &format_, sizeof(struct text_attrib_beginning));
	fg_cache = fg;
	bg_cache = bg;
	goto end_format_change;
}

static void
line_break(void *p_)
{
	struct part *p = p_;
	struct tag *t = NULL;
	struct list_head *lt;
	if (p->cx >= 0 && safe_add(p->cx, par_format.rightmargin) > p->x)
		p->x = p->cx + par_format.rightmargin;
	if (nobreak) {
		nobreak = 0;
		p->cx = -1;
		p->xa = 0;
		return;
	}
	if (!p->data)
		goto e;
	xpand_lines(p, safe_add(p->cy, 1));
	if (p->cx > par_format.leftmargin && LEN(p->cy) > p->cx - 1
	    && POS(p->cx - 1, p->cy).ch == ' ') {
		del_chars(p, p->cx - 1, p->cy);
		p->cx--;
	}
	if (p->cx > 0)
		align_line(p, p->cy);
	if (p->data)
		foreachbackfrom (struct tag, t, lt, p->data->tags,
		                 last_tag_for_newline) {
			t->x = X(0);
			t->y = Y(p->cy + 1);
		}
e:
	p->cy++;
	p->cx = -1;
	p->xa = 0;
	if (p->spl > d_opt->xw)
		p->spl = d_opt->xw;
	memset(p->spaces, 0, p->spl);
	p->z_spaces = 0;
}

int g_ctrl_num;

/* SHADOWED IN g_html_form_control */
static void
html_form_control(struct part *p, struct form_control *fc)
{
	if (!p->data) {
		add_to_list(p->uf, fc);
		return;
	}
	fc->g_ctrl_num = g_ctrl_num;
	g_ctrl_num = safe_add(g_ctrl_num, 1);
	if (fc->type == FC_TEXT || fc->type == FC_PASSWORD
	    || fc->type == FC_TEXTAREA) {
		unsigned char *dv = convert_string(
		    convert_table, fc->default_value,
		    (int)strlen(cast_const_char fc->default_value), d_opt);
		if (dv) {
			free(fc->default_value);
			fc->default_value = dv;
		}
	}
	if (fc->type == FC_TEXTAREA) {
		unsigned char *p;
		for (p = fc->default_value; p[0]; p++)
			if (p[0] == '\r') {
				if (p[1] == '\n') {
					memmove(p, p + 1,
					        strlen(cast_const_char p));
					p--;
				} else
					p[0] = '\n';
			}
	}
	add_to_list(p->data->forms, fc);
}

static void
add_frameset_entry(struct frameset_desc *fsd, struct frameset_desc *subframe,
                   unsigned char *name, unsigned char *url, int marginwidth,
                   int marginheight, unsigned char scrolling)
{
	if (fsd->yp >= fsd->y)
		return;
	fsd->f[fsd->xp + fsd->yp * fsd->x].subframe = subframe;
	fsd->f[fsd->xp + fsd->yp * fsd->x].name = stracpy(name);
	fsd->f[fsd->xp + fsd->yp * fsd->x].url = stracpy(url);
	fsd->f[fsd->xp + fsd->yp * fsd->x].marginwidth = marginwidth;
	fsd->f[fsd->xp + fsd->yp * fsd->x].marginheight = marginheight;
	fsd->f[fsd->xp + fsd->yp * fsd->x].scrolling = scrolling;
	if (++fsd->xp >= fsd->x) {
		fsd->xp = 0;
		fsd->yp++;
	}
}

struct frameset_desc *
create_frameset(struct f_data *fda, struct frameset_param *fp)
{
	int i;
	struct frameset_desc *fd;
	if (!fp->x || !fp->y) {
		internal("zero size of frameset");
		return NULL;
	}
	if (fp->x
	    && (unsigned)fp->x * (unsigned)fp->y / (unsigned)fp->x
	           != (unsigned)fp->y)
		overalloc();
	if ((unsigned)fp->x * (unsigned)fp->y
	    > (INT_MAX - sizeof(struct frameset_desc))
	          / sizeof(struct frame_desc))
		overalloc();
	fd = mem_calloc(sizeof(struct frameset_desc)
	                + fp->x * fp->y * sizeof(struct frame_desc));
	fd->n = fp->x * fp->y;
	fd->x = fp->x;
	fd->y = fp->y;
	for (i = 0; i < fd->n; i++) {
		fd->f[i].xw = fp->xw[i % fp->x];
		fd->f[i].yw = fp->yw[i / fp->x];
	}
	if (fp->parent)
		add_frameset_entry(fp->parent, fd, NULL, NULL, -1, -1,
		                   SCROLLING_AUTO);
	else if (!fda->frame_desc)
		fda->frame_desc = fd;
	else {
		free(fd);
		fd = NULL;
	}
	return fd;
}

void
create_frame(struct frame_param *fp)
{
	add_frameset_entry(fp->parent, NULL, fp->name, fp->url, fp->marginwidth,
	                   fp->marginheight, fp->scrolling);
}

void
process_script(struct f_data *f, unsigned char *t)
{
}

void
set_base(struct f_data *f, unsigned char *t)
{
}

void
html_process_refresh(struct f_data *f, unsigned char *url, int time)
{
	if (!f)
		return;
	if (f->refresh)
		return;
	if (!url)
		f->refresh = stracpy(f->rq->url);
	else
		f->refresh = join_urls(f->rq->url, url);
	f->refresh_seconds = time;
}

static void *
html_special(void *p_, int c, ...)
{
	struct part *p = p_;
	va_list l;
	unsigned char *t;
	struct form_control *fc;
	struct frameset_param *fsp;
	struct frame_param *fp;
	struct refresh_param *rp;
	va_start(l, c);
	switch (c) {
	case SP_TAG:
		t = va_arg(l, unsigned char *);
		va_end(l);
		html_tag(p->data, t, X(p->cx >= 0 ? p->cx : 0), Y(p->cy));
		break;
	case SP_CONTROL:
		fc = va_arg(l, struct form_control *);
		va_end(l);
		html_form_control(p, fc);
		break;
	case SP_TABLE:
		va_end(l);
		return convert_table;
	case SP_USED:
		va_end(l);
		return (void *)(long)!!p->data;
	case SP_FRAMESET:
		fsp = va_arg(l, struct frameset_param *);
		va_end(l);
		return create_frameset(p->data, fsp);
	case SP_FRAME:
		fp = va_arg(l, struct frame_param *);
		va_end(l);
		create_frame(fp);
		break;
	case SP_NOWRAP:
		nowrap = va_arg(l, int);
		va_end(l);
		break;
	case SP_SCRIPT:
		t = va_arg(l, unsigned char *);
		va_end(l);
		if (p->data)
			process_script(p->data, t);
		break;
	case SP_REFRESH:
		rp = va_arg(l, struct refresh_param *);
		va_end(l);
		html_process_refresh(p->data, rp->url, rp->time);
		break;
	case SP_SET_BASE:
		t = va_arg(l, unsigned char *);
		va_end(l);
		if (p->data)
			set_base(p->data, t);
		break;
	default:
		va_end(l);
		internal("html_special: unknown code %d", c);
	}
	return NULL;
}

static void
do_format(unsigned char *start, unsigned char *end, struct part *part,
          unsigned char *head)
{
	pr(parse_html(start, end, put_chars, line_break, html_special, part,
	              head);){};
}

int margin;

struct part *
format_html_part(unsigned char *start, unsigned char *end, int align, int m,
                 int width, struct f_data *data, int xs, int ys,
                 unsigned char *head, int link_num)
{
	struct part *p;
	struct html_element *e;
	int llm = last_link_to_move;
	struct list_head *ltm = last_tag_to_move;
	int lm = margin;
	int ef = empty_format;
	struct form_control *fc = NULL;
	struct list_head *lfc;

	if (par_format.implicit_pre_wrap) {
		if (width > d_opt->xw)
			width = d_opt->xw;
	}

	if (!data) {
		p = find_table_cache_entry(start, end, align, m, width, xs,
		                           link_num);
		if (p)
			return p;
	}
	if (ys < 0) {
		internal("format_html_part: ys == %d", ys);
		return NULL;
	}
	if (data) {
		struct node *n;
		n = xmalloc(sizeof(struct node));
		n->x = xs;
		n->y = ys;
		n->xw = !table_level ? INT_MAX - 1 : width;
		add_to_list(data->nodes, n);
	}
	last_link_to_move = data ? data->nlinks : 0;
	last_tag_to_move = data ? &data->tags : NULL;
	last_tag_for_newline = data ? &data->tags : NULL;
	margin = m;
	empty_format = !data;
	free(last_link);
	free(last_image);
	free(last_target);
	last_link = last_image = last_target = NULL;
	last_form = NULL;
	nobreak = align != AL_NO && align != AL_NO_BREAKABLE;
	p = mem_calloc(sizeof(struct part));
	/*p->x = p->y = 0;*/
	p->data = data;
	p->xp = xs;
	p->yp = ys;
	/*p->xmax = p->xa = 0;*/
	p->attribute =
	    get_attribute(find_nearest_color(&format_.fg, 16),
	                  find_nearest_color(&par_format.bgcolor, 8));
	p->spaces = NULL;
	/*p->z_spaces = 0;*/
	/*p->spl = 0;*/
	p->link_num = link_num;
	init_list(p->uf);
	html_stack_dup();
	e = &html_top;
	html_top.dontkill = 2;
	html_top.namelen = 0;
	par_format.align = align;
	par_format.leftmargin = m;
	par_format.rightmargin = m;
	par_format.width = width;
	par_format.list_level = 0;
	par_format.list_number = 0;
	par_format.dd_margin = 0;
	if (align == AL_NO || align == AL_NO_BREAKABLE)
		format_.attr |= AT_FIXED;
	p->cx = -1;
	p->cy = 0;
	do_format(start, end, p, head);
	if (p->xmax < p->x)
		p->xmax = p->x;
	if (align == AL_NO || align == AL_NO_BREAKABLE) {
		if (p->cy > p->y)
			p->y = p->cy;
	}
	nobreak = 0;
	line_breax = 1;
	free(last_link);
	free(last_image);
	free(last_target);
	while (&html_top != e) {
		kill_html_stack_item(&html_top);
		if (!&html_top || (void *)&html_top == (void *)&html_stack) {
			internal("html stack trashed");
			break;
		}
	}
	html_top.dontkill = 0;
	kill_html_stack_item(&html_top);
	free(p->spaces);
	if (data) {
		struct node *n = list_struct(data->nodes.next, struct node);
		n->yw = ys - n->y + p->y;
	}
	foreach (struct form_control, fc, lfc, p->uf)
		destroy_fc(fc);
	free_list(struct form_control, p->uf);
	last_link_to_move = llm;
	last_tag_to_move = ltm;
	margin = lm;
	empty_format = ef;
	last_link = last_image = last_target = NULL;
	last_form = NULL;

	if (table_level > 1 && !data) {
		add_table_cache_entry(start, end, align, m, width, xs, link_num,
		                      p);
	}
	return p;
}

static void
push_base_format(unsigned char *url, struct document_options *opt, int frame,
                 int implicit_pre_wrap)
{
	struct html_element *e;
	if (!list_empty(html_stack)) {
		internal("something on html stack");
		init_list(html_stack);
	}
	e = mem_calloc(sizeof(struct html_element));
	add_to_list(html_stack, e);
	format_.attr = opt->plain & 1 ? AT_FIXED : 0;
	format_.fontsize = 3;
	format_.link = format_.target = format_.image = format_.select = NULL;
	format_.form = NULL;
	memcpy(&format_.fg, &opt->default_fg, sizeof(struct rgb));
	memcpy(&format_.bg, &opt->default_bg, sizeof(struct rgb));
	memcpy(&format_.clink, &opt->default_link, sizeof(struct rgb));
	format_.href_base = stracpy(url);
	format_.target_base = stracpy(opt->framename);
	par_format.align = !(opt->plain & 1)    ? AL_LEFT
	                   : !implicit_pre_wrap ? AL_NO
	                                        : AL_NO_BREAKABLE;
	par_format.leftmargin = opt->plain & 1 ? 0 : opt->margin;
	par_format.rightmargin = opt->plain & 1 ? 0 : opt->margin;
	if (frame && par_format.leftmargin)
		par_format.leftmargin = 1;
	if (frame && par_format.rightmargin)
		par_format.rightmargin = 1;
	par_format.width = opt->xw;
	par_format.list_level = par_format.list_number = 0;
	par_format.dd_margin = opt->margin;
	par_format.flags = 0;
	memcpy(&par_format.bgcolor, &opt->default_bg, sizeof(struct rgb));
	par_format.implicit_pre_wrap = implicit_pre_wrap;
	html_top.invisible = 0;
	html_top.name = NULL;
	html_top.namelen = 0;
	html_top.options = NULL;
	html_top.linebreak = 1;
	html_top.dontkill = 1;
}

struct conv_table *
get_convert_table(unsigned char *head, int to, int def, int *frm, int *aa,
                  int hard)
{
	int from = -1;
	unsigned char *a, *b;
	unsigned char *p = head;
	while (from == -1
	       && (a = parse_http_header(p, cast_uchar "Content-Type", &p))) {
		if ((b = parse_header_param(a, cast_uchar "charset", 0))) {
			from = 0;
			free(b);
		}
		free(a);
	}
	if (aa) {
		*aa = from == -1;
		if (hard && !*aa)
			*aa = 2;
	}
	if (hard || from == -1)
		from = def;
	if (frm)
		*frm = from;
	return get_translation_table(from, to);
}

struct document_options dd_opt;

struct document_options *d_opt = &dd_opt;

struct f_data *current_f_data = NULL;

void
really_format_html(struct cache_entry *ce, unsigned char *start,
                   unsigned char *end, struct f_data *screen, int frame)
{
	unsigned char *url = ce->url;
	unsigned char *head, *t;
	int hdl;
	int i;
	unsigned char *bg = NULL, *bgcolor = NULL;
	int implicit_pre_wrap;
	int bg_col, fg_col;
	struct part *rp;

	current_f_data = screen;
	d_opt = &screen->opt;
	screen->use_tag = ce->count;
	startf = start;
	eofff = end;
	head = init_str();
	hdl = 0;
	if (ce->head)
		add_to_str(&head, &hdl, ce->head);
	scan_http_equiv(start, end, &head, &hdl, &t, d_opt->plain ? NULL : &bg,
	                d_opt->plain || d_opt->col < 2 ? NULL : &bgcolor,
	                &implicit_pre_wrap);
	if (d_opt->break_long_lines)
		implicit_pre_wrap = 1;
	if (d_opt->plain)
		*t = 0;
	if (screen->opt.plain == 2) {
		screen->cp = 0;
		screen->ass = -1;
		convert_table = get_translation_table(0, screen->opt.cp);
	} else {
		convert_table = get_convert_table(
		    head, screen->opt.cp, screen->opt.assume_cp, &screen->cp,
		    &screen->ass, screen->opt.hard_assume);
	}
	screen->opt.real_cp = screen->cp;
	i = d_opt->plain;
	d_opt->plain = 0;
	screen->title = convert_string(convert_table, t,
	                               (int)strlen(cast_const_char t), d_opt);
	d_opt->plain = i;
	free(t);
	push_base_format(url, &screen->opt, frame, implicit_pre_wrap);
	table_level = 0;
	g_ctrl_num = 0;
	last_form_tag = NULL;
	last_form_attr = NULL;
	last_input_tag = NULL;
	if ((rp = format_html_part(start, end, par_format.align,
	                           par_format.leftmargin, screen->opt.xw,
	                           screen, 0, 0, head, 1)))
		free(rp);
	free(head);
	free(bg);
	free(bgcolor);
	screen->x = 0;
	for (i = screen->y - 1; i >= 0; i--) {
		if (!screen->data[i].l) {
			free(screen->data[i].d);
			screen->y--;
		} else {
			break;
		}
	}
	for (i = 0; i < screen->y; i++)
		if (screen->data[i].l > screen->x)
			screen->x = screen->data[i].l;
	free(form.action);
	free(form.target);
	free(form.form_name);
	free(form.onsubmit);
	form.action = NULL;
	form.target = NULL;
	form.form_name = NULL;
	form.onsubmit = NULL;
	bg_col = find_nearest_color(&format_.bg, 8);
	fg_col = find_nearest_color(&format_.fg, 16);
	fg_col = fg_color(fg_col, bg_col);
	screen->bg = get_attribute(fg_col, bg_col);
	kill_html_stack_item(&html_top);
	if (!list_empty(html_stack)) {
		internal("html stack not empty after operation");
		init_list(html_stack);
	}
	sort_links(screen);
	current_f_data = NULL;
	d_opt = &dd_opt;
}

int
compare_opt(struct document_options *o1, struct document_options *o2)
{
	if (o1->xw == o2->xw && o1->yw == o2->yw && o1->xp == o2->xp
	    && o1->yp == o2->yp && o1->scrolling == o2->scrolling
	    && o1->col == o2->col && o1->cp == o2->cp
	    && o1->assume_cp == o2->assume_cp
	    && o1->hard_assume == o2->hard_assume && o1->tables == o2->tables
	    && o1->frames == o2->frames
	    && o1->break_long_lines == o2->break_long_lines
	    && o1->images == o2->images && o1->image_names == o2->image_names
	    && o1->margin == o2->margin && o1->plain == o2->plain
	    && o1->num_links == o2->num_links
	    && o1->table_order == o2->table_order
	    && o1->auto_refresh == o2->auto_refresh
	    && o1->font_size == o2->font_size
	    && o1->display_images == o2->display_images
	    && o1->image_scale == o2->image_scale
	    && o1->porn_enable == o2->porn_enable
	    && o1->gamma_stamp == o2->gamma_stamp
	    && !memcmp(&o1->default_fg, &o2->default_fg, sizeof(struct rgb))
	    && !memcmp(&o1->default_bg, &o2->default_bg, sizeof(struct rgb))
	    && !memcmp(&o1->default_link, &o2->default_link, sizeof(struct rgb))
	    && ((o1->framename && o2->framename
	         && !casestrcmp(o1->framename, o2->framename))
	        || (!o1->framename && !o2->framename)))
		return 0;
	return 1;
}

void
copy_opt(struct document_options *o1, struct document_options *o2)
{
	memcpy(o1, o2, sizeof(struct document_options));
	o1->framename = stracpy(o2->framename);
}

struct link *
get_link_at_location(struct f_data *f, int x, int y)
{
	struct link *l1, *l2, *l;
	if (y < 0 || y >= f->y)
		return NULL;
	l1 = f->lines1[y];
	l2 = f->lines2[y];
	if (!l1 || !l2)
		return NULL;
	for (l = l1; l <= l2; l++) {
		int i;
		for (i = 0; i < l->n; i++)
			if (l->pos[i].x == x && l->pos[i].y == y)
				return l;
	}
	return NULL;
}

static int
sort_srch(struct f_data *f)
{
	int i;
	int *min, *max;
	if ((unsigned)f->y > INT_MAX / sizeof(struct search *))
		overalloc();
	if ((unsigned)f->y > INT_MAX / sizeof(int))
		overalloc();
	f->slines1 = xmalloc(f->y * sizeof(int));
	f->slines2 = xmalloc(f->y * sizeof(int));
	min = xmalloc(f->y * sizeof(int));
	max = xmalloc(f->y * sizeof(int));
	if (!f->slines1 || !f->slines2 || !min || !max) {
		free(f->slines1);
		free(f->slines2);
		f->slines1 = NULL;
		f->slines2 = NULL;
		free(min);
		free(max);
		return -1;
	}
	for (i = 0; i < f->y; i++)
		f->slines1[i] = f->slines2[i] = -1;
	for (i = 0; i < f->y; i++) {
		min[i] = INT_MAX;
		max[i] = 0;
	}
	for (i = 0; i < f->nsearch_pos; i++) {
		struct search *s = &f->search_pos[i];
		int xe;
		if (s->x < min[s->y]) {
			min[s->y] = s->x;
			f->slines1[s->y] = s->idx;
		}
		if (s->n == 1)
			xe = safe_add(s->x, s->co);
		else
			xe = safe_add(s->x, s->n);
		if (xe > max[s->y]) {
			max[s->y] = xe;
			f->slines2[s->y] = s->idx + s->co - 1;
		}
	}
	free(min);
	free(max);
	return 0;
}

static inline int
is_spc(chr *cc)
{
	return cc->ch <= ' ' || cc->at & ATTR_FRAME;
}

static int n_chr, n_pos;
static char_t srch_last_chr;
static int srch_last_x, srch_last_y;
static int srch_cont;

static void
get_srch_reset(void)
{
	n_chr = n_pos = 0;
	srch_last_chr = ' ';
	srch_last_x = srch_last_y = -1;
	srch_cont = 0;
}

static int
add_srch_chr(struct f_data *f, unsigned c, int x, int y, int nn)
{
	if (c == ' ' && srch_last_chr == ' ')
		return 0;
	if (c == '_') {
		struct link *l = get_link_at_location(f, x, y);
		if (l
		    && (l->type == L_SELECT || l->type == L_FIELD
		        || l->type == L_AREA))
			return 0;
	}
	srch_last_chr = c;
	if (f->search_chr)
		f->search_chr[n_chr] = c;
	if (n_chr == INT_MAX)
		return -1;
	n_chr++;
	if (srch_cont < 0xffff && x == srch_last_x + 1 && y == srch_last_y
	    && nn == 1) {
		srch_cont++;
		if (f->search_pos)
			f->search_pos[n_pos - 1].co = (unsigned short)srch_cont;
	} else {
		if (f->search_pos) {
			f->search_pos[n_pos].idx = n_chr - 1;
			f->search_pos[n_pos].x = x;
			f->search_pos[n_pos].y = y;
			f->search_pos[n_pos].n = (unsigned short)nn;
			if (f->search_pos[n_pos].n != nn)
				f->search_pos[n_pos].n = (unsigned short)~0U;
			f->search_pos[n_pos].co = 1;
		}
		srch_cont = 1;
		if (n_pos == INT_MAX)
			return -1;
		n_pos++;
	}
	if (nn == 1) {
		srch_last_x = x;
		srch_last_y = y;
	} else {
		srch_last_x = -1;
		srch_last_y = -1;
	}
	return 0;
}

static int
get_srch(struct f_data *f)
{
	struct node *n = NULL;
	struct list_head *ln;
	get_srch_reset();
#define add_srch(c_, x_, y_, n_)                                               \
	do {                                                                   \
		if (add_srch_chr(f, c_, x_, y_, n_))                           \
			return -1;                                             \
	} while (0)
	foreachback (struct node, n, ln, f->nodes) {
		int x, y;
		int xm = safe_add(n->x, n->xw), ym = safe_add(n->y, n->yw);
		/*printf("%d %d - %d %d\n", n->x, n->y, xm, ym);
		fflush(stdout);*/
		for (y = n->y; y < ym && y < f->y; y++) {
			int ns = 1;
			for (x = n->x; x < xm && x < f->data[y].l; x++) {
				unsigned c = f->data[y].d[x].ch;
				if (is_spc(&f->data[y].d[x]))
					c = ' ';
				if (c == ' ' && ns)
					continue;
				c = charset_upcase(c, f->opt.cp);
				if (ns) {
					add_srch(c, x, y, 1);
					ns = 0;
					continue;
				}
				if (c != ' ') {
					add_srch(c, x, y, 1);
				} else {
					int xx;
					for (xx = safe_add(x, 1);
					     xx < xm && xx < f->data[y].l; xx++)
						if (!is_spc(&f->data[y].d[xx]))
							goto ja_uz_z_toho_programovani_asi_zcvoknu;
					xx = x;
ja_uz_z_toho_programovani_asi_zcvoknu:
					/* uz jsem zcvoknul, trpim poruchou
					 * osobnosti */
					add_srch(' ', x, y, xx - x);
					if (xx == x)
						goto uz_jsem_zcvoknul__jsem_psychopat__trpim_poruchou_osobnosti;
					x = xx - 1;
				}
			}
uz_jsem_zcvoknul__jsem_psychopat__trpim_poruchou_osobnosti:
			add_srch(' ', x, y, 0);
		}
	}
#undef add_srch
	return 0;
}

int
get_search_data(struct f_data *f)
{
	if (f->search_pos)
		return 0;
	if (get_srch(f))
		return -1;
	if ((size_t)n_chr > ((size_t)-1) / sizeof(char_t)
	    || (size_t)n_pos > ((size_t)-1) / sizeof(struct search))
		return -1;
	f->search_chr = xmalloc(n_chr * sizeof(char_t));
	if (!f->search_chr)
		return -1;
	f->search_pos = xmalloc(n_pos * sizeof(struct search));
	if (!f->search_pos) {
		free(f->search_chr);
		f->search_chr = NULL;
		return -1;
	}
	if (get_srch(f))
		internal(
		    "get_search_data: get_srch should not fail second time");
	while (n_chr && f->search_chr[n_chr - 1] == ' ')
		n_chr--;
	f->nsearch_chr = n_chr;
	f->nsearch_pos = n_pos;
	if (sort_srch(f)) {
		free(f->search_pos);
		f->search_pos = NULL;
		free(f->search_chr);
		f->search_chr = NULL;
		f->nsearch_chr = f->nsearch_pos = 0;
		return -1;
	}
	return 0;
}
