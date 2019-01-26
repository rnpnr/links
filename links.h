/* links.h
 * (c) 2002 Mikulas Patocka, Karel 'Clock' Kulhavy, Petr 'Brain' Kulhavy,
 *          Martin 'PerM' Pergel
 * This file is a part of the Links program, released under GPL.
 */

#define LINKS_COPYRIGHT "(C) 1999 - 2018 Mikulas Patocka\n(C) 2000 - 2018 Petr Kulhavy, Karel Kulhavy, Martin Pergel"

#include <dirent.h>
#include <errno.h>
#include <event.h>
#include <fcntl.h>
#include <grp.h>
#include <netdb.h>
#include <poll.h>
#include <pwd.h>
#include <search.h>
#include <signal.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

#include "os_dep.h"
#include "setup.h"

#include <arpa/inet.h>

#include <openssl/ssl.h>

#if defined(G)
#if defined(HAVE_PNG_H)
#include <png.h>
#endif /* #if defined(HAVE_PNG_H) */
#ifndef png_jmpbuf
#define png_jmpbuf(png_ptr)	((png_ptr)->jmpbuf)
#endif
#ifndef _SETJMP_H
#include <setjmp.h>
#endif /* _SETJMP_H */
#endif /* #if defined(G) */


#define longlong long long
#define ulonglong unsigned long long

#define stringify_internal(arg)	#arg
#define stringify(arg)		stringify_internal(arg)

#define array_elements(a)	(sizeof(a) / sizeof(*a))

#ifdef HAVE_POINTER_COMPARISON_BUG
#define DUMMY ((void *)1L)
#else
#define DUMMY ((void *)-1L)
#endif

#define cast_const_char	(const char *)
#define cast_char	(char *)
#define cast_uchar	(unsigned char *)

#define RET_OK		0
#define RET_ERROR	1
#define RET_SYNTAX	2

#define EINTRLOOPX(ret_, call_, x_)			\
do {							\
	(ret_) = (call_);				\
} while ((ret_) == (x_) && errno == EINTR)

#define EINTRLOOP(ret_, call_)	EINTRLOOPX(ret_, call_, -1)

#define ENULLLOOP(ret_, call_)				\
do {							\
	errno = 0;					\
	(ret_) = (call_);				\
} while (!(ret_) && errno == EINTR)

#ifndef G
#define F 0
#else
extern int F;
#endif

#ifndef G
#define gf_val(x, y) (x)
#define GF(x)
#else
#define gf_val(x, y) (F ? (y) : (x))
#define GF(x) if (F) {x;}
#endif

#define MAX_STR_LEN	1024

#define BIN_SEARCH(entries, eq, ab, key, result)			\
{									\
	int s_ = 0, e_ = (entries) - 1;					\
	(result) = -1;							\
	while (s_ <= e_) {						\
		int m_ = (int)(((unsigned)s_ + (unsigned)e_) / 2);	\
		if (eq((m_), (key))) {					\
			(result) = m_;					\
			break;						\
		}							\
		if (ab((m_), (key))) e_ = m_ - 1;			\
		else s_ = m_ + 1;					\
	}								\
}									\

void die(const char *, ...);
void *xmalloc(size_t);
void *xrealloc(void *, size_t);
void usage(void);
#define internal die
#define error die
#define fatal_exit die

/* error.c */

#define overalloc_at(f, l)						\
do {									\
	fatal_exit("ERROR: attempting to allocate too large block at %s:%d", f, l);\
} while (1)	/* while (1) is not a typo --- it's here to allow the compiler
	that doesn't know that fatal_exit doesn't return to do better
	optimizations */

#define overalloc()	overalloc_at(__FILE__, __LINE__)

#define overflow()							\
do {									\
	fatal_exit("ERROR: arithmetic overflow at %s:%d", __FILE__, __LINE__);\
} while (1)

static inline int test_int_overflow(int x, int y, int *result)
{
	int z = *result = x + y;
	return ~(x ^ y) & (x ^ z) & (int)(1U << (sizeof(unsigned int) * 8 - 1));
}

static inline int safe_add_function(int x, int y, unsigned char *file, int line)
{
	int ret;
	if (test_int_overflow(x, y, &ret))
		fatal_exit("ERROR: arithmetic overflow at %s:%d: %d + %d", file, line, (x), (y));
	return ret;
}

#define safe_add(x, y)	safe_add_function(x, y, (unsigned char *)__FILE__, __LINE__)

void *mem_calloc(size_t size);

unsigned char *memacpy(const unsigned char *src, size_t len);
unsigned char *stracpy(const unsigned char *src);

#define pr(code) if (1) {code;} else

/* inline */

#define ALLOC_GR	0x040		/* must be power of 2 */

#define get_struct_(ptr, struc, entry)	((struc *)((char *)(ptr) - offsetof(struc, entry)))
#define get_struct(ptr, struc, entry)	((void)(&get_struct_(ptr, struc, entry)->entry == (ptr)), get_struct_(ptr, struc, entry))

struct list_head {
	struct list_head *next;
	struct list_head *prev;
};

#define verify_list_entry(e)			((void)0)
#define verify_double_add(l, e)			((void)0)

#define list_struct(ptr, struc)			get_struct(ptr, struc, list_entry)
#define init_list(x)				do { (x).next = &(x); (x).prev = &(x); } while (0)
#define list_empty(x)				(verify_list_entry(&x), (x).next == &(x))
#define del_list_entry(x)			do { verify_list_entry(x); (x)->next->prev = (x)->prev; (x)->prev->next = (x)->next; (x)->prev = (x)->next = NULL; } while (0)
#define del_from_list(x)			del_list_entry(&(x)->list_entry)
#define add_after_list_entry(p, x)		do { verify_double_add(p, x); verify_list_entry(p); (x)->next = (p)->next; (x)->prev = (p); (p)->next = (x); (x)->next->prev = (x); } while (0)
#define add_before_list_entry(p, x)		do { verify_double_add(p, x); verify_list_entry(p); (x)->prev = (p)->prev; (x)->next = (p); (p)->prev = (x); (x)->prev->next = (x); } while (0)
#define add_to_list(l, x)			add_after_list_entry(&(l), &(x)->list_entry)
#define add_to_list_end(l, x)			add_before_list_entry(&(l), &(x)->list_entry)
#define add_after_pos(p, x)			add_after_list_entry(&(p)->list_entry, &(x)->list_entry)
#define add_before_pos(p, x)			add_before_list_entry(&(p)->list_entry, &(x)->list_entry)
#define fix_list_after_realloc(x)		do { (x)->list_entry.prev->next = &(x)->list_entry; (x)->list_entry.next->prev = &(x)->list_entry; } while (0)
#define foreachfrom(struc, e, h, l, s)		for ((h) = (s); verify_list_entry(h), (h) == &(l) ? 0 : ((e) = list_struct(h, struc), 1); (h) = (h)->next)
#define foreach(struc, e, h, l)			foreachfrom(struc, e, h, l, (l).next)
#define foreachbackfrom(struc, e, h, l, s)	for ((h) = (s); verify_list_entry(h), (h) == &(l) ? 0 : ((e) = list_struct(h, struc), 1); (h) = (h)->prev)
#define foreachback(struc, e, h, l)		foreachbackfrom(struc, e, h, l, (l).prev)
#define free_list(struc, l)			do { while (!list_empty(l)) { struc *a__ = list_struct((l).next, struc); del_from_list(a__); free(a__); } } while (0)

static inline int list_size(struct list_head *l)
{
	struct list_head *e;
	int n = 0;
	for (e = l->next; e != l; e = e->next)
		n++;
	return n;
}

#define list_entry_1st		struct list_head list_entry;
#define list_entry_last
#define init_list_1st(x)	{ (x), (x) },
#define init_list_last(x)

#define WHITECHAR(x) ((x) == 9 || (x) == 10 || (x) == 12 || (x) == 13 || (x) == ' ')
#define U(x) ((x) == '"' || (x) == '\'')

#define CI_BYTES	1
#define CI_FILES	2
#define CI_LOCKED	3
#define CI_LOADING	4
#define CI_TIMERS	5
#define CI_TRANSFER	6
#define CI_CONNECTING	7
#define CI_KEEP		8

/* string.c */

int snprint(unsigned char *s, int n, int num);
int snzprint(unsigned char *s, int n, off_t num);
void add_to_strn(unsigned char **s, unsigned char *a);
void extend_str(unsigned char **s, int n);

static inline unsigned char *init_str()
{
	unsigned char *p;

	p = mem_calloc(1L);
	return p;
}

void add_bytes_to_str(unsigned char **s, int *l, unsigned char *a, size_t ll);
void add_to_str(unsigned char **s, int *l, unsigned char *a);
void add_chr_to_str(unsigned char **s, int *l, unsigned char a);
void add_unsigned_num_to_str(unsigned char **s, int *l, off_t n);
void add_unsigned_long_num_to_str(unsigned char **s, int *l, unsigned long n);
void add_num_to_str(unsigned char **s, int *l, off_t n);
void add_knum_to_str(unsigned char **s, int *l, off_t n);
long strtolx(unsigned char *c, unsigned char **end);

void safe_strncpy(unsigned char *dst, const unsigned char *src, size_t dst_size);
/* case insensitive compare of 2 strings */
/* comparison ends after len (or less) characters */
/* return value: 1=strings differ, 0=strings are same */
static inline unsigned upcase(unsigned a)
{
	if (a >= 'a' && a <= 'z')
		a -= 0x20;
	return a;
}
static inline unsigned locase(unsigned a)
{
	if (a >= 'A' && a <= 'Z')
		a += 0x20;
	return a;
}
static inline int srch_cmp(unsigned char c1, unsigned char c2)
{
	return upcase(c1) != upcase(c2);
}
int casestrcmp(const unsigned char *s1, const unsigned char *s2);
int casecmp(const unsigned char *c1, const unsigned char *c2, size_t len);
int casestrstr(const unsigned char *h, const unsigned char *n);
static inline int xstrcmp(const unsigned char *s1, const unsigned char *s2)
{
	if (!s1 && !s2) return 0;
	if (!s1) return -1;
	if (!s2) return 1;
	return strcmp(cast_const_char s1, cast_const_char s2);
}

static inline int cmpbeg(const unsigned char *str, const unsigned char *b)
{
	while (*str && upcase(*str) == upcase(*b)) {
		str++;
		b++;
	}
	return !!*b;
}


/* os_dep.c */

typedef unsigned long long uttime;
typedef unsigned long long tcount;

struct terminal;

struct open_in_new {
	unsigned char *text;
	unsigned char *hk;
	int (* const *open_window_fn)(struct terminal *, unsigned char *, unsigned char *);
};

void close_fork_tty(void);
int is_screen(void);
int is_xterm(void);
int get_terminal_size(int, int *, int *);
void handle_terminal_resize(int, void (*)(void));
void unhandle_terminal_resize(int);
void set_nonblock(int);
int c_pipe(int *);
int c_dup(int oh);
int c_socket(int, int, int);
int c_accept(int, struct sockaddr *, socklen_t *);
int c_open(unsigned char *, int);
int c_open3(unsigned char *, int, int);
DIR *c_opendir(unsigned char *);
#ifdef OS_SETRAW
int setraw(int ctl, int save);
#endif
int start_thread(void (*)(void *, int), void *, int, int);
unsigned char *get_clipboard_text(struct terminal *);
void set_clipboard_text(struct terminal *, unsigned char *);
int clipboard_support(struct terminal *);
void set_window_title(unsigned char *);
unsigned char *get_window_title(void);
int is_safe_in_shell(unsigned char);
unsigned char *escape_path(unsigned char *);
void check_shell_security(unsigned char **);
void check_filename(unsigned char **);
int check_shell_url(unsigned char *);
void do_signal(int sig, void (*handler)(int));
uttime get_time(void);
uttime get_absolute_time(void);
void ignore_signals(void);
int os_get_system_name(unsigned char *buffer);
unsigned char *os_conv_to_external_path(unsigned char *, unsigned char *);
unsigned char *os_fixup_external_program(unsigned char *);
int exe(unsigned char *, int);
int can_open_os_shell(int);
unsigned char *links_xterm(void);
struct open_in_new *get_open_in_new(int);

void os_free_clipboard(void);

void os_detach_console(void);

/* memory.c */

#define SH_CHECK_QUOTA		0
#define SH_FREE_SOMETHING	1
#define SH_FREE_ALL		2

#define ST_SOMETHING_FREED	1
#define ST_CACHE_EMPTY		2

#define MF_GPI			1

int shrink_memory(int);
void register_cache_upcall(int (*)(int), int, unsigned char *);
void free_all_caches(void);
int out_of_memory(void);

/* select.c */

#ifndef FD_SETSIZE
#define FD_SETSIZE	(sizeof(fd_set) * 8)
#endif

#define NBIT(p)		(sizeof((p)->fds_bits[0]) * 8)

#ifndef FD_SET
#define FD_SET(n, p)    ((p)->fds_bits[(n)/NBIT(p)] |= (1 << ((n) % NBIT(p))))
#endif
#ifndef FD_CLR
#define FD_CLR(n, p)    ((p)->fds_bits[(n)/NBIT(p)] &= ~(1 << ((n) % NBIT(p))))
#endif
#ifndef FD_ISSET
#define FD_ISSET(n, p)  ((p)->fds_bits[(n)/NBIT(p)] & (1 << ((n) % NBIT(p))))
#endif
#ifndef FD_ZERO
#define FD_ZERO(p)      memset((void *)(p), 0, sizeof(*(p)))
#endif


extern int terminate_loop;

int can_write(int fd);
int can_read(int fd);
int can_read_timeout(int fd, int sec);
int close_stderr(void);
void restore_stderr(int);
unsigned long select_info(int);
void reinit_child(void);
void select_loop(void (*)(void));
void terminate_select(void);
void register_bottom_half(void (*)(void *), void *);
void unregister_bottom_half(void (*)(void *), void *);
void check_bottom_halves(void);
void add_event_string(unsigned char **, int *, struct terminal *);
struct timer;
struct timer *install_timer(uttime, void (*)(void *), void *);
void kill_timer(struct timer *);
void portable_sleep(unsigned msec);

#define H_READ	0
#define H_WRITE	1

void (*get_handler(int, int))(void *);
void *get_handler_data(int);
extern unsigned char *sh_file;
extern int sh_line;
void set_handlers_file_line(int, void (*)(void *), void (*)(void *), void *);
#define set_handlers(a, b, c, d)	(sh_file = (unsigned char *)__FILE__, sh_line = __LINE__, set_handlers_file_line(a, b, c, d))
void clear_events(int, int);
extern pid_t signal_pid;
extern int signal_pipe[2];
void install_signal_handler(int, void (*)(void *), void *, int);
void interruptible_signal(int sig, int in);
void block_signals(int except1, int except2);
void unblock_signals(void);
void set_sigcld(void);

/* dns.c */

#define MAX_ADDRESSES		64

struct host_address {
	int af;
	unsigned char addr[16];
	unsigned scope_id;
};

struct lookup_result {
	int n;
	struct host_address a[MAX_ADDRESSES];
};

