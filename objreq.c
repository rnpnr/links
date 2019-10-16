/* objreq.c
 * Object Requester
 * (c) 2002 Mikulas Patocka
 * This file is a part of the Links program, released under GPL.
 */

#include "links.h"

static void objreq_end(struct status *, void *);
static void object_timer(void *);


static struct list_head requests = {&requests, &requests};
static tcount obj_req_count = 1;

#define LL gf_val(1, G_BFU_FONT_SIZE)

#define MAX_UID_LEN 256

struct auth_dialog {
	unsigned char uid[MAX_UID_LEN];
	unsigned char passwd[MAX_UID_LEN];
	unsigned char *realm;
	int proxy;
	tcount count;
	unsigned char msg[1];
};

static inline struct object_request *find_rq(tcount c)
{
	struct object_request *rq = NULL;
	struct list_head *lrq;
	foreach(struct object_request, rq, lrq, requests) if (rq->count == c) return rq;
	return NULL;
}

static void auth_fn(struct dialog_data *dlg)
{
	struct terminal *term = dlg->win->term;
	struct auth_dialog *a = dlg->dlg->udata;
	int max = 0, min = 0;
	int w, rw;
	int y = 0;
	max_text_width(term, a->msg, &max, AL_LEFT);
	min_text_width(term, a->msg, &min, AL_LEFT);
	max_text_width(term, TEXT_(T_USERID), &max, AL_LEFT);
	min_text_width(term, TEXT_(T_USERID), &min, AL_LEFT);
	max_text_width(term, TEXT_(T_PASSWORD), &max, AL_LEFT);
	min_text_width(term, TEXT_(T_PASSWORD), &min, AL_LEFT);
	max_buttons_width(term, dlg->items + 2, 2, &max);
	min_buttons_width(term, dlg->items + 2, 2, &min);
	w = term->x * 9 / 10 - 2 * DIALOG_LB;
	if (w > max) w = max;
	if (w < min) w = min;
	rw = w;
	dlg_format_text(dlg, NULL, a->msg, 0, &y, w, &rw, COLOR_DIALOG_TEXT, AL_LEFT);
	y += LL;
	dlg_format_text_and_field(dlg, NULL, TEXT_(T_USERID), dlg->items, 0, &y, w, &rw, COLOR_DIALOG_TEXT, AL_LEFT);
	y += LL;
	dlg_format_text_and_field(dlg, NULL, TEXT_(T_PASSWORD), dlg->items + 1, 0, &y, w, &rw, COLOR_DIALOG_TEXT, AL_LEFT);
	y += LL;
	dlg_format_buttons(dlg, NULL, dlg->items + 2, 2, 0, &y, w, &rw, AL_CENTER);
	w = rw;
	dlg->xw = rw + 2 * DIALOG_LB;
	dlg->yw = y + 2 * DIALOG_TB;
	center_dlg(dlg);
	draw_dlg(dlg);
	y = dlg->y + DIALOG_TB;
	y += LL;
	dlg_format_text(dlg, term, a->msg, dlg->x + DIALOG_LB, &y, w, NULL, COLOR_DIALOG_TEXT, AL_LEFT);
	y += LL;
	dlg_format_text_and_field(dlg, term, TEXT_(T_USERID), dlg->items, dlg->x + DIALOG_LB, &y, w, NULL, COLOR_DIALOG_TEXT, AL_LEFT);
	y += LL;
	dlg_format_text_and_field(dlg, term, TEXT_(T_PASSWORD), dlg->items + 1, dlg->x + DIALOG_LB, &y, w, NULL, COLOR_DIALOG_TEXT, AL_LEFT);
	y += LL;
	dlg_format_buttons(dlg, term, dlg->items + 2, 2, dlg->x + DIALOG_LB, &y, w, NULL, AL_CENTER);
}

static int auth_cancel(struct dialog_data *dlg, struct dialog_item_data *item)
{
	struct auth_dialog *a = dlg->dlg->udata;
	struct object_request *rq = find_rq(a->count);
	if (rq) {
		rq->hold = 0;
		rq->state = O_OK;
		if (rq->timer != NULL)
			kill_timer(rq->timer);
		rq->timer = install_timer(0, object_timer, rq);
		if (!rq->ce)
			(rq->ce = rq->ce_internal)->refcount++;
	}
	cancel_dialog(dlg, item);
	return 0;
}

