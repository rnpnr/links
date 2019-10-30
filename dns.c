/* dns.c
 * (c) 2002 Mikulas Patocka
 * This file is a part of the Links program, released under GPL
 */

#include "links.h"

int support_ipv6;

struct dnsentry {
	list_entry_1st
	uttime absolute_time;
	struct lookup_result addr;
	list_entry_last
	char name[1];
};

struct dnsquery {
	void (*fn)(void *, int);
	void *data;
	int h;
	struct dnsquery **s;
	struct lookup_result *addr;
	int addr_preference;
	char name[];
};

static int dns_cache_addr_preference = -1;
static struct list_head dns_cache = {&dns_cache, &dns_cache};

static void end_dns_lookup(struct dnsquery *q, int a);
static int shrink_dns_cache(int u);

static int get_addr_byte(const char *p, char *res, const char stp)
{
	char u = 0;
	if (!(*p >= '0' && *p <= '9'))
		return -1;
	for (; *p >= '0' && *p <= '9'; p++) {
		if (u * 10 + *p - '0' > 255)
			return -1;
		u = u * 10 + *p - '0';
	}
	if (*p != stp) return -1;
	p++;
	*res = u;
	return 0;
}

int numeric_ip_address(const char *name, char address[4])
{
	char dummy[4];
	if (!address)
		address = dummy;
	if (get_addr_byte(name, address + 0, '.')
	|| get_addr_byte(name, address + 1, '.')
	|| get_addr_byte(name, address + 2, '.')
	|| get_addr_byte(name, address + 3, 0))
		return -1;
	return 0;
}

static int extract_ipv6_address(struct addrinfo *p, char address[16], unsigned *scope_id)
{
	if (p->ai_family == AF_INET6
	&& (socklen_t)p->ai_addrlen >= (socklen_t)sizeof(struct sockaddr_in6)
	&& p->ai_addr->sa_family == AF_INET6) {
		memcpy(address, &((struct sockaddr_in6 *)p->ai_addr)->sin6_addr, 16);
		*scope_id = ((struct sockaddr_in6 *)p->ai_addr)->sin6_scope_id;
		return 0;
	}
	return -1;
}

int numeric_ipv6_address(const char *name, char address[16], unsigned *scope_id)
{
	char dummy_a[16];
	unsigned dummy_s;
	int r;
	struct in6_addr i6a;
	struct addrinfo hints, *res;
	if (!address)
		address = dummy_a;
	if (!scope_id)
		scope_id = &dummy_s;

	if (inet_pton(AF_INET6, name, &i6a) == 1) {
		memcpy(address, &i6a, 16);
		*scope_id = 0;
		return 0;
	}
	if (!strchr(cast_const_char name, '%'))
		return -1;

	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_INET6;
	hints.ai_flags = AI_NUMERICHOST;
	if (getaddrinfo(name, NULL, &hints, &res))
		return -1;
	r = extract_ipv6_address(res, address, scope_id);
	freeaddrinfo(res);
	return r;
}

#if MAX_ADDRESSES > 1
static int memcmp_host_address(struct host_address *a, struct host_address *b)
{
	if (a->af != b->af || a->scope_id != b->scope_id)
		return 1;
	return memcmp(a->addr, b->addr, sizeof a->addr);
}
#endif

static void add_address(struct lookup_result *host, int af,
		unsigned char *address, unsigned scope_id, int preference)
{
	struct host_address neww;
	struct host_address *e, *t;
#if MAX_ADDRESSES > 1
	struct host_address *n;
#endif
	if ((af != AF_INET && preference == ADDR_PREFERENCE_IPV4_ONLY)
	|| (af != AF_INET6 && preference == ADDR_PREFERENCE_IPV6_ONLY)
	|| (host->n >= MAX_ADDRESSES))
		return;
	memset(&neww, 0, sizeof(struct host_address));
	neww.af = af;
	memcpy(neww.addr, address, af == AF_INET ? 4 : 16);
	neww.scope_id = scope_id;
	e = &host->a[host->n];
	t = e;
#if MAX_ADDRESSES > 1
	for (n = host->a; n != e; n++) {
		if (!memcmp_host_address(n, &neww))
			return;
		if ((preference == ADDR_PREFERENCE_IPV4 && af == AF_INET
		&& n->af != AF_INET)
		|| (preference == ADDR_PREFERENCE_IPV6 && af == AF_INET6
		&& n->af != AF_INET6)) {
			t = n;
			break;
		}
	}
	memmove(t + 1, t, (e - t) * sizeof(struct host_address));
#endif
	memcpy(t, &neww, sizeof(struct host_address));
	host->n++;
}

