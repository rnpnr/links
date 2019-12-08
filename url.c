/* url.c
 * (c) 2002 Mikulas Patocka
 * This file is a part of the Links program, released under GPL.
 */

#include <string.h>

#include "links.h"

static const struct {
	char *prot;
	int port;
	void (*func)(struct connection *);
	void (*nc_func)(struct session *, unsigned char *);
	int free_syntax;
	int need_slashes;
	int need_slash_after_host;
	int allow_post;
	int bypasses_socks;
} protocols[]= {
		{"data", 0, data_func, NULL,		1, 0, 0, 0, 0},
		{"file", 0, file_func, NULL,		1, 1, 0, 0, 1},
		{"https", 443, https_func, NULL,	0, 1, 1, 1, 0},
		{"http", 80, http_func, NULL,		0, 1, 1, 1, 0},
		{"proxy", 3128, proxy_func, NULL,	0, 1, 1, 1, 0},
		{NULL, 0, NULL, NULL,			0, 0, 0, 0, 0}
};



static int check_protocol(unsigned char *p, size_t l)
{
	int i;
	for (i = 0; protocols[i].prot; i++)
		if (!casecmp(cast_uchar protocols[i].prot, p, l)
		&& strlen(protocols[i].prot) == l)
			return i;
	return -1;
}

static int get_prot_info(unsigned char *prot, int *port, void (**func)(struct connection *), void (**nc_func)(struct session *ses, unsigned char *), int *allow_post, int *bypasses_socks)
{
	int i;
	for (i = 0; protocols[i].prot; i++)
		if (!casestrcmp(cast_uchar protocols[i].prot, prot)) {
			if (port)
				*port = protocols[i].port;
			if (func)
				*func = protocols[i].func;
			if (nc_func)
				*nc_func = protocols[i].nc_func;
			if (allow_post)
				*allow_post = protocols[i].allow_post;
			if (bypasses_socks)
				*bypasses_socks = protocols[i].bypasses_socks;
			return 0;
		}
	return -1;
}

int parse_url(unsigned char *url, int *prlen, unsigned char **user, int *uslen, unsigned char **pass, int *palen, unsigned char **host, int *holen, unsigned char **port, int *polen, unsigned char **data, int *dalen, unsigned char **post)
{
	unsigned char *p, *q;
	unsigned char p_c[2];
	int a;
	if (prlen)
		*prlen = 0;
	if (user)
		*user = NULL;
	if (uslen)
		*uslen = 0;
	if (pass)
		*pass = NULL;
	if (palen)
		*palen = 0;
	if (host)
		*host = NULL;
	if (holen)
		*holen = 0;
	if (port)
		*port = NULL;
	if (polen)
		*polen = 0;
	if (data)
		*data = NULL;
	if (dalen)
		*dalen = 0;
	if (post)
		*post = NULL;
	if (!url || !(p = cast_uchar strchr(cast_const_char url, ':')))
		return -1;
	if (prlen)
		*prlen = (int)(p - url);
	if ((a = check_protocol(url, p - url)) == -1)
		return -1;
	if (p[1] != '/' || p[2] != '/') {
		if (protocols[a].need_slashes)
			return -1;
		p -= 2;
	}
	if (protocols[a].free_syntax) {
		if (data)
			*data = p + 3;
		if (dalen)
			*dalen = strlen((char *)(p + 3));
		return 0;
	}
	p += 3;
	q = p + strcspn(cast_const_char p, "@/?");
	if (!*q && protocols[a].need_slash_after_host) return -1;
	if (*q == '@') {
		unsigned char *pp;
		while (strcspn(cast_const_char(q + 1), "@") < strcspn(cast_const_char(q + 1), "/?"))
			q = q + 1 + strcspn(cast_const_char(q + 1), "@");
		pp = cast_uchar strchr(cast_const_char p, ':');
		if (!pp || pp > q) {
			if (user)
				*user = p;
			if (uslen)
				*uslen = (int)(q - p);
		} else {
			if (user)
				*user = p;
			if (uslen)
				*uslen = (int)(pp - p);
			if (pass)
				*pass = pp + 1;
			if (palen)
				*palen = (int)(q - pp - 1);
		}
		p = q + 1;
	}
	if (p[0] == '[') {
		q = cast_uchar strchr((char *)p, ']');
		if (q) {
			q++;
			goto have_host;
		}
	}
	q = p + strcspn((char *)p, ":/?");
	have_host:
	if (!*q && protocols[a].need_slash_after_host)
		return -1;
	if (host)
		*host = p;
	if (holen)
		*holen = (int)(q - p);
	if (*q == ':') {
		unsigned char *pp = q + strcspn((char *)q, "/");
		int cc;
		if (*pp != '/' && protocols[a].need_slash_after_host)
			return -1;
		if (port)
			*port = q + 1;
		if (polen)
			*polen = (int)(pp - q - 1);
		for (cc = 0; cc < pp - q - 1; cc++)
			if (q[cc+1] < '0' || q[cc+1] > '9')
				return -1;
		q = pp;
	}
	if (*q && *q != '?')
		q++;
	p = q;
	p_c[0] = POST_CHAR;
	p_c[1] = 0;
	q = p + strcspn((char *)p, (char *)p_c);
	if (data)
		*data = p;
	if (dalen)
		*dalen = (int)(q - p);
	if (post)
		*post = *q ? q + 1 : NULL;
	return 0;
}

