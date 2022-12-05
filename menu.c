/* menu.c
 * (c) 2002 Mikulas Patocka, Petr 'Brain' Kulhavy
 * This file is a part of the Links program, released under GPL.
 */

#include <limits.h>

#include "config.h"
#include "links.h"

static struct history file_history = {
	0, {&file_history.items, &file_history.items}
};

static unsigned char *const version_texts[] = {
	TEXT_(T_LINKS_VERSION),
	TEXT_(T_OPERATING_SYSTEM_VERSION),
	TEXT_(T_WORD_SIZE),
	TEXT_(T_EVENT_HANDLER),
	TEXT_(T_IPV6),
	TEXT_(T_COMPRESSION_METHODS),
	TEXT_(T_ENCRYPTION),
	TEXT_(T_UTF8_TERMINAL),
	TEXT_(T_GRAPHICS_MODE),
	TEXT_(T_CONFIGURATION_DIRECTORY),
	NULL,
};

static size_t
add_and_pad(unsigned char **s, size_t l, struct terminal *term,
            unsigned char *str, int maxlen)
{
	unsigned char *x = get_text_translation(str, term);
	size_t len = strlen((char *)x);
	l = add_to_str(s, l, x);
	l = add_to_str(s, l, cast_uchar ":  ");
	while (len++ < maxlen)
		l = add_chr_to_str(s, l, ' ');
	return l;
}

static void
menu_version(void *term_)
{
	struct terminal *term = (struct terminal *)term_;
	int i;
	int maxlen = 0;
	unsigned char *s;
	size_t l;
	unsigned char *const *text_ptr;
	for (i = 0; version_texts[i]; i++) {
		unsigned char *t = get_text_translation(version_texts[i], term);
		int tl = strlen((char *)t);
		if (tl > maxlen)
			maxlen = tl;
	}

	s = NULL;
	text_ptr = version_texts;

	l = add_and_pad(&s, 0, term, *text_ptr++, maxlen);
	l = add_to_str(&s, l, cast_uchar VERSION);
	l = add_chr_to_str(&s, l, '\n');

	l = add_and_pad(&s, l, term, *text_ptr++, maxlen);
	l = add_to_str(&s, l, system_name);
	l = add_chr_to_str(&s, l, '\n');

	l = add_and_pad(&s, l, term, *text_ptr++, maxlen);
	l = add_to_str(&s, l, get_text_translation(TEXT_(T_MEMORY), term));
	l = add_chr_to_str(&s, l, ' ');
	l = add_num_to_str(&s, l, sizeof(void *) * 8);
	l = add_to_str(&s, l, cast_uchar "-bit, ");
	l = add_to_str(&s, l, get_text_translation(TEXT_(T_FILE_SIZE), term));
	l = add_chr_to_str(&s, l, ' ');
	l = add_num_to_str(&s, l, sizeof(off_t) * 8 /*- ((off_t)-1 < 0)*/);
	l = add_to_str(&s, l, cast_uchar "-bit");
	l = add_chr_to_str(&s, l, '\n');

	l = add_and_pad(&s, l, term, *text_ptr++, maxlen);
	l = add_event_string(&s, l, term);
	l = add_chr_to_str(&s, l, '\n');

	l = add_and_pad(&s, l, term, *text_ptr++, maxlen);
	if (!support_ipv6)
		l = add_to_str(
		    &s, l,
		    get_text_translation(TEXT_(T_NOT_ENABLED_IN_SYSTEM), term));
	else if (!ipv6_full_access())
		l = add_to_str(
		    &s, l,
		    get_text_translation(TEXT_(T_LOCAL_NETWORK_ONLY), term));
	else
		l = add_to_str(&s, l, get_text_translation(TEXT_(T_YES), term));
	l = add_chr_to_str(&s, l, '\n');

	l = add_and_pad(&s, l, term, *text_ptr++, maxlen);
	l = add_compress_methods(&s, l);
	l = add_chr_to_str(&s, l, '\n');

	l = add_and_pad(&s, l, term, *text_ptr++, maxlen);
#ifdef OPENSSL_VERSION
	l = add_to_str(&s, l,
	               (unsigned char *)OpenSSL_version(OPENSSL_VERSION));
#else
	l = add_to_str(&s, l, (unsigned char *)SSLeay_version(SSLEAY_VERSION));
#endif
	l = add_chr_to_str(&s, l, '\n');

	l = add_and_pad(&s, l, term, *text_ptr++, maxlen);
	l = add_to_str(&s, l, get_text_translation(TEXT_(T_YES), term));
	l = add_chr_to_str(&s, l, '\n');
	l = add_and_pad(&s, l, term, *text_ptr++, maxlen);
	l = add_to_str(&s, l, get_text_translation(TEXT_(T_NO), term));
	l = add_chr_to_str(&s, l, '\n');

	l = add_and_pad(&s, l, term, *text_ptr++, maxlen);
	if (links_home) {
		unsigned char *native_home =
		    os_conv_to_external_path(links_home, NULL);
		l = add_to_str(&s, l, native_home);
		free(native_home);
	} else
		l = add_to_str(&s, l,
		               get_text_translation(TEXT_(T_NONE), term));
	l = add_chr_to_str(&s, l, '\n');

	if (*text_ptr)
		internal("menu_version: text mismatched");

	msg_box(term, getml(s, NULL), TEXT_(T_VERSION_INFORMATION),
	        AL_LEFT | AL_MONO, s, MSG_BOX_END, NULL, 1, TEXT_(T_OK),
	        msg_box_null, B_ENTER | B_ESC);
}

static void
menu_about(struct terminal *term, void *d, void *ses_)
{
	msg_box(term, NULL, TEXT_(T_ABOUT), AL_CENTER,
	        TEXT_(T_LINKS__LYNX_LIKE), MSG_BOX_END, (void *)term, 2,
	        TEXT_(T_OK), msg_box_null, B_ENTER | B_ESC, TEXT_(T_VERSION),
	        menu_version, 0);
}

static void
menu_keys(struct terminal *term, void *d, void *ses_)
{
	msg_box(term, NULL, TEXT_(T_KEYS), AL_LEFT | AL_MONO,
	        TEXT_(T_KEYS_DESC), MSG_BOX_END, NULL, 1, TEXT_(T_OK),
	        msg_box_null, B_ENTER | B_ESC);
}

void
activate_keys(struct session *ses)
{
	menu_keys(ses->term, NULL, ses);
}

static void
menu_copying(struct terminal *term, void *d, void *ses_)
{
	msg_box(term, NULL, TEXT_(T_COPYING), AL_CENTER, TEXT_(T_COPYING_DESC),
	        MSG_BOX_END, NULL, 1, TEXT_(T_OK), msg_box_null,
	        B_ENTER | B_ESC);
}

static void
menu_url(struct terminal *term, void *url_, void *ses_)
{
	struct session *ses = (struct session *)ses_;
	unsigned char *url = get_text_translation((unsigned char *)url_, term);
	goto_url_utf8(ses, url);
}

static void
menu_for_frame(struct terminal *term, void *f_, void *ses_)
{
	struct session *ses = (struct session *)ses_;
	void (*f)(struct session *, struct f_data_c *, int) =
	    *(void (*const *)(struct session *, struct f_data_c *, int))f_;
	do_for_frame(ses, f, 0);
}

static void
menu_goto_url(struct terminal *term, void *d, void *ses_)
{
	struct session *ses = (struct session *)ses_;
	dialog_goto_url(ses, cast_uchar "");
}

static void
menu_save_url_as(struct terminal *term, void *d, void *ses_)
{
	struct session *ses = (struct session *)ses_;
	dialog_save_url(ses);
}

static void
menu_go_back(struct terminal *term, void *d, void *ses_)
{
	struct session *ses = (struct session *)ses_;
	go_back(ses, 1);
}

static void
menu_go_forward(struct terminal *term, void *d, void *ses_)
{
	struct session *ses = (struct session *)ses_;
	go_back(ses, -1);
}

static void
menu_reload(struct terminal *term, void *d, void *ses_)
{
	struct session *ses = (struct session *)ses_;
	reload(ses, -1);
}

void
really_exit_prog(void *ses_)
{
	struct session *ses = (struct session *)ses_;
	register_bottom_half(destroy_terminal, ses->term);
}

static void
dont_exit_prog(void *ses_)
{
	struct session *ses = (struct session *)ses_;
	ses->exit_query = 0;
}

void
query_exit(struct session *ses)
{
	int only_one_term =
	    ses->term->list_entry.next == ses->term->list_entry.prev;
	ses->exit_query = 1;
	msg_box(
	    ses->term, NULL, TEXT_(T_EXIT_LINKS), AL_CENTER,
	    only_one_term && are_there_downloads() ? TEXT_(
		T_DO_YOU_REALLY_WANT_TO_EXIT_LINKS_AND_TERMINATE_ALL_DOWNLOADS)
	    : 1 || only_one_term ? TEXT_(T_DO_YOU_REALLY_WANT_TO_EXIT_LINKS)
				 : TEXT_(T_DO_YOU_REALLY_WANT_TO_CLOSE_WINDOW),
	    MSG_BOX_END, (void *)ses, 2, TEXT_(T_YES), really_exit_prog,
	    B_ENTER, TEXT_(T_NO), dont_exit_prog, B_ESC);
}

void
exit_prog(struct terminal *term, void *d, void *ses_)
{
	struct session *ses = (struct session *)ses_;
	int only_one_term;
	if (!ses) {
		register_bottom_half(destroy_terminal, term);
		return;
	}
	only_one_term =
	    ses->term->list_entry.next == ses->term->list_entry.prev;
	if (!ses->exit_query
	    && (!d || (only_one_term && are_there_downloads()))) {
		query_exit(ses);
		return;
	}
	really_exit_prog(ses);
}

struct refresh {
	struct terminal *term;
	struct window *win;
	struct session *ses;
	int (*fn)(struct terminal *term, struct refresh *r);
	void *data;
	struct timer *timer;
};

static void
refresh(void *r_)
{
	struct refresh *r = (struct refresh *)r_;
	r->timer = NULL;
	if (r->fn(r->term, r) > 0)
		return;
	delete_window(r->win);
}

static void
end_refresh(struct refresh *r)
{
	if (r->timer != NULL)
		kill_timer(r->timer);
	free(r);
}

static void
refresh_abort(struct dialog_data *dlg)
{
	end_refresh(dlg->dlg->udata2);
}