struct lookup_state {
	struct lookup_result addr;
	int addr_index;
	int dont_try_more_servers;
	int socks_port;
	int target_port;
};

extern int support_ipv6;

int numeric_ip_address(const char *name, char address[4]);
int numeric_ipv6_address(const char *name, char address[16], unsigned *scope_id);
void rotate_addresses(struct lookup_result *);
void do_real_lookup(unsigned char *, int, struct lookup_result *);
int find_host(unsigned char *, struct lookup_result *, void **, void (*)(void *, int), void *);
int find_host_no_cache(unsigned char *, struct lookup_result *, void **, void (*)(void *, int), void *);
void kill_dns_request(void **);
#if MAX_ADDRESSES > 1
void dns_set_priority(unsigned char *, struct host_address *, int);
#endif
void dns_clear_host(unsigned char *);
unsigned long dns_info(int type);
unsigned char *print_address(struct host_address *);
int ipv6_full_access(void);
void init_dns(void);

/* cache.c */

struct cache_entry {
	list_entry_1st
	unsigned char *head;
	int http_code;
	unsigned char *redirect;
	off_t length;
	off_t max_length;
	int incomplete;
	int tgc;
	unsigned char *last_modified;
	time_t expire_time;	/* 0 never, 1 always */
	off_t data_size;
	struct list_head frag;	/* struct fragment */
	tcount count;
	tcount count2;
	int refcount;
	unsigned char *decompressed;
	size_t decompressed_len;
	unsigned char *ip_address;
	unsigned char *ssl_info;
	list_entry_last
	unsigned char url[1];
};

struct fragment {
	list_entry_1st
	off_t offset;
	off_t length;
	off_t real_length;
	list_entry_last
	unsigned char data[1];
};

extern int page_size;

struct connection;

void init_cache(void);
int cache_info(int);
int decompress_info(int);
int find_in_cache(unsigned char *, struct cache_entry **);
int get_connection_cache_entry(struct connection *);
int new_cache_entry(unsigned char *, struct cache_entry **);
void detach_cache_entry(struct cache_entry *);
int add_fragment(struct cache_entry *, off_t, const unsigned char *, off_t);
int defrag_entry(struct cache_entry *);
void truncate_entry(struct cache_entry *, off_t, int);
void free_entry_to(struct cache_entry *, off_t);
void delete_entry_content(struct cache_entry *);
void delete_cache_entry(struct cache_entry *e);
void trim_cache_entry(struct cache_entry *e);

/* sched.c */

typedef struct {
	SSL *ssl;
	SSL_CTX *ctx;
	tcount bytes_read;
	tcount bytes_written;
	int session_set;
	int session_retrieved;
} links_ssl;

#define PRI_MAIN	0
#define PRI_DOWNLOAD	0
#define PRI_FRAME	1
#define PRI_NEED_IMG	2
#define PRI_IMG		3
#define PRI_PRELOAD	4
#define PRI_CANCEL	5
#define N_PRI		6

struct remaining_info {
	int valid;
	off_t size, loaded, last_loaded, cur_loaded;
	off_t pos;
	uttime elapsed;
	uttime last_time;
	uttime dis_b;
	off_t data_in_secs[CURRENT_SPD_SEC];
	struct timer *timer;
};

struct conn_info;

struct connection {
	list_entry_1st
	tcount count;
	unsigned char *url;
	unsigned char *prev_url;	/* allocated string with referrer or NULL */
	int running;
	int state;
	int prev_error;
	off_t from;
	int pri[N_PRI];
	int no_cache;
	int sock1;
	int sock2;
	void *dnsquery;
	pid_t pid;
	int tries;
	tcount netcfg_stamp;
	struct list_head statuss;
	void *info;
	void *buffer;
	struct conn_info *newconn;
	void (*conn_func)(void *);
	struct cache_entry *cache;
	off_t received;
	off_t est_length;
	int unrestartable;
	int no_compress;
	struct remaining_info prg;
	struct timer *timer;
	int detached;
	unsigned char socks_proxy[MAX_STR_LEN];
	unsigned char dns_append[MAX_STR_LEN];
	struct lookup_state last_lookup_state;
	links_ssl *ssl;
	int no_ssl_session;
	int no_tls;
	list_entry_last
};

extern tcount netcfg_stamp;

extern struct list_head queue;

struct k_conn {
	list_entry_1st
	void (*protocol)(struct connection *);
	unsigned char *host;
	int port;
	int conn;
	uttime timeout;
	uttime add_time;
	int protocol_data;
	links_ssl *ssl;
	struct lookup_state last_lookup_state;
	list_entry_last
};

extern struct list_head keepalive_connections;

#define NC_ALWAYS_CACHE	0
#define NC_CACHE	1
#define NC_IF_MOD	2
#define NC_RELOAD	3
#define NC_PR_NO_CACHE	4

#define S_WAIT		0
#define S_DNS		1
#define S_CONN		2
#define S_CONN_ANOTHER	3
#define S_SOCKS_NEG	4
#define S_SSL_NEG	5
#define S_SENT		6
#define S_LOGIN		7
#define S_GETH		8
#define S_PROC		9
#define S_TRANS		10

#define S__OK			(-2000000000)
#define S_INTERRUPTED		(-2000000001)
#define S_EXCEPT		(-2000000002)
#define S_INTERNAL		(-2000000003)
#define S_OUT_OF_MEM		(-2000000004)
#define S_NO_DNS		(-2000000005)
#define S_NO_PROXY_DNS		(-2000000006)
#define S_CANT_WRITE		(-2000000007)
#define S_CANT_READ		(-2000000008)
#define S_MODIFIED		(-2000000009)
#define S_BAD_URL		(-2000000010)
#define S_BAD_PROXY		(-2000000011)
#define S_TIMEOUT		(-2000000012)
#define S_RESTART		(-2000000013)
#define S_STATE			(-2000000014)
#define S_CYCLIC_REDIRECT	(-2000000015)
#define S_LARGE_FILE		(-2000000016)
#define S_SMB_NOT_ALLOWED	(-2000000018)
#define S_FILE_NOT_ALLOWED	(-2000000019)
#define S_NO_PROXY		(-2000000020)

#define S_HTTP_ERROR		(-2000000100)
#define S_HTTP_100		(-2000000101)
#define S_HTTP_204		(-2000000102)
#define S_HTTPS_FWD_ERROR	(-2000000103)
#define S_INVALID_CERTIFICATE	(-2000000104)
#define S_DOWNGRADED_METHOD	(-2000000105)
#define S_INSECURE_CIPHER	(-2000000106)

#define S_FILE_TYPE		(-2000000200)
#define S_FILE_ERROR		(-2000000201)

#define S_FTP_ERROR		(-2000000300)
#define S_FTP_UNAVAIL		(-2000000301)
#define S_FTP_LOGIN		(-2000000302)
#define S_FTP_PORT		(-2000000303)
#define S_FTP_NO_FILE		(-2000000304)
#define S_FTP_FILE_ERROR	(-2000000305)

#define S_SSL_ERROR		(-2000000400)
#define S_NO_SSL		(-2000000401)

#define S_BAD_SOCKS_VERSION	(-2000000500)
#define S_SOCKS_REJECTED	(-2000000501)
#define S_SOCKS_NO_IDENTD	(-2000000502)
#define S_SOCKS_BAD_USERID	(-2000000503)
#define S_SOCKS_UNKNOWN_ERROR	(-2000000504)

#define S_NO_SMB_CLIENT		(-2000000600)

#define S_WAIT_REDIR		(-2000000700)

#define S_UNKNOWN_ERROR		(-2000000800)

#define S_MAX			(-2000000900)


struct status {
	list_entry_1st
	struct connection *c;
	struct cache_entry *ce;
	int state;
	int prev_error;
	int pri;
	void (*end)(struct status *, void *);
	void *data;
	struct remaining_info *prg;
	list_entry_last
};

unsigned char *get_proxy_string(unsigned char *url);
unsigned char *get_proxy(unsigned char *url);
int is_proxy_url(unsigned char *url);
unsigned char *remove_proxy_prefix(unsigned char *url);
int get_allow_flags(unsigned char *url);
int disallow_url(unsigned char *url, int allow_flags);
void check_queue(void *dummy);
unsigned long connect_info(int);
void setcstate(struct connection *c, int);
int get_keepalive_socket(struct connection *c, int *protocol_data);
void add_keepalive_socket(struct connection *c, uttime timeout, int protocol_data);
int is_connection_restartable(struct connection *c);
int is_last_try(struct connection *c);
void retry_connection(struct connection *c);
void abort_connection(struct connection *c);
#define ALLOW_SMB		1
#define ALLOW_FILE		2
#define ALLOW_ALL		(ALLOW_SMB | ALLOW_FILE)
void load_url(unsigned char *, unsigned char *, struct status *, int, int, int, int, off_t);
void change_connection(struct status *, struct status *, int);
void detach_connection(struct status *, off_t, int, int);
void abort_all_connections(void);
void abort_background_connections(void);
int is_entry_used(struct cache_entry *);
void clear_connection_timeout(struct connection *);
void set_connection_timeout(struct connection *);
void add_blacklist_entry(unsigned char *, int);
void del_blacklist_entry(unsigned char *, int);
int get_blacklist_flags(unsigned char *);
void free_blacklist(void);

#define BL_HTTP10		0x001
#define BL_NO_ACCEPT_LANGUAGE	0x002
#define BL_NO_CHARSET		0x004
#define BL_NO_RANGE		0x008
#define BL_NO_COMPRESSION	0x010
#define BL_NO_BZIP2		0x020
#define BL_IGNORE_CERTIFICATE	0x040
#define BL_IGNORE_DOWNGRADE	0x080
#define BL_IGNORE_CIPHER	0x100

/* suffix.c */

int is_tld(unsigned char *name);
int allow_cookie_domain(unsigned char *server, unsigned char *domain);

/* url.c */

struct session;

#define POST_CHAR		1
#define POST_CHAR_STRING	"\001"

static inline int end_of_dir(unsigned char *url, unsigned char c)
{
	return c == POST_CHAR || c == '#' || ((c == ';' || c == '?') && (!url || !casecmp(url, (unsigned char *)"http", 4)));
}

int parse_url(unsigned char *, int *, unsigned char **, int *, unsigned char **, int *, unsigned char **, int *, unsigned char **, int *, unsigned char **, int *, unsigned char **);
unsigned char *get_protocol_name(unsigned char *);
unsigned char *get_host_name(unsigned char *);
unsigned char *get_keepalive_id(unsigned char *);
unsigned char *get_user_name(unsigned char *);
unsigned char *get_pass(unsigned char *);
int get_port(unsigned char *);
unsigned char *get_port_str(unsigned char *);
void (*get_protocol_handle(unsigned char *))(struct connection *);
void (*get_external_protocol_function(unsigned char *))(struct session *, unsigned char *);
int url_bypasses_socks(unsigned char *);
unsigned char *get_url_data(unsigned char *);
int url_non_ascii(unsigned char *url);
unsigned char *join_urls(unsigned char *, unsigned char *);
unsigned char *translate_url(unsigned char *, unsigned char *);
unsigned char *extract_position(unsigned char *);
int url_not_saveable(unsigned char *);
void add_conv_str(unsigned char **s, int *l, unsigned char *b, int ll, int encode_special);
void convert_file_charset(unsigned char **s, int *l, int start_l);
unsigned char *idn_encode_host(unsigned char *host, int len, unsigned char *separator, int decode);
unsigned char *idn_encode_url(unsigned char *url, int decode);
unsigned char *display_url(struct terminal *term, unsigned char *url, int warn_idn);
unsigned char *display_host(struct terminal *term, unsigned char *host);
unsigned char *display_host_list(struct terminal *term, unsigned char *host);

/* connect.c */

struct read_buffer {
	int sock;
	int len;
	int close;
	void (*done)(struct connection *, struct read_buffer *);
	unsigned char data[1];
};

int socket_and_bind(int pf, unsigned char *address);
void close_socket(int *);
void make_connection(struct connection *, int, int *, void (*)(struct connection *));
void retry_connect(struct connection *, int, int);
void continue_connection(struct connection *, int *, void (*)(struct connection *));
int is_ipv6(int);
int get_pasv_socket(struct connection *, int, int *, unsigned char *);
int get_pasv_socket_ipv6(struct connection *, int, int *, unsigned char *);
void write_to_socket(struct connection *, int, unsigned char *, int, void (*)(struct connection *));
struct read_buffer *alloc_read_buffer(struct connection *c);
void read_from_socket(struct connection *, int, struct read_buffer *, void (*)(struct connection *, struct read_buffer *));
void kill_buffer_data(struct read_buffer *, int);

/* cookies.c */

struct cookie {
	list_entry_1st
	unsigned char *name, *value;
	unsigned char *server;
	unsigned char *path, *domain;
	time_t expires; /* zero means undefined */
	int secure;
	list_entry_last
};

struct c_domain {
	list_entry_1st
	list_entry_last
	unsigned char domain[1];
};


extern struct list_head all_cookies;
extern struct list_head c_domains;

int set_cookie(struct terminal *, unsigned char *, unsigned char *);
void add_cookies(unsigned char **, int *, unsigned char *);
void init_cookies(void);
void free_cookies(void);
int is_in_domain(unsigned char *d, unsigned char *s);
int is_path_prefix(unsigned char *d, unsigned char *s);
int cookie_expired(struct cookie *c);
void free_cookie(struct cookie *c);

/* auth.c */

unsigned char *get_auth_realm(unsigned char *url, unsigned char *head, int proxy);
unsigned char *get_auth_string(unsigned char *url, int proxy);
void free_auth(void);
void add_auth(unsigned char *url, unsigned char *realm, unsigned char *user, unsigned char *password, int proxy);
int find_auth(unsigned char *url, unsigned char *realm);

/* http.c */

int get_http_code(unsigned char *head, int *code, int *version);
unsigned char *parse_http_header(unsigned char *, unsigned char *, unsigned char **);
unsigned char *parse_header_param(unsigned char *, unsigned char *, int);
void http_func(struct connection *);
void proxy_func(struct connection *);

/* https.c */

void https_func(struct connection *c);
void ssl_finish(void);
links_ssl *getSSL(void);
void freeSSL(links_ssl *);
int verify_ssl_certificate(links_ssl *ssl, unsigned char *host);
int verify_ssl_cipher(links_ssl *ssl);
int ssl_not_reusable(links_ssl *ssl);
unsigned char *get_cipher_string(links_ssl *ssl);

SSL_SESSION *get_session_cache_entry(SSL_CTX *ctx, unsigned char *host, int port);
void retrieve_ssl_session(struct connection *c);
unsigned long session_info(int type);
void init_session_cache(void);

/* data.c */

void data_func(struct connection *);

/* file.c */

void file_func(struct connection *);

/* kbd.c */