static int auth_ok(struct dialog_data *dlg, struct dialog_item_data *item)
{
	struct auth_dialog *a = dlg->dlg->udata;
	struct object_request *rq = find_rq(a->count);
	if (rq) {
		struct session *ses;
		int net_cp;
		unsigned char *uid, *passwd;
		get_dialog_data(dlg);
		ses = list_struct(dlg->win->term->windows.prev, struct window)->data;
		get_convert_table(rq->ce_internal->head, term_charset(dlg->win->term), ses->ds.assume_cp, &net_cp, NULL, ses->ds.hard_assume);
		uid = convert(term_charset(dlg->win->term), net_cp, a->uid, NULL);
		passwd = convert(term_charset(dlg->win->term), net_cp, a->passwd, NULL);
		add_auth(rq->url, a->realm, uid, passwd, a->proxy);
		free(uid);
		free(passwd);
		rq->hold = 0;
		change_connection(&rq->stat, NULL, PRI_CANCEL);
		load_url(rq->url, rq->prev_url, &rq->stat, rq->pri, NC_RELOAD, 0, 0, 0);
	}
	cancel_dialog(dlg, item);
	return 0;
}

static int auth_window(struct object_request *rq, unsigned char *realm)
{
	unsigned char *host, *port;
	struct dialog *d;
	struct auth_dialog *a;
	struct terminal *term;
	unsigned char *urealm;
	struct session *ses;
	int net_cp;
	if (!(term = find_terminal(rq->term))) return -1;
	ses = list_struct(term->windows.prev, struct window)->data;
	get_convert_table(rq->ce_internal->head, term_charset(term), ses->ds.assume_cp, &net_cp, NULL, ses->ds.hard_assume);
	if (rq->ce_internal->http_code == 407) {
		unsigned char *h = get_proxy_string(rq->url);
		if (!h) h = cast_uchar "";
		host = display_host(term, h);
	} else {
		unsigned char *h = get_host_name(rq->url);
		if (!h) return -1;
		host = display_host(term, h);
		free(h);
		if ((port = get_port_str(rq->url))) {
			add_to_strn(&host, cast_uchar ":");
			add_to_strn(&host, port);
			free(port);
		}
	}
	urealm = convert(term_charset(term), net_cp, realm, NULL);
	d = xmalloc(sizeof(struct dialog) + 5 * sizeof(struct dialog_item) + sizeof(struct auth_dialog) + strlen(cast_const_char get_text_translation(TEXT_(T_ENTER_USERNAME), term)) + strlen(cast_const_char urealm) + 1 + strlen(cast_const_char get_text_translation(TEXT_(T_AT), term)) + strlen(cast_const_char host));
	memset(d, 0, sizeof(struct dialog) + 5 * sizeof(struct dialog_item) + sizeof(struct auth_dialog));
	a = (struct auth_dialog *)((unsigned char *)d + sizeof(struct dialog) + 5 * sizeof(struct dialog_item));
	strcpy(cast_char a->msg, cast_const_char get_text_translation(TEXT_(T_ENTER_USERNAME), term));
	strcat(cast_char a->msg, cast_const_char urealm);
	if (*host) {
		strcat(cast_char a->msg, "\n");
		strcat(cast_char a->msg, cast_const_char get_text_translation(TEXT_(T_AT), term));
		strcat(cast_char a->msg, cast_const_char host);
	}
	free(host);
	free(urealm);
	a->proxy = rq->ce_internal->http_code == 407;
	a->realm = stracpy(realm);
	a->count = rq->count;
	d->udata = a;
	if (rq->ce_internal->http_code == 401) d->title = TEXT_(T_AUTHORIZATION_REQUIRED);
	else d->title = TEXT_(T_PROXY_AUTHORIZATION_REQUIRED);
	d->fn = auth_fn;
	d->items[0].type = D_FIELD;
	d->items[0].dlen = MAX_UID_LEN;
	d->items[0].data = a->uid;

	d->items[1].type = D_FIELD_PASS;
	d->items[1].dlen = MAX_UID_LEN;
	d->items[1].data = a->passwd;

	d->items[2].type = D_BUTTON;
	d->items[2].gid = B_ENTER;
	d->items[2].fn = auth_ok;
	d->items[2].text = TEXT_(T_OK);

	d->items[3].type = D_BUTTON;
	d->items[3].gid = B_ESC;
	d->items[3].fn = auth_cancel;
	d->items[3].text = TEXT_(T_CANCEL);

	do_dialog(term, d, getml(d, a->realm, NULL));
	return 0;
}

