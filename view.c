/* view.c
 * (c) 2002 Mikulas Patocka
 * This file is a part of the Links program, released under GPL.
 */

#include "links.h"

static void init_ctrl(struct form_control *, struct form_state *);

static int c_in_view(struct f_data_c *);

static void set_pos_x(struct f_data_c *, struct link *);
static void set_pos_y(struct f_data_c *, struct link *);
static void find_link(struct f_data_c *, int, int);

static int is_active_frame(struct session *ses, struct f_data_c *f);

static void send_open_in_new_xterm(struct terminal *term, void *open_window_, void *ses_);
static void (* const send_open_in_new_xterm_ptr)(struct terminal *, void *fn_, void *ses_) = send_open_in_new_xterm;

/* FIXME: remove */
static void free_format_text_cache_entry(struct form_state *fs)
{
	struct format_text_cache_entry *ftce = fs->ftce;
	if (!ftce)
		return;
	fs->ftce = NULL;
	free(ftce);
}

struct view_state *create_vs(void)
{
	struct view_state *vs;
	vs = mem_calloc(sizeof(struct view_state));
	vs->refcount = 1;
	vs->current_link = -1;
	vs->orig_link = -1;
	vs->frame_pos = -1;
	vs->plain = -1;
	vs->form_info = NULL;
	vs->form_info_len = 0;
	return vs;
}

/* FIXME: remove */
static void free_form_state(struct form_state *fs)
{
	free_format_text_cache_entry(fs);
	if (fs->string)
		free(fs->string);
}

void destroy_vs(struct view_state *vs)
{
	int i;
	if (--vs->refcount) {
		if (vs->refcount < 0) internal("destroy_vs: view_state refcount underflow");
		return;
	}
	for (i = 0; i < vs->form_info_len; i++) {
		free(vs->form_info[i].string);
		free(vs->form_info[i].ftce);
		vs->form_info[i].ftce = NULL;
	}
	free(vs->form_info);
	free(vs);
}

void check_vs(struct f_data_c *f)
{
	struct view_state *vs = f->vs;
	int ovx, ovy, ol, obx, oby;
	if (f->f_data->frame_desc) {
		int n = (int)list_size(&f->subframes);
		if (vs->frame_pos < 0) vs->frame_pos = 0;
		if (vs->frame_pos >= n) vs->frame_pos = n - 1;
		return;
	}
	ovx = f->vs->orig_view_posx;
	ovy = f->vs->orig_view_pos;
	ol = f->vs->orig_link;
	obx = f->vs->orig_brl_x;
	oby = f->vs->orig_brl_y;
	if (vs->current_link >= f->f_data->nlinks) vs->current_link = f->f_data->nlinks - 1;
	if (!F) {
		if (vs->current_link != -1 && !c_in_view(f)) {
			set_pos_x(f, &f->f_data->links[f->vs->current_link]);
			set_pos_y(f, &f->f_data->links[f->vs->current_link]);
		}
		if (vs->current_link == -1) find_link(f, 1, 0);
	}
	f->vs->orig_view_posx = ovx;
	f->vs->orig_view_pos = ovy;
	f->vs->orig_link = ol;
	f->vs->orig_brl_x = obx;
	f->vs->orig_brl_y = oby;
}

static void set_link(struct f_data_c *f)
{
	if (c_in_view(f)) return;
	find_link(f, 1, 0);
}

static int find_tag(struct f_data *f, unsigned char *name)
{
	struct tag *tag = NULL;
	struct list_head *ltag;
	unsigned char *tt;
	int ll;
	tt = init_str();
	ll = 0;
	add_conv_str(&tt, &ll, name, (int)strlen(cast_const_char name), -2);
	foreachback(struct tag, tag, ltag, f->tags) if (!casestrcmp(tag->name, tt) || (tag->name[0] == '#' && !casestrcmp(tag->name + 1, tt))) {
		free(tt);
		return tag->y;
	}
	free(tt);
	return -1;
}

static int comp_links(const void *l1_, const void *l2_)
{
	const struct link *l1 = (const struct link *)l1_;
	const struct link *l2 = (const struct link *)l2_;
	return l1->num - l2->num;
}

void sort_links(struct f_data *f)
{
	int i;
	if (F) return;
	if (f->nlinks)
		qsort(f->links, f->nlinks, sizeof(struct link), comp_links);
	if ((unsigned)f->y > INT_MAX / sizeof(struct link *)) overalloc();
	f->lines1 = mem_calloc(f->y * sizeof(struct link *));
	f->lines2 = mem_calloc(f->y * sizeof(struct link *));
	for (i = 0; i < f->nlinks; i++) {
		int p, q, j;
		struct link *link = &f->links[i];
		if (!link->n) {
			if (d_opt->num_links) continue;
			free(link->where);
			free(link->target);
			free(link->where_img);
			free(link->img_alt);
			free(link->pos);
			memmove(link, link + 1, (f->nlinks - i - 1) * sizeof(struct link));
			f->nlinks--;
			i--;
			continue;
		}
		p = f->y - 1;
		q = 0;
		for (j = 0; j < link->n; j++) {
			if (link->pos[j].y < p) p = link->pos[j].y;
			if (link->pos[j].y > q) q = link->pos[j].y;
		}
		if (p > q) {
			j = p;
			p = q;
			q = j;
		}
		for (j = p; j <= q; j++) {
			if (j >= f->y) {
				internal("link out of screen");
				continue;
			}
			f->lines2[j] = &f->links[i];
			if (!f->lines1[j]) f->lines1[j] = &f->links[i];
		}
	}
}

unsigned char *textptr_add(unsigned char *t, int i, int cp)
{
	if (cp) {
		if (i) t += strnlen(cast_const_char t, i);
		return t;
	} else {
		while (i-- && *t) FWD_UTF_8(t);
		return t;
	}
}

int textptr_diff(unsigned char *t2, unsigned char *t1, int cp)
{
	if (cp)
		return (int)(t2 - t1);
	else {
		int i = 0;
		while (t2 > t1) {
			FWD_UTF_8(t1);
			i++;
		}
		return i;
	}
}

static struct format_text_cache_entry *format_text_uncached(unsigned char *text, int width, int wrap, int cp)
{
	unsigned char *text_start = text;
	struct format_text_cache_entry *ftce;
	int lnn_allocated = ALLOC_GR;
	int lnn = 0;
	unsigned char *b = text;
	int sk, ps = 0;
	int xpos = 0;
	unsigned char *last_space = NULL;
	int last_space_xpos = 0;

	ftce = xmalloc(sizeof(struct format_text_cache_entry) - sizeof(struct line_info) + lnn_allocated * sizeof(struct line_info));

	ftce->width = width;
	ftce->wrap = wrap;
	ftce->cp = cp;
	ftce->last_state = -1;

	while (*text) {
		if (*text == '\n') {
			sk = 1;
			put:
			if (lnn == lnn_allocated) {
				if ((unsigned)lnn_allocated > INT_MAX / sizeof(struct line_info) - ALLOC_GR)
					overalloc();
				ftce = xrealloc(ftce, sizeof(struct format_text_cache_entry) - sizeof(struct line_info) + lnn_allocated * sizeof(struct line_info));
			}
			ftce->ln[lnn].st_offs = (int)(b - text_start);
			ftce->ln[lnn].en_offs = (int)(text - text_start);
			ftce->ln[lnn++].chars = xpos;
			b = text += sk;
			xpos = 0;
			last_space = NULL;
			continue;
		}
		if (*text == ' ') {
			last_space = text;
			last_space_xpos = xpos;
		}
		if (!wrap || xpos < width) {
			if (cp)
				text++;
			else FWD_UTF_8(text);
			xpos++;
			continue;
		}
		if (last_space) {
			text = last_space;
			xpos = last_space_xpos;
			if (wrap == 2) {
				unsigned char *s = last_space;
				*s = '\n';
				for (s++; *s; s++) if (*s == '\n') {
					if (s[1] != '\n') *s = ' ';
					break;
				}
			}
			sk = 1;
			goto put;
		}
		sk = 0;
		goto put;
	}
	if (ps < 1) {
		ps++;
		sk = 0;
		goto put;
	}
	ftce->n_lines = lnn;
	return ftce;
}

struct format_text_cache_entry *format_text(struct f_data_c *fd, struct form_control *fc, struct form_state *fs)
{
	int width = fc->cols;
	int wrap = fc->wrap;
	int cp = fd->f_data->opt.cp;
	struct format_text_cache_entry *ftce = fs->ftce;

	if (ftce && ftce->width == width && ftce->wrap == wrap && ftce->cp == cp)
		return fs->ftce;
	
	free_format_text_cache_entry(fs);

	ftce = format_text_uncached(fs->string, width, wrap, cp);
	fs->ftce = ftce;
	return ftce;
}

static int find_cursor_line(struct format_text_cache_entry *ftce, int state)
{
	int res;
#define LINE_EQ(x, key) (key >= ftce->ln[x].st_offs && (x >= ftce->n_lines - 1 || key < ftce->ln[x + 1].st_offs))
#define LINE_ABOVE(x, key) (key < ftce->ln[x].st_offs)
	BIN_SEARCH(ftce->n_lines, LINE_EQ, LINE_ABOVE, state, res);
#undef LINE_EQ
#undef LINE_ABOVE
	return res;
}

int area_cursor(struct f_data_c *f, struct form_control *fc, struct form_state *fs)
{
	struct format_text_cache_entry *ftce;
	int q = 0;
	int x, y;
	ftce = format_text(f, fc, fs);
	if (ftce->last_state == fs->state && ftce->last_vpos == fs->vpos && ftce->last_vypos == fs->vypos)
		return fs->ftce->last_cursor;
	y = find_cursor_line(ftce, fs->state);
	if (y >= 0) {
		x = textptr_diff(fs->string + fs->state, fs->string + ftce->ln[y].st_offs, f->f_data->opt.cp);
		if (fc->wrap && x == fc->cols) x--;

		if (x >= fc->cols + fs->vpos) fs->vpos = x - fc->cols + 1;
		if (x < fs->vpos) fs->vpos = x;

		if (fs->vypos > ftce->n_lines - fc->rows) {
			fs->vypos = ftce->n_lines - fc->rows;
			if (fs->vypos < 0) fs->vypos = 0;
		}

		if (y < fs->vypos) fs->vypos = y;
		x -= fs->vpos;
		y -= fs->vypos;
		q = y * fc->cols + x;
	}
	ftce->last_state = fs->state;
	ftce->last_vpos = fs->vpos;
	ftce->last_vypos = fs->vypos;
	ftce->last_cursor = q;
	return q;
}

static void draw_link(struct terminal *t, struct f_data_c *scr, int l)
{
	struct link *link = &scr->f_data->links[l];
	int xp = scr->xp;
	int yp = scr->yp;
	int xw = scr->xw;
	int yw = scr->yw;
	int vx, vy;
	struct view_state *vs = scr->vs;
	int f = 0;
	vx = vs->view_posx;
	vy = vs->view_pos;
	if (scr->link_bg) {
		internal("link background not empty");
		free(scr->link_bg);
	}
	if (l == -1) return;
	switch (link->type) {
		int i;
		int q;
		case L_LINK:
		case L_CHECKBOX:
		case L_BUTTON:
		case L_SELECT:
		case L_FIELD:
		case L_AREA:
			q = 0;
			if (link->type == L_FIELD) {
				struct form_state *fs = find_form_state(scr, link->form);
				q = textptr_diff(fs->string + fs->state, fs->string + fs->vpos, scr->f_data->opt.cp);
			} else if (link->type == L_AREA) {
				struct form_state *fs = find_form_state(scr, link->form);
				q = area_cursor(scr, link->form, fs);
			}
			if ((unsigned)link->n > INT_MAX / sizeof(struct link_bg)) overalloc();
			scr->link_bg = xmalloc(link->n * sizeof(struct link_bg));
			scr->link_bg_n = link->n;
			for (i = 0; i < link->n; i++) {
				int x = link->pos[i].x + xp - vx;
				int y = link->pos[i].y + yp - vy;
				if (x >= xp && y >= yp && x < xp+xw && y < yp+yw) {
					const chr *co;
					co = get_char(t, x, y);
					scr->link_bg[i].x = x;
					scr->link_bg[i].y = y;
					scr->link_bg[i].c = co->at;
					if (!f || (link->type == L_CHECKBOX && i == 1) || (link->type == L_BUTTON && i == 2) || ((link->type == L_FIELD || link->type == L_AREA) && i == q)) {
						int xx = x, yy = y;
						if (link->type != L_FIELD && link->type != L_AREA) {
							if ((unsigned)(co->at & 0x38) != (link->sel_color & 0x38)) {
								xx = xp + xw - 1;
								yy = yp + yw - 1;
							}
						}
						set_cursor(t, x, y, xx, yy);
						set_window_ptr(scr->ses->win, x, y);
						f = 1;
					}
					set_color(t, x, y, link->sel_color);
				} else {
					scr->link_bg[i].x = scr->link_bg[i].y = -1;
					scr->link_bg[i].c = 0;
				}
			}
			break;
		default: internal("bad link type");
	}
}

static void free_link(struct f_data_c *scr)
{
	free(scr->link_bg);
	scr->link_bg = NULL;
	scr->link_bg_n = 0;
}

static void clear_link(struct terminal *t, struct f_data_c *scr)
{
	if (scr->link_bg) {
		int i;
		for (i = scr->link_bg_n - 1; i >= 0; i--)
			set_color(t, scr->link_bg[i].x, scr->link_bg[i].y, scr->link_bg[i].c);
		free_link(scr);
	}
}

static struct search *search_lookup(struct f_data *f, int idx)
{
	static struct search sr;
	int result;
#define S_EQUAL(i, id) (f->search_pos[i].idx <= id && f->search_pos[i].idx + f->search_pos[i].co > id)
#define S_ABOVE(i, id) (f->search_pos[i].idx > id)
	BIN_SEARCH(f->nsearch_pos, S_EQUAL, S_ABOVE, idx, result)
	if (result == -1)
		internal("search_lookup: invalid index: %d, %d", idx, f->nsearch_chr);
	if (idx == f->search_pos[result].idx)
		return &f->search_pos[result];
	memcpy(&sr, &f->search_pos[result], sizeof(struct search));
	sr.x += idx - f->search_pos[result].idx;
	return &sr;
}

static int get_range(struct f_data *f, int y, int yw, int l, int *s1, int *s2)
{
	int i;
	*s1 = *s2 = -1;
	for (i = y < 0 ? 0 : y; i < y + yw && i < f->y; i++) {
		if (f->slines1[i] >= 0 && (*s1 < 0 || f->slines1[i] < *s1)) *s1 = f->slines1[i];
		if (f->slines2[i] >= 0 && (*s2 < 0 || f->slines2[i] > *s2)) *s2 = f->slines2[i];
	}

	if (l > f->nsearch_chr) *s1 = *s2 = -1;
	if (*s1 < 0 || *s2 < 0) return -1;

	if (*s1 < l) *s1 = 0;
	else *s1 -= l;

	if (f->nsearch_chr - *s2 < l) *s2 = f->nsearch_chr - l;

	if (*s1 > *s2) *s1 = *s2 = -1;
	if (*s1 < 0 || *s2 < 0) return -1;

	return 0;
}

static int is_in_range(struct f_data *f, int y, int yw, unsigned char *txt, int *min, int *max)
{
	int utf8 = f->opt.cp == 0;
	int found = 0;
	int l;
	int s1, s2;
	*min = INT_MAX;
	*max = 0;

	l = strlen((char *)txt);

	if (get_range(f, y, yw, l, &s1, &s2))
		return 0;
	for (; s1 <= s2; s1++) {
		int i;
		if (!utf8) {
			if (f->search_chr[s1] != txt[0])
				continue;
			for (i = 1; i < l; i++)
				if (f->search_chr[s1 + i] != txt[i])
					goto cont;
		} else {
			unsigned char *tt = txt;
			for (i = 0; i < l; i++) {
				unsigned cc;
				GET_UTF_8(tt, cc);
				if (f->search_chr[s1 + i] != cc)
					goto cont;
			}
		}
		for (i = 0; i < l; i++) {
			struct search *sr = search_lookup(f, s1 + i);
			if (sr->y >= y && sr->y < y + yw && sr->n) goto in_view;
		}
		continue;
in_view:
		found = 1;
		for (i = 0; i < l; i++) {
			struct search *sr = search_lookup(f, s1 + i);
			if (sr->n) {
				if (sr->x < *min)
					*min = sr->x;
				if (sr->x + sr->n > *max)
					*max = sr->x + sr->n;
			}
		}
cont:;
	}
	return found;
}

static int get_searched(struct f_data_c *scr, struct point **pt, int *pl)
{
	int utf8 = term_charset(scr->ses->term) == 0;
	struct f_data *f = scr->f_data;
	int xp = scr->xp;
	int yp = scr->yp;
	int xw = scr->xw;
	int yw = scr->yw;
	int vx = scr->vs->view_posx;
	int vy = scr->vs->view_pos;
	int s1, s2;
	int l;
	unsigned c;
	struct point *points = NULL;
	int len = 0;
	unsigned char *ww;
	unsigned char *w = scr->ses->search_word;
	if (!w || !*w)
		return -1;
	if (get_search_data(f) < 0) {
		free(scr->ses->search_word);
		scr->ses->search_word = NULL;
		return -1;
	}
	l = strlen((char *)w);
	ww = w;
	GET_UTF_8(ww, c);
	if (get_range(f, scr->vs->view_pos, scr->yw, l, &s1, &s2))
		goto ret;
	for (; s1 <= s2; s1++) {
		int i, j;
		if (f->search_chr[s1] != c) {
c:
			continue;
		}
		if (!utf8) {
			for (i = 1; i < l; i++)
				if (f->search_chr[s1 + i] != w[i])
					goto c;
		} else {
			ww = w;
			for (i = 0; i < l; i++) {
				unsigned cc;
				GET_UTF_8(ww, cc);
				if (f->search_chr[s1 + i] != cc)
					goto c;
			}
		}
		for (i = 0; i < l; i++) {
			struct search *sr = search_lookup(f, s1 + i);
			for (j = 0; j < sr->n; j++) {
				int x = sr->x + j + xp - vx;
				int y = sr->y + yp - vy;
				if (x >= xp && y >= yp && x < xp + xw && y < yp + yw) {
					if (!(len & (ALLOC_GR - 1))) {
						struct point *points2;
						if ((unsigned)len > INT_MAX / sizeof(struct point) - ALLOC_GR)
							goto ret;
						points2 = xrealloc(points,
								sizeof(struct point) * (len + ALLOC_GR));
						if (!points2)
							goto ret;
						points = points2;
					}
					points[len].x = sr->x + j;
					points[len++].y = sr->y;
				}
			}
		}
	}
ret:
	*pt = points;
	*pl = len;
	return 0;
}

static void draw_searched(struct terminal *t, struct f_data_c *scr)
{
	int xp = scr->xp;
	int yp = scr->yp;
	int vx = scr->vs->view_posx;
	int vy = scr->vs->view_pos;
	struct point *pt;
	int len, i;
	if (get_searched(scr, &pt, &len) < 0) return;
	for (i = 0; i < len; i++) {
		int x = pt[i].x + xp - vx, y = pt[i].y + yp - vy;
		const chr *co;
		unsigned char nco;
		co = get_char(t, x, y);
		nco = ((co->at >> 3) & 0x07) | ((co->at << 3) & 0x38);
		set_color(t, x, y, nco);
	}
	free(pt);
}