#define BM_BUTT		15
#define B_LEFT		0
#define B_MIDDLE	1
#define B_RIGHT		2
#define B_FOURTH	3
#define B_FIFTH		4
#define B_SIXTH		5
#define B_WHEELUP	8
#define B_WHEELDOWN	9
#define B_WHEELUP1	10
#define B_WHEELDOWN1	11
#define B_WHEELLEFT	12
#define B_WHEELRIGHT	13
#define B_WHEELLEFT1	14
#define B_WHEELRIGHT1	15
#define BM_IS_WHEEL(b)	((b) & 8)

#define BM_ACT		48
#define B_DOWN		0
#define B_UP		16
#define B_DRAG		32
#define B_MOVE		48

#define KBD_ENTER	-0x100
#define KBD_BS		-0x101
#define KBD_TAB		-0x102
#define KBD_ESC		-0x103
#define KBD_LEFT	-0x104
#define KBD_RIGHT	-0x105
#define KBD_UP		-0x106
#define KBD_DOWN	-0x107
#define KBD_INS		-0x108
#define KBD_DEL		-0x109
#define KBD_HOME	-0x10a
#define KBD_END		-0x10b
#define KBD_PAGE_UP	-0x10c
#define KBD_PAGE_DOWN	-0x10d
#define KBD_MENU	-0x10e
#define KBD_STOP	-0x10f

#define KBD_F1		-0x120
#define KBD_F2		-0x121
#define KBD_F3		-0x122
#define KBD_F4		-0x123
#define KBD_F5		-0x124
#define KBD_F6		-0x125
#define KBD_F7		-0x126
#define KBD_F8		-0x127
#define KBD_F9		-0x128
#define KBD_F10		-0x129
#define KBD_F11		-0x12a
#define KBD_F12		-0x12b

#define KBD_UNDO	-0x140
#define KBD_REDO	-0x141
#define KBD_FIND	-0x142
#define KBD_HELP	-0x143
#define KBD_COPY	-0x144
#define KBD_PASTE	-0x145
#define KBD_CUT		-0x146
#define KBD_PROPS	-0x147
#define KBD_FRONT	-0x148
#define KBD_OPEN	-0x149
#define KBD_BACK	-0x14a
#define KBD_FORWARD	-0x14b
#define KBD_RELOAD	-0x14c
#define KBD_BOOKMARKS	-0x14d

#define KBD_ESCAPE_MENU(x)	((x) <= KBD_F1 && (x) > KBD_CTRL_C)

#define KBD_CTRL_C	-0x200
#define KBD_CLOSE	-0x201

#define KBD_SHIFT	1
#define KBD_CTRL	2
#define KBD_ALT		4
#define KBD_PASTING	8

void handle_trm(int, void *, int);
void free_all_itrms(void);
void dispatch_special(unsigned char *);
void kbd_ctrl_c(void);
int is_blocked(void);

struct os2_key {
	int x, y;
};

extern struct os2_key os2xtd[256];

struct itrm;

extern unsigned char init_seq[];
extern unsigned char init_seq_x_mouse[];
extern unsigned char init_seq_tw_mouse[];
extern unsigned char term_seq[];
extern unsigned char term_seq_x_mouse[];
extern unsigned char term_seq_tw_mouse[];

struct rgb {
	unsigned char r, g, b; /* This is 3*8 bits with sRGB gamma (in sRGB space)
				* This is not rounded. */
	unsigned char pad;
};

#ifdef G

/* lru.c */

struct lru_entry {
	struct lru_entry *above, *below, *next;
	struct lru_entry **previous;
	void *data;
	unsigned bytes_consumed;
};

struct lru {
	int (*compare_function)(void *, void *);
	struct lru_entry *top, *bottom;
	int bytes, items;
};

void lru_insert(struct lru *cache, void *entry, struct lru_entry **row, unsigned bytes_consumed);
void *lru_get_bottom(struct lru *cache);
void lru_destroy_bottom(struct lru *cache);
void lru_init(struct lru *cache, int (*compare_function)(void *entry, void *templ));
void *lru_lookup(struct lru *cache, void *templ, struct lru_entry **row);

/* drivers.c */

/* Bitmap is allowed to pass only to that driver from which was obtained.
 * It is forbidden to get bitmap from svga driver and pass it to X driver.
 * It is impossible to get an error when registering a bitmap
 */
struct bitmap {
	int x, y; /* Dimensions */
	int skip; /* Byte distance between vertically consecutive pixels */
	void *data; /* Pointer to room for topleft pixel */
	void *flags; /* Allocation flags for the driver */
};

struct rect {
	int x1, x2, y1, y2;
};

struct rect_set {
	int rl;
	int m;
	struct rect r[1];
};

struct graphics_device {
	/* Only graphics driver is allowed to write to this */

	struct rect size; /* Size of the window */
	/*int left, right, top, bottom;*/
	struct rect clip;
		/* right, bottom are coords of the first point that are outside the clipping area */

	void *driver_data;

	/* Only user is allowed to write here, driver inits to zero's */
	void *user_data;
	void (*redraw_handler)(struct graphics_device *dev, struct rect *r);
	void (*resize_handler)(struct graphics_device *dev);
	void (*keyboard_handler)(struct graphics_device *dev, int key, int flags);
	void (*mouse_handler)(struct graphics_device *dev, int x, int y, int buttons);
};

struct driver_param;

struct graphics_driver {
	unsigned char *name;
	unsigned char *(*init_driver)(unsigned char *param, unsigned char *display);	/* param is get from get_driver_param and saved into configure file */

	/* Creates new device and returns pointer to it */
	struct graphics_device *(*init_device)(void);

	/* Destroys the device */
	void (*shutdown_device)(struct graphics_device *dev);

	void (*shutdown_driver)(void);

	unsigned char *(*get_driver_param)(void);	/* returns allocated string with parameter given to init_driver function */
	unsigned char *(*get_af_unix_name)(void);

	/* dest must have x and y filled in when get_empty_bitmap is called */
	int (*get_empty_bitmap)(struct bitmap *dest);

	void (*register_bitmap)(struct bitmap *bmp);

	void *(*prepare_strip)(struct bitmap *bmp, int top, int lines);
	void (*commit_strip)(struct bitmap *bmp, int top, int lines);

	/* Must not touch x and y. Suitable for re-registering. */
	void (*unregister_bitmap)(struct bitmap *bmp);
	void (*draw_bitmap)(struct graphics_device *dev, struct bitmap *hndl, int x, int y);

	/* Input into get_color has gamma 1/display_gamma.
	 * Input of 255 means exactly the largest sample the display is able to produce.
	 * Thus, if we have 3 bits for red, we will perform this code:
	 * red=((red*7)+127)/255;
	 */
	long (*get_color)(int rgb);

	void (*fill_area)(struct graphics_device *dev, int x1, int y1, int x2, int y2, long color);
	void (*draw_hline)(struct graphics_device *dev, int left, int y, int right, long color);
	void (*draw_vline)(struct graphics_device *dev, int x, int top, int bottom, long color);
	int (*scroll)(struct graphics_device *dev, struct rect_set **set, int scx, int scy);
	 /* When scrolling, the empty spaces will have undefined contents. */
	 /* returns:
	    0 - the caller should not care about redrawing, redraw will be sent
	    1 - the caller should redraw uncovered area */
	 /* when set is not NULL rectangles in the set (uncovered area) should be redrawn */
	void (*set_clip_area)(struct graphics_device *dev);

	void (*flush)(struct graphics_device *dev);

	void (*set_palette)(void);

	void (*set_title)(struct graphics_device *dev, unsigned char *title);
		/* set window title. title is in utf-8 encoding -- you should recode it to device charset */
		/* if device doesn't support titles (svgalib, framebuffer), this should be NULL, not empty function ! */

	int (*exec)(unsigned char *command, int flag);
		/* -if !NULL executes command on this graphics device,
		   -if NULL links uses generic (console) command executing
		    functions
		   -return value is the same as of the 'system' syscall
		   -if flag is !0, run command in separate shell
		    else run command directly
		 */

	void (*set_clipboard_text)(struct graphics_device *gd, unsigned char *text);
	unsigned char *(*get_clipboard_text)(void);

	int depth; /* Data layout
		    * depth
		    *  4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
		    * +---------+-+-+---------+-----+
		    * +         | | |         |     |
		    * +---------+-+-+---------+-----+
		    *
		    *  0 - 2  Number of bytes per pixel in passed bitmaps
		    *  3 - 7  Number of significant bits per pixel -- 1, 4, 8, 15, 16, 24
		    *  8      0 -- normal order, 1 -- misordered.Has the same value as vga_misordered from the VGA mode.
		    *  9      1 -- misordered (0rgb)
		    * 10 - 14 1 -- dither to the requested number of bits
		    *
		    * This number is to be used by the layer that generates images.
		    * Memory layout for 1 bytes per pixel is:
		    * 2 colors:
		    *  7 6 5 4 3 2 1 0
		    * +-------------+-+
		    * |      0      |B| B is The Bit. 0 black, 1 white
		    * +-------------+-+
		    *
		    * 16 colors:
		    *  7 6 5 4 3 2 1 0
		    * +-------+-------+
		    * |   0   | PIXEL | Pixel is 4-bit index into palette
		    * +-------+-------+
		    *
		    * 256 colors:
		    *  7 6 5 4 3 2 1 0
		    * +---------------+
		    * |  --PIXEL--    | Pixels is 8-bit index into palette
		    * +---------------+
		    */
	int x, y;	/* size of screen. only for drivers that use virtual devices */
	int flags;	/* GD_xxx flags */
	struct driver_param *param;
};

#define GD_DONT_USE_SCROLL	1
#define GD_NEED_CODEPAGE	2
#define GD_UNICODE_KEYS		4
#define GD_ONLY_1_WINDOW	8
#define GD_NOAUTO		16
#define GD_NO_OS_SHELL		32
#define GD_NO_LIBEVENT		64
#define GD_SELECT_PALETTE	128
#define GD_SWITCH_PALETTE	256

extern struct graphics_driver *drv;

#define CLIP_DRAW_BITMAP				\
	if (!is_rect_valid(&dev->clip)) return;		\
	if (!bmp->x || !bmp->y) return;			\
	if (x >= dev->clip.x2) return;			\
	if (x + bmp->x <= dev->clip.x1) return;		\
	if (y >= dev->clip.y2) return;			\
	if (y + bmp->y <= dev->clip.y1) return;		\

#define CLIP_FILL_AREA					\
	if (x1 < dev->clip.x1) x1 = dev->clip.x1;	\
	if (x2 > dev->clip.x2) x2 = dev->clip.x2;	\
	if (y1 < dev->clip.y1) y1 = dev->clip.y1;	\
	if (y2 > dev->clip.y2) y2 = dev->clip.y2;	\
	if (x1 >= x2 || y1 >= y2) return;		\

#define CLIP_DRAW_HLINE					\
	if (y < dev->clip.y1) return;			\
	if (y >= dev->clip.y2) return;			\
	if (x1 < dev->clip.x1) x1 = dev->clip.x1;	\
	if (x2 > dev->clip.x2) x2 = dev->clip.x2;	\
	if (x1 >= x2) return;				\

#define CLIP_DRAW_VLINE					\
	if (x < dev->clip.x1) return;			\
	if (x >= dev->clip.x2) return;			\
	if (y1 < dev->clip.y1) y1 = dev->clip.y1;	\
	if (y2 > dev->clip.y2) y2 = dev->clip.y2;	\
	if (y1 >= y2) return;				\

void add_graphics_drivers(unsigned char **s, int *l);
unsigned char *init_graphics(unsigned char *, unsigned char *, unsigned char *);
void shutdown_graphics(void);
void update_driver_param(void);
int g_kbd_codepage(struct graphics_driver *drv);

extern struct graphics_device **virtual_devices;
extern int n_virtual_devices;
extern struct graphics_device *current_virtual_device;

static inline int is_rect_valid(struct rect *r)
{
	return r->x1 < r->x2 && r->y1 < r->y2;
}
int do_rects_intersect(struct rect *, struct rect *);
void intersect_rect(struct rect *, struct rect *, struct rect *);
void unite_rect(struct rect *, struct rect *, struct rect *);
struct rect_set *init_rect_set(void);
void add_to_rect_set(struct rect_set **, struct rect *);
void exclude_rect_from_set(struct rect_set **, struct rect *);
static inline void exclude_from_set(struct rect_set **s, int x1, int y1, int x2, int y2)
{
	struct rect r;
	r.x1 = x1;
	r.x2 = x2;
	r.y1 = y1;
	r.y2 = y2;
	exclude_rect_from_set(s, &r);
}

void set_clip_area(struct graphics_device *dev, struct rect *r);
int restrict_clip_area(struct graphics_device *dev, struct rect *r, int x1, int y1, int x2, int y2);

struct rect_set *g_scroll(struct graphics_device *dev, int scx, int scy);

/* dip.c */

/* Digital Image Processing utilities
 * (c) 2000 Clock <clock@atrey.karlin.mff.cuni.cz>
 *
 * This file is a part of Links
 *
 * This file does gray scaling (for prescaling fonts), color scaling (for scaling images
 * where different size is defined in the HTML), two colors mixing (alpha monochromatic letter
 * on a monochromatic backround and font operations.
 */

#define sRGB_gamma	0.45455		/* For HTML, which runs
					 * according to sRGB standard. Number
					 * in HTML tag is linear to photons raised
					 * to this power.
					 */

extern unsigned aspect; /* Must hold at least 20 bits */
int fontcache_info(int type);

#define G_BFU_FONT_SIZE menu_font_size

struct letter {
	const unsigned char *begin; /* Begin in the byte stream (of PNG data) */
	int length; /* Length (in bytes) of the PNG data in the byte stream */
	int code; /* Unicode code of the character */
	short xsize; /* x size of the PNG image */
	short ysize; /* y size of the PNG image */
	struct lru_entry* color_list;
};

struct font {
	unsigned char *family;
	unsigned char *weight;
	unsigned char *slant;
	unsigned char *adstyl;
	unsigned char *spacing;
	int begin; /* Begin in the letter stream */
	int length; /* Length in the letter stream */
};

struct style {
	int refcount;
	unsigned char r0, g0, b0, r1, g1, b1;
	/* ?0 are background, ?1 foreground.
	 * These are unrounded 8-bit sRGB space
	 */
	int height;
	int flags; /* non-zero means underline */
	long underline_color; /* Valid only if flags are nonzero */
	int *table; /* First is refcount, then n_fonts entries. Total
		     * size is n_fonts+1 integers.
		     */
	int mono_space; /* -1 if the font is not monospaced
			 * width of the space otherwise
			 */
	int mono_height; /* Height of the space if mono_space is >=0
			  * undefined otherwise
			  */
};

struct font_cache_entry {
	unsigned char r0,g0,b0,r1,g1,b1;
	struct bitmap bitmap;
	int mono_space, mono_height; /* if the letter was rendered for a
	monospace font, then size of the space. Otherwise, mono_space
	is -1 and mono_height is undefined. */
};


struct cached_image;

