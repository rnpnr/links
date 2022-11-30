/* bfu.c
 * (c) 2002 Mikulas Patocka
 * This file is a part of the Links program, released under GPL.
 */

#include <limits.h>

#include "links.h"

static void menu_func(struct window *, struct links_event *, int);
static void mainmenu_func(struct window *, struct links_event *, int);

struct memory_list *
getml(void *p, ...)
{
	struct memory_list *ml;
	va_list ap;
	int n = 0;
	void *q = p;
	va_start(ap, p);
	while (q) {
		if (n == INT_MAX)
			overalloc();
		n++;
		q = va_arg(ap, void *);
	}
	if ((unsigned)n
	    > (INT_MAX - sizeof(struct memory_list)) / sizeof(void *))
		overalloc();
	ml = xmalloc(sizeof(struct memory_list) + n * sizeof(void *));
	ml->n = n;
	n = 0;
	q = p;
	va_end(ap);
	va_start(ap, p);
	while (q) {
		ml->p[n++] = q;
		q = va_arg(ap, void *);
	}
	va_end(ap);
	return ml;
}

void
add_to_ml(struct memory_list **ml, ...)
{
	struct memory_list *nml;
	va_list ap;
	int n = 0;
	void *q;
	if (!*ml) {
		*ml = xmalloc(sizeof(struct memory_list));
		(*ml)->n = 0;
	}
	va_start(ap, ml);
	while (va_arg(ap, void *)) {
		if (n == INT_MAX)
			overalloc();
		n++;
	}
	if ((unsigned)n + (unsigned)((*ml)->n)
	    > (INT_MAX - sizeof(struct memory_list)) / sizeof(void *))
		overalloc();
	nml = xrealloc(*ml, sizeof(struct memory_list)
	                        + (n + (*ml)->n) * sizeof(void *));
	va_end(ap);
	va_start(ap, ml);
	while ((q = va_arg(ap, void *)))
		nml->p[nml->n++] = q;
	*ml = nml;
	va_end(ap);
}

void
freeml(struct memory_list *ml)
{
	int i;
	if (!ml)
		return;
	for (i = 0; i < ml->n; i++)
		free(ml->p[i]);
	free(ml);
}

static inline int
is_utf_8(struct terminal *term)
{
	if (!term_charset(term))
		return 1;
	return 0;
}

static inline int
txtlen(struct terminal *term, unsigned char *s)
{
	return strlen((char *)s);
}

unsigned char m_bar = 0;

static unsigned
select_hotkey(struct terminal *term, unsigned char *text, unsigned char *hotkey,
              unsigned *hotkeys, int n)
{
	unsigned c;
	if (hotkey == M_BAR)
		return 0;
	if (text) {
		text = stracpy(get_text_translation(text, term));
		charset_upcase_string(&text, term_charset(term));
	}
	hotkey = get_text_translation(hotkey, term);
	while (1) {
		int i;
		c = GET_TERM_CHAR(term, &hotkey);
		if (!c)
			break;
		c = charset_upcase(c, term_charset(term));
		for (i = 0; i < n; i++)
			if (hotkeys[i] == c)
				continue;
		if (!text || strchr((char *)text, c))
			break;
	}
	free(text);
	return c;
}

void
do_menu_selected(struct terminal *term, struct menu_item *items, void *data,
                 int selected, void (*free_function)(void *), void *free_data)
{
	int i;
	struct menu *menu;
	for (i = 0; items[i].text; i++)
		if (i == (INT_MAX - sizeof(struct menu)) / sizeof(unsigned))
			overalloc();
	menu =
	    xmalloc(sizeof(struct menu) + (!i ? 0 : i - 1) * sizeof(unsigned));
	menu->selected = selected;
	menu->view = 0;
	menu->ni = i;
	menu->items = items;
	menu->data = data;
	menu->free_function = free_function;
	menu->free_data = free_data;
	for (i = 0; i < menu->ni; i++)
		menu->hotkeys[i] = select_hotkey(
		    term, items[i].text, items[i].hotkey, menu->hotkeys, i);
	add_window(term, menu_func, menu);
}

void
do_menu(struct terminal *term, struct menu_item *items, void *data)
{
	do_menu_selected(term, items, data, 0, NULL, NULL);
}

static void
select_menu(struct terminal *term, struct menu *menu)
{
	struct menu_item *it;
	void (*func)(struct terminal *, void *, void *);
	void *data1;
	void *data2;
	if (menu->selected < 0 || menu->selected >= menu->ni)
		return;
	it = &menu->items[menu->selected];
	func = it->func;
	data1 = it->data;
	data2 = menu->data;
	if (it->hotkey == M_BAR)
		return;
	flush_terminal(term);
	if (!it->in_m) {
		struct window *win = NULL;
		struct list_head *lwin;
		foreach (struct window, win, lwin, term->windows) {
			if (win->handler != menu_func
			    && win->handler != mainmenu_func)
				break;
			lwin = lwin->prev;
			delete_window(win);
		}
	}
	func(term, data1, data2);
}

static unsigned char *
get_rtext(unsigned char *rtext)
{
	if (!strcmp(cast_const_char rtext, ">"))
		return MENU_SUBMENU;
	return rtext;
}

static void
count_menu_size(struct terminal *term, struct menu *menu)
{
	int sx = term->x;
	int sy = term->y;
	int mx = 4;
	int my;
	for (my = 0; my < menu->ni; my++) {
		int s;
		s = txtlen(term,
		           get_text_translation(menu->items[my].text, term))
		    + txtlen(term, get_text_translation(
				       get_rtext(menu->items[my].rtext), term))
		    + MENU_HOTKEY_SPACE
		          * (get_text_translation(
				 get_rtext(menu->items[my].rtext), term)[0]
		             != 0);
		s += 4;
		if (s > mx)
			mx = s;
	}
	my += 2;
	if (mx > sx)
		mx = sx;
	if (my > sy)
		my = sy;
	menu->nview = my - 2;
	menu->xw = mx;
	menu->yw = my;
	if ((menu->x = menu->xp) < 0)
		menu->x = 0;
	if ((menu->y = menu->yp) < 0)
		menu->y = 0;
	if (menu->x + mx > sx)
		menu->x = sx - mx;
	if (menu->y + my > sy)
		menu->y = sy - my;
}

static void
scroll_menu(struct menu *menu, int d)
{
	int c = 0;
	int w = menu->nview;
	int scr_i = SCROLL_ITEMS > (w - 1) / 2 ? (w - 1) / 2 : SCROLL_ITEMS;
	if (scr_i < 0)
		scr_i = 0;
	if (w < 0)
		w = 0;
	menu->selected += d;
	while (1) {
		if (c++ > menu->ni) {
			menu->selected = -1;
			menu->view = 0;
			return;
		}
		if (menu->selected < 0)
			menu->selected = 0;
		if (menu->selected >= menu->ni)
			menu->selected = menu->ni - 1;
		if (menu->ni && menu->items[menu->selected].hotkey != M_BAR)
			break;
		menu->selected += d;
	}
	if (menu->selected < menu->view + scr_i)
		menu->view = menu->selected - scr_i;
	if (menu->selected >= menu->view + w - scr_i - 1)
		menu->view = menu->selected - w + scr_i + 1;
	if (menu->view > menu->ni - w)
		menu->view = menu->ni - w;
	if (menu->view < 0)
		menu->view = 0;
}

static void
display_menu_txt(struct terminal *term, void *menu_)
{
	struct menu *menu = (struct menu *)menu_;
	int p, s;
	fill_area(term, menu->x + 1, menu->y + 1, menu->xw - 2, menu->yw - 2,
	          ' ', COLOR_MENU_TEXT);
	draw_frame(term, menu->x, menu->y, menu->xw, menu->yw, COLOR_MENU_FRAME,
	           1);
	set_window_ptr(menu->win, menu->x, menu->y);
	for (p = menu->view, s = menu->y + 1;
	     p < menu->ni && p < menu->view + menu->yw - 2; p++, s++) {
		int x;
		int h = 0;
		unsigned c;
		unsigned char *tmptext =
		    get_text_translation(menu->items[p].text, term);
		unsigned char co;
		if (p == menu->selected) {
			h = 1;
			co = COLOR_MENU_SELECTED;
		} else
			co = COLOR_MENU_TEXT;
		if (h) {
			set_cursor(term, menu->x + 1, s, term->x - 1,
			           term->y - 1);
			/*set_window_ptr(menu->win, menu->x+3, s+1);*/
			set_window_ptr(menu->win, menu->x + menu->xw, s);
			fill_area(term, menu->x + 1, s, menu->xw - 2, 1, ' ',
			          co);
		}
		if (menu->items[p].hotkey != M_BAR || (tmptext[0])) {
			unsigned char *rt = get_text_translation(
			    get_rtext(menu->items[p].rtext), term);
			int l = strlen((char *)rt);
			for (x = 0;; x++) {
				c = GET_TERM_CHAR(term, &rt);
				if (!c)
					break;
				if (menu->xw - 4 >= l - x)
					set_char(term,
					         menu->x + menu->xw - 2 - l + x,
					         s, c, co);
			}
			for (x = 0; x < menu->xw - 4; x++) {
				c = GET_TERM_CHAR(term, &tmptext);
				if (!c)
					break;
				if (!h
				    && charset_upcase(c, term_charset(term))
				           == menu->hotkeys[p]) {
					h = 1;
					set_char(term, menu->x + x + 2, s, c,
					         COLOR_MENU_HOTKEY);
				} else
					set_char(term, menu->x + x + 2, s, c,
					         co);
			}
		} else {
			set_char(term, menu->x, s, 0xc3,
			         COLOR_MENU_FRAME | ATTR_FRAME);
			fill_area(term, menu->x + 1, s, menu->xw - 2, 1, 0xc4,
			          COLOR_MENU_FRAME | ATTR_FRAME);
			set_char(term, menu->x + menu->xw - 1, s, 0xb4,
			         COLOR_MENU_FRAME | ATTR_FRAME);
		}
	}
}

static int menu_oldview = -1;
static int menu_oldsel = -1;