struct cert_dialog {
	tcount term;
	unsigned char *host;
	int bl;
	int state;
};

static void cert_action(struct object_request *rq, int yes)
{
	if (yes > 0) {
		rq->hold = 0;
		change_connection(&rq->stat, NULL, PRI_CANCEL);
		load_url(rq->url, rq->prev_url, &rq->stat, rq->pri, NC_CACHE, 0, 0, 0);
	} else {
		rq->hold = 0;
		rq->dont_print_error = 1;
		rq->state = O_FAILED;
		if (rq->timer != NULL) kill_timer(rq->timer);
		rq->timer = install_timer(0, object_timer, rq);
	}
}

static void cert_forall(struct cert_dialog *cs, int yes)
{
	struct object_request *rq = NULL;
	struct list_head *lrq;
	if (yes > 0) {
		add_blacklist_entry(cs->host, cs->bl);
		del_blacklist_entry(cs->host, BL_AVOID_INSECURE);
	}
	if (yes < 0) {
		add_blacklist_entry(cs->host, BL_AVOID_INSECURE);
		del_blacklist_entry(cs->host, BL_IGNORE_CERTIFICATE);
		del_blacklist_entry(cs->host, BL_IGNORE_DOWNGRADE);
		del_blacklist_entry(cs->host, BL_IGNORE_CIPHER);
	}
	foreach(struct object_request, rq, lrq, requests) if (rq->term == cs->term && rq->hold == HOLD_CERT && rq->stat.state == cs->state) {
		unsigned char *host = get_host_name(rq->url);
		if (!strcmp(cast_const_char host, cast_const_char cs->host)) cert_action(rq, yes);
		free(host);
	}
}

static void cert_yes(void *data)
{
	cert_forall((struct cert_dialog *)data, 1);
}

static void cert_no(void *data)
{
	cert_forall((struct cert_dialog *)data, 0);
}

static void cert_never(void *data)
{
	cert_forall((struct cert_dialog *)data, -1);
}

static int cert_compare(void *data1, void *data2)
{
	struct cert_dialog *cs1 = (struct cert_dialog *)data1;
	struct cert_dialog *cs2 = (struct cert_dialog *)data2;
	return !strcmp(cast_const_char cs1->host, cast_const_char cs2->host) && cs1->state == cs2->state;
}

static int cert_window(struct object_request *rq)
{
	struct terminal *term;
	unsigned char *h, *host, *title, *text;
	struct cert_dialog *cs;
	struct memory_list *ml;
	if (!(term = find_terminal(rq->term))) return -1;
	h = get_host_name(rq->url);
	if (get_blacklist_flags(h) & BL_AVOID_INSECURE) {
		free(h);
		return -1;
	}
	cs = xmalloc(sizeof(struct cert_dialog));
	cs->term = rq->term;
	cs->host = h;
	cs->state = rq->stat.state;
	if (rq->stat.state == S_INVALID_CERTIFICATE) {
		title = TEXT_(T_INVALID_CERTIFICATE);
		text = TEXT_(T_DOESNT_HAVE_A_VALID_CERTIFICATE);
		cs->bl = BL_IGNORE_CERTIFICATE;
	} else if (rq->stat.state == S_DOWNGRADED_METHOD) {
		title = TEXT_(T_DOWNGRADED_METHOD);
		text = TEXT_(T_USES_DOWNGRADED_METHOD);
		cs->bl = BL_IGNORE_DOWNGRADE;
	} else {
		title = TEXT_(T_INSECURE_CIPHER);
		text = TEXT_(T_USES_INSECURE_CIPHER);
		cs->bl = BL_IGNORE_CIPHER;
	}
	host = display_host(term, h);
	ml = getml(cs, h, host, NULL);
	if (find_msg_box(term, title, cert_compare, cs)) {
		freeml(ml);
		return 0;
	}
	msg_box(term, ml,
		title,
		AL_CENTER, TEXT_(T_THE_SERVER_), host, text, MSG_BOX_END,
		(void *)cs, 3, TEXT_(T_NO), cert_no, B_ESC, TEXT_(T_YES), cert_yes, B_ENTER, TEXT_(T_NEVER), cert_never, 0);
	return 0;
}