void g_print_text(struct graphics_device *device, int x, int y, struct style *style, unsigned char *text, int *width);
int g_text_width(struct style *style, unsigned char *text);
int g_char_width(struct style *style, unsigned ch);
/*unsigned char ags_8_to_8(unsigned char input, float gamma);*/
unsigned short ags_8_to_16(unsigned char input, float gamma);
unsigned char ags_16_to_8(unsigned short input, float gamma);
unsigned short ags_16_to_16(unsigned short input, float gamma);
void agx_24_to_48(unsigned short *restrict dest, const unsigned char *restrict src, int
			  lenght, float red_gamma, float green_gamma, float
			  blue_gamma);
void make_gamma_table(struct cached_image *cimg);
void agx_24_to_48_table(unsigned short *restrict dest, const unsigned char *restrict src
	,int lenght, unsigned short *restrict gamma_table);
void agx_48_to_48_table(unsigned short *restrict dest,
		const unsigned short *restrict src, int lenght, unsigned short *restrict table);
void agx_48_to_48(unsigned short *restrict dest,
		const unsigned short *restrict src, int lenght, float red_gamma,
		float green_gamma, float blue_gamma);
void agx_and_uc_32_to_48_table(unsigned short *restrict dest,
		const unsigned char *restrict src, int lenght, unsigned short *restrict table,
		unsigned short rb, unsigned short gb, unsigned short bb);
void agx_and_uc_32_to_48(unsigned short *restrict dest,
		const unsigned char *restrict src, int lenght, float red_gamma,
		float green_gamma, float blue_gamma, unsigned short rb, unsigned
		short gb, unsigned short bb);
void agx_and_uc_64_to_48_table(unsigned short *restrict dest,
		const unsigned short *restrict src, int lenght, unsigned short *restrict gamma_table,
		unsigned short rb, unsigned short gb, unsigned short bb);
void agx_and_uc_64_to_48(unsigned short *restrict dest,
		const unsigned short *restrict src, int lenght, float red_gamma,
		float green_gamma, float blue_gamma, unsigned short rb, unsigned
		short gb, unsigned short bb);
void mix_one_color_48(unsigned short *restrict dest, int length,
		   unsigned short r, unsigned short g, unsigned short b);
void mix_one_color_24(unsigned char *restrict dest, int length,
		   unsigned char r, unsigned char g, unsigned char b);
void scale_color(unsigned short *in, int ix, int iy, unsigned short **out,
	int ox, int oy);
void update_aspect(void);

struct g_object;

struct wrap_struct {
	struct style *style;
	unsigned char *text;
	int pos;
	int width;
	struct g_object *obj;
	struct g_object *last_wrap_obj;
	unsigned char *last_wrap;
	int force_break;
};

int g_wrap_text(struct wrap_struct *);

int hack_rgb(int rgb);

#define FF_UNDERLINE	1

struct style *g_get_style(int fg, int bg, int size, unsigned char *font, int fflags);
struct style *g_invert_style(struct style *);
void g_free_style(struct style *style0);
struct style *g_clone_style(struct style *);

extern tcount gamma_stamp;

extern long gamma_cache_color;
extern int gamma_cache_rgb;

extern long real_dip_get_color_sRGB(int rgb);

static inline long dip_get_color_sRGB(int rgb)
{
	if (rgb == gamma_cache_rgb) return gamma_cache_color;
	else return real_dip_get_color_sRGB(rgb);
}

void init_dip(void);
void get_links_icon(char **data, int *width, int *height, int *skip, int pad);

#ifdef PNG_USER_MEM_SUPPORTED
void *my_png_alloc(png_structp png_ptr, png_size_t size);
void my_png_free(png_structp png_ptr, void *ptr);
#endif

/* dither.c */

extern int slow_fpu;	/* -1 --- don't know, 0 --- no, 1 --- yes */

/* Dithering functions (for blocks of pixels being dithered into bitmaps) */
void dither (unsigned short *in, struct bitmap *out);
int *dither_start(unsigned short *in, struct bitmap *out);
void dither_restart(unsigned short *in, struct bitmap *out, int *dregs);
extern void (*round_fn)(unsigned short *restrict in, struct bitmap *out);

long (*get_color_fn(int depth))(int rgb);
void init_dither(int depth);
void round_color_sRGB_to_48(unsigned short *restrict red, unsigned short *restrict green,
		unsigned short *restrict blue, int rgb);

void q_palette(unsigned size, unsigned color, unsigned scale, unsigned rgb[3]);
double rgb_distance(int r1, int g1, int b1, int r2, int g2, int b2);

void free_dither(void);

#endif

/* terminal.c */

extern unsigned char frame_dumb[];

typedef unsigned char_t;

typedef struct {
	char_t ch :24;
	unsigned char at;
} chr;

#define chr_has_padding		(sizeof(chr) != 4)

struct links_event {
	int ev;
	int x;
	int y;
	long b;
};

#define EV_INIT		0
#define EV_KBD		1
#define EV_MOUSE	2
#define EV_REDRAW	3
#define EV_RESIZE	4
#define EV_ABORT	5

#define EVH_NOT_PROCESSED		0
#define EVH_LINK_KEYDOWN_PROCESSED	1
#define EVH_LINK_KEYPRESS_PROCESSED	2
#define EVH_DOCUMENT_KEYDOWN_PROCESSED	3
#define EVH_DOCUMENT_KEYPRESS_PROCESSED	4

struct window {
	list_entry_1st
	void (*handler)(struct window *, struct links_event *, int fwd);
	void *data;
	int xp, yp;
	struct terminal *term;
#ifdef G
	struct rect pos;
	struct rect redr;
#endif
	list_entry_last
};

#define MAX_TERM_LEN	32	/* this must be multiple of 8! (alignment problems) */

#define MAX_CWD_LEN	4096	/* this must be multiple of 8! (alignment problems) */

#define ENV_XWIN	1
#define ENV_SCREEN	2
#define ENV_G		32768

struct term_spec;

struct terminal {
	list_entry_1st
	tcount count;

	int x;
	int y;
	/* text only */
	int master;
	int fdin;
	int fdout;
	int environment;
	unsigned char term[MAX_TERM_LEN];
	unsigned char cwd[MAX_CWD_LEN];
	chr *screen;
	chr *last_screen;
	struct term_spec *spec;
	int default_character_set;
	int cx;
	int cy;
	int lcx;
	int lcy;
	int dirty;
	int redrawing;
	int blocked;
	unsigned char *input_queue;
	int qlen;
	int real_x;
	int real_y;
	int left_margin;
	int top_margin;
	/* end-of text only */

	struct list_head windows;
	unsigned char *title;

	int handle_to_close;
#ifdef G
	struct graphics_device *dev;
	int last_mouse_x;
	int last_mouse_y;
	int last_mouse_b;
#endif
	unsigned char utf8_buffer[7];
	int utf8_paste_mode;
	list_entry_last
};

struct term_spec {
	list_entry_1st
	unsigned char term[MAX_TERM_LEN];
	int mode;
	int m11_hack;
	int restrict_852;
	int block_cursor;
	int col;
	int character_set;
	int left_margin;
	int right_margin;
	int top_margin;
	int bottom_margin;
	list_entry_last
};

#define TERM_DUMB	0
#define TERM_VT100	1

#define ATTR_FRAME	0x80

extern struct list_head term_specs;
extern struct list_head terminals;

static inline int term_charset(struct terminal *term)
{
	if (term->spec->character_set >= 0)
		return term->spec->character_set;
	return term->default_character_set;
}

int hard_write(int, const unsigned char *, int);
int hard_read(int, unsigned char *, int);
unsigned char *get_cwd(void);
void set_cwd(unsigned char *);
unsigned char get_attribute(int, int);
struct terminal *init_term(int, int, void (*)(struct window *, struct links_event *, int));
#ifdef G
struct terminal *init_gfx_term(void (*)(struct window *, struct links_event *, int), unsigned char *, void *, int);
#endif
struct term_spec *new_term_spec(unsigned char *);
void free_term_specs(void);
void destroy_terminal(void *);
void cls_redraw_all_terminals(void);
void redraw_below_window(struct window *);
void add_window(struct terminal *, void (*)(struct window *, struct links_event *, int), void *);
void delete_window(struct window *);
void delete_window_ev(struct window *, struct links_event *ev);
void set_window_ptr(struct window *, int, int);
void get_parent_ptr(struct window *, int *, int *);
void add_empty_window(struct terminal *, void (*)(void *), void *);
void draw_to_window(struct window *, void (*)(struct terminal *, void *), void *);
void redraw_all_terminals(void);
void flush_terminal(struct terminal *);

#ifdef G

void set_window_pos(struct window *, int, int, int, int);
void t_redraw(struct graphics_device *, struct rect *);
void t_resize(struct graphics_device *);
void t_kbd(struct graphics_device *, int, int);
void t_mouse(struct graphics_device *, int, int, int);

#endif

/* text only */
void set_char(struct terminal *, int, int, unsigned, unsigned char);
const chr *get_char(struct terminal *, int, int);
void set_color(struct terminal *, int, int, unsigned char);
void set_only_char(struct terminal *, int, int, unsigned, unsigned char);
void set_line(struct terminal *, int, int, int, chr *);
void set_line_color(struct terminal *, int, int, int, unsigned char);
void fill_area(struct terminal *, int, int, int, int, unsigned, unsigned char);
void draw_frame(struct terminal *, int, int, int, int, unsigned char, int);
void print_text(struct terminal *, int, int, int, unsigned char *, unsigned char);
void set_cursor(struct terminal *, int, int, int, int);

void destroy_all_terminals(void);
void block_itrm(int);
int unblock_itrm(int);
void exec_thread(void *, int);
void close_handle(void *);

#define TERM_FN_TITLE	1
#define TERM_FN_RESIZE	2

#ifdef G
int have_extra_exec(void);
#endif
void exec_on_terminal(struct terminal *, unsigned char *, unsigned char *, unsigned char);
void do_terminal_function(struct terminal *, unsigned char, unsigned char *);
void set_terminal_title(struct terminal *, unsigned char *);
struct terminal *find_terminal(tcount count);

/* language.c */

#include "language.h"

extern unsigned char dummyarray[];

extern int current_language;

unsigned char *get_text_translation(unsigned char *, struct terminal *term);

#define TEXT_(x) (dummyarray + x) /* TEXT causes name clash on windows */

/* main.c */

extern int terminal_pipe[2];

extern int retval;

extern const char *argv0;
extern unsigned char **g_argv;
extern int g_argc;

void sig_tstp(void *t);
void sig_cont(void *t);

void unhandle_terminal_signals(struct terminal *term);
int attach_terminal(void *, int);
#ifdef G
int attach_g_terminal(unsigned char *, void *, int);
void gfx_connection(int);
#endif

/* objreq.c */

#define O_WAITING	0
#define O_LOADING	1
#define O_FAILED	-1
#define O_INCOMPLETE	-2
#define O_OK		-3

struct object_request {
	list_entry_1st
	int refcount;
	tcount count;
	tcount term;
	struct status stat;
	struct cache_entry *ce_internal;
	struct cache_entry *ce;
	unsigned char *orig_url;
	unsigned char *url;
	unsigned char *prev_url;	/* allocated string with referrer or NULL */
	unsigned char *goto_position;
	int pri;
	int cache;
	void (*upcall)(struct object_request *, void *);
	void *data;
	int redirect_cnt;
	int state;
#define HOLD_AUTH	1
#define HOLD_CERT	2
	int hold;
	int dont_print_error;
	struct timer *timer;

	off_t last_bytes;

	uttime last_update;
	list_entry_last
};

void request_object(struct terminal *, unsigned char *, unsigned char *, int, int, int, void (*)(struct object_request *, void *), void *, struct object_request **);
void clone_object(struct object_request *, struct object_request **);
void release_object(struct object_request **);
void release_object_get_stat(struct object_request **, struct status *, int);
void detach_object_connection(struct object_request *, off_t);

/* compress.c */

extern int decompressed_cache_size;

int get_file_by_term(struct terminal *term, struct cache_entry *ce, unsigned char **start, unsigned char **end, int *errp);
int get_file(struct object_request *o, unsigned char **start, unsigned char **end);
void free_decompressed_data(struct cache_entry *e);
void add_compress_methods(unsigned char **s, int *l);

/* session.c */

struct link_def {
	unsigned char *link;
	unsigned char *target;

	unsigned char *label;	/* only for image maps */
	unsigned char *shape;
	unsigned char *coords;

	unsigned char *onclick;
	unsigned char *ondblclick;
	unsigned char *onmousedown;
	unsigned char *onmouseup;
	unsigned char *onmouseover;
	unsigned char *onmouseout;
	unsigned char *onmousemove;
};

struct line {
	int l;
	int allocated;
	chr *d;
};

struct point {
	int x;
	int y;
};

struct form {
	unsigned char *action;
	unsigned char *target;
	unsigned char *form_name;
	unsigned char *onsubmit;
	int method;
	int num;
};

#define FM_GET		0
#define FM_POST		1
#define FM_POST_MP	2

#define FC_TEXT		1
#define FC_PASSWORD	2
#define FC_FILE		3
#define FC_TEXTAREA	4
#define FC_CHECKBOX	5
#define FC_RADIO	6
#define FC_SELECT	7
#define FC_SUBMIT	8
#define FC_IMAGE	9
#define FC_RESET	10
#define FC_HIDDEN	11
#define FC_BUTTON	12

struct menu_item;

struct form_control {
	list_entry_1st
	int form_num;	/* cislo formulare */
	int ctrl_num;	/* identifikace polozky v ramci formulare */
	int g_ctrl_num;	/* identifikace polozky mezi vsemi polozkami (poradi v poli form_info) */
	int position;
	int method;
	unsigned char *action;
	unsigned char *target;
	unsigned char *onsubmit; /* script to be executed on submit */
	int type;
	unsigned char *name;
	unsigned char *form_name;
	unsigned char *alt;
	int ro;
	unsigned char *default_value;
	int default_state;
	int size;
	int cols, rows, wrap;
	int maxlength;
	int nvalues; /* number of values in a select item */
	unsigned char **values; /* values of a select item */
	unsigned char **labels; /* labels (shown text) of a select item */
	struct menu_item *menu;
	list_entry_last
};

struct line_info {
	int st_offs;
	int en_offs;
	int chars;
};

struct format_text_cache_entry {
	int width;
	int wrap;
	int cp;
	int last_state;
	int last_vpos;
	int last_vypos;
	int last_cursor;
	int n_lines;
	struct line_info ln[1];
};

struct form_state {
	int form_num;	/* cislo formulare */
	int ctrl_num;	/* identifikace polozky v ramci formulare */
	int g_ctrl_num;	/* identifikace polozky mezi vsemi polozkami (poradi v poli form_info) */
	int position;
	int type;
	unsigned char *string; /* selected value of a select item */
	int state; /* index of selected item of a select item */
	int vpos;
	int vypos;
	struct format_text_cache_entry *ftce;
};