static int
resource_info(struct terminal *term, struct refresh *r2)
{
	unsigned char *a;
	int l;
	struct refresh *r;

	r = xmalloc(sizeof(struct refresh));
	r->term = term;
	r->win = NULL;
	r->fn = resource_info;
	r->timer = NULL;
	a = NULL;

	l = add_to_str(&a, 0, get_text_translation(TEXT_(T_RESOURCES), term));
	l = add_to_str(&a, l, cast_uchar ": ");
	add_unsigned_long_num_to_str(&a, &l, select_info(CI_FILES));
	l = add_chr_to_str(&a, l, ' ');
	l = add_to_str(&a, l, get_text_translation(TEXT_(T_HANDLES), term));
	l = add_to_str(&a, l, cast_uchar ", ");
	add_unsigned_long_num_to_str(&a, &l, select_info(CI_TIMERS));
	l = add_chr_to_str(&a, l, ' ');
	l = add_to_str(&a, l, get_text_translation(TEXT_(T_TIMERS), term));
	l = add_to_str(&a, l, cast_uchar ".\n");

	l = add_to_str(&a, l, get_text_translation(TEXT_(T_CONNECTIONS), term));
	l = add_to_str(&a, l, cast_uchar ": ");
	add_unsigned_long_num_to_str(&a, &l,
	                             connect_info(CI_FILES)
	                                 - connect_info(CI_CONNECTING)
	                                 - connect_info(CI_TRANSFER));
	l = add_chr_to_str(&a, l, ' ');
	l = add_to_str(&a, l, get_text_translation(TEXT_(T_WAITING), term));
	l = add_to_str(&a, l, cast_uchar ", ");
	add_unsigned_long_num_to_str(&a, &l, connect_info(CI_CONNECTING));
	l = add_chr_to_str(&a, l, ' ');
	l = add_to_str(&a, l, get_text_translation(TEXT_(T_CONNECTING), term));
	l = add_to_str(&a, l, cast_uchar ", ");
	add_unsigned_long_num_to_str(&a, &l, connect_info(CI_TRANSFER));
	l = add_chr_to_str(&a, l, ' ');
	l = add_to_str(&a, l,
	               get_text_translation(TEXT_(T_tRANSFERRING), term));
	l = add_to_str(&a, l, cast_uchar ", ");
	add_unsigned_long_num_to_str(&a, &l, connect_info(CI_KEEP));
	l = add_chr_to_str(&a, l, ' ');
	l = add_to_str(&a, l, get_text_translation(TEXT_(T_KEEPALIVE), term));
	l = add_to_str(&a, l, cast_uchar ".\n");

	l = add_to_str(&a, l,
	               get_text_translation(TEXT_(T_MEMORY_CACHE), term));
	l = add_to_str(&a, l, cast_uchar ": ");
	add_unsigned_long_num_to_str(&a, &l, cache_info(CI_BYTES));
	l = add_chr_to_str(&a, l, ' ');
	l = add_to_str(&a, l, get_text_translation(TEXT_(T_BYTES), term));
	l = add_to_str(&a, l, cast_uchar ", ");
	add_unsigned_long_num_to_str(&a, &l, cache_info(CI_FILES));
	l = add_chr_to_str(&a, l, ' ');
	l = add_to_str(&a, l, get_text_translation(TEXT_(T_FILES), term));
	l = add_to_str(&a, l, cast_uchar ", ");
	add_unsigned_long_num_to_str(&a, &l, cache_info(CI_LOCKED));
	l = add_chr_to_str(&a, l, ' ');
	l = add_to_str(&a, l, get_text_translation(TEXT_(T_LOCKED), term));
	l = add_to_str(&a, l, cast_uchar ", ");
	add_unsigned_long_num_to_str(&a, &l, cache_info(CI_LOADING));
	l = add_chr_to_str(&a, l, ' ');
	l = add_to_str(&a, l, get_text_translation(TEXT_(T_LOADING), term));
	l = add_to_str(&a, l, cast_uchar ".\n");

	l = add_to_str(&a, l,
	               get_text_translation(TEXT_(T_DECOMPRESSED_CACHE), term));
	l = add_to_str(&a, l, cast_uchar ": ");
	add_unsigned_long_num_to_str(&a, &l, decompress_info(CI_BYTES));
	l = add_chr_to_str(&a, l, ' ');
	l = add_to_str(&a, l, get_text_translation(TEXT_(T_BYTES), term));
	l = add_to_str(&a, l, cast_uchar ", ");
	add_unsigned_long_num_to_str(&a, &l, decompress_info(CI_FILES));
	l = add_chr_to_str(&a, l, ' ');
	l = add_to_str(&a, l, get_text_translation(TEXT_(T_FILES), term));
	l = add_to_str(&a, l, cast_uchar ", ");
	add_unsigned_long_num_to_str(&a, &l, decompress_info(CI_LOCKED));
	l = add_chr_to_str(&a, l, ' ');
	l = add_to_str(&a, l, get_text_translation(TEXT_(T_LOCKED), term));
	l = add_to_str(&a, l, cast_uchar ".\n");

	l = add_to_str(
	    &a, l,
	    get_text_translation(TEXT_(T_FORMATTED_DOCUMENT_CACHE), term));
	l = add_to_str(&a, l, cast_uchar ": ");
	add_unsigned_long_num_to_str(&a, &l, formatted_info(CI_FILES));
	l = add_chr_to_str(&a, l, ' ');
	l = add_to_str(&a, l, get_text_translation(TEXT_(T_DOCUMENTS), term));
	l = add_to_str(&a, l, cast_uchar ", ");
	add_unsigned_long_num_to_str(&a, &l, formatted_info(CI_LOCKED));
	l = add_chr_to_str(&a, l, ' ');
	l = add_to_str(&a, l, get_text_translation(TEXT_(T_LOCKED), term));
	l = add_to_str(&a, l, cast_uchar ".\n");

	l = add_to_str(&a, l, get_text_translation(TEXT_(T_DNS_CACHE), term));
	l = add_to_str(&a, l, cast_uchar ": ");
	add_unsigned_long_num_to_str(&a, &l, dns_info(CI_FILES));
	l = add_chr_to_str(&a, l, ' ');
	l = add_to_str(&a, l, get_text_translation(TEXT_(T_SERVERS), term));
	l = add_to_str(&a, l, cast_uchar ", ");
	l = add_to_str(&a, l,
	               get_text_translation(TEXT_(T_TLS_SESSION_CACHE), term));
	l = add_to_str(&a, l, cast_uchar ": ");
	add_unsigned_long_num_to_str(&a, &l, session_info(CI_FILES));
	l = add_chr_to_str(&a, l, ' ');
	l = add_to_str(&a, l, get_text_translation(TEXT_(T_SERVERS), term));
	l = add_chr_to_str(&a, l, '.');

	if (r2
	    && !strcmp(
		cast_const_char a,
		cast_const_char
		    * (unsigned char **)((struct dialog_data *)r2->win->data)
			  ->dlg->udata)) {
		free(a);
		free(r);
		r2->timer = install_timer(RESOURCE_INFO_REFRESH, refresh, r2);
		return 1;
	}

	msg_box(term, getml(a, NULL), TEXT_(T_RESOURCES), AL_LEFT, a,
	        MSG_BOX_END, (void *)r, 1, TEXT_(T_OK), msg_box_null,
	        B_ENTER | B_ESC);
	r->win = list_struct(term->windows.next, struct window);
	((struct dialog_data *)r->win->data)->dlg->abort = refresh_abort;
	r->timer = install_timer(RESOURCE_INFO_REFRESH, refresh, r);
	return 0;
}

static void
resource_info_menu(struct terminal *term, void *d, void *ses_)
{
	resource_info(term, NULL);
}

static void
flush_caches(struct terminal *term, void *d, void *e)
{
	abort_background_connections();
	shrink_memory(SH_FREE_ALL);
}

/* jde v historii na polozku id_ptr */
void
go_backwards(struct terminal *term, void *id_ptr, void *ses_)
{
	struct session *ses = (struct session *)ses_;
	unsigned want_id = (unsigned)(long)id_ptr;
	struct location *l = NULL;
	struct list_head *ll;
	int n = 0;
	foreach (struct location, l, ll, ses->history) {
		if (l->location_id == want_id) {
			goto have_it;
		}
		n++;
	}
	n = -1;
	foreach (struct location, l, ll, ses->forward_history) {
		if (l->location_id == want_id) {
			goto have_it;
		}
		n--;
	}
	return;

have_it:
	go_back(ses, n);
}

static const struct menu_item no_hist_menu[] = {
	{TEXT_(T_NO_HISTORY), cast_uchar "", M_BAR, NULL, NULL, 0, 0},
	{ NULL,               NULL,          0,     NULL, NULL, 0, 0}
};

static void
add_history_menu_entry(struct terminal *term, struct menu_item **mi, int *n,
                       struct location *l)
{
	unsigned char *url;
	if (!*mi)
		*mi = new_menu(MENU_FREE_ITEMS | MENU_FREE_TEXT);
	url = display_url(term, l->url, 1);
	add_to_menu(mi, url, cast_uchar "", cast_uchar "", go_backwards,
	            (void *)(long)l->location_id, 0, *n);
	(*n)++;
	if (*n == INT_MAX)
		overalloc();
}

static void
history_menu(struct terminal *term, void *ddd, void *ses_)
{
	struct session *ses = (struct session *)ses_;
	struct location *l = NULL;
	struct list_head *ll;
	struct menu_item *mi = NULL;
	int n = 0;
	int selected = 0;
	foreachback (struct location, l, ll, ses->forward_history) {
		add_history_menu_entry(term, &mi, &n, l);
	}
	selected = n;
	foreach (struct location, l, ll, ses->history) {
		add_history_menu_entry(term, &mi, &n, l);
	}
	if (!mi)
		do_menu(term, (struct menu_item *)no_hist_menu, ses);
	else
		do_menu_selected(term, mi, ses, selected, NULL, NULL);
}

static const struct menu_item no_downloads_menu[] = {
	{TEXT_(T_NO_DOWNLOADS), cast_uchar "", M_BAR, NULL, NULL, 0, 0},
	{ NULL,		 NULL,          0,     NULL, NULL, 0, 0}
};

static void
downloads_menu(struct terminal *term, void *ddd, void *ses_)
{
	struct session *ses = (struct session *)ses_;
	struct download *d = NULL;
	struct list_head *ld;
	struct menu_item *mi = NULL;
	int n = 0;
	foreachback (struct download, d, ld, downloads) {
		unsigned char *f, *ff;
		if (!mi)
			mi = new_menu(MENU_FREE_ITEMS | MENU_FREE_TEXT
			              | MENU_FREE_RTEXT);
		f = !d->prog ? d->orig_file : d->url;
		for (ff = f; *ff; ff++)
			if ((dir_sep(ff[0])) && ff[1])
				f = ff + 1;
		if (!d->prog)
			f = stracpy(f);
		else
			f = display_url(term, f, 1);
		add_to_menu(&mi, f, download_percentage(d, 0), cast_uchar "",
		            display_download, d, 0, n);
		n++;
	}
	if (!n)
		do_menu(term, (struct menu_item *)no_downloads_menu, ses);
	else
		do_menu(term, mi, ses);
}

static void
menu_doc_info(struct terminal *term, void *ddd, void *ses_)
{
	struct session *ses = (struct session *)ses_;
	state_msg(ses);
}

static void
menu_head_info(struct terminal *term, void *ddd, void *ses_)
{
	struct session *ses = (struct session *)ses_;
	head_msg(ses);
}

static void
menu_toggle(struct terminal *term, void *ddd, void *ses_)
{
	struct session *ses = (struct session *)ses_;
	toggle(ses, ses->screen, 0);
}

static void
set_val(struct terminal *term, void *ip, void *d)
{
	*(int *)d = (int)(long)ip;
}

static void
terminal_options_ok(void *p)
{
	cls_redraw_all_terminals();
}

static unsigned char *const td_labels[] = { TEXT_(T_NO_FRAMES),
	                                    TEXT_(T_VT_100_FRAMES),
	                                    TEXT_(T_USE_11M),
	                                    TEXT_(
						T_RESTRICT_FRAMES_IN_CP850_852),
	                                    TEXT_(T_BLOCK_CURSOR),
	                                    TEXT_(T_COLOR),
	                                    NULL };

static void
terminal_options(struct terminal *term, void *xxx, void *ses_)
{
	struct dialog *d;
	struct term_spec *ts = new_term_spec(term->term);
	d = mem_calloc(sizeof(struct dialog) + 8 * sizeof(struct dialog_item));
	d->title = TEXT_(T_TERMINAL_OPTIONS);
	d->fn = checkbox_list_fn;
	d->udata = (void *)td_labels;
	d->refresh = terminal_options_ok;
	d->items[0].type = D_CHECKBOX;
	d->items[0].gid = 1;
	d->items[0].gnum = TERM_DUMB;
	d->items[0].dlen = sizeof(int);
	d->items[0].data = (void *)&ts->mode;
	d->items[1].type = D_CHECKBOX;
	d->items[1].gid = 1;
	d->items[1].gnum = TERM_VT100;
	d->items[1].dlen = sizeof(int);
	d->items[1].data = (void *)&ts->mode;
	d->items[2].type = D_CHECKBOX;
	d->items[2].gid = 0;
	d->items[2].dlen = sizeof(int);
	d->items[2].data = (void *)&ts->m11_hack;
	d->items[3].type = D_CHECKBOX;
	d->items[3].gid = 0;
	d->items[3].dlen = sizeof(int);
	d->items[3].data = (void *)&ts->restrict_852;
	d->items[4].type = D_CHECKBOX;
	d->items[4].gid = 0;
	d->items[4].dlen = sizeof(int);
	d->items[4].data = (void *)&ts->block_cursor;
	d->items[5].type = D_CHECKBOX;
	d->items[5].gid = 0;
	d->items[5].dlen = sizeof(int);
	d->items[5].data = (void *)&ts->col;
	d->items[6].type = D_BUTTON;
	d->items[6].gid = B_ENTER;
	d->items[6].fn = ok_dialog;
	d->items[6].text = TEXT_(T_OK);
	d->items[7].type = D_BUTTON;
	d->items[7].gid = B_ESC;
	d->items[7].fn = cancel_dialog;
	d->items[7].text = TEXT_(T_CANCEL);
	d->items[8].type = D_END;
	do_dialog(term, d, getml(d, NULL));
}

static void
refresh_network(void *xxx)
{
	abort_background_connections();
	register_bottom_half(check_queue, NULL);
}

static unsigned char max_c_str[3];
static unsigned char max_cth_str[3];
static unsigned char max_t_str[3];
static unsigned char time_str[5];
static unsigned char unrtime_str[5];
static unsigned char addrtime_str[4];

static void
refresh_connections(void *xxx)
{
	netcfg_stamp++;
	max_connections = atoi(cast_const_char max_c_str);
	max_connections_to_host = atoi(cast_const_char max_cth_str);
	max_tries = atoi(cast_const_char max_t_str);
	receive_timeout = atoi(cast_const_char time_str);
	unrestartable_receive_timeout = atoi(cast_const_char unrtime_str);
	timeout_multiple_addresses = atoi(cast_const_char addrtime_str);
	refresh_network(xxx);
}

static unsigned char *net_msg[10];

static void
dlg_net_options(struct terminal *term, void *xxx, void *yyy)
{
	struct dialog *d;
	int a;
	snprint(max_c_str, 3, max_connections);
	snprint(max_cth_str, 3, max_connections_to_host);
	snprint(max_t_str, 3, max_tries);
	snprint(time_str, 5, receive_timeout);
	snprint(unrtime_str, 5, unrestartable_receive_timeout);
	snprint(addrtime_str, 4, timeout_multiple_addresses);
	d = mem_calloc(sizeof(struct dialog) + 12 * sizeof(struct dialog_item));
	d->title = TEXT_(T_NETWORK_OPTIONS);
	d->fn = group_fn;
	d->udata = (void *)net_msg;
	d->refresh = refresh_connections;
	a = 0;
	net_msg[a] = TEXT_(T_MAX_CONNECTIONS);
	d->items[a].type = D_FIELD;
	d->items[a].data = max_c_str;
	d->items[a].dlen = 3;
	d->items[a].fn = check_number;
	d->items[a].gid = 1;
	d->items[a++].gnum = 99;
	net_msg[a] = TEXT_(T_MAX_CONNECTIONS_TO_ONE_HOST);
	d->items[a].type = D_FIELD;
	d->items[a].data = max_cth_str;
	d->items[a].dlen = 3;
	d->items[a].fn = check_number;
	d->items[a].gid = 1;
	d->items[a++].gnum = 99;
	net_msg[a] = TEXT_(T_RETRIES);
	d->items[a].type = D_FIELD;
	d->items[a].data = max_t_str;
	d->items[a].dlen = 3;
	d->items[a].fn = check_number;
	d->items[a].gid = 0;
	d->items[a++].gnum = 16;
	net_msg[a] = TEXT_(T_RECEIVE_TIMEOUT_SEC);
	d->items[a].type = D_FIELD;
	d->items[a].data = time_str;
	d->items[a].dlen = 5;
	d->items[a].fn = check_number;
	d->items[a].gid = 1;
	d->items[a++].gnum = 9999;
	net_msg[a] = TEXT_(T_TIMEOUT_WHEN_UNRESTARTABLE);
	d->items[a].type = D_FIELD;
	d->items[a].data = unrtime_str;
	d->items[a].dlen = 5;
	d->items[a].fn = check_number;
	d->items[a].gid = 1;
	d->items[a++].gnum = 9999;
	net_msg[a] = TEXT_(T_TIMEOUT_WHEN_TRYING_MULTIPLE_ADDRESSES);
	d->items[a].type = D_FIELD;
	d->items[a].data = addrtime_str;
	d->items[a].dlen = 4;
	d->items[a].fn = check_number;
	d->items[a].gid = 1;
	d->items[a++].gnum = 999;
	net_msg[a] = TEXT_(T_BIND_TO_LOCAL_IP_ADDRESS);
	d->items[a].type = D_FIELD;
	d->items[a].data = bind_ip_address;
	d->items[a].dlen = sizeof(bind_ip_address);
	d->items[a++].fn = check_local_ip_address;
	if (support_ipv6) {
		net_msg[a] = TEXT_(T_BIND_TO_LOCAL_IPV6_ADDRESS);
		d->items[a].type = D_FIELD;
		d->items[a].data = bind_ipv6_address;
		d->items[a].dlen = sizeof(bind_ipv6_address);
		d->items[a++].fn = check_local_ipv6_address;
	}
	net_msg[a] = TEXT_(T_SET_TIME_OF_DOWNLOADED_FILES);
	d->items[a].type = D_CHECKBOX;
	d->items[a].data = (unsigned char *)&download_utime;
	d->items[a++].dlen = sizeof(int);
	d->items[a].type = D_BUTTON;
	d->items[a].gid = B_ENTER;
	d->items[a].fn = ok_dialog;
	d->items[a++].text = TEXT_(T_OK);
	d->items[a].type = D_BUTTON;
	d->items[a].gid = B_ESC;
	d->items[a].fn = cancel_dialog;
	d->items[a++].text = TEXT_(T_CANCEL);
	d->items[a].type = D_END;
	do_dialog(term, d, getml(d, NULL));
}