static void draw_current_link(struct terminal *t, struct f_data_c *scr)
{
	draw_link(t, scr, scr->vs->current_link);
	draw_searched(t, scr);
}

static struct link *get_first_link(struct f_data_c *f)
{
	int i;
	struct link *l = f->f_data->links + f->f_data->nlinks;
	for (i = f->vs->view_pos; i < f->vs->view_pos + f->yw; i++)
		if (i >= 0 && i < f->f_data->y && f->f_data->lines1[i] && f->f_data->lines1[i] < l)
			l = f->f_data->lines1[i];
	if (l == f->f_data->links + f->f_data->nlinks) l = NULL;
	return l;
}

static struct link *get_last_link(struct f_data_c *f)
{
	int i;
	struct link *l = NULL;
	for (i = f->vs->view_pos; i < f->vs->view_pos + f->yw; i++)
		if (i >= 0 && i < f->f_data->y && f->f_data->lines2[i] && (!l || f->f_data->lines2[i] > l))
			l = f->f_data->lines2[i];
	return l;
}

void fixup_select_state(struct form_control *fc, struct form_state *fs)
{
	int inited = 0;
	int i;
	retry:
	if (fs->state >= 0 && fs->state < fc->nvalues && !strcmp(cast_const_char fc->values[fs->state], cast_const_char fs->string)) return;
	for (i = 0; i < fc->nvalues; i++) {
		if (!strcmp(cast_const_char fc->values[i], cast_const_char fs->string)) {
			fs->state = i;
			return;
		}
	}
	if (!inited) {
		init_ctrl(fc, fs);
		inited = 1;
		goto retry;
	}
	fs->state = 0;
	free(fs->string);
	if (fc->nvalues) fs->string = stracpy(fc->values[0]);
	else fs->string = stracpy(cast_uchar "");
}

static void init_ctrl(struct form_control *form, struct form_state *fs)
{
	free(fs->string);
	fs->string = NULL;
	switch (form->type) {
		case FC_TEXT:
		case FC_PASSWORD:
		case FC_TEXTAREA:
			fs->string = stracpy(form->default_value);
			fs->state = (int)strlen(cast_const_char form->default_value);
			fs->vpos = 0;
			break;
		case FC_FILE_UPLOAD:
			fs->string = stracpy(cast_uchar "");
			fs->state = 0;
			fs->vpos = 0;
			break;
		case FC_CHECKBOX:
		case FC_RADIO:
			fs->state = form->default_state;
			break;
		case FC_SELECT:
			fs->string = stracpy(form->default_value);
			fs->state = form->default_state;
			fixup_select_state(form, fs);
			break;
	}
}

struct form_state *find_form_state(struct f_data_c *f, struct form_control *form)
{
	struct view_state *vs = f->vs;
	struct form_state *fs;
	int n = form->g_ctrl_num;
	if (n < vs->form_info_len)
		fs = &vs->form_info[n];
	else {
		if ((unsigned)n > INT_MAX / sizeof(struct form_state) - 1)
			overalloc();
		fs = xrealloc(vs->form_info, (n + 1) * sizeof(struct form_state));
		vs->form_info = fs;
		memset(fs + vs->form_info_len, 0, (n + 1 - vs->form_info_len) * sizeof(struct form_state));
		vs->form_info_len = n + 1;
		fs = &vs->form_info[n];
	}
	if (fs->form_num == form->form_num && fs->ctrl_num == form->ctrl_num && fs->g_ctrl_num == form->g_ctrl_num && /*fs->position == form->position &&*/ fs->type == form->type) return fs;
	free_form_state(fs);
	memset(fs, 0, sizeof(struct form_state));
	fs->form_num = form->form_num;
	fs->ctrl_num = form->ctrl_num;
	fs->g_ctrl_num = form->g_ctrl_num;
	fs->position = form->position;
	fs->type = form->type;
	init_ctrl(form, fs);
	return fs;
}

static void draw_form_entry(struct terminal *t, struct f_data_c *f, struct link *l)
{
	int xp = f->xp;
	int yp = f->yp;
	int xw = f->xw;
	int yw = f->yw;
	struct view_state *vs = f->vs;
	int vx = vs->view_posx;
	int vy = vs->view_pos;
	struct form_state *fs;
	struct form_control *form = l->form;
	int i, x, y, td;
	size_t sl;

	if (!form) {
		internal("link %d has no form", (int)(l - f->f_data->links));
		return;
	}
	fs = find_form_state(f, form);
	switch (form->type) {
		unsigned char *s;
		struct format_text_cache_entry *ftce;
		int lid;

		case FC_TEXT:
		case FC_PASSWORD:
		case FC_FILE_UPLOAD:
			if ((size_t)fs->vpos > strlen(cast_const_char fs->string)) fs->vpos = (int)strlen(cast_const_char fs->string);
			sl = strlen((char *)fs->string);
			td = textptr_diff(fs->string + fs->state, fs->string + fs->vpos, f->f_data->opt.cp);

			while (fs->vpos < sl && td >= form->size) {
				unsigned char *p = fs->string + fs->vpos;
				FWD_UTF_8(p);
				fs->vpos = (int)(p - fs->string);
				td--;
			}
			while (fs->vpos > fs->state) {
				unsigned char *p = fs->string + fs->vpos;
				BACK_UTF_8(p, fs->string);
				fs->vpos = (int)(p - fs->string);
			}
			if (!l->n) break;
			x = l->pos[0].x + xp - vx; y = l->pos[0].y + yp - vy;
			s = fs->string + fs->vpos;
			for (i = 0; i < form->size; i++, x++) {
				unsigned ch;
				if (!*s) {
					ch = '_';
				} else {
					if (f->f_data->opt.cp) {
						ch = *s++;
					} else {
						GET_UTF_8(s, ch);
					}
					if (form->type == FC_PASSWORD) {
						ch = '*';
					}
				}
				if (x >= xp && y >= yp && x < xp+xw && y < yp+yw) {
					set_only_char(t, x, y, ch, 0);
				}
			}
			break;
		case FC_TEXTAREA:
			if (!l->n) break;
			x = l->pos[0].x + xp - vx; y = l->pos[0].y + yp - vy;
			area_cursor(f, form, fs);
			ftce = format_text(f, form, fs);
			lid = fs->vypos;
			for (; lid < ftce->n_lines && y < l->pos[0].y + yp - vy + form->rows; lid++, y++) {
				s = textptr_add(fs->string, ftce->ln[lid].st_offs, f->f_data->opt.cp);
				for (i = 0; i < form->cols; i++) {
					unsigned ch;
					if (s >= fs->string + ftce->ln[lid].en_offs) {
						ch = '_';
					} else {
						if (f->f_data->opt.cp) {
							ch = *s++;
						} else {
							GET_UTF_8(s, ch);
						}
					}
					if (x+i >= xp && y >= yp && x+i < xp+xw && y < yp+yw) {
						set_only_char(t, x+i, y, ch, 0);
					}
				}
			}
			for (; y < l->pos[0].y + yp - vy + form->rows; y++) {
				for (i = 0; i < form->cols; i++) {
					if (x+i >= xp && y >= yp && x+i < xp+xw && y < yp+yw)
						set_only_char(t, x+i, y, '_', 0);
				}
			}

			break;
		case FC_CHECKBOX:
			if (l->n < 2) break;
			x = l->pos[1].x + xp - vx;
			y = l->pos[1].y + yp - vy;
			if (x >= xp && y >= yp && x < xp+xw && y < yp+yw)
				set_only_char(t, x, y, fs->state ? 'X' : ' ', 0);
			break;
		case FC_RADIO:
			if (l->n < 2) break;
			x = l->pos[1].x + xp - vx;
			y = l->pos[1].y + yp - vy;
			if (x >= xp && y >= yp && x < xp+xw && y < yp+yw)
				set_only_char(t, x, y, fs->state ? 'X' : ' ', 0);
			break;
		case FC_SELECT:
			fixup_select_state(form, fs);
			s = fs->state < form->nvalues ? form->labels[fs->state] : NULL;
			if (!s) s = cast_uchar "";
			for (i = 0; i < l->n; i++) {
				unsigned chr;
				if (!*s) {
					chr = '_';
				} else {
					if (!term_charset(t)) {
						GET_UTF_8(s, chr);
					} else
					chr = *s++;
				}
				x = l->pos[i].x + xp - vx;
				y = l->pos[i].y + yp - vy;
				if (x >= xp && y >= yp && x < xp+xw && y < yp+yw)
					set_only_char(t, x, y, chr, 0);
			}
			break;
		case FC_SUBMIT:
		case FC_IMAGE:
		case FC_RESET:
		case FC_HIDDEN:
		case FC_BUTTON:
			break;
	}
}

struct xdfe {
	struct f_data_c *f;
	struct link *l;
};

static void y_draw_form_entry(struct terminal *t, void *x_)
{
	struct xdfe *x = (struct xdfe *)x_;
	draw_form_entry(t, x->f, x->l);
}

static void x_draw_form_entry(struct session *ses, struct f_data_c *f, struct link *l)
{
	struct xdfe x;
	x.f = f;
	x.l = l;
	draw_to_window(ses->win, y_draw_form_entry, &x);
}

static void draw_forms(struct terminal *t, struct f_data_c *f)
{
	struct link *l1 = get_first_link(f);
	struct link *l2 = get_last_link(f);
	if (!l1 || !l2) {
		if (l1 || l2) internal("get_first_link == %p, get_last_link == %p", (void *)l1, (void *)l2);
		return;
	}
	do {
		if (l1->type != L_LINK) draw_form_entry(t, f, l1);
	} while (l1++ < l2);
}

/* 0 -> 1 <- 2 v 3 ^ */

static unsigned char fr_trans[2][4] = {{0xb3, 0xc3, 0xb4, 0xc5}, {0xc4, 0xc2, 0xc1, 0xc5}};

static void set_xchar(struct terminal *t, int x, int y, unsigned dir)
{
	const chr *co;
	if (x < 0 || x >= t->x || y < 0 || y >= t->y) return;
	co = get_char(t, x, y);
	if (!(co->at & ATTR_FRAME)) return;
	if (co->ch == fr_trans[dir / 2][0]) set_only_char(t, x, y, fr_trans[dir / 2][1 + (dir & 1)], ATTR_FRAME);
	else if (co->ch == fr_trans[dir / 2][2 - (dir & 1)]) set_only_char(t, x, y, fr_trans[dir / 2][3], ATTR_FRAME);
}

static void draw_frame_lines(struct session *ses, struct frameset_desc *fsd, int xp, int yp)
{
	struct terminal *t = ses->term;
	int i, j;
	int x, y;
	if (!fsd) return;
	y = yp - 1;
	for (j = 0; j < fsd->y; j++) {
		int wwy = fsd->f[j * fsd->x].yw;
		x = xp - 1;
		for (i = 0; i < fsd->x; i++) {
			int wwx = fsd->f[i].xw;
			if (i) {
				fill_area(t, x, y + 1, 1, wwy, 179, ATTR_FRAME | get_session_attribute(ses, 0));
				if (j == fsd->y - 1) set_xchar(t, x, y + wwy + 1, 3);
			} else if (j) set_xchar(t, x, y, 0);
			if (j) {
				fill_area(t, x + 1, y, wwx, 1, 196, ATTR_FRAME | get_session_attribute(ses, 0));
				if (i == fsd->x - 1) set_xchar(t, x + wwx + 1, y, 1);
			} else if (i) set_xchar(t, x, y, 2);
			if (i && j) set_char(t, x, y, 197, ATTR_FRAME | get_session_attribute(ses, 0));
			/*if (fsd->f[j * fsd->x + i].subframe) {
				draw_frame_lines(ses, fsd->f[j * fsd->x + i].subframe, x + 1, y + 1);
			}*/
			x += wwx + 1;
		}
		y += wwy + 1;
	}
}

void draw_doc(struct terminal *t, void *scr_)
{
	struct f_data_c *scr = (struct f_data_c *)scr_;
	struct session *ses = scr->ses;
	int active = scr->active;
	int y;
	int xp = scr->xp;
	int yp = scr->yp;
	int xw = scr->xw;
	int yw = scr->yw;
	struct view_state *vs;
	int vx, vy;
	if (!scr->vs || !scr->f_data) {
		if (!F) {
			if (active) {
				if (!scr->parent) set_cursor(t, 0, 0, 0, 0);
				else set_cursor(t, xp, yp, xp, yp);
			}
			fill_area(t, xp, yp, xw, yw, ' ', get_session_attribute(ses, 0));
#ifdef G
		} else {
			long color = dip_get_color_sRGB(ses->ds.g_background_color /* 0x808080 */);
			drv->fill_area(t->dev, xp, yp, xp + xw, yp + yw, color);
#endif
		}
		if (active) set_window_ptr(ses->win, xp, yp);
		return;
	}
	if (active) {
		if (!F) {
			set_cursor(t, xp + xw - 1, yp + yw - 1, xp + xw - 1, yp + yw - 1);
			set_window_ptr(ses->win, xp, yp);
		}
	}
	check_vs(scr);
	if (scr->f_data->frame_desc) {
		struct f_data_c *f = NULL;
		struct list_head *lf;
		int n;
		if (!F) {
			fill_area(t, xp, yp, xw, yw, ' ', scr->f_data->y ? scr->f_data->bg : 0);
			draw_frame_lines(ses, scr->f_data->frame_desc, xp, yp);
		}
		n = 0;
		foreach(struct f_data_c, f, lf, scr->subframes) {
			f->active = active && n++ == scr->vs->frame_pos;
			draw_doc(t, f);
		}
		return;
	}
	vs = scr->vs;
	if (scr->goto_position && (vy = find_tag(scr->f_data, scr->goto_position)) != -1) {
		if (vy > scr->f_data->y) vy = scr->f_data->y - 1;
		if (vy < 0) vy = 0;
		vs->view_pos = vy;
		vs->orig_view_pos = vy;
		vs->view_posx = 0;
		vs->orig_view_posx = 0;
		if (!F) set_link(scr);
		free(scr->went_to_position);
		scr->went_to_position = scr->goto_position;
		scr->goto_position = NULL;
	}
	if (vs->view_pos != vs->orig_view_pos
	|| vs->view_posx != vs->orig_view_posx
	|| vs->current_link != vs->orig_link) {
		int ol;
		vs->view_pos = vs->orig_view_pos;
		vs->view_posx = vs->orig_view_posx;
		vs->brl_x = vs->orig_brl_x;
		vs->brl_y = vs->orig_brl_y;
		ol = vs->orig_link;
		if (ol < scr->f_data->nlinks) vs->current_link = ol;
		if (!F) {
			while (vs->view_pos >= scr->f_data->y) vs->view_pos -= yw ? yw : 1;
			if (vs->view_pos < 0) vs->view_pos = 0;
		}
		if (!F)
			set_link(scr);
		check_vs(scr);
		vs->orig_link = ol;
	}
	if (!F) {
		vx = vs->view_posx;
		vy = vs->view_pos;
		if (scr->xl == vx && scr->yl == vy && scr->xl != -1 && !ses->search_word) {
			clear_link(t, scr);
			draw_forms(t, scr);
			if (active) draw_current_link(t, scr);
			return;
		}
		free_link(scr);
		scr->xl = vx;
		scr->yl = vy;
		fill_area(t, xp, yp, xw, yw, ' ', scr->f_data->y ? scr->f_data->bg : get_session_attribute(ses, 0));
		if (!scr->f_data->y) return;
		while (vs->view_pos >= scr->f_data->y) vs->view_pos -= yw ? yw : 1;
		if (vs->view_pos < 0) vs->view_pos = 0;
		if (vy != vs->view_pos) {
			vy = vs->view_pos;
			check_vs(scr);
		}
		for (y = vy <= 0 ? 0 : vy; y < (-vy + scr->f_data->y <= yw ? scr->f_data->y : yw + vy); y++) {
			int st = vx <= 0 ? 0 : vx;
			int en = -vx + scr->f_data->data[y].l <= xw ? scr->f_data->data[y].l : xw + vx;
			set_line(t, xp + st - vx, yp + y - vy, en - st, &scr->f_data->data[y].d[st]);
		}
		draw_forms(t, scr);
		if (active) draw_current_link(t, scr);
		if (ses->search_word) scr->xl = scr->yl = -1;
#ifdef G
	} else {
		draw_graphical_doc(t, scr, active);
#endif
	}
}

static void clr_xl(struct f_data_c *fd)
{
	struct f_data_c *fdd = NULL;
	struct list_head *lfdd;
	fd->xl = fd->yl = -1;
	foreach(struct f_data_c, fdd, lfdd, fd->subframes) clr_xl(fdd);
}

static void draw_doc_c(struct terminal *t, void *scr_)
{
	struct f_data_c *scr = (struct f_data_c *)scr_;
	clr_xl(scr);
#ifdef G
	if (F) if (scr == scr->ses->screen) draw_title(scr);
#endif
	draw_doc(t, scr);
}

void draw_formatted(struct session *ses)
{
	/*clr_xl(ses->screen);*/
	ses->screen->active = 1;
	draw_to_window(ses->win, draw_doc_c, ses->screen);
	change_screen_status(ses);
	print_screen_status(ses);
}

void draw_fd(struct f_data_c *f)
{
	if (f->f_data) f->f_data->time_to_draw = -get_time();
	f->active = is_active_frame(f->ses, f);
	draw_to_window(f->ses->win, draw_doc_c, f);
	change_screen_status(f->ses);
	print_screen_status(f->ses);
	if (f->f_data) f->f_data->time_to_draw += get_time();
}

static void draw_fd_nrd(struct f_data_c *f)
{
	f->active = is_active_frame(f->ses, f);
	draw_to_window(f->ses->win, draw_doc, f);
	change_screen_status(f->ses);
	print_screen_status(f->ses);
}

#define D_BUF	65536

