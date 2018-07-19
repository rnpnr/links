/* os_dep.c
 * (c) 2002 Mikulas Patocka
 * This file is a part of the Links program, released under GPL.
 */

#include "links.h"

#include <sys/ioctl.h>

#ifdef HAVE_PTHREADS
#include <pthread.h>
#endif

#if defined(HAVE_PTHREADS)
static pthread_mutex_t pth_mutex;
static void fd_lock(void);
static void fd_unlock(void);
static void fd_init(void)
{
	int r;
	r = pthread_mutex_init(&pth_mutex, NULL);
	if (r)
		fatal_exit("pthread_mutex_create failed: %s", strerror(r));
}
#endif

void init_os(void)
{
	/* Disable per-thread heap */
#if defined(HAVE_MALLOPT) && defined(M_ARENA_TEST)
	mallopt(M_ARENA_TEST, 1);
#endif
#if defined(HAVE_MALLOPT) && defined(M_ARENA_MAX)
	mallopt(M_ARENA_MAX, 1);
#endif

#if defined(HAVE_PTHREADS)
	{
		int r;
		fd_init();
		r = pthread_atfork(fd_lock, fd_unlock, fd_init);
		if (r)
			fatal_exit("pthread_atfork failed: %s", strerror(r));
	}
#endif
}

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

unsigned char *escape_path(unsigned char *path)
{
	unsigned char *result;
	size_t i;
	if (strchr(cast_const_char path, '"')) return stracpy(path);
	for (i = 0; path[i]; i++) if (!is_safe_in_url(path[i])) goto do_esc;
	return stracpy(path);
	do_esc:
	result = stracpy(cast_uchar "\"");
	add_to_strn(&result, path);
	add_to_strn(&result, cast_uchar "\"");
	return result;
}

static inline int get_e(unsigned char *env)
{
	unsigned char *v;
	if ((v = cast_uchar getenv(cast_const_char env))) return atoi(cast_const_char v);
	return 0;
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
	if (clipboard) mem_free(clipboard), clipboard = NULL;
}

/* Terminal size */

#ifdef SIGWINCH
static void sigwinch(void *s)
{
	((void (*)(void))s)();
}
#endif

void handle_terminal_resize(int fd, void (*fn)(void))
{
#ifdef SIGWINCH
	install_signal_handler(SIGWINCH, sigwinch, (void *)fn, 0);
#endif
}

void unhandle_terminal_resize(int fd)
{
#ifdef SIGWINCH
	install_signal_handler(SIGWINCH, NULL, NULL, 0);
#endif
}

int get_terminal_size(int fd, int *x, int *y)
{
	int rs = -1;
#ifdef TIOCGWINSZ
	/* Sun Studio misoptimizes it */
	sun_volatile struct winsize ws;
	EINTRLOOP(rs, ioctl(1, TIOCGWINSZ, &ws));
#endif
	if ((rs == -1
#ifdef TIOCGWINSZ
		|| !(*x = ws.ws_col)
#endif
		) && !(*x = get_e(cast_uchar "COLUMNS"))) {
		*x = 80;
#ifdef _UWIN
		*x = 79;
#endif
	}
	if ((rs == -1
#ifdef TIOCGWINSZ
		|| !(*y = ws.ws_row)
#endif
		) && !(*y = get_e(cast_uchar "LINES"))) {
		*y = 24;
	}
	return 0;
}

#if defined(HAVE_PTHREADS)

static void fd_lock(void)
{
	int r;
	r = pthread_mutex_lock(&pth_mutex);
	if (r)
		fatal_exit("pthread_mutex_lock failed: %s", strerror(r));
}

static void fd_unlock(void)
{
	int r;
	r = pthread_mutex_unlock(&pth_mutex);
	if (r)
		fatal_exit("pthread_mutex_lock failed: %s", strerror(r));
}

#else

#define fd_lock()	do { } while (0)
#define fd_unlock()	do { } while (0)

#endif

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

int c_pipe(int *fd)
{
	int r;
	fd_lock();
	EINTRLOOP(r, pipe(fd));
	if (!r) new_fd_bin(fd[0]), new_fd_bin(fd[1]);
	fd_unlock();
	return r;
}

