/* select.c
 * Select Loop
 * (c) 2002 Mikulas Patocka
 * This file is a part of the Links program, released under GPL.
 */

#include <limits.h>

#include "links.h"

#if defined(evtimer_set) && !defined(timeout_set)
	#define timeout_set evtimer_set
#endif
#if defined(evtimer_add) && !defined(timeout_add)
	#define timeout_add evtimer_add
#endif
#if defined(evtimer_del) && !defined(timeout_del)
	#define timeout_del evtimer_del
#endif

struct thread {
	void (*read_func)(void *);
	void (*write_func)(void *);
	void *data;
	struct event *read_event;
	struct event *write_event;
};

static struct thread *threads = NULL;
static int n_threads = 0;

static fd_set w_read;
static fd_set w_write;

static fd_set x_read;
static fd_set x_write;

static int w_max;

struct timer {
	list_entry_1st uttime interval;
	void (*func)(void *);
	void *data;
};

static struct list_head timers = { &timers, &timers };

void
portable_sleep(unsigned msec)
{
	struct timeval tv;
	int rs;
	block_signals(0, 0);
	tv.tv_sec = msec / 1000;
	tv.tv_usec = msec % 1000 * 1000;
	EINTRLOOP(rs, select(0, NULL, NULL, NULL, &tv));
	unblock_signals();
}

static int
can_do_io(int fd, int wr, int sec)
{
	fd_set fds;
	struct timeval tv, *tvp;
	int rs;

	if (fd < 0)
		die("can_do_io: handle %d", fd);

	struct pollfd p;
	p.fd = fd;
	p.events = !wr ? POLLIN : POLLOUT;
	EINTRLOOP(rs, poll(&p, 1, sec < 0 ? -1 : sec * 1000));
	if (rs < 0)
		die("poll for %s (%d) failed: %s", !wr ? "read" : "write", fd,
		    strerror(errno));
	if (!rs)
		return 0;
	if (p.revents & POLLNVAL)
		goto fallback;
	return 1;
fallback:
	if (sec >= 0) {
		tv.tv_sec = sec;
		tv.tv_usec = 0;
		tvp = &tv;
	} else
		tvp = NULL;
	FD_ZERO(&fds);
	if (fd >= (int)FD_SETSIZE)
		die("too big handle %d\n", fd);
	FD_SET(fd, &fds);
	if (!wr)
		EINTRLOOP(rs, select(fd + 1, &fds, NULL, NULL, tvp));
	else
		EINTRLOOP(rs, select(fd + 1, NULL, &fds, NULL, tvp));
	if (rs < 0)
		die("select for %s (%d) failed: %s\n", !wr ? "read" : "write",
		    fd, strerror(errno));
	return rs;
}

int
can_write(int fd)
{
	return can_do_io(fd, 1, 0);
}

int
can_read_timeout(int fd, int sec)
{
	return can_do_io(fd, 0, sec);
}

int
can_read(int fd)
{
	return can_do_io(fd, 0, 0);
}

int
close_stderr(void)
{
	int n, h, rs;
	fflush(stderr);
	n = c_open(cast_uchar "/dev/null", O_WRONLY | O_NOCTTY);
	if (n == -1)
		goto fail1;
	h = c_dup(2);
	if (h == -1)
		goto fail2;
	EINTRLOOP(rs, dup2(n, 2));
	if (rs == -1)
		goto fail3;
	EINTRLOOP(rs, close(n));
	return h;

fail3:
	EINTRLOOP(rs, close(h));
fail2:
	EINTRLOOP(rs, close(n));
fail1:
	return -1;
}

void
restore_stderr(int h)
{
	int rs;
	fflush(stderr);
	if (h == -1)
		return;
	EINTRLOOP(rs, dup2(h, 2));
	EINTRLOOP(rs, close(h));
}

unsigned long
select_info(int type)
{
	int i, j;
	switch (type) {
	case CI_FILES:
		i = 0;
		for (j = 0; j < w_max; j++)
			if (threads[j].read_func || threads[j].write_func)
				i++;
		return i;
	case CI_TIMERS:
		return list_size(&timers);
	default:
		die("select_info_info: bad request\n");
	}
	return 0;
}

