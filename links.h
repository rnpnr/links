/* links.h
 * (c) 2002 Mikulas Patocka, Karel 'Clock' Kulhavy, Petr 'Brain' Kulhavy,
 *          Martin 'PerM' Pergel
 * This file is a part of the Links program, released under GPL.
 */

#define LINKS_COPYRIGHT                                                        \
	"(C) 1999 - 2019 Mikulas Patocka\n(C) 2000 - 2019 Petr Kulhavy, "      \
	"Karel Kulhavy, Martin Pergel"

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

#define longlong  long long
#define ulonglong unsigned long long

#define stringify_internal(arg) #arg
#define stringify(arg)          stringify_internal(arg)

#define array_elements(a) (sizeof(a) / sizeof(*a))

#ifdef HAVE_POINTER_COMPARISON_BUG
	#define DUMMY ((void *)1L)
#else
	#define DUMMY ((void *)-1L)
#endif

#define cast_const_char (const char *)
#define cast_char       (char *)
#define cast_uchar      (unsigned char *)

enum ret { RET_OK, RET_ERROR, RET_SYNTAX };

#define EINTRLOOPX(ret_, call_, x_)                                            \
	do {                                                                   \
		(ret_) = (call_);                                              \
	} while ((ret_) == (x_) && errno == EINTR)

#define EINTRLOOP(ret_, call_) EINTRLOOPX(ret_, call_, -1)

#define ENULLLOOP(ret_, call_)                                                 \
	do {                                                                   \
		errno = 0;                                                     \
		(ret_) = (call_);                                              \
	} while (!(ret_) && errno == EINTR)

#define MAX_STR_LEN 1024

#define BIN_SEARCH(entries, eq, ab, key, result)                               \
	{                                                                      \
		int s_ = 0, e_ = (entries)-1;                                  \
		(result) = -1;                                                 \
		while (s_ <= e_) {                                             \
			int m_ = (int)(((unsigned)s_ + (unsigned)e_) / 2);     \
			if (eq((m_), (key))) {                                 \
				(result) = m_;                                 \
				break;                                         \
			}                                                      \
			if (ab((m_), (key)))                                   \
				e_ = m_ - 1;                                   \
			else                                                   \
				s_ = m_ + 1;                                   \
		}                                                              \
	}

void die(const char *, ...);
void usage(void);
void *xmalloc(size_t);
void *xrealloc(void *, size_t);
#define internal   die
#define error      die
#define fatal_exit die

/* error.c */

#define overalloc_at(f, l)                                                     \
	do {                                                                   \
		fatal_exit(                                                    \
		    "ERROR: attempting to allocate too large block at %s:%d",  \
		    f, l);                                                     \
	} while (1) /* while (1) is not a typo --- it's here to allow the      \
	    compiler that doesn't know that fatal_exit doesn't return to do    \
	    better optimizations */

#define overalloc() overalloc_at(__FILE__, __LINE__)

#define overflow()                                                             \
	do {                                                                   \
		fatal_exit("ERROR: arithmetic overflow at %s:%d", __FILE__,    \
		           __LINE__);                                          \
	} while (1)

static inline int
test_int_overflow(int x, int y, int *result)
{
	int z = *result = x + y;
	return ~(x ^ y) & (x ^ z) & (int)(1U << (sizeof(unsigned int) * 8 - 1));
}

static inline int
safe_add_function(int x, int y, unsigned char *file, int line)
{
	int ret;
	if (test_int_overflow(x, y, &ret))
		fatal_exit("ERROR: arithmetic overflow at %s:%d: %d + %d", file,
		           line, (x), (y));
	return ret;
}

#define safe_add(x, y)                                                         \
	safe_add_function(x, y, (unsigned char *)__FILE__, __LINE__)

void *mem_calloc(size_t size);

unsigned char *memacpy(const unsigned char *src, size_t len);
unsigned char *stracpy(const unsigned char *src);

#define pr(code)                                                               \
	if (1) {                                                               \
		code;                                                          \
	} else

/* inline */

#define ALLOC_GR 0x040 /* must be power of 2 */

#define get_struct_(ptr, struc, entry)                                         \
	((struc *)((char *)(ptr)-offsetof(struc, entry)))
#define get_struct(ptr, struc, entry)                                          \
	((void)(&get_struct_(ptr, struc, entry)->entry == (ptr)),              \
	 get_struct_(ptr, struc, entry))

struct list_head {
	struct list_head *next;
	struct list_head *prev;
};

#define list_struct(ptr, struc) get_struct(ptr, struc, list_entry)
#define init_list(x)                                                           \
	do {                                                                   \
		(x).next = &(x);                                               \
		(x).prev = &(x);                                               \
	} while (0)
#define list_empty(x) ((x).next == &(x))
#define del_list_entry(x)                                                      \
	do {                                                                   \
		(x)->next->prev = (x)->prev;                                   \
		(x)->prev->next = (x)->next;                                   \
		(x)->prev = (x)->next = NULL;                                  \
	} while (0)
#define del_from_list(x) del_list_entry(&(x)->list_entry)
#define add_after_list_entry(p, x)                                             \
	do {                                                                   \
		(x)->next = (p)->next;                                         \
		(x)->prev = (p);                                               \
		(p)->next = (x);                                               \
		(x)->next->prev = (x);                                         \
	} while (0)
#define add_before_list_entry(p, x)                                            \
	do {                                                                   \
		(x)->prev = (p)->prev;                                         \
		(x)->next = (p);                                               \
		(p)->prev = (x);                                               \
		(x)->prev->next = (x);                                         \
	} while (0)
#define add_to_list(l, x)     add_after_list_entry(&(l), &(x)->list_entry)
#define add_to_list_end(l, x) add_before_list_entry(&(l), &(x)->list_entry)
#define add_after_pos(p, x)                                                    \
	add_after_list_entry(&(p)->list_entry, &(x)->list_entry)
#define add_before_pos(p, x)                                                   \
	add_before_list_entry(&(p)->list_entry, &(x)->list_entry)
#define fix_list_after_realloc(x)                                              \
	do {                                                                   \
		(x)->list_entry.prev->next = &(x)->list_entry;                 \
		(x)->list_entry.next->prev = &(x)->list_entry;                 \
	} while (0)
#define foreachfrom(struc, e, h, l, s)                                         \
	for ((h) = (s); (h) == &(l) ? 0 : ((e) = list_struct(h, struc), 1);    \
	     (h) = (h)->next)
#define foreach(struc, e, h, l) foreachfrom (struc, e, h, l, (l).next)
#define foreachbackfrom(struc, e, h, l, s)                                     \
	for ((h) = (s); (h) == &(l) ? 0 : ((e) = list_struct(h, struc), 1);    \
	     (h) = (h)->prev)
#define foreachback(struc, e, h, l) foreachbackfrom (struc, e, h, l, (l).prev)
#define free_list(struc, l)                                                    \
	do {                                                                   \
		while (!list_empty(l)) {                                       \
			struc *a__ = list_struct((l).next, struc);             \
			del_from_list(a__);                                    \
			free(a__);                                             \
		}                                                              \
	} while (0)

static inline int
list_size(struct list_head *l)
{
	struct list_head *e;
	int n = 0;
	for (e = l->next; e != l; e = e->next)
		n++;
	return n;
}