static unsigned char *const ipv6_labels[] = {
	TEXT_(T_IPV6_DEFAULT),       TEXT_(T_IPV6_PREFER_IPV4),
	TEXT_(T_IPV6_PREFER_IPV6),   TEXT_(T_IPV6_USE_ONLY_IPV4),
	TEXT_(T_IPV6_USE_ONLY_IPV6), NULL
};

static void
dlg_ipv6_options(struct terminal *term, void *xxx, void *yyy)
{
	struct dialog *d;
	d = mem_calloc(sizeof(struct dialog) + 7 * sizeof(struct dialog_item));
	d->title = TEXT_(T_IPV6_OPTIONS);
	d->fn = checkbox_list_fn;
	d->udata = (void *)ipv6_labels;
	d->refresh = refresh_network;
	d->items[0].type = D_CHECKBOX;
	d->items[0].gid = 1;
	d->items[0].gnum = ADDR_PREFERENCE_DEFAULT;
	d->items[0].dlen = sizeof(int);
	d->items[0].data = (void *)&ipv6_options.addr_preference;
	d->items[1].type = D_CHECKBOX;
	d->items[1].gid = 1;
	d->items[1].gnum = ADDR_PREFERENCE_IPV4;
	d->items[1].dlen = sizeof(int);
	d->items[1].data = (void *)&ipv6_options.addr_preference;
	d->items[2].type = D_CHECKBOX;
	d->items[2].gid = 1;
	d->items[2].gnum = ADDR_PREFERENCE_IPV6;
	d->items[2].dlen = sizeof(int);
	d->items[2].data = (void *)&ipv6_options.addr_preference;
	d->items[3].type = D_CHECKBOX;
	d->items[3].gid = 1;
	d->items[3].gnum = ADDR_PREFERENCE_IPV4_ONLY;
	d->items[3].dlen = sizeof(int);
	d->items[3].data = (void *)&ipv6_options.addr_preference;
	d->items[4].type = D_CHECKBOX;
	d->items[4].gid = 1;
	d->items[4].gnum = ADDR_PREFERENCE_IPV6_ONLY;
	d->items[4].dlen = sizeof(int);
	d->items[4].data = (void *)&ipv6_options.addr_preference;
	d->items[5].type = D_BUTTON;
	d->items[5].gid = B_ENTER;
	d->items[5].fn = ok_dialog;
	d->items[5].text = TEXT_(T_OK);
	d->items[6].type = D_BUTTON;
	d->items[6].gid = B_ESC;
	d->items[6].fn = cancel_dialog;
	d->items[6].text = TEXT_(T_CANCEL);
	d->items[7].type = D_END;
	do_dialog(term, d, getml(d, NULL));
}

#define N_N 6

static unsigned char *const proxy_msg[] = {
	TEXT_(T_HTTP_PROXY__HOST_PORT),
	TEXT_(T_HTTPS_PROXY__HOST_PORT),
	TEXT_(T_SOCKS_4A_PROXY__USER_HOST_PORT),
	TEXT_(T_APPEND_TEXT_TO_SOCKS_LOOKUPS),
	TEXT_(T_NOPROXY_LIST),
	TEXT_(T_ONLY_PROXIES),
};

static void
proxy_fn(struct dialog_data *dlg)
{
	struct terminal *term = dlg->win->term;
	int max = 0, min = 0;
	int w, rw;
	int i;
	int y = -1;
	for (i = 0; i < N_N; i++) {
		max_text_width(term, proxy_msg[i], &max, AL_LEFT);
		min_text_width(term, proxy_msg[i], &min, AL_LEFT);
	}
	max_group_width(term, proxy_msg + N_N, dlg->items + N_N,
	                dlg->n - 2 - N_N, &max);
	min_group_width(term, proxy_msg + N_N, dlg->items + N_N,
	                dlg->n - 2 - N_N, &min);
	max_buttons_width(term, dlg->items + dlg->n - 2, 2, &max);
	min_buttons_width(term, dlg->items + dlg->n - 2, 2, &min);
	w = dlg->win->term->x * 9 / 10 - 2 * DIALOG_LB;
	if (w > max)
		w = max;
	if (w < min)
		w = min;
	if (w > dlg->win->term->x - 2 * DIALOG_LB)
		w = dlg->win->term->x - 2 * DIALOG_LB;
	if (w < 1)
		w = 1;
	rw = 0;
	for (i = 0; i < N_N; i++) {
		dlg_format_text_and_field(dlg, NULL, proxy_msg[i],
		                          &dlg->items[i], 0, &y, w, &rw,
		                          COLOR_DIALOG_TEXT, AL_LEFT);
		y++;
	}
	dlg_format_group(dlg, NULL, proxy_msg + N_N, dlg->items + N_N,
	                 dlg->n - 2 - N_N, 0, &y, w, &rw);
	y++;
	dlg_format_buttons(dlg, NULL, dlg->items + dlg->n - 2, 2, 0, &y, w, &rw,
	                   AL_CENTER);
	w = rw;
	dlg->xw = w + 2 * DIALOG_LB;
	dlg->yw = y + 2 * DIALOG_TB;
	center_dlg(dlg);
	draw_dlg(dlg);
	y = dlg->y + DIALOG_TB;
	for (i = 0; i < N_N; i++) {
		dlg_format_text_and_field(
		    dlg, term, proxy_msg[i], &dlg->items[i], dlg->x + DIALOG_LB,
		    &y, w, NULL, COLOR_DIALOG_TEXT, AL_LEFT);
		y++;
	}
	dlg_format_group(dlg, term, proxy_msg + N_N, &dlg->items[N_N],
	                 dlg->n - 2 - N_N, dlg->x + DIALOG_LB, &y, w, NULL);
	y++;
	dlg_format_buttons(dlg, term, &dlg->items[dlg->n - 2], 2,
	                   dlg->x + DIALOG_LB, &y, w, NULL, AL_CENTER);
}

void
reset_settings_for_tor(void)
{
	max_connections = 10;
	max_connections_to_host = 8;
	max_tries = 3;
	receive_timeout = 120;
	unrestartable_receive_timeout = 600;

	max_format_cache_entries = 5;
	memory_cache_size = 4194304;
	image_cache_size = 1048576;
	font_cache_size = 2097152;
	aggressive_cache = 1;

	http_options.http10 = 0;
	http_options.allow_blacklist = 1;
	http_options.no_accept_charset = 0;
	http_options.no_compression = 0;
	http_options.retry_internal_errors = 0;
	http_options.header.extra_header[0] = 0;

	dither_letters = 1;
	dither_images = 1;

	dds.tables = 1;
	dds.frames = 1;
	dds.auto_refresh = 0;
	dds.display_images = 1;
}

static void
data_cleanup(void)
{
	struct session *ses = NULL;
	struct list_head *lses;
	reset_settings_for_tor();
	foreach (struct session, ses, lses, sessions) {
		ses->ds.tables = dds.tables;
		ses->ds.frames = dds.frames;
		ses->ds.auto_refresh = dds.auto_refresh;
		ses->ds.display_images = dds.display_images;
		cleanup_session(ses);
		draw_formatted(ses);
	}
	free_blacklist();
	free_cookies();
	free_auth();
	abort_all_connections();
	shrink_memory(SH_FREE_ALL);
}

static unsigned char http_proxy[MAX_STR_LEN];
static unsigned char https_proxy[MAX_STR_LEN];
static unsigned char socks_proxy[MAX_STR_LEN];
static unsigned char no_proxy[MAX_STR_LEN];

static void
display_proxy(struct terminal *term, unsigned char *result,
              unsigned char *proxy)
{
	unsigned char *url, *res;
	int sl;

	if (!proxy[0]) {
		result[0] = 0;
		return;
	}

	url = stracpy(cast_uchar "proxy://");
	add_to_strn(&url, proxy);
	add_to_strn(&url, cast_uchar "/");

	res = display_url(term, url, 0);

	sl = (int)strlen(cast_const_char res);
	if (sl < 9 || strncmp(cast_const_char res, "proxy://", 8)
	    || res[sl - 1] != '/') {
		result[0] = 0;
	} else {
		res[sl - 1] = 0;
		safe_strncpy(result, res + 8, MAX_STR_LEN);
	}

	free(url);
	free(res);
}

static void
display_noproxy_list(struct terminal *term, unsigned char *result,
                     unsigned char *noproxy_list)
{
	unsigned char *res;
	res = display_host_list(term, noproxy_list);
	if (!res) {
		result[0] = 0;
	} else
		safe_strncpy(result, res, MAX_STR_LEN);
	free(res);
}

int
save_proxy(int charset, unsigned char *result, unsigned char *proxy)
{
	unsigned char *url, *res;
	int sl;
	int retval;

	if (!proxy[0]) {
		result[0] = 0;
		return 0;
	}

	proxy = stracpy(proxy);

	url = stracpy(cast_uchar "proxy://");
	add_to_strn(&url, proxy);
	add_to_strn(&url, cast_uchar "/");

	free(proxy);

	if (parse_url(url, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	              NULL, NULL, NULL)) {
		free(url);
		result[0] = 0;
		return -1;
	}

	res = idn_encode_url(url, 0);

	free(url);

	if (!res) {
		result[0] = 0;
		return -1;
	}
	sl = (int)strlen(cast_const_char res);
	if (sl < 9 || strncmp(cast_const_char res, "proxy://", 8)
	    || res[sl - 1] != '/') {
		result[0] = 0;
		retval = -1;
	} else {
		res[sl - 1] = 0;
		safe_strncpy(result, res + 8, MAX_STR_LEN);
		retval =
		    strlen(cast_const_char(res + 8)) >= MAX_STR_LEN ? -1 : 0;
	}

	free(res);

	return retval;
}

int
save_noproxy_list(int charset, unsigned char *result,
                  unsigned char *noproxy_list)
{
	unsigned char *res;

	noproxy_list = stracpy(noproxy_list);
	res = idn_encode_host(noproxy_list,
	                      (int)strlen(cast_const_char noproxy_list),
	                      cast_uchar ".,", 0);
	free(noproxy_list);
	if (!res) {
		result[0] = 0;
		return -1;
	} else {
		safe_strncpy(result, res, MAX_STR_LEN);
		retval = strlen(cast_const_char res) >= MAX_STR_LEN ? -1 : 0;
	}
	free(res);
	return retval;
}

static int
check_proxy_noproxy(struct dialog_data *dlg, struct dialog_item_data *di,
                    int (*save)(int, unsigned char *, unsigned char *))
{
	unsigned char *result = xmalloc(MAX_STR_LEN);
	if (save(0, result, di->cdata)) {
		free(result);
		msg_box(dlg->win->term, NULL, TEXT_(T_BAD_STRING), AL_CENTER,
		        TEXT_(T_BAD_PROXY_SYNTAX), MSG_BOX_END, NULL, 1,
		        TEXT_(T_CANCEL), msg_box_null, B_ENTER | B_ESC);
		return 1;
	}
	free(result);
	return 0;
}

static int
check_proxy(struct dialog_data *dlg, struct dialog_item_data *di)
{
	return check_proxy_noproxy(dlg, di, save_proxy);
}

static int
check_noproxy_list(struct dialog_data *dlg, struct dialog_item_data *di)
{
	return check_proxy_noproxy(dlg, di, save_noproxy_list);
}

