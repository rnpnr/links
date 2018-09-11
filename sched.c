/* sched.c
 * Links internal scheduler
 * (c) 2002 Mikulas Patocka
 * This file is a part of the Links program, released under GPL.
 */

#include "links.h"

static tcount connection_count = 0;

static int active_connections = 0;

tcount netcfg_stamp = 0;

struct list_head queue = {&queue, &queue};

struct h_conn {
	list_entry_1st
	unsigned char *host;
	int conn;
	list_entry_last
};

static struct list_head h_conns = {&h_conns, &h_conns};

struct list_head keepalive_connections = {&keepalive_connections, &keepalive_connections};

/* prototypes */
static void send_connection_info(struct connection *c);
static void del_keepalive_socket(struct k_conn *kc);
static void check_keepalive_connections(void);

unsigned long connect_info(int type)
{
	int i = 0;
	struct connection *ce;
	struct list_head *lce;
	switch (type) {
	case CI_FILES:
		return list_size(&queue);
	case CI_CONNECTING:
		foreach(struct connection, ce, lce, queue)
			i += ce->state > S_WAIT && ce->state < S_TRANS;
		return i;
	case CI_TRANSFER:
		foreach(struct connection, ce, lce, queue)
			i += ce->state == S_TRANS;
		return i;
	case CI_KEEP:
		check_keepalive_connections();
		return list_size(&keepalive_connections);
	default:
		internal("connect_info: bad request");
	}
	return 0;
}

static int getpri(struct connection *c)
{
	int i;
	for (i = 0; i < N_PRI; i++)
		if (c->pri[i])
			return i;
	internal("connection has no owner");
	return N_PRI;
}

static int connection_disappeared(struct connection *c, tcount count)
{
	struct connection *d;
	struct list_head *ld;
	foreach(struct connection, d, ld, queue) if (c == d && count == d->count) return 0;
	return 1;
}

static struct h_conn *is_host_on_list(struct connection *c)
{
	char *ho = (char *)get_host_name(c->url);
	struct h_conn *h;
	struct list_head *lh;
	if (!ho)
		return NULL;
	foreach(struct h_conn, h, lh, h_conns)
		if (!strcmp((const char *)h->host, ho)) {
			free(ho);
			return h;
		}
	free(ho);
	return NULL;
}

static int st_r = 0;

static void stat_timer(void *c_)
{
	struct connection *c = (struct connection *)c_;
	struct remaining_info *r = &c->prg;
	uttime now = get_time();
	uttime a = now - r->last_time;
	if (a > SPD_DISP_TIME * 100)
		a = SPD_DISP_TIME * 100;
	if (getpri(c) == PRI_CANCEL
	&& (c->est_length > (long)memory_cache_size * MAX_CACHED_OBJECT
	|| c->from > (long)memory_cache_size * MAX_CACHED_OBJECT))
		register_bottom_half(check_queue, NULL);
	if (c->state > S_WAIT) {
		r->loaded = c->received;
		if ((r->size = c->est_length) < (r->pos = c->from) && r->size != -1)
			r->size = c->from;
		r->dis_b += a;
		while (r->dis_b >= SPD_DISP_TIME * CURRENT_SPD_SEC) {
			r->cur_loaded -= r->data_in_secs[0];
			memmove(r->data_in_secs, r->data_in_secs + 1, sizeof(off_t) * (CURRENT_SPD_SEC - 1));
			r->data_in_secs[CURRENT_SPD_SEC - 1] = 0;
			r->dis_b -= SPD_DISP_TIME;
		}
		r->data_in_secs[CURRENT_SPD_SEC - 1] += r->loaded - r->last_loaded;
		r->cur_loaded += r->loaded - r->last_loaded;
		r->last_loaded = r->loaded;
		r->elapsed += a;
	}
	r->last_time = now;
	r->timer = install_timer(SPD_DISP_TIME, stat_timer, c);
	if (!st_r)
		send_connection_info(c);
}

