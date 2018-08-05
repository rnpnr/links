/* session.c
 * (c) 2002 Mikulas Patocka
 * This file is a part of the Links program, released under GPL.
 */

#include "links.h"

struct list_head downloads = {&downloads, &downloads};

/* prototypes */
static void abort_and_delete_download(void *);
static void undisplay_download(void *);
static void increase_download_file(unsigned char **f);
static void copy_additional_files(struct additional_files **a);
static struct location *new_location(void);
static void destroy_location(struct location *loc);


int are_there_downloads(void)
{
	struct download *down;
	struct list_head *ldown;
	foreach(struct download, down, ldown, downloads) if (!down->prog) return 1;
	return 0;
}

struct list_head sessions = {&sessions, &sessions};

struct s_msg_dsc {
	int n;
	unsigned char *msg;
};

static_const struct s_msg_dsc msg_dsc[] = {
	{S_WAIT,		TEXT_(T_WAITING_IN_QUEUE)},
	{S_DNS,			TEXT_(T_LOOKING_UP_HOST)},
	{S_CONN,		TEXT_(T_MAKING_CONNECTION)},
	{S_CONN_ANOTHER,	TEXT_(T_MAKING_CONNECTION_TO_ANOTHER_ADDRESS)},
	{S_SOCKS_NEG,		TEXT_(T_SOCKS_NEGOTIATION)},
	{S_SSL_NEG,		TEXT_(T_SSL_NEGOTIATION)},
	{S_SENT,		TEXT_(T_REQUEST_SENT)},
	{S_LOGIN,		TEXT_(T_LOGGING_IN)},
	{S_GETH,		TEXT_(T_GETTING_HEADERS)},
	{S_PROC,		TEXT_(T_SERVER_IS_PROCESSING_REQUEST)},
	{S_TRANS,		TEXT_(T_TRANSFERRING)},

	{S__OK,			TEXT_(T_OK)},
	{S_INTERRUPTED,		TEXT_(T_INTERRUPTED)},
	{S_EXCEPT,		TEXT_(T_SOCKET_EXCEPTION)},
	{S_INTERNAL,		TEXT_(T_INTERNAL_ERROR)},
	{S_OUT_OF_MEM,		TEXT_(T_OUT_OF_MEMORY)},
	{S_NO_DNS,		TEXT_(T_HOST_NOT_FOUND)},
	{S_NO_PROXY_DNS,	TEXT_(T_PROXY_NOT_FOUND)},
	{S_CANT_WRITE,		TEXT_(T_ERROR_WRITING_TO_SOCKET)},
	{S_CANT_READ,		TEXT_(T_ERROR_READING_FROM_SOCKET)},
	{S_MODIFIED,		TEXT_(T_DATA_MODIFIED)},
	{S_BAD_URL,		TEXT_(T_BAD_URL_SYNTAX)},
	{S_BAD_PROXY,		TEXT_(T_BAD_PROXY_SYNTAX)},
	{S_TIMEOUT,		TEXT_(T_RECEIVE_TIMEOUT)},
	{S_RESTART,		TEXT_(T_REQUEST_MUST_BE_RESTARTED)},
	{S_STATE,		TEXT_(T_CANT_GET_SOCKET_STATE)},
	{S_CYCLIC_REDIRECT,	TEXT_(T_CYCLIC_REDIRECT)},
	{S_LARGE_FILE,		TEXT_(T_TOO_LARGE_FILE)},

	{S_HTTP_ERROR,		TEXT_(T_BAD_HTTP_RESPONSE)},
	{S_HTTP_100,		TEXT_(T_HTTP_100)},
	{S_HTTP_204,		TEXT_(T_NO_CONTENT)},
	{S_HTTPS_FWD_ERROR,	TEXT_(T_HTTPS_FWD_ERROR)},
	{S_INVALID_CERTIFICATE,	TEXT_(T_INVALID_CERTIFICATE)},
	{S_DOWNGRADED_METHOD,	TEXT_(T_DOWNGRADED_METHOD)},
	{S_INSECURE_CIPHER,	TEXT_(T_INSECURE_CIPHER)},

	{S_FILE_TYPE,		TEXT_(T_UNKNOWN_FILE_TYPE)},
	{S_FILE_ERROR,		TEXT_(T_ERROR_OPENING_FILE)},

	{S_FTP_ERROR,		TEXT_(T_BAD_FTP_RESPONSE)},
	{S_FTP_UNAVAIL,		TEXT_(T_FTP_SERVICE_UNAVAILABLE)},
	{S_FTP_LOGIN,		TEXT_(T_BAD_FTP_LOGIN)},
	{S_FTP_PORT,		TEXT_(T_FTP_PORT_COMMAND_FAILED)},
	{S_FTP_NO_FILE,		TEXT_(T_FILE_NOT_FOUND)},
	{S_FTP_FILE_ERROR,	TEXT_(T_FTP_FILE_ERROR)},

	{S_SSL_ERROR,		TEXT_(T_SSL_ERROR)},
	{S_NO_SSL,		TEXT_(T_NO_SSL)},
	{S_BAD_SOCKS_VERSION,	TEXT_(T_BAD_SOCKS_VERSION)},
	{S_SOCKS_REJECTED,	TEXT_(T_SOCKS_REJECTED_OR_FAILED)},
	{S_SOCKS_NO_IDENTD,	TEXT_(T_SOCKS_NO_IDENTD)},
	{S_SOCKS_BAD_USERID,	TEXT_(T_SOCKS_BAD_USERID)},
	{S_SOCKS_UNKNOWN_ERROR,	TEXT_(T_SOCKS_UNKNOWN_ERROR)},

	{S_NO_SMB_CLIENT,	TEXT_(T_NO_SMB_CLIENT)},

	{S_BLOCKED_URL,		TEXT_(T_BLOCKED_URL)},
	{S_NO_PROXY,		TEXT_(T_NO_PROXY)},
	{S_SMB_NOT_ALLOWED,	TEXT_(T_SMB_NOT_ALLOWED)},
	{S_FILE_NOT_ALLOWED,	TEXT_(T_FILE_NOT_ALLOWED)},

	{S_WAIT_REDIR,		TEXT_(T_WAITING_FOR_REDIRECT_CONFIRMATION)},
	{0,			NULL}
};
struct strerror_val {
	list_entry_1st
#ifdef REORDER_LIST_ENTRIES
	unsigned char pad;
#endif
	list_entry_last
	unsigned char msg[1];
};

static struct list_head strerror_buf = { &strerror_buf, &strerror_buf };

void free_strerror_buf(void)
{
	free_list(struct strerror_val, strerror_buf);
}

int get_error_from_errno(int errn)
{
	if (errn > 0 && (errn < -S__OK || errn > -S_MAX))
		return -errn;
	return S_UNKNOWN_ERROR;
}

unsigned char *get_err_msg(int state)
{
	unsigned char *e;
	size_t sl;
	struct strerror_val *s;
	struct list_head *ls;
	if ((state >= S_MAX && state <= S__OK) || state >= S_WAIT) {
		int i;
		for (i = 0; msg_dsc[i].msg; i++)
			if (msg_dsc[i].n == state) return msg_dsc[i].msg;
		unk:
		return TEXT_(T_UNKNOWN_ERROR);
	}
	if ((e = cast_uchar strerror(-state)) && *e) goto have_error;
	goto unk;
have_error:
	foreach(struct strerror_val, s, ls, strerror_buf) if (!strcmp(cast_const_char s->msg, cast_const_char e)) return s->msg;
	sl = strlen(cast_const_char e);
	if (sl > MAXINT - sizeof(struct strerror_val)) overalloc();
	s = xmalloc(sizeof(struct strerror_val) + sl);
	strcpy(cast_char s->msg, cast_const_char e);
	add_to_list(strerror_buf, s);
	return s->msg;
}

static void add_xnum_to_str(unsigned char **s, int *l, off_t n)
{
	unsigned char suff = 0;
	int d = -1;
	if (n >= 1000000000) {
		suff = 'G';
		d = (int)((n / 100000000) % 10);
		n /= 1000000000;
	} else if (n >= 1000000) {
		suff = 'M';
		d = (int)((n / 100000) % 10);
		n /= 1000000;
	} else if (n >= 1000) {
		suff = 'k';
		d = (int)((n / 100) % 10);
		n /= 1000;
	}
	add_num_to_str(s, l, n);
	if (n < 10 && d != -1) {
		add_chr_to_str(s, l, '.');
		add_num_to_str(s, l, d);
	}
	add_chr_to_str(s, l, ' ');
	if (suff) add_chr_to_str(s, l, suff);
	add_chr_to_str(s, l, 'B');
}

static void add_time_to_str(unsigned char **s, int *l, uttime t)
{
	unsigned char q[64];
	if (t >= 86400) {
		sprintf(cast_char q, "%lud ", (unsigned long)(t / 86400));
		add_to_str(s, l, q);
	}
	if (t >= 3600) {
		t %= 86400;
		sprintf(cast_char q, "%d:%02d", (int)(t / 3600), (int)(t / 60 % 60));
		add_to_str(s, l, q);
	} else {
		sprintf(cast_char q, "%d", (int)(t / 60));
		add_to_str(s, l, q);
	}
	sprintf(cast_char q, ":%02d", (int)(t % 60));
	add_to_str(s, l, q);
}

static unsigned char *get_stat_msg(struct status *stat, struct terminal *term)
{
	if (stat->state == S_TRANS && stat->prg->elapsed / 100) {
		unsigned char *m = init_str();
		int l = 0;
		add_to_str(&m, &l, get_text_translation(TEXT_(T_RECEIVED), term));
		add_to_str(&m, &l, cast_uchar " ");
		add_xnum_to_str(&m, &l, stat->prg->pos);
		if (stat->prg->size >= 0) {
			add_to_str(&m, &l, cast_uchar " ");
			add_to_str(&m, &l, get_text_translation(TEXT_(T_OF),
				term));
			add_to_str(&m, &l, cast_uchar " ");
			add_xnum_to_str(&m, &l, stat->prg->size);
		}
		add_to_str(&m, &l, cast_uchar ", ");
		if (stat->prg->elapsed >= CURRENT_SPD_AFTER * SPD_DISP_TIME) {
			add_to_str(&m, &l, get_text_translation(TEXT_(T_AVG),
				term));
			add_to_str(&m, &l, cast_uchar " ");
		}
		add_xnum_to_str(&m, &l, stat->prg->loaded * 10 / (stat->prg->elapsed / 100));
		add_to_str(&m, &l, cast_uchar "/s");
		if (stat->prg->elapsed >= CURRENT_SPD_AFTER * SPD_DISP_TIME) {
			add_to_str(&m, &l, cast_uchar ", ");
			add_to_str(&m, &l, get_text_translation(TEXT_(T_CUR), term));
			add_to_str(&m, &l, cast_uchar " ");
			add_xnum_to_str(&m, &l, stat->prg->cur_loaded / (CURRENT_SPD_SEC * SPD_DISP_TIME / 1000));
			add_to_str(&m, &l, cast_uchar "/s");
		}
		return m;
	}
	return stracpy(get_text_translation(get_err_msg(stat->state), term));
}

void change_screen_status(struct session *ses)
{
	struct status *stat = NULL;
	if (ses->rq) {
		stat = &ses->rq->stat;
	} else {
		struct f_data_c *fd = current_frame(ses);
		if (fd->rq) stat = &fd->rq->stat;
		if (stat && stat->state == S__OK && fd->af) {
			unsigned count = 0;
			struct additional_file *af;
			struct list_head *laf;
			foreachback(struct additional_file, af, laf, fd->af->af) {
				if (af->rq && af->rq->stat.state >= 0) {
					if (af->rq->stat.state > stat->state ||
					    (af->rq->stat.state == S_TRANS &&
					     stat->state == S_TRANS &&
					     af->rq->stat.prg->pos >
					     stat->prg->pos))
						stat = &af->rq->stat;
				}
				count++;
				/* avoid too high cpu consumption */
				if (count >= 100 && stat->state >= 0 && stat->state != S_WAIT)
					break;
			}
		}
	}
	free(ses->st);

	/* default status se ukazuje, kdyz
	 *			a) by se jinak ukazovalo prazdno
	 *			b) neni NULL a ukazovalo by se OK
	 */
	ses->st = NULL;
	if (stat) {
		if (stat->state == S__OK) ses->st = print_current_link(ses);
		if (!ses->st) ses->st = ses->default_status ? stracpy(ses->default_status) : get_stat_msg(stat, ses->term);
	} else {
		ses->st = stracpy(ses->default_status);
	}
}

static void x_print_screen_status(struct terminal *term, void *ses_)
{
	struct session *ses = (struct session *)ses_;
	if (!F) {
		unsigned char color = get_attribute(ses->ds.t_text_color, ses->ds.t_background_color);
		if (!term->spec->col) color = COLOR_TITLE;
		fill_area(term, 0, term->y - 1, term->x, 1, ' ', color);
		if (ses->st) print_text(term, 0, term->y - 1, (int)strlen(cast_const_char ses->st), ses->st, COLOR_STATUS);
#ifdef G
	} else {
		int l = 0;
		if (ses->st) g_print_text(term->dev, 0, term->y - G_BFU_FONT_SIZE, bfu_style_wb_mono, ses->st, &l);
		drv->fill_area(term->dev, l, term->y - G_BFU_FONT_SIZE, term->x, term->y, bfu_bg_color);
#endif
	}
}

static void x_print_screen_title(struct terminal *term, void *ses_)
{
	struct session *ses = (struct session *)ses_;
	unsigned char *m;
	unsigned char color = get_attribute(ses->ds.t_text_color, ses->ds.t_background_color);
	fill_area(term, 0, 0, term->x, 1, ' ', color);
	if ((m = print_current_title(ses))) {
		int p = term->x - 1 - cp_len(term_charset(ses->term), m);
		if (p < 0) p = 0;
		print_text(term, p, 0, cp_len(term_charset(ses->term), m), m, color);
		free(m);
	}
}

static void print_only_screen_status(struct session *ses)
{
#ifdef G
	if (F) {
		/*debug("%s - %s", ses->st_old, ses->st);
		debug("clip: %d.%d , %d.%d", ses->term->dev->clip.x1, ses->term->dev->clip.y1, ses->term->dev->clip.x2, ses->term->dev->clip.y2);
		debug("size: %d.%d , %d.%d", ses->term->dev->size.x1, ses->term->dev->size.y1, ses->term->dev->size.x2, ses->term->dev->size.y2);*/
		if (ses->st_old) {
			if (ses->st && !strcmp(cast_const_char ses->st, cast_const_char ses->st_old)) return;
			free(ses->st_old);
			ses->st_old = NULL;
		}
		if (!memcmp(&ses->term->dev->clip, &ses->term->dev->size, sizeof(struct rect))) ses->st_old = stracpy(ses->st);
	}
#endif
	draw_to_window(ses->win, x_print_screen_status, ses);
}

