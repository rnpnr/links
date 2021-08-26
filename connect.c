/* connect.c
 * (c) 2002 Mikulas Patocka
 * This file is a part of the Links program, released under GPL.
 */

#include <limits.h>

#include "links.h"

static void connected(void *);
static void update_dns_priority(struct connection *);
static void connected_callback(struct connection *);
static void dns_found(void *, int);
static void try_connect(struct connection *);
static void handle_socks_reply(void *);

int socket_and_bind(int pf, unsigned char *address)
{
	int rs, s = c_socket(pf, SOCK_STREAM, IPPROTO_TCP);

	if (!address || !(*address))
		return s;

	switch (pf) {
	case PF_INET: {
		struct sockaddr_in sa;
		unsigned char addr[4];
		if (numeric_ip_address((char *)address, (char *)addr) == -1) {
			EINTRLOOP(rs, close(s));
			errno = EINVAL;
			return -1;
		}
		memset(&sa, 0, sizeof sa);
		sa.sin_family = AF_INET;
		memcpy(&sa.sin_addr.s_addr, addr, 4);
		sa.sin_port = htons(0);
		EINTRLOOP(rs, bind(s, (struct sockaddr *)(void *)&sa, sizeof sa));
		if (rs) {
			int sv_errno = errno;
			EINTRLOOP(rs, close(s));
			errno = sv_errno;
			return -1;
		}
		break;
	}
	case PF_INET6: {
		struct sockaddr_in6 sa;
		unsigned char addr[16];
		unsigned scope;
		if (numeric_ipv6_address((char *)address, (char *)addr, &scope) == -1) {
			EINTRLOOP(rs, close(s));
			errno = EINVAL;
			return -1;
		}
		memset(&sa, 0, sizeof sa);
		sa.sin6_family = AF_INET6;
		memcpy(&sa.sin6_addr, addr, 16);
		sa.sin6_port = htons(0);
		sa.sin6_scope_id = scope;
		EINTRLOOP(rs, bind(s, (struct sockaddr *)(void *)&sa, sizeof sa));
		if (rs) {
			int sv_errno = errno;
			EINTRLOOP(rs, close(s));
			errno = sv_errno;
			return -1;
		}
		break;
	}
	default:
		EINTRLOOP(rs, close(s));
		errno = EINVAL;
		return -1;
	}

	return s;
}

void close_socket(int *s)
{
	int rs;
	if (*s == -1) return;
	set_handlers(*s, NULL, NULL, NULL);
	EINTRLOOP(rs, close(*s));
	*s = -1;
}

struct conn_info {
	void (*func)(struct connection *);
	struct lookup_state l;
	int first_error;
	int *sock;
	int socks_byte_count;
	int socks_handled;
	unsigned char socks_reply[8];
	char host[1];
};

void make_connection(struct connection *c, int port, int *sock, void (*func)(struct connection *))
{
	int socks_port = -1;
	int as;
	char *p, *host;
	size_t sl;
	struct conn_info *b;
	if (*c->socks_proxy) {
		p = strchr(c->socks_proxy, '@');
		if (p) p++;
		else p = c->socks_proxy;
		host = strdup(p);
		socks_port = 1080;
		if ((p = strchr(host, ':'))) {
			long lp;
			*p++ = 0;
			if (!*p) goto badu;
			lp = strtol(p, &p, 10);
			if (*p || lp <= 0 || lp >= 65536) {
				badu:
				free(host);
				setcstate(c, S_BAD_URL);
				abort_connection(c);
				return;
			}
			socks_port = (int)lp;
		}
	} else if (!(host = (char *)get_host_name(c->url))) {
		setcstate(c, S_INTERNAL);
		abort_connection(c);
		return;
	}
	if (c->newconn)
		internal("already making a connection");
	sl = strlen(host);
	if (sl > INT_MAX - sizeof(struct conn_info)) overalloc();
	b = mem_calloc(sizeof(struct conn_info) + sl);
	b->func = func;
	b->sock = sock;
	b->l.socks_port = socks_port;
	b->l.target_port = port;
	strcpy(b->host, host);
	c->newconn = b;
	if (c->last_lookup_state.addr_index < c->last_lookup_state.addr.n) {
		b->l.addr = c->last_lookup_state.addr;
		b->l.addr_index = c->last_lookup_state.addr_index;
		b->l.dont_try_more_servers = 1;
		dns_found(c, 0);
		as = 0;
	} else if (c->no_cache >= NC_RELOAD)
		as = find_host_no_cache(host, &b->l.addr, &c->dnsquery, dns_found, c);
	else
		as = find_host(host, &b->l.addr, &c->dnsquery, dns_found, c);
	free(host);
	if (as) setcstate(c, S_DNS);
}