static void
menu_func(struct window *win, struct links_event *ev, int fwd)
{
	int s = 0;
	int xp, yp;
	struct menu *menu = win->data;
	menu->win = win;
	switch ((int)ev->ev) {
	case EV_INIT:
	case EV_RESIZE:
		get_parent_ptr(win, &menu->xp, &menu->yp);
		count_menu_size(win->term, menu);
		goto xxx;
	case EV_REDRAW:
		get_parent_ptr(win, &xp, &yp);
		if (xp != menu->xp || yp != menu->yp) {
			menu->xp = xp;
			menu->yp = yp;
			count_menu_size(win->term, menu);
		}
xxx:
		menu->selected--;
		scroll_menu(menu, 1);
		draw_to_window(win, display_menu_txt, menu);
		break;
	case EV_MOUSE:
		if ((ev->b & BM_ACT) == B_MOVE)
			break;
		if ((ev->b & BM_BUTT) == B_FOURTH
		    || (ev->b & BM_BUTT) == B_FIFTH) {
			if ((ev->b & BM_ACT) == B_DOWN)
				goto go_lr;
			break;
		}
		if ((ev->b & BM_BUTT) == B_SIXTH)
			break;

		if (ev->x < menu->x || ev->x >= menu->x + menu->xw
		    || ev->y < menu->y || ev->y >= menu->y + menu->yw) {
			int f = 1;
			struct window *w1 = NULL;
			struct list_head *w1l;
			foreachfrom (struct window, w1, w1l, win->term->windows,
			             &win->list_entry) {
				struct menu *m1;
				if (w1->handler == mainmenu_func) {
					if (ev->y < 1)
						goto del;
					break;
				}
				if (w1->handler != menu_func)
					break;
				m1 = w1->data;
				if (ev->x > m1->x && ev->x < m1->x + m1->xw - 1
				    && ev->y > m1->y
				    && ev->y < m1->y + m1->yw - 1)
					goto del;
				f--;
			}
			if ((ev->b & BM_ACT) == B_DOWN)
				goto del;
			if (0)
del:
				delete_window_ev(win, ev);
		} else {
			if (!(ev->x < menu->x || ev->x >= menu->x + menu->xw
			      || ev->y < menu->y + 1
			      || ev->y >= menu->y + menu->yw - 1)) {
				int s = ev->y - menu->y - 1 + menu->view;
				if (s >= 0 && s < menu->ni
				    && menu->items[s].hotkey != M_BAR) {
					menu_oldview = menu->view;
					menu_oldsel = menu->selected;
					menu->selected = s;
					scroll_menu(menu, 0);
					draw_to_window(win, display_menu_txt,
					               menu);
					menu_oldview = menu_oldsel = -1;
					if ((ev->b & BM_ACT) == B_UP)
						select_menu(win->term, menu);
				}
			}
		}
		break;
	case EV_KBD:
		if (ev->y & KBD_PASTING)
			break;
		if (ev->x == KBD_LEFT || ev->x == KBD_RIGHT) {
go_lr:
			if (win->list_entry.next == &win->term->windows)
				goto mm;
			if (list_struct(win->list_entry.next, struct window)
			        ->handler
			    == mainmenu_func)
				goto mm;
			if (ev->ev == EV_MOUSE && (ev->b & BM_BUTT) == B_FIFTH)
				goto mm;
			if (ev->ev == EV_KBD && ev->x == KBD_RIGHT)
				goto enter;
			delete_window(win);
			break;
		}
		if (ev->x == KBD_ESC) {
			if (win->list_entry.next == &win->term->windows)
				ev = NULL;
			else if (list_struct(win->list_entry.next,
			                     struct window)
			             ->handler
			         != mainmenu_func)
				ev = NULL;
			delete_window_ev(win, ev);
			break;
		}
		if (KBD_ESCAPE_MENU(ev->x) || ev->y & KBD_ALT) {
mm:
			delete_window_ev(win, ev);
			break;
		}
		menu_oldview = menu->view;
		menu_oldsel = menu->selected;
		if (ev->x == KBD_UP)
			scroll_menu(menu, -1);
		else if (ev->x == KBD_DOWN)
			scroll_menu(menu, 1);
		else if (ev->x == KBD_HOME
		         || (upcase(ev->x) == 'A' && ev->y & KBD_CTRL)) {
			menu->selected = -1;
			scroll_menu(menu, 1);
		} else if (ev->x == KBD_END
		           || (upcase(ev->x) == 'E' && ev->y & KBD_CTRL)) {
			menu->selected = menu->ni;
			scroll_menu(menu, -1);
		} else if (ev->x == KBD_PAGE_UP
		           || (upcase(ev->x) == 'B' && ev->y & KBD_CTRL)) {
			if ((menu->selected -= menu->yw - 3) < -1)
				menu->selected = -1;
			if ((menu->view -= menu->yw - 2) < 0)
				menu->view = 0;
			scroll_menu(menu, -1);
		} else if (ev->x == KBD_PAGE_DOWN
		           || (upcase(ev->x) == 'F' && ev->y & KBD_CTRL)) {
			if ((menu->selected += menu->yw - 3) > menu->ni)
				menu->selected = menu->ni;
			if ((menu->view += menu->yw - 2)
			    >= menu->ni - menu->yw + 2)
				menu->view = menu->ni - menu->yw + 2;
			scroll_menu(menu, 1);
		} else if (ev->x > ' ') {
			int i;
			for (i = 0; i < menu->ni; i++) {
				if (charset_upcase(ev->x,
				                   term_charset(win->term))
				    == menu->hotkeys[i]) {
					menu->selected = i;
					scroll_menu(menu, 0);
					s = 1;
				}
			}
		}
		draw_to_window(win, display_menu_txt, menu);
		if (s || ev->x == KBD_ENTER || ev->x == ' ') {
enter:
			menu_oldview = menu_oldsel = -1;
			select_menu(win->term, menu);
		}
		menu_oldview = menu_oldsel = -1;
		break;
	case EV_ABORT:
		if (menu->items->free_i) {
			int i;
			for (i = 0; i < menu->ni; i++) {
				if (menu->items[i].free_i & MENU_FREE_TEXT)
					free(menu->items[i].text);
				if (menu->items[i].free_i & MENU_FREE_RTEXT)
					free(menu->items[i].rtext);
				if (menu->items[i].free_i & MENU_FREE_HOTKEY)
					free(menu->items[i].hotkey);
			}
			if (menu->items->free_i & MENU_FREE_ITEMS)
				free(menu->items);
		}
		if (menu->free_function)
			register_bottom_half(menu->free_function,
			                     menu->free_data);
		break;
	}
}

void
do_mainmenu(struct terminal *term, struct menu_item *items, void *data, int sel)
{
	int i;
	struct mainmenu *menu;
	for (i = 0; items[i].text; i++)
		if (i == (INT_MAX - sizeof(struct mainmenu)) / sizeof(unsigned))
			overalloc();
	menu = xmalloc(sizeof(struct mainmenu)
	               + (!i ? 0 : i - 1) * sizeof(unsigned));
	menu->selected = sel == -1 ? 0 : sel;
	menu->ni = i;
	menu->items = items;
	menu->data = data;
	for (i = 0; i < menu->ni; i++)
		menu->hotkeys[i] = select_hotkey(term, NULL, items[i].hotkey,
		                                 menu->hotkeys, i);
	add_window(term, mainmenu_func, menu);
	if (sel != -1) {
		struct links_event ev = { EV_KBD, KBD_ENTER, 0, 0 };
		struct window *win =
		    list_struct(term->windows.next, struct window);
		win->handler(win, (struct links_event *)&ev, 0);
	}
}

static void
display_mainmenu(struct terminal *term, void *menu_)
{
	struct mainmenu *menu = (struct mainmenu *)menu_;
	int i;
	int p = 2;
	fill_area(term, 0, 0, term->x, 1, ' ', COLOR_MAINMENU);
	for (i = 0; i < menu->ni; i++) {
		int s = 0;
		unsigned c;
		unsigned char *tmptext =
		    get_text_translation(menu->items[i].text, term);
		unsigned char co;
		if (i == menu->selected) {
			s = 1;
			co = COLOR_MAINMENU_SELECTED;
		} else {
			co = COLOR_MAINMENU;
		}
		if (i == menu->selected) {
			fill_area(term, p, 0, 2, 1, ' ', co);
			menu->sp = p;
			set_cursor(term, p, 0, term->x - 1, term->y - 1);
			set_window_ptr(menu->win, p, 1);
		}
		p += 2;
		for (;; p++) {
			c = GET_TERM_CHAR(term, &tmptext);
			if (!c)
				break;
			if (!s
			    && charset_upcase(c, term_charset(term))
			           == menu->hotkeys[i]) {
				s = 1;
				set_char(term, p, 0, c, COLOR_MAINMENU_HOTKEY);
			} else {
				set_char(term, p, 0, c, co);
			}
		}
		if (i == menu->selected)
			fill_area(term, p, 0, 2, 1, ' ', co);
		p += 2;
	}
}

static void
select_mainmenu(struct terminal *term, struct mainmenu *menu)
{
	struct menu_item *it;
	if (menu->selected < 0 || menu->selected >= menu->ni)
		return;
	it = &menu->items[menu->selected];
	if (it->hotkey == M_BAR)
		return;
	if (!it->in_m) {
		struct window *win = NULL;
		struct list_head *lwin;
		foreach (struct window, win, lwin, term->windows) {
			if (win->handler != menu_func
			    && win->handler != mainmenu_func)
				break;
			lwin = lwin->prev;
			delete_window(win);
		}
	}
	it->func(term, it->data, menu->data);
}

