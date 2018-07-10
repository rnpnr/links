/* main.c
 * main()
 * (c) 2002 Mikulas Patocka
 * This file is a part of the Links program, released under GPL.
 */

#include "links.h"

int retval = RET_OK;

static void unhandle_basic_signals(struct terminal *);
static void poll_fg(void *);

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
	if (!is_blocked()) kbd_ctrl_c();
}

#ifdef SIGTTOU
static void sig_ign(void *x)
{
}
#endif

static struct timer *fg_poll_timer = NULL;

void sig_tstp(void *t_)
{
	struct terminal *t = (struct terminal *)t_;
#if defined(SIGSTOP) && !defined(NO_CTRL_Z)
#if defined(SIGCONT) && defined(SIGTTOU) && defined(HAVE_GETPID)
	pid_t pid, newpid;
	EINTRLOOP(pid, getpid());
#endif
	if (!F) {
		block_itrm(1);
	}
#ifdef G
	else {
		drv->block(NULL);
	}
#endif
#if defined(SIGCONT) && defined(SIGTTOU) && defined(HAVE_GETPID)
	EINTRLOOP(newpid, fork());
	if (!newpid) {
		while (1) {
			int rr;
			portable_sleep(1000);
			EINTRLOOP(rr, kill(pid, SIGCONT));
		}
	}
#endif
	{
		int rr;
		EINTRLOOP(rr, raise(SIGSTOP));
	}
#if defined(SIGCONT) && defined(SIGTTOU) && defined(HAVE_GETPID)
	if (newpid != -1) {
		int rr;
		EINTRLOOP(rr, kill(newpid, SIGKILL));
	}
#endif
#endif
	if (fg_poll_timer != NULL) kill_timer(fg_poll_timer);
	fg_poll_timer = install_timer(FG_POLL_TIME, poll_fg, t);
}

static void poll_fg(void *t_)
{
	struct terminal *t = (struct terminal *)t_;
	int r;
	fg_poll_timer = NULL;
	if (!F) {
		r = unblock_itrm(1);
#ifdef G
	} else {
		r = drv->unblock(NULL);
#endif
	}
	if (r == -1) {
		fg_poll_timer = install_timer(FG_POLL_TIME, poll_fg, t);
	}
	if (r == -2) {
		/* This will unblock externally spawned viewer, if it exists */
#ifdef SIGCONT
		EINTRLOOP(r, kill(0, SIGCONT));
#endif
	}
}

void sig_cont(void *t_)
{
	if (!F) {
		unblock_itrm(1);
#ifdef G
	} else {
		drv->unblock(NULL);
#endif
	}
}

static void handle_basic_signals(struct terminal *term)
{
	install_signal_handler(SIGHUP, sig_intr, term, 0);
	if (!F) install_signal_handler(SIGINT, sig_ctrl_c, term, 0);
	/*install_signal_handler(SIGTERM, sig_terminate, term, 0);*/
#ifdef SIGTSTP
	if (!F) install_signal_handler(SIGTSTP, sig_tstp, term, 0);
#endif
#ifdef SIGTTIN
	if (!F) install_signal_handler(SIGTTIN, sig_tstp, term, 0);
#endif
#ifdef SIGTTOU
	install_signal_handler(SIGTTOU, sig_ign, term, 0);
#endif
#ifdef SIGCONT
	if (!F) install_signal_handler(SIGCONT, sig_cont, term, 0);
#endif
}

void unhandle_terminal_signals(struct terminal *term)
{
	install_signal_handler(SIGHUP, NULL, NULL, 0);
	if (!F) install_signal_handler(SIGINT, NULL, NULL, 0);
#ifdef SIGTSTP
	install_signal_handler(SIGTSTP, NULL, NULL, 0);
#endif
#ifdef SIGTTIN
	install_signal_handler(SIGTTIN, NULL, NULL, 0);
#endif
#ifdef SIGTTOU
	install_signal_handler(SIGTTOU, NULL, NULL, 0);
#endif
#ifdef SIGCONT
	install_signal_handler(SIGCONT, NULL, NULL, 0);
#endif
	if (fg_poll_timer != NULL) kill_timer(fg_poll_timer), fg_poll_timer = NULL;
}