static void ssl_setup_downgrade(struct connection *c)
{
#if !defined(HAVE_NSS)
	int dd = c->no_tls;
#ifdef SSL_OP_NO_TLSv1_3
	if (dd) {
		SSL_set_options(c->ssl->ssl, SSL_OP_NO_TLSv1_3);
		dd--;
	}
#endif
#ifdef SSL_OP_NO_TLSv1_2
	if (dd) {
		SSL_set_options(c->ssl->ssl, SSL_OP_NO_TLSv1_2);
		dd--;
	}
#endif
#ifdef SSL_OP_NO_TLSv1_1
	if (dd) {
		SSL_set_options(c->ssl->ssl, SSL_OP_NO_TLSv1_1);
		dd--;
	}
#endif
#ifdef SSL_OP_NO_TLSv1
	if (dd) {
		SSL_set_options(c->ssl->ssl, SSL_OP_NO_TLSv1);
		dd--;
	}
#endif
#ifdef SSL_MODE_SEND_FALLBACK_SCSV
	if (dd != c->no_tls) {
		SSL_set_mode(c->ssl->ssl, SSL_MODE_SEND_FALLBACK_SCSV);
	}
#endif
	if (SCRUB_HEADERS) {
#ifdef SSL_OP_NO_SSLv2
		SSL_set_options(c->ssl->ssl, SSL_OP_NO_SSLv2);
#endif
#ifdef SSL_OP_NO_SSLv3
		SSL_set_options(c->ssl->ssl, SSL_OP_NO_SSLv3);
#endif
	}
#endif
}

static void ssl_downgrade_dance(struct connection *c)
{
#if !defined(HAVE_NSS)
	int downgrades = 0;
	c->no_ssl_session = 1;
	if (c->ssl && c->ssl->session_set) {
		retry_connect(c, S_SSL_ERROR, 1);
		return;
	}
#ifdef SSL_OP_NO_TLSv1_3
	downgrades++;
#endif
#ifdef SSL_OP_NO_TLSv1_2
	downgrades++;
#endif
#ifdef SSL_OP_NO_TLSv1_1
	downgrades++;
#endif
#ifdef SSL_OP_NO_TLSv1
	downgrades++;
#endif
	if (++c->no_tls <= downgrades) {
		retry_connect(c, S_SSL_ERROR, 1);
	} else {
		c->no_tls = 0;
		retry_connect(c, S_SSL_ERROR, 0);
	}
#else
	retry_connect(c, S_SSL_ERROR, 0);
#endif
}

static void ssl_want_io(void *c_)
{
	struct connection *c = (struct connection *)c_;
	struct conn_info *b = c->newconn;

	set_connection_timeout(c);

	switch (SSL_get_error(c->ssl->ssl, SSL_connect(c->ssl->ssl))) {
		case SSL_ERROR_NONE:
			connected_callback(c);
			break;
		case SSL_ERROR_WANT_READ:
			set_handlers(*b->sock, ssl_want_io, NULL, c);
			break;
		case SSL_ERROR_WANT_WRITE:
			set_handlers(*b->sock, NULL, ssl_want_io, c);
			break;
		default:
			ssl_downgrade_dance(c);
	}
}

static void handle_socks(void *c_)
{
	struct connection *c = (struct connection *)c_;
	struct conn_info *b = c->newconn;
	unsigned char *command = init_str();
	int len = 0;
	unsigned char *host;
	int wr;
	setcstate(c, S_SOCKS_NEG);
	set_connection_timeout(c);
	add_bytes_to_str(&command, &len, cast_uchar "\004\001", 2);
	add_chr_to_str(&command, &len, b->l.target_port >> 8);
	add_chr_to_str(&command, &len, b->l.target_port);
	add_bytes_to_str(&command, &len, cast_uchar "\000\000\000\001", 4);
	if (strchr(c->socks_proxy, '@'))
		add_bytes_to_str(&command, &len, (unsigned char *)c->socks_proxy, strcspn(c->socks_proxy, "@"));
	add_chr_to_str(&command, &len, 0);
	if (!(host = get_host_name(c->url))) {
		free(command);
		setcstate(c, S_INTERNAL);
		abort_connection(c);
		return;
	}
	add_to_str(&command, &len, host);
	add_to_str(&command, &len, c->dns_append);
	add_chr_to_str(&command, &len, 0);
	free(host);
	if (b->socks_byte_count >= len) {
		free(command);
		setcstate(c, S_MODIFIED);
		retry_connection(c);
		return;
	}
	EINTRLOOP(wr, (int)write(*b->sock, command + b->socks_byte_count, len - b->socks_byte_count));
	free(command);
	if (wr <= 0) {
		setcstate(c, wr ? get_error_from_errno(errno) : S_CANT_WRITE);
		retry_connection(c);
		return;
	}
	b->socks_byte_count += wr;
	if (b->socks_byte_count < len) {
		set_handlers(*b->sock, NULL, handle_socks, c);
		return;
	} else {
		b->socks_byte_count = 0;
		set_handlers(*b->sock, handle_socks_reply, NULL, c);
		return;
	}
}