struct link {
	int type;   /* one of L_XXX constants */
	int num;    /* link number (used when user turns on link numbering) */
	unsigned char *where;   /* URL of the link */
	unsigned char *target;   /* name of target frame where to open the link */
	unsigned char *where_img;   /* URL of image (if any) */
	unsigned char *img_alt;		/* alt of image (if any) - valid only when link is an image */
	struct form_control *form;   /* form info, usually NULL */
	unsigned sel_color;   /* link color */
	int n;   /* number of points */
	int first_point_to_move;
	struct point *pos;
	int obj_order;
#ifdef G
	struct rect r;
	struct g_object *obj;
#endif
};

#define L_LINK		0
#define L_BUTTON	1
#define L_CHECKBOX	2
#define L_SELECT	3
#define L_FIELD		4
#define L_AREA		5

struct link_bg {
	int x, y;
	unsigned char c;
};

struct tag {
	list_entry_1st
	int x;
	int y;
	list_entry_last
	unsigned char name[1];
};

extern struct rgb palette_16_colors[16];

/* when you add anything, don't forget to initialize it in default.c on line:
 * struct document_setup dds = { ... };
 */
struct document_setup {
	int assume_cp;
	int hard_assume;
	int tables;
	int frames;
	int break_long_lines;
	int images;
	int image_names;
	int margin;
	int num_links;
	int table_order;
	int auto_refresh;
	int font_size;
	int display_images;
	int image_scale;
	int porn_enable;
	int target_in_new_window;
	int t_text_color;
	int t_link_color;
	int t_background_color;
	int t_ignore_document_color;
	int g_text_color;
	int g_link_color;
	int g_background_color;
	int g_ignore_document_color;
};


/* IMPORTANT!!!!!
 * if you add anything, fix it in compare_opt and if you add it into
 * document_setup, fix it in ds2do too
 */

struct document_options {
	int xw, yw; /* size of window */
	int xp, yp; /* pos of window */
	int scrolling;
	int col, cp, assume_cp, hard_assume;
	int tables, frames, break_long_lines, images, image_names, margin;
	int plain;
	int num_links, table_order;
	int auto_refresh;
	struct rgb default_fg;
	struct rgb default_bg;
	struct rgb default_link;
	unsigned char *framename;
	int font_size;
	int display_images;
	int image_scale;
	int porn_enable;
	double bfu_aspect; /* 0.1 to 10.0, 1.0 default. >1 makes circle wider */
	int real_cp;	/* codepage of document. Does not really belong here. Must not be compared. Used only in get_attr_val */
};

static inline void color2rgb(struct rgb *rgb, int color)
{
	memset(rgb, 0, sizeof(struct rgb));
	rgb->r = (color >> 16) & 0xff;
	rgb->g = (color >> 8) & 0xff;
	rgb->b = color & 0xff;
}

static inline void ds2do(struct document_setup *ds, struct document_options *doo, int col)
{
	doo->assume_cp = ds->assume_cp;
	doo->hard_assume = ds->hard_assume;
	doo->tables = ds->tables;
	doo->frames = ds->frames;
	doo->break_long_lines = ds->break_long_lines;
	doo->images = ds->images;
	doo->image_names = ds->image_names;
	doo->margin = ds->margin;
	doo->num_links = ds->num_links;
	doo->table_order = ds->table_order;
	doo->auto_refresh = ds->auto_refresh;
	doo->font_size = ds->font_size;
	doo->display_images = ds->display_images;
	doo->image_scale = ds->image_scale;
	doo->porn_enable = ds->porn_enable;
	if (!F) {
		if (!col) {
			doo->default_fg = palette_16_colors[7];
			doo->default_bg = palette_16_colors[0];
			doo->default_link = palette_16_colors[15];
		} else {
			doo->default_fg = palette_16_colors[ds->t_text_color];
			doo->default_bg = palette_16_colors[ds->t_background_color];
			doo->default_link = palette_16_colors[ds->t_link_color];
		}
	}
#ifdef G
	else {
		color2rgb(&doo->default_fg, ds->g_text_color);
		color2rgb(&doo->default_bg, ds->g_background_color);
		color2rgb(&doo->default_link, ds->g_link_color);
	}
#endif
}

struct node {
	list_entry_1st
	int x, y;
	int xw, yw;
	list_entry_last
};

struct search {
	int idx;
	int x, y;
	unsigned short n;
	unsigned short co;
};

struct frameset_desc;

struct frame_desc {
	struct frameset_desc *subframe;
	unsigned char *name;
	unsigned char *url;
	int marginwidth;
	int marginheight;
	int line;
	int xw, yw;
	unsigned char scrolling;
};

struct frameset_desc {
	int n;			/* = x * y */
	int x, y;		/* velikost */
	int xp, yp;		/* pozice pri pridavani */
	struct frame_desc f[1];
};

struct f_data;

#ifdef G

#define SHAPE_DEFAULT	0
#define SHAPE_RECT	1
#define SHAPE_CIRCLE	2
#define SHAPE_POLY	3

struct map_area {
	int shape;
	int *coords;
	int ncoords;
	int link_num;
};

struct image_map {
	int n_areas;
	struct map_area area[1];
};

struct background {
	int sRGB; /* This is 3*8 bytes with sRGB_gamma (in sRGB space).
		     This is not rounded. */
	long color;
	tcount gamma_stamp;
};

struct f_data_c;

#define G_OBJ_ALIGN_SPECIAL	(INT_MAX - 2)
#define G_OBJ_ALIGN_MIDDLE	(INT_MAX - 2)
#define G_OBJ_ALIGN_TOP		(INT_MAX - 1)

struct g_object {
	/* public data --- must be same in all g_object* structures */
	void (*mouse_event)(struct f_data_c *, struct g_object *, int, int, int);
		/* pos is relative to object */
	void (*draw)(struct f_data_c *, struct g_object *, int, int);
		/* absolute pos on screen */
	void (*destruct)(struct g_object *);
	void (*get_list)(struct g_object *, void (*)(struct g_object *parent, struct g_object *child));
	int x, y, xw, yw;
	struct g_object *parent;
};

#define go_go_head_1st	struct g_object go;
#define go_head_last

struct g_object_text_image {
	go_go_head_1st
	int link_num;
	int link_order;
	struct image_map *map;
	int ismap;
	go_head_last
};

struct g_object_text {
	struct g_object_text_image goti;
	struct style *style;
	int srch_pos;
	unsigned char text[1];
};

struct g_object_line {
	go_go_head_1st
	struct background *bg;
	int n_entries;
	go_head_last
	struct g_object *entries[1];
};

struct g_object_area {
	go_go_head_1st
	struct background *bg;
	int n_lines;
	go_head_last
	struct g_object_line *lines[1];
};

struct table;

struct g_object_table {
	go_go_head_1st
	struct table *t;
	go_head_last
};

struct g_object_tag {
	go_go_head_1st
	go_head_last
	/* private data... */
	unsigned char name[1];
};

#define IM_PNG 0
#define IM_GIF 1
#define IM_XBM 2

#ifdef HAVE_JPEG
#define IM_JPG 3
#endif /* #ifdef HAVE_JPEG */

#define MEANING_DIMS	  0
#define MEANING_AUTOSCALE 1
struct cached_image {
	list_entry_1st
	int refcount;

	int background_color; /* nezaokrouhlene pozadi:
			       * sRGB, (r<<16)+(g<<8)+b */
	unsigned char *url;
	int wanted_xw, wanted_yw; /* This is what is written in the alt.
				     If some dimension is omitted, then
				     it's <0. This is what was requested
				     when the image was created. */
	int wanted_xyw_meaning; /* MEANING_DIMS or MEANING_AUTOSCALE.
				   The meaning of wanted_xw and wanted_yw. */
	int scale; /* How is the image scaled */
	unsigned aspect; /* What aspect ratio the image is for. But the
		       PNG aspect is ignored :( */

	int xww, yww; /* This is the resulting dimensions on the screen
			 measured in screen pixels. */

	int width, height; /* From image header.
			    * If the buffer is allocated,
			    * it is always allocated to width*height.
			    * If the buffer is NULL then width and height
			    * are garbage. We assume these dimensions
			    * are given in the meter space (not pixel space).
			    * Which is true for all images except aspect
			    * PNG, but we don't support aspect PNG yet.
			    */
	unsigned char image_type; /* IM_??? constant */
	unsigned char *buffer; /* Buffer with image data */
	unsigned char buffer_bytes_per_pixel; /* 3 or 4 or 6 or 8
				     * 3: RGB
				     * 4: RGBA
				     * 6: RRGGBB
				     * 8: RRGGBBAA
				     */
	float red_gamma, green_gamma, blue_gamma;
		/* data=light_from_monitor^[red|green|blue]_gamma.
		 * i. e. 0.45455 is here if the image is in sRGB
		 * makes sense only if buffer is !=NULL
		 */
	tcount gamma_stamp; /* Number that is increased every gamma change */
	struct bitmap bmp; /* Registered bitmap. bmp.x=-1 and bmp.y=-1
			    * if the bmp is not registered.
			    */
	unsigned char bmp_used;
	int last_length; /* length of cache entry at which last decoding was
			  * done. Makes sense only if reparse==0
			  */
	tcount last_count; /* Always valid. */
	tcount last_count2; /* Always valid. */
	void *decoder;	      /* Decoder unfinished work. If NULL, decoder
			       * has finished or has not yet started.
			       */
	int rows_added; /* 1 if some rows were added inside the decoder */
	unsigned char state; /* 0...3 or 8...15 */
	unsigned char strip_optimized; /* 0 no strip optimization
				1 strip-optimized (no buffer allocated permanently
				and bitmap is always allocated)
			      */
	unsigned char eof_hit;
	int *dregs; /* Only for stip-optimized cached images */
	unsigned short *gamma_table; /* When suitable and source is 8 bits per pixel,
				      * this is allocated to 256*3*sizeof(*gamma_table)
				      * = 1536 bytes and speeds up the gamma calculations
				      * tremendously */
	list_entry_last
};

struct additional_file;

struct g_object_image {
	struct g_object_text_image goti;
			  /* x,y: coordinates
			     xw, yw: width on the screen, or <0 if
			     not yet known. Already scaled. */
	/* For html parser. If xw or yw are zero, then entries
	       background_color
	       af
	       width
	       height
	       image_type
	       buffer
	       buffer_bytes_per_pixel
	       *_gamma
	       gamma_stamp
	       bmp
	       last_length
	       last_count2
	       decoder
	       rows_added
	       reparse
	are uninitialized and thus garbage
	*/

	/* End of compatibility with g_object_text */

	list_entry_1st

	struct cached_image *cimg;
	struct additional_file *af;

	long id;
	unsigned char *name;
	unsigned char *alt;
	int vspace, hspace, border;
	unsigned char *orig_src;
	unsigned char *src;
	int background; /* Remembered background from insert_image
			 * (g_part->root->bg->sRGB)
			 */
	int xyw_meaning;

	list_entry_last
};

void refresh_image(struct f_data_c *fd, struct g_object *img, uttime tm);

#endif

struct additional_file *request_additional_file(struct f_data *f, unsigned char *url);

/*
 * warning: if you add more additional file stuctures, you must
 * set RQ upcalls correctly
 */

struct additional_files {
	int refcount;
	struct list_head af;	/* struct additional_file */
};

struct additional_file {
	list_entry_1st
	struct object_request *rq;
	tcount use_tag;
	tcount use_tag2;
	int need_reparse;
	int unknown_image_size;
	list_entry_last
	unsigned char url[1];
};

#ifdef G
struct image_refresh {
	list_entry_1st
	struct g_object *img;
	uttime tim;
	uttime start;
	list_entry_last
};
#endif

struct f_data {
	list_entry_1st
	struct session *ses;
	struct f_data_c *fd;
	struct object_request *rq;
	tcount use_tag;
	struct additional_files *af;
	struct document_options opt;
	unsigned char *title;
	int cp, ass;
	int x, y; /* size of document */
	uttime time_to_get;
	uttime time_to_draw;
	struct frameset_desc *frame_desc;
	int frame_desc_link;	/* if != 0, do not free frame_desc because it is link */

	/* text only */
	int bg;
	struct line *data;
	struct link *links;
	int nlinks;
	struct link **lines1;
	struct link **lines2;
	struct list_head nodes;		/* struct node */
	struct search *search_pos;
	char_t *search_chr;
	int nsearch_chr;
	int nsearch_pos;
	int *slines1;
	int *slines2;

	struct list_head forms;		/* struct form_control */
	struct list_head tags;		/* struct tag */


	unsigned char *refresh;
	int refresh_seconds;

	int uncacheable;	/* cannot be cached - either created from source modified by document.write or modified by javascript */

	/* graphics only */
#ifdef G
	struct g_object *root;
	struct g_object *locked_on;

	unsigned char *srch_string;
	int srch_string_size;

	unsigned char *last_search;
	int *search_positions;
	int *search_lengths;
	int n_search_positions;
	int hlt_pos; /* index of first highlighted byte */
	int hlt_len; /* length of highlighted bytes; (hlt_pos+hlt_len) is index of last highlighted character */
	int start_highlight_x;
	int start_highlight_y;
	struct list_head images;	/* list of all images in this f_data */
	int n_images;	/* pocet obrazku (tim se obrazky taky identifikujou), po kazdem pridani obrazku se zvedne o 1 */

	struct list_head image_refresh;
#endif
	list_entry_last
};

struct view_state {
	int refcount;

	int view_pos;
	int view_posx;
	int orig_view_pos;
	int orig_view_posx;
	int current_link;	/* platny jen kdyz je <f_data->n_links */
	int orig_link;
	int frame_pos;
	int plain;
	struct form_state *form_info;
	int form_info_len;
	int brl_x;
	int brl_y;
	int orig_brl_x;
	int orig_brl_y;
	int brl_in_field;
#ifdef G
	int g_display_link;
#endif
};

struct location;

struct f_data_c {
	list_entry_1st
	struct f_data_c *parent;
	struct session *ses;
	struct location *loc;
	struct view_state *vs;
	struct f_data *f_data;
	int xw, yw; /* size of window */
	int xp, yp; /* pos of window on screen */
	int xl, yl; /* last pos of view in window */

	int hsb, vsb;
	int hsbsize, vsbsize;

	struct link_bg *link_bg;
	int link_bg_n;
	int depth;

	struct object_request *rq;
	unsigned char *goto_position;
	unsigned char *went_to_position;
	struct additional_files *af;

	struct list_head subframes;	/* struct f_data_c */

	uttime last_update;
	uttime next_update_interval;
	int done;
	int parsed_done;
	int script_t;	/* offset of next script to execute */

	int active;	/* temporary, for draw_doc */

	int marginwidth, marginheight;

	struct timer *image_timer;

	struct timer *refresh_timer;

	unsigned char scrolling;
	unsigned char last_captured;

	list_entry_last
};

struct location {
	list_entry_1st
	struct location *parent;
	unsigned char *name;	/* frame name */
	unsigned char *url;
	unsigned char *prev_url;   /* allocated string with referrer */
	struct list_head subframes;	/* struct location */
	struct view_state *vs;
	unsigned location_id;
	list_entry_last
};

#define WTD_NO		0
#define WTD_FORWARD	1
#define WTD_IMGMAP	2
#define WTD_RELOAD	3
#define WTD_BACK	4