unsigned char *get_protocol_name(unsigned char *url)
{
	int l;
	if (parse_url(url, &l, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
		NULL, NULL, NULL))
		return NULL;
	return memacpy(url, l);
}

unsigned char *get_keepalive_id(unsigned char *url)
{
	unsigned char *h, *p, *k, *d;
	int hl, pl;
	if (parse_url(url, NULL, NULL, NULL, NULL, NULL, &h, &hl, &p, &pl, &d,
		NULL, NULL))
		return NULL;
	if (is_proxy_url(url) && !casecmp(d, cast_uchar "https://", 8)) {
		if (parse_url(d, NULL, NULL, NULL, NULL, NULL, &h, &hl, &p, &pl,
			NULL, NULL, NULL))
			return NULL;
	}
	k = p ? p + pl : h ? h + hl : NULL;
	if (!k)
		return stracpy(cast_uchar "");
	return memacpy(url, k - url);
}

unsigned char *get_host_name(unsigned char *url)
{
	unsigned char *h;
	int hl;
	if (parse_url(url, NULL, NULL, NULL, NULL, NULL, &h, &hl, NULL, NULL,
		NULL, NULL, NULL))
		return stracpy(cast_uchar "");
	return memacpy(h, hl);
}

unsigned char *get_user_name(unsigned char *url)
{
	unsigned char *h;
	int hl;
	if (parse_url(url, NULL, &h, &hl, NULL, NULL, NULL, NULL, NULL, NULL,
		NULL, NULL, NULL))
		return NULL;
	return memacpy(h, hl);
}

unsigned char *get_pass(unsigned char *url)
{
	unsigned char *h;
	int hl;
	if (parse_url(url, NULL,NULL, NULL, &h, &hl, NULL, NULL, NULL, NULL,
		NULL, NULL, NULL))
		return NULL;
	return memacpy(h, hl);
}

unsigned char *get_port_str(unsigned char *url)
{
	unsigned char *h;
	int hl;
	if (parse_url(url, NULL, NULL, NULL, NULL, NULL, NULL, NULL, &h, &hl,
		NULL, NULL, NULL))
		return NULL;
	return hl ? memacpy(h, hl) : NULL;
}

int get_port(unsigned char *url)
{
	unsigned char *h;
	int hl;
	long n = -1;
	if (parse_url(url, NULL, NULL, NULL, NULL, NULL, NULL, NULL, &h, &hl,
		NULL, NULL, NULL))
		return -1;
	if (h) {
		n = strtol((char *)h, NULL, 10);
		if (n > 0 && n < 65536)
			return (int)n;
		return -1;
	}
	if ((h = get_protocol_name(url))) {
		int nn = -1;
		get_prot_info(h, &nn, NULL, NULL, NULL, NULL);
		free(h);
		n = nn;
	}
	return (int)n;
}

void (*get_protocol_handle(unsigned char *url))(struct connection *)
{
	unsigned char *p;
	void (*f)(struct connection *) = NULL;
	int post = 0;
	if (!(p = get_protocol_name(url)))
		return NULL;
	get_prot_info(p, NULL, &f, NULL, &post, NULL);
	free(p);
	if (!post && strchr(cast_const_char url, POST_CHAR))
		return NULL;
	return f;
}

void (*get_external_protocol_function(unsigned char *url))(struct session *, unsigned char *)
{
	unsigned char *p;
	void (*f)(struct session *, unsigned char *) = NULL;
	int post = 0;
	if (!(p = get_protocol_name(url)))
		return NULL;
	get_prot_info(p, NULL, NULL, &f, &post, NULL);
	free(p);
	if (!post && strchr(cast_const_char url, POST_CHAR))
		return NULL;
	return f;
}

int url_bypasses_socks(unsigned char *url)
{
	int ret = 0;
	unsigned char *p;
	if (!(p = get_protocol_name(url)))
		return 1;
	get_prot_info(p, NULL, NULL, NULL, NULL, &ret);
	free(p);
	return ret;
}

unsigned char *get_url_data(unsigned char *url)
{
	unsigned char *d;
	if (parse_url(url, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
		&d, NULL, NULL))
		return NULL;
	return d;
}

#define dsep(x) (lo ? dir_sep(x) : (x) == '/')