static void
mainmenu_func(struct window *win, struct links_event *ev, int fwd)
{
	int s = 0;
	int in_menu;
	struct mainmenu *menu = win->data;
	menu->win = win;
	switch ((int)ev->ev) {
	case EV_INIT:
	case EV_RESIZE:
		/* FALLTHROUGH */
	case EV_REDRAW:
		draw_to_window(win, display_mainmenu, menu);
		break;
	case EV_MOUSE:
		in_menu = ev->x >= 0 && ev->x < win->term->x && ev->y >= 0
		          && ev->y < 1;
		if ((ev->b & BM_ACT) == B_MOVE)
			break;
		if ((ev->b & BM_BUTT) == B_FOURTH) {
			if ((ev->b & BM_ACT) == B_DOWN)
				goto go_left;
			break;
		}
		if ((ev->b & BM_BUTT) == B_FIFTH) {
			if ((ev->b & BM_ACT) == B_DOWN)
				goto go_right;
			break;
		}
		if ((ev->b & BM_BUTT) == B_SIXTH) {
			break;
		}
		if ((ev->b & BM_ACT) == B_DOWN && !in_menu)
			delete_window_ev(win, ev);
		else if (in_menu) {
			int i;
			int p = 2;
			for (i = 0; i < menu->ni; i++) {
				int o = p;
				unsigned char *tmptext = get_text_translation(
				    menu->items[i].text, win->term);
				p += txtlen(win->term, tmptext) + 4;
				if (ev->x >= o && ev->x < p) {
					menu->selected = i;
					draw_to_window(win, display_mainmenu,
					               menu);
					if ((ev->b & BM_ACT) == B_UP
					    || menu->items[s].in_m)
						select_mainmenu(win->term,
						                menu);
					break;
				}
			}
		}
		break;
	case EV_KBD:
		if (ev->y & KBD_PASTING)
			break;
		if (ev->x == ' ' || ev->x == KBD_ENTER || ev->x == KBD_DOWN
		    || ev->x == KBD_UP || ev->x == KBD_PAGE_DOWN
		    || (upcase(ev->x) == 'F' && ev->y & KBD_CTRL)
		    || ev->x == KBD_PAGE_UP
		    || (upcase(ev->x) == 'B' && ev->y & KBD_CTRL)) {
			select_mainmenu(win->term, menu);
			break;
		} else if (ev->x == KBD_LEFT) {
go_left:
			if (!menu->selected--)
				menu->selected = menu->ni - 1;
			s = 1;
			if (fwd)
				s = 2;
		} else if (ev->x == KBD_RIGHT) {
go_right:
			if (++menu->selected >= menu->ni)
				menu->selected = 0;
			s = 1;
			if (fwd)
				s = 2;
		} else if (ev->x == KBD_HOME
		           || (upcase(ev->x) == 'A' && ev->y & KBD_CTRL)) {
			menu->selected = 0;
			s = 1;
		} else if (ev->x == KBD_END
		           || (upcase(ev->x) == 'E' && ev->y & KBD_CTRL)) {
			menu->selected = menu->ni - 1;
			s = 1;
		} else if (ev->x > ' ') {
			int i;
			s = 1;
			for (i = 0; i < menu->ni; i++) {
				if (charset_upcase(ev->x,
				                   term_charset(win->term))
				    == menu->hotkeys[i]) {
					menu->selected = i;
					s = 2;
				}
			}
		}
		if (!s) {
			delete_window_ev(win, KBD_ESCAPE_MENU(ev->x)
			                              || ev->y & KBD_ALT
			                          ? ev
			                          : NULL);
			break;
		}
		draw_to_window(win, display_mainmenu, menu);
		if (s == 2)
			select_mainmenu(win->term, menu);
		break;
	case EV_ABORT:
		break;
	}
}

struct menu_item *
new_menu(int free_i)
{
	struct menu_item *mi;
	mi = mem_calloc(sizeof(struct menu_item));
	mi->free_i = free_i;
	return mi;
}

void
add_to_menu(struct menu_item **mi, unsigned char *text, unsigned char *rtext,
            unsigned char *hotkey,
            void (*func)(struct terminal *, void *, void *), void *data,
            int in_m, int pos)
{
	struct menu_item *mii;
	int n;
	if (pos != -1) {
		n = pos;
		if ((*mi)[n].text)
			internal("invalid menu position %d", n);
	} else
		for (n = 0; (*mi)[n].text; n++)
			if (n == INT_MAX)
				overalloc();
	if (((unsigned)n + 2) > INT_MAX / sizeof(struct menu_item))
		overalloc();
	mii = xrealloc(*mi, (n + 2) * sizeof(struct menu_item));
	*mi = mii;
	memcpy(mii + n + 1, mii + n, sizeof(struct menu_item));
	mii[n].text = text;
	mii[n].rtext = rtext;
	mii[n].hotkey = hotkey;
	mii[n].func = func;
	mii[n].data = data;
	mii[n].in_m = in_m;
}

void
do_dialog(struct terminal *term, struct dialog *dlg, struct memory_list *ml)
{
	struct dialog_data *dd;
	struct dialog_item *d;
	int n = 0;
	for (d = dlg->items; d->type != D_END; d++) {
		if (n == INT_MAX)
			overalloc();
		n++;
	}
	if ((unsigned)n > (INT_MAX - sizeof(struct dialog_data))
	                      / sizeof(struct dialog_item_data))
		overalloc();
	dd = mem_calloc(sizeof(struct dialog_data)
	                + sizeof(struct dialog_item_data) * n);
	dd->dlg = dlg;
	dd->n = n;
	dd->ml = ml;
	add_window(term, dialog_func, dd);
}

void
display_dlg_item(struct dialog_data *dlg, struct dialog_item_data *di, int sel)
{
	struct terminal *term = dlg->win->term;
	unsigned char co;
	unsigned char *text, *t;
	int vposlen, cposlen;

	switch (di->item->type) {
	case D_CHECKBOX:
		/* radio or checkbox */
		if (di->checked)
			print_text(term, di->x, di->y, 3, cast_uchar "[X]",
			           COLOR_DIALOG_CHECKBOX);
		else
			print_text(term, di->x, di->y, 3, cast_uchar "[ ]",
			           COLOR_DIALOG_CHECKBOX);
		if (sel) {
			set_cursor(term, di->x + 1, di->y, di->x + 1, di->y);
			set_window_ptr(dlg->win, di->x, di->y);
		}
		break;
	case D_FIELD:
	case D_FIELD_PASS:
		fill_area(term, di->x, di->y, di->l, 1, ' ',
		          COLOR_DIALOG_FIELD);
		if (di->vpos > di->cpos)
			di->vpos = di->cpos;
		vposlen = strlen((char *)(di->cdata + di->vpos));
		cposlen = strlen((char *)(di->cdata + di->cpos));
		if (!di->l) {
			di->vpos = di->cpos;
			vposlen = cposlen;
		} else {
			while (vposlen - cposlen > di->l - 1) {
				t = di->cdata + di->vpos;
				GET_TERM_CHAR(term, &t);
				di->vpos = (int)(t - di->cdata);
				vposlen--;
			}
		}
		if (di->item->type == D_FIELD_PASS) {
			t = xmalloc(vposlen + 1);
			memset(t, '*', vposlen);
			t[vposlen] = 0;
		} else {
			t = di->cdata + di->vpos;
		}
		print_text(term, di->x, di->y, di->l, t,
		           COLOR_DIALOG_FIELD_TEXT);
		if (di->item->type == D_FIELD_PASS)
			free(t);
		if (sel) {
			set_cursor(term, di->x + vposlen - cposlen, di->y,
			           di->x + vposlen - cposlen, di->y);
			set_window_ptr(dlg->win, di->x, di->y);
		}
		break;
	case D_BUTTON:
		co = sel ? COLOR_DIALOG_BUTTON_SELECTED : COLOR_DIALOG_BUTTON;
		text = get_text_translation(di->item->text, term);
		print_text(term, di->x, di->y, 2, cast_uchar "[ ", co);
		print_text(term, di->x + 2, di->y, strlen((char *)text), text,
		           co);
		print_text(term, di->x + 2 + strlen((char *)text), di->y, 2,
		           cast_uchar " ]", co);
		if (sel) {
			set_cursor(term, di->x + 2, di->y, di->x + 2, di->y);
			set_window_ptr(dlg->win, di->x, di->y);
		}
		break;
	default:
		internal("display_dlg_item: unknown item: %d", di->item->type);
	}
}

struct dspd {
	struct dialog_data *dlg;
	struct dialog_item_data *di;
	int sel;
};

static void
u_display_dlg_item(struct terminal *term, void *p)
{
	struct dspd *d = p;
	display_dlg_item(d->dlg, d->di, d->sel);
}

static void
x_display_dlg_item(struct dialog_data *dlg, struct dialog_item_data *di,
                   int sel)
{
	struct dspd dspd;
	dspd.dlg = dlg;
	dspd.di = di;
	dspd.sel = sel;
	draw_to_window(dlg->win, u_display_dlg_item, &dspd);
}

static void
dlg_select_item(struct dialog_data *dlg, struct dialog_item_data *di)
{
	if (di->item->type == D_CHECKBOX) {
		if (!di->item->gid)
			di->checked = *(int *)di->cdata = !*(int *)di->cdata;
		else {
			int i;
			for (i = 0; i < dlg->n; i++) {
				if (dlg->items[i].item->type == D_CHECKBOX
				    && dlg->items[i].item->gid
				           == di->item->gid) {
					*(int *)dlg->items[i].cdata =
					    di->item->gnum;
					dlg->items[i].checked = 0;
					x_display_dlg_item(dlg, &dlg->items[i],
					                   0);
				}
			}
			di->checked = 1;
		}
		x_display_dlg_item(dlg, di, 1);
	} else if (di->item->type == D_BUTTON)
		di->item->fn(dlg, di);
}

static unsigned char *
dlg_get_history_string(struct terminal *term, struct history_item *hi, int l)
{
	unsigned char *s;
	int ch = term_charset(term);
	s = convert(0, ch, hi->str, NULL);
	if (strlen(cast_const_char s) >= (size_t)l)
		s[l - 1] = 0;
	if (!ch) {
		int r = (int)strlen(cast_const_char s);
		unsigned char *p = s;
		while (r) {
			int chl = utf8chrlen(*p);
			if (chl > r) {
				*p = 0;
				break;
			}
			p += chl;
			r -= chl;
		}
	}
	return s;
}

static void
dlg_set_history(struct terminal *term, struct dialog_item_data *di)
{
	unsigned char *s;
	if (di->cur_hist == &di->history)
		s = stracpy(cast_uchar "");
	else
		s = dlg_get_history_string(
		    term, list_struct(di->cur_hist, struct history_item),
		    di->item->dlen);
	strcpy(cast_char di->cdata, cast_const_char s);
	di->cpos = (int)strlen(cast_const_char s);
	di->vpos = 0;
	free(s);
}