void setcstate(struct connection *c, int state)
{
	struct status *stat;
	struct list_head *lstat;
	if (c->state < 0 && state >= 0)
		c->prev_error = c->state;
	if ((c->state = state) == S_TRANS) {
		struct remaining_info *r = &c->prg;
		if (!r->timer) {
			tcount count = c->count;
			if (!r->valid) {
				memset(r, 0, sizeof(struct remaining_info));
				r->valid = 1;
			}
			r->last_time = get_time();
			r->last_loaded = r->loaded;
			st_r = 1;
			stat_timer(c);
			st_r = 0;
			if (connection_disappeared(c, count)) return;
		}
	} else {
		struct remaining_info *r = &c->prg;
		if (r->timer) {
			kill_timer(r->timer);
			r->timer = NULL;
		}
	}
	foreach(struct status, stat, lstat, c->statuss) {
		stat->state = state;
		stat->prev_error = c->prev_error;
	}
	if (state >= 0)
		send_connection_info(c);
}

static struct k_conn *is_host_on_keepalive_list(struct connection *c)
{
	char *ho = (char *)get_keepalive_id(c->url);
	const int po = get_port(c->url);
	void (*ph)(struct connection *);
	struct k_conn *h;
	struct list_head *lh;
	if (!(ph = get_protocol_handle(c->url)))
		return NULL;
	if (!ho || po < 0)
		return NULL;
	foreach(struct k_conn, h, lh, keepalive_connections)
		if (h->protocol == ph && h->port == po
		&& !strcmp((char *)h->host, ho)) {
			free(ho);
			return h;
		}
	free(ho);
	return NULL;
}

int get_keepalive_socket(struct connection *c, int *protocol_data)
{
	struct k_conn *k;
	int cc;
	if (c->tries > 0 || c->unrestartable)
		return -1;
	if (!(k = is_host_on_keepalive_list(c)))
		return -1;
	cc = k->conn;
	if (protocol_data)
		*protocol_data = k->protocol_data;
	freeSSL(c->ssl);
	c->ssl = k->ssl;
	memcpy(&c->last_lookup_state, &k->last_lookup_state, sizeof(struct lookup_state));
	del_from_list(k);
	free(k->host);
	free(k);
	c->sock1 = cc;
	if (max_tries == 1)
		c->tries = -1;
	return 0;
}

void abort_all_keepalive_connections(void)
{
	while (!list_empty(keepalive_connections))
		del_keepalive_socket(list_struct(keepalive_connections.next,
						struct k_conn));
	check_keepalive_connections();
}

static void free_connection_data(struct connection *c)
{
	struct h_conn *h;
	int rs;
	if (c->sock1 != -1)
		set_handlers(c->sock1, NULL, NULL, NULL);
	close_socket(&c->sock2);
	if (c->pid) {
		EINTRLOOP(rs, kill(c->pid, SIGINT));
		EINTRLOOP(rs, kill(c->pid, SIGTERM));
		EINTRLOOP(rs, kill(c->pid, SIGKILL));
		c->pid = 0;
	}
	if (!c->running)
		internal("connection already suspended");
	c->running = 0;
	if (c->dnsquery)
		kill_dns_request(&c->dnsquery);
	free(c->buffer);
	free(c->newconn);
	free(c->info);
	c->buffer = NULL;
	c->newconn = NULL;
	c->info = NULL;
	clear_connection_timeout(c);
	if (--active_connections < 0) {
		internal("active connections underflow");
		active_connections = 0;
	}
	if (c->state != S_WAIT) {
		if ((h = is_host_on_list(c))) {
			if (!--h->conn) {
				del_from_list(h);
				free(h->host);
				free(h);
			}
		} else
			internal("suspending connection that is not on the list (state %d)", c->state);
	}
}