#define list_entry_1st   struct list_head list_entry
#define init_list_1st(x) { (x), (x) },

#define WHITECHAR(x)                                                           \
	((x) == 9 || (x) == 10 || (x) == 12 || (x) == 13 || (x) == ' ')
#define U(x) ((x) == '"' || (x) == '\'')

enum ci {
	CI_BYTES = 1,
	CI_FILES,
	CI_LOCKED,
	CI_LOADING,
	CI_TIMERS,
	CI_TRANSFER,
	CI_CONNECTING,
	CI_KEEP
};

/* string.c */

int cmpbeg(const unsigned char *str, const unsigned char *b);
int snprint(unsigned char *s, int n, int num);
int snzprint(unsigned char *s, int n, off_t num);
int xstrcmp(const unsigned char *s1, const unsigned char *s2);
void add_to_strn(unsigned char **s, unsigned char *a);
void extend_str(unsigned char **s, int n);

static inline unsigned char *
init_str()
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

void safe_strncpy(unsigned char *dst, const unsigned char *src,
                  size_t dst_size);
int casestrcmp(const unsigned char *s1, const unsigned char *s2);
int casecmp(const unsigned char *c1, const unsigned char *c2, size_t len);
int casestrstr(const unsigned char *h, const unsigned char *n);

/* os_dep.c */

typedef unsigned long long uttime;
typedef unsigned long long tcount;

extern int page_size;

struct terminal;

struct open_in_new {
	unsigned char *text;
	unsigned char *hk;
	int (*const *open_window_fn)(struct terminal *, unsigned char *,
	                             unsigned char *);
};

void close_fork_tty(void);
int is_screen(void);
int is_xterm(void);
void get_terminal_size(int *, int *);
void handle_terminal_resize(void (*)(int, int), int *x, int *y);
void unhandle_terminal_resize(void);
void set_nonblock(int);
int c_pipe(int[2]);
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
unsigned char *escape_path(char *);
void check_shell_security(unsigned char **);
void check_filename(unsigned char **);
int check_shell_url(unsigned char *);
void do_signal(int sig, void (*handler)(int));
uttime get_time(void);
uttime get_absolute_time(void);
void init_page_size(void);
void ignore_signals(void);
int os_get_system_name(unsigned char *buffer);
unsigned char *os_conv_to_external_path(unsigned char *, unsigned char *);
unsigned char *os_fixup_external_program(unsigned char *);
int exe(char *, int);
struct open_in_new *get_open_in_new(int);

void os_free_clipboard(void);

void os_detach_console(void);

/* memory.c */

enum mem_sh { SH_CHECK_QUOTA, SH_FREE_SOMETHING, SH_FREE_ALL };

#define ST_SOMETHING_FREED 1
#define ST_CACHE_EMPTY     2

#define MF_GPI 1

int shrink_memory(int);
void register_cache_upcall(int (*)(int), int, unsigned char *);
void free_all_caches(void);
int out_of_memory(void);

/* select.c */

#ifndef FD_SETSIZE
	#define FD_SETSIZE (sizeof(fd_set) * 8)
#endif

#define NBIT(p) (sizeof((p)->fds_bits[0]) * 8)

#ifndef FD_SET
	#define FD_SET(n, p)                                                   \
		((p)->fds_bits[(n) / NBIT(p)] |= (1 << ((n) % NBIT(p))))
#endif
#ifndef FD_CLR
	#define FD_CLR(n, p)                                                   \
		((p)->fds_bits[(n) / NBIT(p)] &= ~(1 << ((n) % NBIT(p))))
#endif
#ifndef FD_ISSET
	#define FD_ISSET(n, p)                                                 \
		((p)->fds_bits[(n) / NBIT(p)] & (1 << ((n) % NBIT(p))))
#endif
#ifndef FD_ZERO
	#define FD_ZERO(p) memset((void *)(p), 0, sizeof(*(p)))
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

#define H_READ  0
#define H_WRITE 1

void (*get_handler(int, int))(void *);
void *get_handler_data(int);
extern unsigned char *sh_file;
extern int sh_line;
void set_handlers_file_line(int, void (*)(void *), void (*)(void *), void *);
#define set_handlers(a, b, c, d)                                               \
	(sh_file = (unsigned char *)__FILE__, sh_line = __LINE__,              \
	 set_handlers_file_line(a, b, c, d))
void clear_events(int, int);
extern int signal_pipe[2];
void install_signal_handler(int, void (*)(void *), void *, int);
void interruptible_signal(int sig, int in);
void block_signals(int except1, int except2);
void unblock_signals(void);
void set_sigcld(void);

/* dns.c */

#define MAX_ADDRESSES 64

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

int numeric_ip_address(const char *name, char address[4]);
int numeric_ipv6_address(const char *name, char address[16],
                         unsigned *scope_id);
void rotate_addresses(struct lookup_result *);
int find_host(char *, struct lookup_result *, void **, void (*)(void *, int),
              void *);
int find_host_no_cache(char *, struct lookup_result *, void **,
                       void (*)(void *, int), void *);
void kill_dns_request(void **);
#if MAX_ADDRESSES > 1
void dns_set_priority(char *, struct host_address *, int);
#endif
void dns_clear_host(char *);
unsigned long dns_info(int type);
unsigned char *print_address(struct host_address *);
int ipv6_full_access(void);
void init_dns(void);

/* cache.c */

struct cache_entry {
	list_entry_1st;
	unsigned char *head;
	int http_code;
	unsigned char *redirect;
	off_t length;
	off_t max_length;
	int incomplete;
	int tgc;
	unsigned char *last_modified;
	time_t expire_time; /* 0 never, 1 always */
	off_t data_size;
	struct list_head frag; /* struct fragment */
	tcount count;
	tcount count2;
	int refcount;
	unsigned char *decompressed;
	size_t decompressed_len;
	unsigned char *ip_address;
	unsigned char *ssl_info;
	unsigned char *ssl_authority;
	unsigned char url[1];
};

struct fragment {
	list_entry_1st;
	off_t offset;
	off_t length;
	off_t real_length;
	unsigned char data[1];
};

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
	unsigned char *ca;
} links_ssl;

enum pri {
	PRI_MAIN,
	PRI_DOWNLOAD,
	PRI_FRAME,
	PRI_NEED_IMG,
	PRI_IMG,
	PRI_PRELOAD,
	PRI_CANCEL,
	N_PRI
};

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
	list_entry_1st;
	tcount count;
	unsigned char *url;
	unsigned char *prev_url; /* allocated string with referrer or NULL */
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
	int keepalive;
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
	char socks_proxy[MAX_STR_LEN];
	unsigned char dns_append[MAX_STR_LEN];
	struct lookup_state last_lookup_state;
	links_ssl *ssl;
	int no_ssl_session;
	int no_tls;
};

extern tcount netcfg_stamp;

extern struct list_head queue;

struct k_conn {
	list_entry_1st;
	void (*protocol)(struct connection *);
	unsigned char *host;
	int port;
	int conn;
	uttime timeout;
	uttime add_time;
	int protocol_data;
	links_ssl *ssl;
	struct lookup_state last_lookup_state;
};

extern struct list_head keepalive_connections;