static int
dlg_mouse(struct dialog_data *dlg, struct dialog_item_data *di,
          struct links_event *ev)
{
	switch (di->item->type) {
	case D_BUTTON:
		if (ev->y != di->y || ev->x < di->x
		    || ev->x >= di->x
		                    + strlen((char *)get_text_translation(
					di->item->text, dlg->win->term))
		                    + 4)
			return 0;
		if (dlg->selected != di - dlg->items) {
			x_display_dlg_item(dlg, &dlg->items[dlg->selected], 0);
			dlg->selected = (int)(di - dlg->items);
			x_display_dlg_item(dlg, di, 1);
		}
		if ((ev->b & BM_ACT) == B_UP)
			dlg_select_item(dlg, di);
		return 1;
	case D_FIELD:
	case D_FIELD_PASS:
		if (ev->y != di->y || ev->x < di->x || ev->x >= di->x + di->l)
			return 0;
		if (!is_utf_8(dlg->win->term)) {
			if ((size_t)(di->cpos = di->vpos + ev->x - di->x)
			    > strlen(cast_const_char di->cdata))
				di->cpos =
				    (int)strlen(cast_const_char di->cdata);
		} else {
			int p, u;
			unsigned char *t = di->cdata;
			p = di->x - di->vpos;
			while (1) {
				di->cpos = (int)(t - di->cdata);
				if (!*t)
					break;
				GET_UTF_8(t, u);
				if (!u)
					continue;
				p++;
				if (p > ev->x)
					break;
			}
		}
		if (dlg->selected != di - dlg->items) {
			x_display_dlg_item(dlg, &dlg->items[dlg->selected], 0);
			dlg->selected = (int)(di - dlg->items);
			x_display_dlg_item(dlg, di, 1);
		} else
			x_display_dlg_item(dlg, di, 1);
		return 1;
	case D_CHECKBOX:
		if (ev->y != di->y || ev->x < di->x || ev->x >= di->x + 3)
			return 0;
		if (dlg->selected != di - dlg->items) {
			x_display_dlg_item(dlg, &dlg->items[dlg->selected], 0);
			dlg->selected = (int)(di - dlg->items);
			x_display_dlg_item(dlg, di, 1);
		}
		if ((ev->b & BM_ACT) == B_UP)
			dlg_select_item(dlg, di);
		return 1;
	}
	return 0;
}

static void
redraw_dialog_items(struct terminal *term, void *dlg_)
{
	struct dialog_data *dlg = (struct dialog_data *)dlg_;
	int i;
	for (i = 0; i < dlg->n; i++)
		display_dlg_item(dlg, &dlg->items[i], i == dlg->selected);
}

static void
redraw_dialog(struct terminal *term, void *dlg_)
{
	struct dialog_data *dlg = (struct dialog_data *)dlg_;
	dlg->dlg->fn(dlg);
	redraw_dialog_items(term, dlg);
}

static void
tab_compl(struct terminal *term, void *hi_, void *win_)
{
	struct history_item *hi = (struct history_item *)hi_;
	struct window *win = (struct window *)win_;
	struct links_event ev = { EV_REDRAW, 0, 0, 0 };
	struct dialog_item_data *di =
	    &((struct dialog_data *)win->data)
		 ->items[((struct dialog_data *)win->data)->selected];
	unsigned char *s = dlg_get_history_string(term, hi, di->item->dlen);
	strcpy(cast_char di->cdata, cast_const_char s);
	di->cpos = (int)strlen(cast_const_char s);
	di->vpos = 0;
	free(s);
	ev.x = term->x;
	ev.y = term->y;
	dialog_func(win, &ev, 0);
}

static void
do_tab_compl(struct terminal *term, struct list_head *history,
             struct window *win)
{
	unsigned char *cdata =
	    ((struct dialog_data *)win->data)
		->items[((struct dialog_data *)win->data)->selected]
		.cdata;
	int l = (int)strlen(cast_const_char cdata), n = 0;
	struct history_item *hi = NULL;
	struct list_head *lhi;
	struct menu_item *items = NULL;
	foreach (struct history_item, hi, lhi, *history) {
		unsigned char *s = dlg_get_history_string(term, hi, INT_MAX);
		if (!strncmp(cast_const_char cdata, cast_const_char s, l)) {
			if (!(n & (ALLOC_GR - 1))) {
				if ((unsigned)n
				    > INT_MAX / sizeof(struct menu_item)
				          - ALLOC_GR - 1)
					overalloc();
				items = xrealloc(
				    items, (n + ALLOC_GR + 1)
					       * sizeof(struct menu_item));
			}
			items[n].text = s;
			items[n].rtext = cast_uchar "";
			items[n].hotkey = cast_uchar "";
			items[n].func = tab_compl;
			items[n].rtext = cast_uchar "";
			items[n].data = hi;
			items[n].in_m = 0;
			items[n].free_i = MENU_FREE_ITEMS | MENU_FREE_TEXT;
			if (n == INT_MAX)
				overalloc();
			n++;
		} else
			free(s);
	}
	if (n == 1) {
		tab_compl(term, items->data, win);
		free(items->text);
		free(items);
		return;
	}
	if (n) {
		memset(&items[n], 0, sizeof(struct menu_item));
		do_menu_selected(term, items, win, n - 1, NULL, NULL);
	}
}