static int use_getaddrinfo(unsigned char *name, struct addrinfo *hints, int preference, struct lookup_result *host)
{
	int gai_err;
	struct addrinfo *res, *p;
	gai_err = getaddrinfo(cast_const_char name, NULL, hints, &res);
	if (gai_err)
		return gai_err;
	for (p = res; p; p = p->ai_next) {
		if (p->ai_family == AF_INET
		&& (socklen_t)p->ai_addrlen >= (socklen_t)sizeof(struct sockaddr_in)
		&& p->ai_addr->sa_family == AF_INET) {
			add_address(host, AF_INET,
				(unsigned char *)&((struct sockaddr_in *)p->ai_addr)->sin_addr.s_addr,
				0, preference);
			continue;
		}
		{
			unsigned char address[16];
			unsigned scope_id;
			if (!extract_ipv6_address(p, (char *)address, &scope_id)) {
				add_address(host, AF_INET6, address, scope_id,
					preference);
				continue;
			}
		}
	}
	freeaddrinfo(res);
	return 0;
}

void rotate_addresses(struct lookup_result *host)
{
#if MAX_ADDRESSES > 1
	int first_type, first_different, i;

	if (host->n <= 2)
		return;

	first_type = host->a[0].af;

	for (i = 1; i < host->n; i++) {
		if (host->a[i].af != first_type) {
			first_different = i;
			goto do_swap;
		}
	}
	return;

do_swap:
	if (first_different > 1) {
		struct host_address ha;
		memcpy(&ha, &host->a[first_different], sizeof(struct host_address));
		memmove(&host->a[2], &host->a[1], (first_different - 1) * sizeof(struct host_address));
		memcpy(&host->a[1], &ha, sizeof(struct host_address));
	}
#endif
}

void do_real_lookup(unsigned char *name, int preference, struct lookup_result *host)
{
	unsigned char address[16];
	size_t nl;

	memset(host, 0, sizeof(struct lookup_result));

	if (strlen(cast_const_char name) >= 6
	&& !casestrcmp(name + strlen(cast_const_char name) - 6,
			cast_uchar ".onion"))
		goto ret;

	if (!support_ipv6)
		preference = ADDR_PREFERENCE_IPV4_ONLY;

	if (!numeric_ip_address((char *)name, (char *)address)) {
		add_address(host, AF_INET, address, 0, preference);
		goto ret;
	}
	nl = strlen(cast_const_char name);
	if (name[0] == '[' && name[nl - 1] == ']') {
		unsigned char *n2 = cast_uchar strdup(cast_const_char(name + 1));
		if (n2) {
			unsigned scope_id;
			n2[nl - 2] = 0;
			if (!numeric_ipv6_address((char *)n2, (char *)address, &scope_id)) {
				free(n2);
				add_address(host, AF_INET6, address, scope_id, preference);
				goto ret;
			}
			free(n2);
		}
	} else {
		unsigned scope_id;
		if (!numeric_ipv6_address((char *)name, (char *)address, &scope_id)) {
			add_address(host, AF_INET6, address, scope_id, preference);
			goto ret;
		}
	}

	use_getaddrinfo(name, NULL, preference, host);
#if defined(EXTRA_IPV6_LOOKUP)
	if ((preference == ADDR_PREFERENCE_IPV4 && !host->n) ||
	    preference == ADDR_PREFERENCE_IPV6 ||
	    preference == ADDR_PREFERENCE_IPV6_ONLY) {
		struct addrinfo hints;
		int i;
		for (i = 0; i < host->n; i++)
			if (host->a[i].af == AF_INET6)
				goto already_have_inet6;
		memset(&hints, 0, sizeof hints);
		hints.ai_family = AF_INET6;
		hints.ai_flags = 0;
		use_getaddrinfo(name, &hints, preference, host);
	}
	already_have_inet6:;
#endif
ret:
	return;
}