enum nc { NC_ALWAYS_CACHE, NC_CACHE, NC_IF_MOD, NC_RELOAD, NC_PR_NO_CACHE };

enum ses {
	S_WAIT,
	S_DNS,
	S_CONN,
	S_CONN_ANOTHER,
	S_SOCKS_NEG,
	S_SSL_NEG,
	S_SENT,
	S_LOGIN,
	S_GETH,
	S_PROC,
	S_TRANS
};

enum ses_sig {
	S__OK = -2000000000,
	S_INTERRUPTED,
	S_INTERNAL,
	S_OUT_OF_MEM,
	S_NO_DNS,
	S_NO_PROXY_DNS,
	S_CANT_WRITE,
	S_CANT_READ,
	S_MODIFIED,
	S_BAD_URL,
	S_BAD_PROXY,
	S_TIMEOUT,
	S_RESTART,
	S_STATE,
	S_CYCLIC_REDIRECT,
	S_LARGE_FILE,
	S_SMB_NOT_ALLOWED,
	S_FILE_NOT_ALLOWED,
	S_NO_PROXY,

	S_HTTP_ERROR = -2000000100,
	S_HTTP_204,
	S_HTTPS_FWD_ERROR,
	S_INVALID_CERTIFICATE,
	S_DOWNGRADED_METHOD,
	S_INSECURE_CIPHER,

	S_FILE_TYPE = -2000000200,
	S_FILE_ERROR,

	S_SSL_ERROR = -2000000400,
	S_NO_SSL,

	S_BAD_SOCKS_VERSION = -2000000500,
	S_SOCKS_REJECTED,
	S_SOCKS_NO_IDENTD,
	S_SOCKS_BAD_USERID,
	S_SOCKS_UNKNOWN_ERROR,

	S_WAIT_REDIR,
	S_UNKNOWN_ERROR,
	S_MAX
};

struct status {
	list_entry_1st;
	struct connection *c;
	struct cache_entry *ce;
	int state;
	int prev_error;
	int pri;
	void (*end)(struct status *, void *);
	void *data;
	struct remaining_info *prg;
};

int is_noproxy_url(unsigned char *url);
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
void add_keepalive_socket(struct connection *c, uttime timeout,
                          int protocol_data);
int is_connection_restartable(struct connection *c);
int is_last_try(struct connection *c);
void retry_connection(struct connection *c);
void abort_connection(struct connection *c);
#define ALLOW_SMB  1
#define ALLOW_FILE 2
#define ALLOW_ALL  (ALLOW_SMB | ALLOW_FILE)
void load_url(unsigned char *, unsigned char *, struct status *, int, int, int,
              int, off_t);
void change_connection(struct status *, struct status *, int);
void detach_connection(struct status *, off_t, int, int);
void abort_all_connections(void);
int abort_background_connections(void);
int is_entry_used(struct cache_entry *);
void clear_connection_timeout(struct connection *);
void set_connection_timeout(struct connection *);
void set_connection_timeout_keepal(struct connection *);
void add_blacklist_entry(unsigned char *, int);
void del_blacklist_entry(unsigned char *, int);
int get_blacklist_flags(unsigned char *);
void free_blacklist(void);

enum bl {
	BL_HTTP10 = 0x001,
	BL_NO_ACCEPT_LANGUAGE,
	BL_NO_CHARSET,
	BL_NO_RANGE,
	BL_NO_COMPRESSION,
	BL_NO_BZIP2,
	BL_IGNORE_CERTIFICATE,
	BL_IGNORE_DOWNGRADE,
	BL_IGNORE_CIPHER,
	BL_AVOID_INSECURE
};

/* suffix.c */

int is_tld(unsigned char *name);
int allow_cookie_domain(unsigned char *server, unsigned char *domain);

/* url.c */

struct session;

#define POST_CHAR        1
#define POST_CHAR_STRING "\001"

static inline int
end_of_dir(unsigned char *url, unsigned char c)
{
	return c == POST_CHAR || c == '#'
	       || ((c == ';' || c == '?')
	           && (!url || !casecmp(url, (unsigned char *)"http", 4)));
}

int parse_url(unsigned char *, int *, unsigned char **, int *, unsigned char **,
              int *, unsigned char **, int *, unsigned char **, int *,
              unsigned char **, int *, unsigned char **);
unsigned char *get_protocol_name(unsigned char *);
unsigned char *get_host_name(unsigned char *);
unsigned char *get_keepalive_id(unsigned char *);
unsigned char *get_user_name(unsigned char *);
unsigned char *get_pass(unsigned char *);
int get_port(unsigned char *);
unsigned char *get_port_str(unsigned char *);
void (*get_protocol_handle(unsigned char *))(struct connection *);
void (*get_external_protocol_function(unsigned char *))(struct session *,
                                                        unsigned char *);
int url_bypasses_socks(unsigned char *);
unsigned char *get_url_data(unsigned char *);
int url_non_ascii(unsigned char *url);
unsigned char *join_urls(unsigned char *, unsigned char *);
unsigned char *translate_url(unsigned char *, unsigned char *);
unsigned char *extract_position(unsigned char *);
int url_not_saveable(unsigned char *);
void add_conv_str(unsigned char **s, int *l, unsigned char *b, int ll,
                  int encode_special);
void convert_file_charset(unsigned char **s, int *l, int start_l);
unsigned char *idn_encode_host(unsigned char *host, int len,
                               unsigned char *separator, int decode);
unsigned char *idn_encode_url(unsigned char *url, int decode);
unsigned char *display_url(struct terminal *term, unsigned char *url,
                           int warn_idn);
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
void make_connection(struct connection *, int, int *,
                     void (*)(struct connection *));
void retry_connect(struct connection *, int, int);
void continue_connection(struct connection *, int *,
                         void (*)(struct connection *));
void write_to_socket(struct connection *, int, unsigned char *, int,
                     void (*)(struct connection *));
struct read_buffer *alloc_read_buffer(void);
void read_from_socket(struct connection *, int, struct read_buffer *,
                      void (*)(struct connection *, struct read_buffer *));
void kill_buffer_data(struct read_buffer *, int);

/* cookies.c */

struct cookie {
	list_entry_1st;
	unsigned char *name, *value;
	unsigned char *server;
	unsigned char *path, *domain;
	time_t expires; /* zero means undefined */
	int secure;
};

struct c_domain {
	list_entry_1st;
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

unsigned char *get_auth_realm(unsigned char *url, unsigned char *head,
                              int proxy);
unsigned char *get_auth_string(unsigned char *url, int proxy);
void free_auth(void);
void add_auth(unsigned char *url, unsigned char *realm, unsigned char *user,
              unsigned char *password, int proxy);
int find_auth(unsigned char *url, unsigned char *realm);

/* http.c */

int get_http_code(unsigned char *head, int *code, int *version);
unsigned char *parse_http_header(unsigned char *, unsigned char *,
                                 unsigned char **);
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

SSL_SESSION *get_session_cache_entry(SSL_CTX *ctx, unsigned char *host,
                                     int port);
void retrieve_ssl_session(struct connection *c);
unsigned long session_info(int type);
void init_session_cache(void);

/* data.c */

void data_func(struct connection *);

/* file.c */

void file_func(struct connection *);

/* kbd.c */

enum bm {
	B_LEFT,
	B_MIDDLE,
	B_RIGHT,
	B_FOURTH,
	B_FIFTH,
	B_SIXTH,
	B_WHEELUP = 8,
	B_WHEELDOWN,
	B_WHEELUP1,
	B_WHEELDOWN1,
	B_WHEELLEFT,
	B_WHEELRIGHT,
	B_WHEELLEFT1,
	B_WHEELRIGHT1
};
#define BM_BUTT B_WHEELRIGHT1

#define BM_IS_WHEEL(b) ((b)&8)

#define BM_ACT 48
#define B_DOWN 0
#define B_UP   16
#define B_DRAG 32
#define B_MOVE 48

enum kbd {
	KBD_SHIFT = (0 << 1),
	KBD_CTRL = (0 << 2),
	KBD_ALT = (0 << 3),
	KBD_PASTING = (0 << 4),