int c_dup(int oh)
{
	int h;
	fd_lock();
	EINTRLOOP(h, dup(oh));
	if (h != -1) new_fd_cloexec(h);
	fd_unlock();
	return h;
}

int c_socket(int d, int t, int p)
{
	int h;
	fd_lock();
	EINTRLOOP(h, socket(d, t, p));
	if (h != -1) new_fd_cloexec(h);
	fd_unlock();
	return h;
}

int c_accept(int h, struct sockaddr *addr, socklen_t *addrlen)
{
	int rh;
	fd_lock();
	EINTRLOOP(rh, accept(h, addr, addrlen));
	if (rh != -1) new_fd_cloexec(rh);
	fd_unlock();
	return rh;
}

int c_open(unsigned char *path, int flags)
{
	int h;
	fd_lock();
	EINTRLOOP(h, open(cast_const_char path, flags));
	if (h != -1) new_fd_bin(h);
	fd_unlock();
	return h;
}

int c_open3(unsigned char *path, int flags, int mode)
{
	int h;
	fd_lock();
	EINTRLOOP(h, open(cast_const_char path, flags, mode));
	if (h != -1) new_fd_bin(h);
	fd_unlock();
	return h;
}

DIR *c_opendir(unsigned char *path)
{
	DIR *d;
	fd_lock();
	ENULLLOOP(d, opendir(cast_const_char path));
	if (d) {
		int h;
		EINTRLOOP(h, dirfd(d));
		if (h != -1) new_fd_cloexec(h);
	}
	fd_unlock();
	return d;
}

#if defined(O_SIZE) && defined(__EMX__)

int open_prealloc(unsigned char *name, int flags, int mode, off_t siz)
{
	int h;
	fd_lock();
	EINTRLOOP(h, open(cast_const_char name, flags | O_SIZE, mode, (unsigned long)siz));
	if (h != -1) new_fd_bin(h);
	fd_unlock();
	return h;
}

#elif defined(HAVE_OPEN_PREALLOC)

int open_prealloc(unsigned char *name, int flags, int mode, off_t siz)
{
	int h, rs;
	fd_lock();
	EINTRLOOP(h, open(cast_const_char name, flags, mode));
	if (h == -1) {
		fd_unlock();
		return -1;
	}
	new_fd_bin(h);
	fd_unlock();
#if defined(HAVE_FALLOCATE)
#if defined(FALLOC_FL_KEEP_SIZE)
	EINTRLOOP(rs, fallocate(h, FALLOC_FL_KEEP_SIZE, 0, siz));
#else
	EINTRLOOP(rs, fallocate(h, 0, 0, siz));
#endif
	if (!rs) return h;
#endif
#if defined(HAVE_POSIX_FALLOCATE)
	/* posix_fallocate may fall back to overwriting the file with zeros,
	   so don't use it on too big files */
	if (siz > 134217728)
		return h;
	do {
		rs = posix_fallocate(h, 0, siz);
	} while (rs == EINTR);
	if (!rs) return h;
#endif
	return h;
}

#endif

/* Exec */

int is_twterm(void)
{
	static int xt = -1;
	if (xt == -1) xt = !!getenv("TWDISPLAY");
	return xt;
}

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
	struct terminal *t;
	struct list_head *lt;
	struct download *d;
	struct list_head *ld;
	struct connection *c;
	struct list_head *lc;
	struct k_conn *k;
	struct list_head *lk;
	int rs;
#ifndef NO_SIGNAL_HANDLERS
	EINTRLOOP(rs, close(signal_pipe[0]));
	EINTRLOOP(rs, close(signal_pipe[1]));
#endif
#ifdef G
	if (drv && drv->after_fork) drv->after_fork();
#endif
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

void get_path_to_exe(void)
{
	path_to_exe = g_argv[0];
}