#define cur_loc(x)	list_struct((x)->history.next, struct location)

struct kbdprefix {
	int rep;
	int rep_num;
	int prefix;
};

struct download {
	list_entry_1st
	unsigned char *url;
	struct status stat;
	unsigned char decompress;
	unsigned char *cwd;
	unsigned char *orig_file;
	unsigned char *file;
	off_t last_pos;
	off_t file_shift;
	int handle;
	int redirect_cnt;
	int downloaded_something;
	unsigned char *prog;
	int prog_flag_block;
	time_t remotetime;
	struct session *ses;
	struct window *win;
	struct window *ask;
	list_entry_last
};

extern struct list_head downloads;

struct session {
	list_entry_1st
	struct list_head history;	/* struct location */
	struct list_head forward_history;
	struct terminal *term;
	struct window *win;
	int id;
	unsigned char *st;		/* status line string */
	unsigned char *st_old;		/* old status line --- compared with st to prevent cursor flicker */
	unsigned char *default_status;	/* default value of the status line */
	struct f_data_c *screen;
	struct object_request *rq;
	void (*wtd)(struct session *);
	unsigned char *wtd_target;
	struct f_data_c *wtd_target_base;
	unsigned char *wanted_framename;
	int wtd_refresh;
	int wtd_num_steps;
	unsigned char *goto_position;
	struct document_setup ds;
	struct kbdprefix kbdprefix;
	int reloadlevel;
	struct object_request *tq;
	unsigned char *tq_prog;
	int tq_prog_flag_block;
	int tq_prog_flag_direct;
	unsigned char *dn_url;
	int dn_allow_flags;
	unsigned char *search_word;
	unsigned char *last_search_word;
	int search_direction;
	int exit_query;
	struct list_head format_cache;	/* struct f_data */

	unsigned char *imgmap_href_base;
	unsigned char *imgmap_target_base;

	int brl_cursor_mode;

#ifdef G
	int locked_link;	/* for graphics - when link is locked on FIELD/AREA */
	int scrolling;
	int scrolltype;
	int scrolloff;

	int back_size;
#endif
	list_entry_last
};

struct dialog_data;

int get_file(struct object_request *o, unsigned char **start, unsigned char **end);

int f_is_finished(struct f_data *f);
unsigned long formatted_info(int);
void init_fcache(void);
void html_interpret_recursive(struct f_data_c *);
void fd_loaded(struct object_request *, void *);

extern struct list_head sessions;

time_t parse_http_date(unsigned char *);
unsigned char *encode_url(unsigned char *);
unsigned char *decode_url(unsigned char *);
struct session *get_download_ses(struct download *);
unsigned char *subst_file(unsigned char *, unsigned char *, int);
int are_there_downloads(void);
unsigned char *translate_download_file(unsigned char *);
void free_strerror_buf(void);
int get_error_from_errno(int errn);
unsigned char *get_err_msg(int);
void change_screen_status(struct session *);
void print_screen_status(struct session *);
void print_progress(struct session *, unsigned char *);
void print_error_dialog(struct session *, struct status *, unsigned char *);
void start_download(struct session *, unsigned char *, int);
int test_abort_downloads_to_file(unsigned char *, unsigned char *, int);
void abort_all_downloads(void);
unsigned char *download_percentage(struct download *down, int pad);
void download_window_function(struct dialog_data *dlg);
void display_download(struct terminal *, void *, void *);
struct f_data *cached_format_html(struct f_data_c *fd, struct object_request *rq, unsigned char *url, struct document_options *opt, int *cch, int report_status);
struct f_data_c *create_f_data_c(struct session *, struct f_data_c *);
void reinit_f_data_c(struct f_data_c *);
int f_data_c_allow_flags(struct f_data_c *fd);
#define CDF_RESTRICT_PERMISSION		1
#define CDF_EXCL			2
#define CDF_NOTRUNC			4
#define CDF_NO_POPUP_ON_EEXIST		8
int create_download_file(struct session *, unsigned char *, unsigned char *, int, off_t);
void *create_session_info(int, unsigned char *, unsigned char *, int *);
void win_func(struct window *, struct links_event *, int);
void goto_url_f(struct session *, void (*)(struct session *), unsigned char *, unsigned char *, struct f_data_c *, int, int, int, int);
void goto_url(void *, unsigned char *);
void goto_url_utf8(struct session *, unsigned char *);
void goto_url_not_from_dialog(struct session *, unsigned char *, struct f_data_c *);
void goto_imgmap(struct session *ses, struct f_data_c *fd, unsigned char *url, unsigned char *href, unsigned char *target);
void map_selected(struct terminal *term, void *ld, void *ses_);
void go_back(struct session *, int);
void go_backwards(struct terminal *term, void *psteps, void *ses_);
void reload(struct session *, int);
void cleanup_session(struct session *);
void destroy_session(struct session *);
void ses_destroy_defered_jump(struct session *ses);
struct f_data_c *find_frame(struct session *ses, unsigned char *target, struct f_data_c *base);


/* Information about the current document */
unsigned char *get_current_url(struct session *, unsigned char *, size_t);
unsigned char *get_current_title(struct f_data_c *, unsigned char *, size_t);

unsigned char *get_form_url(struct session *ses, struct f_data_c *f, struct form_control *form, int *onsubmit);

/* bfu.c */

extern struct style *bfu_style_wb, *bfu_style_bw, *bfu_style_wb_b, *bfu_style_bw_u, *bfu_style_bw_mono, *bfu_style_wb_mono, *bfu_style_wb_mono_u;
extern long bfu_bg_color, bfu_fg_color;

struct memory_list {
	size_t n;
	void *p[1];
};

struct memory_list *getml(void *, ...);
void add_to_ml(struct memory_list **, ...);
void freeml(struct memory_list *);

void init_bfu(void);
void shutdown_bfu(void);

#define DIALOG_LB	gf_val(DIALOG_LEFT_BORDER + DIALOG_LEFT_INNER_BORDER + 1, G_DIALOG_LEFT_BORDER + G_DIALOG_VLINE_SPACE + 1 + G_DIALOG_LEFT_INNER_BORDER)
#define DIALOG_TB	gf_val(DIALOG_TOP_BORDER + DIALOG_TOP_INNER_BORDER + 1, G_DIALOG_TOP_BORDER + G_DIALOG_HLINE_SPACE + 1 + G_DIALOG_TOP_INNER_BORDER)

extern unsigned char m_bar;

#define M_BAR	(&m_bar)

struct menu_item {
	unsigned char *text;
	unsigned char *rtext;
	unsigned char *hotkey;
	void (*func)(struct terminal *, void *, void *);
	void *data;
	int in_m;
	int free_i;
};

struct menu {
	int selected;
	int view;
	int nview;
	int xp, yp;
	int x, y, xw, yw;
	int ni;
	void *data;
	struct window *win;
	struct menu_item *items;
#ifdef G
	unsigned char **hktxt1;
	unsigned char **hktxt2;
	unsigned char **hktxt3;
	int xl1, yl1, xl2, yl2;
#endif
	void (*free_function)(void *);
	void *free_data;
	unsigned hotkeys[1];
};

struct mainmenu {
	int selected;
	int sp;
	int ni;
	void *data;
	struct window *win;
	struct menu_item *items;
#ifdef G
	int xl1, yl1, xl2, yl2;
#endif
	unsigned hotkeys[1];
};

struct history_item {
	list_entry_1st
	list_entry_last
	unsigned char str[1];
};

struct history {
	int n;
	struct list_head items;
};

#define free_history(h)		free_list(struct history_item, (h).items)

#define D_END		0
#define D_CHECKBOX	1
#define D_FIELD		2
#define D_FIELD_PASS	3
#define D_BUTTON	4

#define B_ENTER		1
#define B_ESC		2

struct dialog_item_data;

typedef void (*msg_button_fn)(void *);
typedef void (*input_field_button_fn)(void *, unsigned char *);

struct dialog_item {
	int type;
	int gid, gnum; /* for buttons: gid - flags B_XXX */	/* for fields: min/max */ /* for box: gid is box height */
	int (*fn)(struct dialog_data *, struct dialog_item_data *);
	struct history *history;
	int dlen;
	unsigned char *data;
	void *udata; /* for box: holds list */
	union {
		msg_button_fn msg_fn;
		input_field_button_fn input_fn;
	} u;
	unsigned char *text;
};

struct dialog_item_data {
	int x, y, l;
	int vpos, cpos;
	int checked;
	struct dialog_item *item;
	struct list_head history;
	struct list_head *cur_hist;
	unsigned char *cdata;
};

#define	EVENT_PROCESSED		0
#define EVENT_NOT_PROCESSED	1

struct dialog {
	unsigned char *title;
	void (*fn)(struct dialog_data *);
	int (*handle_event)(struct dialog_data *, struct links_event *);
	void (*abort)(struct dialog_data *);
	void *udata;
	void *udata2;
	int align;
	void (*refresh)(void *);
	void *refresh_data;
	struct dialog_item items[1];
};

struct dialog_data {
	struct window *win;
	struct dialog *dlg;
	int x, y, xw, yw;
	int n;
	int selected;
	struct memory_list *ml;
	int brl_y;
#ifdef G
	struct rect_set *s;
	struct rect r;
	struct rect rr;
#endif
	struct dialog_item_data items[1];
};

struct menu_item *new_menu(int);
void add_to_menu(struct menu_item **, unsigned char *, unsigned char *, unsigned char *, void (*)(struct terminal *, void *, void *), void *, int, int);
void do_menu(struct terminal *, struct menu_item *, void *);
void do_menu_selected(struct terminal *, struct menu_item *, void *, int, void (*)(void *), void *);
void do_mainmenu(struct terminal *, struct menu_item *, void *, int);
void do_dialog(struct terminal *, struct dialog *, struct memory_list *);
void dialog_func(struct window *, struct links_event *, int);
int check_number(struct dialog_data *, struct dialog_item_data *);
int check_hex_number(struct dialog_data *, struct dialog_item_data *);
int check_float(struct dialog_data *, struct dialog_item_data *);
int check_nonempty(struct dialog_data *, struct dialog_item_data *);
int check_local_ip_address(struct dialog_data *, struct dialog_item_data *);
int check_local_ipv6_address(struct dialog_data *, struct dialog_item_data *);
void max_text_width(struct terminal *, unsigned char *, int *, int);
void min_text_width(struct terminal *, unsigned char *, int *, int);
int dlg_format_text(struct dialog_data *, struct terminal *, unsigned char *, int, int *, int, int *, unsigned char, int);
void dlg_format_text_and_field(struct dialog_data *, struct terminal *, unsigned char *, struct dialog_item_data *, int, int *, int, int *, unsigned char, int);
void max_buttons_width(struct terminal *, struct dialog_item_data *, int, int *);
void min_buttons_width(struct terminal *, struct dialog_item_data *, int, int *);
void dlg_format_buttons(struct dialog_data *, struct terminal *, struct dialog_item_data *, int, int, int *, int, int *, int);
void checkboxes_width(struct terminal *, unsigned char * const *, int, int *, void (*)(struct terminal *, unsigned char *, int *, int));
void dlg_format_checkbox(struct dialog_data *, struct terminal *, struct dialog_item_data *, int, int *, int, int *, unsigned char *);
void dlg_format_checkboxes(struct dialog_data *, struct terminal *, struct dialog_item_data *, int, int, int *, int, int *, unsigned char * const *);
void dlg_format_field(struct dialog_data *, struct terminal *, struct dialog_item_data *, int, int *, int, int *, int);
void max_group_width(struct terminal *, unsigned char * const *, struct dialog_item_data *, int, int *);
void min_group_width(struct terminal *, unsigned char * const *, struct dialog_item_data *, int, int *);
void dlg_format_group(struct dialog_data *, struct terminal *, unsigned char * const *, struct dialog_item_data *, int, int, int *, int, int *);
/*void dlg_format_box(struct terminal *, struct terminal *, struct dialog_item_data *, int, int *, int, int *, int);*/
void checkbox_list_fn(struct dialog_data *);
void group_fn(struct dialog_data *);
void center_dlg(struct dialog_data *);
void draw_dlg(struct dialog_data *);
void display_dlg_item(struct dialog_data *, struct dialog_item_data *, int);
int check_dialog(struct dialog_data *);
void get_dialog_data(struct dialog_data *);
int ok_dialog(struct dialog_data *, struct dialog_item_data *);
int cancel_dialog(struct dialog_data *, struct dialog_item_data *);

void msg_box_fn(struct dialog_data *dlg);
void msg_box_null(void *);
#define MSG_BOX_END	((unsigned char *)NULL)
void msg_box(struct terminal *, struct memory_list *, unsigned char *, int, /*unsigned char *, void *, int,*/ ...);
/* msg_box arguments:
 *		terminal,
 *		blocks to free,
 *		title,
 *		alignment,
 *		strings (followed by MSG_BOX_END),
 *		data for function,
 *		number of buttons,
 *		button title, function, hotkey,
 *		... other buttons
 */

void input_field_null(void);
void input_field(struct terminal *, struct memory_list *, unsigned char *, unsigned char *, void *, struct history *, int, unsigned char *, int, int, int (*)(struct dialog_data *, struct dialog_item_data *), int n, ...);
/* input_field arguments:
 *		terminal,
 *		blocks to free,
 *		title,
 *		question,
 *		data for functions,
 *		history,
 *		length,
 *		string to fill the dialog with,
 *		minimal value,
 *		maximal value,
 *		check_function,
 *		the number of buttons,
 *		OK button text,
 *		ok function,
 *		CANCEL button text,
 *		cancel function,
 *
 *	field can have multiple buttons and functions, and finally NULL
 *	(warning: if there's no cancel function, there will be two NULLs in
 *	a call). Functions have type
 *	void (*fn)(void *data, unsigned char *text), only the last one has type
 *	void (*fn)(void *data). Check it carefully because the compiler wont!
 */
void add_to_history(struct terminal *, struct history *, unsigned char *);

int find_msg_box(struct terminal *term, unsigned char *title, int (*sel)(void *, void *), void *data);

/* menu.c */

extern struct history goto_url_history;

void activate_keys(struct session *ses);
void reset_settings_for_tor(void);
int save_proxy(int charset, unsigned char *result, unsigned char *proxy);
int save_noproxy_list(int charset, unsigned char *result, unsigned char *noproxy_list);
void dialog_html_options(struct session *ses);
void activate_bfu_technology(struct session *, int);
void dialog_goto_url(struct session *ses, unsigned char *url);
void dialog_save_url(struct session *ses);
void free_history_lists(void);
void query_file(struct session *, unsigned char *, unsigned char *, void (*)(struct session *, unsigned char *, int), void (*)(void *), int);
#define DOWNLOAD_DEFAULT	0
#define DOWNLOAD_OVERWRITE	1
#define DOWNLOAD_CONTINUE	2
void search_dlg(struct session *, struct f_data_c *, int);
void search_back_dlg(struct session *, struct f_data_c *, int);
void exit_prog(struct terminal *, void *, void *);
void really_exit_prog(void *ses_);
void query_exit(struct session *ses);

