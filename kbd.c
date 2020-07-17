/* kbd.c
 * (c) 2002 Mikulas Patocka
 * This file is a part of the Links program, released under GPL.
 */

#include <limits.h>

#include "links.h"

#define OUT_BUF_SIZE		10240
#define IN_BUF_SIZE		64

#define USE_TWIN_MOUSE	1
#define BRACKETED_PASTE	2

#define TW_BUTT_LEFT	1
#define TW_BUTT_MIDDLE	2
#define TW_BUTT_RIGHT	4

struct itrm {
	int std_in;
	int std_out;
	int sock_in;
	int sock_out;
	int ctl_in;
	int blocked;
	int flags;
	unsigned char kqueue[IN_BUF_SIZE];
	int qlen;
	struct timer *tm;
	void (*queue_event)(struct itrm *, unsigned char *, int);
	unsigned char *ev_queue;
	int eqlen;
	void *mouse_h;
	unsigned char *orig_title;
	void (*free_trm)(struct itrm *);
};

static void free_trm(struct itrm *);
static void in_kbd(void *);

static struct itrm *ditrm = NULL;


int is_blocked(void)
{
	return ditrm && ditrm->blocked;
}

void free_all_itrms(void)
{
	if (ditrm) ditrm->free_trm(ditrm);
}

static void itrm_error(void *itrm_)
{
	struct itrm *itrm = (struct itrm *)itrm_;
	itrm->free_trm(itrm);
	terminate_loop = 1;
}

static void write_ev_queue(void *itrm_)
{
	struct itrm *itrm = (struct itrm *)itrm_;
	int l, to_write;
	if (!itrm->eqlen) internal("event queue empty");
	to_write = itrm->eqlen;
retry:
	EINTRLOOP(l, (int)write(itrm->sock_out, itrm->ev_queue, to_write));
	if (l <= 0) {
		if (to_write > 1) {
			to_write >>= 1;
			goto retry;
		}
		itrm_error(itrm);
	}
	memmove(itrm->ev_queue, itrm->ev_queue + l, itrm->eqlen -= l);
	if (!itrm->eqlen) set_handlers(itrm->sock_out, get_handler(itrm->sock_out, H_READ), NULL, get_handler_data(itrm->sock_out));
}

static void queue_event(struct itrm *itrm, unsigned char *data, int len)
{
	int w = 0;
	if (!len) return;
	if (!itrm->eqlen && can_write(itrm->sock_out)) {
		int to_write = len;
retry:
		EINTRLOOP(w, (int)write(itrm->sock_out, data, to_write));
		if (w <= 0) {
			if (to_write > 1) {
				to_write >>= 1;
				goto retry;
			}
			register_bottom_half(itrm_error, itrm);
			return;
		}
	}
	if (w < len) {
		if ((unsigned)itrm->eqlen + (unsigned)(len - w) > INT_MAX)
			overalloc();
		itrm->ev_queue = xrealloc(itrm->ev_queue, itrm->eqlen + len - w);
		memcpy(itrm->ev_queue + itrm->eqlen, data + w, len - w);
		itrm->eqlen += len - w;
		set_handlers(itrm->sock_out, get_handler(itrm->sock_out, H_READ), write_ev_queue, itrm);
	}
}

void kbd_ctrl_c(void)
{
	struct links_event ev = { EV_KBD, KBD_CTRL_C, 0, 0 };
	if (ditrm) ditrm->queue_event(ditrm, (unsigned char *)&ev, sizeof(struct links_event));
}

unsigned char init_seq[] = "\033)0\0337";
unsigned char init_seq_x_mouse[] = "\033[?1000h\033[?1002h\033[?1005l\033[?1015l\033[?1006h\033[?2004h";
unsigned char init_seq_tw_mouse[] = "\033[?9h";
unsigned char term_seq[] = "\033[2J\0338\r \b";
unsigned char term_seq_x_mouse[] = "\033[?1000l\r       \r\033[?1002l\r       \r\033[?1006l\r       \r\033[?2004l\r       \r";
unsigned char term_seq_tw_mouse[] = "\033[?9l";