static void unhandle_basic_signals(struct terminal *term)
{
	install_signal_handler(SIGHUP, NULL, NULL, 0);
	if (!F) install_signal_handler(SIGINT, NULL, NULL, 0);
	/*install_signal_handler(SIGTERM, NULL, NULL, 0);*/
#ifdef SIGTSTP
	install_signal_handler(SIGTSTP, NULL, NULL, 0);
#endif
#ifdef SIGTTIN
	install_signal_handler(SIGTTIN, NULL, NULL, 0);
#endif
#ifdef SIGTTOU
	install_signal_handler(SIGTTOU, NULL, NULL, 0);
#endif
#ifdef SIGCONT
	install_signal_handler(SIGCONT, NULL, NULL, 0);
#endif
	if (fg_poll_timer != NULL) kill_timer(fg_poll_timer), fg_poll_timer = NULL;
}

int terminal_pipe[2] = { -1, -1 };

int attach_terminal(int in, int out, int ctl, void *info, int len)
{
	struct terminal *term;
	set_nonblock(terminal_pipe[0]);
	set_nonblock(terminal_pipe[1]);
	handle_trm(in, out, out, terminal_pipe[1], ctl, info, len);
	mem_free(info);
	if ((term = init_term(terminal_pipe[0], out, win_func))) {
		handle_basic_signals(term);	/* OK, this is race condition, but it must be so; GPM installs it's own buggy TSTP handler */
		return terminal_pipe[1];
	}
	close_socket(&terminal_pipe[0]);
	close_socket(&terminal_pipe[1]);
	return -1;
}

#ifdef G

int attach_g_terminal(unsigned char *cwd, void *info, int len)
{
	struct terminal *term;
	term = init_gfx_term(win_func, cwd, info, len);
	mem_free(info);
	return term ? 0 : -1;
}

void gfx_connection(int h)
{
	int r;
	unsigned char cwd[MAX_CWD_LEN];
	unsigned char hold_conn;
	void *info;
	int info_len;
	struct terminal *term;

	if (os_send_fg_cookie(h))
		goto err_close;
	if (hard_read(h, cwd, MAX_CWD_LEN) != MAX_CWD_LEN)
		goto err_close;
	cwd[MAX_CWD_LEN - 1] = 0;
	if (hard_read(h, &hold_conn, 1) != 1)
		goto err_close;
	if (hard_read(h, (unsigned char *)&info_len, sizeof(int)) != sizeof(int) || info_len < 0)
		goto err_close;
	info = mem_alloc(info_len);
	if (hard_read(h, info, info_len) != info_len)
		goto err_close_free;
	term = init_gfx_term(win_func, cwd, info, info_len);
	if (term) {
		if (hold_conn) {
			term->handle_to_close = h;
		} else {
			hard_write(h, cast_uchar "x", 1);
			EINTRLOOP(r, close(h));
		}
		mem_free(info);
		return;
	}
err_close_free:
	mem_free(info);
err_close:
	EINTRLOOP(r, close(h));
}

static void gfx_connection_terminate(void *p)
{
	int h = (int)(my_intptr_t)p;
	set_handlers(h, NULL, NULL, NULL);
	terminate_loop = 1;
}

#endif

static struct object_request *dump_obj;
static off_t dump_pos;