struct bottom_half {
	list_entry_1st void (*fn)(void *);
	void *data;
};

static struct list_head bottom_halves = { &bottom_halves, &bottom_halves };

void
register_bottom_half(void (*fn)(void *), void *data)
{
	struct bottom_half *bh = NULL;
	struct list_head *lbh;
	foreach (struct bottom_half, bh, lbh, bottom_halves)
		if (bh->fn == fn && bh->data == data)
			return;
	bh = xmalloc(sizeof(struct bottom_half));
	bh->fn = fn;
	bh->data = data;
	add_to_list(bottom_halves, bh);
}

void
unregister_bottom_half(void (*fn)(void *), void *data)
{
	struct bottom_half *bh = NULL;
	struct list_head *lbh;
	foreach (struct bottom_half, bh, lbh, bottom_halves)
		if (bh->fn == fn && bh->data == data) {
			del_from_list(bh);
			free(bh);
			return;
		}
}

void
check_bottom_halves(void)
{
	struct bottom_half *bh;
	void (*fn)(void *);
	void *data;
rep:
	if (list_empty(bottom_halves))
		return;
	bh = list_struct(bottom_halves.prev, struct bottom_half);
	fn = bh->fn;
	data = bh->data;
	del_from_list(bh);
	free(bh);
	pr(fn(data)){};
	goto rep;
}

#define CHK_BH                                                                 \
	if (!list_empty(bottom_halves))                                        \
	check_bottom_halves()

static void
restrict_fds(void)
{
#if defined(RLIMIT_OFILE) && !defined(RLIMIT_NOFILE)
	#define RLIMIT_NOFILE RLIMIT_OFILE
#endif
#if defined(RLIMIT_NOFILE)
	struct rlimit limit;
	int rs;
	EINTRLOOP(rs, getrlimit(RLIMIT_NOFILE, &limit));
	if (rs)
		goto skip_limit;
	if (limit.rlim_cur > FD_SETSIZE) {
		limit.rlim_cur = FD_SETSIZE;
		EINTRLOOP(rs, setrlimit(RLIMIT_NOFILE, &limit));
	}
skip_limit:;
#endif
}

unsigned char *sh_file;
int sh_line;

static int event_enabled = 0;

#ifndef HAVE_EVENT_GET_STRUCT_EVENT_SIZE
	#define sizeof_struct_event sizeof(struct event)
#else
	#define sizeof_struct_event (event_get_struct_event_size())
#endif

static inline struct event *
timer_event(struct timer *tm)
{
	return (struct event *)((unsigned char *)tm - sizeof_struct_event);
}

static struct event_base *event_base;

static void
event_callback(int h, short ev, void *data)
{
#ifndef EV_PERSIST
	if (event_add((struct event *)data, NULL) == -1)
		die("event_add: %s\n", strerror(errno));
#endif
	if (!(ev & EV_READ) == !(ev & EV_WRITE))
		die("event_callback: invalid flags %d on handle %d\n", (int)ev,
		    h);
	if (ev & EV_READ) {
#if defined(HAVE_LIBEV)
		/* Old versions of libev badly interact with fork and fire
		 * events spuriously. */
		if (ev_version_major() < 4 && !can_read(h))
			return;
#endif
		pr(threads[h].read_func(threads[h].data))
		{
		}
	} else {
#if defined(HAVE_LIBEV)
		/* Old versions of libev badly interact with fork and fire
		 * events spuriously. */
		if (ev_version_major() < 4 && !can_write(h))
			return;
#endif
		pr(threads[h].write_func(threads[h].data))
		{
		}
	}
	CHK_BH;
}

static void
timer_callback(int h, short ev, void *data)
{
	struct timer *tm = data;
	pr(tm->func(tm->data))
	{
	}
	kill_timer(tm);
	CHK_BH;
}

