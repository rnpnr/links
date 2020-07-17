/* main.c
 * main()
 * (c) 2002 Mikulas Patocka
 * This file is a part of the Links program, released under GPL.
 */

#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __OpenBSD__
#include <unistd.h>
#else
#define pledge(a,b) 0
#endif

#include "links.h"

int retval = RET_OK;

static void initialize_all_subsystems(void);
static void initialize_all_subsystems_2(void);
static void poll_fg(void *);
static void unhandle_basic_signals(struct terminal *);

static int init_b = 0;
int g_argc;
const char *argv0;
char **g_argv;

void
die(const char *errstr, ...)
{
	va_list ap;

	va_start(ap, errstr);
	vfprintf(stderr, errstr, ap);
	va_end(ap);
	exit(1);
}

void
usage(void)
{
	die("usage: %s [options] [url]\n", argv0);
}

void *
xmalloc(size_t len)
{
	void *p;

	if (!(p = malloc(len)))
		die("malloc: %s\n", strerror(errno));

	return p;
}

void *
xrealloc(void *p, size_t len)
{
	if (!(p = realloc(p, len)))
		die("realloc: %s\n", strerror(errno));

	return p;
}

static void sig_intr(void *t_)
{
	struct terminal *t = (struct terminal *)t_;
	if (!t) {
		unhandle_basic_signals(t);
		terminate_loop = 1;
	} else {
		unhandle_basic_signals(t);
		exit_prog(t, NULL, NULL);
	}
}

static void sig_ctrl_c(void *t_)
{
	if (!is_blocked())
		kbd_ctrl_c();
}

static void sig_ign(void *x)
{
}

static struct timer *fg_poll_timer = NULL;

void sig_tstp(void *t_)
{
	struct terminal *t = (struct terminal *)t_;
	pid_t pid, newpid;
	EINTRLOOP(pid, getpid());
	if (!F)
		block_itrm(1);
	EINTRLOOP(newpid, fork());
	if (!newpid) {
		while (1) {
			int rr;
			portable_sleep(1000);
			EINTRLOOP(rr, kill(pid, SIGCONT));
		}
	}
	{
		int rr;
		EINTRLOOP(rr, raise(SIGSTOP));
	}
	if (newpid != -1) {
		int rr;
		EINTRLOOP(rr, kill(newpid, SIGKILL));
	}
	if (fg_poll_timer != NULL) kill_timer(fg_poll_timer);
	fg_poll_timer = install_timer(FG_POLL_TIME, poll_fg, t);
}

static void poll_fg(void *t_)
{
	struct terminal *t = (struct terminal *)t_;
	int r;
	fg_poll_timer = NULL;
	if (!F)
		r = unblock_itrm(1);
	if (r == -1)
		fg_poll_timer = install_timer(FG_POLL_TIME, poll_fg, t);
	if (r == -2) {
		/* This will unblock externally spawned viewer, if it exists */
		EINTRLOOP(r, kill(0, SIGCONT));
	}
}

void sig_cont(void *t_)
{
	if (!F)
		unblock_itrm(1);
}

static void handle_basic_signals(struct terminal *term)
{
	install_signal_handler(SIGHUP, sig_intr, term, 0);
	if (!F) install_signal_handler(SIGINT, sig_ctrl_c, term, 0);
	/*install_signal_handler(SIGTERM, sig_terminate, term, 0);*/
	if (!F) install_signal_handler(SIGTSTP, sig_tstp, term, 0);
	if (!F) install_signal_handler(SIGTTIN, sig_tstp, term, 0);
	install_signal_handler(SIGTTOU, sig_ign, term, 0);
	if (!F) install_signal_handler(SIGCONT, sig_cont, term, 0);
}

void unhandle_terminal_signals(struct terminal *term)
{
	install_signal_handler(SIGHUP, NULL, NULL, 0);
	if (!F) install_signal_handler(SIGINT, NULL, NULL, 0);
	install_signal_handler(SIGTSTP, NULL, NULL, 0);
	install_signal_handler(SIGTTIN, NULL, NULL, 0);
	install_signal_handler(SIGTTOU, NULL, NULL, 0);
	install_signal_handler(SIGCONT, NULL, NULL, 0);
	if (fg_poll_timer != NULL) {
		kill_timer(fg_poll_timer);
		fg_poll_timer = NULL;
	}
}