static void send_connection_info(struct connection *c)
{
	if (!list_empty(c->statuss)) {
		const int st = c->state;
		tcount count = c->count;
		struct list_head *lstat = c->statuss.next;
		while (1) {
			int e;
			struct status *xstat = list_struct(lstat, struct status);
			xstat->ce = c->cache;
			lstat = lstat->next;
			e = lstat == &c->statuss;
			if (xstat->end)
				xstat->end(xstat, xstat->data);
			if (e || (st >= 0 && connection_disappeared(c, count)))
				return;
		}
	}
}

static void del_connection(struct connection *c)
{
	struct cache_entry *ce = c->cache;
	if (ce)
		ce->refcount++;
	del_from_list(c);
	send_connection_info(c);
	if (ce)
		ce->refcount--;
	if (c->detached) {
		if (ce && !ce->url[0] && !is_entry_used(ce) && !ce->refcount)
			delete_cache_entry(ce);
	} else if (ce)
		trim_cache_entry(ce);
	free(c->url);
	free(c->prev_url);
	free(c);
}

void add_keepalive_socket(struct connection *c, uttime timeout, int protocol_data)
{
	struct k_conn *k;
	int rs;
	free_connection_data(c);
	if (c->sock1 == -1) {
		internal("keepalive connection not connected");
		goto del;
	}
	k = xmalloc(sizeof(struct k_conn));
	if (c->netcfg_stamp != netcfg_stamp
	|| ssl_not_reusable(c->ssl)
	|| (k->port = get_port(c->url)) == -1
	|| !(k->protocol = get_protocol_handle(c->url))
	|| !(k->host = get_keepalive_id(c->url))) {
		free(k);
		del_connection(c);
		goto clos;
	}
	k->conn = c->sock1;
	k->timeout = timeout;
	k->add_time = get_absolute_time();
	k->protocol_data = protocol_data;
	k->ssl = c->ssl;
	memcpy(&k->last_lookup_state, &c->last_lookup_state, sizeof(struct lookup_state));
	add_to_list(keepalive_connections, k);
del:
	del_connection(c);
	register_bottom_half(check_queue, NULL);
	return;
clos:
	EINTRLOOP(rs, close(c->sock1));
	register_bottom_half(check_queue, NULL);
}

static void del_keepalive_socket(struct k_conn *kc)
{
	int rs;
	del_from_list(kc);
	freeSSL(kc->ssl);
	EINTRLOOP(rs, close(kc->conn));
	free(kc->host);
	free(kc);
}

static struct timer *keepalive_timeout = NULL;

static void keepalive_timer(void *x)
{
	keepalive_timeout = NULL;
	check_keepalive_connections();
}

static void check_keepalive_connections(void)
{
	struct k_conn *kc;
	struct list_head *lkc;
	uttime ct = get_absolute_time();
	int p = 0;
	if (keepalive_timeout) {
		kill_timer(keepalive_timeout);
		keepalive_timeout = NULL;
	}
	foreach(struct k_conn, kc, lkc, keepalive_connections) {
		if (can_read(kc->conn) || ct - kc->add_time > kc->timeout) {
			lkc = lkc->prev;
			del_keepalive_socket(kc);
		} else
			p++;
	}
	for (; p > MAX_KEEPALIVE_CONNECTIONS; p--)
		if (!list_empty(keepalive_connections))
			del_keepalive_socket(list_struct(keepalive_connections.prev, struct k_conn));
		else
			internal("keepalive list empty");
	if (!list_empty(keepalive_connections))
		keepalive_timeout = install_timer(KEEPALIVE_CHECK_TIME, keepalive_timer, NULL);
}

static void add_to_queue(struct connection *c)
{
	struct connection *cc;
	struct list_head *lcc;
	int pri = getpri(c);
	foreach(struct connection, cc, lcc, queue) if (getpri(cc) > pri) break;
	add_before_list_entry(lcc, &c->list_entry);
}

static void sort_queue(void)
{
	struct connection *c, *n;
	struct list_head *lc;
	int swp;
	do {
		swp = 0;
		foreach(struct connection, c, lc, queue)
			if (c->list_entry.next != &queue) {
				n = list_struct(c->list_entry.next,
					struct connection);
				if (getpri(n) < getpri(c)) {
					del_from_list(c);
					add_after_pos(n, c);
					swp = 1;
				}
			}
	} while (swp);
}