/* charsets.c */

struct conv_table {
	int t;
	union {
		unsigned char *str;
		struct conv_table *tbl;
	} u;
};

struct conv_table *get_translation_table(const int, const int);
int get_entity_number(unsigned char *st, int l);
unsigned char *get_entity_string(unsigned char *, int, int);
unsigned char *convert_string(struct conv_table *, unsigned char *, int, struct document_options *);
unsigned char *convert(int from, int to, unsigned char *c, struct document_options *dopt);
int get_cp_index(const unsigned char *);
unsigned char *get_cp_name(int);
unsigned char *get_cp_mime_name(int);
void free_conv_table(void);
unsigned char *encode_utf_8(int);
unsigned char *u2cp(int u, int to, int fallback);
int cp2u(unsigned, int);

unsigned uni_locase(unsigned);
unsigned charset_upcase(unsigned, int);
void charset_upcase_string(unsigned char **, int);
unsigned char *unicode_upcase_string(unsigned char *ch);
unsigned char *to_utf8_upcase(unsigned char *str, int cp);
int compare_case_utf8(unsigned char *u1, unsigned char *u2);

unsigned get_utf_8(unsigned char **p);
#define GET_UTF_8(s, c)							\
do {									\
	if ((unsigned char)(s)[0] < 0x80)				\
		(c) = (s)++[0];						\
	else if ((unsigned char)(s)[0] >= 0xc2 && (unsigned char)(s)[0] < 0xe0 &&\
	         ((unsigned char)(s)[1] & 0xc0) == 0x80) {		\
		(c) = (unsigned char)(s)[0] * 0x40 + (unsigned char)(s)[1], (c) -= 0x3080, (s) += 2;\
	} else								\
		(c) = get_utf_8(&(s));					\
} while (0)
#define FWD_UTF_8(s)							\
do {									\
	if ((unsigned char)(s)[0] < 0x80)				\
		(s)++;							\
	else								\
		get_utf_8(&(s));					\
} while (0)
#define BACK_UTF_8(p, b)						\
do {									\
	while ((p) > (b)) {						\
		(p)--;							\
		if ((*(p) & 0xc0) != 0x80)				\
			break;						\
	}								\
} while (0)

extern unsigned char utf_8_1[256];

static inline int utf8chrlen(unsigned char c)
{
	unsigned char l = utf_8_1[c];
	if (l == 7) return 1;
	return 7 - l;
}

static inline unsigned GET_TERM_CHAR(struct terminal *term, unsigned char **str)
{
	unsigned ch;
	if (!term_charset(term))
		GET_UTF_8(*str, ch);
	else
		ch = *(*str)++;
	return ch;
}

/* view.c */

unsigned char *textptr_add(unsigned char *t, int i, int cp);
int textptr_diff(unsigned char *t2, unsigned char *t1, int cp);

extern int ismap_link, ismap_x, ismap_y;

void frm_download(struct session *, struct f_data_c *);
void frm_download_image(struct session *, struct f_data_c *);
void frm_view_image(struct session *, struct f_data_c *);
struct format_text_cache_entry *format_text(struct f_data_c *fd, struct form_control *fc, struct form_state *fs);
int area_cursor(struct f_data_c *f, struct form_control *fc, struct form_state *fs);
struct form_state *find_form_state(struct f_data_c *, struct form_control *);
void fixup_select_state(struct form_control *fc, struct form_state *fs);
int enter(struct session *ses, struct f_data_c *f, int a);
int field_op(struct session *ses, struct f_data_c *f, struct link *l, struct links_event *ev);

int can_open_in_new(struct terminal *);
void open_in_new_window(struct terminal *, void *fn_, void *ses_);
extern void (* const send_open_new_xterm_ptr)(struct terminal *, void *fn_, void *ses_);
void destroy_fc(struct form_control *);
void sort_links(struct f_data *);
struct view_state *create_vs(void);
void destroy_vs(struct view_state *);
int dump_to_file(struct f_data *, int);
void check_vs(struct f_data_c *);
void draw_doc(struct terminal *t, void *scr_);
void draw_formatted(struct session *);
void draw_fd(struct f_data_c *);
void next_frame(struct session *, int);
void send_event(struct session *, struct links_event *);
void link_menu(struct terminal *, void *, void *);
void save_as(struct terminal *, void *, void *);
void save_url(void *, unsigned char *);
void menu_save_formatted(struct terminal *, void *, void *);
void copy_url_location(struct terminal *, void *, void *);
void selected_item(struct terminal *, void *, void *);
void toggle(struct session *, struct f_data_c *, int);
void do_for_frame(struct session *, void (*)(struct session *, struct f_data_c *, int), int);
int get_current_state(struct session *);
unsigned char *print_current_link(struct session *);
unsigned char *print_current_title(struct session *);
void loc_msg(struct terminal *, struct location *, struct f_data_c *);
void state_msg(struct session *);
void head_msg(struct session *);
void search_for(void *, unsigned char *);
void search_for_back(void *, unsigned char *);
void find_next(struct session *, struct f_data_c *, int);
void find_next_back(struct session *, struct f_data_c *, int);
void set_frame(struct session *, struct f_data_c *, int);
struct f_data_c *current_frame(struct session *);
void reset_form(struct f_data_c *f, int form_num);
void set_textarea(struct session *, struct f_data_c *, int);

/* font_inc.c */

#ifdef G
extern struct letter letter_data[];
extern struct font font_table[];
extern int n_fonts; /* Number of fonts. font number 0 is system_font (it's
		     * images are in system_font/ directory) and is used
		     * for special purpose.
		     */
#endif

/* gif.c */

#ifdef G

void gif_destroy_decoder(struct cached_image *);
void gif_start(struct cached_image *goi);
void gif_restart(unsigned char *data, int length);

void xbm_start(struct cached_image *goi);
void xbm_restart(struct cached_image *goi, unsigned char *data, int length);

#endif

/* png.c */

#ifdef G

void png_start(struct cached_image *cimg);
void png_restart(struct cached_image *cimg, unsigned char *data, int length);
void png_destroy_decoder(struct cached_image *cimg);
void add_png_version(unsigned char **s, int *l);

#endif /* #ifdef G */

/* img.c */

#ifdef G

struct image_description {
	unsigned char *url;		/* url=completed url */
	int xsize, ysize;		/* -1 --- unknown size. Space:pixel
					   space of the screen */
	int link_num;
	int link_order;
	unsigned char *name;
	unsigned char *alt;
	unsigned char *src;		/* reflects the src attribute */
	int border, vspace, hspace;
	int align;
	int ismap;
	int insert_flag;		/* pokud je 1, ma se vlozit do seznamu obrazku ve f_data */

	unsigned char *usemap;
	unsigned autoscale_x, autoscale_y; /* Requested autoscale dimensions
					      (maximum allowed rectangle), 0,0
					      means turned off. 0,something or
					      something,0 not allowed. */
};

extern int end_callback_hit;
extern struct cached_image *global_cimg;

/* Below are internal functions shared with imgcache.c, gif.c, and xbm.c */
int header_dimensions_known(struct cached_image *cimg);
void img_end(struct cached_image *cimg);
void compute_background_8(unsigned char *rgb, struct cached_image *cimg);
void buffer_to_bitmap_incremental(struct cached_image *cimg
	,unsigned char *buffer, int height, int yoff, int *dregs, int use_strip);

/* Below is external interface provided by img.c */
struct g_part;
int get_foreground(int rgb);
struct g_object_image *insert_image(struct g_part *p, struct image_description *im);
void change_image (struct g_object_image *goi, unsigned char *url, unsigned char *src, struct f_data *fdata);
void img_destruct_cached_image(struct cached_image *img);

#endif

/* jpeg.c */

#if defined(G) && defined(HAVE_JPEG)

/* Functions exported by jpeg.c for higher layers */
void jpeg_start(struct cached_image *cimg);
void jpeg_restart(struct cached_image *cimg, unsigned char *data, int length);
void jpeg_destroy_decoder(struct cached_image *cimg);
void add_jpeg_version(unsigned char **s, int *l);

#endif /* #if defined(G) && defined(HAVE_JPEG) */

int known_image_type(unsigned char *type);

/* imgcache.c */

#ifdef G

void init_imgcache(void);
int imgcache_info(int type);
struct cached_image *find_cached_image(int bg, unsigned char *url, int xw, int
		yw, int xyw_meaning, int scale, unsigned aspect);
void add_image_to_cache(struct cached_image *ci);

#endif

/* view_gr.c */

#ifdef G

/* intersection of 2 intervals s=start, l=len (len 0 is empty interval) */
static inline void intersect(int s1, int l1, int s2, int l2, int *s3, int *l3)
{
	int e1 = s1 + l1;
	int e2 = s2 + l2;
	int e3;

	if (e1 < s1) { int tmp = s1; s1 = e1; e1 = tmp; }
	if (e2 < s2) { int tmp = s2; s2 = e2; e2 = tmp; }

	if (!l1 || !l2)
		goto intersect_empty;

	if (s1 <= s2 && s2 <= e1)
		*s3 = s2;
	else if (s2 < s1)
		*s3 = s1;
	else
		goto intersect_empty;

	if (s1 <= e2 && e2 <= e1)
		e3 = e2;
	else if (e2 > e1)
		e3 = e1;
	else
		goto intersect_empty;

	*l3 = e3 - *s3;
	return;

	intersect_empty:
	*s3 = 0;
	*l3 = 0;
}


int g_forward_mouse(struct f_data_c *fd, struct g_object *a, int x, int y, int b);

void draw_vscroll_bar(struct graphics_device *dev, int x, int y, int yw, int total, int view, int pos);
void draw_hscroll_bar(struct graphics_device *dev, int x, int y, int xw, int total, int view, int pos);
void get_scrollbar_pos(int dsize, int total, int vsize, int vpos, int *start, int *end);


void get_parents(struct f_data *f, struct g_object *a);

void g_dummy_mouse(struct f_data_c *, struct g_object *, int, int, int);
void g_text_mouse(struct f_data_c *, struct g_object *, int, int, int);
void g_line_mouse(struct f_data_c *, struct g_object *, int, int, int);
void g_area_mouse(struct f_data_c *, struct g_object *, int, int, int);

void g_dummy_draw(struct f_data_c *, struct g_object *, int, int);
void g_text_draw(struct f_data_c *, struct g_object *, int, int);
void g_line_draw(struct f_data_c *, struct g_object *, int, int);
void g_area_draw(struct f_data_c *, struct g_object *, int, int);

void g_tag_destruct(struct g_object *);
void g_text_destruct(struct g_object *);
void g_line_destruct(struct g_object *);
void g_line_bg_destruct(struct g_object *);
void g_area_destruct(struct g_object *);

void g_line_get_list(struct g_object *, void (*)(struct g_object *parent, struct g_object *child));
void g_area_get_list(struct g_object *, void (*)(struct g_object *parent, struct g_object *child));

void draw_one_object(struct f_data_c *fd, struct g_object *o);
void draw_title(struct f_data_c *f);
void draw_graphical_doc(struct terminal *t, struct f_data_c *scr, int active);
int g_next_link(struct f_data_c *fd, int dir, int do_scroll);
int g_frame_ev(struct session *ses, struct f_data_c *fd, struct links_event *ev);
void g_find_next(struct f_data_c *f, int);

int is_link_in_view(struct f_data_c *fd, int nl);

void init_grview(void);

#endif

/* html.c */

#define AT_BOLD		1
#define AT_ITALIC	2
#define AT_UNDERLINE	4
#define AT_FIXED	8
#define AT_GRAPHICS	16
#define AT_INVERT	32

#define AL_LEFT		0
#define AL_CENTER	1
#define AL_RIGHT	2
#define AL_BLOCK	3
#define AL_NO		4
#define AL_NO_BREAKABLE	5
#define AL_BOTTOM	6
#define AL_MIDDLE	7
#define AL_TOP		8

#define AL_MASK		0x1f

#define AL_NOBRLEXP	0x20
#define AL_MONO		0x40

struct text_attrib_beginning {
	int attr;
	struct rgb fg;
	struct rgb bg;
	int fontsize;
	int baseline;
};

struct text_attrib {
	int attr;
	struct rgb fg;
	struct rgb bg;
	int fontsize;
	int baseline;
	unsigned char *fontface;
	unsigned char *link;
	unsigned char *target;
	unsigned char *image;
	struct form_control *form;
	struct rgb clink;
	unsigned char *href_base;
	unsigned char *target_base;
	unsigned char *select;
	int select_disabled;
};

#define P_NUMBER	1
#define P_alpha		2
#define P_ALPHA		3
#define P_roman		4
#define P_ROMAN		5
#define P_STAR		1
#define P_O		2
#define P_PLUS		3
#define P_LISTMASK	7
#define P_COMPACT	8

struct par_attrib {
	int align;
	int leftmargin;
	int rightmargin;
	int width;
	int list_level;
	unsigned list_number;
	int dd_margin;
	int flags;
	struct rgb bgcolor;
	int implicit_pre_wrap;
};

struct html_element {
	list_entry_1st
	struct text_attrib attr;
	struct par_attrib parattr;
#define INVISIBLE		1
#define INVISIBLE_SCRIPT	2
#define INVISIBLE_STYLE		3
	int invisible;
	unsigned char *name;
	int namelen;
	unsigned char *options;
	int linebreak;
	int dontkill;
	struct frameset_desc *frameset;
	list_entry_last
};

extern int get_attr_val_nl;

extern struct list_head html_stack;
extern int line_breax;

extern int html_format_changed;

extern unsigned char *startf;
extern unsigned char *eofff;

#define html_top_	list_struct(html_stack.next, struct html_element)
#define html_top	(*html_top_)
#define format_		(html_top.attr)
#define par_format	(html_top.parattr)

extern void *ff;
extern void (*put_chars_f)(void *, unsigned char *, int);
extern void (*line_break_f)(void *);
extern void *(*special_f)(void *, int, ...);

extern int table_level;
extern int empty_format;

extern struct form form;
extern unsigned char *last_form_tag;
extern unsigned char *last_form_attr;
extern unsigned char *last_input_tag;

extern unsigned char *last_link;
extern unsigned char *last_image;
extern unsigned char *last_target;
extern struct form_control *last_form;

int parse_element(unsigned char *, unsigned char *, unsigned char **, int *, unsigned char **, unsigned char **);
unsigned char *get_attr_val(unsigned char *, unsigned char *);
int has_attr(unsigned char *, unsigned char *);
int get_num(unsigned char *, unsigned char *);
int get_width(unsigned char *, unsigned char *, int);
int get_color(unsigned char *, unsigned char *, struct rgb *);
int get_bgcolor(unsigned char *, struct rgb *);
void html_stack_dup(void);
void kill_html_stack_item(struct html_element *);
int should_skip_script(unsigned char *);
unsigned char *skip_comment(unsigned char *, unsigned char *);
void parse_html(unsigned char *, unsigned char *, void (*)(void *, unsigned char *, int), void (*)(void *), void *(*)(void *, int, ...), void *, unsigned char *);
int get_image_map(unsigned char *, unsigned char *, unsigned char *, unsigned char *a, struct menu_item **, struct memory_list **, unsigned char *, unsigned char *, int, int, int, int gfx);
void scan_http_equiv(unsigned char *, unsigned char *, unsigned char **, int *, unsigned char **, unsigned char **, unsigned char **, int *);

