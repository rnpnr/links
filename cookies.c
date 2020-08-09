/* cookies.c
 * Cookies
 * (c) 2002 Mikulas Patocka
 * This file is a part of the Links program, released under GPL
 */

#include <limits.h>

#include "links.h"

#define ACCEPT_NONE	0
#define ACCEPT_ALL	1

static int accept_cookies = ACCEPT_ALL;

struct list_head all_cookies = { &all_cookies, &all_cookies };

struct list_head c_domains = { &c_domains, &c_domains };

struct c_server {
	list_entry_1st
	int accpt;
	unsigned char server[1];
};

static struct list_head c_servers = { &c_servers, &c_servers };

static void accept_cookie(struct cookie *);

void free_cookie(struct cookie *c)
{
	free(c->name);
	free(c->value);
	free(c->server);
	free(c->path);
	free(c->domain);
	free(c);
}


/* sezere 1 cookie z retezce str, na zacatku nesmi byt zadne whitechars
 * na konci muze byt strednik nebo 0
 * cookie musi byt ve tvaru nazev=hodnota, kolem rovnase nesmi byt zadne mezery
 * (respektive mezery se budou pocitat do nazvu a do hodnoty)
 */
int set_cookie(struct terminal *term, unsigned char *url, unsigned char *str)
{
	if (!accept_cookies)
		return 0;
	int noval = 0;
	struct cookie *cookie;
	struct c_server *cs = NULL;
	struct list_head *lcs;
	unsigned char *p, *q, *s, *server, *date, *dom;
	for (p = str; *p != ';' && *p; p++);
	for (q = str; *q != '='; q++)
		if (!*q || q >= p) {
			noval = 1;
			break;
		}
	if (str == q || q + 1 == p)
		return 0;
	cookie = xmalloc(sizeof(struct cookie));
	server = get_host_name(url);
	cookie->name = memacpy(str, q - str);
	cookie->value = !noval ? memacpy(q + 1, p - q - 1) : NULL;
	cookie->server = stracpy(server);
	date = parse_header_param(str, cast_uchar "expires", 0);
	if (date) {
		cookie->expires = parse_http_date(date);
		free(date);
	} else
		cookie->expires = 0;
	if (!(cookie->path = parse_header_param(str, cast_uchar "path", 0)))
		cookie->path = stracpy(cast_uchar "/");
	else if (cookie->path[0] != '/') {
		add_to_strn(&cookie->path, cast_uchar "x");
		memmove(cookie->path + 1, cookie->path, strlen((const char *)cookie->path) - 1);
		cookie->path[0] = '/';
	}
	dom = parse_header_param(str, cast_uchar "domain", 0);
	if (!dom)
		cookie->domain = stracpy(server);
	else {
		cookie->domain = idn_encode_host(dom, strlen((const char *)dom), cast_uchar ".", 0);
		if (!cookie->domain)
			cookie->domain = stracpy(server);
		free(dom);
	}
	if (cookie->domain[0] == '.')
		memmove(cookie->domain, cookie->domain + 1,
			strlen((const char *)cookie->domain));
	if ((s = parse_header_param(str, cast_uchar "secure", 0))) {
		cookie->secure = 1;
		free(s);
	} else
		cookie->secure = 0;
	if (!allow_cookie_domain(server, cookie->domain)) {
		free(cookie->domain);
		cookie->domain = stracpy(server);
	}
	foreach(struct c_server, cs, lcs, c_servers)
		if (!casestrcmp(cs->server, server)) {
			if (cs->accpt)
				goto ok;
			else {
				free_cookie(cookie);
				free(server);
				return 0;
			}
		}
	if (!accept_cookies) {
		free_cookie(cookie);
		free(server);
		return 1;
	}
ok:
	accept_cookie(cookie);
	free(server);
	return 0;
}

static void accept_cookie(struct cookie *c)
{
	struct c_domain *cd = NULL;
	struct list_head *lcd;
	struct cookie *d = NULL;
	struct list_head *ld;
	size_t sl;
	foreach(struct cookie, d, ld, all_cookies)
		if (!casestrcmp(d->name, c->name)
		&& !casestrcmp(d->domain, c->domain)) {
			ld = ld->prev;
			del_from_list(d);
			free_cookie(d);
		}
	if (c->value && !casestrcmp(c->value, cast_uchar "deleted")) {
		free_cookie(c);
		return;
	}
	add_to_list(all_cookies, c);
	foreach(struct c_domain, cd, lcd, c_domains)
		if (!casestrcmp(cd->domain, c->domain))
			return;
	sl = strlen((const char *)c->domain);
	if (sl > INT_MAX - sizeof(struct c_domain))
		overalloc();
	cd = xmalloc(sizeof(struct c_domain) + sl);
	strcpy(cast_char cd->domain, cast_const_char c->domain);
	add_to_list(c_domains, cd);
}

int is_in_domain(unsigned char *d, unsigned char *s)
{
	const int dl = strlen((const char *)d);
	const int sl = strlen((const char *)s);
	if (dl > sl)
		return 0;
	if (dl == sl)
		return !casestrcmp(d, s);
	if (s[sl - dl - 1] != '.')
		return 0;
	return !casecmp(d, s + sl - dl, dl);
}

int is_path_prefix(unsigned char *d, unsigned char *s)
{
	const int dl = strlen((const char *)d);
	const int sl = strlen((const char *)s);
	if (!dl)
		return 1;
	if (dl > sl)
		return 0;
	if (memcmp(d, s, dl))
		return 0;
	return d[dl - 1] == '/' || !s[dl] || s[dl] == '/' || s[dl] == POST_CHAR || s[dl] == '?' || s[dl] == '&';
}

int cookie_expired(struct cookie *c)
{
	time_t t;
	errno = 0;
	EINTRLOOPX(t, time(NULL), (time_t)-1);
	return c->expires && c->expires < t;
}

void add_cookies(unsigned char **s, int *l, unsigned char *url)
{
	int nc = 0;
	struct c_domain *cd = NULL;
	struct list_head *lcd;
	struct cookie *c = NULL;
	struct list_head *lc;
	unsigned char *server = get_host_name(url);
	unsigned char *data = get_url_data(url);
	if (data > url)
		data--;
	foreach(struct c_domain, cd, lcd, c_domains)
		if (is_in_domain(cd->domain, server))
			goto ok;
	free(server);
	return;
ok:
	foreachback(struct cookie, c, lc, all_cookies)
		if (is_in_domain(c->domain, server))
			if (is_path_prefix(c->path, data)) {
				if (cookie_expired(c)) {
					lc = lc->prev;
					del_from_list(c);
					free_cookie(c);
					continue;
				}
				if (c->secure
				&& casecmp(url, cast_uchar "https://", 8))
					continue;
				if (!nc) {
					add_to_str(s, l, cast_uchar "Cookie: ");
					nc = 1;
				} else
					add_to_str(s, l, cast_uchar "; ");
				add_to_str(s, l, c->name);
				if (c->value) {
					add_chr_to_str(s, l, '=');
					add_to_str(s, l, c->value);
				}
			}
	if (nc)
		add_to_str(s, l, cast_uchar "\r\n");
	free(server);
}

void free_cookies(void)
{
	free_list(struct c_domain, c_domains);
	/* !!! FIXME: save cookies */
	while (!list_empty(all_cookies)) {
		struct cookie *c = list_struct(all_cookies.next, struct cookie);
		del_from_list(c);
		free_cookie(c);
	}
}

void init_cookies(void)
{
	/* !!! FIXME: read cookies */
}