static void interrupt_connection(struct connection *c)
{
	freeSSL(c->ssl);
	c->ssl = NULL;
	close_socket(&c->sock1);
	free_connection_data(c);
}

static void suspend_connection(struct connection *c)
{
	interrupt_connection(c);
	setcstate(c, S_WAIT);
}

static int try_to_suspend_connection(struct connection *c, unsigned char *ho)
{
	int pri = getpri(c);
	struct connection *d;
	struct list_head *ld;
	foreachback(struct connection, d, ld, queue) {
		if (getpri(d) <= pri)
			return -1;
		if (d->state == S_WAIT)
			continue;
		if (d->unrestartable == 2 && getpri(d) < PRI_CANCEL)
			continue;
		if (ho) {
			unsigned char *h;
			if (!(h = get_host_name(d->url)))
				continue;
			if (strcmp((const char *)h, (const char *)ho)) {
				free(h);
				continue;
			}
			free(h);
		}
		suspend_connection(d);
		return 0;
	}
	return -1;
}

static int is_noproxy_url(unsigned char *url)
{
	unsigned char *host = get_host_name(url);
	if (!proxies.only_proxies) {
		unsigned char *np = proxies.no_proxy;
		int host_l = (int)strlen(cast_const_char host);
		if (*np) while (1) {
			int l = (int)strcspn(cast_const_char np, ",");
			if (l > host_l)
				goto no_match;
			if (l < host_l && host[host_l - l - 1] != '.')
				goto no_match;
			if (casecmp(np, host + (host_l - l), l))
				goto no_match;
			free(host);
			return 1;
no_match:
			if (!np[l])
				break;
			np += l + 1;
		}
	}
	free(host);
	return 0;
}

static void run_connection(struct connection *c)
{
	struct h_conn *hc;
	void (*func)(struct connection *);
	if (c->running) {
		internal("connection already running");
		return;
	}

	memset(&c->last_lookup_state, 0, sizeof(struct lookup_state));

	if (is_noproxy_url(remove_proxy_prefix(c->url))) {
		c->socks_proxy[0] = 0;
		c->dns_append[0] = 0;
	} else {
		safe_strncpy(c->socks_proxy, proxies.socks_proxy,
			sizeof(c->socks_proxy));
		safe_strncpy(c->dns_append, proxies.dns_append,
			sizeof(c->dns_append));
	}

	if (proxies.only_proxies && !is_proxy_url(c->url)
	&& casecmp(c->url, cast_uchar "data:", 5)
	&& (!*c->socks_proxy || url_bypasses_socks(c->url))) {
		setcstate(c, S_NO_PROXY);
		del_connection(c);
		return;
	}

	if (!(func = get_protocol_handle(c->url))) {
		s_bad_url:
		if (is_proxy_url(c->url))
			setcstate(c, S_BAD_PROXY);
		else
			setcstate(c, S_BAD_URL);
		del_connection(c);
		return;
	}
	if (!(hc = is_host_on_list(c))) {
		hc = xmalloc(sizeof(struct h_conn));
		if (!(hc->host = get_host_name(c->url))) {
			free(hc);
			goto s_bad_url;
		}
		hc->conn = 0;
		add_to_list(h_conns, hc);
	}
	hc->conn++;
	active_connections++;
	c->running = 1;
	func(c);
}

static int is_connection_seekable(struct connection *c)
{
	unsigned char *protocol = get_protocol_name(c->url);
	if (!casestrcmp(protocol, cast_uchar "http")
	|| !casestrcmp(protocol, cast_uchar "https")
	|| !casestrcmp(protocol, cast_uchar "proxy")) {
		unsigned char *d;
		free(protocol);
		if (!c->cache || !c->cache->head)
			return 1;
		d = parse_http_header(c->cache->head, cast_uchar "Accept-Ranges", NULL);
		if (d) {
			free(d);
			return 1;
		}
		return 0;
	}
	if (!casestrcmp(protocol, cast_uchar "ftp")) {
		free(protocol);
		return 1;
	}
	free(protocol);
	return 0;
}