void
dialog_func(struct window *win, struct links_event *ev, int fwd)
{
	int i;
	struct terminal *term = win->term;
	struct dialog_data *dlg = win->data;
	struct dialog_item_data *di;

	dlg->win = win;

	/* Use nonstandard event handlers */
	if (dlg->dlg->handle_event
	    && dlg->dlg->handle_event(dlg, ev) == EVENT_PROCESSED) {
		return;
	}

	switch ((int)ev->ev) {
	case EV_INIT:
		for (i = 0; i < dlg->n; i++) {
			struct dialog_item_data *di = &dlg->items[i];
			memset(di, 0, sizeof(struct dialog_item_data));
			di->item = &dlg->dlg->items[i];
			di->cdata = xmalloc(di->item->dlen);
			if (di->item->dlen)
				memcpy(di->cdata, di->item->data,
				       di->item->dlen);
			if (di->item->type == D_CHECKBOX) {
				if (di->item->gid) {
					if (*(int *)di->cdata == di->item->gnum)
						di->checked = 1;
				} else if (*(int *)di->cdata)
					di->checked = 1;
			}
			init_list(di->history);
			di->cur_hist = &di->history;
			if (di->item->type == D_FIELD
			    || di->item->type == D_FIELD_PASS) {
				if (di->item->history) {
					struct history_item *j = NULL;
					struct list_head *lj;
					foreach (struct history_item, j, lj,
					         di->item->history->items) {
						struct history_item *hi;
						size_t sl = strlen(
						    cast_const_char j->str);
						if (sl > INT_MAX
						             - sizeof(
								 struct
								 history_item))
							overalloc();
						hi = xmalloc(
						    sizeof(struct history_item)
						    + sl);
						strcpy(cast_char hi->str,
						       cast_const_char j->str);
						add_to_list(di->history, hi);
					}
				}
				di->cpos =
				    (int)strlen(cast_const_char di->cdata);
			}
		}
		dlg->selected = 0;
		/*-fallthrough*/
	case EV_RESIZE:
		/* this must be really called twice !!! */
		draw_to_window(dlg->win, redraw_dialog, dlg);
		/*-fallthrough*/
	case EV_REDRAW:
		draw_to_window(dlg->win, redraw_dialog, dlg);
		break;
	case EV_MOUSE:
		if ((ev->b & BM_ACT) == B_MOVE)
			break;
		if ((ev->b & BM_BUTT) == B_FOURTH) {
			if ((ev->b & BM_ACT) == B_DOWN)
				goto go_prev;
			break;
		}
		if ((ev->b & BM_BUTT) == B_FIFTH) {
			if ((ev->b & BM_ACT) == B_DOWN)
				goto go_next;
			break;
		}
		if ((ev->b & BM_BUTT) == B_SIXTH) {
			if ((ev->b & BM_ACT) == B_DOWN)
				goto go_enter;
			break;
		}
		for (i = 0; i < dlg->n; i++)
			if (dlg_mouse(dlg, &dlg->items[i], ev))
				break;
		if ((ev->b & BM_ACT) == B_DOWN
		    && (ev->b & BM_BUTT) == B_MIDDLE) {
			/* don't delete this!!! it's here because of jump from
			 * mouse event */
			di = &dlg->items[dlg->selected];
			if (di->item->type == D_FIELD
			    || di->item->type == D_FIELD_PASS)
				goto clipbd_paste;
		}
		break;
	case EV_KBD:
		di = &dlg->items[dlg->selected];
		if (ev->y & KBD_PASTING) {
			if (!((di->item->type == D_FIELD
			       || di->item->type == D_FIELD_PASS)
			      && (ev->x >= ' '
			          && !(ev->y & (KBD_CTRL | KBD_ALT)))))
				break;
		}
		if (di->item->type == D_FIELD
		    || di->item->type == D_FIELD_PASS) {
			if (ev->x == KBD_UP
			    && di->cur_hist->prev != &di->history) {
				di->cur_hist = di->cur_hist->prev;
				dlg_set_history(term, di);
				goto dsp_f;
			}
			if (ev->x == KBD_DOWN && di->cur_hist != &di->history) {
				di->cur_hist = di->cur_hist->next;
				dlg_set_history(term, di);
				goto dsp_f;
			}
			if (ev->x == KBD_RIGHT) {
				if ((size_t)di->cpos
				    < strlen(cast_const_char di->cdata)) {
					if (!is_utf_8(term))
						di->cpos++;
					else {
						int u;
						unsigned char *p =
						    di->cdata + di->cpos;
						GET_UTF_8(p, u);
						di->cpos = (int)(p - di->cdata);
					}
				}
				goto dsp_f;
			}
			if (ev->x == KBD_LEFT) {
				if (di->cpos > 0) {
					if (!is_utf_8(term))
						di->cpos--;
					else {
						unsigned char *p =
						    di->cdata + di->cpos;
						BACK_UTF_8(p, di->cdata);
						di->cpos = (int)(p - di->cdata);
					}
				}
				goto dsp_f;
			}
			if (ev->x == KBD_HOME
			    || (upcase(ev->x) == 'A' && ev->y & KBD_CTRL)) {
				di->cpos = 0;
				goto dsp_f;
			}
			if (ev->x == KBD_END
			    || (upcase(ev->x) == 'E' && ev->y & KBD_CTRL)) {
				di->cpos =
				    (int)strlen(cast_const_char di->cdata);
				goto dsp_f;
			}
			if (ev->x >= ' ' && !(ev->y & (KBD_CTRL | KBD_ALT))) {
				unsigned char *u;
				unsigned char p[2] = { 0, 0 };
				if (!is_utf_8(term)) {
					p[0] = (unsigned char)ev->x;
					u = p;
				} else {
					u = encode_utf_8(ev->x);
				}
				if (strlen(cast_const_char di->cdata)
				        + strlen(cast_const_char u)
				    < (size_t)di->item->dlen) {
					memmove(
					    di->cdata + di->cpos
						+ strlen(cast_const_char u),
					    di->cdata + di->cpos,
					    strlen(cast_const_char di->cdata)
						- di->cpos + 1);
					memcpy(&di->cdata[di->cpos], u,
					       strlen(cast_const_char u));
					di->cpos +=
					    (int)strlen(cast_const_char u);
				}
				goto dsp_f;
			}
			if (ev->x == KBD_BS) {
				if (di->cpos) {
					int s = 1;
					if (is_utf_8(term)) {
						unsigned u;
						unsigned char *p, *pp;
						p = di->cdata;
a:
						pp = p;
						GET_UTF_8(p, u);
						if (p < di->cdata + di->cpos)
							goto a;
						s = (int)(p - pp);
					}
					memmove(
					    di->cdata + di->cpos - s,
					    di->cdata + di->cpos,
					    strlen(cast_const_char di->cdata)
						- di->cpos + s);
					di->cpos -= s;
				}
				goto dsp_f;
			}
			if (ev->x == KBD_DEL
			    || (upcase(ev->x) == 'D' && ev->y & KBD_CTRL)) {
				if ((size_t)di->cpos
				    < strlen(cast_const_char di->cdata)) {
					int s = 1;
					if (is_utf_8(term)) {
						unsigned u;
						unsigned char *p =
						    di->cdata + di->cpos;
						GET_UTF_8(p, u);
						s = (int)(p
						          - (di->cdata
						             + di->cpos));
					}
					memmove(
					    di->cdata + di->cpos,
					    di->cdata + di->cpos + s,
					    strlen(cast_const_char di->cdata)
						- di->cpos + s);
				}
				goto dsp_f;
			}
			if (upcase(ev->x) == 'U' && ev->y & KBD_CTRL) {
				unsigned char *a = memacpy(di->cdata, di->cpos);
				if (a) {
					set_clipboard_text(term, a);
					free(a);
				}
				memmove(
				    di->cdata, di->cdata + di->cpos,
				    strlen(cast_const_char di->cdata + di->cpos)
					+ 1);
				di->cpos = 0;
				goto dsp_f;
			}
			if (upcase(ev->x) == 'K' && ev->y & KBD_CTRL) {
				set_clipboard_text(term, di->cdata + di->cpos);
				di->cdata[di->cpos] = 0;
				goto dsp_f;
			}
			/* Copy to clipboard */
			if ((ev->x == KBD_INS && ev->y & KBD_CTRL)
			    || (upcase(ev->x) == 'B' && ev->y & KBD_CTRL)
			    || ev->x == KBD_COPY) {
				set_clipboard_text(term, di->cdata);
				break; /* We don't need to redraw */
			}
			/* FIXME -- why keyboard shortcuts with shift don't
			 * works??? */
			/* Cut to clipboard */
			if ((ev->x == KBD_DEL && ev->y & KBD_SHIFT)
			    || (upcase(ev->x) == 'X' && ev->y & KBD_CTRL)
			    || ev->x == KBD_CUT) {
				set_clipboard_text(term, di->cdata);
				di->cdata[0] = 0;
				di->cpos = 0;
				goto dsp_f;
			}
			/* Paste from clipboard */
			if ((ev->x == KBD_INS && ev->y & KBD_SHIFT)
			    || (upcase(ev->x) == 'V' && ev->y & KBD_CTRL)
			    || ev->x == KBD_PASTE) {
				unsigned char *clipboard;
clipbd_paste:
				clipboard = get_clipboard_text(term);
				if (clipboard) {
					unsigned char *nl = clipboard;
					while ((nl = cast_uchar strchr(
						    cast_const_char nl, '\n')))
						*nl = ' ';
					if (strlen(cast_const_char di->cdata)
					            + strlen(cast_const_char
					                         clipboard)
					        < (size_t)di->item->dlen
					    || strlen(cast_const_char di->cdata)
					               + strlen(cast_const_char
					                            clipboard)
					           < strlen(cast_const_char di
					                        ->cdata)) {

						memmove(
						    di->cdata + di->cpos
							+ strlen(cast_const_char
						                     clipboard),
						    di->cdata + di->cpos,
						    strlen(cast_const_char di
						               ->cdata)
							- di->cpos + 1);
						memcpy(&di->cdata[di->cpos],
						       clipboard,
						       strlen(cast_const_char
						                  clipboard));
						di->cpos += (int)strlen(
						    cast_const_char clipboard);
					}
					free(clipboard);
				}
				goto dsp_f;
			}
			if ((upcase(ev->x) == 'W' && ev->y & KBD_CTRL)
			    || ev->x == KBD_FIND) {
				do_tab_compl(term, &di->history, win);
				goto dsp_f;
			}
			goto gh;
dsp_f:
			x_display_dlg_item(dlg, di, 1);
			break;
		}
		if ((ev->x == KBD_ENTER && di->item->type == D_BUTTON)
		    || ev->x == ' ') {
			dlg_select_item(dlg, di);
			break;
		}
gh:
		if (ev->x > ' ')
			for (i = 0; i < dlg->n; i++) {
				unsigned char *tx = get_text_translation(
				    dlg->dlg->items[i].text, term);
				if (dlg->dlg->items[i].type == D_BUTTON
				    && charset_upcase(GET_TERM_CHAR(term, &tx),
				                      term_charset(term))
				           == charset_upcase(
					       ev->x, term_charset(term)))
					goto sel;
			}
		if (ev->x == KBD_ENTER) {
go_enter:
			for (i = 0; i < dlg->n; i++)
				if (dlg->dlg->items[i].type == D_BUTTON
				    && dlg->dlg->items[i].gid & B_ENTER)
					goto sel;
			break;
		}
		if (ev->x == KBD_ESC) {
			for (i = 0; i < dlg->n; i++)
				if (dlg->dlg->items[i].type == D_BUTTON
				    && dlg->dlg->items[i].gid & B_ESC)
					goto sel;
			break;
		}
		if (0) {
sel:
			if (dlg->selected != i) {
				x_display_dlg_item(
				    dlg, &dlg->items[dlg->selected], 0);
				x_display_dlg_item(dlg, &dlg->items[i], 1);
				dlg->selected = i;
			}
			dlg_select_item(dlg, &dlg->items[i]);
			break;
		}
		if (((ev->x == KBD_TAB && !ev->y) || ev->x == KBD_DOWN
		     || ev->x == KBD_RIGHT)
		    && dlg->n > 1) {
go_next:
			x_display_dlg_item(dlg, &dlg->items[dlg->selected], 0);
			if ((++dlg->selected) >= dlg->n)
				dlg->selected = 0;
			x_display_dlg_item(dlg, &dlg->items[dlg->selected], 1);
			break;
		}
		if (((ev->x == KBD_TAB && ev->y) || ev->x == KBD_UP
		     || ev->x == KBD_LEFT)
		    && dlg->n > 1) {
go_prev:
			x_display_dlg_item(dlg, &dlg->items[dlg->selected], 0);
			if ((--dlg->selected) < 0)
				dlg->selected = dlg->n - 1;
			x_display_dlg_item(dlg, &dlg->items[dlg->selected], 1);
			break;
		}
		break;
	case EV_ABORT:
		/* Moved this line up so that the dlg would have access to its
		   member vars before they get freed. */
		if (dlg->dlg->abort)
			dlg->dlg->abort(dlg);
		for (i = 0; i < dlg->n; i++) {
			struct dialog_item_data *di = &dlg->items[i];
			free(di->cdata);
			free_list(struct history_item, di->history);
		}
		freeml(dlg->ml);
	}
}

/* gid and gnum are 100 times greater than boundaries (e.g. if gid==1 boundary
 * is 0.01) */
int
check_float(struct dialog_data *dlg, struct dialog_item_data *di)
{
	char *end;
	double d = strtod(cast_const_char di->cdata, &end);
	if (!*di->cdata || *end
	    || di->cdata[strspn(cast_const_char di->cdata, "0123456789.")]
	    || *di->cdata == (unsigned char)'.') {
		msg_box(dlg->win->term, NULL, TEXT_(T_BAD_NUMBER), AL_CENTER,
		        TEXT_(T_NUMBER_EXPECTED), MSG_BOX_END, NULL, 1,
		        TEXT_(T_CANCEL), msg_box_null, B_ENTER | B_ESC);
		return 1;
	}
	if (d < 0 || d > di->item->gnum || 100 * d < di->item->gid
	    || 100 * d > di->item->gnum) {
		msg_box(dlg->win->term, NULL, TEXT_(T_BAD_NUMBER), AL_CENTER,
		        TEXT_(T_NUMBER_OUT_OF_RANGE), MSG_BOX_END, NULL, 1,
		        TEXT_(T_CANCEL), msg_box_null, B_ENTER | B_ESC);
		return 1;
	}
	return 0;
}

int
check_number(struct dialog_data *dlg, struct dialog_item_data *di)
{
	char *end;
	long l = strtol(cast_const_char di->cdata, &end, 10);
	if (!*di->cdata || *end) {
		msg_box(dlg->win->term, NULL, TEXT_(T_BAD_NUMBER), AL_CENTER,
		        TEXT_(T_NUMBER_EXPECTED), MSG_BOX_END, NULL, 1,
		        TEXT_(T_CANCEL), msg_box_null, B_ENTER | B_ESC);
		return 1;
	}
	if (l < di->item->gid || l > di->item->gnum) {
		msg_box(dlg->win->term, NULL, TEXT_(T_BAD_NUMBER), AL_CENTER,
		        TEXT_(T_NUMBER_OUT_OF_RANGE), MSG_BOX_END, NULL, 1,
		        TEXT_(T_CANCEL), msg_box_null, B_ENTER | B_ESC);
		return 1;
	}
	return 0;
}