static void unhandle_basic_signals(struct terminal *term)
{
	install_signal_handler(SIGHUP, NULL, NULL, 0);
	if (!F) install_signal_handler(SIGINT, NULL, NULL, 0);
	/*install_signal_handler(SIGTERM, NULL, NULL, 0);*/
	install_signal_handler(SIGTSTP, NULL, NULL, 0);
	install_signal_handler(SIGTTIN, NULL, NULL, 0);
	install_signal_handler(SIGTTOU, NULL, NULL, 0);
	install_signal_handler(SIGCONT, NULL, NULL, 0);
	if (fg_poll_timer != NULL) {
		kill_timer(fg_poll_timer);
		fg_poll_timer = NULL;
	}
}

int terminal_pipe[2] = { -1, -1 };

int attach_terminal(void *info, int len)
{
	struct terminal *term;
	set_nonblock(terminal_pipe[0]);
	set_nonblock(terminal_pipe[1]);
	handle_trm(terminal_pipe[1], info, len);
	free(info);
	if ((term = init_term(terminal_pipe[0], 1, win_func))) {
		handle_basic_signals(term);	/* OK, this is race condition, but it must be so; GPM installs it's own buggy TSTP handler */
		return terminal_pipe[1];
	}
	close_socket(&terminal_pipe[0]);
	close_socket(&terminal_pipe[1]);
	return -1;
}

static struct object_request *dump_obj;
static off_t dump_pos;

static void end_dump(struct object_request *r, void *p)
{
	struct cache_entry *ce;
	if (!r->state || (r->state == 1 && dmp != D_SOURCE)) return;
	ce = r->ce;
	if (dmp == D_SOURCE) {
		if (ce) {
			struct fragment *frag = NULL;
			struct list_head *lfrag;
			nextfrag:
			foreach(struct fragment, frag, lfrag, ce->frag) if (frag->offset <= dump_pos && frag->offset + frag->length > dump_pos) {
				off_t l;
				int w;
				l = frag->length - (dump_pos - frag->offset);
				if (l >= INT_MAX)
					l = INT_MAX;
				w = hard_write(1, frag->data + dump_pos - frag->offset, (int)l);
				if (w != l) {
					detach_object_connection(r, dump_pos);
					if (w < 0)
						fprintf(stderr, "Error writing to stdout: %s.\n", strerror(errno));
					else
						fprintf(stderr, "Can't write to stdout.\n");
					retval = RET_ERROR;
					goto terminate;
				}
				dump_pos += w;
				detach_object_connection(r, dump_pos);
				goto nextfrag;
			}
		}
		if (r->state >= 0)
			return;
	} else if (ce) {
		struct document_options o;
		struct f_data_c *fd;
		int err;
		fd = create_f_data_c(NULL, NULL);
		memset(&o, 0, sizeof(struct document_options));
		o.xp = 0;
		o.yp = 1;
		o.xw = screen_width;
		o.yw = 25;
		o.col = 0;
		o.cp = 0;
		ds2do(&dds, &o, 0);
		o.plain = 0;
		o.frames = 0;
		o.framename = cast_uchar "";
		if (!casecmp(r->url, cast_uchar "file://", 7) && !o.hard_assume) {
			o.assume_cp = 0;
		}
		if (!(fd->f_data = cached_format_html(fd, r, r->url, &o, NULL, 0))) goto term_1;
		if ((err = dump_to_file(fd->f_data, 1))) {
			fprintf(stderr, "Error writing to stdout: %s.\n", get_err_msg(err));
			retval = RET_ERROR;
		}
		term_1:
		reinit_f_data_c(fd);
		free(fd);
	}
	if (r->state != O_OK) {
		unsigned char *m = get_err_msg(r->stat.state);
		fprintf(stderr, "%s\n", get_text_translation(m, NULL));
		retval = RET_ERROR;
		goto terminate;
	}
	terminate:
	terminate_loop = 1;
}