int is_connection_restartable(struct connection *c)
{
	return !(c->unrestartable >= 2 || (c->tries + 1 >= (max_tries ? max_tries : 1000)));
}

int is_last_try(struct connection *c)
{
	int is_restartable;
	c->tries++;
	is_restartable = is_connection_restartable(c) && c->tries < 10;
	c->tries--;
	return !is_restartable;
}

void retry_connection(struct connection *c)
{
	interrupt_connection(c);
	if (!is_connection_restartable(c)) {
		del_connection(c);
		register_bottom_half(check_queue, NULL);
	} else {
		c->tries++;
		c->prev_error = c->state;
		run_connection(c);
	}
}

void abort_connection(struct connection *c)
{
	if (c->running)
		interrupt_connection(c);
	del_connection(c);
	register_bottom_half(check_queue, NULL);
}

static int try_connection(struct connection *c)
{
	struct h_conn *hc = NULL;
	if ((hc = is_host_on_list(c))) {
		if (hc->conn >= max_connections_to_host) {
			if (try_to_suspend_connection(c, hc->host))
				return 0;
			else
				return -1;
		}
	}
	if (active_connections >= max_connections) {
		if (try_to_suspend_connection(c, NULL))
			return 0;
		else
			return -1;
	}
	run_connection(c);
	return 1;
}

void check_queue(void *dummy)
{
	struct connection *c;
	struct list_head *lc;
again:
	check_keepalive_connections();
	foreach(struct connection, c, lc, queue) {
		struct connection *d;
		struct list_head *ld;
		const int cp = getpri(c);
		foreachfrom(struct connection, d, ld, queue, &c->list_entry) {
			if (getpri(d) != cp)
				break;
			if (!d->state && is_host_on_keepalive_list(d))
				if (try_connection(d))
					goto again;
		}
		foreachfrom(struct connection, d, ld, queue, &c->list_entry) {
			if (getpri(d) != cp)
				break;
			if (!d->state)
				if (try_connection(d))
					goto again;
		}
		lc = ld->prev;
	}
again2:
	foreachback(struct connection, c, lc, queue) {
		if (getpri(c) < PRI_CANCEL)
			break;
		if (c->state == S_WAIT) {
			setcstate(c, S_INTERRUPTED);
			del_connection(c);
			goto again2;
		} else if (c->est_length > (long)memory_cache_size * MAX_CACHED_OBJECT
		|| c->from > (long)memory_cache_size * MAX_CACHED_OBJECT) {
			setcstate(c, S_INTERRUPTED);
			abort_connection(c);
			goto again2;
		}
	}
}

unsigned char *get_proxy_string(unsigned char *url)
{
	if (is_noproxy_url(url))
		return NULL;
	if (*proxies.http_proxy && !casecmp(url, cast_uchar "http://", 7))
		return proxies.http_proxy;
	if (*proxies.https_proxy && !casecmp(url, cast_uchar "https://", 8))
		return proxies.https_proxy;
	return NULL;
}

unsigned char *get_proxy(unsigned char *url)
{
	unsigned char *proxy = get_proxy_string(url);
	unsigned char *u;
	u = stracpy(cast_uchar "");
	if (proxy) {
		add_to_strn(&u, cast_uchar "proxy://");
		add_to_strn(&u, proxy);
		add_to_strn(&u, cast_uchar "/");
	}
	add_to_strn(&u, url);
	return u;
}

int is_proxy_url(unsigned char *url)
{
	return !casecmp(url, cast_uchar "proxy://", 8);
}

unsigned char *remove_proxy_prefix(unsigned char *url)
{
	unsigned char *x = NULL;
	if (is_proxy_url(url))
		x = get_url_data(url);
	if (!x)
		x = url;
	return x;
}