int
check_hex_number(struct dialog_data *dlg, struct dialog_item_data *di)
{
	char *end;
	long l = strtol(cast_const_char di->cdata, &end, 16);
	if (!*di->cdata || *end) {
		msg_box(dlg->win->term, NULL, TEXT_(T_BAD_NUMBER), AL_CENTER,
		        TEXT_(T_NUMBER_EXPECTED), MSG_BOX_END, NULL, 1,
		        TEXT_(T_CANCEL), msg_box_null, B_ENTER | B_ESC);
		return 1;
	}
	if (l < di->item->gid || l > di->item->gnum) {
		msg_box(dlg->win->term, NULL, TEXT_(T_BAD_NUMBER), AL_CENTER,
		        TEXT_(T_NUMBER_OUT_OF_RANGE), MSG_BOX_END, NULL, 1,
		        TEXT_(T_CANCEL), msg_box_null, B_ENTER | B_ESC);
		return 1;
	}
	return 0;
}

int
check_nonempty(struct dialog_data *dlg, struct dialog_item_data *di)
{
	unsigned char *p;
	for (p = di->cdata; *p; p++)
		if (*p > ' ')
			return 0;
	msg_box(dlg->win->term, NULL, TEXT_(T_BAD_STRING), AL_CENTER,
	        TEXT_(T_EMPTY_STRING_NOT_ALLOWED), MSG_BOX_END, NULL, 1,
	        TEXT_(T_CANCEL), msg_box_null, B_ENTER | B_ESC);
	return 1;
}

static int
check_local_ip_address_internal(struct dialog_data *dlg,
                                struct dialog_item_data *di, int pf)
{
	int s;
	int rs;
	unsigned char *p = di->cdata;
	if (!*p) {
		return 0;
	}
	if (pf == PF_INET6)
		rs = numeric_ipv6_address((char *)p, NULL, NULL);
	else
		rs = numeric_ip_address((char *)p, NULL);
	if (rs) {
		msg_box(dlg->win->term, NULL, TEXT_(T_BAD_IP_ADDRESS),
		        AL_CENTER, TEXT_(T_INVALID_IP_ADDRESS_SYNTAX),
		        MSG_BOX_END, NULL, 1, TEXT_(T_CANCEL), msg_box_null,
		        B_ENTER | B_ESC);
		return 1;
	}
	s = socket_and_bind(pf, p);
	if (s != -1)
		EINTRLOOP(rs, close(s));
	else {
		if (1
#ifdef ENFILE
		    && errno != ENFILE
#endif
#ifdef EMFILE
		    && errno != EMFILE
#endif
#ifdef ENOBUFS
		    && errno != ENOBUFS
#endif
#ifdef ENOMEM
		    && errno != ENOMEM
#endif
		) {
			unsigned char *er = stracpy(cast_uchar strerror(errno));
			unsigned char *ad = stracpy(p);
			msg_box(dlg->win->term, getml(er, ad, NULL),
			        TEXT_(T_BAD_IP_ADDRESS), AL_CENTER,
			        TEXT_(T_UNABLE_TO_USE_LOCAL_IP_ADDRESS),
			        cast_uchar " ", ad, cast_uchar ": ", er,
			        MSG_BOX_END, NULL, 1, TEXT_(T_CANCEL),
			        msg_box_null, B_ENTER | B_ESC);
			return 1;
		}
	}
	return 0;
}

int
check_local_ip_address(struct dialog_data *dlg, struct dialog_item_data *di)
{
	return check_local_ip_address_internal(dlg, di, PF_INET);
}

int
check_local_ipv6_address(struct dialog_data *dlg, struct dialog_item_data *di)
{
	return check_local_ip_address_internal(dlg, di, PF_INET6);
}

int
cancel_dialog(struct dialog_data *dlg, struct dialog_item_data *di)
{
	delete_window(dlg->win);
	return 0;
}

int
check_dialog(struct dialog_data *dlg)
{
	int i;
	for (i = 0; i < dlg->n; i++)
		if (dlg->dlg->items[i].type == D_CHECKBOX
		    || dlg->dlg->items[i].type == D_FIELD
		    || dlg->dlg->items[i].type == D_FIELD_PASS)
			if (dlg->dlg->items[i].fn
			    && dlg->dlg->items[i].fn(dlg, &dlg->items[i])) {
				dlg->selected = i;
				draw_to_window(dlg->win, redraw_dialog_items,
				               dlg);
				return 1;
			}
	return 0;
}

void
get_dialog_data(struct dialog_data *dlg)
{
	int i;
	for (i = 0; i < dlg->n; i++) {
		void *p1 = dlg->dlg->items[i].data;
		void *p2 = dlg->items[i].cdata;
		int l = dlg->dlg->items[i].dlen;
		if (l)
			memcpy(p1, p2, l);
	}
}

int
ok_dialog(struct dialog_data *dlg, struct dialog_item_data *di)
{
	void (*fn)(void *) = dlg->dlg->refresh;
	void *data = dlg->dlg->refresh_data;
	if (check_dialog(dlg))
		return 1;
	get_dialog_data(dlg);
	if (fn)
		fn(data);
	return cancel_dialog(dlg, di);
}

void
center_dlg(struct dialog_data *dlg)
{
	dlg->x = (dlg->win->term->x - dlg->xw) / 2;
	dlg->y = (dlg->win->term->y - dlg->yw) / 2;
}

void
draw_dlg(struct dialog_data *dlg)
{
	int i, tpos;
	struct terminal *term = dlg->win->term;

	fill_area(term, dlg->x, dlg->y, dlg->xw, dlg->yw, ' ', COLOR_DIALOG);
	draw_frame(term, dlg->x + DIALOG_LEFT_BORDER,
	           dlg->y + DIALOG_TOP_BORDER, dlg->xw - 2 * DIALOG_LEFT_BORDER,
	           dlg->yw - 2 * DIALOG_TOP_BORDER, COLOR_DIALOG_FRAME,
	           DIALOG_FRAME);
	i = strlen((char *)get_text_translation(dlg->dlg->title, term));
	tpos = (dlg->xw - i) / 2;
	print_text(term, tpos + dlg->x - 1, dlg->y + DIALOG_TOP_BORDER, 1,
	           cast_uchar " ", COLOR_DIALOG_TITLE);
	print_text(term, tpos + dlg->x, dlg->y + DIALOG_TOP_BORDER, i,
	           get_text_translation(dlg->dlg->title, term),
	           COLOR_DIALOG_TITLE);
	print_text(term, tpos + dlg->x + i, dlg->y + DIALOG_TOP_BORDER, 1,
	           cast_uchar " ", COLOR_DIALOG_TITLE);
}

void
max_text_width(struct terminal *term, unsigned char *text, int *width,
               int align)
{
	text = get_text_translation(text, term);
	do {
		int c = 0;
		while (*text && *text != '\n') {
			if (!is_utf_8(term)) {
				text++;
				c++;
			} else {
				int u;
				GET_UTF_8(text, u);
				c++;
			}
		}
		if (c > *width)
			*width = c;
	} while (*(text++));
}

void
min_text_width(struct terminal *term, unsigned char *text, int *width,
               int align)
{
	text = get_text_translation(text, term);
	do {
		int c = 0;
		while (*text && *text != '\n' && *text != ' ') {
			if (!is_utf_8(term)) {
				text++;
				c++;
			} else {
				int u;
				GET_UTF_8(text, u);
				c++;
			}
		}
		if (c > *width)
			*width = c;
	} while (*(text++));
}

int
dlg_format_text(struct dialog_data *dlg, struct terminal *term,
                unsigned char *text, int x, int *y, int w, int *rw,
                unsigned char co, int align)
{
	int xx = x;
	text = get_text_translation(text, dlg->win->term);
	for (;;) {
		unsigned char *t1;
		unsigned ch;
		int cx, lbr;

		t1 = text;
		cx = 0;
		lbr = 0;
next_chr:
		ch = GET_TERM_CHAR(dlg->win->term, &t1);
		if (ch == ' ') {
			lbr = cx;
		}
		if (ch && ch != '\n') {
			if (cx == w) {
				if (!lbr)
					lbr = cx;
				goto print_line;
			}
			cx++;
			goto next_chr;
		}
		if (!ch && !cx)
			break;
		lbr = cx;
print_line:
		if (rw && lbr > *rw)
			*rw = lbr;
		xx = x;
		if ((align & AL_MASK) == AL_CENTER) {
			xx += (w - lbr) / 2;
		}
		for (; lbr--; xx++) {
			ch = GET_TERM_CHAR(dlg->win->term, &text);
			if (term)
				set_char(term, xx, *y, ch, co);
		}
		xx++;
		if (*text == ' ' || *text == '\n')
			text++;
		(*y)++;
	}
	return xx - x;
}

void
max_buttons_width(struct terminal *term, struct dialog_item_data *butt, int n,
                  int *width)
{
	int w = -2;
	int i;
	for (i = 0; i < n; i++)
		w += txtlen(term,
		            get_text_translation((butt++)->item->text, term))
		     + 6;
	if (w > *width)
		*width = w;
}

void
min_buttons_width(struct terminal *term, struct dialog_item_data *butt, int n,
                  int *width)
{
	int i;
	for (i = 0; i < n; i++) {
		int w = txtlen(term,
		               get_text_translation((butt++)->item->text, term))
		        + 4;
		if (w > *width)
			*width = w;
	}
}