static void fixup_g(void)
{
	if (ggr_drv[0] || ggr_mode[0] || force_g)
		ggr = 1;
	if (dmp)
		ggr = 0;
}

static void init(void)
{
	void *info;
	int len;
	unsigned char *u;

	initialize_all_subsystems();

/* OS/2 has some stupid bug and the pipe must be created before socket :-/ */
	if (c_pipe(terminal_pipe)) {
		fatal_exit("ERROR: can't create pipe for internal communication");
	}
	if (!(u = parse_options(g_argc - 1, g_argv + 1))) {
		retval = RET_SYNTAX;
		goto ttt;
	}
	fixup_g();
	dds.assume_cp = 0;
	load_config();
	if (proxies.only_proxies)
		reset_settings_for_tor();
	u = parse_options(g_argc - 1, g_argv + 1);
	fixup_g();
	if (!u) {
		ttt:
		initialize_all_subsystems_2();
		tttt:
		terminate_loop = 1;
		return;
	}
	init_cookies();
	if (!dmp) {
		if (ggr) {
			close_socket(&terminal_pipe[0]);
			close_socket(&terminal_pipe[1]);
			fprintf(stderr, "Graphics not enabled when compiling\n");
			retval = RET_SYNTAX;
			goto ttt;
		}
		init_b = 1;
		init_bookmarks();
		create_initial_extensions();
		load_url_history();
		initialize_all_subsystems_2();
		info = create_session_info(base_session, u, default_target, &len);
		if (!F) {
			if (attach_terminal(info, len) < 0)
				fatal_exit("Could not open initial session");
		}
	} else {
		unsigned char *uu, *uuu, *wd;
		initialize_all_subsystems_2();
		close_socket(&terminal_pipe[0]);
		close_socket(&terminal_pipe[1]);
		if (!*u) {
			fprintf(stderr, "URL expected after %s\n", dmp == D_DUMP ? "-dump" : "-source");
			retval = RET_SYNTAX;
			goto tttt;
		}
		uu = stracpy(u);
		if (!(uuu = translate_url(uu, wd = get_cwd()))) uuu = stracpy(uu);
		free(uu);
		request_object(NULL, uuu, NULL, PRI_MAIN, NC_RELOAD, ALLOW_ALL, end_dump, NULL, &dump_obj);
		free(uuu);
		free(wd);
	}
}

/* Is called before gaphics driver init */
static void initialize_all_subsystems(void)
{
	set_sigcld();
	init_home();
	init_dns();
	init_session_cache();
	init_cache();
	memset(&dd_opt, 0, sizeof dd_opt);
}

/* Is called sometimes after and sometimes before graphics driver init */
static void initialize_all_subsystems_2(void)
{
	GF(init_dip());
	GF(init_imgcache());
	init_fcache();
	GF(init_grview());
}

static void terminate_all_subsystems(void)
{
	check_bottom_halves();
	abort_all_downloads();
	check_bottom_halves();
	destroy_all_terminals();
	check_bottom_halves();
	if (!F)
		free_all_itrms();
	release_object(&dump_obj);
	abort_all_connections();

	free_all_caches();
	ssl_finish();
	if (init_b)
		save_url_history();
	free_history_lists();
	free_term_specs();
	free_types();
	finalize_bookmarks();
	free_conv_table();
	free_blacklist();
	free_cookies();
	free_auth();
	check_bottom_halves();
	end_config();
	free_strerror_buf();
	GF(free_dither());
	GF(shutdown_graphics());
	os_free_clipboard();
	if (fg_poll_timer != NULL) {
		kill_timer(fg_poll_timer);
		fg_poll_timer = NULL;
	}
	terminate_select();
}

int
main(int argc, char *argv[])
{
	g_argc = argc;
	g_argv = argv;
	argv0 = argv[0];

	if (pledge("stdio rpath wpath cpath inet dns tty unix", NULL) < 0)
		die("pledge: %s\n", strerror(errno));

	init_page_size();
	select_loop(init);
	terminate_all_subsystems();

	return retval;
}