static void end_dump(struct object_request *r, void *p)
{
	struct cache_entry *ce;
	int oh;
	if (!r->state || (r->state == 1 && dmp != D_SOURCE)) return;
	if ((oh = get_output_handle()) == -1) return;
	ce = r->ce;
	if (dmp == D_SOURCE) {
		if (ce) {
			struct fragment *frag;
			struct list_head *lfrag;
			nextfrag:
			foreach(struct fragment, frag, lfrag, ce->frag) if (frag->offset <= dump_pos && frag->offset + frag->length > dump_pos) {
				off_t l;
				int w;
				l = frag->length - (dump_pos - frag->offset);
				if (l >= MAXINT) l = MAXINT;
				w = hard_write(oh, frag->data + dump_pos - frag->offset, (int)l);
				if (w != l) {
					detach_object_connection(r, dump_pos);
					if (w < 0) fprintf(stderr, "Error writing to stdout: %s.\n", strerror(errno));
					else fprintf(stderr, "Can't write to stdout.\n");
					retval = RET_ERROR;
					goto terminate;
				}
				dump_pos += w;
				detach_object_connection(r, dump_pos);
				goto nextfrag;
			}
		}
		if (r->state >= 0) return;
	} else if (ce) {
		struct document_options o;
		struct f_data_c *fd;
		fd = create_f_data_c(NULL, NULL);
		memset(&o, 0, sizeof(struct document_options));
		o.xp = 0;
		o.yp = 1;
		o.xw = screen_width;
		o.yw = 25;
		o.col = 0;
		o.cp = get_commandline_charset();
		ds2do(&dds, &o, 0);
		o.plain = 0;
		o.frames = 0;
		o.js_enable = 0;
		o.framename = cast_uchar "";
		if (!casecmp(r->url, cast_uchar "file://", 7) && !o.hard_assume) {
			o.assume_cp = get_commandline_charset();
		}
		if (!(fd->f_data = cached_format_html(fd, r, r->url, &o, NULL, 0))) goto term_1;
		dump_to_file(fd->f_data, oh);
		term_1:
		reinit_f_data_c(fd);
		mem_free(fd);
	}
	if (r->state != O_OK) {
		unsigned char *m = get_err_msg(r->stat.state);
		fprintf(stderr, "%s\n", get_english_translation(m));
		retval = RET_ERROR;
		goto terminate;
	}
	terminate:
	terminate_loop = 1;
}

int g_argc;
unsigned char **g_argv;

unsigned char *path_to_exe;

static unsigned char init_b = 0;

static void initialize_all_subsystems(void);
static void initialize_all_subsystems_2(void);

static void fixup_g(void)
{
	if (ggr_drv[0] || ggr_mode[0] || force_g) ggr = 1;
	if (dmp) ggr = 0;
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
	if (!dmp && !ggr) {
		init_os_terminal();
	}
	if ((dds.assume_cp = get_cp_index(cast_uchar "ISO-8859-1")) == -1) dds.assume_cp = 0;
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
#ifdef G
			{
				unsigned char *r;
				if ((r = init_graphics(ggr_drv, ggr_mode, ggr_display))) {
					fprintf(stderr, "%s", r);
					mem_free(r);
					retval = RET_SYNTAX;
					goto ttt;
				}
				handle_basic_signals(NULL);
				if (drv->get_af_unix_name && !no_connect) {
					unsigned char *n = stracpy(drv->name);
					unsigned char *nn = drv->get_af_unix_name();
					if (*nn) {
						add_to_strn(&n, cast_uchar "-");
						add_to_strn(&n, nn);
					}
					mem_free(n);
				}
				spawn_font_thread();
				init_dither(drv->depth);
			}
#else
			fprintf(stderr, "Graphics not enabled when compiling\n");
			retval = RET_SYNTAX;
			goto ttt;
#endif
		}
		init_b = 1;
		init_bookmarks();
		create_initial_extensions();
		load_url_history();
		initialize_all_subsystems_2();
		info = create_session_info(base_session, u, default_target, &len);
		if (!F) {
			if (attach_terminal(get_input_handle(), get_output_handle(), get_ctl_handle(), info, len) < 0)
				fatal_exit("Could not open initial session");
		}
