/* terminal.c
 * (c) 2002 Mikulas Patocka
 * This file is a part of the Links program, released under GPL.
 */

#include "links.h"

static void in_term(void *);
static void check_if_no_terminal(void);

int hard_write(int fd, const unsigned char *p, int l)
{
	int t = 0;
	while (l > 0) {
		int w;
		EINTRLOOP(w, (int)write(fd, p, l));
		if (w < 0)
			return -1;
		if (!w) {
			errno = ENOSPC;
			break;
		}
		t += w;
		p += w;
		l -= w;
	}
	return t;
}

int hard_read(int fd, unsigned char *p, int l)
{
	int r = 1;
	int t = 0;
	while (l > 0 && r) {
		EINTRLOOP(r, (int)read(fd, p, l));
		if (r < 0)
			return -1;
		t += r;
		p += r;
		l -= r;
	}
	return t;
}

unsigned char *get_cwd(void)
{
	int bufsize = 128;
	unsigned char *buf;
	unsigned char *gcr;
	while (1) {
		buf = xmalloc(bufsize);
		ENULLLOOP(gcr, cast_uchar getcwd(cast_char buf, bufsize));
		if (gcr) return buf;
		free(buf);
		if (errno != ERANGE) break;
		if ((unsigned)bufsize > INT_MAX - 128) overalloc();
		bufsize += 128;
	}
	return NULL;
}

void set_cwd(unsigned char *path)
{
	int rs;
	if (path)
		EINTRLOOP(rs, chdir(cast_const_char path));
}

unsigned char get_attribute(int fg, int bg)
{
	return ((bg & 7) << 3) | (fg & 7) | ((fg & 8) << 3);
}

struct list_head terminals = {&terminals, &terminals};

static void set_margin(struct terminal *term)
{
	term->left_margin = 0;
	term->x = term->real_x;
	term->top_margin = 0;
	term->y = term->real_y;
}


static void alloc_term_screen(struct terminal *term)
{
	chr *s, *t;
	if (term->x < 0)
		term->x = 1;
	if (term->y < 0)
		term->y = 1;
	if ((term->x && (unsigned)term->x * (unsigned)term->y / (unsigned)term->x != (unsigned)term->y)
	|| (unsigned)term->x * (unsigned)term->y > INT_MAX / sizeof(*term->screen))
		overalloc();
	s = xrealloc(term->screen, term->x * term->y * sizeof(*term->screen));
	t = xrealloc(term->last_screen,
		term->x * term->y * sizeof(*term->screen));
	memset(t, -1, term->x * term->y * sizeof(*term->screen));
	term->last_screen = t;
	memset(s, 0, term->x * term->y * sizeof(*term->screen));
	term->screen = s;
	term->dirty = 1;
	term->lcx = -1;
	term->lcy = -1;
}

static void clear_terminal(struct terminal *term)
{
	fill_area(term, 0, 0, term->x, term->y, ' ', 0);
	set_cursor(term, 0, 0, 0, 0);
}

void redraw_below_window(struct window *win)
{
	int tr;
	struct terminal *term = win->term;
	struct window *w = NULL;
	struct list_head *lw;
	struct links_event ev = { EV_REDRAW, 0, 0, 0 };
	ev.x = term->x;
	ev.y = term->y;
	if (term->redrawing >= 2) return;
	tr = term->redrawing;
	win->term->redrawing = 2;
	foreachback(struct window, w, lw, term->windows) {
		if (w == win)
			break;
		w->handler(w, &ev, 0);
	}
	term->redrawing = tr;
}

static void redraw_terminal_ev(struct terminal *term, int e)
{
	struct window *win = NULL;
	struct list_head *lwin;
	struct links_event ev = {0, 0, 0, 0};
	ev.ev = e;
	ev.x = term->x;
	ev.y = term->y;
	clear_terminal(term);
	term->redrawing = 2;
	foreachback(struct window, win, lwin, term->windows) win->handler(win, &ev, 0);
	term->redrawing = 0;
}

static void redraw_terminal(struct terminal *term)
{
	redraw_terminal_ev(term, EV_REDRAW);
}

static void redraw_terminal_all(struct terminal *term)
{
	redraw_terminal_ev(term, EV_RESIZE);
}

static void erase_screen(struct terminal *term)
{
	if (!term->master || !is_blocked())
		hard_write(term->fdout, cast_uchar "\033[2J\033[1;1H", 10);
}

static void redraw_terminal_cls(struct terminal *term)
{
	erase_screen(term);
	set_margin(term);
	alloc_term_screen(term);
	redraw_terminal_all(term);
}

void cls_redraw_all_terminals(void)
{
	struct terminal *term = NULL;
	struct list_head *lterm;
	foreach(struct terminal, term, lterm, terminals) {
		if (!F) redraw_terminal_cls(term);
#ifdef G
		else
			term->dev->resize_handler(term->dev);
#endif
	}
}