	KBD_ENTER = -0x100,
	KBD_BS,
	KBD_TAB,
	KBD_ESC,
	KBD_LEFT,
	KBD_RIGHT,
	KBD_UP,
	KBD_DOWN,
	KBD_INS,
	KBD_DEL,
	KBD_HOME,
	KBD_END,
	KBD_PAGE_UP,
	KBD_PAGE_DOWN,
	KBD_MENU,
	KBD_STOP,

	KBD_F1 = -0x120,
	KBD_F2,
	KBD_F3,
	KBD_F4,
	KBD_F5,
	KBD_F6,
	KBD_F7,
	KBD_F8,
	KBD_F9,
	KBD_F10,
	KBD_F11,
	KBD_F12,

	KBD_UNDO = -0x140,
	KBD_REDO,
	KBD_FIND,
	KBD_HELP,
	KBD_COPY,
	KBD_PASTE,
	KBD_CUT,
	KBD_PROPS,
	KBD_FRONT,
	KBD_OPEN,
	KBD_BACK,
	KBD_FORWARD,
	KBD_RELOAD,
	KBD_BOOKMARKS,
	KBD_SELECT,

	KBD_CTRL_C = -0x200,
	KBD_CLOSE
};

#define KBD_ESCAPE_MENU(x) ((x) <= KBD_F1 && (x) > KBD_CTRL_C)

void handle_trm(int, void *, int);
void free_all_itrms(void);
void dispatch_special(unsigned char *);
void kbd_ctrl_c(void);
int is_blocked(void);

struct itrm;

extern unsigned char init_seq[];
extern unsigned char init_seq_x_mouse[];
extern unsigned char init_seq_tw_mouse[];
extern unsigned char term_seq[];
extern unsigned char term_seq_x_mouse[];
extern unsigned char term_seq_tw_mouse[];

struct rgb {
	unsigned char r, g, b; /* This is 3*8 bits with sRGB gamma (in sRGB
	                        * space) This is not rounded. */
	unsigned char pad;
};

/* terminal.c */

extern unsigned char frame_dumb[];

typedef unsigned char_t;

typedef struct {
	char_t ch : 24;
	unsigned char at;
} chr;

#define chr_has_padding (sizeof(chr) != 4)

struct links_event {
	int ev;
	int x;
	int y;
	long b;
};

enum ev { EV_INIT, EV_KBD, EV_MOUSE, EV_EXTRA, EV_REDRAW, EV_RESIZE, EV_ABORT };
#define EV_EXTRA_OPEN_URL EV_INIT

enum evh {
	EVH_NOT_PROCESSED,
	EVH_LINK_KEYDOWN_PROCESSED,
	EVH_LINK_KEYPRESS_PROCESSED,
	EVH_DOCUMENT_KEYDOWN_PROCESSED,
	EVH_DOCUMENT_KEYPRESS_PROCESSED
};

struct window {
	list_entry_1st;
	void (*handler)(struct window *, struct links_event *, int fwd);
	void *data;
	int xp, yp;
	struct terminal *term;
};

#define MAX_TERM_LEN 32 /* this must be multiple of 8! (alignment problems) */

#define MAX_CWD_LEN                                                            \
	4096 /* this must be multiple of 8! (alignment problems)               \
	      */

#define ENV_XWIN   1
#define ENV_SCREEN 2

struct term_spec;

struct terminal {
	list_entry_1st;
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
	unsigned char utf8_buffer[7];
	int utf8_paste_mode;
};

struct term_spec {
	list_entry_1st;
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
};

#define TERM_DUMB  0
#define TERM_VT100 1

#define ATTR_FRAME 0x80

extern struct list_head term_specs;
extern struct list_head terminals;

static inline int
term_charset(struct terminal *term)
{
	return 0;
}

int hard_write(int, const unsigned char *, int);
int hard_read(int, unsigned char *, int);
unsigned char *get_cwd(void);
void set_cwd(unsigned char *);
unsigned char get_attribute(int, int);
struct terminal *
init_term(int, int, void (*)(struct window *, struct links_event *, int));
struct term_spec *new_term_spec(unsigned char *);
void free_term_specs(void);
void destroy_terminal(void *);
void cls_redraw_all_terminals(void);
void redraw_below_window(struct window *);
void add_window(struct terminal *,
                void (*)(struct window *, struct links_event *, int), void *);
void delete_window(struct window *);
void delete_window_ev(struct window *, struct links_event *ev);
void set_window_ptr(struct window *, int, int);
void get_parent_ptr(struct window *, int *, int *);
void add_empty_window(struct terminal *, void (*)(void *), void *);
void draw_to_window(struct window *, void (*)(struct terminal *, void *),
                    void *);
void redraw_all_terminals(void);
void flush_terminal(struct terminal *);

/* text only */
void set_char(struct terminal *, int, int, unsigned, unsigned char);
const chr *get_char(struct terminal *, int, int);
void set_color(struct terminal *, int, int, unsigned char);
void set_only_char(struct terminal *, int, int, unsigned, unsigned char);
void set_line(struct terminal *, int, int, int, chr *);
void set_line_color(struct terminal *, int, int, int, unsigned char);
void fill_area(struct terminal *, int, int, int, int, unsigned, unsigned char);
void draw_frame(struct terminal *, int, int, int, int, unsigned char, int);
void print_text(struct terminal *, int, int, int, unsigned char *,
                unsigned char);
void set_cursor(struct terminal *, int, int, int, int);

void destroy_all_terminals(void);
void block_itrm(int);
int unblock_itrm(int);

#define TERM_FN_TITLE  1
#define TERM_FN_RESIZE 2

void exec_on_terminal(struct terminal *, unsigned char *, unsigned char *,
                      unsigned char);
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
extern char **g_argv;
extern int g_argc;

void sig_tstp(void *t);
void sig_cont(void *t);

void unhandle_terminal_signals(struct terminal *term);
int attach_terminal(void *, int);

/* objreq.c */

#define O_WAITING    0
#define O_LOADING    1
#define O_FAILED     -1
#define O_INCOMPLETE -2
#define O_OK         -3

struct object_request {
	list_entry_1st;
	int refcount;
	tcount count;
	tcount term;
	struct status stat;
	struct cache_entry *ce_internal;
	struct cache_entry *ce;
	unsigned char *orig_url;
	unsigned char *url;
	unsigned char *prev_url; /* allocated string with referrer or NULL */
	unsigned char *goto_position;
	int pri;
	int cache;
	void (*upcall)(struct object_request *, void *);
	void *data;
	int redirect_cnt;
	int state;
#define HOLD_AUTH 1
#define HOLD_CERT 2
	int hold;
	int dont_print_error;
	struct timer *timer;