int dump_to_file(struct f_data *fd, int h)
{
	int x, y;
	unsigned char *buf;
	int bptr = 0;
	int retval;
	buf = xmalloc(D_BUF);
	for (y = 0; y < fd->y; y++) for (x = 0; x <= fd->data[y].l; x++) {
		unsigned c;
		if (x == fd->data[y].l) c = '\n';
		else {
			c = fd->data[y].d[x].ch;
			if (c == 1) c = ' ';
			if (fd->data[y].d[x].at & ATTR_FRAME && c >= 176 && c < 224) c = frame_dumb[c - 176];
		}
		if (!fd->opt.cp && c >= 0x80) {
			unsigned char *enc = encode_utf_8(c);
			strcpy(cast_char(buf + bptr), cast_const_char enc);
			bptr += (int)strlen(cast_const_char enc);
		} else
			buf[bptr++] = (unsigned char)c;
		if (bptr >= D_BUF - 7) {
			if ((retval = hard_write(h, buf, bptr)) != bptr) {
				free(buf);
				goto fail;
			}
			bptr = 0;
		}
	}
	if ((retval = hard_write(h, buf, bptr)) != bptr) {
		free(buf);
		goto fail;
	}
	free(buf);
	if (fd->opt.num_links && fd->nlinks) {
		static const unsigned char head[] = "\nLinks:\n";
		int i;
		if ((retval = hard_write(h, head, (int)strlen(cast_const_char head))) != (int)strlen(cast_const_char head))
			goto fail;
		for (i = 0; i < fd->nlinks; i++) {
			struct form_control *fc;
			struct link *lnk = &fd->links[i];
			unsigned char *s = init_str();
			int l = 0;
			add_num_to_str(&s, &l, i + 1);
			add_to_str(&s, &l, cast_uchar ". ");
			if (lnk->where) {
				add_to_str(&s, &l, lnk->where);
			} else if (lnk->where_img) {
				add_to_str(&s, &l, cast_uchar "Image: ");
				add_to_str(&s, &l, lnk->where_img);
			} else if (lnk->type == L_BUTTON) {
				fc = lnk->form;
				if (fc->type == FC_RESET) add_to_str(&s, &l, cast_uchar "Reset form");
				else if (fc->type == FC_BUTTON || !fc->action) add_to_str(&s, &l, cast_uchar "Button");
				else {
					if (fc->method == FM_GET) add_to_str(&s, &l, cast_uchar "Submit form: ");
					else add_to_str(&s, &l, cast_uchar "Post form: ");
					add_to_str(&s, &l, fc->action);
				}
			} else if (lnk->type == L_CHECKBOX || lnk->type == L_SELECT || lnk->type == L_FIELD || lnk->type == L_AREA) {
				fc = lnk->form;
				switch (fc->type) {
				case FC_RADIO:
					add_to_str(&s, &l, cast_uchar "Radio button");
					break;
				case FC_CHECKBOX:
					add_to_str(&s, &l, cast_uchar "Checkbox");
					break;
				case FC_SELECT:
					add_to_str(&s, &l, cast_uchar "Select field");
					break;
				case FC_TEXT:
					add_to_str(&s, &l, cast_uchar "Text field");
					break;
				case FC_TEXTAREA:
					add_to_str(&s, &l, cast_uchar "Text area");
					break;
				case FC_FILE_UPLOAD:
					add_to_str(&s, &l, cast_uchar "File upload");
					break;
				case FC_PASSWORD:
					add_to_str(&s, &l, cast_uchar "Password field");
					break;
				default:
					goto unknown;
				}
				if (fc->name && fc->name[0]) {
					add_to_str(&s, &l, cast_uchar ", Name ");
					add_to_str(&s, &l, fc->name);
				}
				if ((fc->type == FC_CHECKBOX || fc->type == FC_RADIO)
				&& fc->default_value
				&& fc->default_value[0]) {
					add_to_str(&s, &l, cast_uchar ", Value ");
					add_to_str(&s, &l, fc->default_value);
				}
			}
			unknown:
			add_to_str(&s, &l, cast_uchar "\n");
			if ((retval = hard_write(h, s, l)) != l) {
				free(s);
				goto fail;
			}
			free(s);
		}
	}
	return 0;

 fail:
	if (retval < 0)
		return get_error_from_errno(errno);
	else
		return S_CANT_WRITE;
}

static int in_viewx(struct f_data_c *f, struct link *l)
{
	int i;
	for (i = 0; i < l->n; i++) {
		if (l->pos[i].x >= f->vs->view_posx && l->pos[i].x < f->vs->view_posx + f->xw)
			return 1;
	}
	return 0;
}

static int in_viewy(struct f_data_c *f, struct link *l)
{
	int i;
	for (i = 0; i < l->n; i++) {
		if (l->pos[i].y >= f->vs->view_pos && l->pos[i].y < f->vs->view_pos + f->yw)
			return 1;
	}
	return 0;
}

static int in_view(struct f_data_c *f, struct link *l)
{
	return in_viewy(f, l) && in_viewx(f, l);
}

static int c_in_view(struct f_data_c *f)
{
	return f->vs->current_link != -1 && in_view(f, &f->f_data->links[f->vs->current_link]);
}

static int next_in_view(struct f_data_c *f, int p, int d, int (*fn)(struct f_data_c *, struct link *), void (*cntr)(struct f_data_c *, struct link *))
{
	int p1 = f->f_data->nlinks - 1;
	int p2 = 0;
	int y;
	int yl = f->vs->view_pos + f->yw;
	if (yl > f->f_data->y) yl = f->f_data->y;
	for (y = f->vs->view_pos < 0 ? 0 : f->vs->view_pos; y < yl; y++) {
		if (f->f_data->lines1[y] && f->f_data->lines1[y] - f->f_data->links < p1)
			p1 = (int)(f->f_data->lines1[y] - f->f_data->links);
		if (f->f_data->lines2[y] && f->f_data->lines2[y] - f->f_data->links > p2)
			p2 = (int)(f->f_data->lines2[y] - f->f_data->links);
	}
	while (p >= p1 && p <= p2) {
		if (fn(f, &f->f_data->links[p])) {
			f->vs->current_link = p;
			f->vs->orig_link = f->vs->current_link;
			if (cntr) cntr(f, &f->f_data->links[p]);
			return 1;
		}
		p += d;
	}
	f->vs->current_link = -1;
	f->vs->orig_link = f->vs->current_link;
	return 0;
}

static void set_pos_x(struct f_data_c *f, struct link *l)
{
	int i;
	int xm = 0;
	int xl = INT_MAX;
	for (i = 0; i < l->n; i++) {
		if (l->pos[i].y >= f->vs->view_pos && l->pos[i].y < f->vs->view_pos + f->yw) {
			if (l->pos[i].x >= xm) xm = l->pos[i].x + 1;
			if (l->pos[i].x < xl) xl = l->pos[i].x;
		}
	}
	if (xl == INT_MAX) return;
	if (f->vs->view_posx + f->xw < xm) f->vs->view_posx = xm - f->xw;
	if (f->vs->view_posx > xl) f->vs->view_posx = xl;
	f->vs->orig_view_posx = f->vs->view_posx;
}

static void set_pos_y(struct f_data_c *f, struct link *l)
{
	int i;
	int ym = 0;
	int yl = f->f_data->y;
	for (i = 0; i < l->n; i++) {
		if (l->pos[i].y >= ym) ym = l->pos[i].y + 1;
		if (l->pos[i].y < yl) yl = l->pos[i].y;
	}
	if ((f->vs->view_pos = (ym + yl) / 2 - f->yw / 2) > f->f_data->y - f->yw)
		f->vs->view_pos = f->f_data->y - f->yw;
	if (f->vs->view_pos < 0) f->vs->view_pos = 0;
	f->vs->orig_view_pos = f->vs->view_pos;
}

static void find_link(struct f_data_c *f, int p, int s)
{ /* p=1 - top, p=-1 - bottom, s=0 - pgdn, s=1 - down */
	int y;
	int l;
	struct link *link;
	struct link **line;
	line = p == -1 ? f->f_data->lines2 : f->f_data->lines1;
	if (p == -1) {
		y = f->vs->view_pos + f->yw - 1;
		if (y >= f->f_data->y) y = f->f_data->y - 1;
	} else {
		y = f->vs->view_pos;
		if (y < 0) y = 0;
	}
	if (y < 0 || y >= f->f_data->y) goto nolink;
	link = NULL;
	do {
		if (line[y] && (!link || (p > 0 ? line[y] < link : line[y] > link)))
			link = line[y];
		y += p;
	} while (!(y < 0 || y < f->vs->view_pos || y >= f->vs->view_pos + f->yw || y >= f->f_data->y));
	if (!link) goto nolink;
	l = (int)(link - f->f_data->links);
	if (s == 0) {
		next_in_view(f, l, p, in_view, NULL);
		return;
	}
	f->vs->current_link = l;
	f->vs->orig_link = f->vs->current_link;
	set_pos_x(f, link);
	return;
	nolink:
	f->vs->current_link = -1;
	f->vs->orig_link = f->vs->current_link;
}

static void page_down(struct session *ses, struct f_data_c *f, int a)
{
	if (f->vs->view_pos + f->yw < f->f_data->y) {
		f->vs->view_pos += f->yw;
		f->vs->orig_view_pos = f->vs->view_pos;
		find_link(f, 1, a);
	} else
		find_link(f, -1, a);
}

static void page_up(struct session *ses, struct f_data_c *f, int a)
{
	f->vs->view_pos -= f->yw;
	find_link(f, -1, a);
	if (f->vs->view_pos < 0) {
		f->vs->view_pos = 0;
	}
	f->vs->orig_view_pos = f->vs->view_pos;
}

static void down(struct session *ses, struct f_data_c *f, int a)
{
	int l = f->vs->current_link;
	if (f->vs->current_link == -1 || !next_in_view(f, f->vs->current_link+1, 1, in_viewy, set_pos_x)) page_down(ses, f, 1);
	if (l != f->vs->current_link) set_textarea(ses, f, -1);
}

static void up(struct session *ses, struct f_data_c *f, int a)
{
	int l = f->vs->current_link;
	if (f->vs->current_link == -1 || !next_in_view(f, f->vs->current_link-1, -1, in_viewy, set_pos_x)) page_up(ses, f, 1);
	if (l != f->vs->current_link) set_textarea(ses, f, 1);
}

static void scroll(struct session *ses, struct f_data_c *f, int a)
{
	if (f->vs->view_pos + f->yw >= f->f_data->y && a > 0) return;
	f->vs->view_pos += a;
	if (f->vs->view_pos > f->f_data->y - f->yw && a > 0) f->vs->view_pos = f->f_data->y - f->yw;
	if (f->vs->view_pos < 0) f->vs->view_pos = 0;
	f->vs->orig_view_pos = f->vs->view_pos;
	if (c_in_view(f)) return;
	find_link(f, a < 0 ? -1 : 1, 0);
}

static void hscroll(struct session *ses, struct f_data_c *f, int a)
{
	f->vs->view_posx += a;
	if (f->vs->view_posx >= f->f_data->x) f->vs->view_posx = f->f_data->x - 1;
	if (f->vs->view_posx < 0) f->vs->view_posx = 0;
	f->vs->orig_view_posx = f->vs->view_posx;
	if (c_in_view(f)) return;
	find_link(f, 1, 0);
	/* !!! FIXME: check right margin */
}

static void home(struct session *ses, struct f_data_c *f, int a)
{
	f->vs->view_pos = f->vs->view_posx = 0;
	f->vs->orig_view_pos = f->vs->view_pos;
	f->vs->orig_view_posx = f->vs->view_posx;
	find_link(f, 1, 0);
}

static void x_end(struct session *ses, struct f_data_c *f, int a)
{
	f->vs->view_posx = 0;
	if (f->vs->view_pos < f->f_data->y - f->yw) f->vs->view_pos = f->f_data->y - f->yw;
	if (f->vs->view_pos < 0) f->vs->view_pos = 0;
	f->vs->orig_view_pos = f->vs->view_pos;
	f->vs->orig_view_posx = f->vs->view_posx;
	find_link(f, -1, 0);
}

static int has_form_submit(struct f_data *f, struct form_control *form)
{
	struct form_control *i = NULL;
	struct list_head *li;
	int q = 0;
	foreach (struct form_control, i, li, f->forms) if (i->form_num == form->form_num) {
		if ((i->type == FC_SUBMIT || i->type == FC_IMAGE)) return 1;
		q = 1;
	}
	if (!q) internal("form is not on list");
	return 0;
}

struct submitted_value {
	list_entry_1st
	int type;
	unsigned char *name;
	unsigned char *value;
	void *file_content;
	int fc_len;
	int position;
	list_entry_last
};

static void free_succesful_controls(struct list_head *submit)
{
	struct submitted_value *v = NULL;
	struct list_head *lv;
	foreach(struct submitted_value, v, lv, *submit) {
		free(v->name);
		free(v->value);
		free(v->file_content);
	}
	free_list(struct submitted_value, *submit);
}

static unsigned char *encode_textarea(unsigned char *t)
{
	int len = 0;
	unsigned char *o = init_str();
	for (; *t; t++) {
		if (*t != '\n') add_chr_to_str(&o, &len, *t);
		else add_to_str(&o, &len, cast_uchar "\r\n");
	}
	return o;
}

static int compare_submitted(struct submitted_value *sub1, struct submitted_value *sub2)
{
	return sub1->position - sub2->position;
}

static void get_succesful_controls(struct f_data_c *f, struct form_control *fc, struct list_head *subm)
{
	int ch;
	struct form_control *form = NULL;
	struct list_head *lform;
	init_list(*subm);
	foreach(struct form_control, form, lform, f->f_data->forms) {
		if (form->form_num == fc->form_num && ((form->type != FC_SUBMIT && form->type != FC_IMAGE && form->type != FC_RESET && form->type != FC_BUTTON) || form == fc) && form->name && form->name[0] && form->ro != 2) {
			struct submitted_value *sub;
			struct form_state *fs;
			int fi = form->type == FC_IMAGE && form->default_value && *form->default_value ? -1 : 0;
			int svl;
			fs = find_form_state(f, form);
			if ((form->type == FC_CHECKBOX || form->type == FC_RADIO) && !fs->state) continue;
			if (form->type == FC_BUTTON) continue;
			if (form->type == FC_SELECT && !form->nvalues) continue;
			fi_rep:
			sub = mem_calloc(sizeof(struct submitted_value));
			sub->type = form->type;
			sub->name = stracpy(form->name);
			switch (form->type) {
			case FC_TEXT:
			case FC_PASSWORD:
			case FC_FILE_UPLOAD:
				sub->value = stracpy(fs->string);
				break;
			case FC_TEXTAREA:
				sub->value = encode_textarea(fs->string);
				break;
			case FC_CHECKBOX:
			case FC_RADIO:
			case FC_SUBMIT:
			case FC_HIDDEN:
				sub->value = encode_textarea(form->default_value);
				break;
			case FC_SELECT:
				fixup_select_state(form, fs);
				sub->value = encode_textarea(fs->string);
				break;
			case FC_IMAGE:
				if (fi == -1) {
					sub->value = encode_textarea(form->default_value);
					break;
				}
				add_to_strn(&sub->name, fi ? cast_uchar ".x" : cast_uchar ".y");
				sub->value = init_str();
				svl = 0;
				add_num_to_str(&sub->value, &svl, fi ? ismap_x : ismap_y);
				break;
			default:
				internal("bad form control type");
			}
			sub->position = form->form_num + form->ctrl_num;
			add_to_list(*subm, sub);
			if (form->type == FC_IMAGE && fi < 1) {
				fi++;
				goto fi_rep;
			}
		}
	}
	do {
		struct submitted_value *sub = NULL, *nx;
		struct list_head *lsub;
		ch = 0;
		foreach(struct submitted_value, sub, lsub, *subm) if (sub->list_entry.next != subm) {
			nx = list_struct(sub->list_entry.next, struct submitted_value);
			if (compare_submitted(nx, sub) < 0) {
				del_from_list(sub);
				add_after_pos(nx, sub);
				lsub = &nx->list_entry;
				ch = 1;
			}
		}
		foreachback(struct submitted_value, sub, lsub, *subm) if (sub->list_entry.next != subm) {
			nx = list_struct(sub->list_entry.next, struct submitted_value);
			if (compare_submitted(nx, sub) < 0) {
				del_from_list(sub);
				add_after_pos(nx, sub);
				sub = nx;
				lsub = &nx->list_entry;
				ch = 1;
			}
		}
	} while (ch);
}

static unsigned char *strip_file_name(unsigned char *f)
{
	unsigned char *n;
	unsigned char *l = f - 1;
	for (n = f; *n; n++) if (dir_sep(*n)) l = n;
	return l + 1;
}

static inline int safe_char(unsigned char c)
{
	return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c== '.' || c == '-' || c == '_';
}

static void encode_string(unsigned char *name, unsigned char **data, int *len)
{
	for (; *name; name++) {
		if (*name == ' ') add_chr_to_str(data, len, '+');
		else if (safe_char(*name)) add_chr_to_str(data, len, *name);
		else {
			unsigned char n[4];
			sprintf(cast_char n, "%%%02X", *name);
			add_to_str(data, len, n);
		}
	}
}

static void encode_controls(struct list_head *l, unsigned char **data, int *len,
		     int cp_from, int cp_to)
{
	struct submitted_value *sv = NULL;
	struct list_head *lsv;
	int lst = 0;
	unsigned char *p2;
	*len = 0;
	*data = init_str();
	foreach(struct submitted_value, sv, lsv, *l) {
		unsigned char *p = sv->value;
		if (lst)
			add_chr_to_str(data, len, '&');
		else
			lst = 1;
		encode_string(sv->name, data, len);
		add_chr_to_str(data, len, '=');
		if (sv->type == FC_TEXT || sv->type == FC_PASSWORD || sv->type == FC_TEXTAREA)
			p2 = convert(cp_from, cp_to, p, NULL);
		else
			p2 = stracpy(p);
		encode_string(p2, data, len);
		free(p2);
	}
}

#define BL	56
#define BL1	27