static void translate_directories(unsigned char *url)
{
	unsigned char *dd = get_url_data(url);
	unsigned char *s, *d;
	int lo = !casecmp(url, cast_uchar "file://", 7);
	if (!casecmp(url, cast_uchar "javascript:", 11))
		return;
	if (!casecmp(url, cast_uchar "magnet:", 7))
		return;
	if (!dd || dd == url)
		return;
	if (!dsep(*dd)) {
		dd--;
		if (!dsep(*dd)) {
			dd++;
			memmove(dd + 1, dd, strlen((char *)dd) + 1);
			*dd = '/';
		}
	}
	s = dd;
	d = dd;
r:
	if (end_of_dir(url, s[0])) {
		memmove(d, s, strlen((char *)s) + 1);
		return;
	}
	if (dsep(s[0]) && s[1] == '.' && (dsep(s[2]) || !s[2]
	|| end_of_dir(url, s[2]))) {
		if (!dsep(s[2]))
			*d++ = *s;
		s += 2;
		goto r;
	}
	if (dsep(s[0]) && s[1] == '.' && s[2] == '.' && (dsep(s[3]) || !s[3]
	|| end_of_dir(url, s[3]))) {
		while (d > dd) {
			d--;
			if (dsep(*d))
				goto b;
		}
b:
		if (!dsep(s[3]))
			*d++ = *s;
		s += 3;
		goto r;
	}
	if ((*d++ = *s++))
		goto r;
}

static unsigned char *translate_hashbang(unsigned char *up)
{
	unsigned char *u, *p, *dp, *data, *post_seq;
	int q;
	unsigned char *r;
	int rl;
	if (!strstr((char *)up, "#!") && !strstr((char *)up, "#%21"))
		return up;
	u = stracpy(up);
	p = extract_position(u);
	if (!p) {
		free_u_ret_up:
		free(u);
		return up;
	}
	if (p[0] == '!')
		dp = p + 1;
	else if (!casecmp(p, cast_uchar "%21", 3))
		dp = p + 3;
	else {
		free(p);
		goto free_u_ret_up;
	}
	if (!(post_seq = cast_uchar strchr((char *)u, POST_CHAR)))
		post_seq = cast_uchar strchr((char *)u, 0);
	data = get_url_data(u);
	if (!data)
		data = u;
	r = init_str();
	rl = 0;
	add_bytes_to_str(&r, &rl, u, post_seq - u);
	q = strlen((char *)data);
	if (q && (data[q - 1] == '&' || data[q - 1] == '?'))
		;
	else if (strchr((char *)data, '?'))
		add_chr_to_str(&r, &rl, '&');
	else
		add_chr_to_str(&r, &rl, '?');
	add_to_str(&r, &rl, cast_uchar "_escaped_fragment_=");
	for (; *dp; dp++) {
		unsigned char c = *dp;
		if (c <= 0x20 || c == 0x23 || c == 0x25 || c == 0x26
		|| c == 0x2b || c >= 0x7f) {
			unsigned char h[4];
			sprintf((char *)h, "%%%02X", c);
			add_to_str(&r, &rl, h);
		} else
			add_chr_to_str(&r, &rl, c);
	}
	add_to_str(&r, &rl, post_seq);
	free(u);
	free(p);
	free(up);
	return r;
}

static unsigned char *rewrite_url_google_docs(unsigned char *n)
{
	int i;
	unsigned char *id, *id_end, *url_end;
	unsigned char *res;
	int l;
	struct {
		const char *beginning;
		const char *result1;
		const char *result2;
	} const patterns[] = {
		{ "https://docs.google.com/document/d/", "https://docs.google.com/document/d/", "/export?format=pdf" },
		{ "https://docs.google.com/document/u/", "https://docs.google.com/document/u/", "/export?format=pdf" },
		{ "https://docs.google.com/spreadsheets/d/", "https://docs.google.com/spreadsheets/d/", "/export?format=pdf" },
		{ "https://docs.google.com/spreadsheets/u/", "https://docs.google.com/spreadsheets/u/", "/export?format=pdf" },
		{ "https://docs.google.com/presentation/d/", "https://docs.google.com/presentation/d/", "/export/pdf" },
		{ "https://docs.google.com/presentation/u/", "https://docs.google.com/presentation/u/", "/export/pdf" },
		{ "https://drive.google.com/file/d/", "https://drive.google.com/uc?export=download&id=", "" },
		{ "https://drive.google.com/file/u/", "https://drive.google.com/uc?export=download&id=", "" }
	};
	for (i = 0; i < array_elements(patterns); i++)
		if (!cmpbeg(n, cast_uchar patterns[i].beginning))
			goto match;
	return n;
match:
	id = n + strlen(patterns[i].beginning);
	url_end = id + strcspn(cast_const_char id, "#" POST_CHAR_STRING);
	id_end = memchr(id, '/', url_end - id);
	if (!id_end)
		return n;
	if (!cmpbeg(id_end, cast_uchar "/export"))
		return n;
	if (!patterns[i].result2[0]) {
		id = id_end;
		while (id[-1] != '/')
			id--;
	}
	res = init_str();
	l = 0;
	add_to_str(&res, &l, cast_uchar patterns[i].result1);
	add_bytes_to_str(&res, &l, id, id_end - id);
	add_to_str(&res, &l, cast_uchar patterns[i].result2);
	free(n);
	return res;
}