	off_t last_bytes;

	uttime last_update;
};

void request_object(struct terminal *, unsigned char *, unsigned char *, int,
                    int, int, void (*)(struct object_request *, void *), void *,
                    struct object_request **);
void clone_object(struct object_request *, struct object_request **);
void release_object(struct object_request **);
void release_object_get_stat(struct object_request **, struct status *, int);
void detach_object_connection(struct object_request *, off_t);

/* compress.c */

extern int decompressed_cache_size;

int get_file_by_term(struct terminal *term, struct cache_entry *ce,
                     unsigned char **start, size_t *len, int *errp);
int get_file(struct object_request *o, unsigned char **start, size_t *len);
void free_decompressed_data(struct cache_entry *e);
void add_compress_methods(unsigned char **s, int *l);

/* session.c */

struct link_def {
	unsigned char *link;
	unsigned char *target;

	unsigned char *label; /* only for image maps */
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

enum fm { FM_GET, FM_POST, FM_POST_MP };

enum fc {
	FC_TEXT = 1,
	FC_PASSWORD,
	FC_FILE_UPLOAD,
	FC_TEXTAREA,
	FC_CHECKBOX,
	FC_RADIO,
	FC_SELECT,
	FC_SUBMIT,
	FC_IMAGE,
	FC_RESET,
	FC_HIDDEN,
	FC_BUTTON
};

struct menu_item;

struct form_control {
	list_entry_1st;
	int form_num;   /* cislo formulare */
	int ctrl_num;   /* identifikace polozky v ramci formulare */
	int g_ctrl_num; /* identifikace polozky mezi vsemi polozkami (poradi v
	                   poli form_info) */
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
	int nvalues;            /* number of values in a select item */
	unsigned char **values; /* values of a select item */
	unsigned char **labels; /* labels (shown text) of a select item */
	struct menu_item *menu;
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
	int form_num;   /* cislo formulare */
	int ctrl_num;   /* identifikace polozky v ramci formulare */
	int g_ctrl_num; /* identifikace polozky mezi vsemi polozkami (poradi v
	                   poli form_info) */
	int position;
	int type;
	unsigned char *string; /* selected value of a select item */
	int state;             /* index of selected item of a select item */
	int vpos;
	int vypos;
	struct format_text_cache_entry *ftce;
};

struct link {
	int type; /* one of L_XXX constants */
	int num;  /* link number (used when user turns on link numbering) */
	unsigned char *where;  /* URL of the link */
	unsigned char *target; /* name of target frame where to open the link */
	unsigned char *where_img; /* URL of image (if any) */
	unsigned char *img_alt; /* alt of image (if any) - valid only when link
	                           is an image */
	struct form_control *form; /* form info, usually NULL */
	unsigned sel_color;        /* link color */
	int n;                     /* number of points */
	int first_point_to_move;
	struct point *pos;
	int obj_order;
};

enum l_link { L_LINK, L_BUTTON, L_CHECKBOX, L_SELECT, L_FIELD, L_AREA };

struct link_bg {
	int x, y;
	unsigned char c;
};

struct tag {
	list_entry_1st;
	int x;
	int y;
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
	tcount gamma_stamp;
	int real_cp; /* codepage of document. Does not really belong here. Must
	                not be compared. Used only in get_attr_val */
};

static inline void
color2rgb(struct rgb *rgb, int color)
{
	memset(rgb, 0, sizeof(struct rgb));
	rgb->r = (color >> 16) & 0xff;
	rgb->g = (color >> 8) & 0xff;
	rgb->b = color & 0xff;
}

static inline void
ds2do(struct document_setup *ds, struct document_options *doo, int col)
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

struct node {
	list_entry_1st;
	int x, y;
	int xw, yw;
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
	int n;      /* = x * y */
	int x, y;   /* velikost */
	int xp, yp; /* pozice pri pridavani */
	struct frame_desc f[1];
};

struct f_data;

struct additional_file *request_additional_file(struct f_data *f,
                                                unsigned char *url);

/*
 * warning: if you add more additional file stuctures, you must
 * set RQ upcalls correctly
 */

struct additional_files {
	int refcount;
	struct list_head af; /* struct additional_file */
};

struct additional_file {
	list_entry_1st;
	struct object_request *rq;
	tcount use_tag;
	tcount use_tag2;
	int need_reparse;
	int unknown_image_size;
	unsigned char url[1];
};

struct f_data {
	list_entry_1st;
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
	int frame_desc_link; /* if != 0, do not free frame_desc because it is
	                        link */

	/* text only */
	int bg;
	struct line *data;
	struct link *links;
	int nlinks;
	struct link **lines1;
	struct link **lines2;
	struct list_head nodes; /* struct node */
	struct search *search_pos;
	char_t *search_chr;
	int nsearch_chr;
	int nsearch_pos;
	int *slines1;
	int *slines2;

	struct list_head forms; /* struct form_control */
	struct list_head tags;  /* struct tag */

	unsigned char *refresh;
	int refresh_seconds;

	int uncacheable; /* cannot be cached - either created from source
	                    modified by document.write or modified by javascript
	                  */
};

struct view_state {
	int refcount;

	int view_pos;
	int view_posx;
	int orig_view_pos;
	int orig_view_posx;
	int current_link; /* platny jen kdyz je <f_data->n_links */
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
};

struct location;

struct f_data_c {
	list_entry_1st;
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

	struct list_head subframes; /* struct f_data_c */

	uttime last_update;
	uttime next_update_interval;
	int done;
	int parsed_done;
	int script_t; /* offset of next script to execute */

	int active; /* temporary, for draw_doc */

	int marginwidth, marginheight;

	struct timer *image_timer;

	struct timer *refresh_timer;

	unsigned char scrolling;
	unsigned char last_captured;
};

struct location {
	list_entry_1st;
	struct location *parent;
	unsigned char *name; /* frame name */
	unsigned char *url;
	unsigned char *prev_url;    /* allocated string with referrer */
	struct list_head subframes; /* struct location */
	struct view_state *vs;
	unsigned location_id;
};

#define cur_loc(x) list_struct((x)->history.next, struct location)

struct kbdprefix {
	int rep;
	int rep_num;
	int prefix;
};

struct download {
	list_entry_1st;
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
};

extern struct list_head downloads;

struct session {
	list_entry_1st;
	struct list_head history; /* struct location */
	struct list_head forward_history;
	struct terminal *term;
	struct window *win;
	int id;
	unsigned char *st;     /* status line string */
	unsigned char *st_old; /* old status line --- compared with st to
	                          prevent cursor flicker */
	unsigned char *default_status; /* default value of the status line */
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
	struct list_head format_cache; /* struct f_data */

	unsigned char *imgmap_href_base;
	unsigned char *imgmap_target_base;