static int
proxy_ok_dialog(struct dialog_data *dlg, struct dialog_item_data *di)
{
	struct terminal *term = dlg->win->term;
	int op = proxies.only_proxies;
	int r = ok_dialog(dlg, di);
	if (r)
		return r;
	save_proxy(0, proxies.http_proxy, http_proxy);
	save_proxy(0, proxies.https_proxy, https_proxy);
	save_proxy(0, proxies.socks_proxy, socks_proxy);
	save_noproxy_list(0, proxies.no_proxy, no_proxy);

	if (!proxies.only_proxies) {
		/* parsing duplicated in make_connection */
		long lp;
		char *end;
		unsigned char *p =
		    cast_uchar strchr(cast_const_char proxies.socks_proxy, '@');
		if (!p)
			p = proxies.socks_proxy;
		else
			p++;
		p = cast_uchar strchr(cast_const_char p, ':');
		if (p) {
			p++;
			lp = strtol(cast_const_char p, &end, 10);
			if (!*end && lp == 9050) {
				proxies.only_proxies = 1;
				msg_box(term, NULL, TEXT_(T_PROXIES), AL_LEFT,
				        TEXT_(T_TOR_MODE_ENABLED), MSG_BOX_END,
				        NULL, 1, TEXT_(T_OK), msg_box_null,
				        B_ENTER | B_ESC);
			}
		}
	}

	if (op != proxies.only_proxies) {
		data_cleanup();
	}
	refresh_network(NULL);
	return 0;
}

static void
dlg_proxy_options(struct terminal *term, void *xxx, void *yyy)
{
	struct dialog *d;
	int a = 0;
	display_proxy(term, http_proxy, proxies.http_proxy);
	display_proxy(term, https_proxy, proxies.https_proxy);
	display_proxy(term, socks_proxy, proxies.socks_proxy);
	display_noproxy_list(term, no_proxy, proxies.no_proxy);
	d = mem_calloc(sizeof(struct dialog)
	               + (N_N + 2) * sizeof(struct dialog_item));
	d->title = TEXT_(T_PROXIES);
	d->fn = proxy_fn;
	d->items[a].type = D_FIELD;
	d->items[a].dlen = MAX_STR_LEN;
	d->items[a].data = http_proxy;
	d->items[a].fn = check_proxy;
	a++;
	d->items[a].type = D_FIELD;
	d->items[a].dlen = MAX_STR_LEN;
	d->items[a].data = https_proxy;
	d->items[a].fn = check_proxy;
	a++;
	d->items[a].type = D_FIELD;
	d->items[a].dlen = MAX_STR_LEN;
	d->items[a].data = socks_proxy;
	d->items[a].fn = check_proxy;
	a++;
	d->items[a].type = D_FIELD;
	d->items[a].dlen = MAX_STR_LEN;
	d->items[a].data = proxies.dns_append;
	a++;
	d->items[a].type = D_FIELD;
	d->items[a].dlen = MAX_STR_LEN;
	d->items[a].data = no_proxy;
	d->items[a].fn = check_noproxy_list;
	a++;
	d->items[a].type = D_CHECKBOX;
	d->items[a].data = (unsigned char *)&proxies.only_proxies;
	d->items[a].dlen = sizeof(int);
	a++;
	d->items[a].type = D_BUTTON;
	d->items[a].gid = B_ENTER;
	d->items[a].fn = proxy_ok_dialog;
	d->items[a].text = TEXT_(T_OK);
	a++;
	d->items[a].type = D_BUTTON;
	d->items[a].gid = B_ESC;
	d->items[a].fn = cancel_dialog;
	d->items[a].text = TEXT_(T_CANCEL);
	a++;
	d->items[a].type = D_END;
	do_dialog(term, d, getml(d, NULL));
}

#undef N_N

static int
check_file(struct dialog_data *dlg, struct dialog_item_data *di, int type)
{
	unsigned char *p = di->cdata;
	int r;
	struct stat st;
	links_ssl *ssl;
	if (!p[0])
		return 0;
	EINTRLOOP(r, stat(cast_const_char p, &st));
	if (r || !S_ISREG(st.st_mode)) {
		msg_box(dlg->win->term, NULL, TEXT_(T_BAD_FILE), AL_CENTER,
		        TEXT_(T_THE_FILE_DOES_NOT_EXIST), MSG_BOX_END, NULL, 1,
		        TEXT_(T_CANCEL), msg_box_null, B_ENTER | B_ESC);
		return 1;
	}
	ssl = getSSL();
	if (!ssl)
		return 0;
#if !defined(OPENSSL_NO_STDIO)
	if (!type) {
		r = SSL_use_PrivateKey_file(ssl->ssl, cast_const_char p,
		                            SSL_FILETYPE_PEM);
		r = r != 1;
	} else {
		r = SSL_use_certificate_file(ssl->ssl, cast_const_char p,
		                             SSL_FILETYPE_PEM);
		r = r != 1;
	}
#else
	r = 0;
#endif
	if (r)
		msg_box(dlg->win->term, NULL, TEXT_(T_BAD_FILE), AL_CENTER,
		        TEXT_(T_THE_FILE_HAS_INVALID_FORMAT), MSG_BOX_END, NULL,
		        1, TEXT_(T_CANCEL), msg_box_null, B_ENTER | B_ESC);
	freeSSL(ssl);
	return r;
}

static int
check_file_key(struct dialog_data *dlg, struct dialog_item_data *di)
{
	return check_file(dlg, di, 0);
}

static int
check_file_crt(struct dialog_data *dlg, struct dialog_item_data *di)
{
	return check_file(dlg, di, 1);
}

static unsigned char *const ssl_labels[] = {
	TEXT_(T_ACCEPT_INVALID_CERTIFICATES),
	TEXT_(T_WARN_ON_INVALID_CERTIFICATES),
	TEXT_(T_REJECT_INVALID_CERTIFICATES),
	TEXT_(T_CLIENT_CERTIFICATE_KEY_FILE),
	TEXT_(T_CLIENT_CERTIFICATE_FILE),
	TEXT_(T_CLIENT_CERTIFICATE_KEY_PASSWORD),
	NULL
};

static void
ssl_options_fn(struct dialog_data *dlg)
{
	struct terminal *term = dlg->win->term;
	int max = 0, min = 0;
	int w, rw;
	int y = 0;
	checkboxes_width(term, dlg->dlg->udata, dlg->n - 4, &max,
	                 max_text_width);
	checkboxes_width(term, dlg->dlg->udata, dlg->n - 4, &min,
	                 min_text_width);
	max_text_width(term, ssl_labels[dlg->n - 4], &max, AL_LEFT);
	min_text_width(term, ssl_labels[dlg->n - 4], &min, AL_LEFT);
	max_text_width(term, ssl_labels[dlg->n - 3], &max, AL_LEFT);
	min_text_width(term, ssl_labels[dlg->n - 3], &min, AL_LEFT);
	max_buttons_width(term, dlg->items + dlg->n - 2, 2, &max);
	min_buttons_width(term, dlg->items + dlg->n - 2, 2, &min);
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
	dlg_format_checkboxes(dlg, NULL, dlg->items, dlg->n - 5, 0, &y, w, &rw,
	                      dlg->dlg->udata);
	y++;
	dlg_format_text_and_field(dlg, NULL, ssl_labels[dlg->n - 5],
	                          dlg->items + dlg->n - 5, 0, &y, w, &rw,
	                          COLOR_DIALOG_TEXT, AL_LEFT);
	y++;
	dlg_format_text_and_field(dlg, NULL, ssl_labels[dlg->n - 4],
	                          dlg->items + dlg->n - 4, 0, &y, w, &rw,
	                          COLOR_DIALOG_TEXT, AL_LEFT);
	y++;
	dlg_format_text_and_field(dlg, NULL, ssl_labels[dlg->n - 3],
	                          dlg->items + dlg->n - 3, 0, &y, w, &rw,
	                          COLOR_DIALOG_TEXT, AL_LEFT);
	y++;
	dlg_format_buttons(dlg, NULL, dlg->items + dlg->n - 2, 2, 0, &y, w, &rw,
	                   AL_CENTER);
	w = rw;
	dlg->xw = rw + 2 * DIALOG_LB;
	dlg->yw = y + 2 * DIALOG_TB;
	center_dlg(dlg);
	draw_dlg(dlg);
	y = dlg->y + DIALOG_TB + 1;
	dlg_format_checkboxes(dlg, term, dlg->items, dlg->n - 5,
	                      dlg->x + DIALOG_LB, &y, w, NULL, dlg->dlg->udata);
	y++;
	dlg_format_text_and_field(dlg, term, ssl_labels[dlg->n - 5],
	                          dlg->items + dlg->n - 5, dlg->x + DIALOG_LB,
	                          &y, w, NULL, COLOR_DIALOG_TEXT, AL_LEFT);
	y++;
	dlg_format_text_and_field(dlg, term, ssl_labels[dlg->n - 4],
	                          dlg->items + dlg->n - 4, dlg->x + DIALOG_LB,
	                          &y, w, NULL, COLOR_DIALOG_TEXT, AL_LEFT);
	y++;
	dlg_format_text_and_field(dlg, term, ssl_labels[dlg->n - 3],
	                          dlg->items + dlg->n - 3, dlg->x + DIALOG_LB,
	                          &y, w, NULL, COLOR_DIALOG_TEXT, AL_LEFT);
	y++;
	dlg_format_buttons(dlg, term, dlg->items + dlg->n - 2, 2,
	                   dlg->x + DIALOG_LB, &y, w, &rw, AL_CENTER);
}

static void
dlg_ssl_options(struct terminal *term, void *xxx, void *yyy)
{
	struct dialog *d;
	int a = 0;
	const int items = 8;
	d = mem_calloc(sizeof(struct dialog)
	               + items * sizeof(struct dialog_item));
	d->title = TEXT_(T_SSL_OPTIONS);
	d->fn = ssl_options_fn;
	d->udata = (void *)ssl_labels;
	d->refresh = refresh_network;
	d->items[a].type = D_CHECKBOX;
	d->items[a].gid = 1;
	d->items[a].gnum = SSL_ACCEPT_INVALID_CERTIFICATE;
	d->items[a].dlen = sizeof(int);
	d->items[a].data = (void *)&ssl_options.certificates;
	a++;
	d->items[a].type = D_CHECKBOX;
	d->items[a].gid = 1;
	d->items[a].gnum = SSL_WARN_ON_INVALID_CERTIFICATE;
	d->items[a].dlen = sizeof(int);
	d->items[a].data = (void *)&ssl_options.certificates;
	a++;
	d->items[a].type = D_CHECKBOX;
	d->items[a].gid = 1;
	d->items[a].gnum = SSL_REJECT_INVALID_CERTIFICATE;
	d->items[a].dlen = sizeof(int);
	d->items[a].data = (void *)&ssl_options.certificates;
	a++;
	d->items[a].type = D_FIELD;
	d->items[a].dlen = MAX_STR_LEN;
	d->items[a].data = ssl_options.client_cert_key;
	d->items[a].fn = check_file_key;
	a++;
	d->items[a].type = D_FIELD;
	d->items[a].dlen = MAX_STR_LEN;
	d->items[a].data = ssl_options.client_cert_crt;
	d->items[a].fn = check_file_crt;
	a++;
	d->items[a].type = D_FIELD_PASS;
	d->items[a].dlen = MAX_STR_LEN;
	d->items[a].data = ssl_options.client_cert_password;
	a++;
	d->items[a].type = D_BUTTON;
	d->items[a].gid = B_ENTER;
	d->items[a].fn = ok_dialog;
	d->items[a].text = TEXT_(T_OK);
	a++;
	d->items[a].type = D_BUTTON;
	d->items[a].gid = B_ESC;
	d->items[a].fn = cancel_dialog;
	d->items[a].text = TEXT_(T_CANCEL);
	a++;
	d->items[a].type = D_END;
	do_dialog(term, d, getml(d, NULL));
}

static unsigned char *const http_labels[] = {
	TEXT_(T_USE_HTTP_10),
	TEXT_(T_ALLOW_SERVER_BLACKLIST),
	TEXT_(T_DO_NOT_SEND_ACCEPT_CHARSET),
	TEXT_(T_DO_NOT_ADVERTISE_COMPRESSION_SUPPORT),
	TEXT_(T_RETRY_ON_INTERNAL_ERRORS),
	NULL
};

static unsigned char *const http_header_labels[] = { TEXT_(T_FAKE_FIREFOX),
	                                             TEXT_(T_FAKE_USERAGENT),
	                                             TEXT_(T_EXTRA_HEADER),
	                                             NULL };

static void
httpheadopt_fn(struct dialog_data *dlg)
{
	struct terminal *term = dlg->win->term;
	int max = 0, min = 0;
	int w, rw;
	int y = 0;
	checkboxes_width(term, dlg->dlg->udata, dlg->n - 5, &max,
	                 max_text_width);
	checkboxes_width(term, dlg->dlg->udata, dlg->n - 5, &min,
	                 min_text_width);
	max_text_width(term, http_header_labels[dlg->n - 5], &max, AL_LEFT);
	min_text_width(term, http_header_labels[dlg->n - 5], &min, AL_LEFT);
	max_text_width(term, http_header_labels[dlg->n - 4], &max, AL_LEFT);
	min_text_width(term, http_header_labels[dlg->n - 4], &min, AL_LEFT);
	max_text_width(term, http_header_labels[dlg->n - 3], &max, AL_LEFT);
	min_text_width(term, http_header_labels[dlg->n - 3], &min, AL_LEFT);
	max_buttons_width(term, dlg->items + dlg->n - 2, 2, &max);
	min_buttons_width(term, dlg->items + dlg->n - 2, 2, &min);
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
	dlg_format_checkboxes(dlg, NULL, dlg->items, dlg->n - 5, 0, &y, w, &rw,
	                      dlg->dlg->udata);
	y++;
	dlg_format_text_and_field(dlg, NULL, http_header_labels[dlg->n - 5],
	                          dlg->items + dlg->n - 5, 0, &y, w, &rw,
	                          COLOR_DIALOG_TEXT, AL_LEFT);
	y++;
	dlg_format_text_and_field(dlg, NULL, http_header_labels[dlg->n - 4],
	                          dlg->items + dlg->n - 4, 0, &y, w, &rw,
	                          COLOR_DIALOG_TEXT, AL_LEFT);
	y++;
	dlg_format_text_and_field(dlg, NULL, http_header_labels[dlg->n - 3],
	                          dlg->items + dlg->n - 3, 0, &y, w, &rw,
	                          COLOR_DIALOG_TEXT, AL_LEFT);
	y++;
	dlg_format_buttons(dlg, NULL, dlg->items + dlg->n - 2, 2, 0, &y, w, &rw,
	                   AL_CENTER);
	w = rw;
	dlg->xw = rw + 2 * DIALOG_LB;
	dlg->yw = y + 2 * DIALOG_TB;
	center_dlg(dlg);
	draw_dlg(dlg);
	y = dlg->y + DIALOG_TB + 1;
	dlg_format_checkboxes(dlg, term, dlg->items, dlg->n - 5,
	                      dlg->x + DIALOG_LB, &y, w, NULL, dlg->dlg->udata);
	y++;
	dlg_format_text_and_field(dlg, term, http_header_labels[dlg->n - 5],
	                          dlg->items + dlg->n - 5, dlg->x + DIALOG_LB,
	                          &y, w, NULL, COLOR_DIALOG_TEXT, AL_LEFT);
	y++;
	dlg_format_text_and_field(dlg, term, http_header_labels[dlg->n - 4],
	                          dlg->items + dlg->n - 4, dlg->x + DIALOG_LB,
	                          &y, w, NULL, COLOR_DIALOG_TEXT, AL_LEFT);
	y++;
	dlg_format_text_and_field(dlg, term, http_header_labels[dlg->n - 3],
	                          dlg->items + dlg->n - 3, dlg->x + DIALOG_LB,
	                          &y, w, NULL, COLOR_DIALOG_TEXT, AL_LEFT);
	y++;
	dlg_format_buttons(dlg, term, dlg->items + dlg->n - 2, 2,
	                   dlg->x + DIALOG_LB, &y, w, &rw, AL_CENTER);
}