void print_screen_status(struct session *ses)
{
	unsigned char *m;

	print_only_screen_status(ses);
	if (!F)
		draw_to_window(ses->win, x_print_screen_title, ses);

	m = stracpy(cast_uchar "Links");
	if (ses->screen
	&& ses->screen->f_data
	&& ses->screen->f_data->title
	&& ses->screen->f_data->title[0]) {
		add_to_strn(&m, cast_uchar " - ");
		add_to_strn(&m, ses->screen->f_data->title);
	}
	set_terminal_title(ses->term, m);

	if (!F && ses->brl_cursor_mode) {
		if (ses->brl_cursor_mode == 1) set_cursor(ses->term, 0, 0, 0, 0);
		if (ses->brl_cursor_mode == 2) set_cursor(ses->term, 0, ses->term->y - 1, 0, ses->term->y - 1);
	}
}

void print_progress(struct session *ses, unsigned char *msg)
{
	free(ses->st);
	ses->st = stracpy(get_text_translation(msg, ses->term));
	print_only_screen_status(ses);
	flush_terminal(ses->term);
}

void print_error_dialog(struct session *ses, struct status *stat, unsigned char *url)
{
	unsigned char *t = get_err_msg(stat->state);
	unsigned char *u = display_url(ses->term, url, 1);
	msg_box(ses->term, getml(u, NULL), TEXT_(T_ERROR), AL_CENTER, TEXT_(T_ERROR_LOADING), cast_uchar " ", u, cast_uchar ":\n\n", t, MSG_BOX_END, (void *)ses, 1, TEXT_(T_CANCEL), msg_box_null, B_ENTER | B_ESC /*, get_text_translation("Retry"), NULL, 0 !!! FIXME: retry */);
}

static inline unsigned char hx(int a)
{
	return a >= 10 ? a + 'A' - 10 : a + '0';
}

static inline int unhx(unsigned char a)
{
	if (a >= '0' && a <= '9') return a - '0';
	if (a >= 'A' && a <= 'F') return a - 'A' + 10;
	if (a >= 'a' && a <= 'f') return a - 'a' + 10;
	return -1;
}

unsigned char *encode_url(unsigned char *url)
{
	unsigned char *u = init_str();
	int l = 0;
	add_to_str(&u, &l, cast_uchar "+++");
	for (; *url; url++) {
		if (is_safe_in_shell(*url) && *url != '+') add_chr_to_str(&u, &l, *url);
		else {
			add_chr_to_str(&u, &l, '+');
			add_chr_to_str(&u, &l, hx(*url >> 4));
			add_chr_to_str(&u, &l, hx(*url & 0xf));
		}
	}
	return u;
}

unsigned char *decode_url(unsigned char *url)
{
	unsigned char *u;
	int l;
	if (casecmp(url, cast_uchar "+++", 3)) return stracpy(url);
	url += 3;
	u = init_str();
	l = 0;
	for (; *url; url++) {
		if (*url != '+' || unhx(url[1]) == -1 || unhx(url[2]) == -1) add_chr_to_str(&u, &l, *url);
		else {
			add_chr_to_str(&u, &l, (unhx(url[1]) << 4) + unhx(url[2]));
			url += 2;
		}
	}
	return u;
}

struct session *get_download_ses(struct download *down)
{
	struct session *ses;
	struct list_head *lses;
	if (down) foreach(struct session, ses, lses, sessions) if (ses == down->ses) return ses;
	if (!list_empty(sessions)) return list_struct(sessions.next, struct session);
	return NULL;
}

static int close_download_file(struct download *down)
{
	int rs;
	if (down->handle != -1) {
		EINTRLOOP(rs, ftruncate(down->handle, down->last_pos - down->file_shift));
		EINTRLOOP(rs, close(down->handle));
		down->handle = -1;
		if (rs) return -1;
	}
	return 0;
}

static void delete_download_file(struct download *down)
{
	int rs;
	unsigned char *file = stracpy(down->orig_file);
	unsigned char *wd = get_cwd();
	set_cwd(down->cwd);
	while (1) {
		unsigned char *f = translate_download_file(file);
		EINTRLOOP(rs, unlink(cast_const_char f));
		free(f);
		if (!strcmp(cast_const_char file, cast_const_char down->file)) break;
		increase_download_file(&file);
	}
	free(file);
	if (wd) {
		set_cwd(wd);
		free(wd);
	}
}

static void abort_download(void *down_)
{
	struct download *down = (struct download *)down_;
	unregister_bottom_half(abort_download, down);
	unregister_bottom_half(abort_and_delete_download, down);
	unregister_bottom_half(undisplay_download, down);

	if (down->win) delete_window(down->win);
	if (down->ask) delete_window(down->ask);
	if (down->stat.state >= 0) change_connection(&down->stat, NULL, PRI_CANCEL);
	free(down->url);
	close_download_file(down);
	if (down->prog) {
		delete_download_file(down);
		free(down->prog);
	}
	free(down->cwd);
	free(down->orig_file);
	free(down->file);
	del_from_list(down);
	free(down);
}

static void abort_and_delete_download(void *down_)
{
	struct download *down = (struct download *)down_;
	abort_download(down);
}

int test_abort_downloads_to_file(unsigned char *file, unsigned char *cwd, int abort_downloads)
{
	int ret = 0;
	struct download *down;
	struct list_head *ldown;
	foreach(struct download, down, ldown, downloads) {
		if (strcmp(cast_const_char down->cwd, cast_const_char cwd)) {
			if (file[0] == '/') goto abs;
			continue;
		}
		abs:
		if (!strcmp(cast_const_char down->file, cast_const_char file) || !strcmp(cast_const_char down->orig_file, cast_const_char file)) {
			ret = 1;
			if (!abort_downloads) break;
			ldown = ldown->prev;
			abort_download(down);
		}
	}
	return ret;
}

static void undisplay_download(void *down_)
{
	struct download *down = (struct download *)down_;
	if (down->win) delete_window(down->win);
}

static int dlg_abort_download(struct dialog_data *dlg, struct dialog_item_data *di)
{
	register_bottom_half(abort_download, dlg->dlg->udata);
	return 0;
}

static int dlg_abort_and_delete_download(struct dialog_data *dlg, struct dialog_item_data *di)
{
	register_bottom_half(abort_and_delete_download, dlg->dlg->udata);
	return 0;
}

static int dlg_undisplay_download(struct dialog_data *dlg, struct dialog_item_data *di)
{
	register_bottom_half(undisplay_download, dlg->dlg->udata);
	return 0;
}

static void download_abort_function(struct dialog_data *dlg)
{
	struct download *down = dlg->dlg->udata;
	down->win = NULL;
}

static int test_percentage(struct status *stat)
{
	return stat->prg->size > 0;
}

static int download_meter(int size, struct status *stat)
{
	int m;
	if (!stat->prg->size) return 0;
	m = (int)((double)size * (double)stat->prg->pos / (double)stat->prg->size);
	if (m < 0) m = 0;
	if (m > size) m = size;
	return m;
}

unsigned char *download_percentage(struct download *down, int pad)
{
	unsigned char *s;
	int l;
	int perc;
	struct status *stat = &down->stat;
	if (stat->state != S_TRANS || !test_percentage(stat)) return stracpy(cast_uchar "");
	s = init_str();
	l = 0;
	perc = download_meter(100, stat);
	if (pad) {
		if (perc < 10) add_chr_to_str(&s, &l, ' ');
		if (perc < 100) add_chr_to_str(&s, &l, ' ');
	}
	add_num_to_str(&s, &l, perc);
	add_chr_to_str(&s, &l, '%');
	return s;
}

void download_window_function(struct dialog_data *dlg)
{
	struct download *down = dlg->dlg->udata;
	struct terminal *term = dlg->win->term;
	int max = 0, min = 0;
	int w, x, y;
	int t = 0;
	int show_percentage = 0;
	unsigned char *m, *u;
	struct status *stat = &down->stat;
	if (!F) redraw_below_window(dlg->win);
	down->win = dlg->win;
	if (stat->state == S_TRANS && stat->prg->elapsed / 100) {
		int l = 0;
		m = init_str();
		t = 1;
		add_to_str(&m, &l, get_text_translation(TEXT_(T_RECEIVED), term));
		add_to_str(&m, &l, cast_uchar " ");
		add_xnum_to_str(&m, &l, stat->prg->pos);
		if (stat->prg->size >= 0) {
			add_to_str(&m, &l, cast_uchar " ");
			add_to_str(&m, &l, get_text_translation(TEXT_(T_OF),term));
			add_to_str(&m, &l, cast_uchar " ");
			add_xnum_to_str(&m, &l, stat->prg->size);
			add_to_str(&m, &l, cast_uchar " ");
		}
		add_to_str(&m, &l, cast_uchar "\n");
		if (stat->prg->elapsed >= CURRENT_SPD_AFTER * SPD_DISP_TIME)
			add_to_str(&m, &l, get_text_translation(TEXT_(T_AVERAGE_SPEED), term));
		else add_to_str(&m, &l, get_text_translation(TEXT_(T_SPEED), term));
		add_to_str(&m, &l, cast_uchar " ");
		add_xnum_to_str(&m, &l, (longlong)stat->prg->loaded * 10 / (stat->prg->elapsed / 100));
		add_to_str(&m, &l, cast_uchar "/s");
		if (stat->prg->elapsed >= CURRENT_SPD_AFTER * SPD_DISP_TIME) {
			add_to_str(&m, &l, cast_uchar ", ");
			add_to_str(&m, &l, get_text_translation(TEXT_(T_CURRENT_SPEED), term));
			add_to_str(&m, &l, cast_uchar " ");
			add_xnum_to_str(&m, &l, stat->prg->cur_loaded / (CURRENT_SPD_SEC * SPD_DISP_TIME / 1000));
			add_to_str(&m, &l, cast_uchar "/s");
		}
		add_to_str(&m, &l, cast_uchar "\n");
		add_to_str(&m, &l, get_text_translation(TEXT_(T_ELAPSED_TIME), term));
		add_to_str(&m, &l, cast_uchar " ");
		add_time_to_str(&m, &l, stat->prg->elapsed / 1000);
		if (stat->prg->size >= 0 && stat->prg->loaded > 0) {
			add_to_str(&m, &l, cast_uchar ", ");
			add_to_str(&m, &l, get_text_translation(TEXT_(T_ESTIMATED_TIME), term));
			add_to_str(&m, &l, cast_uchar " ");
			/*add_time_to_str(&m, &l, stat->prg->elapsed / 1000 * stat->prg->size / stat->prg->loaded * 1000 - stat->prg->elapsed);*/
			/*add_time_to_str(&m, &l, (stat->prg->size - stat->prg->pos) / ((longlong)stat->prg->loaded * 10 / (stat->prg->elapsed / 100)));*/
			add_time_to_str(&m, &l, (uttime)((stat->prg->size - stat->prg->pos) / ((double)stat->prg->loaded * 1000 / stat->prg->elapsed)));
		}
	} else m = stracpy(get_text_translation(get_err_msg(stat->state), term));
	show_percentage = t && test_percentage(stat);
	u = display_url(term, down->url, 1);
	max_text_width(term, u, &max, AL_LEFT);
	min_text_width(term, u, &min, AL_LEFT);
	max_text_width(term, m, &max, AL_LEFT);
	min_text_width(term, m, &min, AL_LEFT);
	max_buttons_width(term, dlg->items, dlg->n, &max);
	min_buttons_width(term, dlg->items, dlg->n, &min);
	w = dlg->win->term->x * 9 / 10 - 2 * DIALOG_LB;
	if (w < min) w = min;
	if (w > dlg->win->term->x - 2 * DIALOG_LB)
		w = dlg->win->term->x - 2 * DIALOG_LB;
	if (show_percentage) {
		if (w < DOWN_DLG_MIN) w = DOWN_DLG_MIN;
	} else {
		if (w > max) w = max;
	}
	if (w < 1) w = 1;
	y = 0;
	dlg_format_text(dlg, NULL, u, 0, &y, w, NULL, COLOR_DIALOG_TEXT, AL_LEFT);
	y += gf_val(1, G_BFU_FONT_SIZE);
	if (show_percentage) y += gf_val(2, 2 * G_BFU_FONT_SIZE);
	dlg_format_text(dlg, NULL, m, 0, &y, w, NULL, COLOR_DIALOG_TEXT, AL_LEFT);
	y += gf_val(1, G_BFU_FONT_SIZE);
	dlg_format_buttons(dlg, NULL, dlg->items, dlg->n, 0, &y, w, NULL, AL_CENTER);
	dlg->xw = w + 2 * DIALOG_LB;
	dlg->yw = y + 2 * DIALOG_TB;
	center_dlg(dlg);
	draw_dlg(dlg);
	y = dlg->y + DIALOG_TB + gf_val(1, G_BFU_FONT_SIZE);
	x = dlg->x + DIALOG_LB;
	dlg_format_text(dlg, term, u, x, &y, w, NULL, COLOR_DIALOG_TEXT, AL_LEFT);
	if (show_percentage) {
		if (!F) {
			unsigned char *q;
			int p = w - 6;
			y++;
			set_only_char(term, x, y, '[', 0);
			set_only_char(term, x + p + 1, y, ']', 0);
			fill_area(term, x + 1, y, download_meter(p, stat), 1, CHAR_DIALOG_METER, COLOR_DIALOG_METER);
			q = download_percentage(down, 1);
			print_text(term, x + p + 2, y, (int)strlen(cast_const_char q), q, COLOR_DIALOG_TEXT);
			free(q);
			y++;
#ifdef G
		} else {
			unsigned char *q;
			int p, s, ss, m;
			y += G_BFU_FONT_SIZE;
			q = download_percentage(down, 1);
			extend_str(&q, 1);
			memmove(q + 1, q, strlen(cast_const_char q) + 1);
			q[0] = ']';
			s = g_text_width(bfu_style_bw_mono, cast_uchar "[");
			ss = g_text_width(bfu_style_bw_mono, q);
			p = w - s - ss;
			if (p < 0) p = 0;
			m = download_meter(p, stat);
			g_print_text(term->dev, x, y, bfu_style_bw_mono, cast_uchar "[", NULL);
			drv->fill_area(term->dev, x + s, y, x + s + m, y + G_BFU_FONT_SIZE, bfu_fg_color);
			drv->fill_area(term->dev, x + s + m, y, x + s + p, y + G_BFU_FONT_SIZE, bfu_bg_color);
			g_print_text(term->dev, x + w - ss, y, bfu_style_bw_mono, q, NULL);
			if (dlg->s) exclude_from_set(&dlg->s, x, y, x + w, y + G_BFU_FONT_SIZE);
			free(q);
			y += G_BFU_FONT_SIZE;
#endif
		}
	}
	y += gf_val(1, G_BFU_FONT_SIZE);
	dlg_format_text(dlg, term, m, x, &y, w, NULL, COLOR_DIALOG_TEXT, AL_LEFT);
	y += gf_val(1, G_BFU_FONT_SIZE);
	dlg_format_buttons(dlg, term, dlg->items, dlg->n, x, &y, w, NULL, AL_CENTER);
	free(u);
	free(m);
}