int decode_color(unsigned char *, struct rgb *);

#define SP_TAG		0
#define SP_CONTROL	1
#define SP_TABLE	2
#define SP_USED		3
#define SP_FRAMESET	4
#define SP_FRAME	5
#define SP_SCRIPT	6
#define SP_IMAGE	7
#define SP_NOWRAP	8
#define SP_REFRESH	9
#define SP_SET_BASE	10
#define SP_HR		11

struct frameset_param {
	struct frameset_desc *parent;
	int x, y;
	int *xw, *yw;
};

#define SCROLLING_NO	0
#define SCROLLING_YES	1
#define SCROLLING_AUTO	2

struct frame_param {
	struct frameset_desc *parent;
	unsigned char *name;
	unsigned char *url;
	int marginwidth;
	int marginheight;
	unsigned char scrolling;
};

struct refresh_param {
	unsigned char *url;
	int time;
};

struct hr_param {
	int size;
	int width;
};

void free_menu(struct menu_item *);
void do_select_submenu(struct terminal *, void *, void *);

void clr_white(unsigned char *name);
void clr_spaces(unsigned char *name, int firstlast);

/* html_r.c */

extern int g_ctrl_num;

extern struct conv_table *convert_table;

struct part {
	int x, y;
	int xp, yp;
	int xmax;
	int xa;
	int cx, cy;
	struct f_data *data;
	int attribute;
	unsigned char *spaces;
	int z_spaces;
	int spl;
	int link_num;
	struct list_head uf;
	unsigned char utf8_part[7];
	unsigned char utf8_part_len;
};

#ifdef G
struct g_part {
	int x, y;
	int xmax;
	int cx, cy;
	int cx_w;
	struct g_object_area *root;
	struct g_object_line *line;
	struct g_object_text *text;
	int pending_text_len;
	struct wrap_struct w;
	struct style *current_style;
	struct f_data *data;
	int link_num;
	struct list_head uf;
};
#endif

struct sizes {
	int xmin, xmax, y;
};

extern struct f_data *current_f_data;

void free_additional_files(struct additional_files **);
void free_frameset_desc(struct frameset_desc *);
struct frameset_desc *copy_frameset_desc(struct frameset_desc *);

struct f_data *init_formatted(struct document_options *);
void destroy_formatted(struct f_data *);

/* d_opt je podle Mikulase nedefinovany mimo html parser, tak to jinde nepouzivejte
 *
 * -- Brain
 */
extern struct document_options dd_opt;
extern struct document_options *d_opt;
extern int margin;

int find_nearest_color(struct rgb *r, int l);
int fg_color(int fg, int bg);

void xxpand_line(struct part *, int, int);
void xxpand_lines(struct part *, int);
void xset_hchar(struct part *, int, int, unsigned, unsigned char);
void xset_hchars(struct part *, int, int, int, unsigned, unsigned char);
void html_tag(struct f_data *, unsigned char *, int, int);
void process_script(struct f_data *, unsigned char *);
void set_base(struct f_data *, unsigned char *);
void html_process_refresh(struct f_data *, unsigned char *, int );

int compare_opt(struct document_options *, struct document_options *);
void copy_opt(struct document_options *, struct document_options *);

struct link *new_link(struct f_data *);
struct conv_table *get_convert_table(unsigned char *, int, int, int *, int *, int);
struct part *format_html_part(unsigned char *, unsigned char *, int, int, int, struct f_data *, int, int, unsigned char *, int);
void really_format_html(struct cache_entry *, unsigned char *, unsigned char *, struct f_data *, int frame);
struct link *get_link_at_location(struct f_data *f, int x, int y);
int get_search_data(struct f_data *);

struct frameset_desc *create_frameset(struct f_data *fda, struct frameset_param *fp);
void create_frame(struct frame_param *fp);

/* html_gr.c */

#ifdef G

void release_image_map(struct image_map *map);
int is_in_area(struct map_area *a, int x, int y);

struct background *g_get_background(unsigned char *bg, unsigned char *bgcolor);
void g_release_background(struct background *bg);
void g_draw_background(struct graphics_device *dev, struct background *bg, int x, int y, int xw, int yw);

void g_x_extend_area(struct g_object_area *a, int width, int height, int align);
struct g_part *g_format_html_part(unsigned char *, unsigned char *, int, int, int, unsigned char *, int, unsigned char *, unsigned char *, struct f_data *);
void g_release_part(struct g_part *);
int g_get_area_width(struct g_object_area *o);
void add_object(struct g_part *pp, struct g_object *o);
void add_object_to_line(struct g_part *pp, struct g_object_line **lp,
	struct g_object *go);
void flush_pending_text_to_line(struct g_part *p);
void flush_pending_line_to_obj(struct g_part *p, int minheight);

#endif

/* html_tbl.c */

unsigned char *skip_element(unsigned char *, unsigned char *, unsigned char *, int);
void format_table(unsigned char *, unsigned char *, unsigned char *, unsigned char **, void *);
void table_bg(struct text_attrib *ta, unsigned char bgstr[8]);

void *find_table_cache_entry(unsigned char *start, unsigned char *end, int align, int m, int width, int xs, int link_num);
void add_table_cache_entry(unsigned char *start, unsigned char *end, int align, int m, int width, int xs, int link_num, void *p);

/* default.c */

extern int ggr;
extern int force_g;
extern unsigned char ggr_drv[MAX_STR_LEN];
extern unsigned char ggr_mode[MAX_STR_LEN];
extern unsigned char ggr_display[MAX_STR_LEN];

extern unsigned char default_target[MAX_STR_LEN];

unsigned char *parse_options(int, unsigned char *[]);
void init_home(void);
unsigned char *read_config_file(unsigned char *);
int write_to_config_file(unsigned char *, unsigned char *, int);
void load_config(void);
void write_config(struct terminal *);
void write_html_config(struct terminal *);
void end_config(void);

void load_url_history(void);
void save_url_history(void);

struct driver_param {
	list_entry_1st
	int kbd_codepage;
	int palette_mode;
	unsigned char *param;
	unsigned char shell_term[MAX_STR_LEN];
	int nosave;
	list_entry_last
	unsigned char name[1];
};
		/* -if exec is NULL, shell_term is unused
		   -otherwise this string describes shell to be executed by the
		    exec function, the '%' char means string to be executed
		   -shell cannot be NULL
		   -if exec is !NULL and shell is empty, exec should use some
		    default shell (e.g. "xterm -e %")
		*/

struct driver_param *get_driver_param(unsigned char *);

extern int anonymous;

extern unsigned char system_name[];

extern unsigned char *links_home;
extern int first_use;

extern int disable_libevent;
extern int no_connect;
extern int base_session;
#define D_DUMP		1
#define D_SOURCE	2
extern int dmp;
extern int screen_width;
extern int dump_codepage;
extern int force_html;

extern int max_connections;
extern int max_connections_to_host;
extern int max_tries;
extern int receive_timeout;
extern int unrestartable_receive_timeout;
extern int timeout_multiple_addresses;
extern unsigned char bind_ip_address[16];
extern unsigned char bind_ipv6_address[INET6_ADDRSTRLEN];
extern int download_utime;

extern int max_format_cache_entries;
extern int memory_cache_size;
extern int image_cache_size;
extern int font_cache_size;
extern int aggressive_cache;

struct ipv6_options {
	int addr_preference;
};

#define ADDR_PREFERENCE_DEFAULT		0
#define ADDR_PREFERENCE_IPV4		1
#define ADDR_PREFERENCE_IPV6		2
#define ADDR_PREFERENCE_IPV4_ONLY	3
#define ADDR_PREFERENCE_IPV6_ONLY	4

extern struct ipv6_options ipv6_options;

struct proxies {
	unsigned char http_proxy[MAX_STR_LEN];
	unsigned char https_proxy[MAX_STR_LEN];
	unsigned char socks_proxy[MAX_STR_LEN];
	unsigned char dns_append[MAX_STR_LEN];
	unsigned char no_proxy[MAX_STR_LEN];
	int only_proxies;
};

extern struct proxies proxies;

#define SSL_ACCEPT_INVALID_CERTIFICATE	0
#define SSL_WARN_ON_INVALID_CERTIFICATE	1
#define SSL_REJECT_INVALID_CERTIFICATE	2

struct ssl_options {
	int certificates;
	int built_in_certificates;
	unsigned char client_cert_key[MAX_STR_LEN];
	unsigned char client_cert_crt[MAX_STR_LEN];
	unsigned char client_cert_password[MAX_STR_LEN];
};

extern struct ssl_options ssl_options;

struct http_header_options {
	int fake_firefox;
	unsigned char fake_useragent[MAX_STR_LEN];
	unsigned char extra_header[MAX_STR_LEN];
};

struct http_options {
	int http10;
	int allow_blacklist;
	int no_accept_charset;
	int no_compression;
	int retry_internal_errors;
	struct http_header_options header;
};

extern struct http_options http_options;

extern unsigned char download_dir[];

#define SCRUB_HEADERS	(proxies.only_proxies || http_options.header.fake_firefox)

extern double display_red_gamma,display_green_gamma,display_blue_gamma;
extern double user_gamma;
extern double bfu_aspect;
extern int dither_letters;
extern int dither_images;
extern int gamma_bits;
extern int overwrite_instead_of_scroll;

extern int menu_font_size;
extern unsigned G_BFU_FG_COLOR, G_BFU_BG_COLOR, G_SCROLL_BAR_AREA_COLOR, G_SCROLL_BAR_BAR_COLOR, G_SCROLL_BAR_FRAME_COLOR;

extern unsigned char bookmarks_file[MAX_STR_LEN];

extern int save_history;

extern struct document_setup dds;

/* regexp.c */

char *regexp_replace(char *, char *, char *);

/* listedit.c */

#define TITLE_EDIT 0
#define TITLE_ADD 1

struct list {
	list_entry_1st
	unsigned char type;
	/*
	 * bit 0: 0=item, 1=directory
	 * bit 1: directory is open (1)/closed (0); for item unused
	 * bit 2: 1=item is selected 0=item is not selected
	 */
	int depth;
	struct list *fotr;   /* ignored when list is flat */
	list_entry_last
};

#define list_next(l)	(verify_list_entry(&(l)->list_entry), list_struct((l)->list_entry.next, struct list))
#define list_prev(l)	(verify_list_entry(&(l)->list_entry), list_struct((l)->list_entry.prev, struct list))

#define list_head_1st	struct list head;
#define list_head_last

struct list_description {
	unsigned char type;  /* 0=flat, 1=tree */
	struct list *list;   /* head of the list */
	struct list *(*new_item)(void * /* data in internal format */);  /* creates new item, does NOT add to the list */
	void (*edit_item)(struct dialog_data *, struct list *, void (*)(struct dialog_data *, struct list *, struct list *, struct list_description *) /* ok function */, struct list * /* parameter for the ok_function */, unsigned char);  /* must call call delete_item on the item after all */
	void *(*default_value)(struct session *, unsigned char /* 0=item, 1=directory */);  /* called when add button is pressed, allocates memory, return value is passed to the new_item function, new_item fills the item with this data */
	void (*delete_item)(struct list *);  /* delete item, if next and prev are not NULL adjusts pointers in the list */
	void (*copy_item)(struct list * /* old */, struct list * /* new */);  /* gets 2 allocated items, copies all item data except pointers from first item to second one, old data (in second item) will be destroyed */
	unsigned char *(*type_item)(struct terminal *, struct list *, int /* 0=type whole item (e.g. when deleting item), 1=type only e.g title (in list window )*/);   /* alllocates buffer and writes item into it */
	struct list *(*find_item)(struct list *start_item, unsigned char *string, int direction /* 1 or -1 */); /* returns pointer to the first item matching given string or NULL if failed. Search starts at start_item including. */
	struct history *search_history;
	int codepage;	/* codepage of all string */
	int n_items;   /* number of items in main window */

	/* following items are string codes */
	int item_description;  /* e.g. "bookmark" or "extension" ... */
	int already_in_use;   /* e.g. "Bookmarks window is already open" */
	int window_title;   /* main window title */
	int delete_dialog_title;   /* e.g. "Delete bookmark dialog" */
	int button;  /* when there's no button button_fn is NULL */

	void (*button_fn)(struct session *, struct list *);  /* gets pointer to the item */
	void (*save)(struct session *);

	/* internal variables, should not be modified, initially set to 0 */
	struct list *current_pos;
	struct list *win_offset;
	int win_pos;
	int open;  /* 0=closed, 1=open */
	int modified; /* listedit reports 1 when the list was modified by user (and should be e.g. saved) */
	struct dialog_data *dlg;  /* current dialog, valid only when open==1 */
	unsigned char *search_word;
	int search_direction;
};

int test_list_window_in_use(struct list_description *ld, struct terminal *term);
int create_list_window(struct list_description *, struct list *, struct terminal *, struct session *);
void reinit_list_window(struct list_description *ld);	/* reinitializes list window */


/* types.c */

struct list;

struct assoc {
	list_head_1st
	unsigned char *label;
	unsigned char *ct;
	unsigned char *prog;
	int cons;
	int xwin;
	int block;
	int ask;
	int accept_http;
	int accept_ftp;
	int system;
	list_head_last
};

struct extension {
	list_head_1st
	unsigned char *ext;
	unsigned char *ct;
	list_head_last
};

struct protocol_program {
	list_entry_1st
	unsigned char *prog;
	int system;
	list_entry_last
};

extern struct list assoc;
extern struct list extensions;

unsigned char *get_compress_by_extension(char *, char *);
unsigned char *get_content_type_by_extension(unsigned char *url);
unsigned char *get_content_type(unsigned char *, unsigned char *);
unsigned char *get_content_encoding(unsigned char *head, unsigned char *url, int just_ce);
unsigned char *encoding_2_extension(unsigned char *);
struct assoc *get_type_assoc(struct terminal *term, unsigned char *, int *);
int is_html_type(unsigned char *ct);
unsigned char *get_filename_from_header(unsigned char *head);
unsigned char *get_filename_from_url(unsigned char *, unsigned char *, int);

void menu_assoc_manager(struct terminal *, void *, void *);
void update_assoc(struct assoc *);
void menu_ext_manager(struct terminal *, void *, void *);
void update_ext(struct extension *);
void update_prog(struct list_head *, unsigned char *, int);
unsigned char *get_prog(struct list_head *);
void create_initial_extensions(void);

void free_types(void);

/* bookmark.c */

/* Where all bookmarks are kept */
extern struct list bookmarks;

void finalize_bookmarks(void);   /* called, when exiting links */
void init_bookmarks(void);   /* called at start */
void reinit_bookmarks(struct session *ses, unsigned char *new_bookmarks_file);

/* Launches bookmark manager */
void menu_bookmark_manager(struct terminal *, void *, void *);