static int
dlg_http_header_options(struct dialog_data *dlg, struct dialog_item_data *di)
{
	struct http_header_options *header =
	    (struct http_header_options *)di->cdata;
	struct dialog *d;
	d = mem_calloc(sizeof(struct dialog) + 12 * sizeof(struct dialog_item));
	d->title = TEXT_(T_HTTP_HEADER_OPTIONS);
	d->fn = httpheadopt_fn;
	d->udata = (void *)http_header_labels;
	d->items[0].type = D_CHECKBOX;
	d->items[0].gid = 0;
	d->items[0].dlen = sizeof(int);
	d->items[0].data = (void *)&header->fake_firefox;
	d->items[1].type = D_FIELD;
	d->items[1].dlen = MAX_STR_LEN;
	d->items[1].data = header->fake_useragent;
	d->items[2].type = D_FIELD;
	d->items[2].dlen = MAX_STR_LEN;
	d->items[2].data = header->extra_header;
	d->items[3].type = D_BUTTON;
	d->items[3].gid = B_ENTER;
	d->items[3].fn = ok_dialog;
	d->items[3].text = TEXT_(T_OK);
	d->items[4].type = D_BUTTON;
	d->items[4].gid = B_ESC;
	d->items[4].fn = cancel_dialog;
	d->items[4].text = TEXT_(T_CANCEL);
	d->items[5].type = D_END;
	do_dialog(dlg->win->term, d, getml(d, NULL));
	return 0;
}

static void
dlg_http_options(struct terminal *term, void *xxx, void *yyy)
{
	struct dialog *d;
	int a = 0;
	d = mem_calloc(sizeof(struct dialog) + 8 * sizeof(struct dialog_item));
	d->title = TEXT_(T_HTTP_BUG_WORKAROUNDS);
	d->fn = checkbox_list_fn;
	d->udata = (void *)http_labels;
	d->refresh = refresh_network;
	d->items[a].type = D_CHECKBOX;
	d->items[a].gid = 0;
	d->items[a].dlen = sizeof(int);
	d->items[a].data = (void *)&http_options.http10;
	a++;
	d->items[a].type = D_CHECKBOX;
	d->items[a].gid = 0;
	d->items[a].dlen = sizeof(int);
	d->items[a].data = (void *)&http_options.allow_blacklist;
	a++;
	d->items[a].type = D_CHECKBOX;
	d->items[a].gid = 0;
	d->items[a].dlen = sizeof(int);
	d->items[a].data = (void *)&http_options.no_accept_charset;
	a++;
	d->items[a].type = D_CHECKBOX;
	d->items[a].gid = 0;
	d->items[a].dlen = sizeof(int);
	d->items[a].data = (void *)&http_options.no_compression;
	a++;
	d->items[a].type = D_CHECKBOX;
	d->items[a].gid = 0;
	d->items[a].dlen = sizeof(int);
	d->items[a].data = (void *)&http_options.retry_internal_errors;
	a++;
	d->items[a].type = D_BUTTON;
	d->items[a].gid = 0;
	d->items[a].fn = dlg_http_header_options;
	d->items[a].text = TEXT_(T_HEADER_OPTIONS);
	d->items[a].data = (void *)&http_options.header;
	d->items[a].dlen = sizeof(struct http_header_options);
	a++;
	d->items[a].type = D_BUTTON;
	d->items[a].gid = B_ENTER;
	d->items[a].fn = ok_dialog;
	d->items[a].text = TEXT_(T_OK);
	a++;
	d->items[a].type = D_BUTTON;
	d->items[a].gid = B_ESC;
	d->items[a].fn = cancel_dialog;
	d->items[a].text = TEXT_(T_CANCEL);
	a++;
	d->items[a].type = D_END;
	a++;
	do_dialog(term, d, getml(d, NULL));
}

static unsigned char mc_str[8];
static unsigned char doc_str[4];

static void
cache_refresh(void *xxx)
{
	memory_cache_size = atoi(cast_const_char mc_str) * 1024;
	max_format_cache_entries = atoi(cast_const_char doc_str);
	shrink_memory(SH_CHECK_QUOTA);
}

static unsigned char *const cache_texts[] = {
	TEXT_(T_MEMORY_CACHE_SIZE__KB), TEXT_(T_NUMBER_OF_FORMATTED_DOCUMENTS),
	TEXT_(T_AGGRESSIVE_CACHE)
};

static void
cache_opt(struct terminal *term, void *xxx, void *yyy)
{
	struct dialog *d;
	int a;
	snprint(mc_str, 8, memory_cache_size / 1024);
	snprint(doc_str, 4, max_format_cache_entries);
	d = mem_calloc(sizeof(struct dialog) + 5 * sizeof(struct dialog_item));
	a = 0;
	d->title = TEXT_(T_CACHE_OPTIONS);
	d->fn = group_fn;
	d->udata = (void *)cache_texts;
	d->refresh = cache_refresh;
	d->items[a].type = D_FIELD;
	d->items[a].dlen = 8;
	d->items[a].data = mc_str;
	d->items[a].fn = check_number;
	d->items[a].gid = 0;
	d->items[a].gnum = INT_MAX / 1024;
	a++;
	d->items[a].type = D_FIELD;
	d->items[a].dlen = 4;
	d->items[a].data = doc_str;
	d->items[a].fn = check_number;
	d->items[a].gid = 0;
	d->items[a].gnum = 999;
	a++;
	d->items[a].type = D_CHECKBOX;
	d->items[a].gid = 0;
	d->items[a].dlen = sizeof(int);
	d->items[a].data = (void *)&aggressive_cache;
	a++;
	d->items[a].type = D_BUTTON;
	d->items[a].gid = B_ENTER;
	d->items[a].fn = ok_dialog;
	d->items[a].text = TEXT_(T_OK);
	a++;
	d->items[a].type = D_BUTTON;
	d->items[a].gid = B_ESC;
	d->items[a].fn = cancel_dialog;
	d->items[a].text = TEXT_(T_CANCEL);
	a++;
	d->items[a].type = D_END;
	do_dialog(term, d, getml(d, NULL));
}

static void
menu_kill_background_connections(struct terminal *term, void *xxx, void *yyy)
{
	abort_background_connections();
}

static void
menu_kill_all_connections(struct terminal *term, void *xxx, void *yyy)
{
	abort_all_connections();
}

static void
menu_save_html_options(struct terminal *term, void *xxx, void *ses_)
{
	struct session *ses = (struct session *)ses_;
	memcpy(&dds, &ses->ds, sizeof(struct document_setup));
	write_html_config(term);
}

static unsigned char marg_str[2];

static void
session_refresh(struct session *ses)
{
	html_interpret_recursive(ses->screen);
	draw_formatted(ses);
}

static void
html_refresh(void *ses_)
{
	struct session *ses = (struct session *)ses_;
	ses->ds.margin = atoi(cast_const_char marg_str);
	session_refresh(ses);
}

static unsigned char *const html_texts[] = {
	TEXT_(T_DISPLAY_TABLES),
	TEXT_(T_DISPLAY_FRAMES),
	TEXT_(T_BREAK_LONG_LINES),
	TEXT_(T_DISPLAY_LINKS_TO_IMAGES),
	TEXT_(T_DISPLAY_IMAGE_FILENAMES),
	TEXT_(T_LINK_ORDER_BY_COLUMNS),
	TEXT_(T_NUMBERED_LINKS),
	TEXT_(T_AUTO_REFRESH),
	TEXT_(T_TARGET_IN_NEW_WINDOW),
	TEXT_(T_TEXT_MARGIN),
	cast_uchar "",
	TEXT_(T_IGNORE_CHARSET_INFO_SENT_BY_SERVER)
};

static int
dlg_assume_cp(struct dialog_data *dlg, struct dialog_item_data *di)
{
	return 0;
}

void
dialog_html_options(struct session *ses)
{
	struct dialog *d;
	int a;

	snprint(marg_str, 2, ses->ds.margin);
	d = mem_calloc(sizeof(struct dialog) + 14 * sizeof(struct dialog_item));
	d->title = TEXT_(T_HTML_OPTIONS);
	d->fn = group_fn;
	d->udata = (void *)html_texts;
	d->udata2 = ses;
	d->refresh = html_refresh;
	d->refresh_data = ses;
	a = 0;
	d->items[a].type = D_CHECKBOX;
	d->items[a].data = (unsigned char *)&ses->ds.tables;
	d->items[a].dlen = sizeof(int);
	a++;
	d->items[a].type = D_CHECKBOX;
	d->items[a].data = (unsigned char *)&ses->ds.frames;
	d->items[a].dlen = sizeof(int);
	a++;
	d->items[a].type = D_CHECKBOX;
	d->items[a].data = (unsigned char *)&ses->ds.break_long_lines;
	d->items[a].dlen = sizeof(int);
	a++;
	d->items[a].type = D_CHECKBOX;
	d->items[a].data = (unsigned char *)&ses->ds.images;
	d->items[a].dlen = sizeof(int);
	a++;
	d->items[a].type = D_CHECKBOX;
	d->items[a].data = (unsigned char *)&ses->ds.image_names;
	d->items[a].dlen = sizeof(int);
	a++;
	d->items[a].type = D_CHECKBOX;
	d->items[a].data = (unsigned char *)&ses->ds.table_order;
	d->items[a].dlen = sizeof(int);
	a++;
	d->items[a].type = D_CHECKBOX;
	d->items[a].data = (unsigned char *)&ses->ds.num_links;
	d->items[a].dlen = sizeof(int);
	a++;
	d->items[a].type = D_CHECKBOX;
	d->items[a].data = (unsigned char *)&ses->ds.auto_refresh;
	d->items[a].dlen = sizeof(int);
	a++;
	d->items[a].type = D_CHECKBOX;
	d->items[a].data = (unsigned char *)&ses->ds.target_in_new_window;
	d->items[a].dlen = sizeof(int);
	a++;
	d->items[a].type = D_FIELD;
	d->items[a].dlen = 2;
	d->items[a].data = marg_str;
	d->items[a].fn = check_number;
	d->items[a].gid = 0;
	d->items[a].gnum = 9;
	a++;
	d->items[a].type = D_BUTTON;
	d->items[a].gid = 0;
	d->items[a].fn = dlg_assume_cp;
	d->items[a].text = TEXT_(T_DEFAULT_CODEPAGE);
	d->items[a].data = (unsigned char *)&ses->ds.assume_cp;
	d->items[a].dlen = sizeof(int);
	a++;
	d->items[a].type = D_CHECKBOX;
	d->items[a].data = (unsigned char *)&ses->ds.hard_assume;
	d->items[a].dlen = sizeof(int);
	a++;
	d->items[a].type = D_BUTTON;
	d->items[a].gid = B_ENTER;
	d->items[a].fn = ok_dialog;
	d->items[a].text = TEXT_(T_OK);
	a++;
	d->items[a].type = D_BUTTON;
	d->items[a].gid = B_ESC;
	d->items[a].fn = cancel_dialog;
	d->items[a].text = TEXT_(T_CANCEL);
	a++;
	d->items[a].type = D_END;
	do_dialog(ses->term, d, getml(d, NULL));
}

static void
menu_html_options(struct terminal *term, void *xxx, void *ses_)
{
	struct session *ses = (struct session *)ses_;
	dialog_html_options(ses);
}

static unsigned char *const color_texts[] = { cast_uchar "", cast_uchar "",
	                                      cast_uchar "",
	                                      TEXT_(T_IGNORE_DOCUMENT_COLOR) };

static void
html_color_refresh(void *ses_)
{
	struct session *ses = (struct session *)ses_;
	html_interpret_recursive(ses->screen);
	draw_formatted(ses);
}

static void
select_color(struct terminal *term, int n, int *ptr)
{
	int i;
	struct menu_item *mi;
	mi = new_menu(MENU_FREE_ITEMS);
	for (i = 0; i < n; i++) {
		add_to_menu(&mi, TEXT_(T_COLOR_0 + i), cast_uchar "",
		            cast_uchar "", set_val, (void *)(unsigned long)i, 0,
		            i);
	}
	do_menu_selected(term, mi, ptr, *ptr, NULL, NULL);
}