void display_download(struct terminal *term, void *down_, void *ses_)
{
	struct download *down = (struct download *)down_;
	struct session *ses = (struct session *)ses_;
	struct dialog *dlg;
	struct download *dd;
	struct list_head *ldd;
	foreach(struct download, dd, ldd, downloads) if (dd == down) goto found;
	return;
found:
	dlg = mem_calloc(sizeof(struct dialog) + 4 * sizeof(struct dialog_item));
	undisplay_download(down);
	down->ses = ses;
	dlg->title = TEXT_(T_DOWNLOAD);
	dlg->fn = download_window_function;
	dlg->abort = download_abort_function;
	dlg->udata = down;
	dlg->align = AL_CENTER;
	dlg->items[0].type = D_BUTTON;
	dlg->items[0].gid = B_ENTER | B_ESC;
	dlg->items[0].fn = dlg_undisplay_download;
	dlg->items[0].text = TEXT_(T_BACKGROUND);
	dlg->items[1].type = D_BUTTON;
	dlg->items[1].gid = 0;
	dlg->items[1].fn = dlg_abort_download;
	dlg->items[1].text = TEXT_(T_ABORT);
	if (!down->prog) {
		dlg->items[2].type = D_BUTTON;
		dlg->items[2].gid = 0;
		dlg->items[2].fn = dlg_abort_and_delete_download;
		dlg->items[2].text = TEXT_(T_ABORT_AND_DELETE_FILE);
		dlg->items[3].type = D_END;
	} else {
		dlg->items[2].type = D_END;
	}
	do_dialog(term, dlg, getml(dlg, NULL));
}

time_t parse_http_date(unsigned char *date)	/* this functions is bad !!! */
{
	static unsigned char *months[12] = {
		cast_uchar "Jan",
		cast_uchar "Feb",
		cast_uchar "Mar",
		cast_uchar "Apr",
		cast_uchar "May",
		cast_uchar "Jun",
		cast_uchar "Jul",
		cast_uchar "Aug",
		cast_uchar "Sep",
		cast_uchar "Oct",
		cast_uchar "Nov",
		cast_uchar "Dec"
	};

	time_t t = 0;
	/* Mon, 03 Jan 2000 21:29:33 GMT */
	int y;
	struct tm tm;
	memset(&tm, 0, sizeof(struct tm));

	date = cast_uchar strchr(cast_const_char date, ' ');
	if (!date) return 0;
	date++;
	if (*date >= '0' && *date <= '9') {
			/* Sun, 06 Nov 1994 08:49:37 GMT */
			/* Sunday, 06-Nov-94 08:49:37 GMT */
		y = 0;
		if (date[0] < '0' || date[0] > '9') return 0;
		if (date[1] < '0' || date[1] > '9') return 0;
		tm.tm_mday = (date[0] - '0') * 10 + date[1] - '0';
		date += 2;
		if (*date != ' ' && *date != '-') return 0;
		date += 1;
		for (tm.tm_mon = 0; tm.tm_mon < 12; tm.tm_mon++)
			if (!casecmp(date, months[tm.tm_mon], 3)) goto f1;
		return 0;
		f1:
		date += 3;
		if (*date == ' ') {
				/* Sun, 06 Nov 1994 08:49:37 GMT */
			date++;
			if (date[0] < '0' || date[0] > '9') return 0;
			if (date[1] < '0' || date[1] > '9') return 0;
			if (date[2] < '0' || date[2] > '9') return 0;
			if (date[3] < '0' || date[3] > '9') return 0;
			tm.tm_year = (date[0] - '0') * 1000 + (date[1] - '0') * 100 + (date[2] - '0') * 10 + date[3] - '0' - 1900;
			date += 4;
		} else if (*date == '-') {
				/* Sunday, 06-Nov-94 08:49:37 GMT */
			date++;
			if (date[0] < '0' || date[0] > '9') return 0;
			if (date[1] < '0' || date[1] > '9') return 0;
			tm.tm_year = (date[0] >= '7' ? 1900 : 2000) + (date[0] - '0') * 10 + date[1] - '0' - 1900;
			date += 2;
		} else return 0;
		if (*date != ' ') return 0;
		date++;
	} else {
			/* Sun Nov  6 08:49:37 1994 */
		y = 1;
		for (tm.tm_mon = 0; tm.tm_mon < 12; tm.tm_mon++)
			if (!casecmp(date, months[tm.tm_mon], 3)) goto f2;
		return 0;
		f2:
		date += 3;
		while (*date == ' ') date++;
		if (date[0] < '0' || date[0] > '9') return 0;
		tm.tm_mday = date[0] - '0';
		date++;
		if (*date != ' ') {
			if (date[0] < '0' || date[0] > '9') return 0;
			tm.tm_mday = tm.tm_mday * 10 + date[0] - '0';
			date++;
		}
		if (*date != ' ') return 0;
		date++;
	}

	if (date[0] < '0' || date[0] > '9') return 0;
	if (date[1] < '0' || date[1] > '9') return 0;
	tm.tm_hour = (date[0] - '0') * 10 + date[1] - '0';
	date += 2;
	if (*date != ':') return 0;
	date++;
	if (date[0] < '0' || date[0] > '9') return 0;
	if (date[1] < '0' || date[1] > '9') return 0;
	tm.tm_min = (date[0] - '0') * 10 + date[1] - '0';
	date += 2;
	if (*date != ':') return 0;
	date++;
	if (date[0] < '0' || date[0] > '9') return 0;
	if (date[1] < '0' || date[1] > '9') return 0;
	tm.tm_sec = (date[0] - '0') * 10 + date[1] - '0';
	date += 2;
	if (y) {
		if (*date != ' ') return 0;
		date++;
		if (date[0] < '0' || date[0] > '9') return 0;
		if (date[1] < '0' || date[1] > '9') return 0;
		if (date[2] < '0' || date[2] > '9') return 0;
		if (date[3] < '0' || date[3] > '9') return 0;
		tm.tm_year = (date[0] - '0') * 1000 + (date[1] - '0') * 100 + (date[2] - '0') * 10 + date[3] - '0' - 1900;
		date += 4;
	}
	if (*date != ' ' && *date) return 0;

	t = mktime(&tm);
	if (t == (time_t) -1) return 0;
	return t;
}


static void download_file_error(struct download *down, int err)
{
	if (get_download_ses(down)) {
		unsigned char *emsg = stracpy(err ? cast_uchar strerror(err) : cast_uchar "Zero returned");
		unsigned char *msg = stracpy(down->file);
		msg_box(get_download_ses(down)->term, getml(msg, emsg, NULL), TEXT_(T_DOWNLOAD_ERROR), AL_CENTER, TEXT_(T_COULD_NOT_WRITE_TO_FILE), cast_uchar " ", msg, cast_uchar ": ", emsg, MSG_BOX_END, NULL, 1, TEXT_(T_CANCEL), msg_box_null, B_ENTER | B_ESC);
	}
}

static int download_write(struct download *down, void *ptr, off_t to_write)
{
	int w;
	int err;
	if (to_write != (int)to_write || (int)to_write < 0) to_write = MAXINT;
	try_write_again:
	w = hard_write(down->handle, ptr, (int)to_write);
	if (w >= 0) err = 0;
	else err = errno;
	if (w <= -!to_write) {
#ifdef EFBIG
		if (err == EFBIG && !down->prog) {
			if (to_write > 1) {
				to_write >>= 1;
				goto try_write_again;
			}
			if (down->last_pos == down->file_shift) goto no_e2big;
			if (close_download_file(down)) {
				download_file_error(down, errno);
				return -1;
			}
			increase_download_file(&down->file);
			if ((down->handle = create_download_file(get_download_ses(down), down->cwd, down->file, 0, down->last_pos - down->file_shift)) < 0) return -1;
			down->file_shift = down->last_pos;
			goto try_write_again;
			no_e2big:;
		}
#endif
		download_file_error(down, err);
		return -1;
	}
	down->last_pos += w;
	down->downloaded_something = 1;
	return 0;
}

static void download_data(struct status *stat, void *down_)
{
	struct download *down = (struct download *)down_;
	struct cache_entry *ce;
	struct fragment *frag;
	struct list_head *lfrag;
	int rs;
	if (!(ce = stat->ce)) goto end_store;
	if (stat->state >= S_WAIT && stat->state < S_TRANS) goto end_store;
	if (!down->remotetime && ce->last_modified) down->remotetime = parse_http_date(ce->last_modified);
	if (!down->downloaded_something) {
		unsigned char *enc;
		if (ce->redirect) {
			if (down->redirect_cnt++ < MAX_REDIRECTS) {
				unsigned char *u, *p;
				unsigned char *prev_down_url;
				int cache, allow_flags;
				if (stat->state >= 0) change_connection(&down->stat, NULL, PRI_CANCEL);
				u = join_urls(down->url, ce->redirect);
				extract_position(u);
				if (!http_options.bug_302_redirect) if (!ce->redirect_get && (p = cast_uchar strchr(cast_const_char down->url, POST_CHAR))) add_to_strn(&u, p);
				prev_down_url = down->url;
				down->url = u;
				down->stat.state = S_WAIT_REDIR;
				if (down->win) {
					struct links_event ev = { EV_REDRAW, 0, 0, 0 };
					ev.x = down->win->term->x;
					ev.y = down->win->term->y;
					down->win->handler(down->win, &ev, 0);
				}
				cache = NC_CACHE;
				if (!strcmp(cast_const_char down->url, cast_const_char prev_down_url) || down->redirect_cnt >= MAX_CACHED_REDIRECTS) cache = NC_RELOAD;
				allow_flags = get_allow_flags(prev_down_url);
				free(prev_down_url);
				load_url(down->url, NULL, &down->stat, PRI_DOWNLOAD, cache, 1, allow_flags, down->last_pos);
				return;
			} else {
				if (stat->state >= 0) change_connection(&down->stat, NULL, PRI_CANCEL);
				stat->state = S_CYCLIC_REDIRECT;
				goto end_store;
			}
		}
		enc = get_content_encoding(ce->head, ce->url, !down->prog);
		if (enc) {
			down->decompress = 1;
			free(enc);
			enc = get_content_encoding(ce->head, ce->url, 1);
			if (enc) {
				free(enc);
				detach_connection(stat, down->last_pos, 1, 0);
			}
		} else {
			down->decompress = 0;
		}
	}
	if (!down->decompress) {
		foreachback(struct fragment, frag, lfrag, ce->frag)
			if (frag->offset <= down->last_pos)
				goto have_frag;
		foreach(struct fragment, frag, lfrag, ce->frag) {
have_frag:
			while (frag->offset <= down->last_pos && frag->offset + frag->length > down->last_pos) {
				if (download_write(down, frag->data + (down->last_pos - frag->offset), frag->length - (down->last_pos - frag->offset))) {
					det_abt:
					detach_connection(stat, down->last_pos, 0, 0);
					abort_download(down);
					return;
				}
			}
		}
	}
	if (!down->decompress) detach_connection(stat, down->last_pos, 0, 0);
	end_store:
	if (stat->state < 0) {
		if (down->decompress) {
			struct session *ses = get_download_ses(down);
			unsigned char *start, *end;
			int err;
			get_file_by_term(ses ? ses->term : NULL, ce, &start, &end, &err);
			if (err) goto det_abt;
			while (down->last_pos < end - start) {
				if (download_write(down, start + down->last_pos, end - start - down->last_pos)) goto det_abt;
			}
		}
		if (stat->state != S__OK) {
			unsigned char *t = get_err_msg(stat->state);
			unsigned char *tt = display_url(get_download_ses(down)->term, down->url, 1);
			msg_box(get_download_ses(down)->term, getml(tt, NULL), TEXT_(T_DOWNLOAD_ERROR), AL_CENTER, TEXT_(T_ERROR_DOWNLOADING), cast_uchar " ", tt, cast_uchar ":\n\n", t, MSG_BOX_END, (void *)get_download_ses(down), 1, TEXT_(T_CANCEL), msg_box_null, B_ENTER | B_ESC /*, TEXT_(T_RETRY), NULL, 0 !!! FIXME: retry */);
		} else {
			if (close_download_file(down)) {
				download_file_error(down, errno);
			} else if (down->prog) {
				exec_on_terminal(get_download_ses(down)->term, down->prog, down->orig_file, !!down->prog_flag_block);
				free(down->prog);
				down->prog = NULL;
			} else if (down->remotetime && download_utime) {
				struct timeval utv[2];
				unsigned char *file = stracpy(down->orig_file);
				unsigned char *wd = get_cwd();
				set_cwd(down->cwd);
				utv[0].tv_usec = utv[1].tv_usec = 0;
				utv[0].tv_sec = utv[1].tv_sec = down->remotetime;
				while (1) {
					unsigned char *f = translate_download_file(file);
					EINTRLOOP(rs, utimes(cast_char f, utv));
					free(f);
					if (!strcmp(cast_const_char file, cast_const_char down->file)) break;
					increase_download_file(&file);
				}
				free(file);
				if (wd) {
					set_cwd(wd);
					free(wd);
				}
			}
		}
		abort_download(down);
		return;
	}
	if (down->win) {
		struct links_event ev = { EV_REDRAW, 0, 0, 0 };
		ev.x = down->win->term->x;
		ev.y = down->win->term->y;
		down->win->handler(down->win, &ev, 0);
	}
}

unsigned char *translate_download_file(unsigned char *fi)
{
	unsigned char *file = stracpy(cast_uchar "");
	unsigned char *h;
	if (fi[0] == '~' && dir_sep(fi[1]) && (h = cast_uchar getenv("HOME"))) {
		add_to_strn(&file, h);
		fi++;
	}
	add_to_strn(&file, fi);
	return file;
}