/* prev_url is a pointer to previous url or NULL */
/* prev_url will NOT be deallocated */
void request_object(struct terminal *term, unsigned char *url, unsigned char *prev_url, int pri, int cache, int allow_flags, void (*upcall)(struct object_request *, void *), void *data, struct object_request **rqp)
{
	struct object_request *rq;
	rq = mem_calloc(sizeof(struct object_request));
	rq->state = O_WAITING;
	rq->refcount = 1;
	rq->term = term ? term->count : 0;
	rq->stat.end = objreq_end;
	rq->stat.data = rq;
	rq->orig_url = stracpy(url);
	rq->url = stracpy(url);
	rq->pri = pri;
	rq->cache = cache;
	rq->upcall = upcall;
	rq->data = data;
	rq->timer = NULL;
	rq->last_update = get_time() - STAT_UPDATE_MAX;
	free(rq->prev_url);
	rq->prev_url = stracpy(prev_url);
	if (rqp) *rqp = rq;
	rq->count = obj_req_count++;
	add_to_list(requests, rq);
	load_url(url, prev_url, &rq->stat, pri, cache, 0, allow_flags, 0);
}

static void set_ce_internal(struct object_request *rq)
{
	if (rq->stat.ce != rq->ce_internal) {
		if (!rq->stat.ce) {
			rq->ce_internal->refcount--;
			rq->ce_internal = NULL;
		} else {
			if (rq->ce_internal)
				rq->ce_internal->refcount--;
			rq->ce_internal = rq->stat.ce;
			rq->ce_internal->refcount++;
		}
	}
}

static void objreq_end(struct status *stat, void *data)
{
	struct object_request *rq = (struct object_request *)data;

	set_ce_internal(rq);

	if (stat->state < 0) {
		if (!stat->ce && rq->state == O_WAITING
		&& (stat->state == S_INVALID_CERTIFICATE
		|| stat->state == S_DOWNGRADED_METHOD
		|| stat->state == S_INSECURE_CIPHER)
		&& ssl_options.certificates == SSL_WARN_ON_INVALID_CERTIFICATE) {
			if (!cert_window(rq)) {
				rq->hold = HOLD_CERT;
				rq->redirect_cnt = 0;
				goto tm;
			}
		}
		if (stat->ce && rq->state == O_WAITING && stat->ce->redirect) {
			if (rq->redirect_cnt++ < MAX_REDIRECTS) {
				int cache, allow_flags;
				unsigned char *u, *pos;
				change_connection(stat, NULL, PRI_CANCEL);
				u = join_urls(rq->url, stat->ce->redirect);
				if ((pos = extract_position(u))) {
					free(rq->goto_position);
					rq->goto_position = pos;
				}
				cache = rq->cache;
				if (cache < NC_RELOAD
				&& (!strcmp(cast_const_char u, cast_const_char rq->url)
				|| !strcmp(cast_const_char u, cast_const_char rq->orig_url)
				|| rq->redirect_cnt >= MAX_CACHED_REDIRECTS))
					cache = NC_RELOAD;
				allow_flags = get_allow_flags(rq->url);
				free(rq->url);
				rq->url = u;
				load_url(u, rq->prev_url, &rq->stat, rq->pri, cache, 0, allow_flags, 0);
				return;
			} else {
				maxrd:
				rq->stat.state = S_CYCLIC_REDIRECT;
			}
		}
		if (stat->ce && rq->state == O_WAITING
		&& (stat->ce->http_code == 401 || stat->ce->http_code == 407)) {
			unsigned char *realm = get_auth_realm(rq->url,
							stat->ce->head,
							stat->ce->http_code == 407);
			unsigned char *user;
			if (!realm)
				goto xx;
			if (stat->ce->http_code == 401
			&& !find_auth(rq->url, realm)) {
				free(realm);
				if (rq->redirect_cnt++ >= MAX_REDIRECTS)
					goto maxrd;
				change_connection(stat, NULL, PRI_CANCEL);
				load_url(rq->url, rq->prev_url, &rq->stat,
					rq->pri, NC_RELOAD, 0, 0, 0);
				return;
			}
			user = get_user_name(rq->url);
			if (stat->ce->http_code == 401 && *user) {
				free(user);
				free(realm);
				goto xx;
			}
			free(user);
			if (!auth_window(rq, realm)) {
				rq->hold = HOLD_AUTH;
				rq->redirect_cnt = 0;
				free(realm);
				goto tm;
			}
			free(realm);
			goto xx;
		}
	}
	if ((stat->state < 0 || stat->state == S_TRANS)
	&& stat->ce && !stat->ce->redirect
	&& stat->ce->http_code != 401
	&& stat->ce->http_code != 407) {
		rq->state = O_LOADING;
		if (0) {
			xx:
			rq->state = O_OK;
		}
		if (!rq->ce)
			(rq->ce = stat->ce)->refcount++;
	}
	tm:
	if (rq->timer != NULL)
		kill_timer(rq->timer);
	rq->timer = install_timer(0, object_timer, rq);
}