void draw_to_window(struct window *win, void (*fn)(struct terminal *term, void *), void *data)
{
	struct terminal *term = win->term;
	struct window *w = NULL;
	struct list_head *lw;
	if (!F) {
		pr(fn(term, data)) {};
		term = win->term;
		if (win->list_entry.prev == &term->windows || term->redrawing) return;
		term->redrawing = 1;
		{
			struct links_event ev = { EV_REDRAW, 0, 0, 0 };
			ev.x = term->x;
			ev.y = term->y;
			foreachbackfrom(struct window, w, lw, term->windows, win->list_entry.prev)
				w->handler(w, &ev, 0);
		}
		term->redrawing = 0;
#ifdef G
	} else {
		struct rect r1, *r;
		struct rect_set *s;
		int i, a;
		if (win->list_entry.prev == &term->windows) {
			pr(fn(term, data)) {};
			return;
		}
		s = init_rect_set();
		intersect_rect(&r1, &win->pos, &term->dev->clip);
		add_to_rect_set(&s, &r1);
		foreachbackfrom(struct window, w, lw, term->windows, win->list_entry.prev)
			exclude_rect_from_set(&s, &w->pos);
		a = 0;
		memcpy(&r1, &term->dev->clip, sizeof(struct rect));
		for (i = 0; i < s->m; i++) if (is_rect_valid(r = &s->r[i])) {
			set_clip_area(term->dev, r);
			pr(fn(term, data)) {
			}
			a = 1;
		}
		if (!a) {
			struct rect empty = { 0, 0, 0, 0 };
			set_clip_area(term->dev, &empty);
			fn(term, data);
		}
		set_clip_area(term->dev, &r1);
		free(s);
#endif
	}
}

#ifdef G

static void redraw_windows(void *term_)
{
	struct terminal *term = (struct terminal *)term_;
	struct terminal *t1 = NULL;
	struct list_head *lt1;
	struct window *win = NULL;
	struct list_head *lwin;
	foreach(struct terminal, t1, lt1, terminals) if (t1 == term) goto ok;
	return;
ok:
	foreach(struct window, win, lwin, term->windows) {
		struct links_event ev = { EV_REDRAW, 0, 0, 0 };
		ev.x = term->x;
		ev.y = term->y;
		set_clip_area(term->dev, &win->redr);
		memset(&win->redr, 0, sizeof(struct rect));
		win->handler(win, &ev, 0);
	}
	set_clip_area(term->dev, &term->dev->size);
}

void set_window_pos(struct window *win, int x1, int y1, int x2, int y2)
{
	struct terminal *term = win->term;
	struct rect r;
	r.x1 = x1;
	r.y1 = y1;
	r.x2 = x2;
	r.y2 = y2;
	if (is_rect_valid(&win->pos) && (x1 > win->pos.x1 || x2 < win->pos.x2 || y1 > win->pos.y1 || y2 < win->pos.y2) && term->redrawing < 2) {
		struct window *w = NULL;
		struct list_head *lw;
		foreachfrom(struct window, w, lw, term->windows, win->list_entry.next) unite_rect(&w->redr, &win->pos, &w->redr);
		register_bottom_half(redraw_windows, term);
	}
	memcpy(&win->pos, &r, sizeof(struct rect));
}

#endif

void add_window(struct terminal *term, void (*handler)(struct window *, struct links_event *, int), void *data)
{
	struct links_event ev = { EV_INIT, 0, 0, 0 };
	struct window *win;
	ev.x = term->x;
	ev.y = term->y;
	win = mem_calloc(sizeof(struct window));
	win->handler = handler;
	win->data = data;
	win->term = term;
	win->xp = win->yp = 0;
	add_to_list(term->windows, win);
	win->handler(win, &ev, 0);
}

void delete_window(struct window *win)
{
#ifdef G
	struct list_head *nxw;
#endif
	struct terminal *term = win->term;
	struct links_event ev = { EV_ABORT, 0, 0, 0 };
	win->handler(win, &ev, 1);
#ifdef G
	nxw = win->list_entry.next;
#endif
	del_from_list(win);
	free(win->data);
	if (!F) redraw_terminal(term);
#ifdef G
	else {
		struct window *w = NULL;
		struct list_head *lw;
		foreachfrom(struct window, w, lw, term->windows, nxw) unite_rect(&w->redr, &win->pos, &w->redr);
		register_bottom_half(redraw_windows, term);
	}
#endif
	free(win);
}

void delete_window_ev(struct window *win, struct links_event *ev)
{
	struct terminal *term = win->term;
	struct list_head *lw = win->list_entry.next;
	delete_window(win);
	if (ev && lw != &term->windows) {
		struct window *w = list_struct(lw, struct window);
		w->handler(w, ev, 1);
	}
}

void set_window_ptr(struct window *win, int x, int y)
{
	if (win->xp == x && win->yp == y) return;
	win->xp = x;
	win->yp = y;
#ifdef G
	{
		struct terminal *term = win->term;
		if (F && win->list_entry.prev != &term->windows) {
			struct window *prev = list_struct(win->list_entry.prev, struct window);
			memcpy(&prev->redr, &term->dev->size, sizeof(struct rect));
			register_bottom_half(redraw_windows, term);
		}
	}
#endif
}

void get_parent_ptr(struct window *win, int *x, int *y)
{
	if (win->list_entry.next != &win->term->windows) {
		struct window *next = list_struct(win->list_entry.next, struct window);
		*x = next->xp;
		*y = next->yp;
	} else {
		*x = *y = 0;
	}
}

struct ewd {
	void (*fn)(void *);
	void *data;
	int b;
};

static void empty_window_handler(struct window *win, struct links_event *ev, int fwd)
{
	struct terminal *term = win->term;
	struct list_head *ln;
	struct ewd *ewd = win->data;
	int x, y;
	void (*fn)(void *) = ewd->fn;
	void *data = ewd->data;
	if (ewd->b)
		return;
	switch ((int)ev->ev) {
	case EV_INIT:
	case EV_RESIZE:
	case EV_REDRAW:
		get_parent_ptr(win, &x, &y);
		set_window_ptr(win, x, y);
		return;
	case EV_ABORT:
		fn(data);
		return;
	}
	ewd->b = 1;
	ln = win->list_entry.next;
	delete_window(win);
	fn(data);
	if (ln != &term->windows) {
		struct window *n = list_struct(ln, struct window);
		n->handler(n, ev, fwd);
	}
}