static void handle_socks_reply(void *c_)
{
	struct connection *c = (struct connection *)c_;
	struct conn_info *b = c->newconn;
	int rd;
	set_connection_timeout(c);
	EINTRLOOP(rd, (int)read(*b->sock, b->socks_reply + b->socks_byte_count, sizeof b->socks_reply - b->socks_byte_count));
	if (rd <= 0) {
		setcstate(c, rd ? get_error_from_errno(errno) : S_CANT_READ);
		retry_connection(c);
		return;
	}
	b->socks_byte_count += rd;
	if (b->socks_byte_count < (int)sizeof b->socks_reply) return;
	if (b->socks_reply[0]) {
		setcstate(c, S_BAD_SOCKS_VERSION);
		abort_connection(c);
		return;
	}
	switch (b->socks_reply[1]) {
		case 91:
			setcstate(c, S_SOCKS_REJECTED);
			retry_connection(c);
			return;
		case 92:
			setcstate(c, S_SOCKS_NO_IDENTD);
			abort_connection(c);
			return;
		case 93:
			setcstate(c, S_SOCKS_BAD_USERID);
			abort_connection(c);
			return;
		default:
			setcstate(c, S_SOCKS_UNKNOWN_ERROR);
			retry_connection(c);
			return;
		case 90:
			break;
	}
	connected(c);
}

static void dns_found(void *c_, int state)
{
	struct connection *c = (struct connection *)c_;
	if (state) {
		setcstate(c, *c->socks_proxy || is_proxy_url(c->url) ? S_NO_PROXY_DNS : S_NO_DNS);
		abort_connection(c);
		return;
	}
	try_connect(c);
}

void retry_connect(struct connection *c, int err, int ssl_downgrade)
{
	struct conn_info *b = c->newconn;
	if (!b->l.addr_index) b->first_error = err;
	freeSSL(c->ssl);
	c->ssl = NULL;
	if (ssl_downgrade) {
		close_socket(b->sock);
		try_connect(c);
		return;
	}
	b->l.addr_index++;
#if MAX_ADDRESSES > 1
	if (b->l.addr_index < b->l.addr.n && !b->l.dont_try_more_servers) {
		if (b->l.addr_index == 1)
			rotate_addresses(&b->l.addr);
		close_socket(b->sock);
		try_connect(c);
	} else
#endif
	{
		dns_clear_host(b->host);
		setcstate(c, b->first_error);
		retry_connection(c);
	}
}