int get_allow_flags(unsigned char *url)
{
	return	!casecmp(url, cast_uchar "smb://", 6) ? ALLOW_SMB :
		!casecmp(url, cast_uchar "file://", 7) ? ALLOW_FILE : 0;
}

int disallow_url(unsigned char *url, int allow_flags)
{
	if (!casecmp(url, cast_uchar "smb://", 6) && !(allow_flags & ALLOW_SMB))
		return S_SMB_NOT_ALLOWED;
	if (!casecmp(url, cast_uchar "file://", 7)
	&& !(allow_flags & ALLOW_FILE))
		return S_FILE_NOT_ALLOWED;
	return 0;
}

/* prev_url is a pointer to previous url or NULL */
/* prev_url will NOT be deallocated */
void load_url(unsigned char *url, unsigned char *prev_url, struct status *stat,
	int pri, int no_cache, int no_compress, int allow_flags, off_t position)
{
	struct cache_entry *e = NULL;
	struct connection *c;
	struct list_head *lc;
	unsigned char *u;
	int must_detach = 0;
	const int err = disallow_url(url, allow_flags);

	if (stat) {
		stat->c = NULL;
		stat->ce = NULL;
		stat->state = S_OUT_OF_MEM;
		stat->prev_error = 0;
		stat->pri = pri;
	}
	if (err) {
		if (stat) {
			stat->state = err;
			if (stat->end) stat->end(stat, stat->data);
		}
		goto ret;
	}
	if (no_cache <= NC_CACHE && !find_in_cache(url, &e)) {
		if (e->incomplete) {
			e->refcount--;
			goto skip_cache;
		}
		if (!aggressive_cache && no_cache > NC_ALWAYS_CACHE) {
			if (e->expire_time && e->expire_time < time(NULL)) {
				if (no_cache < NC_IF_MOD)
					no_cache = NC_IF_MOD;
				e->refcount--;
				goto skip_cache;
			}
		}
		if (no_compress) {
			unsigned char *enc;
			enc = parse_http_header(e->head,
					cast_uchar "Content-Encoding", NULL);
			if (enc) {
				free(enc);
				e->refcount--;
				must_detach = 1;
				goto skip_cache;
			}
		}
		if (stat) {
			stat->ce = e;
			stat->state = S__OK;
			if (stat->end)
				stat->end(stat, stat->data);
		}
		e->refcount--;
		goto ret;
	}
skip_cache:
	if (is_proxy_url(url)) {
		if (stat) {
			stat->state = S_BAD_URL;
			if (stat->end)
				stat->end(stat, stat->data);
		}
		goto ret;
	}
	u = get_proxy(url);
	foreach(struct connection, c, lc, queue)
		if (!c->detached && !strcmp((const char *)c->url, (const char *)u)) {
			if (c->from < position)
				continue;
			if (no_compress && !c->no_compress) {
				unsigned char *enc;
				if ((c->state >= S_WAIT && c->state < S_TRANS)
				|| !c->cache) {
					must_detach = 1;
					break;
				}
				enc = parse_http_header(c->cache->head,
						cast_uchar "Content-Encoding",
						NULL);
				if (enc) {
					free(enc);
					must_detach = 1;
					break;
				}
			}
			free(u);
			if (getpri(c) > pri) {
				del_from_list(c);
				c->pri[pri]++;
				add_to_queue(c);
				register_bottom_half(check_queue, NULL);
			} else
				c->pri[pri]++;
			if (stat) {
				stat->prg = &c->prg;
				stat->c = c;
				stat->ce = c->cache;
				add_to_list(c->statuss, stat);
				setcstate(c, c->state);
			}
			goto ret;
		}
	c = mem_calloc(sizeof(struct connection));
	c->count = connection_count++;
	c->url = u;
	c->prev_url = stracpy(prev_url);
	c->running = 0;
	c->prev_error = 0;
	if (position || must_detach)
		c->from = position;
	else if (no_cache >= NC_IF_MOD || !e)
		c->from = 0;
	else {
		struct fragment *frag;
		struct list_head *lfrag;
		c->from = 0;
		foreach(struct fragment, frag, lfrag, e->frag) {
			if (frag->offset != c->from)
				break;
			c->from += frag->length;
		}
	}
	memset(c->pri, 0, sizeof c->pri);
	c->pri[pri] = 1;
	c->no_cache = no_cache;
	c->sock1 = c->sock2 = -1;
	c->dnsquery = NULL;
	c->tries = 0;
	c->netcfg_stamp = netcfg_stamp;
	init_list(c->statuss);
	c->info = NULL;
	c->buffer = NULL;
	c->newconn = NULL;
	c->cache = NULL;
	c->est_length = -1;
	c->unrestartable = 0;
	c->no_compress = http_options.no_compression || no_compress || dmp == D_SOURCE;
	c->prg.timer = NULL;
	c->timer = NULL;
	if (position || must_detach) {
		if (new_cache_entry(cast_uchar "", &c->cache)) {
			free(c->url);
			free(c->prev_url);
			free(c);
			if (stat) {
				stat->state = S_OUT_OF_MEM;
				if (stat->end)
					stat->end(stat, stat->data);
			}
			goto ret;
		}
		c->cache->refcount--;
		detach_cache_entry(c->cache);
		c->detached = 2;
	}
	if (stat) {
		stat->prg = &c->prg;
		stat->c = c;
		stat->ce = NULL;
		add_to_list(c->statuss, stat);
	}
	add_to_queue(c);
	setcstate(c, S_WAIT);
	register_bottom_half(check_queue, NULL);

ret:
	return;
}