void add_empty_window(struct terminal *term, void (*fn)(void *), void *data)
{
	struct ewd *ewd;
	ewd = xmalloc(sizeof(struct ewd));
	ewd->fn = fn;
	ewd->data = data;
	ewd->b = 0;
	add_window(term, empty_window_handler, ewd);
}

struct list_head term_specs = { &term_specs, &term_specs };

void free_term_specs(void)
{
	free_list(struct term_spec, term_specs);
}

static struct term_spec dumb_term = { init_list_1st(NULL) "", 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, init_list_last(NULL) };
static struct term_spec cygwin_term = { init_list_1st(NULL) "", 2, 1, 1, 0, 1, 0, 0, 0, 0, 0, init_list_last(NULL) };

static struct term_spec *default_term_spec(unsigned char *term)
{
	if (!casestrcmp(term, cast_uchar "cygwin"))
		return &cygwin_term;
	return &dumb_term;
}

static struct term_spec *get_term_spec(unsigned char *term)
{
	struct term_spec *t = NULL;
	struct list_head *lt;
	foreach(struct term_spec, t, lt, term_specs) if (!casestrcmp(t->term, term)) return t;
	return default_term_spec(term);
}

static void sync_term_specs(void)
{
	struct terminal *term = NULL;
	struct list_head *lterm;
	foreach(struct terminal, term, lterm, terminals) term->spec = get_term_spec(term->term);
}

struct term_spec *new_term_spec(unsigned char *term)
{
	struct term_spec *t = NULL;
	struct list_head *lt;
	foreach(struct term_spec, t, lt, term_specs) if (!casestrcmp(t->term, term)) return t;
	t = xmalloc(sizeof(struct term_spec));
	memcpy(t, default_term_spec(term), sizeof(struct term_spec));
	if (strlen(cast_const_char term) < MAX_TERM_LEN) strcpy(cast_char t->term, cast_const_char term);
	else {
		memcpy(t->term, term, MAX_TERM_LEN - 1);
		t->term[MAX_TERM_LEN - 1] = 0;
	}
	add_to_list(term_specs, t);
	sync_term_specs();
	return t;
}

struct terminal *init_term(int fdin, int fdout, void (*root_window)(struct window *, struct links_event *, int))
{
	static tcount terminal_count = 0;
	struct terminal *term;
	struct window *win;
	term = mem_calloc(sizeof(struct terminal));
	term->count = terminal_count++;
	term->fdin = fdin;
	term->fdout = fdout;
	term->master = term->fdout == 1;
	term->lcx = -1;
	term->lcy = -1;
	term->dirty = 1;
	term->blocked = -1;
	term->screen = NULL;
	term->last_screen = NULL;
	term->spec = default_term_spec(cast_uchar "");
	term->input_queue = NULL;
	init_list(term->windows);
	term->handle_to_close = -1;
	win = mem_calloc(sizeof(struct window));
	win->handler = root_window;
	win->term = term;
	add_to_list(term->windows, win);
	add_to_list(terminals, term);
	set_handlers(fdin, in_term, NULL, term);
	return term;
}

static int process_utf_8(struct terminal *term, struct links_event *ev)
{
	if (ev->ev == EV_KBD) {
		if ((!F && !term_charset(term))
#ifdef G
		|| (F && !(drv->flags & GD_UNICODE_KEYS) && !g_kbd_codepage(drv))
#endif
		) {
			size_t l;
			unsigned char *p;
			unsigned c;
			if (ev->x <= 0 || ev->x >= 0x100) goto direct;
			if ((term->utf8_paste_mode ^ ev->y) & KBD_PASTING) {
				term->utf8_paste_mode = ev->y & KBD_PASTING;
				term->utf8_buffer[0] = 0;
			}
			if ((l = strlen(cast_const_char term->utf8_buffer))
				>= sizeof(term->utf8_buffer) - 1
			|| ev->x < 0x80 || ev->x >= 0xc0) {
				term->utf8_buffer[0] = 0;
				l = 0;
			}
			term->utf8_buffer[l] = (unsigned char)ev->x;
			term->utf8_buffer[l + 1] = 0;
			p = term->utf8_buffer;
			GET_UTF_8(p, c);
			if (!c) return 0;
			ev->x = c;
		}
direct:
		term->utf8_buffer[0] = 0;
	}
	return 1;
}

#ifdef G

static struct term_spec gfx_term = { init_list_1st(NULL) "", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, init_list_last(NULL) };