static void try_connect(struct connection *c)
{
	int s;
	int rs;
	unsigned short p;
	struct conn_info *b = c->newconn;
	struct host_address *addr = &b->l.addr.a[b->l.addr_index];
	if (addr->af == AF_INET)
		s = socket_and_bind(PF_INET, bind_ip_address);
	else if (addr->af == AF_INET6)
		s = socket_and_bind(PF_INET6, bind_ipv6_address);
	else {
		setcstate(c, S_INTERNAL);
		abort_connection(c);
		return;
	}
	if (s == -1) {
		retry_connect(c, get_error_from_errno(errno), 0);
		return;
	}
	set_nonblock(s);
	*b->sock = s;
	b->socks_handled = 0;
	b->socks_byte_count = 0;
	p = b->l.socks_port != -1 ? b->l.socks_port : b->l.target_port;
	if (addr->af == AF_INET) {
		struct sockaddr_in sa;
		memset(&sa, 0, sizeof sa);
		sa.sin_family = AF_INET;
		memcpy(&sa.sin_addr.s_addr, addr->addr, 4);
		sa.sin_port = htons(p);
		EINTRLOOP(rs, connect(s, (struct sockaddr *)(void *)&sa, sizeof sa));
	} else if (addr->af == AF_INET6) {
		struct sockaddr_in6 sa;
		memset(&sa, 0, sizeof sa);
		sa.sin6_family = AF_INET6;
		memcpy(&sa.sin6_addr, addr->addr, 16);
		sa.sin6_scope_id = addr->scope_id;
		sa.sin6_port = htons(p);
		EINTRLOOP(rs, connect(s, (struct sockaddr *)(void *)&sa, sizeof sa));
	} else {
		rs = -1;
		errno = EINVAL;
	}
	if (rs) {
		if (errno != EALREADY && errno != EINPROGRESS) {
			retry_connect(c, get_error_from_errno(errno), 0);
			return;
		}
		set_handlers(s, NULL, connected, c);
		setcstate(c, !b->l.addr_index ? S_CONN : S_CONN_ANOTHER);
#if MAX_ADDRESSES > 1
		if (b->l.addr.n > 1 && (is_connection_restartable(c) || max_tries == 1)) {
			set_connection_timeout(c);
		}
#endif
	} else {
		connected(c);
	}
}

void continue_connection(struct connection *c, int *sock, void (*func)(struct connection *))
{
	struct conn_info *b;
	if (c->newconn)
		internal("already making a connection");
	b = mem_calloc(sizeof(struct conn_info));
	b->func = func;
	b->sock = sock;
	b->l = c->last_lookup_state;
	b->socks_handled = 1;
	c->newconn = b;
	connected(c);
}

static void connected(void *c_)
{
	struct connection *c = (struct connection *)c_;
	struct conn_info *b = c->newconn;
#ifdef SO_ERROR
	int err = 0;
	socklen_t len = sizeof(int);
	int rs;
#endif
	clear_connection_timeout(c);
#ifdef SO_ERROR
	errno = 0;
	EINTRLOOP(rs, getsockopt(*b->sock, SOL_SOCKET, SO_ERROR, (void *)&err, &len));
	if (!rs) {
		if (err >= 10000) err -= 10000;	/* Why does EMX return so large values? */
	} else {
		if (!(err = errno)) {
			retry_connect(c, S_STATE, 0);
			return;
		}
	}
	if (err > 0
#ifdef EISCONN
	    && err != EISCONN
#endif
	    ) {
		retry_connect(c, get_error_from_errno(err), 0);
		return;
	}
#endif
	set_connection_timeout(c);
	if (b->l.socks_port != -1 && !b->socks_handled) {
		b->socks_handled = 1;
		update_dns_priority(c);
		handle_socks(c);
		return;
	}
	if (c->ssl) {
		unsigned char *orig_url = remove_proxy_prefix(c->url);
		unsigned char *h = get_host_name(orig_url);
		if (*h && h[strlen(cast_const_char h) - 1] == '.')
			h[strlen(cast_const_char h) - 1] = 0;
		c->ssl = getSSL();
		if (!c->ssl) {
			free(h);
			goto ssl_error;
		}
		if (!proxies.only_proxies && !c->no_ssl_session) {
			unsigned char *h = get_host_name(orig_url);
			int p = get_port(orig_url);
			SSL_SESSION *ses = get_session_cache_entry(c->ssl->ctx, h, p);
			if (ses) {
				if (SSL_set_session(c->ssl->ssl, ses) == 1)
					c->ssl->session_set = 1;
			}
			free(h);
		}
#if !defined(OPENSSL_NO_STDIO)
		if (!proxies.only_proxies) {
			if (ssl_options.client_cert_key[0]) {
				SSL_use_PrivateKey_file(c->ssl->ssl, cast_const_char ssl_options.client_cert_key, SSL_FILETYPE_PEM);
			}
			if (ssl_options.client_cert_crt[0]) {
				SSL_use_certificate_file(c->ssl->ssl, cast_const_char ssl_options.client_cert_crt, SSL_FILETYPE_PEM);
			}
		}
#endif
		SSL_set_fd(c->ssl->ssl, *b->sock);
		ssl_setup_downgrade(c);
#ifdef SSL_CTRL_SET_TLSEXT_HOSTNAME
		if (h[0] == '[' || !numeric_ip_address((char *)h, NULL)
		    || !numeric_ipv6_address((char *)h, NULL, NULL)
		    ) goto skip_numeric_address;
		SSL_set_tlsext_host_name(c->ssl->ssl, h);
skip_numeric_address:
#endif
		free(h);
		switch (SSL_get_error(c->ssl->ssl, SSL_connect(c->ssl->ssl))) {
			case SSL_ERROR_WANT_READ:
				setcstate(c, S_SSL_NEG);
				set_handlers(*b->sock, ssl_want_io, NULL, c);
				return;
			case SSL_ERROR_WANT_WRITE:
				setcstate(c, S_SSL_NEG);
				set_handlers(*b->sock, NULL, ssl_want_io, c);
				return;
			case SSL_ERROR_NONE:
				break;
			default:
 ssl_error:
				ssl_downgrade_dance(c);
				return;
		}
	}
	connected_callback(c);
}