static void
set_event_for_action(int h, void (*func)(void *), struct event **evptr,
                     short evtype)
{
	if (func) {
		if (!*evptr) {
#ifdef EV_PERSIST
			evtype |= EV_PERSIST;
#endif
			*evptr = xmalloc(sizeof_struct_event);
			event_set(*evptr, h, evtype, event_callback, *evptr);
			if (event_base_set(event_base, *evptr) == -1)
				die("event_base_set: %s at %s:%d, handle %d\n",
				    strerror(errno), sh_file, sh_line, h);
		}
		if (event_add(*evptr, NULL) == -1)
			die("event_add: %s at %s:%d, handle %d\n",
			    strerror(errno), sh_file, sh_line, h);
	} else {
		if (*evptr) {
			if (event_del(*evptr) == -1)
				die("event_del: %s at %s:%d, handle %d\n",
				    strerror(errno), sh_file, sh_line, h);
		}
	}
}

static void
set_events_for_handle(int h)
{
	set_event_for_action(h, threads[h].read_func, &threads[h].read_event,
	                     EV_READ);
	set_event_for_action(h, threads[h].write_func, &threads[h].write_event,
	                     EV_WRITE);
}

static void
set_event_for_timer(struct timer *tm)
{
	struct timeval tv;
	struct event *ev = timer_event(tm);
	timeout_set(ev, timer_callback, tm);
	if (event_base_set(event_base, ev) == -1)
		die("event_base_set: %s\n", strerror(errno));
	tv.tv_sec = tm->interval / 1000;
	tv.tv_usec = (tm->interval % 1000) * 1000;
#if defined(HAVE_LIBEV)
	if (!tm->interval && ev_version_major() < 4) {
		/* libev bug */
		tv.tv_usec = 1;
	}
#endif
	if (timeout_add(ev, &tv) == -1)
		die("timeout_add: %s\n", strerror(errno));
}

static void
enable_libevent(void)
{
	int i;
	struct timer *tm = NULL;
	struct list_head *ltm;

	if (disable_libevent)
		return;

	event_base = event_base_new();
	if (!event_base)
		return;
	event_enabled = 1;

	sh_file = (unsigned char *)__FILE__;
	sh_line = __LINE__;
	for (i = 0; i < w_max; i++)
		set_events_for_handle(i);

	foreach (struct timer, tm, ltm, timers)
		set_event_for_timer(tm);
}

static void
terminate_libevent(void)
{
	int i;
	if (event_enabled) {
		for (i = 0; i < n_threads; i++) {
			set_event_for_action(i, NULL, &threads[i].read_event,
			                     EV_READ);
			free(threads[i].read_event);
			set_event_for_action(i, NULL, &threads[i].write_event,
			                     EV_WRITE);
			free(threads[i].write_event);
		}
		event_base_free(event_base);
		event_enabled = 0;
	}
}

static void
do_event_loop(int flags)
{
	int e;
	e = event_base_loop(event_base, flags);
	if (e == -1)
		die("event_base_loop: %s\n", strerror(errno));
}

void
add_event_string(unsigned char **s, int *l, struct terminal *term)
{
	if (!event_enabled)
		add_to_str(s, l,
		           get_text_translation(TEXT_(T_SELECT_SYSCALL), term));
	if (!event_enabled)
		add_to_str(s, l, cast_uchar " (");
#if defined(HAVE_LIBEV)
	add_to_str(s, l, cast_uchar "LibEv");
#else
	add_to_str(s, l, cast_uchar "LibEvent");
#endif
	add_chr_to_str(s, l, ' ');
	{
#if defined(HAVE_LIBEV)
		/* old libev report bogus version */
		if (!casestrcmp(cast_uchar event_get_version(), cast_uchar
		                "EV_VERSION_MAJOR.EV_VERSION_MINOR")) {
			add_num_to_str(s, l, ev_version_major());
			add_chr_to_str(s, l, '.');
			add_num_to_str(s, l, ev_version_minor());
		} else
#endif
			add_to_str(s, l, cast_uchar event_get_version());
	}
	if (!event_enabled) {
		add_chr_to_str(s, l, ' ');
		add_to_str(s, l, get_text_translation(TEXT_(T_dISABLED), term));
		add_chr_to_str(s, l, ')');
	} else {
		add_chr_to_str(s, l, ' ');
		add_to_str(s, l, cast_uchar event_base_get_method(event_base));
	}
}