#ifdef G
		else {
			unsigned char *cwd = get_cwd();
			if (!cwd)
				cwd = stracpy(cast_uchar "");
			if (attach_g_terminal(cwd, info, len) < 0)
				fatal_exit("Could not open initial session");
			mem_free(cwd);
		}
#endif
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
		uu = convert(get_commandline_charset(), utf8_table, u, NULL);
		if (!(uuu = translate_url(uu, wd = get_cwd()))) uuu = stracpy(uu);
		mem_free(uu);
		request_object(NULL, uuu, NULL, PRI_MAIN, NC_RELOAD, ALLOW_ALL, end_dump, NULL, &dump_obj);
		mem_free(uuu);
		if (wd) mem_free(wd);
	}
}

/* Is called before gaphics driver init */
static void initialize_all_subsystems(void)
{
	init_charset();
	init_trans();
	set_sigcld();
	init_home();
	init_dns();
	init_session_cache();
	init_cache();
	init_blocks();
	memset(&dd_opt, 0, sizeof dd_opt);
}

/* Is called sometimes after and sometimes before graphics driver init */
static void initialize_all_subsystems_2(void)
{
	GF(init_dip());
	init_bfu();
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
	shutdown_bfu();
	if (!F) free_all_itrms();
	release_object(&dump_obj);
	abort_all_connections();

	free_all_caches();
	free_format_text_cache();
#ifdef HAVE_SSL
	ssl_finish();
#endif
	if (init_b) save_url_history();
	free_history_lists();
	free_term_specs();
	free_types();
	free_blocks();
	finalize_bookmarks();
	free_conv_table();
	free_blacklist();
	free_cookies();
	free_auth();
	check_bottom_halves();
	end_config();
	free_strerror_buf();
	shutdown_trans();
	GF(free_dither());
	GF(shutdown_graphics());
	os_free_clipboard();
	if (fg_poll_timer != NULL) kill_timer(fg_poll_timer), fg_poll_timer = NULL;
	terminate_select();
	terminate_osdep();
}