int create_download_file(struct session *ses, unsigned char *cwd, unsigned char *fi, int mode, off_t siz)
{
	unsigned char *wd;
	unsigned char *file;
	int h;
#ifdef NO_FILE_SECURITY
	int perm = 0666;
#else
	int perm = mode & CDF_RESTRICT_PERMISSION ? 0600 : 0666;
#endif
	wd = get_cwd();
	set_cwd(cwd);
	file = translate_download_file(fi);
	h = c_open3(file, O_CREAT | O_NOCTTY | O_WRONLY | (mode & CDF_NOTRUNC ? 0 : O_TRUNC) | (mode & CDF_EXCL ? O_EXCL : 0), perm);
	if (h == -1) {
		unsigned char *msg, *msge;
		int errn = errno;
		if (errn == EEXIST && mode & CDF_NO_POPUP_ON_EEXIST) {
			h = -2;
			goto x;
		}
		if (!ses) goto x;
		msg = stracpy(file);
		msge = stracpy(cast_uchar strerror(errn));
		msg_box(ses->term, getml(msg, msge, NULL), TEXT_(T_DOWNLOAD_ERROR), AL_CENTER, TEXT_(T_COULD_NOT_CREATE_FILE), cast_uchar " ", msg, cast_uchar ": ", msge, MSG_BOX_END, NULL, 1, TEXT_(T_CANCEL), msg_box_null, B_ENTER | B_ESC);
		goto x;
	}
	x:
	free(file);
	if (wd) {
		set_cwd(wd);
		free(wd);
	}
	return h;
}

static int create_or_append_download_file(struct session *ses, unsigned char *cwd, unsigned char *fi, int mode, int *hp, unsigned char **xl_file, off_t *last_pos, off_t *file_shift)
{
	int rs;
	int down_flags = mode == DOWNLOAD_CONTINUE ? CDF_NOTRUNC : mode == DOWNLOAD_OVERWRITE ? 0 : CDF_EXCL;

	*xl_file = stracpy(fi);
	*last_pos = 0;
	*file_shift = 0;

	retry_next_file:
	test_abort_downloads_to_file(*xl_file, ses->term->cwd, 1);

	if ((*hp = create_download_file(ses, ses->term->cwd, *xl_file, down_flags, 0)) < 0)
		goto err_free;

	if (mode == DOWNLOAD_CONTINUE) {
		off_t ls;
		unsigned char *f;
		struct stat st;

		EINTRLOOP(rs, fstat(*hp, &st));
		if (rs || !S_ISREG(st.st_mode))
			goto ret_0;

		EINTRLOOP(ls, lseek(*hp, 0, SEEK_END));
		if (ls == (off_t)-1) {
			unsigned char *emsg = stracpy(cast_uchar strerror(errno));
			unsigned char *msg = stracpy(*xl_file);
			msg_box(ses->term, getml(msg, emsg, NULL), TEXT_(T_DOWNLOAD_ERROR), AL_CENTER, TEXT_(T_ERROR_CALLING_LSEEK_ON_FILE), cast_uchar " ", msg, cast_uchar ": ", emsg, MSG_BOX_END, NULL, 1, TEXT_(T_CANCEL), msg_box_null, B_ENTER | B_ESC);
			goto err_close;
		}
		if ((off_t)(0UL + *last_pos + ls) < 0 ||
		    (off_t)(0UL + *last_pos + ls) < *last_pos) {
			unsigned char *msg1 = stracpy(fi);
			unsigned char *msg2 = stracpy(*xl_file);
			msg_box(ses->term, getml(msg1, msg2, NULL), TEXT_(T_DOWNLOAD_ERROR), AL_CENTER, TEXT_(T_TOO_LARGE_FILE_SEQUENCE), cast_uchar " ", msg1, cast_uchar " - ", msg2, MSG_BOX_END, NULL, 1, TEXT_(T_CANCEL), msg_box_null, B_ENTER | B_ESC);
			goto err_close;
		}
		*last_pos += ls;

		f = stracpy(*xl_file);
		increase_download_file(&f);
		EINTRLOOP(rs, stat(cast_const_char f, &st));
		if (rs || !S_ISREG(st.st_mode)) {
			free(f);
			goto ret_0;
		}
		EINTRLOOP(rs, close(*hp));
		free(*xl_file);
		*xl_file = f;
		*file_shift = *last_pos;
		goto retry_next_file;
	}

	ret_0:
	return 0;

	err_close:
	EINTRLOOP(rs, close(*hp));
	err_free:
	free(*xl_file);
	return -1;
}

static void increase_download_file(unsigned char **f)
{
	unsigned char *p = NULL, *pp = *f;
	unsigned char *q;
	while ((pp = cast_uchar strstr(cast_const_char pp, ".part-"))) p = pp += 6;
	if (!p || !*p) {
		no_suffix:
		add_to_strn(f, cast_uchar ".part-2");
		return;
	}
	for (q = p; *q; q++) if (*q < '0' || *q > '9') goto no_suffix;
	for (q--; q >= p; q--) {
		if (*q < '9') {
			(*q)++;
			return;
		}
		*q = '0';
	}
	*p = '1';
	add_to_strn(f, cast_uchar "0");
}

static unsigned char *get_temp_name(unsigned char *url, unsigned char *head)
{
	int nl;
	unsigned char *name, *fn, *fnx;
	unsigned char *nm;
	unsigned char *directory = NULL;
	nm = cast_uchar tempnam(cast_const_char directory, "links");
	if (!nm) return NULL;
	name = init_str();
	nl = 0;
	add_to_str(&name, &nl, nm);
	free(nm);
	fn = get_filename_from_url(url, head, 1);
	fnx = cast_uchar strchr(cast_const_char fn, '.');
	if (fnx) {
		unsigned char *s;
		s = stracpy(fnx);
		check_shell_security(&s);
		add_to_str(&name, &nl, s);
		free(s);
	}
	free(fn);
	return name;
}

unsigned char *subst_file(unsigned char *prog, unsigned char *file, int cyg_subst)
{
	unsigned char *orig_prog = prog;
	unsigned char *nn;
	unsigned char *n = init_str();
	int l = 0;
	while (*prog) {
		int p;
		for (p = 0; prog[p] && prog[p] != '%'; p++);
		add_bytes_to_str(&n, &l, prog, p);
		prog += p;
		if (*prog == '%') {
			if (cyg_subst) {
				unsigned char *conv = os_conv_to_external_path(file, orig_prog);
				add_to_str(&n, &l, conv);
				free(conv);
			} else
				add_to_str(&n, &l, file);
			prog++;
		}
	}
	nn = os_fixup_external_program(n);
	free(n);
	return nn;
}

void start_download(struct session *ses, unsigned char *file, int mode)
{
	struct download *down;
	int h;
	unsigned char *url = ses->dn_url;
	unsigned char *xl_file;
	off_t last_pos = 0, file_shift = 0;

	if (!url)
		return;
	extract_position(url);

	if (create_or_append_download_file(ses, ses->term->cwd, file, mode, &h, &xl_file, &last_pos, &file_shift) < 0) return;

	down = mem_calloc(sizeof(struct download));
	down->url = stracpy(url);
	down->stat.end = download_data;
	down->stat.data = down;
	down->decompress = 0;
	down->last_pos = last_pos;
	down->file_shift = file_shift;
	down->cwd = stracpy(ses->term->cwd);
	down->orig_file = stracpy(file);
	down->file = xl_file;
	down->handle = h;
	down->ses = ses;
	down->remotetime = 0;
	add_to_list(downloads, down);
	load_url(url, NULL, &down->stat, PRI_DOWNLOAD, NC_CACHE, 1, ses->dn_allow_flags, down->last_pos);
	display_download(ses->term, down, ses);
}

void abort_all_downloads(void)
{
	while (!list_empty(downloads)) {
		struct download *down = list_struct(downloads.next, struct download);
		abort_download(down);
	}
}

int f_is_finished(struct f_data *f)
{
	struct additional_file *af;
	struct list_head *laf;
	if (!f || f->rq->state >= 0) return 0;
	if (f->fd && f->fd->rq && f->fd->rq->state >= 0) return 0;
	if (f->af) foreach(struct additional_file, af, laf, f->af->af) if (!af->rq || af->rq->state >= 0) return 0;
	return 1;
}

static int f_is_cacheable(struct f_data *f)
{
	if (!f || f->rq->state >= 0) return 0;
	if (f->fd && f->fd->rq && f->fd->rq->state >= 0) return 0;
	return 1;
}

static int f_need_reparse(struct f_data *f)
{
	struct additional_file *af;
	struct list_head *laf;
	if (!f || f->rq->state >= 0) return 1;
	if (f->af) foreach(struct additional_file, af, laf, f->af->af) if (af->need_reparse > 0) return 1;
	return 0;
}

static struct f_data *format_html(struct f_data_c *fd, struct object_request *rq, unsigned char *url, struct document_options *opt, int *cch)
{
	struct f_data *f;
	pr(
	if (cch) *cch = 0;
	if (!rq->ce || !(f = init_formatted(opt))) goto nul;
	f->fd = fd;
	f->ses = fd->ses;
	f->time_to_get = -get_time();
	clone_object(rq, &f->rq);
	if (f->rq->ce) {
		unsigned char *start; unsigned char *end;
		int stl = -1;
		struct additional_file *af;
		struct list_head *laf;

		if (fd->af) foreach(struct additional_file, af, laf, fd->af->af) if (af->need_reparse > 0) af->need_reparse = 0;

		get_file(rq, &start, &end);
		f->uncacheable = 1;
		if (opt->plain == 2) {
			start = init_str();
			stl = 0;
			add_to_str(&start, &stl, cast_uchar "<img src=\"");
			add_to_str(&start, &stl, f->rq->ce->url);
			add_to_str(&start, &stl, cast_uchar "\">");
			end = start + stl;
		}
		really_format_html(f->rq->ce, start, end, f, fd->ses ? fd != fd->ses->screen : 0);
		if (stl != -1)
			free(start);
		f->use_tag = f->rq->ce->count;
		if (f->af) foreach(struct additional_file, af, laf, f->af->af) {
			if (af->rq && af->rq->ce) {
				af->use_tag = af->rq->ce->count;
				af->use_tag2 = af->rq->ce->count2;
			} else {
				af->use_tag = 0;
				af->use_tag2 = 0;
			}
		}
	} else f->use_tag = 0;
	f->time_to_get += get_time();
	) nul:return NULL;
	return f;
}

static void count_frames(struct f_data_c *fd, unsigned long *i)
{
	struct f_data_c *sub;
	struct list_head *lsub;
	if (!fd) return;
	if (fd->f_data) (*i)++;
	foreach(struct f_data_c, sub, lsub, fd->subframes) count_frames(sub, i);
}

unsigned long formatted_info(int type)
{
	unsigned long i = 0;
	struct session *ses;
	struct list_head *lses;
	switch (type) {
		case CI_FILES:
			foreach(struct session, ses, lses, sessions)
				i += list_size(&ses->format_cache);
			/*-fallthrough*/
		case CI_LOCKED:
			foreach(struct session, ses, lses, sessions)
				count_frames(ses->screen, &i);
			return i;
		default:
			internal("formatted_info: bad request");
	}
	return 0;
}

static void f_data_attach(struct f_data_c *fd, struct f_data *f)
{
	struct additional_file *af;
	struct list_head *laf;
	f->rq->upcall = fd_loaded;
	f->rq->data = fd;
	free_additional_files(&fd->af);
	fd->af = f->af;
	if (f->af) {
		f->af->refcount++;
		foreachback(struct additional_file, af, laf, f->af->af) {
			if (af->rq) {
				af->rq->upcall = fd_loaded;
				af->rq->data = fd;
			} else {
				request_object(fd->ses->term, af->url, f->rq->url, PRI_IMG, NC_CACHE, get_allow_flags(f->rq->url), f->rq->upcall, f->rq->data, &af->rq);
			}
		}
	}
}

static inline int is_format_cache_entry_uptodate(struct f_data *f)
{
	struct cache_entry *ce = f->rq->ce;
	struct additional_file *af;
	struct list_head *laf;
	if (!ce || ce->count != f->use_tag) return 0;
	if (f->af) foreach(struct additional_file, af, laf, f->af->af) {
		struct cache_entry *ce = af->rq ? af->rq->ce : NULL;
		tcount tag = ce ? ce->count : 0;
		tcount tag2 = ce ? ce->count2 : 0;
		if (af->need_reparse > 0) if (tag != af->use_tag) return 0;
		if (af->unknown_image_size) if (tag2 != af->use_tag2) return 0;
	}
	return 1;
}

static void detach_f_data(struct f_data **ff)
{
	struct f_data *f = *ff;
	struct f_data_c *fd;
	if (!f) return;
	fd = f->fd;
	*ff = NULL;

	f->fd = NULL;
#ifdef G
	f->hlt_pos = -1;
	f->hlt_len = 0;
	f->start_highlight_x = -1;
	f->start_highlight_y = -1;
	f->locked_on = NULL;
	free_list(struct image_refresh, f->image_refresh);
#endif
	if (f->frame_desc_link || f->uncacheable || !f_is_cacheable(f) || !is_format_cache_entry_uptodate(f) || !f->ses) {
		destroy_formatted(f);
	} else {
		add_to_list(f->ses->format_cache, f);
		copy_additional_files(&fd->af);	/* break structure sharing */
	}
}

static int shrink_format_cache(int u)
{
	static int sc = 0;
	int scc;
	int r = 0;
	int c = 0;
	struct session *ses;
	struct list_head *lses;
	foreach(struct session, ses, lses, sessions) {
		struct f_data *f;
		struct list_head *lf;
		foreach(struct f_data, f, lf, ses->format_cache) {
			if (u == SH_FREE_ALL || !is_format_cache_entry_uptodate(f)) {
				lf = lf->prev;
				del_from_list(f);
				destroy_formatted(f);
				r |= ST_SOMETHING_FREED;
			} else c++;
		}
	}
	if (c > max_format_cache_entries || (c && u == SH_FREE_SOMETHING)) {
		int sc_cycle = 0;
		unsigned char freed_in_cycle = 0;
		a:
		scc = sc++;
		foreach (struct session, ses, lses, sessions) if (!scc--) {
			if (!list_empty(ses->format_cache)) {
				struct f_data *ff = list_struct(ses->format_cache.prev, struct f_data);
				del_from_list(ff);
				destroy_formatted(ff);
				r |= ST_SOMETHING_FREED;
				if (--c <= max_format_cache_entries ||
				    u == SH_FREE_SOMETHING) goto ret;
				freed_in_cycle = 1;
			}
			goto a;
		}
		sc = 0;
		sc_cycle++;
		if (sc_cycle >= 2 && !freed_in_cycle)
			goto ret;
		freed_in_cycle = 0;
		goto a;
	}
	ret:
	return r | (!c ? ST_CACHE_EMPTY : 0);
}

void init_fcache(void)
{
	register_cache_upcall(shrink_format_cache, MF_GPI, cast_uchar "format");
}