static void send_init_sequence(int h, int flags)
{
	hard_write(h, init_seq, (int)strlen(cast_const_char init_seq));
	if (flags & USE_TWIN_MOUSE)
		hard_write(h, init_seq_tw_mouse, (int)strlen(cast_const_char init_seq_tw_mouse));
	else
		hard_write(h, init_seq_x_mouse, (int)strlen(cast_const_char init_seq_x_mouse));
}

static void send_term_sequence(int h, int flags)
{
	hard_write(h, term_seq, (int)strlen(cast_const_char term_seq));
	if (flags & USE_TWIN_MOUSE)
		hard_write(h, term_seq_tw_mouse, (int)strlen(cast_const_char term_seq_tw_mouse));
	else
		hard_write(h, term_seq_x_mouse, (int)strlen(cast_const_char term_seq_x_mouse));
}

static void resize_terminal(int x, int y)
{
	struct links_event ev = { EV_RESIZE, 0, 0, 0 };
	ev.x = x;
	ev.y = y;
	queue_event(ditrm, (unsigned char *)&ev, sizeof(struct links_event));
}

static void os_cfmakeraw(struct termios *t)
{
	cfmakeraw(t);
#ifdef VMIN
	t->c_cc[VMIN] = 1;
#endif
#if defined(NO_CTRL_Z) && defined(VSUSP)
	t->c_cc[VSUSP] = 0;
#endif
}

static int ttcgetattr(int fd, struct termios *t)
{
	int r;
	block_signals(
#ifdef SIGTTOU
		SIGTTOU
#else
		0
#endif
		,
#ifdef SIGTTIN
		SIGTTIN
#else
		0
#endif
		);
#ifdef SIGTTOU
	interruptible_signal(SIGTTOU, 1);
#endif
#ifdef SIGTTIN
	interruptible_signal(SIGTTIN, 1);
#endif
	r = tcgetattr(fd, t);
#ifdef SIGTTOU
	interruptible_signal(SIGTTOU, 0);
#endif
#ifdef SIGTTIN
	interruptible_signal(SIGTTIN, 0);
#endif
	unblock_signals();
	return r;
}

static int ttcsetattr(int fd, int a, struct termios *t)
{
	int r;
	block_signals(
#ifdef SIGTTOU
		SIGTTOU
#else
		0
#endif
		,
#ifdef SIGTTIN
		SIGTTIN
#else
		0
#endif
		);
#ifdef SIGTTOU
	interruptible_signal(SIGTTOU, 1);
#endif
#ifdef SIGTTIN
	interruptible_signal(SIGTTIN, 1);
#endif
	r = tcsetattr(fd, a, t);
#ifdef SIGTTOU
	interruptible_signal(SIGTTOU, 0);
#endif
#ifdef SIGTTIN
	interruptible_signal(SIGTTIN, 0);
#endif
	unblock_signals();
	return r;
}

static struct termios saved_termios;

static int setraw(int ctl, int save)
{
	struct termios t;
	if (ctl < 0) return 0;
	memset(&t, 0, sizeof(struct termios));
	if (ttcgetattr(ctl, &t)) {
		/*fprintf(stderr, "getattr result %s\n", strerror(errno));*/
		/* If the terminal was destroyed (the user logged off),
		 * we fake success here so that we can destroy the terminal
		 * later.
		 *
		 * Linux returns EIO
		 * FreeBSD returns ENXIO
		 */
		if (errno == EIO || errno == ENXIO) return 0;
		return -1;
	}
	if (save) memcpy(&saved_termios, &t, sizeof(struct termios));
	os_cfmakeraw(&t);
	t.c_lflag |= ISIG;
#ifdef TOSTOP
	t.c_lflag |= TOSTOP;
#endif
	t.c_oflag |= OPOST;
	if (ttcsetattr(ctl, TCSANOW, &t)) {
		return -1;
	}
	return 0;
}