struct terminal *init_gfx_term(void (*root_window)(struct window *, struct links_event *, int), unsigned char *cwd, void *info, int len)
{
	static tcount terminal_count = 0;
	struct terminal *term;
	struct graphics_device *dev;
	struct window *win;
	term = mem_calloc(sizeof(struct terminal));
	term->count = terminal_count++;
	term->fdin = -1;
	if (!(term->dev = dev = drv->init_device())) {
		free(term);
		check_if_no_terminal();
		return NULL;
	}
	dev->user_data = term;
	term->master = 1;
	term->blocked = -1;
	term->x = dev->size.x2;
	term->y = dev->size.y2;
	term->last_mouse_x = term->last_mouse_y = term->last_mouse_b = INT_MAX;
	term->environment = !(drv->flags & GD_ONLY_1_WINDOW) ? ENV_G : 0;
	if (!casestrcmp(drv->name, cast_uchar "x")) term->environment |= ENV_XWIN;
	term->spec = &gfx_term;
	term->default_character_set = 0;
	safe_strncpy(term->cwd, cwd, MAX_CWD_LEN);
	gfx_term.character_set = 0;
	if (gfx_term.character_set == -1) gfx_term.character_set = 0;
	init_list(term->windows);
	term->handle_to_close = -1;
	win = mem_calloc(sizeof (struct window));
	win->handler = root_window;
	win->term = term;
	win->pos.x2 = dev->size.x2;
	win->pos.y2 = dev->size.y2;
	add_to_list(term->windows, win);
	add_to_list(terminals, term);
	dev->redraw_handler = t_redraw;
	dev->resize_handler = t_resize;
	dev->keyboard_handler = t_kbd;
	dev->mouse_handler = t_mouse;
	{
		int *ptr;
		struct links_event ev = { EV_INIT, 0, 0, 0 };
		ev.x = dev->size.x2;
		ev.y = dev->size.y2;
		if ((unsigned)len > INT_MAX - sizeof(int)) overalloc();
		ptr = xmalloc(sizeof(int) + len);
		*ptr = len;
		memcpy(ptr + 1, info, len);
		ev.b = (long)ptr;
		root_window(win, &ev, 0);
		free(ptr);
	}
	return term;
}

void t_redraw(struct graphics_device *dev, struct rect *r)
{
	struct terminal *term = dev->user_data;
	struct window *win = NULL;
	struct list_head *lwin;
	foreach(struct window, win, lwin, term->windows) unite_rect(&win->redr, r, &win->redr);
	register_bottom_half(redraw_windows, term);
}

void t_resize(struct graphics_device *dev)
{
	struct terminal *term = dev->user_data;
	struct window *win = NULL;
	struct list_head *lwin;
	struct links_event ev = { EV_RESIZE, 0, 0, 0 };
	term->x = ev.x = dev->size.x2;
	term->y = ev.y = dev->size.y2;
	set_clip_area(dev, &dev->size);
	foreach(struct window, win, lwin, term->windows) {
		win->handler(win, &ev, 0);
	}
	set_clip_area(dev, &dev->size);
}

void t_kbd(struct graphics_device *dev, int key, int flags)
{
	struct terminal *term = dev->user_data;
	struct links_event ev = { EV_KBD, 0, 0, 0 };
	struct rect r = { 0, 0, 0, 0 };
	r.x2 = dev->size.x2;
	r.y2 = dev->size.y2;
	ev.x = key;
	ev.y = flags;
	if (upcase(ev.x) == 'L' && !(ev.y & KBD_PASTING) && ev.y & KBD_CTRL) {
		t_redraw(dev, &r);
		return;
	} else {
		if (ev.x == KBD_STOP)
			abort_background_connections();
		set_clip_area(dev, &r);
		if (list_empty(term->windows)) return;
		if (ev.x == KBD_CTRL_C || ev.x == KBD_CLOSE) {
			struct window *prev = list_struct(term->windows.prev, struct window);
			prev->handler(prev, &ev, 0);
		} else {
			if (process_utf_8(term, &ev)) {
				struct window *next = list_struct(term->windows.next, struct window);
				next->handler(next, &ev, 0);
			}
		}
	}
}

void t_mouse(struct graphics_device *dev, int x, int y, int b)
{
	struct terminal *term = dev->user_data;
	struct links_event ev = { EV_MOUSE, 0, 0, 0 };
	struct rect r = {0, 0, 0, 0};
	int bt, ac;
	struct window *next;
	if (x == term->last_mouse_x && y == term->last_mouse_y && b == term->last_mouse_b) {
		return;
	}
	bt = b & BM_BUTT;
	ac = b & BM_ACT;
	if ((ac == B_MOVE || ac == B_DRAG) &&
	    (bt == B_LEFT || bt == B_MIDDLE || bt == B_RIGHT || bt == B_FOURTH || bt == B_FIFTH || bt == B_SIXTH)) {
		term->last_mouse_x = x;
		term->last_mouse_y = y;
		term->last_mouse_b = b;
	} else {
		term->last_mouse_x = term->last_mouse_y = term->last_mouse_b = INT_MAX;
	}
	r.x2 = dev->size.x2;
	r.y2 = dev->size.y2;
	ev.x = x;
	ev.y = y;
	ev.b = b;
	set_clip_area(dev, &r);
	if (list_empty(term->windows)) return;
	next = list_struct(term->windows.next, struct window);
	next->handler(next, &ev, 0);
}

#endif