	int brl_cursor_mode;
};

struct dialog_data;

int f_is_finished(struct f_data *f);
unsigned long formatted_info(int);
int shrink_format_cache(int u);
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
unsigned char get_session_attribute(struct session *, int);
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
struct f_data *cached_format_html(struct f_data_c *fd,
                                  struct object_request *rq, unsigned char *url,
                                  struct document_options *opt, int *cch,
                                  int report_status);
struct f_data_c *create_f_data_c(struct session *, struct f_data_c *);
void reinit_f_data_c(struct f_data_c *);
int f_data_c_allow_flags(struct f_data_c *fd);
#define CDF_RESTRICT_PERMISSION 1
#define CDF_EXCL                2
#define CDF_NOTRUNC             4
#define CDF_NO_POPUP_ON_EEXIST  8
int create_download_file(struct session *, unsigned char *, unsigned char *,
                         int, off_t);
void *create_session_info(int, unsigned char *, unsigned char *, int *);
void win_func(struct window *, struct links_event *, int);
void goto_url_f(struct session *, void (*)(struct session *), unsigned char *,
                unsigned char *, struct f_data_c *, int, int, int, int);
void goto_url(void *, unsigned char *);
void goto_url_utf8(struct session *, unsigned char *);
void goto_url_not_from_dialog(struct session *, unsigned char *,
                              struct f_data_c *);
void goto_imgmap(struct session *ses, struct f_data_c *fd, unsigned char *url,
                 unsigned char *href, unsigned char *target);
void map_selected(struct terminal *term, void *ld, void *ses_);
void go_back(struct session *, int);
void go_backwards(struct terminal *term, void *psteps, void *ses_);
void reload(struct session *, int);
void cleanup_session(struct session *);
void destroy_session(struct session *);
struct f_data_c *find_frame(struct session *ses, unsigned char *target,
                            struct f_data_c *base);

/* Information about the current document */
unsigned char *get_current_url(struct session *, unsigned char *, size_t);
unsigned char *get_current_title(struct f_data_c *, unsigned char *, size_t);

unsigned char *get_form_url(struct session *ses, struct f_data_c *f,
                            struct form_control *form, int *onsubmit);

/* bfu.c */

extern struct style *bfu_style_wb, *bfu_style_bw, *bfu_style_wb_b,
    *bfu_style_bw_u, *bfu_style_bw_mono, *bfu_style_wb_mono,
    *bfu_style_wb_mono_u;
extern long bfu_bg_color, bfu_fg_color;

struct memory_list {
	size_t n;
	void *p[1];
};

struct memory_list *getml(void *, ...);
void add_to_ml(struct memory_list **, ...);
void freeml(struct memory_list *);

#define DIALOG_LB (DIALOG_LEFT_BORDER + DIALOG_LEFT_INNER_BORDER + 1)
#define DIALOG_TB (DIALOG_TOP_BORDER + DIALOG_TOP_INNER_BORDER + 1)

extern unsigned char m_bar;

#define M_BAR (&m_bar)

struct menu_item {
	unsigned char *text;
	unsigned char *rtext;
	unsigned char *hotkey;
	void (*func)(struct terminal *, void *, void *);
	void *data;
	int in_m;
	int free_i;
};

enum menu_items {
	MENU_FREE_ITEMS = 1,
	MENU_FREE_TEXT = 2,
	MENU_FREE_RTEXT = 4,
	MENU_FREE_HOTKEY = 8
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
	unsigned hotkeys[1];
};

struct history_item {
	list_entry_1st;
	unsigned char str[1];
};

struct history {
	int n;
	struct list_head items;
};

#define free_history(h) free_list(struct history_item, (h).items)

enum dlog { D_END, D_CHECKBOX, D_FIELD, D_FIELD_PASS, D_BUTTON };

enum bfu { B_ENTER = 1, B_ESC };

struct dialog_item_data;

typedef void (*msg_button_fn)(void *);
typedef void (*input_field_button_fn)(void *, unsigned char *);

struct dialog_item {
	int type;
	int gid, gnum;            /* for buttons: gid - flags B_XXX */
	/* for fields: min/max */ /* for box: gid is box height */
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

enum even { EVENT_PROCESSED, EVENT_NOT_PROCESSED };

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
	struct dialog_item_data items[1];
};

struct menu_item *new_menu(int);
void add_to_menu(struct menu_item **, unsigned char *, unsigned char *,
                 unsigned char *, void (*)(struct terminal *, void *, void *),
                 void *, int, int);
void do_menu(struct terminal *, struct menu_item *, void *);
void do_menu_selected(struct terminal *, struct menu_item *, void *, int,
                      void (*)(void *), void *);
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
int dlg_format_text(struct dialog_data *, struct terminal *, unsigned char *,
                    int, int *, int, int *, unsigned char, int);
void dlg_format_text_and_field(struct dialog_data *, struct terminal *,
                               unsigned char *, struct dialog_item_data *, int,
                               int *, int, int *, unsigned char, int);
void max_buttons_width(struct terminal *, struct dialog_item_data *, int,
                       int *);
void min_buttons_width(struct terminal *, struct dialog_item_data *, int,
                       int *);
void dlg_format_buttons(struct dialog_data *, struct terminal *,
                        struct dialog_item_data *, int, int, int *, int, int *,
                        int);
void checkboxes_width(struct terminal *, unsigned char *const *, int, int *,
                      void (*)(struct terminal *, unsigned char *, int *, int));
void dlg_format_checkbox(struct dialog_data *, struct terminal *,
                         struct dialog_item_data *, int, int *, int, int *,
                         unsigned char *);
void dlg_format_checkboxes(struct dialog_data *, struct terminal *,
                           struct dialog_item_data *, int, int, int *, int,
                           int *, unsigned char *const *);
void dlg_format_field(struct dialog_data *, struct terminal *,
                      struct dialog_item_data *, int, int *, int, int *, int);
void max_group_width(struct terminal *, unsigned char *const *,
                     struct dialog_item_data *, int, int *);
void min_group_width(struct terminal *, unsigned char *const *,
                     struct dialog_item_data *, int, int *);
void dlg_format_group(struct dialog_data *, struct terminal *,
                      unsigned char *const *, struct dialog_item_data *, int,
                      int, int *, int, int *);
/*void dlg_format_box(struct terminal *, struct terminal *, struct
 * dialog_item_data *, int, int *, int, int *, int);*/
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
#define MSG_BOX_END ((unsigned char *)NULL)
void msg_box(struct terminal *, struct memory_list *, unsigned char *, int,
             /*unsigned char *, void *, int,*/...);
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
void input_field(struct terminal *, struct memory_list *, unsigned char *,
                 unsigned char *, void *, struct history *, int,
                 unsigned char *, int, int,
                 int (*)(struct dialog_data *, struct dialog_item_data *),
                 int n, ...);
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

int find_msg_box(struct terminal *term, unsigned char *title,
                 int (*sel)(void *, void *), void *data);

/* menu.c */

extern struct history goto_url_history;

void activate_keys(struct session *ses);
void reset_settings_for_tor(void);
int save_proxy(int charset, unsigned char *result, unsigned char *proxy);
int save_noproxy_list(int charset, unsigned char *result,
                      unsigned char *noproxy_list);
void dialog_html_options(struct session *ses);
void activate_bfu_technology(struct session *, int);
void dialog_goto_url(struct session *ses, unsigned char *url);
void dialog_save_url(struct session *ses);
void free_history_lists(void);
void query_file(struct session *, unsigned char *, unsigned char *,
                void (*)(struct session *, unsigned char *, int),
                void (*)(void *), int);
#define DOWNLOAD_DEFAULT   0
#define DOWNLOAD_OVERWRITE 1
#define DOWNLOAD_CONTINUE  2
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

#define is_entity_terminator(c)                                                \
	(c <= ' ' || c == ';' || c == '&' || c == '/' || c == '?')

unsigned int locase(unsigned int a);
unsigned int upcase(unsigned int a);
int get_entity_number(unsigned char *st, int l);
unsigned char *get_entity_string(unsigned char *, int);
unsigned char *convert_string(struct conv_table *, unsigned char *, int,
                              struct document_options *);
unsigned char *convert(int from, int to, unsigned char *c,
                       struct document_options *dopt);
unsigned char *get_cp_name(int);
unsigned char *get_cp_mime_name(int);
void free_conv_table(void);
unsigned char *encode_utf_8(int);
unsigned char *u2cp(int u);

unsigned uni_locase(unsigned);
unsigned charset_upcase(unsigned, int);
void charset_upcase_string(unsigned char **, int);
unsigned char *unicode_upcase_string(unsigned char *ch);
unsigned char *to_utf8_upcase(unsigned char *str, int cp);
int compare_case_utf8(unsigned char *u1, unsigned char *u2);

unsigned get_utf_8(unsigned char **p);
#define GET_UTF_8(s, c)                                                        \
	do {                                                                   \
		if ((unsigned char)(s)[0] < 0x80)                              \
			(c) = (s)++[0];                                        \
		else if ((unsigned char)(s)[0] >= 0xc2                         \
		         && (unsigned char)(s)[0] < 0xe0                       \
		         && ((unsigned char)(s)[1] & 0xc0) == 0x80) {          \
			(c) = (unsigned char)(s)[0] * 0x40                     \
			      + (unsigned char)(s)[1],                         \
			(c) -= 0x3080, (s) += 2;                               \
		} else                                                         \
			(c) = get_utf_8(&(s));                                 \
	} while (0)
#define FWD_UTF_8(s)                                                           \
	do {                                                                   \
		if ((unsigned char)(s)[0] < 0x80)                              \
			(s)++;                                                 \
		else                                                           \
			get_utf_8(&(s));                                       \
	} while (0)
#define BACK_UTF_8(p, b)                                                       \
	do {                                                                   \
		while ((p) > (b)) {                                            \
			(p)--;                                                 \
			if ((*(p)&0xc0) != 0x80)                               \
				break;                                         \
		}                                                              \
	} while (0)

extern unsigned char utf_8_1[256];

static inline int
utf8chrlen(unsigned char c)
{
	unsigned char l = utf_8_1[c];
	if (l == 7)
		return 1;
	return 7 - l;
}

static inline unsigned
GET_TERM_CHAR(struct terminal *term, unsigned char **str)
{
	unsigned ch;
	GET_UTF_8(*str, ch);
	return ch;
}

/* view.c */

unsigned char *textptr_add(unsigned char *t, int i, int cp);
int textptr_diff(unsigned char *t2, unsigned char *t1, int cp);

extern int ismap_link, ismap_x, ismap_y;

void frm_download(struct session *, struct f_data_c *);
void frm_download_image(struct session *, struct f_data_c *);
void frm_view_image(struct session *, struct f_data_c *);
struct format_text_cache_entry *format_text(struct f_data_c *fd,
                                            struct form_control *fc,
                                            struct form_state *fs);
int area_cursor(struct f_data_c *f, struct form_control *fc,
                struct form_state *fs);
struct form_state *find_form_state(struct f_data_c *, struct form_control *);
void fixup_select_state(struct form_control *fc, struct form_state *fs);
int enter(struct session *ses, struct f_data_c *f, int a);
int field_op(struct session *ses, struct f_data_c *f, struct link *l,
             struct links_event *ev);

int can_open_in_new(struct terminal *);
void open_in_new_window(struct terminal *, void *fn_, void *ses_);
extern void (*const send_open_new_xterm_ptr)(struct terminal *, void *fn_,
                                             void *ses_);
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
void do_for_frame(struct session *,
                  void (*)(struct session *, struct f_data_c *, int), int);
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

/* html.c */

enum html_attr {
	AT_BOLD = 1,
	AT_ITALIC = 2,
	AT_UNDERLINE = 4,
	AT_FIXED = 8,
	AT_GRAPHICS = 16,
	AT_INVERT = 32
};

enum html_al {
	AL_LEFT,
	AL_CENTER,
	AL_RIGHT,
	AL_BLOCK,
	AL_NO,
	AL_NO_BREAKABLE,
	AL_BOTTOM,
	AL_MIDDLE,
	AL_TOP,