void
dlg_format_buttons(struct dialog_data *dlg, struct terminal *term,
                   struct dialog_item_data *butt, int n, int x, int *y, int w,
                   int *rw, int align)
{
	int i1 = 0;
	while (i1 < n) {
		int i2 = i1 + 1;
		int mw;
		while (i2 < n) {
			mw = 0;
			max_buttons_width(dlg->win->term, butt + i1,
			                  i2 - i1 + 1, &mw);
			if (mw <= w)
				i2++;
			else
				break;
		}
		mw = 0;
		max_buttons_width(dlg->win->term, butt + i1, i2 - i1, &mw);
		if (rw && mw > *rw)
			if ((*rw = mw) > w)
				*rw = w;
		if (term) {
			int i;
			int p = x
			        + ((align & AL_MASK) == AL_CENTER ? (w - mw) / 2
			                                          : 0);
			for (i = i1; i < i2; i++) {
				butt[i].x = p;
				butt[i].y = *y;
				p += (butt[i].l = txtlen(dlg->win->term,
				                         get_text_translation(
							     butt[i].item->text,
							     dlg->win->term))
				                  + 4)
				     + 2;
			}
		}
		*y += 2;
		i1 = i2;
	}
}

void
dlg_format_checkbox(struct dialog_data *dlg, struct terminal *term,
                    struct dialog_item_data *chkb, int x, int *y, int w,
                    int *rw, unsigned char *text)
{
	int k = 4;
	if (term) {
		chkb->x = x;
		chkb->y = *y;
	}
	if (rw)
		*rw -= k;
	dlg_format_text(dlg, term, text, x + k, y, w - k, rw,
	                COLOR_DIALOG_CHECKBOX_TEXT, AL_LEFT | AL_NOBRLEXP);
	if (rw)
		*rw += k;
}

void
dlg_format_checkboxes(struct dialog_data *dlg, struct terminal *term,
                      struct dialog_item_data *chkb, int n, int x, int *y,
                      int w, int *rw, unsigned char *const *texts)
{
	while (n) {
		dlg_format_checkbox(dlg, term, chkb, x, y, w, rw, texts[0]);
		texts++;
		chkb++;
		n--;
	}
}

void
checkboxes_width(struct terminal *term, unsigned char *const *texts, int n,
                 int *w,
                 void (*fn)(struct terminal *, unsigned char *, int *, int))
{
	int k = 4;
	while (n--) {
		*w -= k;
		fn(term, get_text_translation(texts[0], term), w, 0);
		*w += k;
		texts++;
	}
}

void
dlg_format_field(struct dialog_data *dlg, struct terminal *term,
                 struct dialog_item_data *item, int x, int *y, int w, int *rw,
                 int align)
{
	if (term) {
		item->x = x;
		item->y = *y;
		item->l = w;
		if (rw && item->l > *rw)
			if ((*rw = item->l) > w)
				*rw = w;
	}
	(*y)++;
}

void
dlg_format_text_and_field(struct dialog_data *dlg, struct terminal *term,
                          unsigned char *text, struct dialog_item_data *item,
                          int x, int *y, int w, int *rw, unsigned char co,
                          int align)
{
	dlg_format_text(dlg, term, text, x, y, w, rw, co, align);
	dlg_format_field(dlg, term, item, x, y, w, rw, align);
}

#if 0
/* Layout for generic boxes */
void dlg_format_box(struct terminal *term, struct terminal *t2, struct dialog_item_data *item, int x, int *y, int w, int *rw, int align) {
	item->x = x;
	item->y = *y;
	item->l = w;
	if (rw && item->l > *rw) if ((*rw = item->l) > w) *rw = w;
	(*y) += item->item->gid;
}
#endif

void
max_group_width(struct terminal *term, unsigned char *const *texts,
                struct dialog_item_data *item, int n, int *w)
{
	int ww = 0;
	while (n--) {
		int wx =
		    item->item->type == D_CHECKBOX ? 4
		    : item->item->type == D_BUTTON
			? txtlen(term,
		                 get_text_translation(item->item->text, term))
			      + 4
			: item->item->dlen;
		wx += txtlen(term, get_text_translation(texts[0], term)) + 1;
		if (n)
			wx++;
		ww += wx;
		texts++;
		item++;
	}
	if (ww > *w)
		*w = ww;
}

void
min_group_width(struct terminal *term, unsigned char *const *texts,
                struct dialog_item_data *item, int n, int *w)
{
	while (n--) {
		int wx =
		    item->item->type == D_CHECKBOX ? 4
		    : item->item->type == D_BUTTON
			? txtlen(term,
		                 get_text_translation(item->item->text, term))
			      + 4
			: item->item->dlen + 1;
		wx += txtlen(term, get_text_translation(texts[0], term));
		if (wx > *w)
			*w = wx;
		texts++;
		item++;
	}
}

void
dlg_format_group(struct dialog_data *dlg, struct terminal *term,
                 unsigned char *const *texts, struct dialog_item_data *item,
                 int n, int x, int *y, int w, int *rw)
{
	int nx = 0;
	while (n--) {
		int wx = item->item->type == D_CHECKBOX ? 3
		         : item->item->type == D_BUTTON
		             ? txtlen(dlg->win->term,
		                      get_text_translation(item->item->text,
		                                           dlg->win->term))
		                   + 4
		             : item->item->dlen;
		int sl;
		if (get_text_translation(texts[0], dlg->win->term)[0])
			sl = txtlen(
				 dlg->win->term,
				 get_text_translation(texts[0], dlg->win->term))
			     + 1;
		else
			sl = 0;
		wx += sl;
		if (nx && nx + wx > w) {
			nx = 0;
			(*y) += 2;
		}
		if (term) {
			print_text(
			    term, x + nx + 4 * (item->item->type == D_CHECKBOX),
			    *y,
			    strlen((char *)get_text_translation(
				texts[0], dlg->win->term)),
			    get_text_translation(texts[0], dlg->win->term),
			    COLOR_DIALOG_TEXT);
			item->x =
			    x + nx + sl * (item->item->type != D_CHECKBOX);
			item->y = *y;
			if (item->item->type == D_FIELD
			    || item->item->type == D_FIELD_PASS)
				item->l = item->item->dlen;
		}
		if (rw && nx + wx > *rw)
			if ((*rw = nx + wx) > w)
				*rw = w;
		nx += wx + 1;
		texts++;
		item++;
	}
	(*y)++;
}

void
checkbox_list_fn(struct dialog_data *dlg)
{
	struct terminal *term = dlg->win->term;
	int n_checkboxes;
	int max = 0, min = 0;
	int w, rw;
	int y = 0;
	for (n_checkboxes = 0;
	     ((unsigned char **)dlg->dlg->udata)[n_checkboxes]; n_checkboxes++)
		;
	checkboxes_width(term, dlg->dlg->udata, n_checkboxes, &max,
	                 max_text_width);
	checkboxes_width(term, dlg->dlg->udata, n_checkboxes, &min,
	                 min_text_width);
	max_buttons_width(term, dlg->items + n_checkboxes,
	                  dlg->n - n_checkboxes, &max);
	min_buttons_width(term, dlg->items + n_checkboxes,
	                  dlg->n - n_checkboxes, &min);
	w = term->x * 9 / 10 - 2 * DIALOG_LB;
	if (w > max)
		w = max;
	if (w < min)
		w = min;
	if (w > term->x - 2 * DIALOG_LB)
		w = term->x - 2 * DIALOG_LB;
	if (w < 5)
		w = 5;
	rw = 0;
	dlg_format_checkboxes(dlg, NULL, dlg->items, n_checkboxes, 0, &y, w,
	                      &rw, dlg->dlg->udata);
	y++;
	dlg_format_buttons(dlg, NULL, dlg->items + n_checkboxes,
	                   dlg->n - n_checkboxes, 0, &y, w, &rw, AL_CENTER);
	w = rw;
	dlg->xw = rw + 2 * DIALOG_LB;
	dlg->yw = y + 2 * DIALOG_TB;
	center_dlg(dlg);
	draw_dlg(dlg);
	y = dlg->y + DIALOG_TB + 1;
	dlg_format_checkboxes(dlg, term, dlg->items, n_checkboxes,
	                      dlg->x + DIALOG_LB, &y, w, NULL, dlg->dlg->udata);
	y++;
	dlg_format_buttons(dlg, term, dlg->items + n_checkboxes,
	                   dlg->n - n_checkboxes, dlg->x + DIALOG_LB, &y, w,
	                   &rw, AL_CENTER);
}

void
group_fn(struct dialog_data *dlg)
{
	struct terminal *term = dlg->win->term;
	int max = 0, min = 0;
	int w, rw;
	int y = 0;
	max_group_width(term, dlg->dlg->udata, dlg->items, dlg->n - 2, &max);
	min_group_width(term, dlg->dlg->udata, dlg->items, dlg->n - 2, &min);
	max_buttons_width(term, dlg->items + dlg->n - 2, 2, &max);
	min_buttons_width(term, dlg->items + dlg->n - 2, 2, &min);
	w = term->x * 9 / 10 - 2 * DIALOG_LB;
	if (w > max)
		w = max;
	if (w < min)
		w = min;
	if (w > term->x - 2 * DIALOG_LB)
		w = term->x - 2 * DIALOG_LB;
	if (w < 1)
		w = 1;
	rw = 0;
	dlg_format_group(dlg, NULL, dlg->dlg->udata, dlg->items, dlg->n - 2, 0,
	                 &y, w, &rw);
	y++;
	dlg_format_buttons(dlg, NULL, dlg->items + dlg->n - 2, 2, 0, &y, w, &rw,
	                   AL_CENTER);
	w = rw;
	dlg->xw = rw + 2 * DIALOG_LB;
	dlg->yw = y + 2 * DIALOG_TB;
	center_dlg(dlg);
	draw_dlg(dlg);
	y = dlg->y + DIALOG_TB + 1;
	dlg_format_group(dlg, term, dlg->dlg->udata, dlg->items, dlg->n - 2,
	                 dlg->x + DIALOG_LB, &y, w, NULL);
	y++;
	dlg_format_buttons(dlg, term, dlg->items + dlg->n - 2, 2,
	                   dlg->x + DIALOG_LB, &y, w, &rw, AL_CENTER);
}