void change_connection(struct status *oldstat, struct status *newstat, int newpri)
{
	struct connection *c;
	const int oldpri = oldstat->pri;
	if (oldstat->state < 0) {
		if (newstat) {
			struct cache_entry *ce = oldstat->ce;
			if (ce)
				ce->refcount++;
			newstat->ce = oldstat->ce;
			newstat->state = oldstat->state;
			newstat->prev_error = oldstat->prev_error;
			if (newstat->end)
				newstat->end(newstat, newstat->data);
			if (ce)
				ce->refcount--;
		}
		return;
	}
	c = oldstat->c;
	if (--c->pri[oldpri] < 0) {
		internal("priority counter underflow");
		c->pri[oldpri] = 0;
	}
	c->pri[newpri]++;
	del_from_list(oldstat);
	oldstat->state = S_INTERRUPTED;
	if (newstat) {
		newstat->prg = &c->prg;
		add_to_list(c->statuss, newstat);
		newstat->state = c->state;
		newstat->prev_error = c->prev_error;
		newstat->pri = newpri;
		newstat->c = c;
		newstat->ce = c->cache;
	}
	if (c->detached && !newstat) {
		setcstate(c, S_INTERRUPTED);
		abort_connection(c);
	}
	sort_queue();
	register_bottom_half(check_queue, NULL);
}

void detach_connection(struct status *stat, off_t pos, int want_to_restart, int dont_check_refcount)
{
	struct connection *c;
	int i, n_users;
	off_t l;
	if (stat->state < 0)
		return;
	c = stat->c;
	if (c->no_compress && want_to_restart)
		return;
	if (!c->cache || (!dont_check_refcount && c->cache->refcount))
		return;
	want_to_restart |= pos > c->from && is_connection_seekable(c);
	if (c->detached)
		goto detach_done;
	if (c->est_length == -1)
		l = c->from;
	else
		l = c->est_length;
	if (!dont_check_refcount
	&& l < (long)memory_cache_size * MAX_CACHED_OBJECT && !want_to_restart)
		return;
	n_users = 0;
	for (i = 0; i < PRI_CANCEL; i++)
		n_users += c->pri[i];
	if (!n_users)
		internal("detaching free connection");
	if (n_users != 1)
		return;
	shrink_memory(SH_CHECK_QUOTA);
	detach_cache_entry(c->cache);
	c->detached = 1;
detach_done:
	free_entry_to(c->cache, pos);

	if (c->detached < 2 && want_to_restart) {
		int running = c->running;
		c->no_compress = 1;
		if (running)
			interrupt_connection(c);
		c->from = pos;
		if (running)
			run_connection(c);
		c->detached = 2;
	}
}