static uttime last_time;

static void
check_timers(void)
{
	uttime interval = get_time() - last_time;
	struct timer *t = NULL;
	struct list_head *lt;
	foreach (struct timer, t, lt, timers) {
		if (t->interval < interval)
			t->interval = 0;
		else
			t->interval -= interval;
	}
	while (!list_empty(timers)) {
		struct timer *t = list_struct(timers.next, struct timer);
		if (t->interval)
			break;
		pr(t->func(t->data)) break;
		kill_timer(t);
		CHK_BH;
	}
	last_time += interval;
}

struct timer *
install_timer(uttime t, void (*func)(void *), void *data)
{
	struct timer *tm;
	unsigned char *q = xmalloc(sizeof_struct_event + sizeof(struct timer));
	tm = (struct timer *)(q + sizeof_struct_event);
	tm->interval = t;
	tm->func = func;
	tm->data = data;
	if (event_enabled) {
		set_event_for_timer(tm);
		add_to_list(timers, tm);
	} else {
		struct timer *tt = NULL;
		struct list_head *ltt;
		foreach (struct timer, tt, ltt, timers)
			if (tt->interval >= t)
				break;
		add_before_list_entry(ltt, &tm->list_entry);
	}
	return tm;
}

void
kill_timer(struct timer *tm)
{
	del_from_list(tm);
	if (event_enabled)
		timeout_del(timer_event(tm));
	free(timer_event(tm));
}

void (*get_handler(int fd, int tp))(void *)
{
	if (fd < 0)
		die("get_handler: handle %d\n", fd);
	if (fd >= w_max)
		return NULL;
	switch (tp) {
	case H_READ:
		return threads[fd].read_func;
	case H_WRITE:
		return threads[fd].write_func;
	}
	die("get_handler: bad type %d\n", tp);
	return NULL;
}

void *
get_handler_data(int fd)
{
	if (fd < 0)
		die("get_handler: handle %d\n", fd);
	if (fd >= w_max)
		return NULL;
	return threads[fd].data;
}

void
set_handlers_file_line(int fd, void (*read_func)(void *),
                       void (*write_func)(void *), void *data)
{
	if (fd < 0)
		goto invl;
	if (!event_enabled)
		if (fd >= (int)FD_SETSIZE) {
			die("too big handle %d at %s:%d\n", fd, sh_file,
			    sh_line);
			return;
		}
	if (fd >= n_threads) {
		if ((unsigned)fd
		    > (unsigned)INT_MAX / sizeof(struct thread) - 1)
			overalloc();
		threads = xrealloc(threads, (fd + 1) * sizeof(struct thread));
		memset(threads + n_threads, 0,
		       (fd + 1 - n_threads) * sizeof(struct thread));
		n_threads = fd + 1;
	}
	if (threads[fd].read_func == read_func
	    && threads[fd].write_func == write_func && threads[fd].data == data)
		return;
	threads[fd].read_func = read_func;
	threads[fd].write_func = write_func;
	threads[fd].data = data;
	if (read_func || write_func) {
		if (fd >= w_max)
			w_max = fd + 1;
	} else if (fd == w_max - 1) {
		int i;
		for (i = fd - 1; i >= 0; i--)
			if (threads[i].read_func || threads[i].write_func)
				break;
		w_max = i + 1;
	}
	if (event_enabled) {
		set_events_for_handle(fd);
		return;
	}
	if (read_func)
		FD_SET(fd, &w_read);
	else {
		FD_CLR(fd, &w_read);
		FD_CLR(fd, &x_read);
	}
	if (write_func)
		FD_SET(fd, &w_write);
	else {
		FD_CLR(fd, &w_write);
		FD_CLR(fd, &x_write);
	}
	return;

invl:
	die("invalid set_handlers call at %s:%d: %d, %p, %p, %p\n", sh_file,
	    sh_line, fd, read_func, write_func, data);
}

