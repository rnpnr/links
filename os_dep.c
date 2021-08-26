/* os_dep.c
 * (c) 2002 Mikulas Patocka
 * This file is a part of the Links program, released under GPL.
 */

#include <fcntl.h>
#include <limits.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

#include "links.h"

int page_size = 4096;

int is_safe_in_shell(unsigned char c)
{
	return c == '@' || c == '+' || c == '-' || c == '.' || c == ',' || c == '=' || (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || c == '_' || (c >= 'a' && c <= 'z');
}

static inline int is_safe_in_file(unsigned char c)
{
	return !(c < ' ' || c == '"' || c == '*' || c == '/' || c == ':' || c == '<' || c == '>' || c == '\\' || c == '|' || c >= 0x80);
}

static inline int is_safe_in_url(unsigned char c)
{
	return is_safe_in_shell(c) || c == ':' || c == '/' || c >= 0x80;
}

void check_shell_security(unsigned char **cmd)
{
	unsigned char *c = *cmd;
	while (*c) {
		if (!is_safe_in_shell(*c)) *c = '_';
		c++;
	}
}

void check_filename(unsigned char **file)
{
	unsigned char *c = *file;
	while (*c) {
		if (!is_safe_in_file(*c)) *c = '_';
		c++;
	}
}

int check_shell_url(unsigned char *url)
{
	while (*url) {
		if (!is_safe_in_url(*url)) return -1;
		url++;
	}
	return 0;
}

unsigned char *escape_path(char *path)
{
	unsigned char *result;
	size_t i;
	if (strchr(path, '"')) return stracpy(cast_uchar path);
	for (i = 0; path[i]; i++) if (!is_safe_in_url(path[i])) goto do_esc;
	return stracpy(cast_uchar path);
	do_esc:
	result = stracpy(cast_uchar "\"");
	add_to_strn(&result, cast_uchar path);
	add_to_strn(&result, cast_uchar "\"");
	return result;
}

static int get_e(const char *env)
{
	const char *v;
	if ((v = getenv(env)))
		return atoi(v);
	return 0;
}

void init_page_size(void) {
	long getpg = -1;
	if (getpg < 0)
		getpg = getpagesize();
	if (getpg > 0 && !(getpg & (getpg - 1))) page_size = (int)getpg;
}

void do_signal(int sig, void (*handler)(int))
{
	errno = 0;
	while (signal(sig, handler) == SIG_ERR && errno == EINTR) errno = 0;
}

void ignore_signals(void)
{
	do_signal(SIGPIPE, SIG_IGN);
#ifdef SIGXFSZ
	do_signal(SIGXFSZ, SIG_IGN);
#endif
}

uttime get_absolute_time(void)
{
	struct timeval tv;
	int rs;
	EINTRLOOP(rs, gettimeofday(&tv, NULL));
	if (rs) fatal_exit("gettimeofday failed: %d", errno);
	return (uttime)tv.tv_sec * 1000 + (unsigned)tv.tv_usec / 1000;
}

uttime get_time(void)
{
#if defined(CLOCK_MONOTONIC_RAW) || defined(CLOCK_MONOTONIC)
	struct timespec ts;
	int rs;
#if defined(CLOCK_MONOTONIC_RAW)
	EINTRLOOP(rs, clock_gettime(CLOCK_MONOTONIC_RAW, &ts));
	if (!rs) return (uttime)ts.tv_sec * 1000 + (unsigned)ts.tv_nsec / 1000000;
#endif
#if defined(CLOCK_MONOTONIC)
	EINTRLOOP(rs, clock_gettime(CLOCK_MONOTONIC, &ts));
	if (!rs) return (uttime)ts.tv_sec * 1000 + (unsigned)ts.tv_nsec / 1000000;
#endif
#endif
	return get_absolute_time();
}

static unsigned char *clipboard = NULL;

void os_free_clipboard(void)
{
	free(clipboard);
	clipboard = NULL;
}

/* Terminal size */

static void (*terminal_resize_callback)(int, int);

#ifdef SIGWINCH
static void sigwinch(void *s)
{
	int cur_xsize, cur_ysize;
	get_terminal_size(&cur_xsize, &cur_ysize);
	terminal_resize_callback(cur_xsize, cur_ysize);
}
#endif

void handle_terminal_resize(void (*fn)(int, int), int *x, int *y)
{
	terminal_resize_callback = fn;
	get_terminal_size(x, y);
#if defined(SIGWINCH)
	install_signal_handler(SIGWINCH, sigwinch, NULL, 0);
#endif
}

void unhandle_terminal_resize(void)
{
#if defined(SIGWINCH)
	install_signal_handler(SIGWINCH, NULL, NULL, 0);
#endif
}

void get_terminal_size(int *x, int *y)
{
	int rs = -1;
#ifdef TIOCGWINSZ
	struct winsize ws;
	EINTRLOOP(rs, ioctl(1, TIOCGWINSZ, &ws));
#endif
	if ((rs == -1
#ifdef TIOCGWINSZ
		|| !(*x = ws.ws_col)
#endif
		) && !(*x = get_e("COLUMNS"))) {
		*x = 80;
	}
	if ((rs == -1
#ifdef TIOCGWINSZ
		|| !(*y = ws.ws_row)
#endif
		) && !(*y = get_e("LINES"))) {
		*y = 24;
	}
}

static void new_fd_cloexec(int fd)
{
	int rs;
	EINTRLOOP(rs, fcntl(fd, F_SETFD, FD_CLOEXEC));
}

static void new_fd_bin(int fd)
{
	new_fd_cloexec(fd);
}

/* Pipe */

void set_nonblock(int fd)
{
#ifdef O_NONBLOCK
	int rs;
	EINTRLOOP(rs, fcntl(fd, F_SETFL, O_NONBLOCK));
#elif defined(FIONBIO)
	int rs;
	int on = 1;
	EINTRLOOP(rs, ioctl(fd, FIONBIO, &on));
#endif
}

static int cleanup_fds(void)
{
#ifdef ENFILE
	if (errno == ENFILE) return abort_background_connections();
#endif
#ifdef EMFILE
	if (errno == EMFILE) return abort_background_connections();
#endif
	return 0;
}

int c_pipe(int fd[2])
{
	int r;
	do {
		EINTRLOOP(r, pipe(fd));
		if (!r) new_fd_bin(fd[0]), new_fd_bin(fd[1]);
	} while (r == -1 && cleanup_fds());
	return r;
}

int c_dup(int oh)
{
	int h;
	do {
		EINTRLOOP(h, dup(oh));
		if (h != -1) new_fd_cloexec(h);
	} while (h == -1 && cleanup_fds());
	return h;
}

int c_socket(int d, int t, int p)
{
	int h = socket(d, t, p);

	if (h == -1)
		die("socket()\n");

	if (fcntl(h, F_SETFD, FD_CLOEXEC) == -1)
		die("c_socket(): fcntl()\n");

	return h;
}

int c_accept(int sh, struct sockaddr *addr, socklen_t *addrlen)
{
	int h;
	do {
		EINTRLOOP(h, accept(sh, addr, addrlen));
		if (h != -1) new_fd_cloexec(h);
	} while (h == -1 && cleanup_fds());
	return h;
}

int c_open(unsigned char *path, int flags)
{
	int h;
	do {
		EINTRLOOP(h, open(cast_const_char path, flags));
		if (h != -1) new_fd_bin(h);
	} while (h == -1 && cleanup_fds());
	return h;
}

int c_open3(unsigned char *path, int flags, int mode)
{
	int h;
	do {
		EINTRLOOP(h, open(cast_const_char path, flags, mode));
		if (h != -1) new_fd_bin(h);
	} while (h == -1 && cleanup_fds());
	return h;
}

DIR *c_opendir(unsigned char *path)
{
	DIR *d;
	do {
		ENULLLOOP(d, opendir(cast_const_char path));
		if (d) {
			int h;
			EINTRLOOP(h, dirfd(d));
			if (h != -1) new_fd_cloexec(h);
		}
	} while (!d && cleanup_fds());
	return d;
}

/* Exec */

int is_screen(void)
{
	static int xt = -1;
	if (xt == -1) xt = !!getenv("STY");
	return xt;
}

int is_xterm(void)
{
	static int xt = -1;
	if (xt == -1) xt = getenv("DISPLAY") && *(char *)getenv("DISPLAY");
	return xt;
}

void close_fork_tty(void)
{
	struct terminal *t = NULL;
	struct list_head *lt;
	struct download *d = NULL;
	struct list_head *ld;
	struct connection *c = NULL;
	struct list_head *lc;
	struct k_conn *k = NULL;
	struct list_head *lk;
	int rs;
	EINTRLOOP(rs, close(signal_pipe[0]));
	EINTRLOOP(rs, close(signal_pipe[1]));
	if (terminal_pipe[1] != -1) EINTRLOOP(rs, close(terminal_pipe[1]));
	foreach(struct terminal, t, lt, terminals) {
		if (t->fdin > 0)
			EINTRLOOP(rs, close(t->fdin));
		if (t->handle_to_close >= 0)
			EINTRLOOP(rs, close(t->handle_to_close));
	}
	foreach(struct download, d, ld, downloads) if (d->handle > 0)
		EINTRLOOP(rs, close(d->handle));
	foreach(struct connection, c, lc, queue) {
		if (c->sock1 >= 0) EINTRLOOP(rs, close(c->sock1));
		if (c->sock2 >= 0) EINTRLOOP(rs, close(c->sock2));
	}
	foreach(struct k_conn, k, lk, keepalive_connections)
		EINTRLOOP(rs, close(k->conn));
}

unsigned char *os_conv_to_external_path(unsigned char *file, unsigned char *prog)
{
	return stracpy(file);
}

unsigned char *os_fixup_external_program(unsigned char *prog)
{
	return stracpy(prog);
}

/* UNIX */
int exe(char *path, int fg)
{
#ifdef SIGCHLD
	do_signal(SIGCHLD, SIG_DFL);
#endif
	do_signal(SIGPIPE, SIG_DFL);
#ifdef SIGXFSZ
	do_signal(SIGXFSZ, SIG_DFL);
#endif
#ifdef SIGTSTP
	do_signal(SIGTSTP, SIG_DFL);
#endif
#ifdef SIGCONT
	do_signal(SIGCONT, SIG_DFL);
#endif
#ifdef SIGWINCH
	do_signal(SIGWINCH, SIG_DFL);
#endif
	return system(path);
}

/* clipboard -> links */
unsigned char *get_clipboard_text(struct terminal *term)
{
	if (!clipboard)
		return NULL;
	return convert(0, term_charset(term), clipboard, NULL);
}

/* links -> clipboard */
void set_clipboard_text(struct terminal *term, unsigned char *data)
{
	free(clipboard);
	clipboard = convert(term_charset(term), 0, data, NULL);
}

int clipboard_support(struct terminal *term)
{
	return 0;
}

void set_window_title(unsigned char *title)
{
	/* !!! FIXME */
}

unsigned char *get_window_title(void)
{
	/* !!! FIXME */
	return NULL;
}

/* Threads */

int start_thread(void (*fn)(void *, int), void *ptr, int l, int counted)
{
	int p[2];
	pid_t f;
	int rs;
	if (c_pipe(p) < 0) return -1;
	EINTRLOOP(f, fork());
	if (!f) {
		close_fork_tty();
		EINTRLOOP(rs, close(p[0]));
		fn(ptr, p[1]);
		EINTRLOOP(rs, (int)write(p[1], "x", 1));
		EINTRLOOP(rs, close(p[1]));
		_exit(0);
	}
	if (f == -1) {
		EINTRLOOP(rs, close(p[0]));
		EINTRLOOP(rs, close(p[1]));
		return -1;
	}
	EINTRLOOP(rs, close(p[1]));
	return p[0];
}

static void exec_new_links(struct terminal *term, unsigned char *xterm, unsigned char *exe, unsigned char *param)
{
	unsigned char *str;
	str = stracpy(cast_uchar "");
	if (*xterm) {
		add_to_strn(&str, xterm);
		add_to_strn(&str, cast_uchar " ");
	}
	add_to_strn(&str, exe);
	add_to_strn(&str, cast_uchar " ");
	add_to_strn(&str, param);
	exec_on_terminal(term, str, cast_uchar "", 2);
	free(str);
}

unsigned char *links_xterm(void)
{
	unsigned char *xterm;
	if (!(xterm = cast_uchar getenv("LINKS_XTERM"))) {
		xterm = cast_uchar "xterm -e";
	}
	return xterm;
}

static int open_in_new_xterm(struct terminal *term, unsigned char *exe, unsigned char *param)
{
	exec_new_links(term, links_xterm(), exe, param);
	return 0;
}

static int open_in_new_screen(struct terminal *term, unsigned char *exe, unsigned char *param)
{
	exec_new_links(term, cast_uchar "screen", exe, param);
	return 0;
}

static const struct {
	int env;
	int (*open_window_fn)(struct terminal *term, unsigned char *, unsigned char *);
	unsigned char *text;
	unsigned char *hk;
} oinw[] = {
	{ENV_XWIN, open_in_new_xterm, TEXT_(T_XTERM), TEXT_(T_HK_XTERM)},
	{ENV_SCREEN, open_in_new_screen, TEXT_(T_SCREEN), TEXT_(T_HK_SCREEN)},
};

struct open_in_new *get_open_in_new(int environment)
{
	int i;
	struct open_in_new *oin = NULL;
	int noin = 0;
	if (anonymous)
		return NULL;
	for (i = 0; i < (int)array_elements(oinw); i++)
		if ((environment & oinw[i].env) == oinw[i].env) {
			if ((unsigned)noin > INT_MAX / sizeof(struct open_in_new) - 2)
				overalloc();
			oin = xrealloc(oin, (noin + 2) * sizeof(struct open_in_new));
			oin[noin].text = oinw[i].text;
			oin[noin].hk = oinw[i].hk;
			oin[noin].open_window_fn = &oinw[i].open_window_fn;
			noin++;
			oin[noin].text = NULL;
			oin[noin].hk = NULL;
			oin[noin].open_window_fn = NULL;
		}
	return oin;
}

int can_open_os_shell(int environment)
{
	return 1;
}

void os_detach_console(void)
{
#if !defined(NO_FORK_ON_EXIT)
	pid_t rp;
	EINTRLOOP(rp, fork());
	if (!rp)
		reinit_child();
	if (rp > 0)
		_exit(0);
#endif
}