static uttime get_timeout_value(struct connection *c)
{
	uttime t;
	if (c->state == S_CONN || c->state == S_CONN_ANOTHER)
		t = timeout_multiple_addresses * (c->tries < 1 ? 1 : c->tries + 1);
	else if (c->unrestartable)
		t = unrestartable_receive_timeout;
	else
		t = receive_timeout;
	t *= 500;
	return t;
}

static void connection_timeout(void *c_)
{
	struct connection *c = (struct connection *)c_;
	c->timer = NULL;
	if (c->state == S_CONN || c->state == S_CONN_ANOTHER) {
		retry_connect(c, get_error_from_errno(ETIMEDOUT), 0);
		return;
	}
	setcstate(c, S_TIMEOUT);
	retry_connection(c);
}

static void connection_timeout_1(void *c_)
{
	struct connection *c = (struct connection *)c_;
	c->timer = install_timer(get_timeout_value(c), connection_timeout, c);
}

void clear_connection_timeout(struct connection *c)
{
	if (c->timer) {
		kill_timer(c->timer);
		c->timer = NULL;
	}
}

void set_connection_timeout(struct connection *c)
{
	clear_connection_timeout(c);
	c->timer = install_timer(get_timeout_value(c), connection_timeout_1, c);
}

void abort_all_connections(void)
{
	while (!list_empty(queue)) {
		struct connection *c = list_struct(queue.next, struct connection);
		setcstate(c, S_INTERRUPTED);
		abort_connection(c);
	}
	abort_all_keepalive_connections();
}

void abort_background_connections(void)
{
	struct connection *c;
	struct list_head *lc;
again:
	foreach(struct connection, c, lc, queue) {
		if (getpri(c) >= PRI_CANCEL) {
			setcstate(c, S_INTERRUPTED);
			abort_connection(c);
			goto again;
		}
	}
	abort_all_keepalive_connections();
}

int is_entry_used(struct cache_entry *e)
{
	struct connection *c;
	struct list_head *lc;
	foreach(struct connection, c, lc, queue)
		if (c->cache == e)
			return 1;
	return 0;
}

struct blacklist_entry {
	list_entry_1st
	int flags;
	list_entry_last
	unsigned char host[1];
};

static struct list_head blacklist = { &blacklist, &blacklist };

void add_blacklist_entry(unsigned char *host, int flags)
{
	struct blacklist_entry *b;
	struct list_head *lb;
	size_t sl;
	foreach(struct blacklist_entry, b, lb, blacklist)
		if (!casestrcmp(host, b->host)) {
			b->flags |= flags;
			return;
		}
	sl = strlen((const char *)host);
	if (sl > INT_MAX - sizeof(struct blacklist_entry))
		overalloc();
	b = xmalloc(sizeof(struct blacklist_entry) + sl);
	b->flags = flags;
	strcpy((char *)b->host, (const char *)host);
	add_to_list(blacklist, b);
}

void del_blacklist_entry(unsigned char *host, int flags)
{
	struct blacklist_entry *b;
	struct list_head *lb;
	foreach(struct blacklist_entry, b, lb, blacklist)
		if (!casestrcmp(host, b->host)) {
			b->flags &= ~flags;
			if (!b->flags) {
				del_from_list(b);
				free(b);
			}
			return;
		}
}

int get_blacklist_flags(unsigned char *host)
{
	struct blacklist_entry *b;
	struct list_head *lb;
	foreach(struct blacklist_entry, b, lb, blacklist)
		if (!casestrcmp(host, b->host))
			return b->flags;
	return 0;
}

void free_blacklist(void)
{
	free_list(struct blacklist_entry, blacklist);
}