static void calculate_scrollbars(struct f_data_c *fd, struct f_data *f)
{
	fd->hsb = 0;
	fd->vsb = 0;
	fd->hsbsize = 0;
	fd->vsbsize = 0;
	if (!f)
		return;
	if (f->opt.scrolling == SCROLLING_YES) {
		fd->hsb = 1;
		fd->vsb = 1;
	} else if (f->opt.scrolling == SCROLLING_AUTO) {
		x:
		if (!fd->hsb && f->x > fd->xw - fd->vsb * G_SCROLL_BAR_WIDTH) {
			fd->hsb = 1;
			goto x;
		}
		if (!fd->vsb && f->y > fd->yw - fd->hsb * G_SCROLL_BAR_WIDTH) {
			fd->vsb = 1;
			goto x;
		}
	}
	if (fd->hsb) fd->hsbsize = fd->xw - fd->vsb * G_SCROLL_BAR_WIDTH;
	if (fd->vsb) fd->vsbsize = fd->yw - fd->hsb * G_SCROLL_BAR_WIDTH;
	if (fd->hsbsize < 0) fd->hsb = 0;
	if (fd->vsbsize < 0) fd->vsb = 0;
}

struct f_data *cached_format_html(struct f_data_c *fd, struct object_request *rq, unsigned char *url, struct document_options *opt, int *cch, int report_status)
{
	struct session *ses = fd->ses;
	struct f_data *f;
	struct list_head *lf;
	if (fd->marginwidth != -1) {
		int marg = (fd->marginwidth + G_HTML_MARGIN - 1) / G_HTML_MARGIN;
		if (marg >= 0 && marg < 9) opt->margin = marg;
	}
	if (opt->plain == 2) {
		opt->margin = 0;
		opt->display_images = 1;
	}
	pr(
	if (ses) {
		if (fd->f_data && !strcmp(cast_const_char fd->f_data->rq->url, cast_const_char url) && !compare_opt(&fd->f_data->opt, opt) && is_format_cache_entry_uptodate(fd->f_data)) {
			f = fd->f_data;
			goto ret_f;
		}
		foreach(struct f_data, f, lf, ses->format_cache) {
			if (!strcmp(cast_const_char f->rq->url, cast_const_char url) && !compare_opt(&f->opt, opt)) {
				if (!is_format_cache_entry_uptodate(f)) {
					lf = lf->prev;
					del_from_list(f);
					destroy_formatted(f);
					continue;
				}
				detach_f_data(&fd->f_data);
				del_from_list(f);
				f->fd = fd;
				if (cch) *cch = 1;
				f_data_attach(fd, f);
				goto ret_f;
			}
		}
	}) {};
	if (ses) {
		if (report_status || !fd->f_data || fd->f_data->time_to_get >= DISPLAY_FORMATTING_STATUS)
			print_progress(ses, TEXT_(T_FORMATTING_DOCUMENT));
	}
	detach_f_data(&fd->f_data);
	f = format_html(fd, rq, url, opt, cch);
	if (f) f->fd = fd;
	shrink_memory(SH_CHECK_QUOTA);
ret_f:
	calculate_scrollbars(fd, f);
	return f;
}

static void create_new_frames(struct f_data_c *fd, struct frameset_desc *fs, struct document_options *o)
{
	struct list_head *lloc;
	struct frame_desc *frm;
	int c_loc;
	int x, y;
	int xp, yp;

	if (list_size(&fd->loc->subframes) != (unsigned long)fs->n) {
		while (!list_empty(fd->loc->subframes))
			destroy_location(list_struct(fd->loc->subframes.next, struct location));
		c_loc = 1;
		lloc = NULL;	/* against warning */
	} else {
		c_loc = 0;
		lloc = fd->loc->subframes.next;
	}

	yp = fd->yp;
	frm = &fs->f[0];
	for (y = 0; y < fs->y; y++) {
		xp = fd->xp;
		for (x = 0; x < fs->x; x++) {
			struct f_data_c *nfdc;
			struct location *loc;
			nfdc = create_f_data_c(fd->ses, fd);
			if (c_loc) {
				loc = new_location();
				add_to_list_end(fd->loc->subframes, loc);
				loc->parent = fd->loc;
				loc->name = stracpy(frm->name);
				if ((loc->url = stracpy(frm->url)))
					nfdc->goto_position = extract_position(loc->url);
			} else {
				loc = list_struct(lloc, struct location);
			}
			nfdc->xp = xp; nfdc->yp = yp;
			nfdc->xw = frm->xw;
			nfdc->yw = frm->yw;
			nfdc->scrolling = frm->scrolling;
			nfdc->loc = loc;
			nfdc->vs = loc->vs;
			if (frm->marginwidth != -1) nfdc->marginwidth = frm->marginwidth;
			else nfdc->marginwidth = fd->marginwidth;
			if (frm->marginheight != -1) nfdc->marginheight = frm->marginheight;
			else nfdc->marginheight = fd->marginheight;
			add_to_list_end(fd->subframes, nfdc);
			if (frm->subframe) {
				create_new_frames(nfdc, frm->subframe, o);
				/*nfdc->f_data = init_formatted(&fd->f_data->opt);*/
				nfdc->f_data = init_formatted(o);
				nfdc->f_data->frame_desc = copy_frameset_desc(frm->subframe);
				nfdc->f_data->frame_desc_link = 1;
			} else {
				if (fd->depth < HTML_MAX_FRAME_DEPTH && loc->url && *loc->url) {
					struct f_data_c *rel = fd;
					while (rel->parent && !rel->rq) rel = rel->parent;
					request_object(fd->ses->term, loc->url, rel->rq ? rel->rq->url : NULL, PRI_FRAME, NC_CACHE, rel->rq ? get_allow_flags(rel->rq->url) : 0, fd_loaded, nfdc, &nfdc->rq);
				}
			}
			xp += frm->xw + gf_val(1, 0);
			frm++;
			if (!c_loc) lloc = lloc->next;
		}
		yp += (frm - 1)->yw + gf_val(1, 0);
	}
}

static void html_interpret(struct f_data_c *fd, int report_status)
{
	int i;
	int oxw; int oyw; int oxp; int oyp;
	struct f_data_c *sf;
	struct list_head *lsf;
	int cch;
	struct document_options o;
	if (!fd->loc) goto d;
	if (fd->f_data) {
		oxw = fd->f_data->opt.xw;
		oyw = fd->f_data->opt.yw;
		oxp = fd->f_data->opt.xp;
		oyp = fd->f_data->opt.yp;
	} else {
		oxw = oyw = oxp = oyp = -1;
	}
	memset(&o, 0, sizeof(struct document_options));
	ds2do(&fd->ses->ds, &o, F || fd->ses->term->spec->col);
	if (!casecmp(fd->loc->url, cast_uchar "file://", 7) && !o.hard_assume) {
		o.assume_cp = term_charset(fd->ses->term);
	}
	if (fd->parent && fd->parent->f_data && !o.hard_assume) {
		o.assume_cp = fd->parent->f_data->cp;
	}
	o.js_enable=0;
#ifdef G
	o.bfu_aspect=bfu_aspect;
#else
	o.bfu_aspect=0;
#endif
	o.plain = fd->vs->plain;
	if (o.plain == 1 && !o.break_long_lines) {
		o.xp = 0;
		o.yp = 0;
		o.xw = MAXINT;
		o.yw = MAXINT;
	} else {
		o.xp = fd->xp;
		o.yp = fd->yp;
		o.xw = fd->xw;
		o.yw = fd->yw;
	}
	o.scrolling = fd->scrolling;
	if (fd->ses->term->spec) {
		if (!F) {
			if (!fd->ses->ds.t_ignore_document_color)
				o.col = fd->ses->term->spec->col;
			else
				o.col = 0;
		}
#ifdef G
		else {
			if (!fd->ses->ds.g_ignore_document_color)
				o.col = 2;
			else
				o.col = 0;
		}
#endif
		o.cp = term_charset(fd->ses->term);
	} else {
		o.col = 3;
		o.cp = 0;
	}
	if (!(o.framename = fd->loc->name)) o.framename = NULL;
	if (!(fd->f_data = cached_format_html(fd, fd->rq, fd->rq->url, &o, &cch, report_status))) {
		goto d;
	}

	/* erase frames if changed */
	i = (int)list_size(&fd->subframes);
	if (i != (fd->f_data->frame_desc ? fd->f_data->frame_desc->n : 0) && (f_is_finished(fd->f_data) || !f_need_reparse(fd->f_data))) {
		rd:
		foreach(struct f_data_c, sf, lsf, fd->subframes) reinit_f_data_c(sf);
		free_list(struct f_data_c, fd->subframes);

		/* create new frames */
		if (fd->f_data->frame_desc) create_new_frames(fd, fd->f_data->frame_desc, &fd->f_data->opt);
	} else {
		if (fd->f_data->frame_desc && fd->f_data->rq->state < 0) {
			if (fd->f_data->opt.xw != oxw ||
			    fd->f_data->opt.yw != oyw ||
			    fd->f_data->opt.xp != oxp ||
			    fd->f_data->opt.yp != oyp) goto rd;
		}
	}

	d:;
}

void html_interpret_recursive(struct f_data_c *f)
{
	struct f_data_c *fd;
	struct list_head *lfd;
	if (f->rq) html_interpret(f, 1);
	foreach(struct f_data_c, fd, lfd, f->subframes) html_interpret_recursive(fd);
}

/* You get a struct_additionl_file. never mem_free it. When you stop
 * using it, just forget the pointer.
 */
struct additional_file *request_additional_file(struct f_data *f, unsigned char *url_)
{
	size_t sl;
	struct additional_file *af;
	struct list_head *laf;
	unsigned char *url;
	url = stracpy(url_);
	extract_position(url);
	if (!f->af) {
		if (!(f->af = f->fd->af)) {
			f->af = f->fd->af = xmalloc(sizeof(struct additional_files));
			f->af->refcount = 1;
			init_list(f->af->af);
		}
		f->af->refcount++;
	}
	foreach(struct additional_file, af, laf, f->af->af) if (!strcmp(cast_const_char af->url, cast_const_char url)) {
		free(url);
		return af;
	}
	sl = strlen(cast_const_char url);
	if (sl > MAXINT - sizeof(struct additional_file)) overalloc();
	af = xmalloc(sizeof(struct additional_file) + sl);
	af->use_tag = 0;
	af->use_tag2 = 0;
	strcpy(cast_char af->url, cast_const_char url);
	if (!strcmp(cast_const_char url, cast_const_char f->rq->url))
		clone_object(f->rq, &af->rq);
	else
		request_object(f->ses->term, url, f->rq->url, PRI_IMG, NC_CACHE, get_allow_flags(f->rq->url), f->rq->upcall, f->rq->data, &af->rq);
	af->need_reparse = 0;
	af->unknown_image_size = 0;
	add_to_list(f->af->af, af);
	free(url);
	return af;
}

static void copy_additional_files(struct additional_files **a)
{
	struct additional_files *afs;
	struct additional_file *af;
	struct list_head *laf;
	if (!*a || (*a)->refcount == 1) return;
	(*a)->refcount--;
	afs = xmalloc(sizeof(struct additional_files));
	afs->refcount = 1;
	init_list(afs->af);
	foreachback(struct additional_file, af, laf, (*a)->af) {
		struct additional_file *afc;
		size_t sl = strlen(cast_const_char af->url);
		if (sl > MAXINT - sizeof(struct additional_file)) overalloc();
		afc = xmalloc(sizeof(struct additional_file) + sl);
		memcpy(afc, af, sizeof(struct additional_file) + sl);
		if (af->rq) clone_object(af->rq, &afc->rq);
		add_to_list(afs->af, afc);
	}
	*a = afs;
}

#ifdef G

static void image_timer(void *fd_)
{
	struct f_data_c *fd = (struct f_data_c *)fd_;
	uttime now;
	struct image_refresh *ir;
	struct list_head *lir;
	struct list_head neww;
	init_list(neww);
	fd->image_timer = NULL;
	if (!fd->f_data) return;
	now = get_time();
	foreach(struct image_refresh, ir, lir, fd->f_data->image_refresh) {
		if (now - ir->start >= ir->tim) {
			lir = lir->prev;
			del_from_list(ir);
			add_to_list(neww, ir);
		}
	}
	foreach(struct image_refresh, ir, lir, neww) {
		draw_one_object(fd, ir->img);
	}
	free_list(struct image_refresh, neww);
	if (fd->image_timer == NULL && !list_empty(fd->f_data->image_refresh)) fd->image_timer = install_timer(G_IMG_REFRESH, image_timer, fd);
}

void refresh_image(struct f_data_c *fd, struct g_object *img, uttime tm)
{
	struct image_refresh *ir;
	struct list_head *lir;
	uttime now, e;
	if (!fd->f_data) return;
	now = get_time();
	e = now + tm;
	foreach(struct image_refresh, ir, lir, fd->f_data->image_refresh) if (ir->img == img) {
		if (e - ir->start < ir->tim) {
			ir->tim = tm;
			ir->start = now;
		}
		return;
	}
	ir = xmalloc(sizeof(struct image_refresh));
	ir->img = img;
	ir->tim = tm;
	ir->start = now;
	add_to_list(fd->f_data->image_refresh, ir);
	if (fd->image_timer == NULL) fd->image_timer = install_timer(!tm ? 0 : G_IMG_REFRESH, image_timer, fd);
}

#endif

void reinit_f_data_c(struct f_data_c *fd)
{
	struct additional_file *af;
	struct list_head *laf;
	struct f_data_c *fd1;
	struct list_head *lfd1;
#ifdef G
	if (F)
		if (fd == current_frame(fd->ses))
			fd->ses->locked_link = 0;
#endif
	foreach(struct f_data_c, fd1, lfd1, fd->subframes) {
		if (fd->ses->wtd_target_base == fd1) fd->ses->wtd_target_base = NULL;
		reinit_f_data_c(fd1);
		if (fd->ses->wtd_target_base == fd1) fd->ses->wtd_target_base = fd;
	}
	free_list(struct f_data_c, fd->subframes);
	fd->loc = NULL;
	if (fd->f_data && fd->f_data->rq) fd->f_data->rq->upcall = NULL;
	if (fd->f_data && fd->f_data->af) foreach(struct additional_file, af, laf, fd->f_data->af->af) if (af->rq) {
		af->rq->upcall = NULL;
		if (af->rq->state != O_OK) release_object(&af->rq);
	}
	if (fd->af) foreach(struct additional_file, af, laf, fd->af->af) if (af->rq) af->rq->upcall = NULL;
	free_additional_files(&fd->af);
	detach_f_data(&fd->f_data);
	release_object(&fd->rq);
	free(fd->link_bg);
	fd->link_bg = NULL;
	fd->link_bg_n = 0;
	free(fd->goto_position);
	fd->goto_position = NULL;
	free(fd->went_to_position);
	fd->went_to_position = NULL;
	fd->last_update = get_time();
	fd->next_update_interval = 0;
	fd->done = 0;
	fd->parsed_done = 0;
	if (fd->image_timer != NULL) {
		kill_timer(fd->image_timer);
		fd->image_timer = NULL;
	}
	if (fd->refresh_timer != NULL) {
		kill_timer(fd->refresh_timer);
		fd->refresh_timer = NULL;
	}
}