static unsigned char *rewrite_url_mediawiki_svg(unsigned char *n)
{
	const char u1[] = "/media/math/render/svg/";
	const char u2[] = "/media/math/render/png/";
	unsigned char *d, *s;
	d = get_url_data(n);
	if (!d)
		return n;
	s = cast_uchar strstr((char *)d, u1);
	if (!s)
		return n;
	memcpy(s, u2, strlen(u2));
	return n;
}

static unsigned char *rewrite_url(unsigned char *n)
{
	extend_str(&n, 1);
	translate_directories(n);
	n = translate_hashbang(n);
	n = rewrite_url_google_docs(n);
	n = rewrite_url_mediawiki_svg(n);
	return n;
}

static int test_qualified_name(char *host, char *hostname)
{
	char *c;
	if (!strcasecmp(host, hostname))
		return 1;
	c = strchr(hostname, '.');
	if (c) {
		*c = 0;
		if (!strcasecmp(host, hostname))
			return 1;
	}
	return 0;
}

static int is_local_host(char *host)
{
	if (!*host)
		return 1;
	if (!strcasecmp(host, "localhost"))
		return 1;
	{
		int rs;
		char n[4096];
		n[0] = 0;
		EINTRLOOP(rs, gethostname(n, sizeof(n)));
		n[sizeof(n) - 1] = 0;
		if (!rs && strlen(n) < sizeof(n) - 1) {
			if (test_qualified_name(host, n))
				return 1;
		}
	}
	return 0;
		
}

static void insert_wd(unsigned char **up, unsigned char *cwd)
{
	unsigned char *u = *up;
	unsigned char *cw;
	unsigned char *url;
	char *host;
	int url_l;
	int i;
	if (!u || !cwd || !*cwd)
		return;
	if (casecmp(u, cast_uchar "file://", 7))
		return;
	for (i = 7; u[i] && !dir_sep(u[i]); i++);
	host = cast_char memacpy(u + 7, i - 7);
	if (is_local_host(host)) {
		free(host);
		memmove(u + 7, u + i, strlen(cast_const_char (u + i)) + 1);
		return;
	}
	free(host);
	url = init_str();
	url_l = 0;
	add_bytes_to_str(&url, &url_l, u, 7);
	for (cw = cwd; *cw; cw++) {
		unsigned char c = *cw;
		if (c < ' ' || c == '%' || c >= 127) {
			unsigned char h[4];
			sprintf((char *)h, "%%%02X", (unsigned)c & 0xff);
			add_to_str(&url, &url_l, h);
		} else
			add_chr_to_str(&url, &url_l, c);
	}
	if (!dir_sep(cwd[strlen((char *)cwd) - 1]))
		add_chr_to_str(&url, &url_l, '/');
	add_to_str(&url, &url_l, u + 7);
	free(u);
	*up = url;
}

int url_non_ascii(unsigned char *url)
{
	unsigned char *ch;
	for (ch = url; *ch; ch++)
		if (*ch >= 128)
			return 1;
	return 0;
}

static unsigned char *translate_idn(unsigned char *nu, int canfail)
{
	if (url_non_ascii(nu)) {
		unsigned char *id = idn_encode_url(nu, 0);
		if (!id) {
			if (!canfail)
				return nu;
			free(nu);
			return NULL;
		}
		free(nu);
		return id;
	}
	return nu;
}

/*
 * U funkce join_urls musi byt prvni url absolutni (takove, co projde funkci
 * parse_url bez chyby --- pokud neni absolutni, tak to spatne na internal) a
 * druhe url je relativni cesta vuci nemu nebo taky absolutni url. Pokud je
 * druhe url absolutni, vrati se to; pokud je relativni, tak se spoji prvni a
 * druhe url.
 */