static void in_term(void *term_)
{
	struct terminal *term = (struct terminal *)term_;
	struct links_event *ev;
	int r;
	unsigned char *iq;
	if ((unsigned)term->qlen + ALLOC_GR > INT_MAX)
		overalloc();
	iq = xrealloc(term->input_queue, term->qlen + ALLOC_GR);
	term->input_queue = iq;
	EINTRLOOP(r, (int)read(term->fdin, iq + term->qlen, ALLOC_GR));
	if (r <= 0) {
		if (r == -1 && errno != ECONNRESET) error("ERROR: error %d on terminal: could not read event", errno);
		destroy_terminal(term);
		return;
	}
	term->qlen += r;
	test_queue:
	if ((size_t)term->qlen < sizeof(struct links_event)) return;
	ev = (struct links_event *)iq;
	r = sizeof(struct links_event);
	if (ev->ev != EV_INIT && ev->ev != EV_RESIZE && ev->ev != EV_REDRAW && ev->ev != EV_KBD && ev->ev != EV_MOUSE && ev->ev != EV_ABORT) {
		error("ERROR: error on terminal: bad event %d", ev->ev);
		goto mm;
	}
	if (ev->ev == EV_INIT) {
		int init_len;
		if ((size_t)term->qlen < sizeof(struct links_event) + MAX_TERM_LEN + MAX_CWD_LEN + 3 * sizeof(int)) return;
		init_len = *(int *)(iq + sizeof(struct links_event) + MAX_TERM_LEN + MAX_CWD_LEN + 2 * sizeof(int));
		if ((size_t)term->qlen < sizeof(struct links_event) + MAX_TERM_LEN + MAX_CWD_LEN + 3 * sizeof(int) + init_len) return;
		memcpy(term->term, iq + sizeof(struct links_event), MAX_TERM_LEN);
		term->term[MAX_TERM_LEN - 1] = 0;
		memcpy(term->cwd, iq + sizeof(struct links_event) + MAX_TERM_LEN, MAX_CWD_LEN);
		term->cwd[MAX_CWD_LEN - 1] = 0;
		term->environment = *(int *)(iq + sizeof(struct links_event) + MAX_TERM_LEN + MAX_CWD_LEN);
		term->default_character_set = *(int *)(iq + sizeof(struct links_event) + MAX_TERM_LEN + MAX_CWD_LEN + sizeof(int));
		ev->b = (long)(iq + sizeof(struct links_event) + MAX_TERM_LEN + MAX_CWD_LEN + 2 * sizeof(int));
		r = (int)sizeof(struct links_event) + MAX_TERM_LEN + MAX_CWD_LEN + 3 * (int)sizeof(int) + init_len;
		sync_term_specs();
	}
	if (ev->ev == EV_REDRAW || ev->ev == EV_RESIZE || ev->ev == EV_INIT) {
		struct window *win = NULL;
		struct list_head *lwin;

		term->real_x = ev->x;
		term->real_y = ev->y;

		set_margin(term);

		send_redraw:
		if (ev->x < 0 || ev->y < 0) {
			error("ERROR: bad terminal size: %d, %d", (int)ev->x, (int)ev->y);
			goto mm;
		}
		alloc_term_screen(term);
		clear_terminal(term);
		erase_screen(term);
		term->redrawing = 1;
		foreachback(struct window, win, lwin, term->windows)
				win->handler(win, ev, 0);
		term->redrawing = 0;
	}
	if (ev->ev == EV_MOUSE) {
		ev->x -= term->left_margin;
		ev->y -= term->top_margin;
	}
	if (ev->ev == EV_KBD || ev->ev == EV_MOUSE) {
		if (ev->ev == EV_KBD && upcase(ev->x) == 'L' && !(ev->y & KBD_PASTING) && ev->y & KBD_CTRL) {
			ev->ev = EV_REDRAW;
			ev->x = term->x;
			ev->y = term->y;
			goto send_redraw;
		}
		if (ev->ev == EV_KBD && ev->x == KBD_STOP) {
			abort_background_connections();
		}
		if (!list_empty(term->windows)) {
			if (ev->ev == EV_KBD && ev->x == KBD_CTRL_C && !(ev->y & KBD_PASTING)) {
				struct window *prev = list_struct(term->windows.prev, struct window);
				prev->handler(prev, ev, 0);
			} else {
				if (process_utf_8(term, ev)) {
					struct window *next = list_struct(term->windows.next, struct window);
					next->handler(next, ev, 0);
				}
			}
		}
	}
	if (ev->ev == EV_ABORT) {
		destroy_terminal(term);
		return;
	}
	mm:
	if (term->qlen == r) term->qlen = 0;
	else memmove(iq, iq + r, term->qlen -= r);
	goto test_queue;
}

static inline int getcompcode(int c)
{
	return (c<<1 | (c&4)>>2) & 7;
}

unsigned char frame_dumb[49] =	"   ||||++||++++++--|-+||++--|-+----++++++++     ";
static unsigned char frame_vt100[49] =	"aaaxuuukkuxkjjjkmvwtqnttmlvwtqnvvwwmmllnnjla    ";

#define utf8_hack(x)	(x)

#define SETPOS(x, y)							\
{									\
	add_to_str(&a, &l, cast_uchar "\033[");				\
	add_num_to_str(&a, &l, (y) + 1 + term->top_margin);		\
	add_to_str(&a, &l, cast_uchar ";");				\
	add_num_to_str(&a, &l, (x) + 1 + term->left_margin);		\
	add_to_str(&a, &l, cast_uchar "H");				\
	n_chars = 0;							\
}