static void update_dns_priority(struct connection *c)
{
#if MAX_ADDRESSES > 1
	struct conn_info *b = c->newconn;
	if (!b->l.dont_try_more_servers && b->host[0]) {
		if (b->l.addr_index) {
			int i;
			for (i = 0; i < b->l.addr_index; i++)
				dns_set_priority(b->host, &b->l.addr.a[i], 0);
			dns_set_priority(b->host, &b->l.addr.a[i], 1);
		}
		b->l.dont_try_more_servers = 1;
	}
#endif
}

static void connected_callback(struct connection *c)
{
	int flags;
	struct conn_info *b = c->newconn;
	update_dns_priority(c);
	if (c->ssl) {
		if (ssl_options.certificates != SSL_ACCEPT_INVALID_CERTIFICATE) {
			unsigned char *h = get_host_name(remove_proxy_prefix(c->url));
			int err = verify_ssl_certificate(c->ssl, h);
			if (err) {
				if (ssl_options.certificates == SSL_WARN_ON_INVALID_CERTIFICATE) {
					flags = get_blacklist_flags(h);
					if (flags & BL_IGNORE_CERTIFICATE)
						goto ignore_cert;
				}
				free(h);
				setcstate(c, err);
				abort_connection(c);
				return;
			}
			ignore_cert:

			if (c->no_tls) {
				if (ssl_options.certificates == SSL_WARN_ON_INVALID_CERTIFICATE) {
					flags = get_blacklist_flags(h);
					if (flags & BL_IGNORE_DOWNGRADE)
						goto ignore_downgrade;
				}
				free(h);
				setcstate(c, S_DOWNGRADED_METHOD);
				abort_connection(c);
				return;
			}
			ignore_downgrade:

			err = verify_ssl_cipher(c->ssl);
			if (err) {
				if (ssl_options.certificates == SSL_WARN_ON_INVALID_CERTIFICATE) {
					flags = get_blacklist_flags(h);
					if (flags & BL_IGNORE_CIPHER)
						goto ignore_cipher;
				}
				free(h);
				setcstate(c, err);
				abort_connection(c);
				return;
			}
			ignore_cipher:

			free(h);
		}
	}
	retrieve_ssl_session(c);
	c->last_lookup_state = b->l;
	c->newconn = NULL;
	b->func(c);
	free(b);
}

struct write_buffer {
	int sock;
	int len;
	int pos;
	void (*done)(struct connection *);
	unsigned char data[1];
};

static void write_select(void *c_)
{
	struct connection *c = (struct connection *)c_;
	struct write_buffer *wb;
	int wr;
	if (!(wb = c->buffer)) {
		internal("write socket has no buffer");
		setcstate(c, S_INTERNAL);
		abort_connection(c);
		return;
	}
	if (wb->pos)
		set_connection_timeout(c);
	else
		set_connection_timeout_keepal(c);

	if (c->ssl) {
		set_handlers(wb->sock, NULL, write_select, c);
		if ((wr = SSL_write(c->ssl->ssl, (void *)(wb->data + wb->pos), wb->len - wb->pos)) <= 0) {
			int err;
			err = SSL_get_error(c->ssl->ssl, wr);
			if (err == SSL_ERROR_WANT_WRITE) {
				return;
			}
			if (err == SSL_ERROR_WANT_READ) {
				set_handlers(wb->sock, write_select, NULL, c);
				return;
			}
			setcstate(c, wr ? (err == SSL_ERROR_SYSCALL ? get_error_from_errno(errno) : S_SSL_ERROR) : S_CANT_WRITE);
			if (!wr || err == SSL_ERROR_SYSCALL) retry_connection(c);
			else abort_connection(c);
			return;
		}
		c->ssl->bytes_written += wr;
	} else {
		EINTRLOOP(wr, (int)write(wb->sock, wb->data + wb->pos, wb->len - wb->pos));
		if (wr <= 0) {
			setcstate(c, wr ? get_error_from_errno(errno) : S_CANT_WRITE);
			retry_connection(c);
			return;
		}
	}

	if ((wb->pos += wr) == wb->len) {
		void (*f)(struct connection *) = wb->done;
		c->buffer = NULL;
		set_handlers(wb->sock, NULL, NULL, NULL);
		free(wb);
		f(c);
	}
}