static int
select_color_8(struct dialog_data *dlg, struct dialog_item_data *di)
{
	select_color(dlg->win->term, 8, (int *)di->cdata);
	return 0;
}

static int
select_color_16(struct dialog_data *dlg, struct dialog_item_data *di)
{
	select_color(dlg->win->term, 16, (int *)di->cdata);
	return 0;
}

static void
menu_color(struct terminal *term, void *xxx, void *ses_)
{
	struct session *ses = (struct session *)ses_;
	struct dialog *d;

	d = mem_calloc(sizeof(struct dialog) + 6 * sizeof(struct dialog_item));
	d->title = TEXT_(T_COLOR);
	d->fn = group_fn;
	d->udata = (void *)color_texts;
	d->udata2 = ses;
	d->refresh = html_color_refresh;
	d->refresh_data = ses;

	d->items[0].type = D_BUTTON;
	d->items[0].gid = 0;
	d->items[0].text = TEXT_(T_TEXT_COLOR);
	d->items[0].fn = select_color_16;
	d->items[0].data = (unsigned char *)&ses->ds.t_text_color;
	d->items[0].dlen = sizeof(int);

	d->items[1].type = D_BUTTON;
	d->items[1].gid = 0;
	d->items[1].text = TEXT_(T_LINK_COLOR);
	d->items[1].fn = select_color_16;
	d->items[1].data = (unsigned char *)&ses->ds.t_link_color;
	d->items[1].dlen = sizeof(int);

	d->items[2].type = D_BUTTON;
	d->items[2].gid = 0;
	d->items[2].text = TEXT_(T_BACKGROUND_COLOR);
	d->items[2].fn = select_color_8;
	d->items[2].data = (unsigned char *)&ses->ds.t_background_color;
	d->items[2].dlen = sizeof(int);

	d->items[3].type = D_CHECKBOX;
	d->items[3].data = (unsigned char *)(&ses->ds.t_ignore_document_color);
	d->items[3].dlen = sizeof(int);

	d->items[4].type = D_BUTTON;
	d->items[4].gid = B_ENTER;
	d->items[4].fn = ok_dialog;
	d->items[4].text = TEXT_(T_OK);

	d->items[5].type = D_BUTTON;
	d->items[5].gid = B_ESC;
	d->items[5].fn = cancel_dialog;
	d->items[5].text = TEXT_(T_CANCEL);

	d->items[6].type = D_END;

	do_dialog(term, d, getml(d, NULL));
}

static unsigned char new_bookmarks_file[MAX_STR_LEN];
static int new_bookmarks_codepage = 0;

static void
refresh_misc(void *ses_)
{
	struct session *ses = (struct session *)ses_;
	if (strcmp((char *)new_bookmarks_file, (char *)bookmarks_file))
		reinit_bookmarks(ses, new_bookmarks_file);
}

static unsigned char *const miscopt_labels[] = { TEXT_(T_BOOKMARKS_FILE),
	                                         NULL };
static unsigned char *const miscopt_checkbox_labels[] = {
	TEXT_(T_SAVE_URL_HISTORY_ON_EXIT), NULL
};

static void
miscopt_fn(struct dialog_data *dlg)
{
	struct terminal *term = dlg->win->term;
	unsigned char **labels = dlg->dlg->udata;
	int max = 0, min = 0;
	int w, rw;
	int y = 0;
	int a = 0;
	int bmk = !anonymous;

	max_text_width(term, labels[0], &max, AL_LEFT);
	min_text_width(term, labels[0], &min, AL_LEFT);
	if (bmk) {
		max_buttons_width(term, dlg->items + dlg->n - 3 - a - bmk, 1,
		                  &max);
		min_buttons_width(term, dlg->items + dlg->n - 3 - a - bmk, 1,
		                  &min);
	}
	if (a) {
		max_buttons_width(term, dlg->items + dlg->n - 3 - bmk, 1, &max);
		min_buttons_width(term, dlg->items + dlg->n - 3 - bmk, 1, &min);
	}
	if (bmk) {
		checkboxes_width(term, miscopt_checkbox_labels, 1, &max,
		                 max_text_width);
		checkboxes_width(term, miscopt_checkbox_labels, 1, &min,
		                 min_text_width);
	}
	max_buttons_width(term, dlg->items + dlg->n - 2, 2, &max);
	min_buttons_width(term, dlg->items + dlg->n - 2, 2, &min);
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

	if (bmk) {
		dlg_format_text_and_field(
		    dlg, NULL, labels[0], dlg->items + dlg->n - 4 - a - bmk, 0,
		    &y, w, &rw, COLOR_DIALOG_TEXT, AL_LEFT);
		y++;
	}
	if (bmk) {
		y++;
		dlg_format_buttons(dlg, NULL, dlg->items + dlg->n - 3 - a - bmk,
		                   1, 0, &y, w, &rw, AL_LEFT);
	}
	if (a)
		dlg_format_buttons(dlg, NULL, dlg->items + dlg->n - 3 - bmk, 1,
		                   0, &y, w, &rw, AL_LEFT);
	if (bmk)
		dlg_format_checkboxes(dlg, NULL, dlg->items + dlg->n - 3, 1, 0,
		                      &y, w, &rw, miscopt_checkbox_labels);
	dlg_format_buttons(dlg, NULL, dlg->items + dlg->n - 2, 2, 0, &y, w, &rw,
	                   AL_CENTER);
	w = rw;
	dlg->xw = w + 2 * DIALOG_LB;
	dlg->yw = y + 2 * DIALOG_TB;
	center_dlg(dlg);
	draw_dlg(dlg);
	y = dlg->y + DIALOG_TB;
	y++;
	if (bmk) {
		dlg_format_text_and_field(dlg, term, labels[0],
		                          dlg->items + dlg->n - 4 - a - bmk,
		                          dlg->x + DIALOG_LB, &y, w, NULL,
		                          COLOR_DIALOG_TEXT, AL_LEFT);
		y++;
		dlg_format_buttons(dlg, term, dlg->items + dlg->n - 3 - a - bmk,
		                   1, dlg->x + DIALOG_LB, &y, w, NULL,
		                   AL_CENTER);
	}
	if (a)
		dlg_format_buttons(dlg, term, dlg->items + dlg->n - 3 - bmk, 1,
		                   dlg->x + DIALOG_LB, &y, w, NULL, AL_CENTER);
	if (bmk) {
		dlg_format_checkboxes(dlg, term, dlg->items + dlg->n - 3, 1,
		                      dlg->x + DIALOG_LB, &y, w, NULL,
		                      miscopt_checkbox_labels);
		y++;
	}
	dlg_format_buttons(dlg, term, dlg->items + dlg->n - 2, 2,
	                   dlg->x + DIALOG_LB, &y, w, NULL, AL_CENTER);
}

static void
miscelaneous_options(struct terminal *term, void *xxx, void *ses_)
{
	struct session *ses = (struct session *)ses_;
	struct dialog *d;
	int a = 0;

	/* if you add something into text mode (or both text and graphics),
	 * remove this (and enable also miscelaneous_options in do_setup_menu)
	 */
	if (anonymous)
		return;

	safe_strncpy(new_bookmarks_file, bookmarks_file, MAX_STR_LEN);
	d = mem_calloc(sizeof(struct dialog) + 5 * sizeof(struct dialog_item));
	d->title = TEXT_(T_MISCELANEOUS_OPTIONS);
	d->refresh = refresh_misc;
	d->refresh_data = ses;
	d->fn = miscopt_fn;
	d->udata = (void *)miscopt_labels;
	d->udata2 = ses;
	if (!anonymous) {
		d->items[a].type = D_FIELD;
		d->items[a].dlen = MAX_STR_LEN;
		d->items[a].data = new_bookmarks_file;
		a++;
		d->items[a].type = D_BUTTON;
		d->items[a].gid = 0;
		d->items[a].fn = dlg_assume_cp;
		d->items[a].text = TEXT_(T_BOOKMARKS_ENCODING);
		d->items[a].data = (unsigned char *)&new_bookmarks_codepage;
		d->items[a].dlen = sizeof(int);
		a++;
	}
	if (!anonymous) {
		d->items[a].type = D_CHECKBOX;
		d->items[a].gid = 0;
		d->items[a].dlen = sizeof(int);
		d->items[a].data = (void *)&save_history;
		a++;
	}
	d->items[a].type = D_BUTTON;
	d->items[a].gid = B_ENTER;
	d->items[a].fn = ok_dialog;
	d->items[a].text = TEXT_(T_OK);
	a++;
	d->items[a].type = D_BUTTON;
	d->items[a].gid = B_ESC;
	d->items[a].fn = cancel_dialog;
	d->items[a].text = TEXT_(T_CANCEL);
	a++;
	d->items[a].type = D_END;
	do_dialog(term, d, getml(d, NULL));
}

static const struct menu_item file_menu11[] = {
	{TEXT_(T_GOTO_URL),    cast_uchar "g",      TEXT_(T_HK_GOTO_URL),
         menu_goto_url,								   NULL, 0, 1},
	{ TEXT_(T_GO_BACK),    cast_uchar "z",      TEXT_(T_HK_GO_BACK),    menu_go_back,
         NULL,										  0, 1},
	{ TEXT_(T_GO_FORWARD), cast_uchar "x",      TEXT_(T_HK_GO_FORWARD),
         menu_go_forward,								 NULL, 0, 1},
	{ TEXT_(T_HISTORY),    cast_uchar ">",      TEXT_(T_HK_HISTORY),    history_menu,
         NULL,										  1, 1},
	{ TEXT_(T_RELOAD),     cast_uchar "Ctrl-R", TEXT_(T_HK_RELOAD),     menu_reload,
         NULL,										  0, 1},
};

static const struct menu_item file_menu12[] = {
	{TEXT_(T_BOOKMARKS), cast_uchar "s", TEXT_(T_HK_BOOKMARKS),
         menu_bookmark_manager, NULL, 0, 1},
};

static const struct menu_item file_menu21[] = {
	{cast_uchar "",                     cast_uchar "", M_BAR,                   NULL,                NULL, 0, 1},
	{ TEXT_(T_SAVE_AS),                 cast_uchar "", TEXT_(T_HK_SAVE_AS),     save_as,             NULL,
         0,												       1},
	{ TEXT_(T_SAVE_URL_AS),             cast_uchar "", TEXT_(T_HK_SAVE_URL_AS),
         menu_save_url_as,									       NULL, 0, 1},
	{ TEXT_(T_SAVE_FORMATTED_DOCUMENT), cast_uchar "",
         TEXT_(T_HK_SAVE_FORMATTED_DOCUMENT),                                       menu_save_formatted, NULL, 0,
         1													 },
};

static const struct menu_item file_menu22[] = {
	{cast_uchar "",			 cast_uchar "", M_BAR,                     NULL,                      NULL, 0, 1},
	{ TEXT_(T_KILL_BACKGROUND_CONNECTIONS), cast_uchar "",
         TEXT_(T_HK_KILL_BACKGROUND_CONNECTIONS),
         menu_kill_background_connections,									   NULL, 0, 1},
	{ TEXT_(T_KILL_ALL_CONNECTIONS),        cast_uchar "",
         TEXT_(T_HK_KILL_ALL_CONNECTIONS),						menu_kill_all_connections, NULL, 0,
         1														     },
	{ TEXT_(T_FLUSH_ALL_CACHES),            cast_uchar "",
         TEXT_(T_HK_FLUSH_ALL_CACHES),						    flush_caches,              NULL, 0, 1},
	{ TEXT_(T_RESOURCE_INFO),               cast_uchar "", TEXT_(T_HK_RESOURCE_INFO),
         resource_info_menu,											 NULL, 0, 1},
	{ cast_uchar "",                        cast_uchar "", M_BAR,                     NULL,                      NULL, 0, 1},
};

static const struct menu_item file_menu3[] = {
	{cast_uchar "",  cast_uchar "",  M_BAR,            NULL,      NULL, 0, 1},
	{ TEXT_(T_EXIT), cast_uchar "q", TEXT_(T_HK_EXIT), exit_prog, NULL, 0,
         1								      },
	{ NULL,          NULL,           0,                NULL,      NULL, 0, 0}
};

static void
do_file_menu(struct terminal *term, void *xxx, void *ses_)
{
	struct session *ses = (struct session *)ses_;
	int x;
	int o;
	struct menu_item *file_menu, *e;
	file_menu =
	    xmalloc(sizeof(file_menu11) + sizeof(file_menu12)
	            + sizeof(file_menu21) + sizeof(file_menu22)
	            + sizeof(file_menu3) + 3 * sizeof(struct menu_item));
	e = file_menu;
	memcpy(e, file_menu11, sizeof(file_menu11));
	e += sizeof(file_menu11) / sizeof(struct menu_item);
	if (!anonymous) {
		memcpy(e, file_menu12, sizeof(file_menu12));
		e += sizeof(file_menu12) / sizeof(struct menu_item);
	}
	if ((o = can_open_in_new(term))) {
		e->text = TEXT_(T_NEW_WINDOW);
		e->rtext = o - 1 ? cast_uchar ">" : cast_uchar "";
		e->hotkey = TEXT_(T_HK_NEW_WINDOW);
		e->func = open_in_new_window;
		e->data = (void *)&send_open_new_xterm_ptr;
		e->in_m = o - 1;
		e->free_i = 0;
		e++;
	}
	if (!anonymous) {
		memcpy(e, file_menu21, sizeof(file_menu21));
		e += sizeof(file_menu21) / sizeof(struct menu_item);
	}
	memcpy(e, file_menu22, sizeof(file_menu22));
	e += sizeof(file_menu22) / sizeof(struct menu_item);
	x = 1;
	memcpy(e, file_menu3 + x,
	       sizeof(file_menu3) - x * sizeof(struct menu_item));
	do_menu(term, file_menu, ses);
}