#define PRINT_CHAR(p)							\
{									\
	char_t c = term->screen[p].ch;					\
	unsigned char A = term->screen[p].at & 0x7f;			\
	unsigned char frm = !!(term->screen[p].at & ATTR_FRAME);	\
	if (s->mode == TERM_VT100) {					\
		if (frm != mode) {					\
			if (!(mode = frm)) add_to_str(&a, &l, cast_uchar "\017");\
			else add_to_str(&a, &l, cast_uchar "\016");	\
		}							\
		if (frm && c >= 176 && c < 224) c = frame_vt100[c - 176];\
	} else if (s->mode == TERM_DUMB && frm && c >= 176 && c < 224) c = frame_dumb[c - 176];\
	if (!(A & 0100) && (A >> 3) == (A & 7)) A = (A & 070) | 7 * !(A & 020);\
	if (A != attrib) {						\
		attrib = A;						\
		add_to_str(&a, &l, cast_uchar "\033[0");		\
		if (s->col) {						\
			unsigned char m[4];				\
			m[0] = ';'; m[1] = '3'; m[3] = 0;		\
			m[2] = (attrib & 7) + '0';			\
			add_to_str(&a, &l, m);				\
			m[1] = '4';					\
			m[2] = ((attrib >> 3) & 7) + '0';		\
			add_to_str(&a, &l, m);				\
		} else if (getcompcode(attrib & 7) < getcompcode(attrib >> 3 & 7))\
			add_to_str(&a, &l, cast_uchar ";7");		\
		if (attrib & 0100) add_to_str(&a, &l, cast_uchar ";1");	\
		add_to_str(&a, &l, cast_uchar "m");			\
	}								\
	if (c >= ' ' && c != 127 && (c != 155 ||			\
	    (term_charset(term) && cp2u(155, term_charset(term)) != -1))) {\
		if (c < 128 || frm || term_charset(term)) {		\
			add_chr_to_str(&a, &l, (unsigned char)c);	\
		} else {						\
		/*							\
		 * Linux UTF-8 console is broken and doesn't advance cursor\
		 * on some characters. So we first print an one-byte	\
		 * replacement, then set the cursor back, then print	\
		 * the UTF-8 character and finally set the cursor again.\
		 */							\
			unsigned char *r;				\
			c = utf8_hack(c);				\
			r = u2cp(c, 0, 1);				\
			if (!(r && r[0] >= 32 && r[0] < 127 && !r[1])) r = cast_uchar "*";\
			add_chr_to_str(&a, &l, r[0]);			\
			if (cx + 1 < term->x)				\
				add_chr_to_str(&a, &l, 8);		\
			else						\
				SETPOS(cx, y);				\
			add_to_str(&a, &l, encode_utf_8(c));		\
			SETPOS(cx + 1, y);				\
			print_next = 1;					\
		}							\
	}								\
	else if (!c || c == 1) add_chr_to_str(&a, &l, ' ');		\
	else add_chr_to_str(&a, &l, '.');				\
	cx++;								\
	n_chars++;							\
}

static void redraw_screen(struct terminal *term)
{
	int x, y, p = 0;
	int cx = term->lcx, cy = term->lcy;
	unsigned n_chars = INT_MAX / 2;
	unsigned char *a;
	int attrib = -1;
	int mode = -1;
	int l = 0;
	int print_next = 0;
	struct term_spec *s;
	if (!term->dirty || (term->master && is_blocked())) return;
	a = init_str();
	s = term->spec;
	for (y = 0; y < term->y; y++) {
		if (!memcmp(&term->screen[p], &term->last_screen[p], sizeof(chr) * term->x)) {
			p += term->x;
			continue;
		}
		for (x = 0; x < term->x; x++, p++) {
			int i;
			if (y == term->y - 1 && x == term->x - 1 && term->left_margin + term->x == term->real_x && term->top_margin + term->y == term->real_y) break;
			if (term->screen[p].ch == term->last_screen[p].ch && term->screen[p].at == term->last_screen[p].at) {
				/* make sure that padding is identical */
				if (chr_has_padding)
					memcpy(&term->last_screen[p], &term->screen[p], sizeof(chr));
				if (print_next) {
					print_next = 0;
					goto must_print_next;
				}
				continue;
			}
			memcpy(&term->last_screen[p], &term->screen[p], sizeof(chr));
 must_print_next:
			if (cx == x && cy == y)
				goto pc;
			else if (cy == y && x - cx < 10 && x - cx > 0) {
				for (i = x - cx; i >= 0; i--) {
					ppc:
					PRINT_CHAR(p - i);
				}
			} else {
				SETPOS(x, y);
				cx = x; cy = y;
				pc:
				i = 0;
				goto ppc;
			}
		}
		if (print_next && term->left_margin + term->x < term->real_x) {
			add_to_str(&a, &l, cast_uchar "\033[0m ");
			attrib = -1;
			print_next = 0;
		}
	}
	if (l) {
		if (s->col) add_to_str(&a, &l, cast_uchar "\033[37;40m");
		add_to_str(&a, &l, cast_uchar "\033[0m");
		if (s->mode == TERM_VT100) add_to_str(&a, &l, cast_uchar "\017");
	}
	term->lcx = cx;
	term->lcy = cy;
	if (term->cx != term->lcx || term->cy != term->lcy) {
		term->lcx = term->cx;
		term->lcy = term->cy;
		add_to_str(&a, &l, cast_uchar "\033[");
		add_num_to_str(&a, &l, term->cy + 1 + term->top_margin);
		add_to_str(&a, &l, cast_uchar ";");
		add_num_to_str(&a, &l, term->cx + 1 + term->left_margin);
		add_to_str(&a, &l, cast_uchar "H");
	}
	hard_write(term->fdout, a, l);
	free(a);
	term->dirty = 0;
}

void redraw_all_terminals(void)
{
	struct terminal *term = NULL;
	struct list_head *lterm;
	foreach(struct terminal, term, lterm, terminals) redraw_screen(term);
}

void flush_terminal(struct terminal *term)
{
	if (!F) {
		redraw_screen(term);
#ifdef G
	} else {
		if (drv->flush)
			drv->flush(term->dev);
#endif
	}
}