static void encode_multipart(struct session *ses, struct list_head *l, unsigned char **data, int *len,
		      unsigned char *bound, int cp_from, int cp_to)
{
	int errn;
	int *bound_ptrs = NULL;
	int nbound_ptrs = 0;
	unsigned char *m1, *m2;
	struct submitted_value *sv = NULL;	/* against warning */
	struct list_head *lsv;
	int i, j;
	int flg = 0;
	unsigned char *p;
	int rs;
	memset(bound, 'x', BL);
	*len = 0;
	*data = init_str();
	foreach(struct submitted_value, sv, lsv, *l) {
		unsigned char *ct;
		bnd:
		add_to_str(data, len, cast_uchar "--");
		if (!(nbound_ptrs & (ALLOC_GR-1))) {
			if ((unsigned)nbound_ptrs > INT_MAX / sizeof(int) - ALLOC_GR)
				overalloc();
			bound_ptrs = xrealloc(bound_ptrs,
					(nbound_ptrs + ALLOC_GR) * sizeof(int));
		}
		bound_ptrs[nbound_ptrs++] = *len;
		add_bytes_to_str(data, len, bound, BL);
		if (flg) break;
		add_to_str(data, len, cast_uchar "\r\nContent-Disposition: form-data; name=\"");
		add_to_str(data, len, sv->name);
		add_to_str(data, len, cast_uchar "\"");
		if (sv->type == FC_FILE_UPLOAD) {
			add_to_str(data, len, cast_uchar "; filename=\"");
			add_to_str(data, len, strip_file_name(sv->value));
				/* It sends bad data if the file name contains ", but
				   Netscape does the same */
			add_to_str(data, len, cast_uchar "\"");
			if (*sv->value) if ((ct = get_content_type(NULL, sv->value))) {
				add_to_str(data, len, cast_uchar "\r\nContent-Type: ");
				add_to_str(data, len, ct);
				if (strlen(cast_const_char ct) >= 4 && !casecmp(ct, cast_uchar "text", 4)) {
					add_to_str(data, len, cast_uchar "; charset=");
					if (!F) add_to_str(data, len, get_cp_mime_name(term_charset(ses->term)));
#ifdef G
					else add_to_str(data, len, get_cp_mime_name(ses->ds.assume_cp));
#endif
				}
				free(ct);
			}
		}
		add_to_str(data, len, cast_uchar "\r\n\r\n");
		if (sv->type != FC_FILE_UPLOAD) {
			if (sv->type == FC_TEXT || sv->type == FC_PASSWORD || sv->type == FC_TEXTAREA)
				p = convert(cp_from, cp_to, sv->value, NULL);
			else p = stracpy(sv->value);
			add_to_str(data, len, p);
			free(p);
		} else {
			int fh, rd;
#define F_BUFLEN 1024
			unsigned char buffer[F_BUFLEN];
			if (*sv->value) {
				unsigned char *wd;
				if (anonymous) {
					goto not_allowed;
				}
				wd = get_cwd();
				set_cwd(ses->term->cwd);
				fh = c_open(sv->value, O_RDONLY | O_NOCTTY);
				if (fh == -1) {
					errn = errno;
					if (wd) {
						set_cwd(wd);
						free(wd);
					}
					goto error;
				}
				if (wd) {
					set_cwd(wd);
					free(wd);
				}
				do {
					if ((rd = hard_read(fh, buffer, F_BUFLEN)) == -1) {
						errn = errno;
						EINTRLOOP(rs, close(fh));
						goto error;
					}
					if (rd) add_bytes_to_str(data, len, buffer, rd);
				} while (rd);
				EINTRLOOP(rs, close(fh));
			}
		}
		add_to_str(data, len, cast_uchar "\r\n");
	}
	if (!flg) {
		flg = 1;
		goto bnd;
	}
	add_to_str(data, len, cast_uchar "--\r\n");
	memset(bound, '-', BL1);
	memset(bound + BL1, '0', BL - BL1);
	again:
	for (i = 0; i <= *len - BL; i++) {
		for (j = 0; j < BL; j++) if ((*data)[i + j] != bound[j]) goto nb;
		for (j = BL - 1; j >= 0; j--) {
			if (bound[j] < '0') bound[j] = '0' - 1;
			if (bound[j]++ >= '9') bound[j] = '0';
			else goto again;
		}
		internal("Could not assign boundary");
		nb:;
	}
	for (i = 0; i < nbound_ptrs; i++) memcpy(*data + bound_ptrs[i], bound, BL);
	free(bound_ptrs);
	return;

	error:
	free(bound_ptrs);
	free(*data);
	*data = NULL;
	m1 = stracpy(sv->value);
	m2 = stracpy(cast_uchar strerror(errn));
	msg_box(ses->term, getml(m1, m2, NULL), TEXT_(T_ERROR_WHILE_POSTING_FORM), AL_CENTER, TEXT_(T_COULD_NOT_GET_FILE), cast_uchar " ", m1, cast_uchar ": ", m2, MSG_BOX_END, (void *)ses, 1, TEXT_(T_CANCEL), msg_box_null, B_ENTER | B_ESC);
	return;

	not_allowed:
	free(bound_ptrs);
	free(*data);
	*data = NULL;
	msg_box(ses->term, NULL, TEXT_(T_ERROR_WHILE_POSTING_FORM), AL_CENTER, TEXT_(T_READING_FILES_IS_NOT_ALLOWED), MSG_BOX_END, (void *)ses, 1, TEXT_(T_CANCEL), msg_box_null, B_ENTER | B_ESC);
}

void reset_form(struct f_data_c *f, int form_num)
{
	struct form_control *form = NULL;
	struct list_head *lform;
	foreach(struct form_control, form, lform, f->f_data->forms) if (form->form_num == form_num) {
		struct form_state *fs;
		fs = find_form_state(f, form);
		init_ctrl(form, fs);
	}
}

unsigned char *get_form_url(struct session *ses, struct f_data_c *f, struct form_control *form, int *onsubmit)
{
	struct list_head submit;
	unsigned char *data;
	unsigned char bound[BL];
	int len;
	unsigned char *go = NULL;
	int cp_from, cp_to;
	if (!form) return NULL;
	if (form->type == FC_RESET) {
		reset_form(f, form->form_num);
#ifdef G
		if (F) draw_fd(f);
#endif
		return NULL;
	}
	if (onsubmit)*onsubmit=0;
	if (!form->action) return NULL;
	get_succesful_controls(f, form, &submit);
	cp_from = term_charset(ses->term);
	cp_to = f->f_data->cp;
	if (form->method == FM_GET || form->method == FM_POST)
		encode_controls(&submit, &data, &len, cp_from, cp_to);
	else
		encode_multipart(ses, &submit, &data, &len, bound, cp_from, cp_to);
	if (!data) goto ff;
	if (!casecmp(form->action, cast_uchar "javascript:", 11))
	{
		go=stracpy(form->action);
		goto x;
	}
	if (form->method == FM_GET) {
		unsigned char *pos, *da;
		size_t q;
		go = stracpy(form->action);
		pos = extract_position(go);
		if (!(da = get_url_data(go))) da = go;
		q = strlen(cast_const_char da);
		if (q && (da[q - 1] == '&' || da[q - 1] == '?'))
			;
		else if (strchr(cast_const_char da, '?')) add_to_strn(&go, cast_uchar "&");
		else add_to_strn(&go, cast_uchar "?");
		add_to_strn(&go, data);
		if (pos) {
			add_to_strn(&go, pos);
			free(pos);
		}
	} else {
		int l = 0;
		int i;
		go = init_str();
		add_to_str(&go, &l, form->action);
		add_chr_to_str(&go, &l, POST_CHAR);
		if (form->method == FM_POST) add_to_str(&go, &l, cast_uchar "application/x-www-form-urlencoded\n");
		else {
			add_to_str(&go, &l, cast_uchar "multipart/form-data; boundary=");
			add_bytes_to_str(&go, &l, bound, BL);
			add_to_str(&go, &l, cast_uchar "\n");
		}
		for (i = 0; i < len; i++) {
			unsigned char p[3];
			sprintf(cast_char p, "%02x", (int)data[i]);
			add_to_str(&go, &l, p);
		}
	}
	x:
	free(data);
	ff:
	free_succesful_controls(&submit);
	return go;
}

int ismap_link = 0, ismap_x = 1, ismap_y = 1;

/* if onsubmit is not NULL it will contain 1 if link is submit and the form has an onsubmit handler */
static unsigned char *get_link_url(struct session *ses, struct f_data_c *f, struct link *l, int *onsubmit)
{
	if (l->type == L_LINK) {
		if (!l->where) {
			if (l->where_img && (!F || (!f->f_data->opt.display_images && f->f_data->opt.plain != 2))) return stracpy(l->where_img);
			return NULL;
		}
		if (ismap_link && strlen(cast_const_char l->where) >= 4 && !strcmp(cast_const_char(l->where + strlen(cast_const_char l->where) - 4), "?0,0")) {
			unsigned char *nu = init_str();
			int ll = 0;
			add_bytes_to_str(&nu, &ll, l->where, strlen(cast_const_char l->where) - 3);
			add_num_to_str(&nu, &ll, ismap_x);
			add_chr_to_str(&nu, &ll, ',');
			add_num_to_str(&nu, &ll, ismap_y);
			return nu;
		}
		return stracpy(l->where);
	}
	if (l->type != L_BUTTON && l->type != L_FIELD) return NULL;
	return get_form_url(ses, f, l->form, onsubmit);
}

static struct menu_item *clone_select_menu(struct menu_item *m)
{
	struct menu_item *n = NULL;
	int i = 0;
	do {
		if ((unsigned)i > INT_MAX / sizeof(struct menu_item) - 1)
			overalloc();
		n = xrealloc(n, (i + 1) * sizeof(struct menu_item));
		n[i].text = stracpy(m->text);
		n[i].rtext = stracpy(m->rtext);
		n[i].hotkey = stracpy(m->hotkey);
		n[i].in_m = m->in_m;
		n[i].free_i = 0;
		if ((n[i].func = m->func) != do_select_submenu) {
			n[i].data = m->data;
		} else n[i].data = clone_select_menu(m->data);
		i++;
	} while (m++->text);
	return n;
}

static void free_select_menu(void *m_)
{
	struct menu_item *m = (struct menu_item *)m_;
	struct menu_item *om = m;
	do {
		free(m->text);
		free(m->rtext);
		free(m->hotkey);
		if (m->func == do_select_submenu) free_select_menu(m->data);
	} while (m++->text);
	free(om);
}

void set_frame(struct session *ses, struct f_data_c *f, int a)
{
	if (f == ses->screen) return;
	if (!f->loc->url) return;
	goto_url_not_from_dialog(ses, f->loc->url, ses->screen);
}

static struct link *get_current_link(struct f_data_c *f)
{
	if (!f || !f->f_data || !f->vs) return NULL;
	if (f->vs->current_link >= 0 && f->vs->current_link < f->f_data->nlinks)
		return &f->f_data->links[f->vs->current_link];
	if (F && f->f_data->opt.plain == 2 && f->f_data->nlinks == 1)
		return &f->f_data->links[0];
	return NULL;
}

/* pokud je a==1, tak se nebude submitovat formular, kdyz kliknu na input field a formular nema submit */
int enter(struct session *ses, struct f_data_c *f, int a)
{
	struct link *link;
	unsigned char *u;
	link = get_current_link(f);
	if (!link) return 1;
	if (link->type == L_LINK || link->type == L_BUTTON) {
		int has_onsubmit;
		if (link->type==L_BUTTON&&link->form->type==FC_BUTTON)return 1;
		submit:
		if ((u = get_link_url(ses, f, link, &has_onsubmit))) {
			if (strlen(cast_const_char u) >= 4 && !casecmp(u, cast_uchar "MAP@", 4)) {
				goto_imgmap(ses, f, u + 4, stracpy(u + 4), stracpy(link->target));
			} else if (ses->ds.target_in_new_window && link->target && *link->target && !find_frame(ses, link->target, f) && can_open_in_new(ses->term)) {	/* open in new window */
				free(ses->wtd_target);
				ses->wtd_target = stracpy(link->target);
				open_in_new_window(ses->term, (void *)&send_open_in_new_xterm_ptr, ses);
				free(ses->wtd_target);
				ses->wtd_target=NULL;
			} else {
				goto_url_f(
				ses,
				NULL,
				u,
				link->target,
				f,
				(link->type==L_BUTTON&&link->form&&link->form->type==FC_SUBMIT)?link->form->form_num:-1,
				0
				,0,0
				);
			}
			free(u);
			return 2;
		}
		return 1;
	}
	if (link->type == L_CHECKBOX) {
		struct form_state *fs = find_form_state(f, link->form);
		if (link->form->ro) return 1;
		if (link->form->type == FC_CHECKBOX) fs->state = !fs->state;
		else {
			struct form_control *fc = NULL;
			struct list_head *lfc;
#ifdef G
			int re = 0;
#endif
			foreach(struct form_control, fc, lfc, f->f_data->forms)
				if (fc->form_num == link->form->form_num && fc->type == FC_RADIO && !xstrcmp(fc->name, link->form->name)) {
					struct form_state *fffs = find_form_state(f, fc);
					fffs->state = 0;
#ifdef G
					re = 1;
#endif
				}
			fs = find_form_state(f, link->form);
			fs->state = 1;
#ifdef G
			if (F && re) draw_fd(f);
#endif
		}
		return 1;
	}
	if (link->type == L_SELECT) {
		struct menu_item *m;
		if (link->form->ro) return 1;
		m = clone_select_menu(link->form->menu);
		if (!m) return 1;
		/* execute onfocus code of the select object */
		add_empty_window(ses->term, free_select_menu, m);
		do_select_submenu(ses->term, m, ses);
		return 1;
	}
	if (link->type == L_FIELD || link->type == L_AREA) {
		/* pri enteru v textovem policku se bude posilat vzdycky       -- Brain */
		if (!has_form_submit(f->f_data, link->form) && (!a || !F)) goto submit;
#ifdef G
		if (F && a) {
			ses->locked_link = 1;
			return 2;
		}
#endif
		if (!F) {
			down(ses, f, 0);
		}
#ifdef G
		else g_next_link(f, 1, 1);
#endif
		return 1;
	}
	internal("bad link type %d", link->type);
	return 1;
}

void toggle(struct session *ses, struct f_data_c *f, int a)
{
	if (!f || !f->vs) {
		msg_box(ses->term, NULL, TEXT_(T_TOGGLE_HTML_PLAIN), AL_LEFT, TEXT_(T_YOU_ARE_NOWHERE), MSG_BOX_END, NULL, 1, TEXT_(T_OK), msg_box_null, B_ENTER | B_ESC);
		return;
	}
	if (f->vs->plain == -1) f->vs->plain = 1;
	else f->vs->plain = f->vs->plain ^ 1;
	html_interpret_recursive(f);
	draw_formatted(ses);
}

void selected_item(struct terminal *term, void *pitem, void *ses_)
{
	struct session *ses = (struct session *)ses_;
	int item = (int)(long)pitem;
	struct f_data_c *f = current_frame(ses);
	struct link *l;
	struct form_state *fs;
	struct form_control *form;
	l = get_current_link(f);
	if (!l) return;
	if (l->type != L_SELECT) return;
	form = l->form;
	fs = find_form_state(f, form);
	if (item >= 0 && item < form->nvalues) {
		free_format_text_cache_entry(fs);
		fs->state = item;
		free(fs->string);
		fs->string = stracpy(form->values[item]);
	}
	fixup_select_state(form, fs);
	f->active = 1;
#ifdef G
	if (F) {
		f->xl = -1;
		f->yl = -1;
	}
#endif
	draw_to_window(ses->win, draw_doc, f);
	change_screen_status(ses);
	print_screen_status(ses);
}

int get_current_state(struct session *ses)
{
	struct f_data_c *f = current_frame(ses);
	struct link *l;
	struct form_state *fs;
	l = get_current_link(f);
	if (!l) return -1;
	if (l->type != L_SELECT) return -1;
	fs = find_form_state(f, l->form);
	return fs->state;
}

static int find_pos_in_link(struct f_data_c *fd,struct link *l,struct links_event *ev,int *xx,int *yy);

static void set_form_position(struct f_data_c *fd, struct link *l, struct links_event *ev)
{
	/* if links is a field, set cursor position */
	if (l->form && (l->type == L_AREA || l->type == L_FIELD)) {
		struct form_state *fs = find_form_state(fd,l->form);
		int xx = 0, yy = 0; /* against uninitialized warning */

		if (l->type == L_AREA) {
			struct format_text_cache_entry *ftce;

			if (!find_pos_in_link(fd, l, ev, &xx, &yy)) {
				xx += fs->vpos;
				yy += fs->vypos;
				ftce = format_text(fd, l->form, fs);
				if (yy >= ftce->n_lines)
					yy = ftce->n_lines - 1;
				if (yy >= 0) {
					unsigned char *ptr;
					fs->state = ftce->ln[yy].st_offs;
					ptr = textptr_add(fs->string + fs->state, xx, fd->f_data->opt.cp);
					if (ptr > fs->string + ftce->ln[yy].en_offs)
						ptr = fs->string + ftce->ln[yy].en_offs;
					fs->state = (int)(ptr - fs->string);
					goto br;
				}
				fs->state = (int)strlen(cast_const_char fs->string);
				br:;
			}
		} else if (l->type == L_FIELD) {
			if (!find_pos_in_link(fd, l, ev, &xx, &yy)) {
				unsigned char *ptr;
				ptr = textptr_add(fs->string + fs->vpos, xx, fd->f_data->opt.cp);
				fs->state = (int)(ptr - fs->string);
			}
		}

		fd->last_captured = 1;
	}
}

static int textarea_adjust_viewport(struct f_data_c *fd, struct link *l)
{
	struct form_control *fc = l->form;
	struct view_state *vs = fd->vs;
	int r = 0;
	if (l->pos[0].x + fc->cols > fd->xw + vs->view_posx) {
		vs->view_posx = l->pos[0].x + fc->cols - fd->xw;
		r = 1;
	}
	if (l->pos[0].x < vs->view_posx) {
		vs->view_posx = l->pos[0].x;
		r = 1;
	}
	if (l->pos[0].y + fc->rows > fd->yw + vs->view_pos) {
		vs->view_pos = l->pos[0].y + fc->rows - fd->yw;
		r = 1;
	}
	if (l->pos[0].y < vs->view_pos) {
		vs->view_pos = l->pos[0].y;
		r = 1;
	}
	vs->orig_view_pos = vs->view_pos;
	vs->orig_view_posx = vs->view_posx;
	return r;
}