static void (*const search_dlg_ptr)(struct session *ses, struct f_data_c *f,
                                    int a) = search_dlg;
static void (*const search_back_dlg_ptr)(struct session *ses,
                                         struct f_data_c *f,
                                         int a) = search_back_dlg;
static void (*const find_next_ptr)(struct session *ses, struct f_data_c *f,
                                   int a) = find_next;
static void (*const find_next_back_ptr)(struct session *ses, struct f_data_c *f,
                                        int a) = find_next_back;
static void (*const set_frame_ptr)(struct session *ses, struct f_data_c *f,
                                   int a) = set_frame;

static const struct menu_item view_menu[] = {
	{TEXT_(T_SEARCH),                cast_uchar "/",  TEXT_(T_HK_SEARCH),        menu_for_frame,
         (void *)&search_dlg_ptr,													  0, 0},
	{ TEXT_(T_SEARCH_BACK),          cast_uchar "?",  TEXT_(T_HK_SEARCH_BACK),
         menu_for_frame,										     (void *)&search_back_dlg_ptr, 0, 0},
	{ TEXT_(T_FIND_NEXT),            cast_uchar "n",  TEXT_(T_HK_FIND_NEXT),
         menu_for_frame,										     (void *)&find_next_ptr,       0, 0},
	{ TEXT_(T_FIND_PREVIOUS),        cast_uchar "N",  TEXT_(T_HK_FIND_PREVIOUS),
         menu_for_frame,										     (void *)&find_next_back_ptr,  0, 0},
	{ cast_uchar "",                 cast_uchar "",   M_BAR,                     NULL,                   NULL,                         0, 0},
	{ TEXT_(T_TOGGLE_HTML_PLAIN),    cast_uchar "\\",
         TEXT_(T_HK_TOGGLE_HTML_PLAIN),					      menu_toggle,            NULL,                         0, 0},
	{ TEXT_(T_DOCUMENT_INFO),        cast_uchar "=",  TEXT_(T_HK_DOCUMENT_INFO),
         menu_doc_info,										      NULL,                         0, 0},
	{ TEXT_(T_HEADER_INFO),          cast_uchar "|",  TEXT_(T_HK_HEADER_INFO),
         menu_head_info,										     NULL,                         0, 0},
	{ TEXT_(T_FRAME_AT_FULL_SCREEN), cast_uchar "f",
         TEXT_(T_HK_FRAME_AT_FULL_SCREEN),                                           menu_for_frame,
         (void *)&set_frame_ptr,													   0, 0},
	{ cast_uchar "",                 cast_uchar "",   M_BAR,                     NULL,                   NULL,                         0, 0},
	{ TEXT_(T_HTML_OPTIONS),         cast_uchar "",   TEXT_(T_HK_HTML_OPTIONS),
         menu_html_options,										  NULL,                         0, 0},
	{ TEXT_(T_SAVE_HTML_OPTIONS),    cast_uchar "",
         TEXT_(T_HK_SAVE_HTML_OPTIONS),					      menu_save_html_options, NULL,                         0, 0},
	{ NULL,			  NULL,            0,			 NULL,                   NULL,                         0, 0}
};

static const struct menu_item view_menu_anon[] = {
	{TEXT_(T_SEARCH),                cast_uchar "/",  TEXT_(T_HK_SEARCH),        menu_for_frame,
         (void *)&search_dlg_ptr,												  0, 0},
	{ TEXT_(T_SEARCH_BACK),          cast_uchar "?",  TEXT_(T_HK_SEARCH_BACK),
         menu_for_frame,									     (void *)&search_back_dlg_ptr, 0, 0},
	{ TEXT_(T_FIND_NEXT),            cast_uchar "n",  TEXT_(T_HK_FIND_NEXT),
         menu_for_frame,									     (void *)&find_next_ptr,       0, 0},
	{ TEXT_(T_FIND_PREVIOUS),        cast_uchar "N",  TEXT_(T_HK_FIND_PREVIOUS),
         menu_for_frame,									     (void *)&find_next_back_ptr,  0, 0},
	{ cast_uchar "",                 cast_uchar "",   M_BAR,                     NULL,           NULL,                         0, 0},
	{ TEXT_(T_TOGGLE_HTML_PLAIN),    cast_uchar "\\",
         TEXT_(T_HK_TOGGLE_HTML_PLAIN),					      menu_toggle,    NULL,                         0, 0},
	{ TEXT_(T_DOCUMENT_INFO),        cast_uchar "=",  TEXT_(T_HK_DOCUMENT_INFO),
         menu_doc_info,									      NULL,                         0, 0},
	{ TEXT_(T_FRAME_AT_FULL_SCREEN), cast_uchar "f",
         TEXT_(T_HK_FRAME_AT_FULL_SCREEN),                                           menu_for_frame,
         (void *)&set_frame_ptr,												   0, 0},
	{ cast_uchar "",                 cast_uchar "",   M_BAR,                     NULL,           NULL,                         0, 0},
	{ TEXT_(T_HTML_OPTIONS),         cast_uchar "",   TEXT_(T_HK_HTML_OPTIONS),
         menu_html_options,									  NULL,                         0, 0},
	{ NULL,			  NULL,            0,			 NULL,           NULL,                         0, 0}
};

static const struct menu_item view_menu_color[] = {
	{TEXT_(T_SEARCH),                cast_uchar "/",  TEXT_(T_HK_SEARCH),        menu_for_frame,
         (void *)&search_dlg_ptr,													  0, 0},
	{ TEXT_(T_SEARCH_BACK),          cast_uchar "?",  TEXT_(T_HK_SEARCH_BACK),
         menu_for_frame,										     (void *)&search_back_dlg_ptr, 0, 0},
	{ TEXT_(T_FIND_NEXT),            cast_uchar "n",  TEXT_(T_HK_FIND_NEXT),
         menu_for_frame,										     (void *)&find_next_ptr,       0, 0},
	{ TEXT_(T_FIND_PREVIOUS),        cast_uchar "N",  TEXT_(T_HK_FIND_PREVIOUS),
         menu_for_frame,										     (void *)&find_next_back_ptr,  0, 0},
	{ cast_uchar "",                 cast_uchar "",   M_BAR,                     NULL,                   NULL,                         0, 0},
	{ TEXT_(T_TOGGLE_HTML_PLAIN),    cast_uchar "\\",
         TEXT_(T_HK_TOGGLE_HTML_PLAIN),					      menu_toggle,            NULL,                         0, 0},
	{ TEXT_(T_DOCUMENT_INFO),        cast_uchar "=",  TEXT_(T_HK_DOCUMENT_INFO),
         menu_doc_info,										      NULL,                         0, 0},
	{ TEXT_(T_HEADER_INFO),          cast_uchar "|",  TEXT_(T_HK_HEADER_INFO),
         menu_head_info,										     NULL,                         0, 0},
	{ TEXT_(T_FRAME_AT_FULL_SCREEN), cast_uchar "f",
         TEXT_(T_HK_FRAME_AT_FULL_SCREEN),                                           menu_for_frame,
         (void *)&set_frame_ptr,													   0, 0},
	{ cast_uchar "",                 cast_uchar "",   M_BAR,                     NULL,                   NULL,                         0, 0},
	{ TEXT_(T_HTML_OPTIONS),         cast_uchar "",   TEXT_(T_HK_HTML_OPTIONS),
         menu_html_options,										  NULL,                         0, 0},
	{ TEXT_(T_COLOR),                cast_uchar "",   TEXT_(T_HK_COLOR),         menu_color,             NULL,                         0,
         0																     },
	{ TEXT_(T_SAVE_HTML_OPTIONS),    cast_uchar "",
         TEXT_(T_HK_SAVE_HTML_OPTIONS),					      menu_save_html_options, NULL,                         0, 0},
	{ NULL,			  NULL,            0,			 NULL,                   NULL,                         0, 0}
};

static const struct menu_item view_menu_anon_color[] = {
	{TEXT_(T_SEARCH),                cast_uchar "/",  TEXT_(T_HK_SEARCH),        menu_for_frame,
         (void *)&search_dlg_ptr,												  0, 0},
	{ TEXT_(T_SEARCH_BACK),          cast_uchar "?",  TEXT_(T_HK_SEARCH_BACK),
         menu_for_frame,									     (void *)&search_back_dlg_ptr, 0, 0},
	{ TEXT_(T_FIND_NEXT),            cast_uchar "n",  TEXT_(T_HK_FIND_NEXT),
         menu_for_frame,									     (void *)&find_next_ptr,       0, 0},
	{ TEXT_(T_FIND_PREVIOUS),        cast_uchar "N",  TEXT_(T_HK_FIND_PREVIOUS),
         menu_for_frame,									     (void *)&find_next_back_ptr,  0, 0},
	{ cast_uchar "",                 cast_uchar "",   M_BAR,                     NULL,           NULL,                         0, 0},
	{ TEXT_(T_TOGGLE_HTML_PLAIN),    cast_uchar "\\",
         TEXT_(T_HK_TOGGLE_HTML_PLAIN),					      menu_toggle,    NULL,                         0, 0},
	{ TEXT_(T_DOCUMENT_INFO),        cast_uchar "=",  TEXT_(T_HK_DOCUMENT_INFO),
         menu_doc_info,									      NULL,                         0, 0},
	{ TEXT_(T_FRAME_AT_FULL_SCREEN), cast_uchar "f",
         TEXT_(T_HK_FRAME_AT_FULL_SCREEN),                                           menu_for_frame,
         (void *)&set_frame_ptr,												   0, 0},
	{ cast_uchar "",                 cast_uchar "",   M_BAR,                     NULL,           NULL,                         0, 0},
	{ TEXT_(T_HTML_OPTIONS),         cast_uchar "",   TEXT_(T_HK_HTML_OPTIONS),
         menu_html_options,									  NULL,                         0, 0},
	{ TEXT_(T_COLOR),                cast_uchar "",   TEXT_(T_HK_COLOR),         menu_color,     NULL,                         0,
         0															     },
	{ NULL,			  NULL,            0,			 NULL,           NULL,                         0, 0}
};

static void
do_view_menu(struct terminal *term, void *xxx, void *ses_)
{
	struct session *ses = (struct session *)ses_;
	if (term->spec->col) {
		if (!anonymous)
			do_menu(term, (struct menu_item *)view_menu_color, ses);
		else
			do_menu(term, (struct menu_item *)view_menu_anon_color,
			        ses);
	} else {
		if (!anonymous)
			do_menu(term, (struct menu_item *)view_menu, ses);
		else
			do_menu(term, (struct menu_item *)view_menu_anon, ses);
	}
}

static const struct menu_item help_menu[] = {
	{TEXT_(T_ABOUT),     cast_uchar "",   TEXT_(T_HK_ABOUT),    menu_about,   NULL, 0,
         0										  },
	{ TEXT_(T_KEYS),     cast_uchar "F1", TEXT_(T_HK_KEYS),     menu_keys,    NULL, 0,
         0										  },
	{ TEXT_(T_MANUAL),   cast_uchar "",   TEXT_(T_HK_MANUAL),   menu_url,
         TEXT_(T_URL_MANUAL),							   0, 0},
	{ TEXT_(T_HOMEPAGE), cast_uchar "",   TEXT_(T_HK_HOMEPAGE), menu_url,
         TEXT_(T_URL_HOMEPAGE),							 0, 0},
	{ TEXT_(T_COPYING),  cast_uchar "",   TEXT_(T_HK_COPYING),  menu_copying,
         NULL,									  0, 0},
	{ NULL,              NULL,            0,                    NULL,         NULL, 0, 0}
};

static const struct menu_item net_options_menu[] = {
	{TEXT_(T_CONNECTIONS),   cast_uchar "", TEXT_(T_HK_CONNECTIONS),
         dlg_net_options,						       NULL, 0, 0},
	{ TEXT_(T_PROXIES),      cast_uchar "", TEXT_(T_HK_PROXIES),
         dlg_proxy_options,						     NULL, 0, 0},
	{ TEXT_(T_SSL_OPTIONS),  cast_uchar "", TEXT_(T_HK_SSL_OPTIONS),
         dlg_ssl_options,						       NULL, 0, 0},
	{ TEXT_(T_HTTP_OPTIONS), cast_uchar "", TEXT_(T_HK_HTTP_OPTIONS),
         dlg_http_options,						      NULL, 0, 0},
	{ NULL,		  NULL,          0,                        NULL, NULL, 0, 0}
};

static const struct menu_item net_options_ipv6_menu[] = {
	{TEXT_(T_CONNECTIONS),   cast_uchar "", TEXT_(T_HK_CONNECTIONS),
         dlg_net_options,						       NULL, 0, 0},
	{ TEXT_(T_IPV6_OPTIONS), cast_uchar "", TEXT_(T_HK_IPV6_OPTIONS),
         dlg_ipv6_options,						      NULL, 0, 0},
	{ TEXT_(T_PROXIES),      cast_uchar "", TEXT_(T_HK_PROXIES),
         dlg_proxy_options,						     NULL, 0, 0},
	{ TEXT_(T_SSL_OPTIONS),  cast_uchar "", TEXT_(T_HK_SSL_OPTIONS),
         dlg_ssl_options,						       NULL, 0, 0},
	{ TEXT_(T_HTTP_OPTIONS), cast_uchar "", TEXT_(T_HK_HTTP_OPTIONS),
         dlg_http_options,						      NULL, 0, 0},
	{ NULL,		  NULL,          0,                        NULL, NULL, 0, 0}
};

static void
network_menu(struct terminal *term, void *xxx, void *yyy)
{
	if (support_ipv6)
		do_menu(term, (struct menu_item *)net_options_ipv6_menu, NULL);
	else
		do_menu(term, (struct menu_item *)net_options_menu, NULL);
}

static void
menu_write_config(struct terminal *term, void *xxx, void *yyy)
{
	write_config(term);
}