struct f_data_c *create_f_data_c(struct session *ses, struct f_data_c *parent)
{
	static long id = 1;
	struct f_data_c *fd;
	fd = mem_calloc(sizeof(struct f_data_c));
	fd->parent = parent;
	fd->ses = ses;
	fd->depth = parent ? parent->depth + 1 : 1;
	init_list(fd->subframes);
	fd->last_update = get_time();
	fd->next_update_interval = 0;
	fd->done = 0;
	fd->parsed_done = 0;
	fd->script_t = 0;
	fd->id = id++;
	fd->marginwidth = fd->marginheight = -1;
	fd->image_timer = NULL;
	fd->refresh_timer = NULL;
	fd->scrolling = SCROLLING_AUTO;
	return fd;
}

int f_data_c_allow_flags(struct f_data_c *fd)
{
	if (fd->rq) return get_allow_flags(fd->rq->url);
	return 0;
}

static int plain_type(struct session *ses, struct object_request *rq, unsigned char **p)
{
	struct cache_entry *ce;
	unsigned char *ct;
	int r = 0;
	if (p) *p = NULL;
	if (!rq || !(ce = rq->ce)) {
		r = 1;
		goto f;
	}
	if (!(ct = get_content_type(ce->head, ce->url))) goto f;
	if (is_html_type(ct)) goto ff;
	r = 1;
	if (!casestrcmp(ct, cast_uchar "text/plain") ||
	    !casestrcmp(ct, cast_uchar "file/txt")) goto ff;
	r = 2;
	if (F && known_image_type(ct)) goto ff;
	r = -1;

	ff:
	if (!p)
		free(ct);
	else *p = ct;
	f:
	return r;
}

static void refresh_timer(void *fd_)
{
	struct f_data_c *fd = (struct f_data_c *)fd_;
	if (fd->ses->rq) {
		fd->refresh_timer = install_timer(500, refresh_timer, fd);
		return;
	}
	fd->refresh_timer = NULL;
	if (fd->f_data && fd->f_data->refresh) {
		fd->refresh_timer = install_timer(fd->f_data->refresh_seconds * 1000, refresh_timer, fd);
		goto_url_f(fd->ses, NULL, fd->f_data->refresh, cast_uchar "_self", fd, -1, 0, 0, 1);
	}
}

void fd_loaded(struct object_request *rq, void *fd_)
{
	struct f_data_c *fd = (struct f_data_c *)fd_;
	int first = !fd->f_data;
	if (fd->done) {
		if (f_is_finished(fd->f_data)) goto priint;
		else {
			fd->done = 0;
			fd->parsed_done = 1;
		}
	}
	if (fd->parsed_done && f_need_reparse(fd->f_data)) fd->parsed_done = 0;
	if (fd->vs->plain == -1 && rq->state != O_WAITING) {
		fd->vs->plain = plain_type(fd->ses, fd->rq, NULL);
	}
	if (fd->rq->state < 0 && (f_is_finished(fd->f_data) || !fd->f_data)) {
		if (!fd->parsed_done) {
			html_interpret(fd, 1);
			if (fd->went_to_position) {
				if (!fd->goto_position) {
					fd->goto_position = fd->went_to_position;
					fd->went_to_position = NULL;
				} else {
					free(fd->went_to_position);
					fd->went_to_position = NULL;
				}
			}
		}
		draw_fd(fd);
		/* it may happen that html_interpret requests load of additional file */
		if (!f_is_finished(fd->f_data)) goto more_data;
fn:
		fd->done = 1;
		fd->parsed_done = 0;
		if (fd->f_data->refresh) {
			if (fd->refresh_timer != NULL) kill_timer(fd->refresh_timer);
			fd->refresh_timer = install_timer(fd->f_data->refresh_seconds * 1000, refresh_timer, fd);
		}
	} else if (get_time() - fd->last_update >= fd->next_update_interval || (rq == fd->rq && rq->state < 0)) {
		uttime t;
		if (!fd->parsed_done) {
			html_interpret(fd, rq == fd->rq && rq->state < 0);
			if (fd->rq->state < 0 && !f_need_reparse(fd->f_data)) {
				if (fd->went_to_position) {
					if (!fd->goto_position) {
						fd->goto_position = fd->went_to_position;
						fd->went_to_position = NULL;
					} else {
						free(fd->went_to_position);
						fd->went_to_position = NULL;
					}
				}
				fd->parsed_done = 1;
			}
		}
		draw_fd(fd);
		if (fd->rq->state < 0 && f_is_finished(fd->f_data)) goto fn;
more_data:
		t = fd->f_data ? ((fd->parsed_done ? 0 : fd->f_data->time_to_get * DISPLAY_TIME) + fd->f_data->time_to_draw * IMG_DISPLAY_TIME) : 0;
		if (t < DISPLAY_TIME_MIN) t = DISPLAY_TIME_MIN;
		if (first && t > DISPLAY_TIME_MAX_FIRST) t = DISPLAY_TIME_MAX_FIRST;
		fd->last_update = get_time();
		fd->next_update_interval = t;
	} else {
		change_screen_status(fd->ses);
		print_screen_status(fd->ses);
	}
priint:
	/* process onload handler of a frameset */
	if (rq && (rq->state == O_FAILED || rq->state == O_INCOMPLETE) && (fd->rq == rq || fd->ses->rq == rq) && !rq->dont_print_error) print_error_dialog(fd->ses, &rq->stat, rq->url);
#ifdef LINKS_TESTMODE_DOCUMENT_AUTO_EXIT
	if (f_is_finished(fd->f_data))
		terminate_loop = 1;
#endif
}

static unsigned location_id = 0;

static struct location *new_location(void)
{
	struct location *loc;
	loc = mem_calloc(sizeof(struct location));
	loc->location_id = ++location_id;
	init_list(loc->subframes);
	loc->vs = create_vs();
	return loc;
}

static struct location *alloc_ses_location(struct session *ses)
{
	struct location *loc;
	loc = new_location();
	add_to_list(ses->history, loc);
	return loc;
}

static void subst_location(struct f_data_c *fd, struct location *old, struct location *neww)
{
	struct f_data_c *f;
	struct list_head *lf;
	foreach(struct f_data_c, f, lf, fd->subframes) subst_location(f, old, neww);
	if (fd->loc == old) fd->loc = neww;
}

static struct location *copy_sublocations(struct session *ses, struct location *d, struct location *s, struct location *x)
{
	struct location *sl, *y;
	struct list_head *lsl;
	d->name = stracpy(s->name);
	if (s == x) return d;
	d->url = stracpy(s->url);
	d->prev_url = stracpy(s->prev_url);
	destroy_vs(d->vs);
	d->vs = s->vs; s->vs->refcount++;
	subst_location(ses->screen, s, d);
	y = NULL;
	foreach(struct location, sl, lsl, s->subframes) {
		struct location *dl, *z;

		dl = new_location();
		add_to_list_end(d->subframes, dl);
		dl->parent = d;
		z = copy_sublocations(ses, dl, sl, x);
		if (z && y) internal("copy_sublocations: crossed references");
		if (z) y = z;
	}
	return y;
}

static struct location *copy_location(struct session *ses, struct location *loc)
{
	struct location *l2, *l1, *nl;
	l1 = cur_loc(ses);
	l2 = alloc_ses_location(ses);
	if (!(nl = copy_sublocations(ses, l2, l1, loc))) internal("copy_location: sublocation not found");
	return nl;
}

static struct f_data_c *new_main_location(struct session *ses)
{
	struct location *loc;
	loc = alloc_ses_location(ses);
	reinit_f_data_c(ses->screen);
	ses->screen->loc = loc;
	ses->screen->vs = loc->vs;
	if (ses->wanted_framename) {
		loc->name=ses->wanted_framename;
		ses->wanted_framename=NULL;
	}
	return ses->screen;
}

static struct f_data_c *copy_location_and_replace_frame(struct session *ses, struct f_data_c *fd)
{
	struct location *loc;
	loc = copy_location(ses, fd->loc);
	reinit_f_data_c(fd);
	fd->loc = loc;
	fd->vs = loc->vs;
	return fd;
}

/* vrati frame prislusici danemu targetu
   pokud takovy frame nenajde, vraci NULL
 */
struct f_data_c *find_frame(struct session *ses, unsigned char *target, struct f_data_c *base)
{
	struct f_data_c *f, *ff;
	struct list_head *lff;
	if (!base) base = ses->screen;
	if (!target || !*target) return base;
	if (!casestrcmp(target, cast_uchar "_blank"))
		return NULL;  /* open in new window */
	if (!casestrcmp(target, cast_uchar "_top"))
		return ses->screen;
	if (!casestrcmp(target, cast_uchar "_self")) return base;
	if (!casestrcmp(target, cast_uchar "_parent")) {
		for (ff = base->parent; ff && !ff->rq; ff = ff->parent)
			;
		return ff ? ff : ses->screen;
	}
	f = ses->screen;
	if (f->loc && f->loc->name && !casestrcmp(f->loc->name, target)) return f;
	d:
	foreach(struct f_data_c, ff, lff, f->subframes) if (ff->loc && ff->loc->name && !casestrcmp(ff->loc->name, target)) return ff;
	if (!list_empty(f->subframes)) {
		f = list_struct(f->subframes.next, struct f_data_c);
		goto d;
	}
	u:
	if (!f->parent) return NULL;
	if (f->list_entry.next == &f->parent->subframes) {
		f = f->parent;
		goto u;
	}
	f = list_struct(f->list_entry.next, struct f_data_c);
	goto d;
}

static void destroy_location(struct location *loc)
{
	while (!list_empty(loc->subframes))
		destroy_location(list_struct(loc->subframes.next, struct location));
	del_from_list(loc);
	free(loc->name);
	free(loc->url);
	free(loc->prev_url);
	destroy_vs(loc->vs);
	free(loc);
}

static void clear_forward_history(struct session *ses)
{
	while (!list_empty(ses->forward_history))
		destroy_location(list_struct(ses->forward_history.next, struct location));
}

static void ses_go_forward(struct session *ses, int plain, int refresh)
{
	struct location *cl;
	struct f_data_c *fd;
	clear_forward_history(ses);
	free(ses->search_word);
	ses->search_word = NULL;
	free(ses->default_status);
	ses->default_status = NULL;	/* smazeme default status, aby neopruzoval na jinych strankach */
	if ((fd = find_frame(ses, ses->wtd_target, ses->wtd_target_base))&&fd!=ses->screen) {
		cl = NULL;
		if (refresh && fd->loc && !strcmp(cast_const_char fd->loc->url, cast_const_char ses->rq->url)) cl = cur_loc(ses);
		fd = copy_location_and_replace_frame(ses, fd);
		if (cl) destroy_location(cl);
	} else fd = new_main_location(ses);
	fd->vs->plain = plain;
	ses->wtd = NULL;
	fd->rq = ses->rq; ses->rq = NULL;
	fd->goto_position = ses->goto_position; ses->goto_position = NULL;
	fd->loc->url = stracpy(fd->rq->url);
	fd->loc->prev_url = stracpy(fd->rq->prev_url);
	fd->rq->upcall = fd_loaded;
	fd->rq->data = fd;
	fd->rq->upcall(fd->rq, fd);
	draw_formatted(ses);
}

static void ses_go_backward(struct session *ses)
{
	int n;
	struct location *loc;
	struct list_head *lloc;
	free(ses->search_word);
	ses->search_word = NULL;
	free(ses->default_status);
	ses->default_status = NULL;	/* smazeme default status, aby neopruzoval na jinych strankach */
	reinit_f_data_c(ses->screen);
	if (!ses->wtd_num_steps) internal("ses_go_backward: wtd_num_steps is zero");
	if (ses->wtd_num_steps > 0) {
		n = ses->wtd_num_steps;
		foreach(struct location, loc, lloc, ses->history) {
			if (!n--) goto have_back_loc;
		}
		internal("ses_go_backward: session history disappeared");
		return;
		have_back_loc:
		for (n = 0; n < ses->wtd_num_steps; n++) {
			loc = cur_loc(ses);
			del_from_list(loc);
			add_to_list(ses->forward_history, loc);
		}
	} else {
		n = ses->wtd_num_steps;
		foreach(struct location, loc, lloc, ses->forward_history) {
			if (!++n) goto have_fwd_loc;
		}
		internal("ses_go_backward: session forward history disappeared");
		return;
		have_fwd_loc:
		for (n = 0; n < -ses->wtd_num_steps; n++) {
			loc = list_struct(ses->forward_history.next, struct location);
			del_from_list(loc);
			add_to_list(ses->history, loc);
		}
	}
#ifdef G
	ses->locked_link = 0;
#endif
	ses->screen->loc = cur_loc(ses);
	ses->screen->vs = ses->screen->loc->vs;
	ses->wtd = NULL;
	ses->screen->rq = ses->rq; ses->rq = NULL;
	ses->screen->rq->upcall = fd_loaded;
	ses->screen->rq->data = ses->screen;
	ses->screen->rq->upcall(ses->screen->rq, ses->screen);

}

static void tp_cancel(void *ses_)
{
	struct session *ses = (struct session *)ses_;
	release_object(&ses->tq);
}