void
clear_events(int h, int blocking)
{
#if !defined(O_NONBLOCK) && !defined(FIONBIO)
	blocking = 1;
#endif
	while (blocking ? can_read(h) : 1) {
		unsigned char c[64];
		int rd;
		EINTRLOOP(rd, (int)read(h, c, sizeof c));
		if (rd != sizeof c)
			break;
	}
}

#if defined(NSIG) && NSIG > 32
	#define NUM_SIGNALS NSIG
#else
	#define NUM_SIGNALS 32
#endif

static void
clear_events_ptr(void *handle)
{
	clear_events((int)(long)handle, 0);
}

struct signal_handler {
	void (*fn)(void *);
	void *data;
	int critical;
};

static volatile int signal_mask[NUM_SIGNALS];
static volatile struct signal_handler signal_handlers[NUM_SIGNALS];

static pid_t signal_pid;
int signal_pipe[2];

static void
got_signal(int sig)
{
	void (*fn)(void *);
	int sv_errno = errno;
	/*fprintf(stderr, "ERROR: signal number: %d\n", sig);*/

	/* if we get signal from a forked child, don't do anything */
	if (getpid() != signal_pid)
		goto ret;

	if (sig >= NUM_SIGNALS || sig < 0)
		goto ret;
	fn = signal_handlers[sig].fn;
	if (!fn)
		goto ret;
	if (signal_handlers[sig].critical) {
		fn(signal_handlers[sig].data);
		goto ret;
	}
	signal_mask[sig] = 1;
	if (can_write(signal_pipe[1])) {
		int wr;
		EINTRLOOP(wr, (int)write(signal_pipe[1], "", 1));
	}
ret:
	errno = sv_errno;
}

static struct sigaction sa_zero;

void
install_signal_handler(int sig, void (*fn)(void *), void *data, int critical)
{
	int rs;
	struct sigaction sa = sa_zero;
	if (sig >= NUM_SIGNALS || sig < 0) {
		die("bad signal number: %d\n", sig);
		return;
	}
	if (!fn)
		sa.sa_handler = SIG_IGN;
	else
		sa.sa_handler = (void (*)(int))got_signal;
	sigfillset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	if (!fn)
		EINTRLOOP(rs, sigaction(sig, &sa, NULL));
	signal_handlers[sig].fn = fn;
	signal_handlers[sig].data = data;
	signal_handlers[sig].critical = critical;
	if (fn)
		EINTRLOOP(rs, sigaction(sig, &sa, NULL));
}

void
interruptible_signal(int sig, int in)
{
	struct sigaction sa = sa_zero;
	int rs;
	if (sig >= NUM_SIGNALS || sig < 0) {
		die("bad signal number: %d\n", sig);
		return;
	}
	if (!signal_handlers[sig].fn)
		return;
	sa.sa_handler = (void (*)(int))got_signal;
	sigfillset(&sa.sa_mask);
	if (!in)
		sa.sa_flags = SA_RESTART;
	EINTRLOOP(rs, sigaction(sig, &sa, NULL));
}

static sigset_t sig_old_mask;
static int sig_unblock = 0;

void
block_signals(int except1, int except2)
{
	int rs;
	sigset_t mask;
	sigfillset(&mask);
	if (except1)
		sigdelset(&mask, except1);
	if (except2)
		sigdelset(&mask, except2);
#ifdef SIGILL
	sigdelset(&mask, SIGILL);
#endif
#ifdef SIGABRT
	sigdelset(&mask, SIGABRT);
#endif
#ifdef SIGFPE
	sigdelset(&mask, SIGFPE);
#endif
#ifdef SIGSEGV
	sigdelset(&mask, SIGSEGV);
#endif
#ifdef SIGBUS
	sigdelset(&mask, SIGBUS);
#endif
	EINTRLOOP(rs, sigprocmask(SIG_BLOCK, &mask, &sig_old_mask));
	if (!rs)
		sig_unblock = 1;
}