static void setcooked(int ctl)
{
	if (ctl < 0) return;
	ttcsetattr(ctl, TCSANOW, &saved_termios);
}

void handle_trm(int sock_out, void *init_string, int init_len)
{
	struct itrm *itrm;
	struct links_event ev = { EV_INIT, 0, 0, 0 };
	unsigned char *ts;
	int xwin, def_charset;
	itrm = xmalloc(sizeof(struct itrm));
	itrm->queue_event = queue_event;
	itrm->free_trm = free_trm;
	ditrm = itrm;
	itrm->std_in = 0;
	itrm->std_out = 1;
	itrm->sock_in = 1;
	itrm->sock_out = sock_out;
	itrm->ctl_in = 0;
	itrm->blocked = 0;
	itrm->qlen = 0;
	itrm->tm = NULL;
	itrm->ev_queue = NULL;
	itrm->eqlen = 0;
	setraw(itrm->ctl_in, 1);
	set_handlers(0, in_kbd, NULL, itrm);
	handle_terminal_resize(resize_terminal, &ev.x, &ev.y);
	queue_event(itrm, (unsigned char *)&ev, sizeof(struct links_event));
	xwin = is_xterm() * ENV_XWIN + is_screen() * ENV_SCREEN;
	itrm->flags = 0;
	if (!(ts = cast_uchar getenv("TERM"))) ts = cast_uchar "";
	if (strlen(cast_const_char ts) >= MAX_TERM_LEN) queue_event(itrm, ts, MAX_TERM_LEN);
	else {
		unsigned char *mm;
		int ll = MAX_TERM_LEN - (int)strlen(cast_const_char ts);
		queue_event(itrm, ts, (int)strlen(cast_const_char ts));
		mm = mem_calloc(ll);
		queue_event(itrm, mm, ll);
		free(mm);
	}
	if (!(ts = get_cwd())) ts = stracpy(cast_uchar "");
	if (strlen(cast_const_char ts) >= MAX_CWD_LEN) queue_event(itrm, ts, MAX_CWD_LEN);
	else {
		unsigned char *mm;
		int ll = MAX_CWD_LEN - (int)strlen(cast_const_char ts);
		queue_event(itrm, ts, (int)strlen(cast_const_char ts));
		mm = mem_calloc(ll);
		queue_event(itrm, mm, ll);
		free(mm);
	}
	free(ts);
	queue_event(itrm, (unsigned char *)&xwin, sizeof(int));
	def_charset = 0;
	queue_event(itrm, (unsigned char *)&def_charset, sizeof(int));
	queue_event(itrm, (unsigned char *)&init_len, sizeof(int));
	queue_event(itrm, (unsigned char *)init_string, init_len);
	itrm->orig_title = get_window_title();
	set_window_title(cast_uchar "Links");
	itrm->mouse_h = NULL;
	send_init_sequence(1, itrm->flags);
}

int unblock_itrm(int fd)
{
	struct itrm *itrm = ditrm;
	int x, y;
	if (!itrm) return -1;
	if (setraw(itrm->ctl_in, 0)) return -1;
	if (itrm->blocked != fd + 1) return -2;
	itrm->blocked = 0;
	send_init_sequence(itrm->std_out, itrm->flags);
	set_handlers(itrm->std_in, in_kbd, NULL, itrm);
	handle_terminal_resize(resize_terminal, &x, &y);
	itrm->mouse_h = NULL;
	resize_terminal(x, y);
	return 0;
}

void block_itrm(int fd)
{
	struct itrm *itrm = ditrm;
	if (!itrm) return;
	if (itrm->blocked) return;
	itrm->blocked = fd + 1;
	unhandle_terminal_resize();
	itrm->mouse_h = NULL;
	send_term_sequence(itrm->std_out, itrm->flags);
	setcooked(itrm->ctl_in);
	set_handlers(itrm->std_in, NULL, NULL, itrm);
}