static void continue_download(struct session *ses, unsigned char *file, int mode)
{
	struct download *down;
	int h;
	int namecount = 0;
	unsigned char *url = ses->tq->url;
	unsigned char *xl_file;
	off_t last_pos = 0, file_shift = 0;

	if (ses->tq_prog) {
		if (ses->tq_prog_flag_direct && ses->tq->state != O_OK && !strchr(cast_const_char url, POST_CHAR) && !check_shell_url(url)) {
			unsigned char *prog = subst_file(ses->tq_prog, url, 0);
			exec_on_terminal(ses->term, prog, cast_uchar "", !!ses->tq_prog_flag_block);
			free(prog);
			tp_cancel(ses);
			abort_background_connections();
			return;
		}
		new_name:
		if (!(file = get_temp_name(url, ses->tq->ce ? ses->tq->ce->head : NULL))) {
			tp_cancel(ses);
			return;
		}
		if ((h = create_download_file(ses, ses->term->cwd, file, CDF_RESTRICT_PERMISSION | CDF_EXCL | CDF_NO_POPUP_ON_EEXIST, 0)) < 0) {
			if (h == -2 && ses->tq_prog) {
				if (++namecount < DOWNLOAD_NAME_TRIES) {
					free(file);
					goto new_name;
				}
				msg_box(ses->term, NULL, TEXT_(T_DOWNLOAD_ERROR), AL_CENTER, TEXT_(T_COULD_NOT_CREATE_TEMPORARY_FILE), MSG_BOX_END, NULL, 1, TEXT_(T_CANCEL), msg_box_null, B_ENTER | B_ESC);
			}
			free(file);
			tp_cancel(ses);
			return;
		}
		xl_file = stracpy(file);
	} else {
		if (create_or_append_download_file(ses, ses->term->cwd, file, mode, &h, &xl_file, &last_pos, &file_shift)) {
			tp_cancel(ses);
			return;
		}
	}
	down = mem_calloc(sizeof(struct download));
	down->url = stracpy(url);
	down->stat.end = download_data;
	down->stat.data = down;
	down->decompress = 0;
	down->last_pos = last_pos;
	down->file_shift = file_shift;
	down->cwd = stracpy(ses->term->cwd);
	down->orig_file = stracpy(file);
	down->file = xl_file;
	down->handle = h;
	down->ses = ses;
	down->remotetime = 0;
	if (ses->tq_prog) {
		down->prog = subst_file(ses->tq_prog, file, 1);
		free(file);
		free(ses->tq_prog);
		ses->tq_prog = NULL;
	}
	down->prog_flag_block = ses->tq_prog_flag_block;
	add_to_list(downloads, down);
	release_object_get_stat(&ses->tq, &down->stat, PRI_DOWNLOAD);
	display_download(ses->term, down, ses);
}


static void tp_save(void *ses_)
{
	struct session *ses = (struct session *)ses_;
	free(ses->tq_prog);
	ses->tq_prog = NULL;
	query_file(ses, ses->tq->url, ses->tq->ce ? ses->tq->ce->head : NULL, &continue_download, tp_cancel, DOWNLOAD_CONTINUE);
}

static void tp_open(void *ses_)
{
	struct session *ses = (struct session *)ses_;
	continue_download(ses, cast_uchar "", DOWNLOAD_DEFAULT);
}

static int ses_abort_1st_state_loading(struct session *ses)
{
	int r = !!ses->rq;
	release_object(&ses->rq);
	ses->wtd = NULL;
	free(ses->wtd_target);
	ses->wtd_target = NULL;
	ses->wtd_target_base = NULL;
	free(ses->goto_position);
	ses->goto_position = NULL;
	change_screen_status(ses);
	print_screen_status(ses);
	return r;
}

static void tp_display(void *ses_)
{
	struct session *ses = (struct session *)ses_;
	ses_abort_1st_state_loading(ses);
	ses->rq = ses->tq;
	ses->tq = NULL;
	ses_go_forward(ses, 1, 0);
}

static int direct_download_possible(struct object_request *rq, struct assoc *a)
{
	unsigned char *proto = get_protocol_name(rq->url);
	int ret = 0;
	if (!proto) return 0;
	if (a->accept_http && !casestrcmp(proto, cast_uchar "http")) ret = 1;
	if (a->accept_ftp && !casestrcmp(proto, cast_uchar "ftp")) ret = 1;
	free(proto);
	if (proxies.only_proxies) ret = 0;
	return ret;
}

static int prog_sel_save(struct dialog_data *dlg, struct dialog_item_data *idata)
{
	struct session *ses = (struct session *)dlg->dlg->udata2;

	tp_save(ses);

	cancel_dialog(dlg, idata);
	return 0;
}

static int prog_sel_display(struct dialog_data *dlg, struct dialog_item_data *idata)
{
	struct session *ses = (struct session *)dlg->dlg->udata2;

	tp_display(ses);

	cancel_dialog(dlg, idata);
	return 0;
}

static int prog_sel_cancel(struct dialog_data *dlg, struct dialog_item_data *idata)
{
	struct session *ses = (struct session *)dlg->dlg->udata2;

	tp_cancel(ses);

	cancel_dialog(dlg, idata);
	return 0;
}

static int prog_sel_open(struct dialog_data *dlg, struct dialog_item_data *idata)
{
	struct assoc *a = (struct assoc*)idata->item->udata;
	struct session *ses = (struct session *)dlg->dlg->udata2;

	if (!a) internal("This should not happen.\n");
	ses->tq_prog = stracpy(a->prog);
	ses->tq_prog_flag_block = a->block;
	ses->tq_prog_flag_direct = direct_download_possible(ses->tq, a);
	tp_open(ses);
	cancel_dialog(dlg,idata);
	return 0;
}

static void type_query_multiple_programs(struct session *ses, unsigned char *ct, struct assoc *a, int n)
{
	int i;
	struct dialog *d;
	struct memory_list *ml;
	unsigned char **text_array;

	text_array = xmalloc(6 * sizeof(unsigned char *));
	text_array[0] = TEXT_(T_CONTENT_TYPE_IS);
	text_array[1] = cast_uchar " ";
	text_array[2] = ct;
	text_array[3] = cast_uchar ".\n";
	text_array[4] = !anonymous ? TEXT_(T_DO_YOU_WANT_TO_OPEN_SAVE_OR_DISPLAY_THIS_FILE) : TEXT_(T_DO_YOU_WANT_TO_OPEN_OR_DISPLAY_THIS_FILE);
	text_array[5] = NULL;

	if ((unsigned)n > (MAXINT - sizeof(struct dialog)) / sizeof(struct dialog_item) - 4) overalloc();
	d = mem_calloc(sizeof(struct dialog) + (n + 2 + (!anonymous)) * sizeof(struct dialog_item));
	d->title = TEXT_(T_WHAT_TO_DO);
	d->fn = msg_box_fn;
	d->udata = text_array;
	d->udata2 = ses;
	d->align = AL_CENTER;
	ml = getml(d, a, ct, text_array, NULL);

	for (i = 0; i < n; i++) {
		unsigned char *bla = stracpy(get_text_translation(TEXT_(T_OPEN_WITH),ses->term));
		add_to_strn(&bla, cast_uchar " ");
		add_to_strn(&bla, a[i].label);

		d->items[i].type = D_BUTTON;
		d->items[i].fn = prog_sel_open;
		d->items[i].udata = a + i;
		d->items[i].text = bla;
		a[i].prog = stracpy(a[i].prog);
		add_to_ml(&ml, bla, a[i].prog, NULL);
	}
	if (!anonymous) {
		d->items[i].type = D_BUTTON;
		d->items[i].fn = prog_sel_save;
		d->items[i].text = TEXT_(T_SAVE);
		i++;
	}
	d->items[i].type = D_BUTTON;
	d->items[i].fn = prog_sel_display;
	d->items[i].text = TEXT_(T_DISPLAY);
	i++;
	d->items[i].type = D_BUTTON;
	d->items[i].fn = prog_sel_cancel;
	d->items[i].gid = B_ESC;
	d->items[i].text = TEXT_(T_CANCEL);
	i++;
	d->items[i].type = D_END;
	do_dialog(ses->term, d, ml);
}



/* deallocates a */
static void type_query(struct session *ses, unsigned char *ct, struct assoc *a, int n)
{
	unsigned char *m1;
	unsigned char *m2;
	if (!ct) ct = stracpy(cast_uchar "unknown");
	free(ses->tq_prog);
	ses->tq_prog = NULL;

	if (n > 1) {
		type_query_multiple_programs(ses, ct, a, n);
		return;
	}

	if (a) {
		ses->tq_prog = stracpy(a[0].prog);
		ses->tq_prog_flag_block = a[0].block;
		ses->tq_prog_flag_direct = direct_download_possible(ses->tq, a);
	}
	if (a && !a[0].ask) {
		tp_open(ses);
		if (n)
			free(a);
		free(ct);
		return;
	}
	m1 = stracpy(ct);
	if (!a) {
		if (!anonymous) msg_box(ses->term, getml(m1, NULL), TEXT_(T_UNKNOWN_TYPE), AL_CENTER, TEXT_(T_CONTENT_TYPE_IS), cast_uchar " ", m1, cast_uchar ".\n", TEXT_(T_DO_YOU_WANT_TO_SAVE_OR_DISLPAY_THIS_FILE), MSG_BOX_END, (void *)ses, 3, TEXT_(T_SAVE), tp_save, B_ENTER, TEXT_(T_DISPLAY), tp_display, 0, TEXT_(T_CANCEL), tp_cancel, B_ESC);
		else msg_box(ses->term, getml(m1, NULL), TEXT_(T_UNKNOWN_TYPE), AL_CENTER, TEXT_(T_CONTENT_TYPE_IS), cast_uchar " ", m1, cast_uchar ".\n", TEXT_(T_DO_YOU_WANT_TO_SAVE_OR_DISLPAY_THIS_FILE), MSG_BOX_END, (void *)ses, 2, TEXT_(T_DISPLAY), tp_display, B_ENTER, TEXT_(T_CANCEL), tp_cancel, B_ESC);
	} else {
		m2 = stracpy(a[0].label ? a[0].label : (unsigned char *)"");
		if (!anonymous) msg_box(ses->term, getml(m1, m2, NULL), TEXT_(T_WHAT_TO_DO), AL_CENTER, TEXT_(T_CONTENT_TYPE_IS), cast_uchar " ", m1, cast_uchar ".\n", TEXT_(T_DO_YOU_WANT_TO_OPEN_FILE_WITH), cast_uchar " ", m2, cast_uchar ", ", TEXT_(T_SAVE_IT_OR_DISPLAY_IT), MSG_BOX_END, (void *)ses, 4, TEXT_(T_OPEN), tp_open, B_ENTER, TEXT_(T_SAVE), tp_save, 0, TEXT_(T_DISPLAY), tp_display, 0, TEXT_(T_CANCEL), tp_cancel, B_ESC);
		else msg_box(ses->term, getml(m1, m2, NULL), TEXT_(T_WHAT_TO_DO), AL_CENTER, TEXT_(T_CONTENT_TYPE_IS), cast_uchar " ", m1, cast_uchar ".\n", TEXT_(T_DO_YOU_WANT_TO_OPEN_FILE_WITH), cast_uchar " ", m2, cast_uchar ", ", TEXT_(T_SAVE_IT_OR_DISPLAY_IT), MSG_BOX_END, (void *)ses, 3, TEXT_(T_OPEN), tp_open, B_ENTER, TEXT_(T_DISPLAY), tp_display, 0, TEXT_(T_CANCEL), tp_cancel, B_ESC);
	}
	if (n)
		free(a);
	free(ct);
}

static void ses_go_to_2nd_state(struct session *ses)
{
	struct assoc *a;
	int n;
	unsigned char *ct = NULL;
	int r = plain_type(ses, ses->rq, &ct);
	if (r == 0 || r == 1 || r == 2 || r == 3) goto go;
	if (!(a = get_type_assoc(ses->term, ct, &n)) && strlen(cast_const_char ct) >= 4 && !casecmp(ct, cast_uchar "text", 4)) {
		r = 1;
		goto go;
	}
	if (ses->tq) {
		ses_abort_1st_state_loading(ses);
		free(a);
		free(ct);
		return;
	}
	(ses->tq = ses->rq)->upcall = NULL;
	ses->rq = NULL;
	ses_abort_1st_state_loading(ses);
	if (!a && ses->tq->ce && ses->tq->ce->head) {
		unsigned char *file = get_filename_from_header(ses->tq->ce->head);
		if (file) {
			unsigned char *new_ct = get_content_type_by_extension(file);
			free(file);
			if (new_ct) {
				a = get_type_assoc(ses->term, new_ct, &n);
				free(new_ct);
			}
		}
	}
	type_query(ses, ct, a, n);
	return;
	go:
	ses_go_forward(ses, r, ses->wtd_refresh);
	free(ct);
}

static void ses_go_back_to_2nd_state(struct session *ses)
{
	ses_go_backward(ses);
}

static void ses_finished_1st_state(struct object_request *rq, void *ses_)
{
	struct session *ses = (struct session *)ses_;
	if (rq->state != O_WAITING) {
		if (ses->wtd_refresh && ses->wtd_target_base && ses->wtd_target_base->refresh_timer != NULL) {
			kill_timer(ses->wtd_target_base->refresh_timer);
			ses->wtd_target_base->refresh_timer = NULL;
		}
	}
	switch (rq->state) {
		case O_WAITING:
			change_screen_status(ses);
			print_screen_status(ses);
			break;
		case O_FAILED:
			if (!rq->dont_print_error)
				print_error_dialog(ses, &rq->stat, rq->url);
			ses_abort_1st_state_loading(ses);
			break;
		case O_LOADING:
		case O_INCOMPLETE:
		case O_OK:
			if (!ses->goto_position && rq->goto_position) ses->goto_position = stracpy(rq->goto_position);
			ses->wtd(ses);
			break;
	}
}

void ses_destroy_defered_jump(struct session *ses)
{
}

/* if from_goto_dialog is 1, set prev_url to NULL */
void goto_url_f(struct session *ses, void (*state2)(struct session *), unsigned char *url, unsigned char *target, struct f_data_c *df, int data, int defer, int from_goto_dialog, int refresh)
{
	unsigned char *u, *pos;
	unsigned char *prev_url;
	void (*fn)(struct session *, unsigned char *);
	int reloadlevel, allow_flags;
	if (!state2) state2 = ses_go_to_2nd_state;
	ses_destroy_defered_jump(ses);
	if ((fn = get_external_protocol_function(url))) {
		if (proxies.only_proxies && url_bypasses_socks(url)) {
			msg_box(ses->term, NULL, TEXT_(T_ERROR), AL_CENTER, TEXT_(T_NO_PROXY), MSG_BOX_END, NULL, 1, TEXT_(T_CANCEL), msg_box_null, B_ENTER | B_ESC);
			return;
		}
		fn(ses, url);
		return;
	}
	ses->reloadlevel = NC_CACHE;
	if (!(u = translate_url(url, ses->term->cwd))) {
		struct status stat = { init_list_1st(NULL) NULL, NULL, S_BAD_URL, PRI_CANCEL, 0, NULL, NULL, NULL, init_list_last(NULL) };
		print_error_dialog(ses, &stat, url);
		return;
	}
	pos = extract_position(u);
	if (ses->wtd == state2 && !strcmp(cast_const_char ses->rq->orig_url, cast_const_char u) && !xstrcmp(ses->wtd_target, target) && ses->wtd_target_base == df) {
		free(u);
		free(ses->goto_position);
		ses->goto_position = pos;
		return;
	}
	ses_abort_1st_state_loading(ses);
	ses->wtd = state2;
	ses->wtd_target = stracpy(target);
	ses->wtd_target_base = df;
	ses->wtd_refresh = refresh;
	free(ses->goto_position);
	ses->goto_position = pos;
	if (!from_goto_dialog && df && df->rq) prev_url = df->rq->url;
	else prev_url = NULL;   /* previous page is empty - this probably never happens, but for sure */
	if (refresh && ses->wtd == ses_go_to_2nd_state) {
		struct f_data_c *fr = find_frame(ses, ses->wtd_target, ses->wtd_target_base);
		if (fr && fr->loc) if (!strcmp(cast_const_char fr->loc->url, cast_const_char u)) ses->reloadlevel = NC_RELOAD;
	}
	reloadlevel = ses->reloadlevel;
	if (ses->wtd == ses_go_to_2nd_state && !from_goto_dialog && !refresh) {
		struct f_data_c *fd;
		fd = find_frame(ses, ses->wtd_target, ses->wtd_target_base);
		if (!fd) fd = ses->screen;
		if (fd && fd->rq && !strcmp(cast_const_char fd->rq->url, cast_const_char u)) reloadlevel = NC_ALWAYS_CACHE;
	}

	if (from_goto_dialog) allow_flags = ALLOW_ALL;
	else if (df && df->rq) allow_flags = get_allow_flags(df->rq->url);
	else allow_flags = 0;

	request_object(ses->term, u, prev_url, PRI_MAIN, reloadlevel, allow_flags, ses_finished_1st_state, ses, &ses->rq);
	free(u);
}