int field_op(struct session *ses, struct f_data_c *f, struct link *l, struct links_event *ev)
{
	struct form_control *form = l->form;
	struct form_state *fs;
	int r = 1;
	struct format_text_cache_entry *ftce;
	int y;

	if (!form) {
		internal("link has no form control");
		return 0;
	}
	if (form->ro == 2) return 0;
	fs = find_form_state(f, form);
	if (!fs->string) return 0;
	if (ev->ev == EV_KBD) {
		if (!(ev->y & (KBD_CTRL | KBD_ALT)) && ev->x >= ' ') {
			if (!form->ro && strlen((char *)fs->string) < form->maxlength) {
				unsigned char *v;
				unsigned char a_[2];
				unsigned char *nw;
				int ll;
				free_format_text_cache_entry(fs);
				v = fs->string = xrealloc(fs->string,
							strlen(cast_const_char fs->string) + 12);
				if (f->f_data->opt.cp) {
					nw = a_;
					a_[0] = (unsigned char)ev->x;
					a_[1] = 0;
				} else {
					nw = encode_utf_8(ev->x);
				}
				ll = (int)strlen(cast_const_char nw);
				if (ll > 10) goto done;
				memmove(v + fs->state + ll, v + fs->state, strlen(cast_const_char(v + fs->state)) + 1);
				memcpy(&v[fs->state], nw, ll);
				fs->state += ll;
			}
			goto done;
		} else if (!(ev->y & (KBD_SHIFT | KBD_CTRL | KBD_ALT))
		&& ev->x == KBD_ENTER
		&& form->type == FC_TEXTAREA) {
			if (!form->ro
			&& strlen(cast_const_char fs->string) < (size_t)form->maxlength) {
				unsigned char *v;
				free_format_text_cache_entry(fs);
				v = xrealloc(fs->string,
					strlen(cast_const_char fs->string) + 2);
				fs->string = v;
				memmove(v + fs->state + 1, v + fs->state, strlen(cast_const_char(v + fs->state)) + 1);
				v[fs->state++] = '\n';
			}
			goto done;
		}
		if (ev->y & KBD_PASTING) {
			r = 0;
			goto done;
		}
		if (ev->x == KBD_LEFT) {
			if (f->f_data->opt.cp)
				fs->state = fs->state ? fs->state - 1 : 0;
			else {
				unsigned char *p = fs->string + fs->state;
				BACK_UTF_8(p, fs->string);
				fs->state = (int)(p - fs->string);
			}
		} else if (ev->x == KBD_RIGHT) {
			if ((size_t)fs->state < strlen(cast_const_char fs->string)) {
				if (f->f_data->opt.cp)
					fs->state = fs->state + 1;
				else {
					unsigned char *p = fs->string + fs->state;
					FWD_UTF_8(p);
					fs->state = (int)(p - fs->string);
				}
			} else fs->state = (int)strlen(cast_const_char fs->string);
		} else if ((ev->x == KBD_HOME || (upcase(ev->x) == 'A' && ev->y & KBD_CTRL))) {
			if (form->type == FC_TEXTAREA) {
				ftce = format_text(f, form, fs);
				y = find_cursor_line(ftce, fs->state);
				if (y >= 0) {
					fs->state = ftce->ln[y].st_offs;
					goto done;
				}
				fs->state = 0;
			} else fs->state = 0;
		} else if (ev->x == KBD_END || (upcase(ev->x) == 'E' && ev->y & KBD_CTRL)) {
			if (form->type == FC_TEXTAREA) {
				ftce = format_text(f, form, fs);
				y = find_cursor_line(ftce, fs->state);
				if (y >= 0) {
					fs->state = ftce->ln[y].en_offs;
					if (fs->state && y < ftce->n_lines - 1 && ftce->ln[y + 1].st_offs == ftce->ln[y].en_offs)
						fs->state--;
					goto done;
				}
				fs->state = (int)strlen(cast_const_char fs->string);
			} else fs->state = (int)strlen(cast_const_char fs->string);
		} else if (ev->x == KBD_UP) {
			if (form->type == FC_TEXTAREA) {
				ftce = format_text(f, form, fs);
				y = find_cursor_line(ftce, fs->state);
				if (y > 0) {
					fs->state = (int)(textptr_add(fs->string + ftce->ln[y - 1].st_offs, textptr_diff(fs->string + fs->state, fs->string + ftce->ln[y].st_offs, f->f_data->opt.cp), f->f_data->opt.cp) - fs->string);
					if (fs->state > ftce->ln[y - 1].en_offs) fs->state = ftce->ln[y - 1].en_offs;
				} else
					goto b;
			} else {
				r = 0;
				f->vs->brl_in_field = 0;
			}
		} else if (ev->x == KBD_DOWN) {
			if (form->type == FC_TEXTAREA) {
				ftce = format_text(f, form, fs);
				y = find_cursor_line(ftce, fs->state);
				if (y >= 0) {
					if (y >= ftce->n_lines - 1)
						goto b;
					fs->state = (int)(textptr_add(fs->string + ftce->ln[y + 1].st_offs, textptr_diff(fs->string + fs->state, fs->string + ftce->ln[y].st_offs, f->f_data->opt.cp), f->f_data->opt.cp) - fs->string);
					if (fs->state > ftce->ln[y + 1].en_offs)
						fs->state = ftce->ln[y + 1].en_offs;
				} else 
					goto b;
			} else {
				r = 0;
				f->vs->brl_in_field = 0;
			}
		} else if ((ev->x == KBD_INS && ev->y & KBD_CTRL) || (upcase(ev->x) == 'B' && ev->y & KBD_CTRL) || ev->x == KBD_COPY) {
			set_clipboard_text(ses->term, fs->string);
		} else if ((ev->x == KBD_DEL && ev->y & KBD_SHIFT) || (upcase(ev->x) == 'X' && ev->y & KBD_CTRL) || ev->x == KBD_CUT) {
			set_clipboard_text(ses->term, fs->string);
			if (!form->ro) {
				free_format_text_cache_entry(fs);
				fs->string[0] = 0;
			}
			fs->state = 0;
		} else if ((ev->x == KBD_INS && ev->y & KBD_SHIFT) || (upcase(ev->x) == 'V' && ev->y & KBD_CTRL) || ev->x == KBD_PASTE) {
			unsigned char *clipboard;
			clipboard = get_clipboard_text(ses->term);
			if (!clipboard) goto done;
			if (form->type != FC_TEXTAREA) {
				unsigned char *nl = clipboard;
				while ((nl = cast_uchar strchr(cast_const_char nl, '\n'))) *nl = ' ';
			}
			if (!form->ro && strlen(cast_const_char fs->string) + strlen(cast_const_char clipboard) <= form->maxlength) {
				unsigned char *v;
				free_format_text_cache_entry(fs);
				v = xrealloc(fs->string,
					strlen(cast_const_char fs->string) + strlen(cast_const_char clipboard) + 1);
				fs->string = v;
				memmove(v + fs->state + strlen(cast_const_char clipboard), v + fs->state, strlen(cast_const_char v) - fs->state + 1);
				memcpy(v + fs->state, clipboard, strlen(cast_const_char clipboard));
				fs->state += (int)strlen(cast_const_char clipboard);
			}
			free(clipboard);
		} else if (ev->x == KBD_ENTER)
			r = 0;
		else if (ev->x == KBD_BS) {
			if (!form->ro && fs->state) {
				int ll = 1;
				free_format_text_cache_entry(fs);
				if (!f->f_data->opt.cp) {
					unsigned char *p = fs->string + fs->state;
					BACK_UTF_8(p, fs->string);
					ll = (int)(fs->string + fs->state - p);
				}
				memmove(fs->string + fs->state - ll, fs->string + fs->state, strlen(cast_const_char(fs->string + fs->state)) + 1);
				fs->state -= ll;
			}
		} else if (ev->x == KBD_DEL || (upcase(ev->x) == 'D' && ev->y & KBD_CTRL)) {
			int ll = 1;
			if (!F && ev->x == KBD_DEL && !f->last_captured)
				return 0;
			if (!f->f_data->opt.cp) {
				unsigned char *p = fs->string+ fs->state;
				FWD_UTF_8(p);
				ll = (int)(p - (fs->string+ fs->state));
			}
			if (!form->ro && (size_t)fs->state < strlen(cast_const_char fs->string)) {
				free_format_text_cache_entry(fs);
				memmove(fs->string + fs->state, fs->string + fs->state + ll, strlen(cast_const_char(fs->string + fs->state + ll)) + 1);
			}
		} else if (upcase(ev->x) == 'U' && ev->y & KBD_CTRL) {
			unsigned char *a;
			a = memacpy(fs->string, fs->state);
			set_clipboard_text(ses->term, a);
			free(a);
			if (!form->ro) {
				free_format_text_cache_entry(fs);
				memmove(fs->string, fs->string + fs->state, strlen(cast_const_char(fs->string + fs->state)) + 1);
			}
			fs->state = 0;
		} else if (upcase(ev->x) == 'K' && ev->y & KBD_CTRL) {
			if (!form->ro) {
				if (form->type == FC_TEXTAREA) {
					ftce = format_text(f, form, fs);
					y = find_cursor_line(ftce, fs->state);
					if (y >= 0) {
						int l = ftce->ln[y].en_offs - ftce->ln[y].st_offs;
						unsigned char *start_line = fs->string + ftce->ln[y].st_offs;
						unsigned char *cp = memacpy(start_line, ftce->ln[y].en_offs - ftce->ln[y].st_offs);
						set_clipboard_text(ses->term, cp);
						free(cp);
						l += y < ftce->n_lines - 1 && ftce->ln[y + 1].st_offs > ftce->ln[y].en_offs;
						memmove(fs->string + ftce->ln[y].st_offs, fs->string + ftce->ln[y].st_offs + l, strlen(cast_const_char(fs->string + ftce->ln[y].st_offs + l)) + 1);
						fs->state = ftce->ln[y].st_offs;
					}
				} else {
					set_clipboard_text(ses->term, fs->state + fs->string);
					fs->string[fs->state] = 0;
				}
				free_format_text_cache_entry(fs);
			}
		} else {
			b:
			f->vs->brl_in_field = 0;
			r = 0;
		}
	} else if (ev->ev == EV_MOUSE && BM_IS_WHEEL(ev->b) && form->type == FC_TEXTAREA) {
		int xdiff = 0, ydiff = 0;
		int x;
		unsigned char *ap;

		ftce = format_text(f, form, fs);
		y = find_cursor_line(ftce, fs->state);
		if (y >= 0)
			x = textptr_diff(fs->string + fs->state, fs->string + ftce->ln[y].st_offs, f->f_data->opt.cp);
		else {
			x = 0;
			y = 0;
		}
			
		if ((ev->b & BM_BUTT) == B_WHEELUP1)
			ydiff = -1;
		else if ((ev->b & BM_BUTT) == B_WHEELDOWN1)
			ydiff = 1;
		else if ((ev->b & BM_BUTT) == B_WHEELLEFT1)
			xdiff = -1;
		else if ((ev->b & BM_BUTT) == B_WHEELRIGHT1)
			xdiff = 1;

		if (ydiff) {
			int target_y = -1;
			fs->vypos += ydiff;
			if (fs->vypos > ftce->n_lines - form->rows) fs->vypos = ftce->n_lines - form->rows;
			if (fs->vypos < 0) fs->vypos = 0;
			if (y >= form->rows + fs->vypos) {
				target_y = form->rows + fs->vypos - 1;
				if (target_y < 0)
					target_y = 0;
			}
			if (y < fs->vypos)
				target_y = fs->vypos;

			if (target_y >= 0) {
				if (target_y >= ftce->n_lines)
					target_y = ftce->n_lines - 1;
				fs->state = ftce->ln[target_y].st_offs;
				if (x > ftce->ln[target_y].chars)
					x = ftce->ln[target_y].chars;
				ap = textptr_add(fs->string + fs->state, x, f->f_data->opt.cp);
				fs->state = (int)(ap - fs->string);
			}
		} else if (xdiff) {
			int j;
			int maxx = 0;
			int longest = y;
			for (j = fs->vypos; j < fs->vypos + form->rows && j < ftce->n_lines; j++) {
				if (ftce->ln[j].chars > maxx) {
					maxx = ftce->ln[j].chars;
					longest = j;
				}
			}
			maxx -= form->cols - 1;
			if (maxx < 0) maxx = 0;
			fs->vpos += xdiff;
			if (fs->vpos < 0) fs->vpos = 0;
			if (fs->vpos > maxx) fs->vpos = maxx;
			if (x > fs->vpos + form->cols - 1) {
				ap = textptr_add(fs->string + ftce->ln[y].st_offs, fs->vpos + form->cols - 1, f->f_data->opt.cp);
				fs->state = (int)(ap - fs->string);
			} else if (x < fs->vpos) {
				ap = textptr_add(fs->string + ftce->ln[y].st_offs, fs->vpos, f->f_data->opt.cp);
				if (y < ftce->n_lines - 1 && ap >= fs->string + ftce->ln[y + 1].st_offs)
					ap = textptr_add(fs->string + ftce->ln[longest].st_offs, fs->vpos, f->f_data->opt.cp);
				fs->state = (int)(ap - fs->string);
			}
		}
		goto done;
	} else
		r = 0;
done:
	if (!F && r) {
		/* FIXME: test might be broken */
		if ((ev->ev == EV_KBD && (ev->x == KBD_UP || ev->x == KBD_DOWN))
		|| form->type != FC_TEXTAREA || textarea_adjust_viewport(f, l))
			x_draw_form_entry(ses, f, l);
	}
	return r;
}

void set_textarea(struct session *ses, struct f_data_c *f, int dir)
{
	struct link *l = get_current_link(f);
	if (l && l->type == L_AREA) {
		struct form_control *form = l->form;
		struct form_state *fs;
		struct format_text_cache_entry *ftce;
		int y;

		if (form->ro == 2) return;
		fs = find_form_state(f, form);
		if (!fs->string) return;

		ftce = format_text(f, form, fs);
		y = find_cursor_line(ftce, fs->state);
		if (y >= 0) {
			int ty = dir < 0 ? 0 : ftce->n_lines - 1;
			if (ty < 0) return;

			fs->state = (int)(textptr_add(fs->string + ftce->ln[ty].st_offs, textptr_diff(fs->string + fs->state, fs->string + ftce->ln[y].st_offs, f->f_data->opt.cp), f->f_data->opt.cp) - fs->string);
			if (fs->state > ftce->ln[ty].en_offs) fs->state = ftce->ln[ty].en_offs;
		}
	}
}

void search_for_back(void *ses_, unsigned char *str)
{
	struct session *ses = (struct session *)ses_;
	struct f_data_c *f = current_frame(ses);
	if (!f || !str || !str[0]) return;
	free(ses->search_word);
	ses->search_word = stracpy(str);
	clr_spaces(ses->search_word, 0);
	charset_upcase_string(&ses->search_word, term_charset(ses->term));
	free(ses->last_search_word);
	ses->last_search_word = stracpy(ses->search_word);
	ses->search_direction = -1;
	find_next(ses, f, 1);
}

void search_for(void *ses_, unsigned char *str)
{
	struct session *ses = (struct session *)ses_;
	struct f_data_c *f = current_frame(ses);
	if (!f || !f->vs || !f->f_data || !str || !str[0]) return;
	free(ses->search_word);
	ses->search_word = stracpy(str);
	clr_spaces(ses->search_word, 0);
	charset_upcase_string(&ses->search_word, term_charset(ses->term));
	free(ses->last_search_word);
	ses->last_search_word = stracpy(ses->search_word);
	ses->search_direction = 1;
	find_next(ses, f, 1);
}

#define HASH_SIZE	4096

#define HASH(p) (((p.y << 6) + p.x) & (HASH_SIZE - 1))

static int point_intersect(struct point *p1, int l1, struct point *p2, int l2)
{
	int i, j;
	static unsigned char hash[HASH_SIZE];
	static unsigned char init = 0;
	if (!init) {
		memset(hash, 0, HASH_SIZE);
		init = 1;
	}
	for (i = 0; i < l1; i++) hash[HASH(p1[i])] = 1;
	for (j = 0; j < l2; j++) if (hash[HASH(p2[j])]) {
		for (i = 0; i < l1; i++) if (p1[i].x == p2[j].x && p1[i].y == p2[j].y) {
			for (i = 0; i < l1; i++) hash[HASH(p1[i])] = 0;
			return 1;
		}
	}
	for (i = 0; i < l1; i++) hash[HASH(p1[i])] = 0;
	return 0;
}

static int find_next_link_in_search(struct f_data_c *f, int d)
{
	struct point *pt;
	int len;
	struct link *link;
	if (d == -2 || d == 2) {
		d /= 2;
		find_link(f, d, 0);
		if (f->vs->current_link == -1) return 1;
	} else nx:if (f->vs->current_link == -1 || !(next_in_view(f, f->vs->current_link + d, d, in_view, NULL))) {
		find_link(f, d, 0);
		return 1;
	}
	link = &f->f_data->links[f->vs->current_link];
	if (get_searched(f, &pt, &len) < 0)
		return 1;
	if (point_intersect(pt, len, link->pos, link->n)) {
		free(pt);
		return 0;
	}
	free(pt);
	goto nx;
}

void find_next(struct session *ses, struct f_data_c *f, int a)
{
	int min, max;
	int c = 0;
	int p;
	if (!f->f_data || !f->vs) {
		msg_box(ses->term, NULL, TEXT_(T_SEARCH), AL_CENTER, TEXT_(T_YOU_ARE_NOWHERE), MSG_BOX_END, NULL, 1, TEXT_(T_CANCEL), msg_box_null, B_ENTER | B_ESC);
		return;
	}
	p = f->vs->view_pos;
	if (!F && !a && ses->search_word) {
		if (!(find_next_link_in_search(f, ses->search_direction))) return;
		p += ses->search_direction * f->yw;
	}
	if (!ses->search_word) {
		if (!ses->last_search_word) {
			msg_box(ses->term, NULL, TEXT_(T_SEARCH), AL_CENTER, TEXT_(T_NO_PREVIOUS_SEARCH), MSG_BOX_END, NULL, 1, TEXT_(T_CANCEL), msg_box_null, B_ENTER | B_ESC);
			return;
		}
		ses->search_word = stracpy(ses->last_search_word);
	}
	print_progress(ses, TEXT_(T_SEARCHING));
#ifdef G
	if (F) {
		g_find_next(f, a);
		return;
	}
#endif
	if (get_search_data(f->f_data) < 0) {
		free(ses->search_word);
		ses->search_word = NULL;
		msg_box(ses->term, NULL, TEXT_(T_SEARCH), AL_CENTER, TEXT_(T_OUT_OF_MEMORY), MSG_BOX_END, NULL, 1, TEXT_(T_CANCEL), msg_box_null, B_ENTER | B_ESC);
		return;
	}
	do {
		if (is_in_range(f->f_data, p, f->yw, ses->search_word, &min, &max)) {
			f->vs->view_pos = p;
			if (max >= min) {
				if (max > f->vs->view_posx + f->xw) f->vs->view_posx = max - f->xw;
				if (min < f->vs->view_posx) f->vs->view_posx = min;
			}
			f->vs->orig_view_pos = f->vs->view_pos;
			f->vs->orig_view_posx = f->vs->view_posx;
			set_link(f);
			find_next_link_in_search(f, ses->search_direction * 2);
			return;
		}
		if ((p += ses->search_direction * f->yw) > f->f_data->y) p = 0;
		if (p < 0) {
			p = 0;
			while (p < f->f_data->y) p += f->yw ? f->yw : 1;
			p -= f->yw;
		}
	} while ((c += f->yw ? f->yw : 1) < f->f_data->y + f->yw);
	msg_box(ses->term, NULL, TEXT_(T_SEARCH), AL_CENTER, TEXT_(T_SEARCH_STRING_NOT_FOUND), MSG_BOX_END, NULL, 1, TEXT_(T_CANCEL), msg_box_null, B_ENTER | B_ESC);
}

void find_next_back(struct session *ses, struct f_data_c *f, int a)
{
	ses->search_direction = - ses->search_direction;
	find_next(ses, f, a);
	ses->search_direction = - ses->search_direction;
}

static void rep_ev(struct session *ses, struct f_data_c *fd, void (*f)(struct session *, struct f_data_c *, int), int a)
{
	int i = ses->kbdprefix.rep ? ses->kbdprefix.rep_num : 1;
	while (i--) f(ses, fd, a);
}

static struct link *choose_mouse_link(struct f_data_c *f, struct links_event *ev)
{
	return get_link_at_location(f->f_data, ev->x + f->vs->view_posx, ev->y + f->vs->view_pos);
}

static void goto_link_number(void *ses_, unsigned char *num)
{
	struct session *ses = (struct session *)ses_;
	int n = atoi(cast_const_char num);
	struct f_data_c *f = current_frame(ses);
	struct link *link;
	if (!f || !f->vs) return;
	if (n < 0 || n > f->f_data->nlinks) return;
	f->vs->current_link = n - 1;
	f->vs->orig_link = f->vs->current_link;
	link = &f->f_data->links[f->vs->current_link];
	check_vs(f);
	f->vs->orig_view_pos = f->vs->view_pos;
	f->vs->orig_view_posx = f->vs->view_posx;
	if (link->type != L_AREA && link->type != L_FIELD) enter(ses, f, 0);
}

/* l must be a valid link, ev must be a mouse event */
static int find_pos_in_link(struct f_data_c *fd, struct link *l, struct links_event *ev, int *xx, int *yy)
{
	int a;
	int minx, miny;
	int found = 0;

	if (!l->n) return 1;
	minx = l->pos[0].x;
	miny = l->pos[0].y;
	for (a = 0; a < l->n; a++) {
		if (l->pos[a].x < minx) minx = l->pos[a].x;
		if (l->pos[a].y < miny) miny = l->pos[a].y;
		if (l->pos[a].x - fd->vs->view_posx == ev->x
		&& l->pos[a].y - fd->vs->view_pos == ev->y) {
			(*xx = l->pos[a].x);
			(*yy = l->pos[a].y);
			found = 1;
		}
	}
	if (!found) return 1;
	*xx -= minx;
	*yy -= miny;
	return 0;
}