static int do_lookup(struct dnsquery *q, int force_async)
{
	do_real_lookup((unsigned char *)q->name, q->addr_preference, q->addr);
	end_dns_lookup(q, !q->addr->n);
	return 0;
}

static int do_queued_lookup(struct dnsquery *q)
{
		return do_lookup(q, 0);
}

static void check_dns_cache_addr_preference(void)
{
	if (dns_cache_addr_preference != ipv6_options.addr_preference) {
		shrink_dns_cache(SH_FREE_ALL);
		dns_cache_addr_preference = ipv6_options.addr_preference;
	}
}

static int find_in_dns_cache(unsigned char *name, struct dnsentry **dnsentry)
{
	struct dnsentry *e = NULL;
	struct list_head *le;
	check_dns_cache_addr_preference();
	foreach(struct dnsentry, e, le, dns_cache)
		if (!casestrcmp((unsigned char *)e->name, name)) {
			del_from_list(e);
			add_to_list(dns_cache, e);
			*dnsentry = e;
			return 0;
		}
	return -1;
}

static void free_dns_entry(struct dnsentry *dnsentry)
{
	del_from_list(dnsentry);
	free(dnsentry);
}

static void end_dns_lookup(struct dnsquery *q, int a)
{
	struct dnsentry *dnsentry;
	void (*fn)(void *, int);
	void *data;
	if (!q->fn || !q->addr) {
		free(q);
		return;
	}
	if (!find_in_dns_cache((unsigned char *)q->name, &dnsentry)) {
		if (a) {
			memcpy(q->addr, &dnsentry->addr, sizeof(struct lookup_result));
			a = 0;
			goto e;
		}
		free_dns_entry(dnsentry);
	}
	if (a)
		goto e;
	if (q->addr_preference != ipv6_options.addr_preference)
		goto e;
	check_dns_cache_addr_preference();
	dnsentry = xmalloc(sizeof(struct dnsentry) + strlen(q->name));
	strcpy(dnsentry->name, q->name);
	memcpy(&dnsentry->addr, q->addr, sizeof(struct lookup_result));
	dnsentry->absolute_time = get_absolute_time();
	add_to_list(dns_cache, dnsentry);
e:
	if (q->s)
		*q->s = NULL;
	fn = q->fn;
	data = q->data;
	free(q);
	fn(data, a);
}

int find_host_no_cache(unsigned char *name, struct lookup_result *addr, void **qp, void (*fn)(void *, int), void *data)
{
	struct dnsquery *q;
	q = xmalloc(sizeof(struct dnsquery) + strlen(cast_const_char name));
	q->fn = fn;
	q->data = data;
	q->s = (struct dnsquery **)qp;
	q->addr = addr;
	q->addr_preference = ipv6_options.addr_preference;
	strcpy(q->name, (char *)name);
	if (qp)
		*qp = q;
	return do_queued_lookup(q);
}

int find_host(unsigned char *name, struct lookup_result *addr, void **qp, void (*fn)(void *, int), void *data)
{
	struct dnsentry *dnsentry;
	if (qp)
		*qp = NULL;
	if (!find_in_dns_cache(name, &dnsentry)) {
		if (get_absolute_time() - dnsentry->absolute_time > DNS_TIMEOUT) goto timeout;
		memcpy(addr, &dnsentry->addr, sizeof(struct lookup_result));
		fn(data, 0);
		return 0;
	}
	timeout:
	return find_host_no_cache(name, addr, qp, fn, data);
}

void kill_dns_request(void **qp)
{
	struct dnsquery *q = *qp;
	q->fn = NULL;
	q->addr = NULL;
	*qp = NULL;
}