unsigned char *join_urls(unsigned char *base, unsigned char *rel)
{
	unsigned char *p, *n, *pp, *ch;
	int l;
	int lo = !casecmp(base, cast_uchar "file://", 7);
	int data = !casecmp(base, cast_uchar "data:", 5);
	if (rel[0] == '#' || !rel[0]) {
		n = stracpy(base);
		for (p = n; *p && *p != POST_CHAR && *p != '#'; p++)
			;
		*p = 0;
		add_to_strn(&n, rel);
		goto return_n;
	}
	if (rel[0] == '?' || rel[0] == '&') {
		unsigned char rj[3];
		unsigned char *d = get_url_data(base);
		if (!d)
			goto bad_base;
		rj[0] = rel[0];
		rj[1] = POST_CHAR;
		rj[2] = 0;
		d += strcspn((char *)d, (char *)rj);
		n = memacpy(base, d - base);
		add_to_strn(&n, rel);
		goto return_n;
	}
	if (rel[0] == '/' && rel[1] == '/' && !data) {
		unsigned char *s;
		if (!(s = cast_uchar strstr(cast_const_char base, "//"))) {
			if (!(s = cast_uchar strchr(cast_const_char base, ':'))) {
bad_base:
				internal("bad base url: %s", base);
				return NULL;
			}
			s++;
		}
		n = memacpy(base, s - base);
		add_to_strn(&n, rel);
		if (!parse_url(n, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
			NULL, NULL, NULL, NULL, NULL))
			goto return_n;
		add_to_strn(&n, cast_uchar "/");
		if (!parse_url(n, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
			NULL, NULL, NULL, NULL, NULL))
			goto return_n;
		free(n);
	}
	if (is_proxy_url(rel))
		goto prx;
	if (!parse_url(rel, &l, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
		NULL, NULL, NULL)) {
		n = stracpy(rel);
		goto return_n;
	}
	n = stracpy(rel);
	while (n[0] && n[strlen((char *)n) - 1] <= ' ') n[strlen((char *)n) - 1] = 0;
	extend_str(&n, 1);
	ch = cast_uchar strrchr((char *)n, '#');
	if (!ch || strchr((char *)ch, '/'))
		ch = n + strlen((char *)n);
	memmove(ch + 1, ch, strlen((char *)ch) + 1);
	*ch = '/';
	if (!parse_url(n, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
		NULL, NULL, NULL))
		goto return_n;
	free(n);
prx:
	if (parse_url(base, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
		NULL, &p, NULL, NULL) || !p) {
		goto bad_base;
	}
	if (!dsep(*p))
		p--;
	if (!data) {
		if (end_of_dir(base, rel[0]))
			for (; *p; p++) {
				if (end_of_dir(base, *p))
					break;
		} else if (!dsep(rel[0]))
			for (pp = p; *pp; pp++) {
				if (end_of_dir(base, *pp))
					break;
				if (dsep(*pp))
					p = pp + 1;
		}
	}
	n = memacpy(base, p - base);
	add_to_strn(&n, rel);
	goto return_n;

return_n:
	n = translate_idn(n, 0);
	n = rewrite_url(n);
	return n;
}

unsigned char *translate_url(unsigned char *url, unsigned char *cwd)
{
	unsigned char *ch;
	unsigned char *nu, *da;
	unsigned char *prefix;
	int sl;
	while (*url == ' ')
		url++;
	if (*url && url[strlen((char *)url) - 1] == ' ') {
		nu = stracpy(url);
		while (*nu && nu[strlen((char *)nu) - 1] == ' ')
			nu[strlen((char *)nu) - 1] = 0;
		ch = translate_url(nu, cwd);
		free(nu);
		return ch;
	}
	if (is_proxy_url(url))
		return NULL;
	if (!parse_url(url, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
		NULL, &da, NULL, NULL)) {
		nu = stracpy(url);
		goto return_nu;
	}
	if (strchr((char *)url, POST_CHAR))
		return NULL;
	if (strstr((char *)url, "://")) {
		nu = stracpy(url);
		extend_str(&nu, 1);
		ch = cast_uchar strrchr((char *)nu, '#');
		if (!ch || strchr((char *)ch, '/'))
			ch = nu + strlen((char *)nu);
		memmove(ch + 1, ch, strlen((char *)ch) + 1);
		*ch = '/';
		if (!parse_url(nu, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
			NULL, NULL, NULL, NULL, NULL))
			goto return_nu;
		free(nu);
	}
	prefix = cast_uchar "file://";
	if (url[0] == '[' && strchr((char *)url, ']')) {
		ch = url;
		goto http;
	}
	ch = url + strcspn((char *)url, ".:/@");
	sl = 0;
	if (*ch != ':' || *(url + strcspn((char *)url, "/@")) == '@') {
		if (*url != '.' && *ch == '.') {
			unsigned char *e, *f, *g;
			int tl;
			for (e = ch + 1; *(f = e + strcspn((char *)e, ".:/")) == '.'; e = f + 1)
				;
			g = memacpy(e, f - e);
			tl = is_tld(g);
			free(g);
			if (tl) {
http:
				prefix = cast_uchar "http://";
				sl = 1;
			}
		}
		if (*ch == '@' || *ch == ':' || !cmpbeg(url, cast_uchar "ftp.")) {
			prefix = cast_uchar "ftp://";
			sl = 1;
		}
		nu = stracpy(prefix);
		add_to_strn(&nu, url);
		if (sl && !strchr((char *)url, '/')) add_to_strn(&nu, cast_uchar "/");
		if (parse_url(nu, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
			NULL, NULL, NULL, NULL, NULL)) {
			free(nu);
			return NULL;
		}
		goto return_nu;
	}
	nu = memacpy(url, ch - url + 1);
	add_to_strn(&nu, cast_uchar "//");
	add_to_strn(&nu, ch + 1);
	if (!parse_url(nu, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
		NULL, NULL, NULL))
		goto return_nu;
	add_to_strn(&nu, cast_uchar "/");
	if (!parse_url(nu, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
		NULL, NULL, NULL))
		goto return_nu;
	free(nu);
	return NULL;

return_nu:
	nu = translate_idn(nu, 1);
	if (!nu)
		return NULL;
	insert_wd(&nu, cwd);
	nu = rewrite_url(nu);
	return nu;
}