static int frame_ev(struct session *ses, struct f_data_c *fd, struct links_event *ev)
{
	int x = 1;

	if (!fd || !fd->vs || !fd->f_data) return 0;
	if (fd->vs->current_link >= 0 && (fd->f_data->links[fd->vs->current_link].type == L_FIELD || fd->f_data->links[fd->vs->current_link].type == L_AREA)) {
		if (field_op(ses, fd, &fd->f_data->links[fd->vs->current_link], ev)) {
			fd->last_captured = 1;
			fd->vs->brl_in_field = 1;
			return 1;
		}
	}
	fd->last_captured = 0;
	if (ev->ev == EV_KBD && !(ev->y & KBD_PASTING)) {
		if (ev->x >= '0'+!ses->kbdprefix.rep && ev->x <= '9' && (!fd->f_data->opt.num_links || (ev->y & (KBD_CTRL | KBD_ALT)))) {
			if (!ses->kbdprefix.rep) ses->kbdprefix.rep_num = 0;
			if ((ses->kbdprefix.rep_num = ses->kbdprefix.rep_num * 10 + ev->x - '0') > 65536) ses->kbdprefix.rep_num = 65536;
			ses->kbdprefix.rep = 1;
			return 1;
		}
		if (ev->x == KBD_PAGE_DOWN || (ev->x == ' ' && (!(ev->y & KBD_ALT))) || (upcase(ev->x) == 'F' && ev->y & KBD_CTRL)) rep_ev(ses, fd, page_down, 0);
		else if (ev->x == KBD_PAGE_UP || (upcase(ev->x) == 'B' && (!(ev->y & KBD_ALT)))) rep_ev(ses, fd, page_up, 0);
		else if (ev->x == KBD_DOWN) rep_ev(ses, fd, down, 0);
		else if (ev->x == KBD_UP) rep_ev(ses, fd, up, 0);
		/* Copy current link to clipboard */
		else if ((ev->x == KBD_INS && ev->y & KBD_CTRL) || (upcase(ev->x) == 'C' && ev->y & KBD_CTRL)) {
			unsigned char *current_link = print_current_link(ses);
			if (current_link) {
				set_clipboard_text(ses->term, current_link);
				free(current_link);
			}
		}
		else if (ev->x == KBD_INS || (upcase(ev->x) == 'P' && ev->y & KBD_CTRL) || (ev->x == 'p' && !(ev->y & (KBD_CTRL | KBD_ALT)))) rep_ev(ses, fd, scroll, -1 - !ses->kbdprefix.rep);
		else if (ev->x == KBD_DEL || (upcase(ev->x) == 'N' && ev->y & KBD_CTRL) || (ev->x == 'l' && !(ev->y & (KBD_CTRL | KBD_ALT)))) rep_ev(ses, fd, scroll, 1 + !ses->kbdprefix.rep);
		else if (ev->x == '[') rep_ev(ses, fd, hscroll, -1 - 7 * !ses->kbdprefix.rep);
		else if (ev->x == ']') rep_ev(ses, fd, hscroll, 1 + 7 * !ses->kbdprefix.rep);
		else if (ev->x == KBD_HOME || (upcase(ev->x) == 'A' && ev->y & KBD_CTRL)) rep_ev(ses, fd, home, 0);
		else if (ev->x == KBD_END || (upcase(ev->x) == 'E' && ev->y & KBD_CTRL)) rep_ev(ses, fd, x_end, 0);
		else if (ev->x == KBD_RIGHT || ev->x == KBD_ENTER) {
			x = enter(ses, fd, 0);
		} else if (ev->x == '*') {
			ses->ds.images ^= 1;
			html_interpret_recursive(ses->screen);
			draw_formatted(ses);
		} else if (ev->x == 'i' && !(ev->y & KBD_ALT)) {
			if (!F || fd->f_data->opt.plain != 2) frm_view_image(ses, fd);
		} else if (ev->x == 'I' && !(ev->y & KBD_ALT)) {
			if (!anonymous) frm_download_image(ses, fd);
		} else if (upcase(ev->x) == 'D' && !(ev->y & KBD_ALT)) {
			if (!anonymous) frm_download(ses, fd);
		} else if (ev->x == '/' || (ev->x == KBD_FIND && !(ev->y & (KBD_SHIFT | KBD_CTRL | KBD_ALT)))) search_dlg(ses, fd, 0);
		else if (ev->x == '?' || (ev->x == KBD_FIND && ev->y & (KBD_SHIFT | KBD_CTRL | KBD_ALT))) search_back_dlg(ses, fd, 0);
		else if ((ev->x == 'n' && !(ev->y & KBD_ALT)) || ev->x == KBD_REDO) find_next(ses, fd, 0);
		else if ((ev->x == 'N' && !(ev->y & KBD_ALT)) || ev->x == KBD_UNDO) find_next_back(ses, fd, 0);
		else if ((upcase(ev->x) == 'F' && !(ev->y & (KBD_ALT | KBD_CTRL))) || ev->x == KBD_FRONT) set_frame(ses, fd, 0);
		else if (ev->x == 'H' && !(ev->y & (KBD_CTRL | KBD_ALT))) find_link(fd, 1, 1);
		else if (ev->x == 'L' && !(ev->y & (KBD_CTRL | KBD_ALT))) find_link(fd, -1, 1);
		else if (ev->x >= '1' && ev->x <= '9' && !(ev->y & (KBD_CTRL | KBD_ALT))) {
			struct f_data *f_data = fd->f_data;
			int nl, lnl;
			unsigned char d[2];
			d[0] = (unsigned char)ev->x;
			d[1] = 0;
			nl = f_data->nlinks;
			lnl = 1;
			while (nl) {
				nl /= 10;
				lnl++;
			}
			if (lnl > 1) input_field(ses->term, NULL, TEXT_(T_GO_TO_LINK), TEXT_(T_ENTER_LINK_NUMBER), ses, NULL, lnl, d, 1, f_data->nlinks, check_number, 2, TEXT_(T_OK), goto_link_number, TEXT_(T_CANCEL), input_field_null);
		}
		else x = 0;
	} else if (ev->ev == EV_MOUSE && (ev->b & BM_BUTT) <= B_RIGHT) {
		struct link *l = choose_mouse_link(fd, ev);
		if (l) {
			x = 1;
			fd->vs->current_link = (int)(l - fd->f_data->links);
			fd->vs->orig_link = fd->vs->current_link;
			if (l->type == L_LINK || l->type == L_BUTTON || l->type == L_CHECKBOX || l->type == L_SELECT) if ((ev->b & BM_ACT) == B_UP) {
				fd->active = 1;
				draw_to_window(ses->win, draw_doc_c, fd);
				change_screen_status(ses);
				print_screen_status(ses);
				if ((ev->b & BM_BUTT) == B_LEFT) x = enter(ses, fd, 0);
				else link_menu(ses->term, NULL, ses);
			}

			set_form_position(fd, l, ev);
		}
	} else x = 0;
	ses->kbdprefix.rep = 0;
	return x;
}

struct f_data_c *current_frame(struct session *ses)
{
	struct f_data_c *fd, *fdd = NULL;
	struct list_head *lfdd;
	fd = ses->screen;
	while (!list_empty(fd->subframes)) {
		int n = fd->vs->frame_pos;
		if (n == -1) break;
		foreach(struct f_data_c, fdd, lfdd, fd->subframes) if (!n--) {
			fd = fdd;
			goto r;
		}
		fd = list_struct(fd->subframes.next, struct f_data_c);
		r:;
	}
	return fd;
}

static int is_active_frame(struct session *ses, struct f_data_c *f)
{
	struct f_data_c *fd, *fdd = NULL;
	struct list_head *lfdd;
	fd = ses->screen;
	if (f == fd) return 1;
	while (!list_empty(fd->subframes)) {
		int n = fd->vs->frame_pos;
		if (n == -1) break;
		foreach(struct f_data_c, fdd, lfdd, fd->subframes) if (!n--) {
			fd = fdd;
			goto r;
		}
		fd = list_struct(fd->subframes.next, struct f_data_c);
		r:
		if (f == fd) return 1;
	}
	return 0;
}

static int send_to_frame(struct session *ses, struct links_event *ev)
{
	int r;
	struct f_data_c *fd;
	fd = current_frame(ses);
	if (!fd)
		return 0;

	if (!F) r = frame_ev(ses, fd, ev);
#ifdef G
	else r = g_frame_ev(ses, fd, ev);
#endif
	if (r == 1) {
		fd->active = 1;
		draw_to_window(ses->win, draw_doc_c, fd);
		change_screen_status(ses);
		print_screen_status(ses);
	}
	if (r == 3) draw_fd_nrd(fd);
	return r;
}

void next_frame(struct session *ses, int p)
{
	int n;
	struct view_state *vs;
	struct f_data_c *fd, *fdd = NULL;
	struct list_head *lfdd;

	if (!(fd = current_frame(ses))) return;
#ifdef G
	ses->locked_link = 0;
#endif
	while ((fd = fd->parent)) {
		n = (int)list_size(&fd->subframes);
		vs = fd->vs;
		vs->frame_pos += p;
		if (vs->frame_pos < -!fd->f_data->frame_desc) { vs->frame_pos = n - 1; continue; }
		if (vs->frame_pos >= n) { vs->frame_pos = -!fd->f_data->frame_desc; continue; }
		break;
	}
	if (!fd) fd = ses->screen;
	vs = fd->vs;
	n = 0;
	foreach(struct f_data_c, fdd, lfdd, fd->subframes) if (n++ == vs->frame_pos) {
		fd = fdd;
		next_sub:
		if (list_empty(fd->subframes)) break;
		fd = list_struct(p < 0 ? fd->subframes.prev : fd->subframes.next, struct f_data_c);
		vs = fd->vs;
		vs->frame_pos = -1;
		if (!fd->f_data || (!fd->f_data->frame_desc && p > 0)) break;
		if (p < 0) vs->frame_pos += (int)list_size(&fd->subframes);
		else vs->frame_pos = 0;
		goto next_sub;
	}
#ifdef G
	if (F && (fd = current_frame(ses)) && fd->vs && fd->f_data) {
		if (fd->vs->current_link >= 0 && fd->vs->current_link < fd->f_data->nlinks) {
			if (fd->vs->g_display_link && (fd->f_data->links[fd->vs->current_link].type == L_FIELD || fd->f_data->links[fd->vs->current_link].type == L_AREA)) {
				if ((fd->f_data->locked_on = fd->f_data->links[fd->vs->current_link].obj)) fd->ses->locked_link = 1;
			}
		}
	}
#endif
}

void do_for_frame(struct session *ses, void (*f)(struct session *, struct f_data_c *, int), int a)
{
	struct f_data_c *fd = current_frame(ses);
	if (!fd)
		return;
	f(ses, fd, a);
	if (!F) {
		fd->active = 1;
		draw_to_window(ses->win, draw_doc_c, fd);
		change_screen_status(ses);
		print_screen_status(ses);
	}
}

static void do_mouse_event(struct session *ses, struct links_event *ev)
{
	struct links_event evv;
	struct f_data_c *fdd, *fd = current_frame(ses);
	if (!fd) return;
	if (ev->x >= fd->xp && ev->x < fd->xp + fd->xw &&
	    ev->y >= fd->yp && ev->y < fd->yp + fd->yw) goto ok;
#ifdef G
	if (ses->scrolling) goto ok;
#endif
	r:
	next_frame(ses, 1);
	fdd = current_frame(ses);
	/*o = &fdd->f_data->opt;*/
	if (ev->x >= fdd->xp && ev->x < fdd->xp + fdd->xw &&
	    ev->y >= fdd->yp && ev->y < fdd->yp + fdd->yw) {
		draw_formatted(ses);
		fd = fdd;
		goto ok;
	}
	if (fdd != fd) goto r;
	return;
	ok:
	memcpy(&evv, ev, sizeof(struct links_event));
	evv.x -= fd->xp;
	evv.y -= fd->yp;
	send_to_frame(ses, &evv);
}

void send_event(struct session *ses, struct links_event *ev)
{
	if (ses->brl_cursor_mode) {
		ses->brl_cursor_mode = 0;
		print_screen_status(ses);
	}
	if (ev->ev == EV_KBD) {
		if (send_to_frame(ses, ev)) return;
		if (ev->y & KBD_PASTING) goto x;
		if (ev->y & KBD_ALT && ev->x != KBD_TAB && !KBD_ESCAPE_MENU(ev->x)) {
			struct window *m;
			ev->y &= ~KBD_ALT;
			activate_bfu_technology(ses, -1);
			m = list_struct(ses->term->windows.next, struct window);
			m->handler(m, ev, 0);
			if (ses->term->windows.next == &m->list_entry) {
				delete_window(m);
			} else goto x;
			ev->y |= KBD_ALT;
		}
		if (ev->x == KBD_F1 || ev->x == KBD_HELP) {
			activate_keys(ses);
			goto x;
		}
		if (ev->x == KBD_ESC || ev->x == KBD_F9) {
			activate_bfu_technology(ses, -1);
			goto x;
		}
		if (ev->x == KBD_F10) {
			activate_bfu_technology(ses, 0);
			goto x;
		}
		if (ev->x == KBD_TAB) {
			next_frame(ses, ev->y ? -1 : 1);
			draw_formatted(ses);
		}
		if (ev->x == KBD_LEFT) {
			go_back(ses, 1);
			goto x;
		}
		if ((upcase(ev->x) == 'Z' && !(ev->y & (KBD_CTRL | KBD_ALT))) || ev->x == KBD_BS || ev->x == KBD_BACK) {
			go_back(ses, 1);
			goto x;
		}
		if ((upcase(ev->x) == 'X' && !(ev->y & (KBD_CTRL | KBD_ALT))) || ev->x == '\'' || ev->x == KBD_FORWARD) {
			go_back(ses, -1);
			goto x;
		}
		if ((upcase(ev->x) == 'R' && ev->y & KBD_CTRL) || ev->x == KBD_RELOAD) {
			reload(ses, -1);
			goto x;
		}
		if ((ev->x == 'g' && !(ev->y & (KBD_CTRL | KBD_ALT))) || (ev->x == KBD_OPEN && !(ev->y & (KBD_SHIFT | KBD_CTRL)))) {
			quak:
			dialog_goto_url(ses, cast_uchar "");
			goto x;
		}
		if ((ev->x == 'G' && !(ev->y & (KBD_CTRL | KBD_ALT))) || (ev->x == KBD_OPEN && ev->y & KBD_SHIFT)) {
			unsigned char *s;
			if (list_empty(ses->history) || !ses->screen->rq->url) goto quak;
			s = display_url(ses->term, ses->screen->rq->url, 0);
			dialog_goto_url(ses, s);
			free(s);
			goto x;
		}
		if ((upcase(ev->x) == 'G' && ev->y & KBD_CTRL) || (ev->x == KBD_OPEN && ev->y & KBD_CTRL)) {
			struct f_data_c *fd = current_frame(ses);
			unsigned char *s;
			if (!fd->vs || !fd->f_data || fd->vs->current_link < 0 || fd->vs->current_link >= fd->f_data->nlinks) goto quak;
			s = display_url(ses->term, fd->f_data->links[fd->vs->current_link].where, 0);
			dialog_goto_url(ses, s);
			free(s);
			goto x;
		}
		if (ev->x == KBD_PROPS) {
			dialog_html_options(ses);
			goto x;
		}
		if ((upcase(ev->x) == 'S' && !(ev->y & (KBD_CTRL | KBD_ALT))) || ev->x == KBD_BOOKMARKS) {
			if (!anonymous) menu_bookmark_manager(ses->term, NULL, ses);
			goto x;
		}
		if ((upcase(ev->x) == 'Q' && !(ev->y & (KBD_CTRL | KBD_ALT))) || ev->x == KBD_CTRL_C) {
			exit_prog(ses->term, (int *)(long)(ev->x == KBD_CTRL_C || ev->x == 'Q'), ses);
			goto x;
		}
		if (ev->x == KBD_CLOSE) {
			really_exit_prog(ses);
			goto x;
		}
		if (ev->x == '=') {
			state_msg(ses);
			goto x;
		}
		if (ev->x == '|') {
			head_msg(ses);
			goto x;
		}
		if (ev->x == '\\') {
			toggle(ses, ses->screen, 0);
			goto x;
		}
	}
	if (ev->ev == EV_MOUSE) {
		if (ev->b == (B_DOWN | B_FOURTH)) {
			go_back(ses, 1);
			goto x;
		}
		if (ev->b == (B_DOWN | B_FIFTH)) {
			go_back(ses, -1);
			goto x;
		}
#ifdef G
		if (ses->locked_link) {
			if (BM_IS_WHEEL(ev->b)) {
				send_to_frame(ses, ev);
				return;
			} else if ((ev->b & BM_ACT) != B_MOVE) {
				ses->locked_link = 0;
				clr_xl(ses->screen);
				draw_formatted(ses);
			} else return;
		}
#endif
		if (ev->y >= 0 && ev->y < gf_val(1, G_BFU_FONT_SIZE) && ev->x >=0 && ev->x < ses->term->x && (ev->b & BM_ACT) == B_DOWN) {
#ifdef G
			if (F && ev->x < ses->back_size) {
				go_back(ses, 1);
				goto x;
			} else
#endif
			{
				struct window *m;
				activate_bfu_technology(ses, -1);
				m = list_struct(ses->term->windows.next, struct window);
				m->handler(m, ev, 0);
				goto x;
			}
		}
		do_mouse_event(ses, ev);
	}
	return;
	x:
	ses->kbdprefix.rep = 0;
}

static void send_enter(struct terminal *term, void *xxx, void *ses_)
{
	struct session *ses = (struct session *)ses_;
	struct links_event ev = { EV_KBD, KBD_ENTER, 0, 0 };
	send_event(ses, &ev);
}

void frm_download(struct session *ses, struct f_data_c *fd)
{
	struct link *link = get_current_link(fd);
	if (!link) return;
	free(ses->dn_url);
	ses->dn_url = NULL;
	if (link->type != L_LINK && link->type != L_BUTTON) return;
	if ((ses->dn_url = get_link_url(ses, fd, link, NULL))) {
		ses->dn_allow_flags = f_data_c_allow_flags(fd);
		if (!casecmp(ses->dn_url, cast_uchar "MAP@", 4)) {
			free(ses->dn_url);
			ses->dn_url = NULL;
			return;
		}
		query_file(ses, ses->dn_url, NULL, start_download, NULL, DOWNLOAD_CONTINUE);
	}
}

void frm_view_image(struct session *ses, struct f_data_c *fd)
{
	struct link *link = get_current_link(fd);
	if (!link) return;
	if (link->type != L_LINK && link->type != L_BUTTON) return;
	if (!link->where_img) return;
	goto_url_not_from_dialog(ses, link->where_img, fd);
}