void init_os_terminal(void)
{
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
int exe(unsigned char *path, int fg)
{
#ifndef EXEC_IN_THREADS
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
#endif
#ifdef G
	if (F && drv->exec) return drv->exec(path, fg);
#endif
	return system(cast_const_char path);
}

/* clipboard -> links */
unsigned char *get_clipboard_text(struct terminal *term)
{
#ifdef G
	if (F && drv->get_clipboard_text) {
		return drv->get_clipboard_text();
	}
#endif
	if (!clipboard)
		return NULL;
	return convert(utf8_table, term_charset(term), clipboard, NULL);
}

/* links -> clipboard */
void set_clipboard_text(struct terminal *term, unsigned char *data)
{
#ifdef G
	if (F && drv->set_clipboard_text) {
		drv->set_clipboard_text(term->dev, data);
		return;
	}
#endif
	if (clipboard) mem_free(clipboard);
	clipboard = convert(term_charset(term), utf8_table, data, NULL);
}

int clipboard_support(struct terminal *term)
{
#ifdef G
	if (F && drv->set_clipboard_text) {
		return 1;
	}
#endif
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

int resize_window(int x, int y)
{
	return -1;
}

/* Threads */

#if defined(HAVE_PTHREADS)

struct tdata {
	void (*fn)(void *, int);
	int h;
	int counted;
	unsigned char data[1];
};

static void bgt(void *t_)
{
	struct tdata *t = t_;
	int rs;
	ignore_signals();
	t->fn(t->data, t->h);
	EINTRLOOP(rs, (int)write(t->h, "x", 1));
	EINTRLOOP(rs, close(t->h));
	free(t);
}

#endif

void terminate_osdep(void)
{
}

void block_stdin(void) {}
void unblock_stdin(void) {}

#if defined(HAVE_PTHREADS)

static unsigned thread_count = 0;

static inline void reset_thread_count(void)
{
	fd_lock();
	thread_count = 0;
	fd_unlock();
}

static void inc_thread_count(void)
{
	fd_lock();
	thread_count++;
	fd_unlock();
}

static void dec_thread_count(void)
{
	fd_lock();
	if (!thread_count)
		internal("thread_count underflow");
	thread_count--;
	fd_unlock();
}

static inline unsigned get_thread_count(void)
{
	unsigned val;
	fd_lock();
	val = thread_count;
	fd_unlock();
	return val;
}

static void *bgpt(void *t)
{
	int counted = ((struct tdata *)t)->counted;
	bgt(t);
	if (counted) dec_thread_count();
	return NULL;
}

int start_thread(void (*fn)(void *, int), void *ptr, int l, int counted)
{
	pthread_attr_t attr;
	pthread_t thread;
	struct tdata *t;
	int p[2];
	int rs;
	if (c_pipe(p) < 0) return -1;
	retry1:
	if (!(t = malloc(sizeof(struct tdata) + l))) {
		if (out_of_memory(0, NULL, 0))
			goto retry1;
		goto err1;
	}
	t->fn = fn;
	t->h = p[1];
	t->counted = counted;
	memcpy(t->data, ptr, l);
	retry2:
	if (pthread_attr_init(&attr)) {
		if (out_of_memory(0, NULL, 0))
			goto retry2;
		goto err2;
	}
	retry3:
	if (pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED)) {
		if (out_of_memory(0, NULL, 0))
			goto retry3;
		goto err3;
	}
#ifdef THREAD_NEED_STACK_SIZE
	retry4:
	if (pthread_attr_setstacksize(&attr, THREAD_NEED_STACK_SIZE)) {
		if (out_of_memory(0, NULL, 0))
			goto retry4;
		goto err3;
	}
#endif
	if (counted) inc_thread_count();
	if (pthread_create(&thread, &attr, bgpt, t)) {
		if (counted) dec_thread_count();
		goto err3;
	}
	pthread_attr_destroy(&attr);
	return p[0];

	err3:
	pthread_attr_destroy(&attr);
	err2:
	free(t);
	err1:
	EINTRLOOP(rs, close(p[0]));
	EINTRLOOP(rs, close(p[1]));
	return -1;
}

#else /* HAVE_BEGINTHREAD */

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

#endif

void want_draw(void) {}
void done_draw(void) {}

int get_output_handle(void) { return 1; }

int get_ctl_handle(void) { return 0; }

#if defined(HAVE_BEGINTHREAD) && defined(HAVE__READ_KBD)

int get_input_handle(void)
{
	int rs;
	int fd[2];
	if (ti != -1) return ti;
	if (is_xterm()) return 0;
	if (c_pipe(fd) < 0) return 0;
	ti = fd[0];
	tp = fd[1];
	if (_beginthread(input_thread, NULL, 0x10000, (void *)tp) == -1) {
		EINTRLOOP(rs, close(fd[0]));
		EINTRLOOP(rs, close(fd[1]));
		return 0;
	}
	return fd[0];
}

#else

int get_input_handle(void)
{
	return 0;
}

#endif /* defined(HAVE_BEGINTHREAD) && defined(HAVE__READ_KBD) */


void *handle_mouse(int cons, void (*fn)(void *, unsigned char *, int), void *data) { return NULL; }
void unhandle_mouse(void *data) { }

int get_system_env(void)
{
	return 0;
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
	mem_free(str);
}

static int open_in_new_twterm(struct terminal *term, unsigned char *exe, unsigned char *param)
{
	unsigned char *twterm;
	if (!(twterm = cast_uchar getenv("LINKS_TWTERM"))) twterm = cast_uchar "twterm -e";
	exec_new_links(term, twterm, exe, param);
	return 0;
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

#ifdef G
static int open_in_new_g(struct terminal *term, unsigned char *exe, unsigned char *param)
{
	void *info;
	unsigned char *target = NULL;
	int len;
	int base = 0;
	unsigned char *url;
	if (!cmpbeg(param, cast_uchar "-base-session ")) {
		param = cast_uchar strchr(cast_const_char param, ' ') + 1;
		base = atoi(cast_const_char param);
		param += strcspn(cast_const_char param, " ");
		if (*param == ' ') param++;
	}
	if (!cmpbeg(param, cast_uchar "-target ")) {
		param = cast_uchar strchr(cast_const_char param, ' ') + 1;
		target = param;
		param += strcspn(cast_const_char param, " ");
		if (*param == ' ') *param++ = 0;
	}
	url = param;
	info = create_session_info(base, url, target, &len);
	return attach_g_terminal(term->cwd, info, len);
}
#endif

static const struct {
	int env;
	int (*open_window_fn)(struct terminal *term, unsigned char *, unsigned char *);
	unsigned char *text;
	unsigned char *hk;
} oinw[] = {
	{ENV_XWIN, open_in_new_xterm, TEXT_(T_XTERM), TEXT_(T_HK_XTERM)},
	{ENV_TWIN, open_in_new_twterm, TEXT_(T_TWTERM), TEXT_(T_HK_TWTERM)},
	{ENV_SCREEN, open_in_new_screen, TEXT_(T_SCREEN), TEXT_(T_HK_SCREEN)},
#ifdef G
	{ENV_G, open_in_new_g, TEXT_(T_WINDOW), TEXT_(T_HK_WINDOW)},
#endif
};

struct open_in_new *get_open_in_new(int environment)
{
	int i;
	struct open_in_new *oin = DUMMY;
	int noin = 0;
	if (anonymous) return NULL;
	if (environment & ENV_G) environment = ENV_G;
	for (i = 0; i < (int)array_elements(oinw); i++) if ((environment & oinw[i].env) == oinw[i].env) {
		if ((unsigned)noin > MAXINT / sizeof(struct open_in_new) - 2) overalloc();
		oin = mem_realloc(oin, (noin + 2) * sizeof(struct open_in_new));
		oin[noin].text = oinw[i].text;
		oin[noin].hk = oinw[i].hk;
		oin[noin].open_window_fn = &oinw[i].open_window_fn;
		noin++;
		oin[noin].text = NULL;
		oin[noin].hk = NULL;
		oin[noin].open_window_fn = NULL;
	}
	if (oin == DUMMY) return NULL;
	return oin;
}

int can_resize_window(struct terminal *term)
{
	return 0;
}

int can_open_os_shell(int environment)
{
#ifdef G
	if (F && drv->flags & GD_NO_OS_SHELL) return 0;
#endif
	return 1;
}

void set_highpri(void)
{
}

void os_seed_random(unsigned char **pool, int *pool_size)
{
	*pool = DUMMY;
	*pool_size = 0;
}

void os_detach_console(void)
{
#if !defined(NO_FORK_ON_EXIT)
	{
		pid_t rp;
		EINTRLOOP(rp, fork());
		if (!rp) {
			reinit_child();
#if defined(HAVE_PTHREADS)
			reset_thread_count();
#endif
		}
		if (rp > 0) {
#if defined(HAVE_PTHREADS)
			while (get_thread_count()) {
				portable_sleep(1000);
			}
#endif
			_exit(0);
		}
	}
#endif
}