void write_to_socket(struct connection *c, int s, unsigned char *data, int len, void (*write_func)(struct connection *))
{
	struct write_buffer *wb;
	if ((unsigned)len > INT_MAX - sizeof(struct write_buffer)) overalloc();
	wb = xmalloc(sizeof(struct write_buffer) + len);
	wb->sock = s;
	wb->len = len;
	wb->pos = 0;
	wb->done = write_func;
	memcpy(wb->data, data, len);
	free(c->buffer);
	c->buffer = wb;
	set_handlers(s, NULL, write_select, c);
}

#define READ_SIZE	64240
#define TOTAL_READ	(4193008 - READ_SIZE)

static void read_select(void *c_)
{
	struct connection *c = (struct connection *)c_;
	int total_read = 0;
	struct read_buffer *rb;
	int rd;
	if (!(rb = c->buffer)) {
		internal("read socket has no buffer");
		setcstate(c, S_INTERNAL);
		abort_connection(c);
		return;
	}
	set_handlers(rb->sock, NULL, NULL, NULL);

read_more:
	if ((unsigned)rb->len > INT_MAX - sizeof(struct read_buffer) - READ_SIZE)
		overalloc();
	rb = xrealloc(rb, sizeof(struct read_buffer) + rb->len + READ_SIZE);
	c->buffer = rb;

	if (c->ssl) {
		if ((rd = SSL_read(c->ssl->ssl, (void *)(rb->data + rb->len), READ_SIZE)) <= 0) {
			int err;
			if (total_read) goto success;
			err = SSL_get_error(c->ssl->ssl, rd);
			if (err == SSL_ERROR_WANT_READ) {
				set_handlers(rb->sock, read_select, NULL, c);
				return;
			}
			if (err == SSL_ERROR_WANT_WRITE) {
				set_handlers(rb->sock, NULL, read_select, c);
				return;
			}
			if (rb->close && !rd) {
				rb->close = 2;
				rb->done(c, rb);
				return;
			}
			setcstate(c, rd ? (err == SSL_ERROR_SYSCALL ? get_error_from_errno(errno) : S_SSL_ERROR) : S_CANT_READ);
			if (!rd || err == SSL_ERROR_SYSCALL) retry_connection(c);
			else abort_connection(c);
			return;
		}
		c->ssl->bytes_read += rd;
	} else {
		EINTRLOOP(rd, (int)read(rb->sock, rb->data + rb->len, READ_SIZE));
		if (rd <= 0) {
			if (total_read) goto success;
			if (rb->close && !rd) {
				rb->close = 2;
				rb->done(c, rb);
				return;
			}
			setcstate(c, rd ? get_error_from_errno(errno) : S_CANT_READ);
			retry_connection(c);
			return;
		}
	}
	rb->len += rd;
	total_read += rd;

	if ((rd == READ_SIZE || c->ssl) && total_read <= TOTAL_READ) {
		if (can_read(rb->sock))
			goto read_more;
	}
success:
	retrieve_ssl_session(c);
	rb->done(c, rb);
}

struct read_buffer *alloc_read_buffer(void)
{
	struct read_buffer *rb;
	rb = xmalloc(sizeof(struct read_buffer) + READ_SIZE);
	memset(rb, 0, sizeof(struct read_buffer) + READ_SIZE);
	return rb;
}

void read_from_socket(struct connection *c, int s, struct read_buffer *buf, void (*read_func)(struct connection *, struct read_buffer *))
{
	buf->done = read_func;
	buf->sock = s;
	if (buf != c->buffer)
		free(c->buffer);
	c->buffer = buf;
	set_handlers(s, read_select, NULL, c);
}

void kill_buffer_data(struct read_buffer *rb, int n)
{
	if (n > rb->len || n < 0) {
		internal("called kill_buffer_data with bad value");
		rb->len = 0;
		return;
	}
	memmove(rb->data, rb->data + n, rb->len - n);
	rb->len -= n;
}