void
msg_box_fn(struct dialog_data *dlg)
{
	struct terminal *term = dlg->win->term;
	int max = 0, min = 0;
	int w, rw;
	int y = 0;
	unsigned char **ptr;
	unsigned char *text = NULL;
	int textl = 0;
	for (ptr = dlg->dlg->udata; *ptr; ptr++)
		add_to_str(&text, &textl, get_text_translation(*ptr, term));
	max_text_width(term, text, &max, dlg->dlg->align);
	min_text_width(term, text, &min, dlg->dlg->align);
	max_buttons_width(term, dlg->items, dlg->n, &max);
	min_buttons_width(term, dlg->items, dlg->n, &min);
	w = term->x * 9 / 10 - 2 * DIALOG_LB;
	if (w > max)
		w = max;
	if (w < min)
		w = min;
	if (w > term->x - 2 * DIALOG_LB)
		w = term->x - 2 * DIALOG_LB;
	if (w < 1)
		w = 1;
	rw = 0;
	dlg_format_text(dlg, NULL, text, 0, &y, w, &rw, COLOR_DIALOG_TEXT,
	                dlg->dlg->align);
	y++;
	dlg_format_buttons(dlg, NULL, dlg->items, dlg->n, 0, &y, w, &rw,
	                   AL_CENTER);
	w = rw;
	dlg->xw = rw + 2 * DIALOG_LB;
	dlg->yw = y + 2 * DIALOG_TB;
	center_dlg(dlg);
	draw_dlg(dlg);
	y = dlg->y + DIALOG_TB + 1;
	dlg_format_text(dlg, term, text, dlg->x + DIALOG_LB, &y, w, NULL,
	                COLOR_DIALOG_TEXT, dlg->dlg->align);
	y++;
	dlg_format_buttons(dlg, term, dlg->items, dlg->n, dlg->x + DIALOG_LB,
	                   &y, w, NULL, AL_CENTER);
	free(text);
}

static int
msg_box_button(struct dialog_data *dlg, struct dialog_item_data *di)
{
	msg_button_fn msg_fn = di->item->u.msg_fn;
	void *data = dlg->dlg->udata2;
	msg_fn(data);
	cancel_dialog(dlg, di);
	return 0;
}

void
msg_box_null(void *data)
{
}

/* coverity[+free : arg-1] */
void
msg_box(struct terminal *term, struct memory_list *ml, unsigned char *title,
        int align, /*unsigned char *text, ..., void *data, int n,*/...)
{
	struct dialog *dlg;
	int i;
	int n;
	unsigned char *text;
	unsigned char **udata;
	void *udata2;
	int udatan;
	va_list ap;
	va_start(ap, align);
	udata = NULL;
	udatan = 0;
	do {
		text = va_arg(ap, unsigned char *);
		udatan++;
		if ((unsigned)udatan > INT_MAX / sizeof(unsigned char *))
			overalloc();
		udata = xrealloc(udata, udatan * sizeof(unsigned char *));
		udata[udatan - 1] = text;
	} while (text);
	udata2 = va_arg(ap, void *);
	n = va_arg(ap, int);
	if ((unsigned)n
	    > (INT_MAX - sizeof(struct dialog)) / sizeof(struct dialog_item)
	          - 1)
		overalloc();
	dlg = mem_calloc(sizeof(struct dialog)
	                 + (n + 1) * sizeof(struct dialog_item));
	dlg->title = title;
	dlg->fn = msg_box_fn;
	dlg->udata = udata;
	dlg->udata2 = udata2;
	dlg->align = align;
	for (i = 0; i < n; i++) {
		unsigned char *m;
		msg_button_fn msg_fn;
		int flags;
		m = va_arg(ap, unsigned char *);
		msg_fn = va_arg(ap, msg_button_fn);
		flags = va_arg(ap, int);
		if (!m) {
			i--;
			n--;
			continue;
		}
		dlg->items[i].type = D_BUTTON;
		dlg->items[i].gid = flags;
		dlg->items[i].fn = msg_box_button;
		dlg->items[i].dlen = 0;
		dlg->items[i].text = m;
		dlg->items[i].u.msg_fn = msg_fn;
	}
	va_end(ap);
	dlg->items[i].type = D_END;
	add_to_ml(&ml, dlg, udata, NULL);
	do_dialog(term, dlg, ml);
}

void
add_to_history(struct terminal *term, struct history *h, unsigned char *t)
{
	unsigned char *s;
	struct history_item *hi, *hs = NULL;
	struct list_head *lhs;
	size_t l;
	if (!h || !t || !*t)
		return;
	if (term)
		s = convert(term_charset(term), 0, t, NULL);
	else
		s = t;
	l = strlen(cast_const_char s);
	if (l > INT_MAX - sizeof(struct history_item))
		overalloc();
	hi = xmalloc(sizeof(struct history_item) + l);
	memcpy(hi->str, s, l + 1);
	if (term)
		free(s);
	if (term)
		foreach (struct history_item, hs, lhs, h->items)
			if (!strcmp(cast_const_char hs->str,
			            cast_const_char hi->str)) {
				lhs = lhs->prev;
				del_from_list(hs);
				free(hs);
				h->n--;
			}
	add_to_list(h->items, hi);
	h->n++;
	while (h->n > MAX_HISTORY_ITEMS) {
		struct history_item *hd;
		if (list_empty(h->items)) {
			internal("history is empty");
			h->n = 0;
			return;
		}
		hd = list_struct(h->items.prev, struct history_item);
		del_from_list(hd);
		free(hd);
		h->n--;
	}
}

static int
input_field_cancel(struct dialog_data *dlg, struct dialog_item_data *di)
{
	input_field_button_fn fn = di->item->u.input_fn;
	void *data = dlg->dlg->udata2;
	unsigned char *text = dlg->items->cdata;
	fn(data, text);
	cancel_dialog(dlg, di);
	return 0;
}

static int
input_field_ok(struct dialog_data *dlg, struct dialog_item_data *di)
{
	input_field_button_fn fn = di->item->u.input_fn;
	void *data = dlg->dlg->udata2;
	unsigned char *text = dlg->items->cdata;
	if (check_dialog(dlg))
		return 1;
	add_to_history(dlg->win->term, dlg->dlg->items->history, text);
	fn(data, text);
	ok_dialog(dlg, di);
	return 0;
}

static void
input_field_fn(struct dialog_data *dlg)
{
	struct terminal *term = dlg->win->term;
	int max = 0, min = 0;
	int w, rw;
	int y = -1;
	max_text_width(term, dlg->dlg->udata, &max, AL_LEFT);
	min_text_width(term, dlg->dlg->udata, &min, AL_LEFT);
	max_buttons_width(term, dlg->items + 1, dlg->n - 1, &max);
	min_buttons_width(term, dlg->items + 1, dlg->n - 1, &min);
	if (max < dlg->dlg->items->dlen)
		max = dlg->dlg->items->dlen;
	w = term->x * 9 / 10 - 2 * DIALOG_LB;
	if (w > max)
		w = max;
	if (w < min)
		w = min;
	rw = w;
	dlg_format_text_and_field(dlg, NULL, dlg->dlg->udata, dlg->items, 0, &y,
	                          w, &rw, COLOR_DIALOG_TEXT, AL_LEFT);
	y++;
	dlg_format_buttons(dlg, NULL, dlg->items + 1, dlg->n - 1, 0, &y, w, &rw,
	                   AL_CENTER);
	w = rw;
	dlg->xw = rw + 2 * DIALOG_LB;
	dlg->yw = y + 2 * DIALOG_TB;
	center_dlg(dlg);
	draw_dlg(dlg);
	y = dlg->y + DIALOG_TB;
	dlg_format_text_and_field(dlg, term, dlg->dlg->udata, dlg->items,
	                          dlg->x + DIALOG_LB, &y, w, NULL,
	                          COLOR_DIALOG_TEXT, AL_LEFT);
	y++;
	dlg_format_buttons(dlg, term, dlg->items + 1, dlg->n - 1,
	                   dlg->x + DIALOG_LB, &y, w, NULL, AL_CENTER);
}

void
input_field_null(void)
{
}

/* coverity[+free : arg-1] */
void
input_field(struct terminal *term, struct memory_list *ml, unsigned char *title,
            unsigned char *text, void *data, struct history *history, int l,
            unsigned char *def, int min, int max,
            int (*check)(struct dialog_data *, struct dialog_item_data *),
            int n, ...)
{
	struct dialog *dlg;
	unsigned char *field;
	va_list va;
	int i;
	if ((unsigned)n > INT_MAX / sizeof(struct dialog_item) - 2)
		overalloc();
	if ((unsigned)l > INT_MAX - sizeof(struct dialog)
	                      - (2 + n) * sizeof(struct dialog_item))
		overalloc();
	dlg = mem_calloc(sizeof(struct dialog)
	                 + (2 + n) * sizeof(struct dialog_item) + l);
	*(field = (unsigned char *)dlg + sizeof(struct dialog)
	          + (2 + n) * sizeof(struct dialog_item)) = 0;
	if (def) {
		if (strlen(cast_const_char def) + 1 > (size_t)l)
			memcpy(field, def, l - 1);
		else
			strcpy(cast_char field, cast_const_char def);
	}
	dlg->title = title;
	dlg->fn = input_field_fn;
	dlg->udata = text;
	dlg->udata2 = data;
	dlg->items[0].type = D_FIELD;
	dlg->items[0].gid = min;
	dlg->items[0].gnum = max;
	dlg->items[0].fn = check;
	dlg->items[0].history = history;
	dlg->items[0].dlen = l;
	dlg->items[0].data = field;
	va_start(va, n);
	for (i = 1; i <= n; i++) {
		dlg->items[i].type = D_BUTTON;
		dlg->items[i].gid = i == 1 ? B_ENTER : i == n ? B_ESC : 0;
		dlg->items[i].fn =
		    i != n || n == 1 ? input_field_ok : input_field_cancel;
		dlg->items[i].dlen = 0;
		dlg->items[i].text = va_arg(va, unsigned char *);
		dlg->items[i].u.input_fn = va_arg(va, input_field_button_fn);
	}
	va_end(va);

	dlg->items[i].type = D_END;
	add_to_ml(&ml, dlg, NULL);
	do_dialog(term, dlg, ml);
}

int
find_msg_box(struct terminal *term, unsigned char *title,
             int (*sel)(void *, void *), void *data)
{
	struct window *win = NULL;
	struct list_head *lwin;
	foreach (struct window, win, lwin, term->windows)
		if (win->handler == dialog_func) {
			struct dialog_data *dd = win->data;
			struct dialog *d = dd->dlg;
			if (d->fn != msg_box_fn)
				continue;
			if (d->title == title) {
				if (sel && !sel(data, d->udata2))
					continue;
				return 1;
			}
		}
	return 0;
}