unsigned char *extract_position(unsigned char *url)
{
	unsigned char *u, *uu, *r;
	if ((u = get_url_data(url)))
		url = u;
	if (!(u = cast_uchar strchr((char *)url, POST_CHAR)))
		u = cast_uchar strchr((char *)url, 0);
	if (!(uu = memchr(url, '#', u - url)))
		return NULL;
	r = memacpy(uu + 1, u - uu - 1);
	memmove(uu, u, strlen((char *)u) + 1);
	return r;
}

int url_not_saveable(unsigned char *url)
{
	int p, palen;
	unsigned char *u = translate_url(url, cast_uchar "/");
	if (!u)
		return 1;
	p = parse_url(u, NULL, NULL, NULL, NULL, &palen, NULL, NULL, NULL, NULL,
		NULL, NULL, NULL);
	free(u);
	return p || palen;
}

#define accept_char(x)	((x) != 10 && (x) != 13 && (x) != '"' && (x) != '\'' && (x) != '&' && (x) != '<' && (x) != '>')
#define special_char(x)	((x) < ' ' || (x) == '%' || (x) == '#' || (x) >= 127)

/*
 * -2 percent to raw
 * -1 percent to html
 *  0 raw to html
 *  1 raw to percent
 */

void add_conv_str(unsigned char **s, int *l, unsigned char *b, int ll, int encode_special)
{
	for (; ll > 0; ll--, b++) {
		unsigned char chr = *b;
		if (!chr)
			continue;
		if (special_char(chr) && encode_special == 1) {
			unsigned char h[4];
			sprintf((char *)h, "%%%02X", (unsigned)chr & 0xff);
			add_to_str(s, l, h);
			continue;
		}
		if (chr == '%' && encode_special <= -1 && ll > 2
		&& ((b[1] >= '0' && b[1] <= '9') || (b[1] >= 'A' && b[1] <= 'F') || (b[1] >= 'a' && b[1] <= 'f'))
		&& ((b[2] >= '0' && b[2] <= '9') || (b[2] >= 'A' && b[2] <= 'F') || (b[2] >= 'a' && b[2] <= 'f'))) {
			int i;
			chr = 0;
			for (i = 1; i < 3; i++) {
				if (b[i] >= '0' && b[i] <= '9')
					chr = chr * 16 + b[i] - '0';
				if (b[i] >= 'A' && b[i] <= 'F')
					chr = chr * 16 + b[i] - 'A' + 10;
				if (b[i] >= 'a' && b[i] <= 'f')
					chr = chr * 16 + b[i] - 'a' + 10;
			}
			ll -= 2;
			b += 2;
			if (!chr)
				continue;
		}
		if (chr == ' ' && (!encode_special || encode_special == -1))
			add_to_str(s, l, cast_uchar "&nbsp;");
		else if (accept_char(chr) || encode_special == -2)
			add_chr_to_str(s, l, chr);
		else if (chr == 10 || chr == 13) {
			continue;
		} else {
			add_to_str(s, l, cast_uchar "&#");
			add_num_to_str(s, l, (int)chr);
			add_chr_to_str(s, l, ';');
		}
	}
}

void convert_file_charset(unsigned char **s, int *l, int start_l)
{
}

static const char xn[] = "xn--";
static const size_t xn_l = sizeof(xn) - 1;

#define puny_max_length	63
#define puny_base	36
#define puny_tmin	1
#define puny_tmax	26
#define puny_skew	38
#define puny_damp	700
#define puny_init_bias	72

static int ascii_allowed(unsigned c)
{
	return  c == '-'
		|| (c >= '0' && c <= '9')
		|| (c >= 'A' && c <= 'Z')
		|| (c >= 'a' && c <= 'z');
}

static unsigned char puny_chrenc(unsigned n)
{
	return n + (n < 26 ? 'a' : '0' - 26);
}

static unsigned puny_chrdec(unsigned char c)
{
	if (c <= '9')
		return c - '0' + 26;
	if (c <= 'Z')
		return c - 'A';
	return c - 'a';
}

struct puny_state {
	unsigned ascii_numpoints;
	unsigned numpoints;
	unsigned bias;
	unsigned k;
};

static void puny_init(struct puny_state *st, unsigned numpoints)
{
	st->ascii_numpoints = numpoints;
	st->numpoints = numpoints;
	st->bias = puny_init_bias;
	st->k = puny_base;
}

static unsigned puny_threshold(struct puny_state *st)
{
	unsigned k = st->k;
	st->k += puny_base;
	if (k <= st->bias)
		return puny_tmin;
	if (k >= st->bias + puny_tmax)
		return puny_tmax;
	return k - st->bias;
}