void frm_download_image(struct session *ses, struct f_data_c *fd)
{
	struct link *link = get_current_link(fd);
	if (!link) return;
	free(ses->dn_url);
	ses->dn_url = NULL;
	if (link->type != L_LINK && link->type != L_BUTTON) return;
	if (!link->where_img) return;
	if ((ses->dn_url = stracpy(link->where_img))) {
		ses->dn_allow_flags = f_data_c_allow_flags(fd);
		if (!casecmp(ses->dn_url, cast_uchar "MAP@", 4)) {
			free(ses->dn_url);
			ses->dn_url = NULL;
			return;
		}
		query_file(ses, ses->dn_url, NULL, start_download, NULL, DOWNLOAD_CONTINUE);
	}
}

static void send_download_image(struct terminal *term, void *xxx, void *ses_)
{
	struct session *ses = (struct session *)ses_;
	struct f_data_c *fd = current_frame(ses);
	struct link *link = get_current_link(fd);
	if (!link) return;
	free(ses->dn_url);
	if ((ses->dn_url = stracpy(link->where_img))) {
		ses->dn_allow_flags = f_data_c_allow_flags(fd);
		query_file(ses, ses->dn_url, NULL, start_download, NULL, DOWNLOAD_CONTINUE);
	}
}

static void send_download(struct terminal *term, void *xxx, void *ses_)
{
	struct session *ses = (struct session *)ses_;
	struct f_data_c *fd = current_frame(ses);
	struct link *link = get_current_link(fd);
	if (!link) return;
	free(ses->dn_url);
	if ((ses->dn_url = get_link_url(ses, fd, link, NULL))) {
		ses->dn_allow_flags = f_data_c_allow_flags(fd);
		query_file(ses, ses->dn_url, NULL, start_download, NULL, DOWNLOAD_CONTINUE);
	}
}

static void send_submit(struct terminal *term, void *xxx, void *ses_)
{
	struct session *ses = (struct session *)ses_;
	int has_onsubmit;
	struct form_control *form;
	struct f_data_c *fd = current_frame(ses);
	struct link *link = get_current_link(fd);
	unsigned char *u;

	if (!link) return;
	if (!(form = link->form)) return;
	u = get_form_url(ses, fd, form, &has_onsubmit);
	if (u) {
		goto_url_f(fd->ses, NULL, u, NULL, fd, form->form_num, has_onsubmit, 0, 0);
		free(u);
	}
	draw_fd(fd);
}

static void send_reset(struct terminal *term, void *xxx, void *ses_)
{
	struct session *ses = ses_;
	struct form_control *form;
	struct f_data_c *fd = current_frame(ses);
	struct link *link = get_current_link(fd);

	if (!link) return;
	if (!(form = link->form)) return;
	reset_form(fd, form->form_num);
	draw_fd(fd);
}

static void copy_link_location(struct terminal *term, void *xxx, void *ses_)
{
	struct session *ses = ses_;
	unsigned char *current_link = print_current_link(ses);

	if (current_link) {
		set_clipboard_text(term, current_link);
		free(current_link);
	}

}

void copy_url_location(struct terminal *term, void *xxx, void *ses_)
{
	struct session *ses = ses_;
	struct location *current_location;
	unsigned char *url;

	if (list_empty(ses->history)) return;

	current_location = cur_loc(ses);

	url = display_url(term, current_location->url, 0);
	set_clipboard_text(term, url);
	free(url);
}

static void cant_open_new_window(struct terminal *term)
{
	msg_box(term, NULL, TEXT_(T_NEW_WINDOW), AL_CENTER, TEXT_(T_UNABLE_TO_OPEN_NEW_WINDOW), MSG_BOX_END, NULL, 1, TEXT_(T_CANCEL), msg_box_null, B_ENTER | B_ESC);
}

/* open a link in a new xterm, pass target frame name */
static void send_open_in_new_xterm(struct terminal *term, void *open_window_, void *ses_)
{
	int (*open_window)(struct terminal *, unsigned char *, unsigned char *) = *(int (* const *)(struct terminal *, unsigned char *, unsigned char *))open_window_;
	struct session *ses = (struct session *)ses_;
	struct f_data_c *fd = current_frame(ses);
	struct link *l;
	l = get_current_link(fd);
	if (!l) return;
	free(ses->dn_url);
	if ((ses->dn_url = get_link_url(ses, fd, l, NULL))) {
		unsigned char *p;
		int pl;
		unsigned char *enc_url;
		unsigned char *path;

		ses->dn_allow_flags = f_data_c_allow_flags(fd);
		if (disallow_url(ses->dn_url, ses->dn_allow_flags)) {
			free(ses->dn_url);
			ses->dn_url = NULL;
			return;
		}

		p = init_str();
		pl = 0;

		add_to_str(&p, &pl, cast_uchar "-base-session ");
		add_num_to_str(&p, &pl, ses->id);
		add_chr_to_str(&p, &pl, ' ');

		if (ses->wtd_target && *ses->wtd_target) {
			unsigned char *tgt = stracpy(ses->wtd_target);

			check_shell_security(&tgt);
			add_to_str(&p, &pl, cast_uchar "-target ");
			add_to_str(&p, &pl, tgt);
			add_chr_to_str(&p, &pl, ' ');
			free(tgt);
		}
		enc_url = encode_url(ses->dn_url);
		add_to_str(&p, &pl, enc_url);
		free(enc_url);
		path = escape_path(g_argv[0]);
		if (open_window(term, path, p))
			cant_open_new_window(term);
		free(p);
		free(path);
	}
}

static void send_open_new_xterm(struct terminal *term, void *open_window_, void *ses_)
{
	int (*open_window)(struct terminal *, unsigned char *, unsigned char *) = *(int (* const *)(struct terminal *, unsigned char *, unsigned char *))open_window_;
	struct session *ses = ses_;
	unsigned char *p = init_str();
	int pl = 0;
	unsigned char *path;
	add_to_str(&p, &pl, cast_uchar "-base-session ");
	add_num_to_str(&p, &pl, ses->id);
	path = escape_path(g_argv[0]);
	if (open_window(term, path, p))
		cant_open_new_window(term);
	free(path);
	free(p);
}

void (* const send_open_new_xterm_ptr)(struct terminal *, void *fn_, void *ses_) = send_open_new_xterm;

void open_in_new_window(struct terminal *term, void *fn_, void *ses_)
{
	struct session *ses = ses_;
	void (*fn)(struct terminal *, void *, void *) = *(void (* const *)(struct terminal *, void *, void *))fn_;
	struct menu_item *mi;
	struct open_in_new *oin, *oi;
	if (!(oin = get_open_in_new(term->environment))) return;
	if (!oin[1].text) {
		fn(term, (void *)oin[0].open_window_fn, ses);
		free(oin);
		return;
	}
	mi = new_menu(MENU_FREE_ITEMS);
	for (oi = oin; oi->text; oi++)
		add_to_menu(&mi, oi->text, cast_uchar "", oi->hk, fn, (void *)oi->open_window_fn, 0, -1);
	free(oin);
	do_menu(term, mi, ses);
}

int can_open_in_new(struct terminal *term)
{
	struct open_in_new *oin = get_open_in_new(term->environment);
	if (!oin) return 0;
	if (!oin[1].text) {
		free(oin);
		return 1;
	}
	free(oin);
	return 2;
}

void save_url(void *ses_, unsigned char *url)
{
	struct session *ses = (struct session *)ses_;
	unsigned char *u1, *u2;
	u1 = convert(term_charset(ses->term), 0, url, NULL);
	u2 = translate_url(u1, ses->term->cwd);
	free(u1);
	if (!u2) {
		struct status stat = { init_list_1st(NULL) NULL, NULL, S_BAD_URL, PRI_CANCEL, 0, NULL, NULL, NULL, init_list_last(NULL) };
		print_error_dialog(ses, &stat, url);
		return;
	}
	free(ses->dn_url);
	ses->dn_url = u2;
	ses->dn_allow_flags = ALLOW_ALL;
	query_file(ses, ses->dn_url, NULL, start_download, NULL, DOWNLOAD_CONTINUE);
}

static void send_image(struct terminal *term, void *xxx, void *ses_)
{
	struct session *ses = ses_;
	unsigned char *u;
	struct f_data_c *fd = current_frame(ses);
	struct link *l;
	l = get_current_link(fd);
	if (!l) return;
	if (!(u = l->where_img)) return;
	goto_url_not_from_dialog(ses, u, fd);
}

#ifdef G

static void send_scale(struct terminal *term, void *xxx, void *ses_)
{
	struct session *ses = ses_;
	ses->ds.porn_enable ^= 1;
	html_interpret_recursive(ses->screen);
	draw_formatted(ses);
}

#endif

void save_as(struct terminal *term, void *xxx, void *ses_)
{
	struct session *ses = ses_;
	unsigned char *head;
	if (list_empty(ses->history)) return;
	free(ses->dn_url);
	ses->dn_url = stracpy(ses->screen->rq->url);
	ses->dn_allow_flags = ALLOW_ALL;
	if (!ses->dn_url) return;
	head = stracpy(ses->screen->rq->ce ? ses->screen->rq->ce->head : NULL);
	if (head) {
		unsigned char *p, *q;
		/* remove Content-Encoding from the header */
		q = parse_http_header(head, cast_uchar "Content-Encoding", &p);
		if (q) {
			free(q);
			if (p > head && p < (unsigned char *)strchr(cast_const_char head, 0)) {
				for (q = p - 1; q > head && *q != 10; q--)
					;
				q[1] = 'X';
			}
		}
	}
	query_file(ses, ses->dn_url, head, start_download, NULL, DOWNLOAD_CONTINUE);
	if (head)
		free(head);
}

static void save_formatted(struct session *ses, unsigned char *file, int mode)
{
	int h, rs, err;
	struct f_data_c *f;
	int download_mode = mode == DOWNLOAD_DEFAULT ? CDF_EXCL : 0;
	if (!(f = current_frame(ses)) || !f->f_data) return;
	if ((h = create_download_file(ses, ses->term->cwd, file, download_mode, 0)) < 0) return;
	if ((err = dump_to_file(f->f_data, h))) {
		msg_box(ses->term, NULL, TEXT_(T_SAVE_ERROR), AL_CENTER, TEXT_(T_ERROR_WRITING_TO_FILE), cast_uchar ": ", get_err_msg(err), MSG_BOX_END, NULL, 1, TEXT_(T_CANCEL), msg_box_null, B_ENTER | B_ESC);
	}
	EINTRLOOP(rs, close(h));
}

void menu_save_formatted(struct terminal *term, void *xxx, void *ses_)
{
	struct session *ses = ses_;
	struct f_data_c *f;
	if (!(f = current_frame(ses)) || !f->f_data) return;
	query_file(ses, f->rq->url, NULL, save_formatted, NULL, DOWNLOAD_OVERWRITE);
}

void link_menu(struct terminal *term, void *xxx, void *ses_)
{
	struct session *ses = ses_;
	struct f_data_c *f = current_frame(ses);
	struct link *link;
	struct menu_item *mi;
	free(ses->wtd_target);
	ses->wtd_target = NULL;
	mi = new_menu(MENU_FREE_ITEMS);
	link = get_current_link(f);
	if (!link) goto no_l;
	if (link->type == L_LINK && link->where) {
		if (strlen(cast_const_char link->where) >= 4 && !casecmp(link->where, cast_uchar "MAP@", 4)) {
			if (!F) {
				add_to_menu(&mi, TEXT_(T_DISPLAY_USEMAP), cast_uchar ">", TEXT_(T_HK_DISPLAY_USEMAP), send_enter, NULL, 1, -1);
			}
		} else {
			int c = can_open_in_new(term);
			add_to_menu(&mi, TEXT_(T_FOLLOW_LINK), cast_uchar "Enter", TEXT_(T_HK_FOLLOW_LINK), send_enter, NULL, 0, -1);
			if (c) add_to_menu(&mi, TEXT_(T_OPEN_IN_NEW_WINDOW), c - 1 ? cast_uchar ">" : cast_uchar "", TEXT_(T_HK_OPEN_IN_NEW_WINDOW), open_in_new_window, (void *)&send_open_in_new_xterm_ptr, c - 1, -1);
			if (!anonymous) add_to_menu(&mi, TEXT_(T_DOWNLOAD_LINK), cast_uchar "d", TEXT_(T_HK_DOWNLOAD_LINK), send_download, NULL, 0, -1);
			if (clipboard_support(term))
				add_to_menu(&mi, TEXT_(T_COPY_LINK_LOCATION), cast_uchar "", TEXT_(T_HK_COPY_LINK_LOCATION), copy_link_location, NULL, 0, -1);
			/*add_to_menu(&mi, TEXT_(T_ADD_BOOKMARK), cast_uchar "A", TEXT_(T_HK_ADD_BOOKMARK), menu_bookmark_manager, NULL, 0);*/

		}
	}
	if ((link->type == L_CHECKBOX || link->type == L_SELECT || link->type == L_FIELD || link->type == L_AREA) && link->form) {
		int c = can_open_in_new(term);
		add_to_menu(&mi, TEXT_(T_SUBMIT_FORM), cast_uchar "", TEXT_(T_HK_SUBMIT_FORM), send_submit, NULL, 0, -1);
		if (c && link->form->method == FM_GET) add_to_menu(&mi, TEXT_(T_SUBMIT_FORM_AND_OPEN_IN_NEW_WINDOW), c - 1 ? cast_uchar ">" : cast_uchar "", TEXT_(T_HK_SUBMIT_FORM_AND_OPEN_IN_NEW_WINDOW), open_in_new_window, (void *)&send_open_in_new_xterm_ptr, c - 1, -1);
		/*if (!anonymous) add_to_menu(&mi, TEXT_(T_SUBMIT_FORM_AND_DOWNLOAD), cast_uchar "d", TEXT_(T_HK_SUBMIT_FORM_AND_DOWNLOAD), send_download, NULL, 0, -1);*/
		add_to_menu(&mi, TEXT_(T_RESET_FORM), cast_uchar "", TEXT_(T_HK_RESET_FORM), send_reset, NULL, 0, -1);
	}
	if (link->type == L_BUTTON && link->form) {
		if (link->form->type == FC_RESET) add_to_menu(&mi, TEXT_(T_RESET_FORM), cast_uchar "", TEXT_(T_HK_RESET_FORM), send_enter, NULL, 0, -1);
		else if (link->form->type==FC_BUTTON)
			;
		else if (link->form->type == FC_SUBMIT || link->form->type == FC_IMAGE) {
			int c = can_open_in_new(term);
			add_to_menu(&mi, TEXT_(T_SUBMIT_FORM), cast_uchar "", TEXT_(T_HK_SUBMIT_FORM), send_enter, NULL, 0, -1);
			if (c && link->form->method == FM_GET) add_to_menu(&mi, TEXT_(T_SUBMIT_FORM_AND_OPEN_IN_NEW_WINDOW), c - 1 ? cast_uchar ">" : cast_uchar "", TEXT_(T_HK_SUBMIT_FORM_AND_OPEN_IN_NEW_WINDOW), open_in_new_window, (void *)&send_open_in_new_xterm_ptr, c - 1, -1);
			if (!anonymous) add_to_menu(&mi, TEXT_(T_SUBMIT_FORM_AND_DOWNLOAD), cast_uchar "d", TEXT_(T_HK_SUBMIT_FORM_AND_DOWNLOAD), send_download, NULL, 0, -1);
		}
	}
	if (link->where_img) {
		if (!F || f->f_data->opt.plain != 2) add_to_menu(&mi, TEXT_(T_VIEW_IMAGE), cast_uchar "i", TEXT_(T_HK_VIEW_IMAGE), send_image, NULL, 0, -1);
#ifdef G
		else add_to_menu(&mi, TEXT_(T_SCALE_IMAGE_TO_FULL_SCREEN), cast_uchar "Enter", TEXT_(T_HK_SCALE_IMAGE_TO_FULL_SCREEN), send_scale, NULL, 0, -1);
#endif
		if (!anonymous) add_to_menu(&mi, TEXT_(T_DOWNLOAD_IMAGE), cast_uchar "I", TEXT_(T_HK_DOWNLOAD_IMAGE), send_download_image, NULL, 0, -1);
	}
	no_l:
	if (!mi->text) add_to_menu(&mi, TEXT_(T_NO_LINK_SELECTED), cast_uchar "", M_BAR, NULL, NULL, 0, -1);
	do_menu(term, mi, ses);
}

static unsigned char *print_current_titlex(struct f_data_c *fd, int w)
{
	int mul, pul;
	int ml = 0, pl = 0;
	unsigned char *m, *p;
	if (!fd || !fd->vs || !fd->f_data) return NULL;
	w -= 1;
	p = init_str();
	if (fd->yw < fd->f_data->y) {
		int pp, pe;
		if (fd->yw) {
			pp = (fd->vs->view_pos + fd->yw / 2) / fd->yw + 1;
			pe = (fd->f_data->y + fd->yw - 1) / fd->yw;
		} else pp = pe = 1;
		if (pp > pe) pp = pe;
		if (fd->vs->view_pos + fd->yw >= fd->f_data->y) pp = pe;
		if (fd->f_data->title)
			add_chr_to_str(&p, &pl, ' ');
		add_to_str(&p, &pl, get_text_translation(TEXT_(T_PAGE_P), fd->ses->term));
		add_num_to_str(&p, &pl, pp);
		add_to_str(&p, &pl, get_text_translation(TEXT_(T_PAGE_OF), fd->ses->term));
		add_num_to_str(&p, &pl, pe);
		add_to_str(&p, &pl, get_text_translation(TEXT_(T_PAGE_CL), fd->ses->term));
	}
	if (!fd->f_data->title) return p;
	m = init_str();
	add_to_str(&m, &ml, fd->f_data->title);
	mul = strlen((char *)m);
	pul = strlen((char *)p);
	if (mul + pul > w) {
		unsigned char *mm;
		if ((mul = w - pul) < 0) mul = 0;
		for (mm = m; mul--; GET_TERM_CHAR(fd->ses->term, &mm))
			;
		ml = (int)(mm - m);
	}
	add_to_str(&m, &ml, p);
	free(p);
	return m;
}