void destroy_terminal(void *term_)
{
	struct terminal *term = (struct terminal *)term_;
	int rs;
	unregister_bottom_half(destroy_terminal, term);
#ifdef G
	unregister_bottom_half(redraw_windows, term);
#endif
	while (!list_empty(term->windows)) {
		delete_window(list_struct(term->windows.next, struct window));
	}
	if (!F && !casestrcmp(term->term, cast_uchar "cygwin")) {
		clear_terminal(term);
		redraw_screen(term);
	}
	del_from_list(term);
	close_socket(&term->blocked);
	free(term->title);
	if (!F) {
		free(term->screen);
		free(term->last_screen);
		free(term->input_queue);
		set_handlers(term->fdin, NULL, NULL, NULL);
		EINTRLOOP(rs, close(term->fdin));
		if (!term->master) {
			if (term->fdout != term->fdin)
				EINTRLOOP(rs, close(term->fdout));
		} else {
			unhandle_terminal_signals(term);
			free_all_itrms();
			if (!list_empty(terminals)) {
				os_detach_console();
			}
		}
#ifdef G
	} else {
		drv->shutdown_device(term->dev);
#endif
	}
	if (term->handle_to_close != -1) {
		hard_write(term->handle_to_close, cast_uchar "x", 1);
		close_socket(&term->handle_to_close);
	}
	free(term);
	check_if_no_terminal();
}

void destroy_all_terminals(void)
{
	while (!list_empty(terminals)) {
		destroy_terminal(list_struct(terminals.next, struct terminal));
	}
}

static void check_if_no_terminal(void)
{
	if (!list_empty(terminals)) return;
	terminate_loop = 1;
}

void set_char(struct terminal *t, int x, int y, unsigned ch, unsigned char at)
{
	t->dirty = 1;
	if (x >= 0 && x < t->x && y >= 0 && y < t->y) {
		chr *cc = &t->screen[x + t->x * y];
		cc->ch = ch;
		cc->at = at;
	}
}

const chr *get_char(struct terminal *t, int x, int y)
{
	int lx, ly;
	lx = t->x - 1;
	ly = t->y - 1;
	if((lx | ly) < 0) {
		static const chr empty = { ' ', 070 };
		return &empty;
	}
	if (x > lx)
		x = lx;
	else if (x < 0)
		x = 0;
	if (y > ly)
		y = ly;
	else if (y < 0)
		y = 0;
	return &t->screen[x + t->x * y];
}

void set_color(struct terminal *t, int x, int y, unsigned char c)
{
	t->dirty = 1;
	if (x >= 0 && x < t->x && y >= 0 && y < t->y) t->screen[x + t->x * y].at = (t->screen[x + t->x * y].at & ATTR_FRAME) | (c & ~ATTR_FRAME);
}

void set_only_char(struct terminal *t, int x, int y, unsigned ch, unsigned char at)
{
	const chr *cc;
	t->dirty = 1;
	cc = get_char(t, x, y);
	at = (at & ATTR_FRAME) | (cc->at & ~ATTR_FRAME);
	set_char(t, x, y, ch, at);
}

void set_line(struct terminal *t, int x, int y, int l, chr *line)
{
	int i;
	chr *cc;
	t->dirty = 1;
	if (y < 0 || y >= t->y) return;
	i = x >= 0 ? 0 : -x;
	cc = &t->screen[x+i + t->x * y];
	line = &line[i];
	i = (x+l <= t->x ? l : t->x-x) - i;
	if (i <= 0) return;
	memcpy(cc, line, i * sizeof(chr));
}

void set_line_color(struct terminal *t, int x, int y, int l, unsigned char c)
{
	int i;
	t->dirty = 1;
	if (y < 0 || y >= t->y) return;
	for (i = x >= 0 ? 0 : -x; i < (x+l <= t->x ? l : t->x-x); i++)
		t->screen[x+i + t->x * y].at = (t->screen[x+i + t->x * y].at & ATTR_FRAME) | (c & ~ATTR_FRAME);
}

void fill_area(struct terminal *t, int x, int y, int xw, int yw, unsigned ch, unsigned char at)
{
	int i;
	chr *p, *ps;
	if (x < 0) {
		xw += x;
		x = 0;
	}
	if (x + xw > t->x) xw = t->x - x;
	if (xw <= 0) return;
	if (y < 0) {
		yw += y;
		y = 0;
	}
	if (y + yw > t->y) yw = t->y - y;
	if (yw <= 0) return;
	t->dirty = 1;
	p = ps = &t->screen[x + t->x * y];
	for (i = 0; i < xw; i++) {
		p->ch = ch;
		p->at = at;
		p++;
	}
	p = ps;
	for (i = 1; i < yw; i++) {
		p += t->x;
		memcpy(p, ps, xw * sizeof(chr));
	}
}

static int p1[] = { 218, 191, 192, 217, 179, 196 };
static int p2[] = { 201, 187, 200, 188, 186, 205 };

void draw_frame(struct terminal *t, int x, int y, int xw, int yw, unsigned char c, int w)
{
	int *p = w > 1 ? p2 : p1;
	c |= ATTR_FRAME;
	set_char(t, x, y, p[0], c);
	set_char(t, x+xw-1, y, p[1], c);
	set_char(t, x, y+yw-1, p[2], c);
	set_char(t, x+xw-1, y+yw-1, p[3], c);
	fill_area(t, x, y+1, 1, yw-2, p[4], c);
	fill_area(t, x+xw-1, y+1, 1, yw-2, p[4], c);
	fill_area(t, x+1, y, xw-2, 1, p[5], c);
	fill_area(t, x+1, y+yw-1, xw-2, 1, p[5], c);
}