static void free_trm(struct itrm *itrm)
{
	if (!itrm) return;
	set_window_title(itrm->orig_title);
	free(itrm->orig_title);
	itrm->orig_title = NULL;
	unhandle_terminal_resize();
	send_term_sequence(itrm->std_out, itrm->flags);
	setcooked(itrm->ctl_in);
	set_handlers(itrm->std_in, NULL, NULL, NULL);
	set_handlers(itrm->sock_in, NULL, NULL, NULL);
	set_handlers(itrm->std_out, NULL, NULL, NULL);
	set_handlers(itrm->sock_out, NULL, NULL, NULL);
	unregister_bottom_half(itrm_error, itrm);
	if (itrm->tm != NULL) kill_timer(itrm->tm);
	free(itrm->ev_queue);
	free(itrm);
	if (itrm == ditrm) ditrm = NULL;
}

static void refresh_terminal_size(void)
{
	int new_x, new_y;
	if (!ditrm->blocked) {
		unhandle_terminal_resize();
		handle_terminal_resize(resize_terminal, &new_x, &new_y);
		resize_terminal(new_x, new_y);
	}
}

static void resize_terminal_x(unsigned char *text)
{
	unsigned char *p;
	if (!(p = cast_uchar strchr(cast_const_char text, ','))) return;
	*p++ = 0;
	refresh_terminal_size();
}

void dispatch_special(unsigned char *text)
{
	switch (text[0]) {
		case TERM_FN_TITLE:
			set_window_title(text + 1);
			break;
		case TERM_FN_RESIZE:
			resize_terminal_x(text + 1);
			break;
	}
}

static int process_queue(struct itrm *);
static int get_esc_code(unsigned char *, int, unsigned char *, int *, int *);

static void kbd_timeout(void *itrm_)
{
	struct itrm *itrm = (struct itrm *)itrm_;
	struct links_event ev = { EV_KBD, KBD_ESC, 0, 0 };
	unsigned char code;
	int num;
	int len = 0;	/* against warning */
	itrm->tm = NULL;
	if (can_read(itrm->std_in)) {
		in_kbd(itrm);
		return;
	}
	if (!itrm->qlen) {
		internal("timeout on empty queue");
		return;
	}
	if (itrm->kqueue[0] != 27) {
		len = 1;
		goto skip_esc;
	}
	itrm->queue_event(itrm, (unsigned char *)&ev, sizeof(struct links_event));
	if (get_esc_code(itrm->kqueue, itrm->qlen, &code, &num, &len)) len = 1;
 skip_esc:
	itrm->qlen -= len;
	memmove(itrm->kqueue, itrm->kqueue + len, itrm->qlen);
	while (process_queue(itrm))
		;
}

static int get_esc_code(unsigned char *str, int len, unsigned char *code, int *num, int *el)
{
	int pos;
	*num = 0;
	for (pos = 2; pos < len; pos++) {
		if (str[pos] < '0' || str[pos] > '9' || pos > 7) {
			*el = pos + 1;
			*code = str[pos];
			return 0;
		}
		*num = *num * 10 + str[pos] - '0';
	}
	return -1;
}

static int xterm_button = -1;