int main(int argc, char *argv[])
{
	g_argc = argc;
	g_argv = (unsigned char **)argv;

#if 0
	if (argc != 3)
		fprintf(stderr, "two args expected\n"), exit(1);
	printf("cookie %saccepted\n", allow_cookie_domain(cast_uchar argv[1], cast_uchar argv[2]) ? "" : "not ");
	return 0;
#endif

	init_heap();

	init_os();

	get_path_to_exe();

#if 0
	{
		int i;
		int ix, iy, ox, oy, rep;
		ulonglong tm = 0;
		parse_options(g_argc - 1, g_argv + 1);
		ix = getenv("SRC_X") ? atoi(getenv("SRC_X")) : 100;
		iy = getenv("SRC_Y") ? atoi(getenv("SRC_Y")) : ix;
		ox = getenv("DST_X") ? atoi(getenv("DST_X")) : 100;
		oy = getenv("DST_Y") ? atoi(getenv("DST_Y")) : ox;
		rep = getenv("REP") ? atoi(getenv("REP")) : 1;
		for (i = 0; i <= rep; i++) {
			unsigned short *dst;
			unsigned short *src;
			struct timeval tv1, tv2;
			src = mem_alloc(sizeof(unsigned short) * ix * iy * 3);
			memset(src, 0x12, sizeof(unsigned short) * ix * iy * 3);
			gettimeofday(&tv1, NULL);
			scale_color(src, ix, iy, &dst, ox, oy);
			gettimeofday(&tv2, NULL);
			if (dst) mem_free(dst);
			if (i)
				tm += ((ulonglong)tv2.tv_sec * 1000000 + tv2.tv_usec) - ((ulonglong)tv1.tv_sec * 1000000 + tv1.tv_usec);
		}
		fprintf(stderr, "time: %f\n", (double)tm / 1000 / rep);
		check_memory_leaks();
		return 0;
	}
#endif
#if 0
	{
		int i;
		for (i = 0; i < 100; i++) {
			unsigned char *a = mem_calloc(i);
			unsigned char *b = base64_encode(a, i, cast_uchar "", cast_uchar "", 5);
			fprintf(stderr, "X:\n%.*s\n", (int)strlen(cast_const_char b), b);
			mem_free(a);
			mem_free(b);
		}
		check_memory_leaks();
		return 0;
	}
#endif
#if 0
	{
		unsigned char *puny_encode(unsigned char *s);
		unsigned char *puny_decode(unsigned char *s);
		int i;
		for (i = 1; i < g_argc; i++) {
			unsigned char *str = puny_encode(g_argv[i]);
			if (str) {
				fprintf(stderr, "'%s'\n", str);
				unsigned char *d = puny_decode(str);
				if (!d || strcmp(cast_const_char d, cast_const_char g_argv[i])) {
					internal("mismatch - %s", d);
				}
				mem_free(str);
				mem_free(d);
			}
		}
		check_memory_leaks();
		return 0;
	}
#endif
#if 0
	while (1) {
#define maxn	12
		static unsigned long long count = 0;
		unsigned char *puny_encode(unsigned char *s, int len);
		unsigned char *puny_decode(unsigned char *s, int len);
		int punycode_encode(size_t input_length, const uint32_t input[], const unsigned char case_flags[], size_t *output_length, char output[]);
		int punycode_decode(size_t input_length, const char input[], size_t * output_length, uint32_t output[], unsigned char case_flags[]);
		uint32_t unistr[maxn];
		unsigned i, n;
		unsigned char *utfstr, *puny, *dec;
		int utfstr_l;
		char pce_o[60];
		size_t pce_ol;
		int pce_s;
		n = random() % (maxn + 1);
		for (i = 0; i < n; i++) {
			uint32_t uni;
			uni = random() % (0x10FFFF + 1);
			if (uni >= 0xd800 && uni <= 0xdfff)
				uni = random() % 128;
			while (uni < 128 && !(
				uni == '-' ||
				(uni >= '0' && uni <= '9') ||
				(uni >= 'A' && uni <= 'Z') ||
				(uni >= 'a' && uni <= 'z'))) {
				uni = random() % 128;
			}
			unistr[i] = uni;
		}
		utfstr = init_str();
		utfstr_l = 0;
		for (i = 0; i < n; i++) {
			unsigned char *us = encode_utf_8(unistr[i]);
			add_to_str(&utfstr, &utfstr_l, us);
		}
		puny = puny_encode(utfstr, utfstr_l);
		if (!puny) {
			fprintf(stderr, "failed:");
			goto err;
		}
		dec = puny_decode(puny, (int)strlen(cast_const_char puny));
		if (!dec)
			dec = stracpy(puny);
		if (strcmp(cast_const_char utfstr, cast_const_char dec)) {
			fprintf(stderr, "mismatch(%s,%s):", utfstr, dec);
			goto err;
		}
		pce_ol = 59;
		pce_s = punycode_encode(n, unistr, NULL, &pce_ol, pce_o);
		if (pce_s) {
			fprintf(stderr, "punycode_encode(%d):", pce_s);
			goto err;
		}
		pce_o[pce_ol] = 0;
		if (!strncmp(cast_const_char puny, "xn--", 4) && strcmp(cast_const_char puny + 4, pce_o)) {
			fprintf(stderr, "punycode_encode differs(%s,%s):", puny, pce_o);
			goto err;
		}
		mem_free(dec);
		mem_free(puny);
		mem_free(utfstr);
		if (0) {
err:
			for (i = 0; i < n; i++) {
				fprintf(stderr, " %u", unistr[i]);
			}
			internal("failed");
		}
		count++;
		if (!(count % 10000)) {
			printf("%llu\r", count);
			fflush(stdout);
		}
	}
#endif

	select_loop(init);
	terminate_all_subsystems();

	check_memory_leaks();
	return retval;
}