static void puny_adapt(struct puny_state *st, unsigned val)
{
	unsigned k;
	val = st->ascii_numpoints == st->numpoints ? val / puny_damp : val / 2;
	st->numpoints++;
	val += val / st->numpoints;
	k = 0;
	while (val > ((puny_base - puny_tmin) * puny_tmax) / 2) {
		val /= puny_base - puny_tmin;
		k += puny_base;
	}
	st->bias = k + (((puny_base - puny_tmin + 1) * val) / (val + puny_skew));
	st->k = puny_base;
}

static unsigned char *puny_encode(unsigned char *s, int len)
{
	unsigned char *p;
	unsigned *uni;
	unsigned uni_l;
	unsigned char *res;
	int res_l;
	unsigned i;
	unsigned ni, cchar, skip;
	struct puny_state st;

	if (len > 7 * puny_max_length)
		goto err;
	uni = xmalloc(len * sizeof(unsigned));
	uni_l = 0;
	for (p = s; p < s + len; ) {
		unsigned c;
		GET_UTF_8(p, c);
		c = uni_locase(c);
		if (c < 128 && !ascii_allowed(c))
			goto err_free_uni;
		if (c > 0x10FFFF)
			goto err_free_uni;
		uni[uni_l++] = c;
	}
	if (uni_l > puny_max_length)
		goto err_free_uni;

	res = init_str();
	res_l = 0;
	add_to_str(&res, &res_l, cast_uchar xn);

	ni = 0;
	for (i = 0; i < uni_l; i++)
		if (uni[i] < 128) {
			add_chr_to_str(&res, &res_l, uni[i]);
			ni++;
		}

	if (ni == uni_l) {
		memmove(res, res + xn_l, res_l - xn_l + 1);
		res_l -= 4;
		goto ret_free_uni;
	}

	if (res_l != xn_l)
		add_chr_to_str(&res, &res_l, '-');

	puny_init(&st, ni);

	cchar = 128;
	skip = 0;

	while (1) {
		unsigned dlen = 0;
		unsigned lchar = -1U;
		for (i = 0; i < uni_l; i++) {
			unsigned c = uni[i];
			if (c < cchar)
				dlen++;
			else if (c < lchar)
				lchar = c;
		}
		if (lchar == -1U)
			break;
		skip += (lchar - cchar) * (dlen + 1);
		for (i = 0; i < uni_l; i++) {
			unsigned c = uni[i];
			if (c < lchar)
				skip++;
			if (c == lchar) {
				unsigned n;
				n = skip;
				while (1) {
					unsigned t = puny_threshold(&st);
					if (n < t) {
						add_chr_to_str(&res, &res_l, puny_chrenc(n));
						break;
					} else {
						unsigned d = (n - t) % (puny_base - t);
						n = (n - t) / (puny_base - t);
						add_chr_to_str(&res, &res_l, puny_chrenc(d + t));
					}
				}
				puny_adapt(&st, skip);
				skip = 0;
			}
		}
		skip++;
		cchar = lchar + 1;
	}

ret_free_uni:
	free(uni);

	if (res_l > puny_max_length)
		goto err;

	return res;

err_free_uni:
	free(uni);
err:
	return NULL;
}

static unsigned char *puny_decode(unsigned char *s, int len)
{
	unsigned char *p, *last_dash;
	unsigned *uni;
	unsigned uni_l;
	unsigned char *res;
	int res_l;
	unsigned i;
	unsigned cchar, pos;
	struct puny_state st;

	if (!(len >= 4 && !casecmp(s, cast_uchar xn, xn_l)))
		return NULL;
	s += xn_l;
	len -= xn_l;

	last_dash = NULL;
	for (p = s; p < s + len; p++) {
		unsigned char c = *p;
		if (!ascii_allowed(c))
			goto err;
		if (c == '-')
			last_dash = p;
	}

	if (len > puny_max_length)
		goto err;

	uni = xmalloc(len * sizeof(unsigned));
	uni_l = 0;

	if (last_dash) {
		for (p = s; p < last_dash; p++)
			uni[uni_l++] = *p;
		p = last_dash + 1;
	} else
		p = s;

	puny_init(&st, uni_l);

	cchar = 128;
	pos = 0;

	while (p < s + len) {
		unsigned w = 1;
		unsigned val = 0;
		while (1) {
			unsigned n, t, nv, nw;
			if (p >= s + len)
				goto err_free_uni;
			n = puny_chrdec(*p++);
			nw = n * w;
			if (nw / w != n)
				goto err_free_uni;
			nv = val + nw;
			if (nv < val)
				goto err_free_uni;
			val = nv;
			t = puny_threshold(&st);
			if (n < t)
				break;
			nw = w * (puny_base - t);
			if (nw / w != puny_base - t)
				goto err_free_uni;
			w = nw;
		}
		puny_adapt(&st, val);

		if (val > uni_l - pos) {
			unsigned cp;
			val -= uni_l - pos + 1;
			pos = 0;
			cp = val / (uni_l + 1) + 1;
			val %= uni_l + 1;
			if (cchar + cp < cchar)
				goto err_free_uni;
			cchar += cp;
			if (cchar > 0x10FFFF)
				goto err_free_uni;
		}
		pos += val;
		memmove(uni + pos + 1, uni + pos, (uni_l - pos) * sizeof(unsigned));
		uni[pos++] = cchar;
		uni_l++;
	}

	res = init_str();
	res_l = 0;

	for (i = 0; i < uni_l; i++) {
		unsigned char *us = encode_utf_8(uni[i]);
		add_to_str(&res, &res_l, us);
	}

	free(uni);

	return res;

err_free_uni:
	free(uni);
err:
	return NULL;
}