	AL_MASK,
	AL_NOBRLEXP,
	AL_MONO
};

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

enum par_t { P_NUMBER = 1, P_alpha, P_ALPHA, P_roman, P_ROMAN };

enum par_s { P_STAR = 1, P_O, P_PLUS, P_LISTMASK = 7, P_COMPACT };

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
	list_entry_1st;
	struct text_attrib attr;
	struct par_attrib parattr;
#define INVISIBLE        1
#define INVISIBLE_SCRIPT 2
#define INVISIBLE_STYLE  3
	int invisible;
	unsigned char *name;
	int namelen;
	unsigned char *options;
	int linebreak;
	int dontkill;
	struct frameset_desc *frameset;
};

extern int get_attr_val_nl;

extern struct list_head html_stack;
extern int line_breax;

extern int html_format_changed;

extern unsigned char *startf;
extern unsigned char *eofff;

#define html_top_  list_struct(html_stack.next, struct html_element)
#define html_top   (*html_top_)
#define format_    (html_top.attr)
#define par_format (html_top.parattr)

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

int parse_element(unsigned char *, unsigned char *, unsigned char **, int *,
                  unsigned char **, unsigned char **);
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
void parse_html(unsigned char *, unsigned char *,
                void (*)(void *, unsigned char *, int), void (*)(void *),
                void *(*)(void *, int, ...), void *, unsigned char *);
int get_image_map(unsigned char *, unsigned char *, unsigned char *,
                  unsigned char *a, struct menu_item **, struct memory_list **,
                  unsigned char *, unsigned char *, int, int, int, int gfx);
void scan_http_equiv(unsigned char *, unsigned char *, unsigned char **, int *,
                     unsigned char **, unsigned char **, unsigned char **,
                     int *);

int decode_color(unsigned char *, struct rgb *);

enum sp {
	SP_TAG,
	SP_CONTROL,
	SP_TABLE,
	SP_USED,
	SP_FRAMESET,
	SP_FRAME,
	SP_SCRIPT,
	SP_IMAGE,
	SP_NOWRAP,
	SP_REFRESH,
	SP_SET_BASE,
	SP_HR
};

struct frameset_param {
	struct frameset_desc *parent;
	int x, y;
	int *xw, *yw;
};

enum scroll { SCROLLING_NO, SCROLLING_YES, SCROLLING_AUTO };

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

struct sizes {
	int xmin, xmax, y;
};

extern struct f_data *current_f_data;

void free_additional_files(struct additional_files **);
void free_frameset_desc(struct frameset_desc *);
struct frameset_desc *copy_frameset_desc(struct frameset_desc *);

struct f_data *init_formatted(struct document_options *);
void destroy_formatted(struct f_data *);

/* d_opt je podle Mikulase nedefinovany mimo html parser, tak to jinde
 * nepouzivejte
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
void html_process_refresh(struct f_data *, unsigned char *, int);

int compare_opt(struct document_options *, struct document_options *);
void copy_opt(struct document_options *, struct document_options *);

struct link *new_link(struct f_data *);
struct conv_table *get_convert_table(unsigned char *, int, int, int *, int *,
                                     int);
struct part *format_html_part(unsigned char *, unsigned char *, int, int, int,
                              struct f_data *, int, int, unsigned char *, int);
void really_format_html(struct cache_entry *, unsigned char *, unsigned char *,
                        struct f_data *, int frame);
struct link *get_link_at_location(struct f_data *f, int x, int y);
int get_search_data(struct f_data *);

struct frameset_desc *create_frameset(struct f_data *fda,
                                      struct frameset_param *fp);
void create_frame(struct frame_param *fp);

/* html_tbl.c */

unsigned char *skip_element(unsigned char *, unsigned char *, unsigned char *,
                            int);
void format_table(unsigned char *, unsigned char *, unsigned char *,
                  unsigned char **, void *);
void table_bg(struct text_attrib *ta, unsigned char bgstr[8]);

void *find_table_cache_entry(unsigned char *start, unsigned char *end,
                             int align, int m, int width, int xs, int link_num);
void add_table_cache_entry(unsigned char *start, unsigned char *end, int align,
                           int m, int width, int xs, int link_num, void *p);

/* default.c */

extern unsigned char default_target[MAX_STR_LEN];

unsigned char *parse_options(int, char *[]);
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
	list_entry_1st;
	int kbd_codepage;
	int palette_mode;
	unsigned char *param;
	unsigned char shell_term[MAX_STR_LEN];
	int nosave;
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
#define D_DUMP   1
#define D_SOURCE 2
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

enum addr {
	ADDR_PREFERENCE_DEFAULT,
	ADDR_PREFERENCE_IPV4,
	ADDR_PREFERENCE_IPV6,
	ADDR_PREFERENCE_IPV4_ONLY,
	ADDR_PREFERENCE_IPV6_ONLY
};

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

enum ssl_cert {
	SSL_ACCEPT_INVALID_CERTIFICATE,
	SSL_WARN_ON_INVALID_CERTIFICATE,
	SSL_REJECT_INVALID_CERTIFICATE
};

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

#define SCRUB_HEADERS (proxies.only_proxies || http_options.header.fake_firefox)

extern double display_red_gamma, display_green_gamma, display_blue_gamma;
extern double user_gamma;
extern double bfu_aspect;
extern int dither_letters;
extern int dither_images;
extern int gamma_bits;
extern int overwrite_instead_of_scroll;

extern unsigned char bookmarks_file[MAX_STR_LEN];

extern int save_history;

extern struct document_setup dds;

/* listedit.c */

enum title { TITLE_EDIT, TITLE_ADD };

struct list {
	list_entry_1st;
	unsigned char type;
	/*
	 * bit 0: 0=item, 1=directory
	 * bit 1: directory is open (1)/closed (0); for item unused
	 * bit 2: 1=item is selected 0=item is not selected
	 */
	int depth;
	struct list *fotr; /* ignored when list is flat */
};

#define list_next(l) (list_struct((l)->list_entry.next, struct list))
#define list_prev(l) (list_struct((l)->list_entry.prev, struct list))

#define list_head_1st struct list head;
#define list_head_last

struct list_description {
	unsigned char type; /* 0=flat, 1=tree */
	struct list *list;  /* head of the list */
	struct list *(*new_item)(
	    void * /* data in internal format */); /* creates new item, does NOT
	                                              add to the list */
	void (*edit_item)(struct dialog_data *, struct list *,
	                  void (*)(struct dialog_data *, struct list *,
	                           struct list *,
	                           struct list_description *) /* ok function */,
	                  struct list * /* parameter for the ok_function */,
	                  unsigned char); /* must call call delete_item on the
	                                     item after all */
	void *(*default_value)(
	    struct session *,
	    unsigned char /* 0=item, 1=directory */); /* called when add button
	                                                 is pressed, allocates
	                                                 memory, return value is
	                                                 passed to the new_item
	                                                 function, new_item
	                                                 fills the item with
	                                                 this data */
	void (*delete_item)(
	    struct list *); /* delete item, if next and prev are not NULL
	                       adjusts pointers in the list */
	void (*copy_item)(
	    struct list * /* old */,
	    struct list
		* /* new */); /* gets 2 allocated items, copies all item data
	                         except pointers from first item to second one,
	                         old data (in second item) will be destroyed */
	unsigned char *(*type_item)(struct terminal *, struct list *, int /* 0=type whole item (e.g. when deleting item), 1=type only e.g title (in list window )*/);   /* alllocates buffer and writes item into it */
	struct list *(*find_item)(
	    struct list *start_item, unsigned char *string,
	    int direction /* 1 or -1 */); /* returns pointer to the first item
	                                     matching given string or NULL if
	                                     failed. Search starts at start_item
	                                     including. */
	struct history *search_history;
	int codepage; /* codepage of all string */
	int n_items;  /* number of items in main window */