#if MAX_ADDRESSES > 1
void dns_set_priority(unsigned char *name, struct host_address *address, int prefer)
{
	int i;
	struct dnsentry *dnsentry;
	if (find_in_dns_cache(name, &dnsentry))
		return;
	for (i = 0; i < dnsentry->addr.n; i++)
		if (!memcmp_host_address(&dnsentry->addr.a[i], address))
			goto found_it;
	return;
found_it:
	if (prefer) {
		memmove(&dnsentry->addr.a[1], &dnsentry->addr.a[0], i * sizeof(struct host_address));
		memcpy(&dnsentry->addr.a[0], address, sizeof(struct host_address));
	} else {
		memmove(&dnsentry->addr.a[i], &dnsentry->addr.a[i + 1],
			(dnsentry->addr.n - i - 1) * sizeof(struct host_address));
		memcpy(&dnsentry->addr.a[dnsentry->addr.n - 1], address,
			sizeof(struct host_address));
	}
}
#endif

void dns_clear_host(unsigned char *name)
{
	struct dnsentry *dnsentry;
	if (find_in_dns_cache(name, &dnsentry))
		return;
	free_dns_entry(dnsentry);
}

unsigned long dns_info(int type)
{
	switch (type) {
	case CI_FILES:
		return list_size(&dns_cache);
	default:
		internal("dns_info: bad request");
	}
	return 0;
}

static int shrink_dns_cache(int u)
{
	uttime now = get_absolute_time();
	struct dnsentry *d = NULL;
	struct list_head *ld;
	int f = 0;
	if (u == SH_FREE_SOMETHING && !list_empty(dns_cache)) {
		d = list_struct(dns_cache.prev, struct dnsentry);
		goto delete_last;
	}
	foreach(struct dnsentry, d, ld, dns_cache)
		if (u == SH_FREE_ALL || now - d->absolute_time > DNS_TIMEOUT) {
delete_last:
			ld = d->list_entry.prev;
			free_dns_entry(d);
			f = ST_SOMETHING_FREED;
		}
	return f | (list_empty(dns_cache) ? ST_CACHE_EMPTY : 0);
}

unsigned char *print_address(struct host_address *a)
{
#define SCOPE_ID_LEN	11
	static char buffer[INET6_ADDRSTRLEN + SCOPE_ID_LEN];
	union {
		struct in_addr in;
		struct in6_addr in6;
		char pad[16];
	} u;
	memcpy(&u, a->addr, 16);
	if (!inet_ntop(a->af, &u, buffer, sizeof(buffer) - SCOPE_ID_LEN))
		return NULL;
	if (a->scope_id) {
		char *end = strchr(buffer, 0);
		snprintf(end, buffer + sizeof(buffer) - end, "%%%u", a->scope_id);
	}
	return (unsigned char *)buffer;
}

int ipv6_full_access(void)
{
	/*
	 * Test if we can access global IPv6 address space.
	 * This doesn't send anything anywhere, it just creates an UDP socket,
	 * connects it and closes it.
	 */
	struct sockaddr_in6 sin6;
	int h, c, rs;
	if (!support_ipv6)
		return 0;
	h = c_socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
	if (h == -1)
		return 0;
	memset(&sin6, 0, sizeof sin6);
	sin6.sin6_family = AF_INET6;
	sin6.sin6_port = htons(1024);
	memcpy(&sin6.sin6_addr.s6_addr, "\052\001\004\060\000\015\000\000\002\314\236\377\376\044\176\032", 16);
	EINTRLOOP(c, connect(h, (struct sockaddr *)(void *)&sin6, sizeof sin6));
	EINTRLOOP(rs, close(h));
	if (!c)
		return 1;
	return 0;
}

void init_dns(void)
{
	register_cache_upcall(shrink_dns_cache, 0, cast_uchar "dns");
	int h, rs;
	h = c_socket(AF_INET6, SOCK_STREAM, 0);
	if (h == -1)
		support_ipv6 = 0;
	else {
		EINTRLOOP(rs, close(h));
		support_ipv6 = 1;
	}
}