static int process_queue(struct itrm *itrm)
{
	struct links_event ev = { EV_KBD, -1, 0, 0 };
	int el = 0;
	if (!itrm->qlen) goto end;
	if (itrm->kqueue[0] == '\033') {
		if (itrm->qlen < 2) goto ret;
		if (itrm->kqueue[1] == '[' || itrm->kqueue[1] == 'O') {
			unsigned char c = 0;
			int v;
			if (itrm->qlen >= 4 && itrm->kqueue[2] == '[') {
				if (itrm->kqueue[3] < 'A' || itrm->kqueue[3] > 'L') goto ret;
				ev.x = KBD_F1 - (itrm->kqueue[3] - 'A');
				el = 4;
			} else if (get_esc_code(itrm->kqueue, itrm->qlen, &c, &v, &el)) goto ret;
			else switch (c) {
				case 'L':
				case '@': ev.x = KBD_INS; break;
				case 'A': ev.x = KBD_UP; break;
				case 'B': ev.x = KBD_DOWN; break;
				case 'C': ev.x = KBD_RIGHT; break;
				case 'D': ev.x = KBD_LEFT; break;
				case 'F':
				case 'K':
				case 'e': ev.x = KBD_END; break;
				case 'H':
				case 0: ev.x = KBD_HOME; break;
				case 'V':
				case 'I': ev.x = KBD_PAGE_UP; break;
				case 'U':
				case 'G': ev.x = KBD_PAGE_DOWN; break;
				case 'P':
					ev.x = KBD_F1; break;
				case 'Q': ev.x = KBD_F2; break;
				case 'S':
					ev.x = KBD_F4; break;
				case 'T':
					ev.x = KBD_F5; break;
				case 'W': ev.x = KBD_F8; break;
				case 'X': ev.x = KBD_F9; break;
				case 'Y':
					ev.x = KBD_F11;
					break;

				case 'q': switch (v) {
					case 139: ev.x = KBD_INS; break;
					case 146: ev.x = KBD_END; break;
					case 150: ev.x = KBD_PAGE_UP; break;
					case 154: ev.x = KBD_PAGE_DOWN; break;
					default: if (v >= 1 && v <= 48) {
							int fn = (v - 1) % 12;
							int mod = (v - 1) / 12;
							ev.x = KBD_F1 - fn;
							if (mod == 1)
								ev.y |= KBD_SHIFT;
							if (mod == 2)
								ev.y |= KBD_CTRL;
							if (mod == 3)
								ev.y |= KBD_ALT;
						} break;
					} break;
				case 'z': switch (v) {
					case 247: ev.x = KBD_INS; break;
					case 214: ev.x = KBD_HOME; break;
					case 220: ev.x = KBD_END; break;
					case 216: ev.x = KBD_PAGE_UP; break;
					case 222: ev.x = KBD_PAGE_DOWN; break;
					case 249: ev.x = KBD_DEL; break;
					} break;
				case '~': switch (v) {
					case 1: ev.x = KBD_HOME; break;
					case 2: ev.x = KBD_INS; break;
					case 3: ev.x = KBD_DEL; break;
					case 4: ev.x = KBD_END; break;
					case 5: ev.x = KBD_PAGE_UP; break;
					case 6: ev.x = KBD_PAGE_DOWN; break;
					case 7: ev.x = KBD_HOME; break;
					case 8: ev.x = KBD_END; break;
					case 17: ev.x = KBD_F6; break;
					case 18: ev.x = KBD_F7; break;
					case 19: ev.x = KBD_F8; break;
					case 20: ev.x = KBD_F9; break;
					case 21: ev.x = KBD_F10; break;
					case 23: ev.x = KBD_F11; break;
					case 24: ev.x = KBD_F12; break;
					case 200: itrm->flags |= BRACKETED_PASTE; break;
					case 201: itrm->flags &= ~BRACKETED_PASTE; break;
					} break;
				case 'R':
					refresh_terminal_size();
					break;
				case 'M':
				case '<':
					if (c == 'M' && v == 5) {
						if (xterm_button == -1) xterm_button = 0;
						if (itrm->qlen - el < 5) goto ret;
						ev.x = (unsigned char)(itrm->kqueue[el+1]) - ' ' - 1 + ((int)((unsigned char)(itrm->kqueue[el+2]) - ' ' - 1) << 7);
						if ( ev.x & (1 << 13)) ev.x = 0;
						ev.y = (unsigned char)(itrm->kqueue[el+3]) - ' ' - 1 + ((int)((unsigned char)(itrm->kqueue[el+4]) - ' ' - 1) << 7);
						if ( ev.y & (1 << 13)) ev.y = 0;
						switch ((itrm->kqueue[el] - ' ') ^ xterm_button) { /* Every event changhes only one bit */
						    case TW_BUTT_LEFT:   ev.b = B_LEFT | ( (xterm_button & TW_BUTT_LEFT) ? B_UP : B_DOWN ); break;
						    case TW_BUTT_MIDDLE: ev.b = B_MIDDLE | ( (xterm_button & TW_BUTT_MIDDLE) ? B_UP : B_DOWN ); break;
						    case TW_BUTT_RIGHT:  ev.b = B_RIGHT | ( (xterm_button & TW_BUTT_RIGHT) ? B_UP : B_DOWN ); break;
						    case 0: ev.b = B_DRAG;
						    /* default : Twin protocol error */
						}
						xterm_button = itrm->kqueue[el] - ' ';
						el += 5;
					} else {
						int x = 0, y = 0, b = 0; 
						int button;
						unsigned char ch;
						if (c == 'M') {
							/* Legacy mouse protocol: \e[Mbxy whereas b, x and y are raw bytes, offset by 32. */
							if (itrm->qlen - el < 3) goto ret;
							b = itrm->kqueue[el++] - ' ';
							x = itrm->kqueue[el++] - ' ';
							y = itrm->kqueue[el++] - ' ';
						} else if (c == '<') {
							/* SGR 1006 mouse extension: \e[<b;x;yM where b, x and y are in decimal, no longer offset by 32,
							   and the trailing letter is 'm' instead of 'M' for mouse release so that the released button is reported. */
							   int eel;
							   eel = el;
							while (1) {
								if (el == itrm->qlen) goto ret;
								if (el - eel >= 9) goto l1;
								ch = itrm->kqueue[el++];
								if (ch == ';') break;
								if (ch < '0' || ch > '9') goto l1;
								b = 10 * b + (ch - '0');
							}
							eel = el;
							while (1) {
								if (el == itrm->qlen) goto ret;
								if (el - eel >= 9) goto l1;
								ch = itrm->kqueue[el++];
								if (ch == ';') break;
								if (ch < '0' || ch > '9') goto l1;
								x = 10 * x + (ch - '0');
							}
							eel = el;
							while (1) {
								if (el == itrm->qlen) goto ret;
								if (el - eel >= 9) goto l1;
								ch = itrm->kqueue[el++];
								if (ch == 'M' || ch == 'm') break;
								if (ch < '0' || ch > '9') goto l1;
								y = 10 * y + (ch - '0');
							}
						} else {
							break;
						}
						x--;
						y--;
						if (x < 0 || y < 0 || b < 0)
							break;
						if (c == 'M' && b == 3) button = B_UP;
						else if (c == '<' && ch == 'm') button = B_UP;
						else if ((b & 0x20) == 0x20) button = B_DRAG, b &= ~0x20;
						else button = B_DOWN;
						if (b == 0) button |= B_LEFT;
						else if (b == 1) button |= B_MIDDLE;
						else if (b == 2) button |= B_RIGHT;
						else if (b == 3 && xterm_button >= 0) button |= xterm_button;
						else if (b == 0x40) button |= B_WHEELUP;
						else if (b == 0x41) button |= B_WHEELDOWN;
						else if (b == 0x42) button |= B_WHEELLEFT;
						else if (b == 0x43) button |= B_WHEELRIGHT;
						else if (b == 0x80) button |= B_FOURTH;
						else if (b == 0x81) button |= B_FIFTH;
						else if (b == 0x82) button |= B_SIXTH;
						else break;
						if ((b == 0x80 || b == 0x81 || b == 0x82) && (button & BM_ACT) == B_DOWN && xterm_button == (button & BM_BUTT)) {
							/* xterm has a bug that it reports down events for both click and release */
							button &= ~BM_ACT;
							button |= B_UP;
						}
						if ((button & BM_ACT) == B_DOWN)
							xterm_button = button & BM_BUTT;
						if ((button & BM_ACT) == B_UP)
							xterm_button = -1;
						ev.b = button;
						ev.x = x;
						ev.y = y;
					}
					ev.ev = EV_MOUSE;
					break;
			}
		} else {
			el = 2;
			if (itrm->kqueue[1] == '\033') {
				if (itrm->qlen >= 3 && (itrm->kqueue[2] == '[' || itrm->kqueue[2] == 'O')) el = 1;
				ev.x = KBD_ESC;
				goto l2;
			} else if (itrm->kqueue[1] == 127) {
				ev.x = KBD_DEL;
				ev.y = 0;
				goto l2;
			} else {
				ev.x = itrm->kqueue[1];
				ev.y |= KBD_ALT;
				goto l2;
			}
		}
		goto l1;
	} else if (itrm->kqueue[0] == 0) {
		el = 1;
		goto l1;
	}
	el = 1;
	ev.x = itrm->kqueue[0];
	l2:
	if (ev.x == 3) ev.x = KBD_CTRL_C;
	if (ev.x == 8) ev.x = KBD_BS;
	if (ev.x == 9) ev.x = KBD_TAB;
	if (ev.x == 10) ev.x = KBD_ENTER;
	if (ev.x == 13) ev.x = KBD_ENTER;
	if (ev.x == 127) ev.x = KBD_BS;
	if (ev.x >= 0 && ev.x < ' ') {
		ev.x += 'A' - 1;
		ev.y |= KBD_CTRL;
	}
	l1:
	if (itrm->qlen < el) {
		internal("event queue underflow");
		itrm->qlen = el;
	}
	if (ev.x != -1) {
		if (itrm->flags & BRACKETED_PASTE && ev.ev == EV_KBD)
			ev.y |= KBD_PASTING;
		itrm->queue_event(itrm, (unsigned char *)&ev, sizeof(struct links_event));
	}
	memmove(itrm->kqueue, itrm->kqueue + el, itrm->qlen -= el);
	end:
	if (itrm->qlen < IN_BUF_SIZE && !itrm->blocked) set_handlers(itrm->std_in, in_kbd, NULL, itrm);
	return el;
	ret:
	itrm->tm = install_timer(ESC_TIMEOUT, kbd_timeout, itrm);
	return 0;
}