void print_text(struct terminal *t, int x, int y, int l, unsigned char *text, unsigned char c)
{
	for (; l--; x++) {
		unsigned u = GET_TERM_CHAR(t, &text);
		if (!u) break;
		set_char(t, x, y, u, c);
	}
}

void set_cursor(struct terminal *term, int x, int y, int altx, int alty)
{
	term->dirty = 1;
	if (term->spec->block_cursor) {
		x = altx;
		y = alty;
	}
	if (x >= term->x) x = term->x - 1;
	if (y >= term->y) y = term->y - 1;
	if (x < 0) x = 0;
	if (y < 0) y = 0;
	term->cx = x;
	term->cy = y;
}

void exec_thread(void *path_, int p)
{
	unsigned char *path = (unsigned char *)path_;
	int rs;
	if (path[0] == 2)
		EINTRLOOP(rs, setpgid(0, 0));
	exe(path + 1, path[0]);
	if (path[1 + strlen(cast_const_char(path + 1)) + 1])
		EINTRLOOP(rs, unlink(cast_const_char(path + 1 + strlen(cast_const_char(path + 1)) + 1)));
}

void close_handle(void *p)
{
	int h = (int)(long)p;
	close_socket(&h);
}

static void unblock_terminal(void *term_)
{
	struct terminal *term = (struct terminal *)term_;
	close_handle(&term->blocked);
	term->blocked = -1;
	if (!F) {
		set_handlers(term->fdin, in_term, NULL, term);
		unblock_itrm(term->fdin);
		/* clear the dirty flag because unblock_itrm queued a resize
		   event - so avoid double redraw */
		term->dirty = 0;
		/*redraw_terminal_cls(term);*/
	}
}

#ifdef G
int have_extra_exec(void)
{
#ifdef NO_FG_EXEC
	return 0;
#else
	return F && drv->exec;
#endif
}
#endif

void exec_on_terminal(struct terminal *term, unsigned char *path, unsigned char *delet, unsigned char fg)
{
	int rs;
	if (path && !*path) return;
	if (!path) path = cast_uchar "";
#ifdef NO_FG_EXEC
	fg = 0;
#endif
#ifdef HAVE_EXE_ON_BACKGROUND
	if (*path) {
		rs = exe_on_background(path, delet);
		if (!rs) return;
	}
#endif
	if (term->master) {
		if (!*path) {
			if (!F) dispatch_special(delet);
		} else {
			int blockh;
			unsigned char *param;
			int paraml;
			if (is_blocked() && fg) {
				if (*delet)
					EINTRLOOP(rs, unlink(cast_const_char delet));
				return;
			}
			param = init_str();
			paraml = 0;
			add_chr_to_str(&param, &paraml, fg);
			add_to_str(&param, &paraml, path);
			add_chr_to_str(&param, &paraml, 0);
			add_to_str(&param, &paraml, delet);
			if (fg == 1)
				if (!F) block_itrm(term->fdin);
			if ((blockh = start_thread(exec_thread, param, paraml + 1, *delet != 0)) == -1) {
				if (fg == 1)
					if (!F) unblock_itrm(term->fdin);
				free(param);
				return;
			}
			free(param);
			if (fg == 1
#ifdef G
				&& !have_extra_exec()
#endif
				) {
				term->blocked = blockh;
				set_handlers(blockh, unblock_terminal, NULL, term);
				if (!F) set_handlers(term->fdin, NULL, NULL, term);
				/*block_itrm(term->fdin);*/
			} else {
				set_handlers(blockh, close_handle, NULL, &blockh);
			}
		}
	} else {
		unsigned char *data;
		int datal;
		data = init_str();
		datal = 0;
		add_chr_to_str(&data, &datal, 0);
		add_chr_to_str(&data, &datal, fg);
		add_to_str(&data, &datal, path);
		add_chr_to_str(&data, &datal, 0);
		add_to_str(&data, &datal, delet);
		hard_write(term->fdout, data, datal + 1);
		free(data);
	}
}

void do_terminal_function(struct terminal *term, unsigned char code, unsigned char *data)
{
	unsigned char *x_data;
	int x_datal;
	x_data = init_str();
	x_datal = 0;
	add_chr_to_str(&x_data, &x_datal, code);
	add_to_str(&x_data, &x_datal, data);
	exec_on_terminal(term, NULL, x_data, 0);
	free(x_data);
}

void set_terminal_title(struct terminal *term, unsigned char *title)
{
	if (strlen(cast_const_char title) > 10000) title[10000] = 0;
	if (strchr(cast_const_char title, 1)) {
		unsigned char *a, *b;
		for (a = title, b = title; *a; a++) if (*a != 1) *b++ = *a;
		*b = 0;
	}
	if (term->title && !strcmp(cast_const_char title, cast_const_char term->title)) goto ret;
	free(term->title);
	term->title = stracpy(title);
#ifdef SET_WINDOW_TITLE_UTF_8
	{
		free(title);
		title = convert(term_charset(term), 0, term->title, NULL);
	}
#endif
	if (!F) do_terminal_function(term, TERM_FN_TITLE, title);
#ifdef G
	else if (drv->set_title) drv->set_title(term->dev, title);
#endif
	ret:
	free(title);
}

struct terminal *find_terminal(tcount count)
{
	struct terminal *term = NULL;
	struct list_head *lterm;
	foreach(struct terminal, term, lterm, terminals) if (term->count == count) return term;
	return NULL;
}