unsigned char *idn_encode_host(unsigned char *host, int len, unsigned char *separator, int decode)
{
	unsigned char *p, *s;
	int pl, l, i;
	p = init_str();
	pl = 0;

next_host_elem:
	l = len;
	for (s = separator; *s; s++) {
		unsigned char *d = memchr(host, *s, l);
		if (d)
			l = (int)(d - host);
	}

	if (!decode) {
		for (i = 0; i < l; i++)
			if (host[i] >= 0x80) {
				unsigned char *enc = puny_encode(host, l);
				if (!enc)
					goto err;
				add_to_str(&p, &pl, enc);
				free(enc);
				goto advance_host;
			}
	} else {
		unsigned char *dec = puny_decode(host, l);
		if (dec) {
			add_to_str(&p, &pl, dec);
			free(dec);
			goto advance_host;
		}
	}

	add_bytes_to_str(&p, &pl, host, l);

advance_host:
	if (l != len) {
		add_chr_to_str(&p, &pl, host[l]);
		host += l + 1;
		len -= l + 1;
		goto next_host_elem;
	}
	return p;

err:
	free(p);
	return NULL;
}

unsigned char *idn_encode_url(unsigned char *url, int decode)
{
	unsigned char *host, *p, *h;
	int holen, pl;
	if (parse_url(url, NULL, NULL, NULL, NULL, NULL, &host, &holen, NULL,
		NULL, NULL, NULL, NULL) || !host) {
		host = url;
		holen = 0;
	}

	h = idn_encode_host(host, holen, cast_uchar ".", decode);
	if (!h)
		return NULL;

	p = init_str();
	pl = 0;
	add_bytes_to_str(&p, &pl, url, host - url);
	add_to_str(&p, &pl, h);
	add_to_str(&p, &pl, host + holen);
	free(h);
	return p;
}

static unsigned char *display_url_or_host(struct terminal *term, unsigned char *url, int warn_idn, int just_host, unsigned char *separator)
{
	unsigned char *uu, *url_dec, *url_conv, *url_conv2, *url_enc, *ret;
	int is_idn;

	if (!url)
		return stracpy(cast_uchar "");

	url = stracpy(url);
	if (!just_host)
		if ((uu = cast_uchar strchr((char *)url, POST_CHAR)))
			*uu = 0;

	if (!url_non_ascii(url) && !strstr((char *)url, xn))
		return url;

	if (!just_host)
		url_dec = idn_encode_url(url, 1);
	else
		url_dec = idn_encode_host(url, (int)strlen((char *)url), separator, 1);
	is_idn = strcmp((char *)url_dec, (char *)url);
	url_conv = convert(0, term_charset(term), url_dec, NULL);
	free(url_dec);
	url_conv2 = convert(term_charset(term), 0, url_conv, NULL);
	if (!just_host)
		url_enc = idn_encode_url(url_conv2, 0);
	else
		url_enc = idn_encode_host(url_conv2, (int)strlen((char *)url_conv2), separator, 0);
	if (!url_enc) {
		url_enc = stracpy(url_conv2);
		is_idn = 1;
	}
	free(url_conv2);
	if (!strcmp((char *)url_enc, (char *)url)) {
		if (is_idn && warn_idn) {
			ret = stracpy(cast_uchar "(IDN) ");
			add_to_strn(&ret, url_conv);
		} else {
			ret = url_conv;
			url_conv = NULL;
		}
	} else
		ret = convert(0, term_charset(term), url, NULL);
	free(url);
	free(url_conv);
	free(url_enc);
	return ret;
}

unsigned char *display_url(struct terminal *term, unsigned char *url, int warn_idn)
{
	return display_url_or_host(term, url, warn_idn, 0, cast_uchar ".");
}

unsigned char *display_host(struct terminal *term, unsigned char *host)
{
	return display_url_or_host(term, host, 1, 1, cast_uchar ".");
}

unsigned char *display_host_list(struct terminal *term, unsigned char *host)
{
	return display_url_or_host(term, host, 0, 1, cast_uchar ".,");
}