static void in_kbd(void *itrm_)
{
	struct itrm *itrm = (struct itrm *)itrm_;
	int r;
	if (!can_read(itrm->std_in)) return;
	if (itrm->tm != NULL) {
		kill_timer(itrm->tm);
		itrm->tm = NULL;
	}
	if (itrm->qlen >= IN_BUF_SIZE) {
		set_handlers(itrm->std_in, NULL, NULL, itrm);
		while (process_queue(itrm))
			;
		return;
	}
	EINTRLOOP(r, (int)read(itrm->std_in, itrm->kqueue + itrm->qlen, IN_BUF_SIZE - itrm->qlen));
	if (r <= 0) {
		struct links_event ev = { EV_ABORT, 0, 0, 0 };
		set_handlers(itrm->std_in, NULL, NULL, itrm);
		itrm->queue_event(itrm, (unsigned char *)&ev, sizeof(struct links_event));
		return;
	}
	more_data:
	if ((itrm->qlen += r) > IN_BUF_SIZE) {
		error("ERROR: too many bytes read");
		itrm->qlen = IN_BUF_SIZE;
	}
	if (itrm->qlen < IN_BUF_SIZE && can_read(itrm->std_in)) {
		EINTRLOOP(r, (int)read(itrm->std_in, itrm->kqueue + itrm->qlen, IN_BUF_SIZE - itrm->qlen));
		if (r > 0) goto more_data;
	}
	while (process_queue(itrm))
		;
}