void
unblock_signals(void)
{
	int rs;
	if (sig_unblock) {
		EINTRLOOP(rs, sigprocmask(SIG_SETMASK, &sig_old_mask, NULL));
		sig_unblock = 0;
	}
}

static int
check_signals(void)
{
	int r = 0;
	int i;
	for (i = 0; i < NUM_SIGNALS; i++)
		if (signal_mask[i]) {
			signal_mask[i] = 0;
			if (signal_handlers[i].fn) {
				pr(signal_handlers[i].fn(
				    signal_handlers[i].data))
				{
				}
			}
			CHK_BH;
			r = 1;
		}
	return r;
}

#ifdef SIGCHLD
static void
sigchld(void *p)
{
	pid_t pid;
	#ifndef WNOHANG
	EINTRLOOP(pid, wait(NULL));
	#else
	do {
		EINTRLOOP(pid, waitpid(-1, NULL, WNOHANG));
	} while (pid > 0);
	#endif
}

void
set_sigcld(void)
{
	install_signal_handler(SIGCHLD, sigchld, NULL, 1);
}
#else
void
set_sigcld(void)
{
}
#endif

void
reinit_child(void)
{
	signal_pid = getpid();
	if (event_enabled) {
		if (event_reinit(event_base))
			die("event_reinit: %s\n", strerror(errno));
	}
}

int terminate_loop = 0;

void
select_loop(void (*init)(void))
{

	memset(&sa_zero, 0, sizeof sa_zero);
	memset((void *)signal_mask, 0, sizeof signal_mask);
	memset((void *)signal_handlers, 0, sizeof signal_handlers);
	FD_ZERO(&w_read);
	FD_ZERO(&w_write);
	w_max = 0;
	last_time = get_time();
	ignore_signals();
	signal_pid = getpid();
	if (c_pipe(signal_pipe))
		die("can't create pipe for signal handling\n");
	set_nonblock(signal_pipe[0]);
	set_nonblock(signal_pipe[1]);
	set_handlers(signal_pipe[0], clear_events_ptr, NULL,
	             (void *)(long)signal_pipe[0]);
	init();
	CHK_BH;
	enable_libevent();
	if (!event_enabled) {
		restrict_fds();
	}
	if (event_enabled) {
		while (!terminate_loop) {
			check_signals();
			do_event_loop(EVLOOP_NONBLOCK);
			check_signals();
			redraw_all_terminals();
			if (terminate_loop)
				break;
			do_event_loop(EVLOOP_ONCE);
		}
	} else

		while (!terminate_loop) {
			volatile int n; /* volatile because of setjmp */
			int i;
			struct timeval tv;
			struct timeval *tm = NULL;
			check_signals();
			check_timers();
			redraw_all_terminals();
			if (!list_empty(timers)) {
				uttime tt =
				    list_struct(timers.next, struct timer)
					->interval
				    + 1;
				tv.tv_sec = tt / 1000 < INT_MAX
				                ? (int)(tt / 1000)
				                : INT_MAX;
				tv.tv_usec = (tt % 1000) * 1000;
				tm = &tv;
			}
			memcpy(&x_read, &w_read, sizeof(fd_set));
			memcpy(&x_write, &w_write, sizeof(fd_set));
			if (terminate_loop)
				break;
			if ((n = select(w_max, &x_read, &x_write, NULL, tm))
			    < 0) {
				if (errno != EINTR)
					die("select: %s\n", strerror(errno));
				continue;
			}
			check_signals();
			check_timers();
			i = -1;
			while (n > 0 && ++i < w_max) {
				int k = 0;
				if (FD_ISSET(i, &x_read)) {
					if (threads[i].read_func) {
						pr(threads[i].read_func(
						    threads[i].data)) continue;
						CHK_BH;
					}
					k = 1;
				}
				if (FD_ISSET(i, &x_write)) {
					if (threads[i].write_func) {
						pr(threads[i].write_func(
						    threads[i].data)) continue;
						CHK_BH;
					}
					k = 1;
				}
				n -= k;
			}
		}
}

void
terminate_select(void)
{
	terminate_libevent();
	free(threads);
}