/* this doesn't send referer */
void goto_url(void *ses_, unsigned char *url)
{
	struct session *ses = (struct session *)ses_;
	unsigned char *u = convert(term_charset(ses->term), utf8_table, url, NULL);
	goto_url_utf8(ses, u);
	free(u);
}

void goto_url_utf8(struct session *ses, unsigned char *url)
{
	goto_url_f(ses, NULL, url, NULL, NULL, -1, 0, 1, 0);
}

/* this one sends referer */
void goto_url_not_from_dialog(struct session *ses, unsigned char *url, struct f_data_c *df)
{
	goto_url_f(ses, NULL, url, cast_uchar "_top", df, -1, 0, 0, 0);
}

static void freeml_void(void *ml_)
{
	freeml((struct memory_list *)ml_);
}

static void ses_imgmap(struct session *ses)
{
	unsigned char *start, *end;
	struct memory_list *ml;
	struct menu_item *menu;
	struct f_data_c *fd;
	if (ses->rq->state != O_OK && ses->rq->state != O_INCOMPLETE) return;
	if (!(fd = current_frame(ses)) || !fd->f_data) return;
	if (get_file(ses->rq, &start, &end)) return;
	d_opt = &fd->f_data->opt;
	if (get_image_map(ses->rq->ce->head, start, end, ses->goto_position, &menu, &ml, ses->imgmap_href_base, ses->imgmap_target_base, term_charset(ses->term), ses->ds.assume_cp, ses->ds.hard_assume, 0)) {
		ses_abort_1st_state_loading(ses);
		return;
	}
	/*add_empty_window(ses->term, freeml_void, ml);*/
	do_menu_selected(ses->term, menu, ses, 0, freeml_void, ml);
	ses_abort_1st_state_loading(ses);
}

void goto_imgmap(struct session *ses, struct f_data_c *fd, unsigned char *url, unsigned char *href, unsigned char *target)
{
	free(ses->imgmap_href_base);
	ses->imgmap_href_base = href;
	free(ses->imgmap_target_base);
	ses->imgmap_target_base = target;
	goto_url_f(ses, ses_imgmap, url, NULL, fd, -1, 0, 0, 0);
}

void map_selected(struct terminal *term, void *ld_, void *ses_)
{
	struct link_def *ld = (struct link_def *)ld_;
	struct session *ses = (struct session *)ses_;
	int x = 0;
	if (ld->onclick) {
		current_frame(ses);
		x = 1;
	}
	if (ld->link) goto_url_f(ses, NULL, ld->link, ld->target, current_frame(ses), -1, x, 0, 0);
}

void go_back(struct session *ses, int num_steps)
{
	struct location *loc;
	struct list_head *lloc;
	int n;
	if (!num_steps) return;
	ses->reloadlevel = NC_CACHE;
	ses_destroy_defered_jump(ses);
	if (ses_abort_1st_state_loading(ses)) {
		change_screen_status(ses);
		print_screen_status(ses);
		return;
	}
	n = num_steps;
	if (num_steps > 0) {
		foreach(struct location, loc, lloc, ses->history) {
			if (!n--) goto have_loc;
		}
		return;
	} else {
		foreach(struct location, loc, lloc, ses->forward_history) {
			if (!++n) goto have_loc;
		}
		return;
	}
	have_loc:
	ses->wtd = ses_go_back_to_2nd_state;
	ses->wtd_num_steps = num_steps;
	request_object(ses->term, loc->url, loc->prev_url, PRI_MAIN, NC_ALWAYS_CACHE, ALLOW_ALL, ses_finished_1st_state, ses, &ses->rq);
}

static void reload_frame(struct f_data_c *fd, int no_cache)
{
	unsigned char *u;
	if (!list_empty(fd->subframes)) {
		struct f_data_c *fdd;
		struct list_head *lfdd;
		foreach(struct f_data_c, fdd, lfdd, fd->subframes) {
			reload_frame(fdd, no_cache);
		}
		return;
	}
	if (!fd->rq) return;
	if (fd->f_data && !f_is_finished(fd->f_data)) return;
	u = stracpy(fd->rq->url);
	release_object(&fd->rq);
	if (fd->f_data) release_object(&fd->f_data->rq);
	request_object(fd->ses->term, u, NULL, PRI_MAIN, no_cache, ALLOW_ALL, fd_loaded, fd, &fd->rq);
	if (fd->f_data) clone_object(fd->rq, &fd->f_data->rq);
	fd->last_update = get_time();
	fd->next_update_interval = 0;
	fd->done = 0;
	fd->parsed_done = 0;
	free(u);
}

void reload(struct session *ses, int no_cache)
{
	ses_destroy_defered_jump(ses);
	if (no_cache == -1) no_cache = ++ses->reloadlevel;
	else ses->reloadlevel = no_cache;
	reload_frame(ses->screen, no_cache);
}

static void set_doc_view(struct session *ses)
{
	ses->screen->xp = 0;
	ses->screen->yp = gf_val(1, G_BFU_FONT_SIZE);
	ses->screen->xw = ses->term->x;
	if (ses->term->y < gf_val(2, 2 * G_BFU_FONT_SIZE)) ses->screen->yw = 0;
	else ses->screen->yw = ses->term->y - gf_val(2, 2 * G_BFU_FONT_SIZE);
}

static struct session *create_session(struct window *win)
{
	static int session_id = 1;
	struct terminal *term = win->term;
	struct session *ses;
	ses = mem_calloc(sizeof(struct session));
	init_list(ses->history);
	init_list(ses->forward_history);
	ses->term = term;
	ses->win = win;
	ses->id = session_id++;
	ses->screen = create_f_data_c(ses, NULL);
	ses->screen->xp = 0; ses->screen->xw = term->x;
	ses->screen->yp = gf_val(1, G_BFU_FONT_SIZE);
	ses->screen->yw = term->y - gf_val(2, 2 * G_BFU_FONT_SIZE);
	memcpy(&ses->ds, &dds, sizeof(struct document_setup));
	init_list(ses->format_cache);
	add_to_list(sessions, ses);
	if (first_use) {
		first_use = 0;
		msg_box(term, NULL, TEXT_(T_WELCOME), AL_CENTER, TEXT_(T_WELCOME_TO_LINKS), cast_uchar "\n\n", TEXT_(T_BASIC_HELP), MSG_BOX_END, NULL, 1, TEXT_(T_OK), msg_box_null, B_ENTER | B_ESC);
	}
	return ses;
}

/*vyrobi retezec znaku, ktery se posilaj skrz unix domain socket hlavni instanci
   prohlizece
   cp=cislo session odkud se ma kopirovat (kdyz se klikne na "open new window")
   url=url kam se ma jit (v pripade kliknuti na "open link in new window" nebo pusteni linksu z prikazove radky s url)
   framename=jmeno ramu, ktery se vytvori

   vraci sekvenci bytu a delku
 */
void *create_session_info(int cp, unsigned char *url, unsigned char *framename, int *ll)
{
	size_t sl = strlen(cast_const_char url);
	size_t sl1 = framename ? strlen(cast_const_char framename) : 0;
	int l = (int)sl;
	int l1 = (int)sl1;
	unsigned char *i;
	if (sl > MAXINT || sl1 > MAXINT) overalloc();
	if (framename && !strcmp(cast_const_char framename, "_blank")) l1 = 0;

	i = init_str();
	*ll = 0;
	add_bytes_to_str(&i, ll, (unsigned char *)&cp, sizeof(int));
	add_bytes_to_str(&i, ll, (unsigned char *)&l, sizeof(int));
	add_bytes_to_str(&i, ll, (unsigned char *)&l1, sizeof(int));
	add_bytes_to_str(&i, ll, url, l);
	add_bytes_to_str(&i, ll, framename, l1);
	return i;
}

/* dostane data z create_session_info a nainicializuje podle nich session
   vraci -1 pokud jsou data vadna
 */
static int read_session_info(struct session *ses, void *data, int len)
{
	unsigned char *h;
	int cpfrom, sz, sz1;
	struct session *s;
	struct list_head *ls;
	if (len < 3 * (int)sizeof(int)) return -1;
	cpfrom = *(int *)data;
	sz = *((int *)data + 1);
	sz1= *((int *)data + 2);
	foreach(struct session, s, ls, sessions) if (s->id == cpfrom) {
		memcpy(&ses->ds, &s->ds, sizeof(struct document_setup));
		if (!sz) {
			if (!list_empty(s->history)) {
				struct location *loc = list_struct(s->history.next, struct location);
				if (loc->url) goto_url_utf8(ses, loc->url);
			}
			return 0;
		} else {
			break;
		}
	}
	if (sz1) {
		unsigned char *tgt;
		if (len<3*(int)sizeof(int)+sz+sz1) goto bla;
		if ((unsigned)sz1 >= MAXINT) overalloc();
		tgt = xmalloc(sz1 + 1);
		memcpy(tgt, (unsigned char*)((int*)data+3)+sz,sz1);
		tgt[sz1] = 0;
		free(ses->wanted_framename);
		ses->wanted_framename=NULL;
		ses->wanted_framename=tgt;
	}
	bla:
	if (sz) {
		unsigned char *u, *uu;
		if (len < 3 * (int)sizeof(int) + sz) return -1;
		if ((unsigned)sz >= MAXINT) overalloc();
		u = xmalloc(sz + 1);
		memcpy(u, (int *)data + 3, sz);
		u[sz] = 0;
		uu = decode_url(u);
		goto_url(ses, uu);
		free(u);
		free(uu);
	} else if ((h = cast_uchar getenv("WWW_HOME")) && *h) {
		goto_url(ses, h);
	}
	return 0;
}

void cleanup_session(struct session *ses)
{
	struct download *d;
	struct list_head *ld;
	foreach(struct download, d, ld, downloads) if (d->ses == ses && d->prog) {
		ld = ld->prev;
		abort_download(d);
	}
	ses_abort_1st_state_loading(ses);
	reinit_f_data_c(ses->screen);
	ses->screen->vs = NULL;
	while (!list_empty(ses->format_cache)) {
		struct f_data *f = list_struct(ses->format_cache.next, struct f_data);
		del_from_list(f);
		destroy_formatted(f);
	}
	while (!list_empty(ses->history))
		destroy_location(list_struct(ses->history.next, struct location));
	clear_forward_history(ses);
	free(ses->default_status);
	free(ses->search_word);
	ses->default_status = NULL;
	ses->search_word = NULL;
	ses_destroy_defered_jump(ses);
}

void destroy_session(struct session *ses)
{
	cleanup_session(ses);
	free(ses->screen);
	free(ses->st);
	free(ses->st_old);
	ses->st = NULL;
	ses->st_old = NULL;
	free(ses->dn_url);
	free(ses->last_search_word);
	free(ses->imgmap_href_base);
	free(ses->imgmap_target_base);
	free(ses->wanted_framename);

	release_object(&ses->tq);
	free(ses->tq_prog);

	del_from_list(ses);
}

static void move_session_to_front(struct session *ses)
{
	if (sessions.next == &ses->list_entry)
		return;
	del_from_list(ses);
	add_to_list(sessions, ses);
}

void win_func(struct window *win, struct links_event *ev, int fw)
{
	struct session *ses = win->data;
	switch ((int)ev->ev) {
		case EV_ABORT:
			if (ses) destroy_session(ses);
			break;
		case EV_INIT:
			ses = win->data = create_session(win);
			if (read_session_info(ses, (char *)ev->b + sizeof(int), *(int *)ev->b)) {
				register_bottom_half(destroy_terminal, win->term);
				return;
			}
			/*-fallthrough*/
		case EV_RESIZE:
			move_session_to_front(ses);
			free(ses->st_old);
			ses->st_old = NULL;
			GF(set_window_pos(win, 0, 0, ev->x, ev->y));
			set_doc_view(ses);
			html_interpret_recursive(ses->screen);
			draw_fd(ses->screen);
			break;
		case EV_REDRAW:
			free(ses->st_old);
			ses->st_old = NULL;
			draw_formatted(ses);
			break;
		case EV_MOUSE:
#ifdef G
			if (F) set_window_ptr(win, ev->x, ev->y);
#endif
			/*-fallthrough*/
		case EV_KBD:
			if (ev->ev == EV_KBD || (ev->b & BM_ACT) != B_MOVE || BM_IS_WHEEL(ev->b)) move_session_to_front(ses);
			send_event(ses, ev);
			break;
		default:
			error("ERROR: unknown event");
	}
}

/*
  Gets the url being viewed by this session. Writes it into str.
  A maximum of str_size bytes (including null) will be written.
*/
unsigned char *get_current_url(struct session *ses, unsigned char *str, size_t str_size)
{
	unsigned char *here;
	size_t url_len = 0;

	/* Not looking at anything */
	if (list_empty(ses->history))
		return NULL;

	here = display_url(ses->term, cur_loc(ses)->url, 0);
	url_len = strlen(cast_const_char here);

	/* Ensure that the url size is not greater than str_size */
	if (url_len >= str_size)
			url_len = str_size - 1;

	safe_strncpy(str, here, url_len + 1);

	free(here);

	return str;
}


/*
  Gets the title of the page being viewed by this session. Writes it into str.
  A maximum of str_size bytes (including null) will be written.
*/
unsigned char *get_current_title(struct f_data_c *fd, unsigned char *str, size_t str_size)
{
	/* Ensure that the title is defined */
	if (!fd || !fd->f_data)
		return NULL;

	safe_strncpy(str, fd->f_data->title, str_size);

	return str;
}