static const struct menu_item setup_menu_2[] = {
	{TEXT_(T_TERMINAL_OPTIONS), cast_uchar "",
         TEXT_(T_HK_TERMINAL_OPTIONS), terminal_options, NULL, 0, 1},
};

static const struct menu_item setup_menu_5[] = {
	{TEXT_(T_NETWORK_OPTIONS), cast_uchar ">", TEXT_(T_HK_NETWORK_OPTIONS),
         network_menu, NULL, 1, 1},
};

static const struct menu_item setup_menu_6[] = {
	{TEXT_(T_MISCELANEOUS_OPTIONS), cast_uchar "",
         TEXT_(T_HK_MISCELANEOUS_OPTIONS), miscelaneous_options, NULL, 0, 1},
};

static const struct menu_item setup_menu_7[] = {
	{TEXT_(T_CACHE),            cast_uchar "", TEXT_(T_HK_CACHE),           cache_opt, NULL, 0,
         1											   },
	{ TEXT_(T_ASSOCIATIONS),    cast_uchar "", TEXT_(T_HK_ASSOCIATIONS),
         menu_assoc_manager,							       NULL, 0, 1},
	{ TEXT_(T_FILE_EXTENSIONS), cast_uchar "", TEXT_(T_HK_FILE_EXTENSIONS),
         menu_ext_manager,								 NULL, 0, 1},
	{ cast_uchar "",            cast_uchar "", M_BAR,                       NULL,      NULL, 0, 1},
	{ TEXT_(T_SAVE_OPTIONS),    cast_uchar "", TEXT_(T_HK_SAVE_OPTIONS),
         menu_write_config,								NULL, 0, 1},
};

static const struct menu_item setup_menu_8[] = {
	{NULL, NULL, 0, NULL, NULL, 0, 0}
};

static void
do_setup_menu(struct terminal *term, void *xxx, void *ses_)
{
	struct session *ses = (struct session *)ses_;
	struct menu_item *setup_menu, *e;
	int size = sizeof(setup_menu_2) + sizeof(setup_menu_5)
	           + sizeof(setup_menu_6) + sizeof(setup_menu_7)
	           + sizeof(setup_menu_8);
	setup_menu = xmalloc(size);
	e = setup_menu;
	memcpy(e, setup_menu_2, sizeof(setup_menu_2));
	e += sizeof(setup_menu_2) / sizeof(struct menu_item);
	if (!anonymous) {
		memcpy(e, setup_menu_5, sizeof(setup_menu_5));
		e += sizeof(setup_menu_5) / sizeof(struct menu_item);
	}
	if (!anonymous) {
		memcpy(e, setup_menu_6, sizeof(setup_menu_6));
		e += sizeof(setup_menu_6) / sizeof(struct menu_item);
	}
	if (!anonymous) {
		memcpy(e, setup_menu_7, sizeof(setup_menu_7));
		e += sizeof(setup_menu_7) / sizeof(struct menu_item);
	}
	memcpy(e, setup_menu_8, sizeof(setup_menu_8));
	do_menu(term, setup_menu, ses);
}

static void
do_help_menu(struct terminal *term, void *xxx, void *ses_)
{
	struct session *ses = (struct session *)ses_;
	do_menu(term, (struct menu_item *)help_menu, ses);
}

static const struct menu_item main_menu[] = {
	{TEXT_(T_FILE),       cast_uchar "", TEXT_(T_HK_FILE),      do_file_menu,  NULL, 1,
         1										   },
	{ TEXT_(T_VIEW),      cast_uchar "", TEXT_(T_HK_VIEW),      do_view_menu,  NULL, 1,
         1										   },
	{ TEXT_(T_LINK),      cast_uchar "", TEXT_(T_HK_LINK),      link_menu,     NULL, 1,
         1										   },
	{ TEXT_(T_DOWNLOADS), cast_uchar "", TEXT_(T_HK_DOWNLOADS),
         downloads_menu,							   NULL, 1, 1},
	{ TEXT_(T_SETUP),     cast_uchar "", TEXT_(T_HK_SETUP),     do_setup_menu, NULL,
         1,										 1},
	{ TEXT_(T_HELP),      cast_uchar "", TEXT_(T_HK_HELP),      do_help_menu,  NULL, 1,
         1										   },
	{ NULL,               NULL,          0,                     NULL,          NULL, 0, 0}
};

/* lame technology rulez ! */

void
activate_bfu_technology(struct session *ses, int item)
{
	struct terminal *term = ses->term;
	struct menu_item *m = (struct menu_item *)main_menu;
	do_mainmenu(term, m, ses, item);
}

struct history goto_url_history = {
	0, {&goto_url_history.items, &goto_url_history.items}
};

void
dialog_goto_url(struct session *ses, unsigned char *url)
{
	input_field(ses->term, NULL, TEXT_(T_GOTO_URL), TEXT_(T_ENTER_URL), ses,
	            &goto_url_history, MAX_INPUT_URL_LEN, url, 0, 0, NULL, 2,
	            TEXT_(T_OK), goto_url, TEXT_(T_CANCEL), input_field_null);
}

void
dialog_save_url(struct session *ses)
{
	input_field(ses->term, NULL, TEXT_(T_SAVE_URL), TEXT_(T_ENTER_URL), ses,
	            &goto_url_history, MAX_INPUT_URL_LEN, cast_uchar "", 0, 0,
	            NULL, 2, TEXT_(T_OK), save_url, TEXT_(T_CANCEL),
	            input_field_null);
}

struct does_file_exist_s {
	void (*fn)(struct session *, unsigned char *, int);
	void (*cancel)(void *);
	int flags;
	struct session *ses;
	unsigned char *file;
	unsigned char *url;
	unsigned char *head;
};

static void
does_file_exist_ok(struct does_file_exist_s *h, int mode)
{
	if (h->fn) {
		unsigned char *d = h->file;
		unsigned char *dd;
		for (dd = h->file; *dd; dd++)
			if (dir_sep(*dd))
				d = dd + 1;
		if (d - h->file < MAX_STR_LEN) {
			memcpy(download_dir, h->file, d - h->file);
			download_dir[d - h->file] = 0;
		}
		h->fn(h->ses, h->file, mode);
	}
}

static void
does_file_exist_continue(void *data)
{
	does_file_exist_ok(data, DOWNLOAD_CONTINUE);
}

static void
does_file_exist_overwrite(void *data)
{
	does_file_exist_ok(data, DOWNLOAD_OVERWRITE);
}

static void
does_file_exist_cancel(void *data)
{
	struct does_file_exist_s *h = (struct does_file_exist_s *)data;
	if (h->cancel)
		h->cancel(h->ses);
}

static void
does_file_exist_rename(void *data)
{
	struct does_file_exist_s *h = (struct does_file_exist_s *)data;
	query_file(h->ses, h->url, h->head, h->fn, h->cancel, h->flags);
}

static void
does_file_exist(void *d_, unsigned char *file)
{
	struct does_file_exist_s *d = (struct does_file_exist_s *)d_;
	unsigned char *f;
	unsigned char *wd;
	struct session *ses = d->ses;
	struct stat st;
	int r;
	struct does_file_exist_s *h;
	unsigned char *msg;
	int file_type = 0;

	h = xmalloc(sizeof(struct does_file_exist_s));
	h->fn = d->fn;
	h->cancel = d->cancel;
	h->flags = d->flags;
	h->ses = ses;
	h->file = stracpy(file);
	h->url = stracpy(d->url);
	h->head = stracpy(d->head);

	if (!*file) {
		does_file_exist_rename(h);
		goto free_h_ret;
	}

	if (test_abort_downloads_to_file(file, ses->term->cwd, 0)) {
		msg = TEXT_(T_ALREADY_EXISTS_AS_DOWNLOAD);
		goto display_msgbox;
	}

	wd = get_cwd();
	set_cwd(ses->term->cwd);
	f = translate_download_file(file);
	EINTRLOOP(r, stat(cast_const_char f, &st));
	free(f);
	if (wd) {
		set_cwd(wd);
		free(wd);
	}
	if (r) {
		does_file_exist_ok(h, DOWNLOAD_DEFAULT);
free_h_ret:
		free(h->head);
		free(h->file);
		free(h->url);
		free(h);
		return;
	}

	if (!S_ISREG(st.st_mode)) {
		if (S_ISDIR(st.st_mode))
			file_type = 2;
		else
			file_type = 1;
	}

	msg = TEXT_(T_ALREADY_EXISTS);
display_msgbox:
	if (file_type == 2) {
		msg_box(ses->term, getml(h, h->file, h->url, h->head, NULL),
		        TEXT_(T_FILE_ALREADY_EXISTS), AL_CENTER,
		        TEXT_(T_DIRECTORY), cast_uchar " ", h->file,
		        cast_uchar " ", TEXT_(T_ALREADY_EXISTS), MSG_BOX_END,
		        (void *)h, 2, TEXT_(T_RENAME), does_file_exist_rename,
		        B_ENTER, TEXT_(T_CANCEL), does_file_exist_cancel,
		        B_ESC);
	} else if (file_type || h->flags != DOWNLOAD_CONTINUE) {
		msg_box(ses->term, getml(h, h->file, h->url, h->head, NULL),
		        TEXT_(T_FILE_ALREADY_EXISTS), AL_CENTER, TEXT_(T_FILE),
		        cast_uchar " ", h->file, cast_uchar " ", msg,
		        cast_uchar " ", TEXT_(T_DO_YOU_WISH_TO_OVERWRITE),
		        MSG_BOX_END, (void *)h, 3, TEXT_(T_OVERWRITE),
		        does_file_exist_overwrite, B_ENTER, TEXT_(T_RENAME),
		        does_file_exist_rename, 0, TEXT_(T_CANCEL),
		        does_file_exist_cancel, B_ESC);
	} else {
		msg_box(ses->term, getml(h, h->file, h->url, h->head, NULL),
		        TEXT_(T_FILE_ALREADY_EXISTS), AL_CENTER, TEXT_(T_FILE),
		        cast_uchar " ", h->file, cast_uchar " ", msg,
		        cast_uchar " ", TEXT_(T_DO_YOU_WISH_TO_CONTINUE),
		        MSG_BOX_END, (void *)h, 4, TEXT_(T_CONTINUE),
		        does_file_exist_continue, B_ENTER, TEXT_(T_OVERWRITE),
		        does_file_exist_overwrite, 0, TEXT_(T_RENAME),
		        does_file_exist_rename, 0, TEXT_(T_CANCEL),
		        does_file_exist_cancel, B_ESC);
	}
}

static void
query_file_cancel(void *d_, unsigned char *s_)
{
	struct does_file_exist_s *d = (struct does_file_exist_s *)d_;
	if (d->cancel)
		d->cancel(d->ses);
}

void
query_file(struct session *ses, unsigned char *url, unsigned char *head,
           void (*fn)(struct session *, unsigned char *, int),
           void (*cancel)(void *), int flags)
{
	unsigned char *fc, *file, *def;
	size_t dfl;
	struct does_file_exist_s *h;

	h = xmalloc(sizeof(struct does_file_exist_s));

	fc = get_filename_from_url(url, head, 0);
	file = stracpy(fc);
	free(fc);
	check_filename(&file);

	def = NULL;
	dfl = add_to_str(&def, 0, download_dir);
	if (*def && !dir_sep(def[strlen(cast_const_char def) - 1]))
		dfl = add_chr_to_str(&def, dfl, '/');
	dfl = add_to_str(&def, dfl, file);
	free(file);

	h->fn = fn;
	h->cancel = cancel;
	h->flags = flags;
	h->ses = ses;
	h->file = NULL;
	h->url = stracpy(url);
	h->head = stracpy(head);

	input_field(ses->term, getml(h, h->url, h->head, NULL),
	            TEXT_(T_DOWNLOAD), TEXT_(T_SAVE_TO_FILE), h, &file_history,
	            MAX_INPUT_URL_LEN, def, 0, 0, NULL, 2, TEXT_(T_OK),
	            does_file_exist, TEXT_(T_CANCEL), query_file_cancel);
	free(def);
}

static struct history search_history = {
	0, {&search_history.items, &search_history.items}
};

void
search_back_dlg(struct session *ses, struct f_data_c *f, int a)
{
	if (list_empty(ses->history) || !f->f_data || !f->vs) {
		msg_box(ses->term, NULL, TEXT_(T_SEARCH), AL_LEFT,
		        TEXT_(T_YOU_ARE_NOWHERE), MSG_BOX_END, NULL, 1,
		        TEXT_(T_OK), msg_box_null, B_ENTER | B_ESC);
		return;
	}
	input_field(
	    ses->term, NULL, TEXT_(T_SEARCH_BACK), TEXT_(T_SEARCH_FOR_TEXT),
	    ses, &search_history, MAX_INPUT_URL_LEN, cast_uchar "", 0, 0, NULL,
	    2, TEXT_(T_OK), search_for_back, TEXT_(T_CANCEL), input_field_null);
}

void
search_dlg(struct session *ses, struct f_data_c *f, int a)
{
	if (list_empty(ses->history) || !f->f_data || !f->vs) {
		msg_box(ses->term, NULL, TEXT_(T_SEARCH_FOR_TEXT), AL_LEFT,
		        TEXT_(T_YOU_ARE_NOWHERE), MSG_BOX_END, NULL, 1,
		        TEXT_(T_OK), msg_box_null, B_ENTER | B_ESC);
		return;
	}
	input_field(ses->term, NULL, TEXT_(T_SEARCH), TEXT_(T_SEARCH_FOR_TEXT),
	            ses, &search_history, MAX_INPUT_URL_LEN, cast_uchar "", 0,
	            0, NULL, 2, TEXT_(T_OK), search_for, TEXT_(T_CANCEL),
	            input_field_null);
}

void
free_history_lists(void)
{
	free_history(goto_url_history);
	free_history(file_history);
	free_history(search_history);
}