static unsigned char *print_current_linkx(struct f_data_c *fd, struct terminal *term)
{
	int ll = 0;
	struct link *l;
	unsigned char *d;
	unsigned char *m = NULL;
	if (!fd || !fd->vs || !fd->f_data) return NULL;
	if (fd->vs->current_link == -1 || fd->vs->current_link >= fd->f_data->nlinks || fd->f_data->frame_desc) return NULL;
	l = &fd->f_data->links[fd->vs->current_link];
	if (l->type == L_LINK) {
		if (!l->where && l->where_img) {
			m = init_str();
			ll = 0;
			if (l->img_alt) {
				unsigned char *txt;

				txt = convert(fd->f_data->cp, fd->f_data->opt.cp, l->img_alt, &fd->f_data->opt);
				add_to_str(&m, &ll, txt);
				free(txt);
			} else {
				add_to_str(&m, &ll, get_text_translation(TEXT_(T_IMAGE), term));
				add_chr_to_str(&m, &ll, ' ');
				d = display_url(term, l->where_img, 1);
				add_to_str(&m, &ll, d);
				free(d);
			}
			goto p;
		}
		if (l->where && strlen(cast_const_char l->where) >= 4 && !casecmp(l->where, cast_uchar "MAP@", 4)) {
			m = init_str();
			ll = 0;
			add_to_str(&m, &ll, get_text_translation(TEXT_(T_USEMAP), term));
			add_chr_to_str(&m, &ll, ' ');
			d = display_url(term, l->where + 4, 1);
			add_to_str(&m, &ll, d);
			free(d);
			goto p;
		}
		if (l->where) {
			m = display_url(term, l->where, 1);
			goto p;
		}
		m = stracpy((unsigned char *)"");
		goto p;
	}
	if (!l->form) return NULL;
	if (l->type == L_BUTTON) {
		if (l->form->type == FC_BUTTON) {
			m = init_str();
			ll = 0;
			add_to_str(&m, &ll, get_text_translation(TEXT_(T_BUTTON), term));
			goto p;
		}
		if (l->form->type == FC_RESET) {
			m = stracpy(get_text_translation(TEXT_(T_RESET_FORM), term));
			goto p;
		}
		if (!l->form->action) return NULL;
		m = init_str();
		ll = 0;
		if (l->form->method == FM_GET) add_to_str(&m, &ll, get_text_translation(TEXT_(T_SUBMIT_FORM_TO), term));
		else add_to_str(&m, &ll, get_text_translation(TEXT_(T_POST_FORM_TO), term));
		add_chr_to_str(&m, &ll, ' ');
		add_to_str(&m, &ll, l->form->action);
		goto p;
	}
	if (l->type == L_CHECKBOX || l->type == L_SELECT || l->type == L_FIELD || l->type == L_AREA) {
		m = init_str();
		ll = 0;
		switch (l->form->type) {
		case FC_RADIO:
			add_to_str(&m, &ll, get_text_translation(TEXT_(T_RADIO_BUTTON), term));
			break;
		case FC_CHECKBOX:
			add_to_str(&m, &ll, get_text_translation(TEXT_(T_CHECKBOX), term));
			break;
		case FC_SELECT:
			add_to_str(&m, &ll, get_text_translation(TEXT_(T_SELECT_FIELD), term));
			break;
		case FC_TEXT:
			add_to_str(&m, &ll, get_text_translation(TEXT_(T_TEXT_FIELD), term));
			break;
		case FC_TEXTAREA:
			add_to_str(&m, &ll, get_text_translation(TEXT_(T_TEXT_AREA), term));
			break;
		case FC_FILE_UPLOAD:
			add_to_str(&m, &ll, get_text_translation(TEXT_(T_FILE_UPLOAD), term));
			break;
		case FC_PASSWORD:
			add_to_str(&m, &ll, get_text_translation(TEXT_(T_PASSWORD_FIELD), term));
			break;
		default:
			free(m);
			return NULL;
		}
		if (l->form->name && l->form->name[0]) {
			add_to_str(&m, &ll, cast_uchar ", ");
			add_to_str(&m, &ll, get_text_translation(TEXT_(T_NAME),
				term));
			add_chr_to_str(&m, &ll, ' ');
			add_to_str(&m, &ll, l->form->name);
		}
		if ((l->form->type == FC_CHECKBOX || l->form->type == FC_RADIO)
		&& l->form->default_value
		&& l->form->default_value[0]) {
			add_to_str(&m, &ll, cast_uchar ", ");
			add_to_str(&m, &ll, get_text_translation(TEXT_(T_VALUE),
				term));
			add_chr_to_str(&m, &ll, ' ');
			add_to_str(&m, &ll, l->form->default_value);
		}
				       /* pri enteru se bude posilat vzdycky   -- Brain */
		if (l->type == L_FIELD && !has_form_submit(fd->f_data, l->form)  && l->form->action) {
			add_to_str(&m, &ll, cast_uchar ", ");
			add_to_str(&m, &ll, get_text_translation(TEXT_(T_HIT_ENTER_TO), term));
			add_chr_to_str(&m, &ll, ' ');
			if (l->form->method == FM_GET) add_to_str(&m, &ll, get_text_translation(TEXT_(T_SUBMIT_TO), term));
			else add_to_str(&m, &ll, get_text_translation(TEXT_(T_POST_TO), term));
			add_chr_to_str(&m, &ll, ' ');
			add_to_str(&m, &ll, l->form->action);
		}
		goto p;
	}
	p:
	return m;
}

/* jako print_current_linkx, ale vypisuje vice informaci o obrazku
   pouziva se v informacich o dokumentu

   Ach jo, to Brain kopiroval kod, snad to nedela i v ty firme,
   kde ted pracuje... -- mikulas
 */
static unsigned char *print_current_linkx_plus(struct f_data_c *fd, struct terminal *term)
{
	int ll = 0;
	struct link *l;
	unsigned char *d;
	unsigned char *m = NULL;
	if (!fd || !fd->vs || !fd->f_data) return NULL;
	if (fd->vs->current_link == -1 || fd->vs->current_link >= fd->f_data->nlinks || fd->f_data->frame_desc) return NULL;
	l = &fd->f_data->links[fd->vs->current_link];
	if (l->type == L_LINK) {
		unsigned char *spc;
		m = init_str();
		ll = 0;
		if (l->where && strlen(cast_const_char l->where) >= 4 && !casecmp(l->where, cast_uchar "MAP@", 4)) {
			add_to_str(&m, &ll, get_text_translation(TEXT_(T_USEMAP), term));
			add_chr_to_str(&m, &ll, ' ');
			d = display_url(term, l->where + 4, 1);
			add_to_str(&m, &ll, d);
			free(d);
		}
		else if (l->where) {
			d = display_url(term, l->where, 1);
			add_to_str(&m, &ll, d);
			free(d);
		}
		spc = stracpy((unsigned char *)"");
		if (spc&&*spc)
		{
			add_to_str(&m, &ll, cast_uchar "\n");
			add_to_str(&m, &ll, get_text_translation(TEXT_(T_JAVASCRIPT), term));
			add_to_str(&m, &ll, cast_uchar ": ");
			add_to_str(&m, &ll, spc);
		}
		free(spc);
		if (l->where_img) {
			add_to_str(&m, &ll, cast_uchar "\n");
			add_to_str(&m, &ll, get_text_translation(TEXT_(T_IMAGE), term));
			add_to_str(&m, &ll, cast_uchar ": src='");
			d = display_url(term, l->where_img, 1);
			add_to_str(&m, &ll, d);
			free(d);
			add_chr_to_str(&m, &ll, '\'');

			if (l->img_alt)
			{
				unsigned char *txt;

				add_to_str(&m, &ll, cast_uchar " alt='");
				txt = convert(fd->f_data->cp, fd->f_data->opt.cp, l->img_alt, &fd->f_data->opt);
				add_to_str(&m, &ll, txt);
				add_chr_to_str(&m, &ll, '\'');
				free(txt);
			}
#ifdef G
			if (F&&l->obj)
			{
				add_to_str(&m, &ll, cast_uchar " size='");
				add_num_to_str(&m, &ll, l->obj->xw);
				add_chr_to_str(&m, &ll, 'x');
				add_num_to_str(&m, &ll, l->obj->yw);
				add_chr_to_str(&m, &ll, '\'');
			}
#endif
			goto p;
		}
		goto p;
	}
	if (!l->form) return NULL;
	if (l->type == L_BUTTON) {
		if (l->form->type == FC_BUTTON) {
			m = init_str();
			ll = 0;
			add_to_str(&m, &ll, get_text_translation(TEXT_(T_BUTTON), term));
			goto p;
		}
		if (l->form->type == FC_RESET) {
			m = stracpy(get_text_translation(TEXT_(T_RESET_FORM), term));
			goto p;
		}
		if (!l->form->action) return NULL;
		m = init_str();
		ll = 0;
		if (l->form->method == FM_GET) add_to_str(&m, &ll, get_text_translation(TEXT_(T_SUBMIT_FORM_TO), term));
		else add_to_str(&m, &ll, get_text_translation(TEXT_(T_POST_FORM_TO), term));
		add_chr_to_str(&m, &ll, ' ');
		add_to_str(&m, &ll, l->form->action);
		goto p;
	}
	if (l->type == L_CHECKBOX || l->type == L_SELECT || l->type == L_FIELD || l->type == L_AREA) {
		m = init_str();
		ll = 0;
		switch (l->form->type) {
		case FC_RADIO:
			add_to_str(&m, &ll, get_text_translation(TEXT_(T_RADIO_BUTTON), term));
			break;
		case FC_CHECKBOX:
			add_to_str(&m, &ll, get_text_translation(TEXT_(T_CHECKBOX), term));
			break;
		case FC_SELECT:
			add_to_str(&m, &ll, get_text_translation(TEXT_(T_SELECT_FIELD), term));
			break;
		case FC_TEXT:
			add_to_str(&m, &ll, get_text_translation(TEXT_(T_TEXT_FIELD), term));
			break;
		case FC_TEXTAREA:
			add_to_str(&m, &ll, get_text_translation(TEXT_(T_TEXT_AREA), term));
			break;
		case FC_FILE_UPLOAD:
			add_to_str(&m, &ll, get_text_translation(TEXT_(T_FILE_UPLOAD), term));
			break;
		case FC_PASSWORD:
			add_to_str(&m, &ll, get_text_translation(TEXT_(T_PASSWORD_FIELD), term));
			break;
		default:
			free(m);
			return NULL;
		}
		if (l->form->name && l->form->name[0]) {
			add_to_str(&m, &ll, cast_uchar ", ");
			add_to_str(&m, &ll, get_text_translation(TEXT_(T_NAME),
				term));
			add_chr_to_str(&m, &ll, ' ');
			add_to_str(&m, &ll, l->form->name);
		}
		if ((l->form->type == FC_CHECKBOX || l->form->type == FC_RADIO)
		&& l->form->default_value
		&& l->form->default_value[0]) {
			add_to_str(&m, &ll, cast_uchar ", ");
			add_to_str(&m, &ll, get_text_translation(TEXT_(T_VALUE),
				term));
			add_chr_to_str(&m, &ll, ' ');
			add_to_str(&m, &ll, l->form->default_value);
		}
				       /* pri enteru se bude posilat vzdycky   -- Brain */
		if (l->type == L_FIELD && !has_form_submit(fd->f_data, l->form)  && l->form->action) {
			add_to_str(&m, &ll, cast_uchar ", ");
			add_to_str(&m, &ll, get_text_translation(TEXT_(T_HIT_ENTER_TO), term));
			add_chr_to_str(&m, &ll, ' ');
			if (l->form->method == FM_GET) add_to_str(&m, &ll, get_text_translation(TEXT_(T_SUBMIT_TO), term));
			else add_to_str(&m, &ll, get_text_translation(TEXT_(T_POST_TO), term));
			add_chr_to_str(&m, &ll, ' ');
			add_to_str(&m, &ll, l->form->action);
		}
		goto p;
	}
p:
	return m;
}

unsigned char *print_current_link(struct session *ses)
{
	return print_current_linkx(current_frame(ses), ses->term);
}

unsigned char *print_current_title(struct session *ses)
{
	return print_current_titlex(current_frame(ses), ses->term->x);
}

void loc_msg(struct terminal *term, struct location *lo, struct f_data_c *frame)
{
	struct cache_entry *ce;
	unsigned char *s;
	int l = 0;
	unsigned char *a;
	if (!lo || !frame || !frame->vs || !frame->f_data) {
		msg_box(term, NULL, TEXT_(T_INFO), AL_LEFT, TEXT_(T_YOU_ARE_NOWHERE), MSG_BOX_END, NULL, 1, TEXT_(T_OK), msg_box_null, B_ENTER | B_ESC);
		return;
	}
	s = init_str();
	add_to_str(&s, &l, get_text_translation(TEXT_(T_URL), term));
	add_to_str(&s, &l, cast_uchar ": ");
	a = display_url(term, lo->url, 1);
	add_to_str(&s, &l, a);
	free(a);
	if (!find_in_cache(lo->url, &ce)) {
		unsigned char *start;
		size_t len;
		if (ce->ip_address) {
			add_to_str(&s, &l, cast_uchar "\n");
			if (!strchr(cast_const_char ce->ip_address, ' '))
				add_to_str(&s, &l, get_text_translation(TEXT_(T_IP_ADDRESS), term));
			else
				add_to_str(&s, &l, get_text_translation(TEXT_(T_IP_ADDRESSES), term));
			add_to_str(&s, &l, cast_uchar ": ");
			add_to_str(&s, &l, ce->ip_address);
		}
		add_to_str(&s, &l, cast_uchar "\n");
		add_to_str(&s, &l, get_text_translation(TEXT_(T_SIZE), term));
		add_to_str(&s, &l, cast_uchar ": ");
		get_file_by_term(NULL, ce, &start, &len, NULL);
		if (ce->decompressed) {
			unsigned char *enc;
			add_unsigned_long_num_to_str(&s, &l, len);
			enc = get_content_encoding(ce->head, ce->url, 0);
			if (enc) {
				add_to_str(&s, &l, cast_uchar " (");
				add_num_to_str(&s, &l, ce->length);
				add_chr_to_str(&s, &l, ' ');
				add_to_str(&s, &l, get_text_translation(TEXT_(T_COMPRESSED_WITH), term));
				add_chr_to_str(&s, &l, ' ');
				add_to_str(&s, &l, enc);
				add_chr_to_str(&s, &l, ')');
				free(enc);
			}
		} else {
			add_num_to_str(&s, &l, ce->length);
		}
		if (ce->incomplete) {
			add_to_str(&s, &l, cast_uchar " (");
			add_to_str(&s, &l, get_text_translation(TEXT_(T_INCOMPLETE), term));
			add_chr_to_str(&s, &l, ')');
		}
		if (frame->f_data->ass >= 0) {
			add_to_str(&s, &l, cast_uchar "\n");
			add_to_str(&s, &l, get_text_translation(TEXT_(T_CODEPAGE), term));
			add_to_str(&s, &l, cast_uchar ": ");
			add_to_str(&s, &l, get_cp_name(frame->f_data->cp));
			if (frame->f_data->ass == 1) {
				add_to_str(&s, &l, cast_uchar " (");
				add_to_str(&s, &l, get_text_translation(TEXT_(T_ASSUMED), term));
				add_chr_to_str(&s, &l, ')');
			}
			if (frame->f_data->ass == 2) {
				add_to_str(&s, &l, cast_uchar " (");
				add_to_str(&s, &l, get_text_translation(TEXT_(T_IGNORING_SERVER_SETTING), term));
				add_chr_to_str(&s, &l, ')');
			}
		}
		if (ce->head && ce->head[0] != '\n' && ce->head[0] != '\r' && (a = parse_http_header(ce->head, cast_uchar "Content-Type", NULL))) {
			add_to_str(&s, &l, cast_uchar "\n");
			add_to_str(&s, &l, get_text_translation(TEXT_(T_CONTENT_TYPE), term));
			add_to_str(&s, &l, cast_uchar ": ");
			add_to_str(&s, &l, a);
			free(a);
		}
		if ((a = parse_http_header(ce->head, cast_uchar "Server", NULL))) {
			add_to_str(&s, &l, cast_uchar "\n");
			add_to_str(&s, &l, get_text_translation(TEXT_(T_SERVER), term));
			add_to_str(&s, &l, cast_uchar ": ");
			add_to_str(&s, &l, a);
			free(a);
		}
		if ((a = parse_http_header(ce->head, cast_uchar "Date", NULL))) {
			add_to_str(&s, &l, cast_uchar "\n");
			add_to_str(&s, &l, get_text_translation(TEXT_(T_DATE), term));
			add_to_str(&s, &l, cast_uchar ": ");
			add_to_str(&s, &l, a);
			free(a);
		}
		if ((a = parse_http_header(ce->head, cast_uchar "Last-Modified", NULL))) {
			add_to_str(&s, &l, cast_uchar "\n");
			add_to_str(&s, &l, get_text_translation(TEXT_(T_LAST_MODIFIED), term));
			add_to_str(&s, &l, cast_uchar ": ");
			add_to_str(&s, &l, a);
			free(a);
		}
		if (ce->ssl_info) {
			add_to_str(&s, &l, cast_uchar "\n");
			add_to_str(&s, &l, get_text_translation(TEXT_(T_SSL_CIPHER), term));
			add_to_str(&s, &l, cast_uchar ": ");
			add_to_str(&s, &l, ce->ssl_info);
		}
		if (ce->ssl_authority) {
			add_to_str(&s, &l, cast_uchar "\n");
			if (strstr(cast_const_char ce->ssl_authority, cast_const_char CERT_RIGHT_ARROW))
				add_to_str(&s, &l, get_text_translation(TEXT_(T_CERTIFICATE_AUTHORITIES), term));
			else
				add_to_str(&s, &l, get_text_translation(TEXT_(T_CERTIFICATE_AUTHORITY), term));
			add_to_str(&s, &l, cast_uchar ": ");
			add_to_str(&s, &l, ce->ssl_authority);
		}
		ce->refcount--;
	}
	if ((a = print_current_linkx_plus(frame, term))) {
		add_to_str(&s, &l, cast_uchar "\n\n");
		if (*a != '\n') {
			add_to_str(&s, &l, get_text_translation(TEXT_(T_LINK), term));
			add_to_str(&s, &l, cast_uchar ": ");
			add_to_str(&s, &l, a);
		} else
			add_to_str(&s, &l, a + 1);
		free(a);
	}
	msg_box(term, getml(s, NULL), TEXT_(T_INFO), AL_LEFT, s, MSG_BOX_END, NULL, 1, TEXT_(T_OK), msg_box_null, B_ENTER | B_ESC);
}

void state_msg(struct session *ses)
{
	if (list_empty(ses->history)) loc_msg(ses->term, NULL, NULL);
	else loc_msg(ses->term, cur_loc(ses), current_frame(ses));
}

void head_msg(struct session *ses)
{
	struct cache_entry *ce;
	unsigned char *s, *ss;
	int len;
	if (list_empty(ses->history)) {
		msg_box(ses->term, NULL, TEXT_(T_HEADER_INFO), AL_LEFT, TEXT_(T_YOU_ARE_NOWHERE), MSG_BOX_END, NULL, 1, TEXT_(T_OK), msg_box_null, B_ENTER | B_ESC);
		return;
	}
	if (!find_in_cache(cur_loc(ses)->url, &ce)) {
		if (ce->head) s = stracpy(ce->head);
		else s = stracpy(cast_uchar "");
		len = (int)strlen(cast_const_char s) - 1;
		if (len > 0) {
			while ((ss = cast_uchar strstr(cast_const_char s, "\r\n"))) memmove(ss, ss + 1, strlen(cast_const_char ss));
			while (*s && s[strlen(cast_const_char s) - 1] == '\n') s[strlen(cast_const_char s) - 1] = 0;
		}
		if (*s && *s != '\n') {
			msg_box(ses->term, getml(s, NULL), TEXT_(T_HEADER_INFO), AL_LEFT, s, MSG_BOX_END, NULL, 1, TEXT_(T_OK), msg_box_null, B_ENTER | B_ESC);
		} else {
			msg_box(ses->term, getml(s, NULL), TEXT_(T_HEADER_INFO), AL_CENTER, TEXT_(T_NO_HEADER), MSG_BOX_END, NULL, 1, TEXT_(T_OK), msg_box_null, B_ENTER | B_ESC);
		}
		ce->refcount--;
	}
}