	/* following items are string codes */
	int item_description;    /* e.g. "bookmark" or "extension" ... */
	int already_in_use;      /* e.g. "Bookmarks window is already open" */
	int window_title;        /* main window title */
	int delete_dialog_title; /* e.g. "Delete bookmark dialog" */
	int button;              /* when there's no button button_fn is NULL */

	void (*button_fn)(struct session *,
	                  struct list *); /* gets pointer to the item */
	void (*save)(struct session *);

	/* internal variables, should not be modified, initially set to 0 */
	struct list *current_pos;
	struct list *win_offset;
	int win_pos;
	int open;     /* 0=closed, 1=open */
	int modified; /* listedit reports 1 when the list was modified by user
	                 (and should be e.g. saved) */
	struct dialog_data *dlg; /* current dialog, valid only when open==1 */
	unsigned char *search_word;
	int search_direction;
};

int test_list_window_in_use(struct list_description *ld, struct terminal *term);
int create_list_window(struct list_description *, struct list *,
                       struct terminal *, struct session *);
void
reinit_list_window(struct list_description *ld); /* reinitializes list window */

/* types.c */

struct list;

struct assoc {
	list_head_1st unsigned char *label;
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
	list_head_1st unsigned char *ext;
	unsigned char *ct;
	list_head_last
};

struct protocol_program {
	list_entry_1st;
	unsigned char *prog;
	int system;
};

extern struct list assoc;
extern struct list extensions;

unsigned char *get_compress_by_extension(char *, char *);
unsigned char *get_content_type_by_extension(unsigned char *url);
unsigned char *get_content_type(unsigned char *, unsigned char *);
unsigned char *get_content_encoding(unsigned char *head, unsigned char *url,
                                    int just_ce);
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

void finalize_bookmarks(void); /* called, when exiting links */
void init_bookmarks(void);     /* called at start */
void reinit_bookmarks(struct session *ses, unsigned char *new_bookmarks_file);

/* Launches bookmark manager */
void menu_bookmark_manager(struct terminal *, void *, void *);