static void object_timer(void *rq_)
{
	struct object_request *rq = (struct object_request *)rq_;
	off_t last;

	rq->timer = NULL;

	set_ce_internal(rq);

	last = rq->last_bytes;
	if (rq->ce)
		rq->last_bytes = rq->ce->length;
	if (rq->stat.state < 0 && !rq->hold
	&& (!rq->ce_internal || !rq->ce_internal->redirect
	|| rq->stat.state == S_CYCLIC_REDIRECT)) {
		if (rq->ce_internal && rq->stat.state != S_CYCLIC_REDIRECT) {
			rq->state = rq->stat.state != S__OK ? O_INCOMPLETE : O_OK;
		} else
			rq->state = O_FAILED;
	}
	if (rq->stat.state != S_TRANS) {
		if (rq->stat.state >= 0)
			rq->timer = install_timer(STAT_UPDATE_MAX, object_timer, rq);
		rq->last_update = get_time() - STAT_UPDATE_MAX;
		if (rq->upcall)
			rq->upcall(rq, rq->data);
	} else {
		uttime ct = get_time();
		uttime t = ct - rq->last_update;
		rq->timer = install_timer(STAT_UPDATE_MIN, object_timer, rq);
		if (t >= STAT_UPDATE_MAX || (t >= STAT_UPDATE_MIN && rq->ce && rq->last_bytes > last)) {
			rq->last_update = ct;
			if (rq->upcall) rq->upcall(rq, rq->data);
		}
	}
}

void release_object_get_stat(struct object_request **rqq, struct status *news, int pri)
{
	struct object_request *rq = *rqq;
	if (!rq) return;
	*rqq = NULL;
	if (--rq->refcount)
		return;
	change_connection(&rq->stat, news, pri);
	if (rq->timer != NULL)
		kill_timer(rq->timer);
	if (rq->ce_internal)
		rq->ce_internal->refcount--;
	if (rq->ce)
		rq->ce->refcount--;
	free(rq->orig_url);
	free(rq->url);
	free(rq->prev_url);
	free(rq->goto_position);
	del_from_list(rq);
	free(rq);
}

void release_object(struct object_request **rqq)
{
	release_object_get_stat(rqq, NULL, PRI_CANCEL);
}

void detach_object_connection(struct object_request *rq, off_t pos)
{
	if (rq->state == O_WAITING || rq->state == O_FAILED) {
		internal("detach_object_connection: no data received");
		return;
	}
	if (rq->refcount == 1) {
		detach_connection(&rq->stat, pos, 0, 1);
	}
}

void clone_object(struct object_request *rq, struct object_request **rqq)
{
	(*rqq = rq)->refcount++;
}
