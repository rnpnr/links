/* types.c
 * (c) 2002 Mikulas Patocka
 * This file is a part of the Links program, released under GPL.
 */

#include <limits.h>

#include "links.h"

/*------------------------ ASSOCIATIONS -----------------------*/

/* DECLARATIONS */

static void assoc_edit_item(struct dialog_data *, struct list *,
                            void (*)(struct dialog_data *, struct list *,
                                     struct list *, struct list_description *),
                            struct list *, unsigned char);
static void assoc_copy_item(struct list *, struct list *);
static struct list *assoc_new_item(void *);
static void assoc_delete_item(struct list *);
static struct list *assoc_find_item(struct list *start, unsigned char *str,
                                    int direction);
static unsigned char *assoc_type_item(struct terminal *, struct list *, int);

struct list assoc = { init_list_1st(&assoc.list_entry) 0, -1, NULL };

static struct history assoc_search_history = {
	0, {&assoc_search_history.items, &assoc_search_history.items}
};

struct assoc_ok_struct {
	void (*fn)(struct dialog_data *, struct list *, struct list *,
	           struct list_description *);
	struct list *data;
	struct dialog_data *dlg;
};

static struct list_description assoc_ld = {
	0,      /* 0= flat; 1=tree */
	&assoc, /* list */
	assoc_new_item,
	assoc_edit_item,
	NULL,
	assoc_delete_item,
	assoc_copy_item,
	assoc_type_item,
	assoc_find_item,
	&assoc_search_history,
	0,  /* this is set in init_assoc function */
	15, /* # of items in main window */
	T_ASSOCIATION,
	T_ASSOCIATIONS_ALREADY_IN_USE,
	T_ASSOCIATIONS_MANAGER,
	T_DELETE_ASSOCIATION,
	0,    /* no button */
	NULL, /* no button */
	NULL, /* no save*/

	NULL,
	NULL,
	0,
	0, /* internal vars */
	0, /* modified */
	NULL,
	NULL,
	1,
};

static struct list *
assoc_new_item(void *ignore)
{
	struct assoc *neww;

	neww = mem_calloc(sizeof(struct assoc));
	neww->label = stracpy(cast_uchar "");
	neww->ct = stracpy(cast_uchar "");
	neww->prog = stracpy(cast_uchar "");
	neww->block = neww->xwin = neww->cons = 1;
	neww->ask = 1;
	neww->accept_http = 0;
	neww->accept_ftp = 0;
	neww->head.type = 0;
	neww->system = SYSTEM_ID;
	return &neww->head;
}

static void
assoc_delete_item(struct list *data)
{
	struct assoc *del = get_struct(data, struct assoc, head);

	if (del->head.list_entry.next)
		del_from_list(&del->head);
	free(del->label);
	free(del->ct);
	free(del->prog);
	free(del);
}

static void
assoc_copy_item(struct list *in, struct list *out)
{
	struct assoc *item_in = get_struct(in, struct assoc, head);
	struct assoc *item_out = get_struct(out, struct assoc, head);

	item_out->cons = item_in->cons;
	item_out->xwin = item_in->xwin;
	item_out->block = item_in->block;
	item_out->ask = item_in->ask;
	item_out->accept_http = item_in->accept_http;
	item_out->accept_ftp = item_in->accept_ftp;
	item_out->system = item_in->system;

	free(item_out->label);
	free(item_out->ct);
	free(item_out->prog);

	item_out->label = stracpy(item_in->label);
	item_out->ct = stracpy(item_in->ct);
	item_out->prog = stracpy(item_in->prog);
}

/* allocate string and print association into it */
/* x: 0=type all, 1=type title only */
static unsigned char *
assoc_type_item(struct terminal *term, struct list *data, int x)
{
	unsigned char *txt, *txt1;
	struct assoc *item;

	if (data == &assoc)
		return stracpy(
		    get_text_translation(TEXT_(T_ASSOCIATIONS), term));

	item = get_struct(data, struct assoc, head);
	txt = stracpy(cast_uchar "");
	if (item->system != SYSTEM_ID)
		add_to_strn(&txt, cast_uchar "XX ");
	add_to_strn(&txt, item->label);
	add_to_strn(&txt, cast_uchar ": ");
	add_to_strn(&txt, item->ct);
	if (!x) {
		add_to_strn(&txt, cast_uchar " -> ");
		if (item->prog)
			add_to_strn(&txt, item->prog);
	}
	txt1 = convert(assoc_ld.codepage, term_charset(term), txt, NULL);
	free(txt);

	return txt1;
}

void
menu_assoc_manager(struct terminal *term, void *fcp, void *ses_)
{
	struct session *ses = (struct session *)ses_;
	create_list_window(&assoc_ld, &assoc, term, ses);
}

static unsigned char *const ct_msg[] = {
	TEXT_(T_LABEL),
	TEXT_(T_CONTENT_TYPES),
	TEXT_(T_PROGRAM__IS_REPLACED_WITH_FILE_NAME),
	TEXT_(T_BLOCK_TERMINAL_WHILE_PROGRAM_RUNNING),
	TEXT_(T_RUN_ON_TERMINAL),
	TEXT_(T_RUN_IN_XWINDOW),
	TEXT_(T_ASK_BEFORE_OPENING),
	TEXT_(T_ACCEPT_HTTP),
	TEXT_(T_ACCEPT_FTP),
};

static void
assoc_edit_item_fn(struct dialog_data *dlg)
{
	struct terminal *term = dlg->win->term;
	int max = 0, min = 0;
	int w, rw;
	int y = -1;
	int p = 3;
	p++;
	p += 2;
	max_text_width(term, ct_msg[0], &max, AL_LEFT);
	min_text_width(term, ct_msg[0], &min, AL_LEFT);
	max_text_width(term, ct_msg[1], &max, AL_LEFT);
	min_text_width(term, ct_msg[1], &min, AL_LEFT);
	max_text_width(term, ct_msg[2], &max, AL_LEFT);
	min_text_width(term, ct_msg[2], &min, AL_LEFT);
	max_group_width(term, ct_msg + 3, dlg->items + 3, p, &max);
	min_group_width(term, ct_msg + 3, dlg->items + 3, p, &min);
	max_buttons_width(term, dlg->items + 3 + p, 2, &max);
	min_buttons_width(term, dlg->items + 3 + p, 2, &min);
	w = term->x * 9 / 10 - 2 * DIALOG_LB;
	if (w > max)
		w = max;
	if (w < min)
		w = min;
	if (w > term->x - 2 * DIALOG_LB)
		w = term->x - 2 * DIALOG_LB;
	if (w < 1)
		w = 1;
	rw = 0;
	dlg_format_text_and_field(
	    dlg, NULL, get_text_translation(ct_msg[0], term), &dlg->items[0], 0,
	    &y, w, &rw, COLOR_DIALOG_TEXT, AL_LEFT);
	y++;
	dlg_format_text_and_field(
	    dlg, NULL, get_text_translation(ct_msg[1], term), &dlg->items[1], 0,
	    &y, w, &rw, COLOR_DIALOG_TEXT, AL_LEFT);
	y++;
	dlg_format_text_and_field(
	    dlg, NULL, get_text_translation(ct_msg[2], term), &dlg->items[2], 0,
	    &y, w, &rw, COLOR_DIALOG_TEXT, AL_LEFT);
	y++;
	dlg_format_group(dlg, NULL, ct_msg + 3, dlg->items + 3, p, 0, &y, w,
	                 &rw);
	y++;
	dlg_format_buttons(dlg, NULL, dlg->items + 3 + p, 2, 0, &y, w, &rw,
	                   AL_CENTER);
	w = rw;
	dlg->xw = w + 2 * DIALOG_LB;
	dlg->yw = y + 2 * DIALOG_TB;
	center_dlg(dlg);
	draw_dlg(dlg);
	y = dlg->y + DIALOG_TB;
	dlg_format_text_and_field(dlg, term, ct_msg[0], &dlg->items[0],
	                          dlg->x + DIALOG_LB, &y, w, NULL,
	                          COLOR_DIALOG_TEXT, AL_LEFT);
	y++;
	dlg_format_text_and_field(dlg, term, ct_msg[1], &dlg->items[1],
	                          dlg->x + DIALOG_LB, &y, w, NULL,
	                          COLOR_DIALOG_TEXT, AL_LEFT);
	y++;
	dlg_format_text_and_field(dlg, term, ct_msg[2], &dlg->items[2],
	                          dlg->x + DIALOG_LB, &y, w, NULL,
	                          COLOR_DIALOG_TEXT, AL_LEFT);
	y++;
	dlg_format_group(dlg, term, ct_msg + 3, &dlg->items[3], p,
	                 dlg->x + DIALOG_LB, &y, w, NULL);
	y++;
	dlg_format_buttons(dlg, term, &dlg->items[3 + p], 2, dlg->x + DIALOG_LB,
	                   &y, w, NULL, AL_CENTER);
}

/* Puts url and title into the bookmark item */
static void
assoc_edit_done(void *data)
{
	struct dialog *d = (struct dialog *)data;
	struct assoc *item = (struct assoc *)d->udata;
	struct assoc_ok_struct *s = (struct assoc_ok_struct *)d->udata2;
	unsigned char *txt;
	unsigned char *label, *ct, *prog;

	label = (unsigned char *)&d->items[12];
	ct = label + MAX_STR_LEN;
	prog = ct + MAX_STR_LEN;

	txt = convert(term_charset(s->dlg->win->term), assoc_ld.codepage, label,
	              NULL);
	free(item->label);
	item->label = txt;

	txt = convert(term_charset(s->dlg->win->term), assoc_ld.codepage, ct,
	              NULL);
	free(item->ct);
	item->ct = txt;

	txt = convert(term_charset(s->dlg->win->term), assoc_ld.codepage, prog,
	              NULL);
	free(item->prog);
	item->prog = txt;

	s->fn(s->dlg, s->data, &item->head, &assoc_ld);
	d->udata = NULL; /* for abort function */
}

/* destroys an item, this function is called when edit window is aborted */
static void
assoc_edit_abort(struct dialog_data *data)
{
	struct dialog *dlg = data->dlg;
	struct assoc *item = (struct assoc *)dlg->udata;

	free(dlg->udata2);
	if (item)
		assoc_delete_item(&item->head);
}

static void
assoc_edit_item(struct dialog_data *dlg, struct list *data,
                void (*ok_fn)(struct dialog_data *, struct list *,
                              struct list *, struct list_description *),
                struct list *ok_arg, unsigned char dlg_title)
{
	int p;
	struct assoc *neww = get_struct(data, struct assoc, head);
	struct terminal *term = dlg->win->term;
	struct dialog *d;
	struct assoc_ok_struct *s;
	unsigned char *ct, *prog, *label;

	d = mem_calloc(sizeof(struct dialog) + 11 * sizeof(struct dialog_item)
	               + 3 * MAX_STR_LEN);

	label = (unsigned char *)&d->items[12];
	ct = label + MAX_STR_LEN;
	prog = ct + MAX_STR_LEN;

	safe_strncpy(label, neww->label, MAX_STR_LEN);
	safe_strncpy(ct, neww->ct, MAX_STR_LEN);
	safe_strncpy(prog, neww->prog, MAX_STR_LEN);

	/* Create the dialog */
	s = xmalloc(sizeof(struct assoc_ok_struct));
	s->fn = ok_fn;
	s->data = ok_arg;
	s->dlg = dlg;

	switch (dlg_title) {
	case TITLE_EDIT:
		d->title = TEXT_(T_EDIT_ASSOCIATION);
		break;

	case TITLE_ADD:
		d->title = TEXT_(T_ADD_ASSOCIATION);
		break;

	default:
		internal("Unsupported dialog title.\n");
	}

	d->udata = neww;
	d->udata2 = s;
	d->fn = assoc_edit_item_fn;
	d->abort = assoc_edit_abort;
	d->refresh = assoc_edit_done;
	d->refresh_data = d;
	d->items[0].type = D_FIELD;
	d->items[0].dlen = MAX_STR_LEN;
	d->items[0].data = label;
	d->items[0].fn = check_nonempty;
	d->items[1].type = D_FIELD;
	d->items[1].dlen = MAX_STR_LEN;
	d->items[1].data = ct;
	d->items[1].fn = check_nonempty;
	d->items[2].type = D_FIELD;
	d->items[2].dlen = MAX_STR_LEN;
	d->items[2].data = prog;
	d->items[2].fn = check_nonempty;
	p = 3;
	d->items[p].type = D_CHECKBOX;
	d->items[p].data = (unsigned char *)&neww->block;
	d->items[p++].dlen = sizeof(int);
	d->items[p].type = D_CHECKBOX;
	d->items[p].data = (unsigned char *)&neww->cons;
	d->items[p++].dlen = sizeof(int);
	d->items[p].type = D_CHECKBOX;
	d->items[p].data = (unsigned char *)&neww->xwin;
	d->items[p++].dlen = sizeof(int);
	d->items[p].type = D_CHECKBOX;
	d->items[p].data = (unsigned char *)&neww->ask;
	d->items[p++].dlen = sizeof(int);
	d->items[p].type = D_CHECKBOX;
	d->items[p].data = (unsigned char *)&neww->accept_http;
	d->items[p++].dlen = sizeof(int);
	d->items[p].type = D_CHECKBOX;
	d->items[p].data = (unsigned char *)&neww->accept_ftp;
	d->items[p++].dlen = sizeof(int);
	d->items[p].type = D_BUTTON;
	d->items[p].gid = B_ENTER;
	d->items[p].fn = ok_dialog;
	d->items[p++].text = TEXT_(T_OK);
	d->items[p].type = D_BUTTON;
	d->items[p].gid = B_ESC;
	d->items[p].text = TEXT_(T_CANCEL);
	d->items[p++].fn = cancel_dialog;
	d->items[p++].type = D_END;
	do_dialog(term, d, getml(d, NULL));
}

static int
assoc_test_entry(struct list *e, unsigned char *str)
{
	struct assoc *a = get_struct(e, struct assoc, head);
	return casestrstr(a->label, str) || casestrstr(a->ct, str);
}

static struct list *
assoc_find_item(struct list *s, unsigned char *str, int direction)
{
	struct list *e;

	if (direction >= 0)
		for (e = list_next(s); e != s; e = list_next(e)) {
			if (e->depth >= 0 && assoc_test_entry(e, str))
				return e;
		}
	else
		for (e = list_prev(s); e != s; e = list_prev(e)) {
			if (e->depth >= 0 && assoc_test_entry(e, str))
				return e;
		}

	if (e->depth >= 0 && assoc_test_entry(e, str))
		return e;

	return NULL;
}

void
update_assoc(struct assoc *neww)
{
	struct assoc *repl;
	struct list *r = NULL;
	struct list_head *lr;
	if (!neww->label[0] || !neww->ct[0] || !neww->prog[0])
		return;
	foreach (struct list, r, lr, assoc.list_entry) {
		repl = get_struct(r, struct assoc, head);
		if (!strcmp(cast_const_char repl->label,
		            cast_const_char neww->label)
		    && !strcmp(cast_const_char repl->ct,
		               cast_const_char neww->ct)
		    && !strcmp(cast_const_char repl->prog,
		               cast_const_char neww->prog)
		    && repl->block == neww->block && repl->cons == neww->cons
		    && repl->xwin == neww->xwin && repl->ask == neww->ask
		    && repl->accept_http == neww->accept_http
		    && repl->accept_ftp == neww->accept_ftp
		    && repl->system == neww->system) {
			del_from_list(&repl->head);
			add_to_list(assoc.list_entry, &repl->head);
			return;
		}
	}
	repl = mem_calloc(sizeof(struct assoc));
	repl->label = stracpy(neww->label);
	repl->ct = stracpy(neww->ct);
	repl->prog = stracpy(neww->prog);
	repl->block = neww->block;
	repl->cons = neww->cons;
	repl->xwin = neww->xwin;
	repl->ask = neww->ask;
	repl->accept_http = neww->accept_http;
	repl->accept_ftp = neww->accept_ftp;
	repl->system = neww->system;
	repl->head.type = 0;
	add_to_list(assoc.list_entry, &repl->head);
}

/*------------------------ EXTENSIONS -----------------------*/

/* DECLARATIONS */
static void ext_edit_item(struct dialog_data *, struct list *,
                          void (*)(struct dialog_data *, struct list *,
                                   struct list *, struct list_description *),
                          struct list *, unsigned char);
static void ext_copy_item(struct list *, struct list *);
static struct list *ext_new_item(void *);
static void ext_delete_item(struct list *);
static struct list *ext_find_item(struct list *start, unsigned char *str,
                                  int direction);
static unsigned char *ext_type_item(struct terminal *, struct list *, int);

struct list extensions = { init_list_1st(&extensions.list_entry) 0, -1, NULL };

static struct history ext_search_history = {
	0, {&ext_search_history.items, &ext_search_history.items}
};

static struct list_description ext_ld = {
	0,           /* 0= flat; 1=tree */
	&extensions, /* list */
	ext_new_item,
	ext_edit_item,
	NULL,
	ext_delete_item,
	ext_copy_item,
	ext_type_item,
	ext_find_item,
	&ext_search_history,
	0,  /* this is set in init_assoc function */
	15, /* # of items in main window */
	T_eXTENSION,
	T_EXTENSIONS_ALREADY_IN_USE,
	T_EXTENSIONS_MANAGER,
	T_DELETE_EXTENSION,
	0,    /* no button */
	NULL, /* no button */
	NULL, /* no save*/

	NULL,
	NULL,
	0,
	0, /* internal vars */
	0, /* modified */
	NULL,
	NULL,
	0,
};

static struct list *
ext_new_item(void *ignore)
{
	struct extension *neww;

	neww = mem_calloc(sizeof(struct extension));
	neww->ext = stracpy(cast_uchar "");
	neww->ct = stracpy(cast_uchar "");
	neww->head.type = 0;
	return &neww->head;
}

static void
ext_delete_item(struct list *data)
{
	struct extension *del = get_struct(data, struct extension, head);

	if (del->head.list_entry.next)
		del_from_list(&del->head);
	free(del->ext);
	free(del->ct);
	free(del);
}

static void
ext_copy_item(struct list *in, struct list *out)
{
	struct extension *item_in = get_struct(in, struct extension, head);
	struct extension *item_out = get_struct(out, struct extension, head);

	free(item_out->ext);
	free(item_out->ct);

	item_out->ext = stracpy(item_in->ext);
	item_out->ct = stracpy(item_in->ct);
}

/* allocate string and print extension into it */
/* x: 0=type all, 1=type title only */
static unsigned char *
ext_type_item(struct terminal *term, struct list *data, int x)
{
	unsigned char *txt, *txt1;
	struct extension *item;

	if (data == &extensions)
		return stracpy(
		    get_text_translation(TEXT_(T_FILE_EXTENSIONS), term));

	item = get_struct(data, struct extension, head);
	txt = stracpy(item->ext);
	add_to_strn(&txt, cast_uchar ": ");
	add_to_strn(&txt, item->ct);
	txt1 = convert(assoc_ld.codepage, term_charset(term), txt, NULL);
	free(txt);

	return txt1;
}

void
menu_ext_manager(struct terminal *term, void *fcp, void *ses_)
{
	struct session *ses = (struct session *)ses_;
	create_list_window(&ext_ld, &extensions, term, ses);
}

static unsigned char *const ext_msg[] = {
	TEXT_(T_EXTENSION_S),
	TEXT_(T_CONTENT_TYPE),
};

static void
ext_edit_item_fn(struct dialog_data *dlg)
{
	struct terminal *term = dlg->win->term;
	int max = 0, min = 0;
	int w, rw;
	int y = -1;
	max_text_width(term, ext_msg[0], &max, AL_LEFT);
	min_text_width(term, ext_msg[0], &min, AL_LEFT);
	max_text_width(term, ext_msg[1], &max, AL_LEFT);
	min_text_width(term, ext_msg[1], &min, AL_LEFT);
	max_buttons_width(term, dlg->items + 2, 2, &max);
	min_buttons_width(term, dlg->items + 2, 2, &min);
	w = term->x * 9 / 10 - 2 * DIALOG_LB;
	if (w > max)
		w = max;
	if (w < min)
		w = min;
	if (w > term->x - 2 * DIALOG_LB)
		w = term->x - 2 * DIALOG_LB;
	if (w < 1)
		w = 1;
	rw = 0;
	dlg_format_text_and_field(dlg, NULL, ext_msg[0], &dlg->items[0], 0, &y,
	                          w, &rw, COLOR_DIALOG_TEXT, AL_LEFT);
	y++;
	dlg_format_text_and_field(dlg, NULL, ext_msg[1], &dlg->items[1], 0, &y,
	                          w, &rw, COLOR_DIALOG_TEXT, AL_LEFT);
	y++;
	dlg_format_buttons(dlg, NULL, dlg->items + 2, 2, 0, &y, w, &rw,
	                   AL_CENTER);
	w = rw;
	dlg->xw = w + 2 * DIALOG_LB;
	dlg->yw = y + 2 * DIALOG_TB;
	center_dlg(dlg);
	draw_dlg(dlg);
	y = dlg->y + DIALOG_TB;
	dlg_format_text_and_field(dlg, term, ext_msg[0], &dlg->items[0],
	                          dlg->x + DIALOG_LB, &y, w, NULL,
	                          COLOR_DIALOG_TEXT, AL_LEFT);
	y++;
	dlg_format_text_and_field(dlg, term, ext_msg[1], &dlg->items[1],
	                          dlg->x + DIALOG_LB, &y, w, NULL,
	                          COLOR_DIALOG_TEXT, AL_LEFT);
	y++;
	dlg_format_buttons(dlg, term, &dlg->items[2], 2, dlg->x + DIALOG_LB, &y,
	                   w, NULL, AL_CENTER);
}

/* Puts url and title into the bookmark item */
static void
ext_edit_done(void *data)
{
	struct dialog *d = (struct dialog *)data;
	struct extension *item = (struct extension *)d->udata;
	struct assoc_ok_struct *s = (struct assoc_ok_struct *)d->udata2;
	unsigned char *txt;
	unsigned char *ext, *ct;

	ext = (unsigned char *)&d->items[5];
	ct = ext + MAX_STR_LEN;

	txt = convert(term_charset(s->dlg->win->term), ext_ld.codepage, ext,
	              NULL);
	free(item->ext);
	item->ext = txt;

	txt =
	    convert(term_charset(s->dlg->win->term), ext_ld.codepage, ct, NULL);
	free(item->ct);
	item->ct = txt;

	s->fn(s->dlg, s->data, &item->head, &ext_ld);
	d->udata = NULL; /* for abort function */
}

/* destroys an item, this function is called when edit window is aborted */
static void
ext_edit_abort(struct dialog_data *data)
{
	struct dialog *dlg = data->dlg;
	struct extension *item = (struct extension *)dlg->udata;

	free(dlg->udata2);
	if (item)
		ext_delete_item(&item->head);
}

static void
ext_edit_item(struct dialog_data *dlg, struct list *data,
              void (*ok_fn)(struct dialog_data *, struct list *, struct list *,
                            struct list_description *),
              struct list *ok_arg, unsigned char dlg_title)
{
	struct extension *neww = get_struct(data, struct extension, head);
	struct terminal *term = dlg->win->term;
	struct dialog *d;
	struct assoc_ok_struct *s;
	unsigned char *ext;
	unsigned char *ct;

	d = mem_calloc(sizeof(struct dialog) + 4 * sizeof(struct dialog_item)
	               + 2 * MAX_STR_LEN);

	ext = (unsigned char *)&d->items[5];
	ct = ext + MAX_STR_LEN;
	safe_strncpy(ext, neww->ext, MAX_STR_LEN);
	safe_strncpy(ct, neww->ct, MAX_STR_LEN);

	/* Create the dialog */
	s = xmalloc(sizeof(struct assoc_ok_struct));
	s->fn = ok_fn;
	s->data = ok_arg;
	s->dlg = dlg;

	switch (dlg_title) {
	case TITLE_EDIT:
		d->title = TEXT_(T_EDIT_EXTENSION);
		break;

	case TITLE_ADD:
		d->title = TEXT_(T_ADD_EXTENSION);
		break;

	default:
		internal("Unsupported dialog title.\n");
	}

	d->udata = neww;
	d->udata2 = s;
	d->abort = ext_edit_abort;
	d->refresh = ext_edit_done;
	d->refresh_data = d;
	d->title = TEXT_(T_EXTENSION);
	d->fn = ext_edit_item_fn;
	d->items[0].type = D_FIELD;
	d->items[0].dlen = MAX_STR_LEN;
	d->items[0].data = ext;
	d->items[0].fn = check_nonempty;
	d->items[1].type = D_FIELD;
	d->items[1].dlen = MAX_STR_LEN;
	d->items[1].data = ct;
	d->items[1].fn = check_nonempty;
	d->items[2].type = D_BUTTON;
	d->items[2].gid = B_ENTER;
	d->items[2].fn = ok_dialog;
	d->items[2].text = TEXT_(T_OK);
	d->items[3].type = D_BUTTON;
	d->items[3].gid = B_ESC;
	d->items[3].text = TEXT_(T_CANCEL);
	d->items[3].fn = cancel_dialog;
	d->items[4].type = D_END;
	do_dialog(term, d, getml(d, NULL));
}

static int
ext_test_entry(struct list *e, unsigned char *str)
{
	struct extension *ext = get_struct(e, struct extension, head);
	return casestrstr(ext->ext, str) || casestrstr(ext->ct, str);
}

static struct list *
ext_find_item(struct list *s, unsigned char *str, int direction)
{
	struct list *e;

	if (direction >= 0)
		for (e = list_next(s); e != s; e = list_next(e)) {
			if (e->depth >= 0 && ext_test_entry(e, str))
				return e;
		}
	else
		for (e = list_prev(s); e != s; e = list_prev(e)) {
			if (e->depth >= 0 && ext_test_entry(e, str))
				return e;
		}

	if (e->depth >= 0 && ext_test_entry(e, str))
		return e;

	return NULL;
}

void
update_ext(struct extension *neww)
{
	struct extension *repl;
	struct list *r = NULL;
	struct list_head *lr;
	if (!neww->ext[0] || !neww->ct[0])
		return;
	foreach (struct list, r, lr, extensions.list_entry) {
		repl = get_struct(r, struct extension, head);
		if (!strcmp(cast_const_char repl->ext,
		            cast_const_char neww->ext)
		    && !strcmp(cast_const_char repl->ct,
		               cast_const_char neww->ct)) {
			del_from_list(&repl->head);
			add_to_list(extensions.list_entry, &repl->head);
			return;
		}
	}
	repl = mem_calloc(sizeof(struct extension));
	repl->ext = stracpy(neww->ext);
	repl->ct = stracpy(neww->ct);
	repl->head.type = 0;
	add_to_list(extensions.list_entry, &repl->head);
}

void
update_prog(struct list_head *l, unsigned char *p, int s)
{
	struct protocol_program *repl = NULL;
	struct list_head *lrepl;
	foreach (struct protocol_program, repl, lrepl, *l)
		if (repl->system == s) {
			free(repl->prog);
			goto ss;
		}
	repl = xmalloc(sizeof(struct protocol_program));
	add_to_list(*l, repl);
	repl->system = s;
ss:
	repl->prog = xmalloc(MAX_STR_LEN);
	safe_strncpy(repl->prog, p, MAX_STR_LEN);
}

unsigned char *
get_prog(struct list_head *l)
{
	struct protocol_program *repl = NULL;
	struct list_head *lrepl;
	foreach (struct protocol_program, repl, lrepl, *l)
		if (repl->system == SYSTEM_ID)
			return repl->prog;
	update_prog(l, cast_uchar "", SYSTEM_ID);
	foreach (struct protocol_program, repl, lrepl, *l)
		if (repl->system == SYSTEM_ID)
			return repl->prog;
	internal("get_prog: program was not added");
	return cast_uchar "";
	;
}

/* creates default extensions if extension list is empty */
void
create_initial_extensions(void)
{
	struct extension ext;

	if (!list_empty(extensions.list_entry))
		return;

	/* here you can add any default extension you want */
	ext.ext = cast_uchar "xpm";
	ext.ct = cast_uchar "image/x-xpixmap";
	update_ext(&ext);
	ext.ext = cast_uchar "xls";
	ext.ct = cast_uchar "application/excel";
	update_ext(&ext);
	ext.ext = cast_uchar "xbm";
	ext.ct = cast_uchar "image/x-xbitmap";
	update_ext(&ext);
	ext.ext = cast_uchar "wav";
	ext.ct = cast_uchar "audio/x-wav";
	update_ext(&ext);
	ext.ext = cast_uchar "tiff,tif";
	ext.ct = cast_uchar "image/tiff";
	update_ext(&ext);
	ext.ext = cast_uchar "tga";
	ext.ct = cast_uchar "image/targa";
	update_ext(&ext);
	ext.ext = cast_uchar "sxw";
	ext.ct = cast_uchar "application/x-openoffice";
	update_ext(&ext);
	ext.ext = cast_uchar "swf";
	ext.ct = cast_uchar "application/x-shockwave-flash";
	update_ext(&ext);
	ext.ext = cast_uchar "svg";
	ext.ct = cast_uchar "image/svg+xml";
	update_ext(&ext);
	ext.ext = cast_uchar "sch";
	ext.ct = cast_uchar "application/gschem";
	update_ext(&ext);
	ext.ext = cast_uchar "rtf";
	ext.ct = cast_uchar "application/rtf";
	update_ext(&ext);
	ext.ext = cast_uchar "ra,rm,ram";
	ext.ct = cast_uchar "audio/x-pn-realaudio";
	update_ext(&ext);
	ext.ext = cast_uchar "qt,mov";
	ext.ct = cast_uchar "video/quicktime";
	update_ext(&ext);
	ext.ext = cast_uchar "ps,eps,ai";
	ext.ct = cast_uchar "application/postscript";
	update_ext(&ext);
	ext.ext = cast_uchar "ppt";
	ext.ct = cast_uchar "application/powerpoint";
	update_ext(&ext);
	ext.ext = cast_uchar "ppm";
	ext.ct = cast_uchar "image/x-portable-pixmap";
	update_ext(&ext);
	ext.ext = cast_uchar "pnm";
	ext.ct = cast_uchar "image/x-portable-anymap";
	update_ext(&ext);
	ext.ext = cast_uchar "png";
	ext.ct = cast_uchar "image/png";
	update_ext(&ext);
	ext.ext = cast_uchar "pgp";
	ext.ct = cast_uchar "application/pgp-signature";
	update_ext(&ext);
	ext.ext = cast_uchar "pgm";
	ext.ct = cast_uchar "image/x-portable-graymap";
	update_ext(&ext);
	ext.ext = cast_uchar "pdf";
	ext.ct = cast_uchar "application/pdf";
	update_ext(&ext);
	ext.ext = cast_uchar "pcb";
	ext.ct = cast_uchar "application/pcb";
	update_ext(&ext);
	ext.ext = cast_uchar "pbm";
	ext.ct = cast_uchar "image/x-portable-bitmap";
	update_ext(&ext);
	ext.ext = cast_uchar "mpeg,mpg,mpe";
	ext.ct = cast_uchar "video/mpeg";
	update_ext(&ext);
	ext.ext = cast_uchar "mp3";
	ext.ct = cast_uchar "audio/mpeg";
	update_ext(&ext);
	ext.ext = cast_uchar "mid,midi";
	ext.ct = cast_uchar "audio/midi";
	update_ext(&ext);
	ext.ext = cast_uchar "jpg,jpeg,jpe";
	ext.ct = cast_uchar "image/jpeg";
	update_ext(&ext);
	ext.ext = cast_uchar "grb";
	ext.ct = cast_uchar "application/gerber";
	update_ext(&ext);
	ext.ext = cast_uchar "gl";
	ext.ct = cast_uchar "video/gl";
	update_ext(&ext);
	ext.ext = cast_uchar "gif";
	ext.ct = cast_uchar "image/gif";
	update_ext(&ext);
	ext.ext = cast_uchar "gbr";
	ext.ct = cast_uchar "application/gerber";
	update_ext(&ext);
	ext.ext = cast_uchar "g";
	ext.ct = cast_uchar "application/brlcad";
	update_ext(&ext);
	ext.ext = cast_uchar "fli";
	ext.ct = cast_uchar "video/fli";
	update_ext(&ext);
	ext.ext = cast_uchar "dxf";
	ext.ct = cast_uchar "application/dxf";
	update_ext(&ext);
	ext.ext = cast_uchar "dvi";
	ext.ct = cast_uchar "application/x-dvi";
	update_ext(&ext);
	ext.ext = cast_uchar "dl";
	ext.ct = cast_uchar "video/dl";
	update_ext(&ext);
	ext.ext = cast_uchar "deb";
	ext.ct = cast_uchar "application/x-debian-package";
	update_ext(&ext);
	ext.ext = cast_uchar "avi";
	ext.ct = cast_uchar "video/x-msvideo";
	update_ext(&ext);
	ext.ext = cast_uchar "au,snd";
	ext.ct = cast_uchar "audio/basic";
	update_ext(&ext);
	ext.ext = cast_uchar "aif,aiff,aifc";
	ext.ct = cast_uchar "audio/x-aiff";
	update_ext(&ext);
}

/* --------------------------- PROG -----------------------------*/

static int
is_in_list(unsigned char *list, unsigned char *str, int l)
{
	unsigned char *l2, *l3;
	if (!l)
		return 0;
rep:
	while (*list && *list <= ' ')
		list++;
	if (!*list)
		return 0;
	for (l2 = list; *l2 && *l2 != ','; l2++)
		;
	for (l3 = l2 - 1; l3 >= list && *l3 <= ' '; l3--)
		;
	l3++;
	if (l3 - list == l && !casecmp(str, list, l))
		return 1;
	list = l2;
	if (*list == ',')
		list++;
	goto rep;
}

/* FIXME */
static char *
canonical_compressed_ext(char *ext, char *ext_end)
{
	size_t len;
	if (!ext_end)
		ext_end = strchr(ext, 0);
	len = ext_end - ext;
	switch (len) {
	case 3:
		if (!strncasecmp(ext, "tgz", 3))
			return "gz";
		else if (!strncasecmp(ext, "txz", 3))
			return "xz";
		else if (!strncasecmp(ext, "tbz", 3))
			return "bz2";
		break;
	case 6:
		if (!strncasecmp(ext, "tar-gz", 3))
			return "gz";
		else if (!strncasecmp(ext, "tar-xz", 3))
			return "xz";
		break;
	case 7:
		if (!strncasecmp(ext, "tar-bz2", 3))
			return "bz2";
		/* fallthrough */
	default:
		break;
	}
	return NULL;
}

/* FIXME */
unsigned char *
get_compress_by_extension(char *ext, char *ext_end)
{
	size_t len;
	char *x;
	if ((x = canonical_compressed_ext(ext, ext_end))) {
		ext = x;
		ext_end = strchr(x, 0);
	}
	len = ext_end - ext;
	switch (len) {
	case 1:
		if (!strncasecmp(ext, "z", 1))
			return cast_uchar "compress";
		break;
	case 2:
		if (!strncasecmp(ext, "br", 2))
			return cast_uchar "br";
		else if (!strncasecmp(ext, "gz", 2))
			return cast_uchar "gzip";
		else if (!strncasecmp(ext, "xz", 2))
			return cast_uchar "lzma2";
		else if (!strncasecmp(ext, "lz", 2))
			return cast_uchar "lzip";
		break;
	case 3:
		if (!strncasecmp(ext, "bz2", 3))
			return cast_uchar "bzip2";
		break;
	case 4:
		if (!strncasecmp(ext, "lzma", 4))
			return cast_uchar "lzma";
		/* fallthrough */
	default:
		break;
	}
	return NULL;
}

unsigned char *
get_content_type_by_extension(unsigned char *url)
{
	struct list *l = NULL;
	struct list_head *ll;
	unsigned char *ct, *eod, *ext, *exxt;
	int extl, el;
	ext = NULL;
	extl = 0;
	if (!(ct = get_url_data(url)))
		ct = url;
	for (eod = ct; *eod && !end_of_dir(url, *eod); eod++)
		;
	for (; ct < eod; ct++)
		if (*ct == '.') {
			if (ext)
				if (get_compress_by_extension((char *)(ct + 1),
				                              (char *)eod))
					break;
			ext = ct + 1;
		} else if (dir_sep(*ct))
			ext = NULL;
	if (ext)
		while (ext[extl] && ext[extl] != '.' && !dir_sep(ext[extl])
		       && !end_of_dir(url, ext[extl]))
			extl++;
	if ((extl == 3 && !casecmp(ext, cast_uchar "htm", 3))
	    || (extl == 4 && !casecmp(ext, cast_uchar "html", 4))
	    || (extl == 5 && !casecmp(ext, cast_uchar "xhtml", 5)))
		return stracpy(cast_uchar "text/html");
	foreach (struct list, l, ll, extensions.list_entry) {
		struct extension *e = get_struct(l, struct extension, head);
		unsigned char *fname = NULL;
		if (!(ct = get_url_data(url)))
			ct = url;
		for (; *ct && !end_of_dir(url, *ct); ct++)
			if (dir_sep(*ct))
				fname = ct + 1;
		if (!fname) {
			if (is_in_list(e->ext, ext, extl))
				return stracpy(e->ct);
		} else {
			int fnlen = 0;
			int x;
			while (fname[fnlen] && !end_of_dir(url, fname[fnlen]))
				fnlen++;
			for (x = 0; x < fnlen; x++)
				if (fname[x] == '.')
					if (is_in_list(e->ext, fname + x + 1,
					               fnlen - x - 1))
						return stracpy(e->ct);
		}
	}

	if ((extl == 3 && !casecmp(ext, cast_uchar "jpg", 3))
	    || (extl == 4 && !casecmp(ext, cast_uchar "pjpg", 4))
	    || (extl == 4 && !casecmp(ext, cast_uchar "jpeg", 4))
	    || (extl == 5 && !casecmp(ext, cast_uchar "pjpeg", 5)))
		return stracpy(cast_uchar "image/jpeg");
	if ((extl == 3 && !casecmp(ext, cast_uchar "png", 3)))
		return stracpy(cast_uchar "image/png");
	if ((extl == 3 && !casecmp(ext, cast_uchar "gif", 3)))
		return stracpy(cast_uchar "image/gif");
	if ((extl == 3 && !casecmp(ext, cast_uchar "xbm", 3)))
		return stracpy(cast_uchar "image/x-xbitmap");
	if ((extl == 3 && !casecmp(ext, cast_uchar "tif", 3))
	    || (extl == 4 && !casecmp(ext, cast_uchar "tiff", 4)))
		return stracpy(cast_uchar "image/tiff");
	exxt = init_str();
	el = 0;
	add_to_str(&exxt, &el, cast_uchar "application/x-");
	add_bytes_to_str(&exxt, &el, ext, extl);
	foreach (struct list, l, ll, assoc.list_entry) {
		struct assoc *a = get_struct(l, struct assoc, head);
		if (is_in_list(a->ct, exxt, el))
			return exxt;
	}
	free(exxt);
	return NULL;
}

static unsigned char *
get_content_type_by_header_and_extension(unsigned char *head,
                                         unsigned char *url)
{
	unsigned char *ct, *file;
	ct = get_content_type_by_extension(url);
	if (ct)
		return ct;
	file = get_filename_from_header(head);
	if (file) {
		ct = get_content_type_by_extension(file);
		free(file);
		if (ct)
			return ct;
	}
	return NULL;
}

static unsigned char *
get_extension_by_content_type(unsigned char *ct)
{
	struct list *l = NULL;
	struct list_head *ll;
	unsigned char *x, *y;
	if (is_html_type(ct))
		return stracpy(cast_uchar "html");
	foreach (struct list, l, ll, extensions.list_entry) {
		struct extension *e = get_struct(l, struct extension, head);
		if (!casestrcmp(e->ct, ct)) {
			x = stracpy(e->ext);
			if ((y = cast_uchar strchr(cast_const_char x, ',')))
				*y = 0;
			return x;
		}
	}
	if (!casestrcmp(ct, cast_uchar "image/jpeg")
	    || !casestrcmp(ct, cast_uchar "image/jpg")
	    || !casestrcmp(ct, cast_uchar "image/jpe")
	    || !casestrcmp(ct, cast_uchar "image/pjpe")
	    || !casestrcmp(ct, cast_uchar "image/pjpeg")
	    || !casestrcmp(ct, cast_uchar "image/pjpg"))
		return stracpy(cast_uchar "jpg");
	if (!casestrcmp(ct, cast_uchar "image/png")
	    || !casestrcmp(ct, cast_uchar "image/x-png"))
		return stracpy(cast_uchar "png");
	if (!casestrcmp(ct, cast_uchar "image/gif"))
		return stracpy(cast_uchar "gif");
	if (!casestrcmp(ct, cast_uchar "image/x-bitmap"))
		return stracpy(cast_uchar "xbm");
	if (!casestrcmp(ct, cast_uchar "image/tiff")
	    || !casestrcmp(ct, cast_uchar "image/tif"))
		return stracpy(cast_uchar "tiff");
	if (!casestrcmp(ct, cast_uchar "image/svg")
	    || !casestrcmp(ct, cast_uchar "image/svg+xml"))
		return stracpy(cast_uchar "svg");
	if (!cmpbeg(ct, cast_uchar "application/x-")) {
		x = ct + strlen("application/x-");
		if (casestrcmp(x, cast_uchar "z")
		    && casestrcmp(x, cast_uchar "gz")
		    && casestrcmp(x, cast_uchar "gzip")
		    && casestrcmp(x, cast_uchar "br")
		    && casestrcmp(x, cast_uchar "bz2")
		    && casestrcmp(x, cast_uchar "bzip2")
		    && casestrcmp(x, cast_uchar "lzma")
		    && casestrcmp(x, cast_uchar "lzma2")
		    && casestrcmp(x, cast_uchar "xz")
		    && casestrcmp(x, cast_uchar "lz")
		    && !strchr(cast_const_char x, '-')
		    && strlen(cast_const_char x) <= 4)
			return stracpy(x);
	}
	return NULL;
}

static unsigned char *
get_content_encoding_from_content_type(unsigned char *ct)
{
	if (!casestrcmp(ct, cast_uchar "application/x-gzip")
	    || !casestrcmp(ct, cast_uchar "application/x-tgz")
	    || !casestrcmp(ct, cast_uchar "application/x-gtar"))
		return cast_uchar "gzip";
	if (!casestrcmp(ct, cast_uchar "application/x-br"))
		return cast_uchar "br";
	if (!casestrcmp(ct, cast_uchar "application/x-bzip2")
	    || !casestrcmp(ct, cast_uchar "application/x-bzip"))
		return cast_uchar "bzip2";
	if (!casestrcmp(ct, cast_uchar "application/x-lzma"))
		return cast_uchar "lzma";
	if (!casestrcmp(ct, cast_uchar "application/x-lzma2")
	    || !casestrcmp(ct, cast_uchar "application/x-xz"))
		return cast_uchar "lzma2";
	if (!casestrcmp(ct, cast_uchar "application/x-lz")
	    || !casestrcmp(ct, cast_uchar "application/x-lzip"))
		return cast_uchar "lzip";
	return NULL;
}

unsigned char *
get_content_type(unsigned char *head, unsigned char *url)
{
	unsigned char *ct;
	int code;
	if ((ct = parse_http_header(head, cast_uchar "Content-Type", NULL))) {
		unsigned char *s;
		if ((s = cast_uchar strchr(cast_const_char ct, ';')))
			*s = 0;
		while (*ct && ct[strlen(cast_const_char ct) - 1] <= ' ')
			ct[strlen(cast_const_char ct) - 1] = 0;
		if (*ct == '"' && ct[1]
		    && ct[strlen(cast_const_char ct) - 1] == '"') {
			memmove(ct, ct + 1, strlen(cast_const_char ct));
			ct[strlen(cast_const_char ct) - 1] = 0;
		}
		if (!casestrcmp(ct, cast_uchar "text/plain")
		    || !casestrcmp(ct, cast_uchar "application/octet-stream")
		    || !casestrcmp(ct, cast_uchar "application/octetstream")
		    || !casestrcmp(ct, cast_uchar "application/octet_stream")
		    || !casestrcmp(ct, cast_uchar "application/binary")
		    || !casestrcmp(ct, cast_uchar
		                   "application/x-www-form-urlencoded")
		    || get_content_encoding_from_content_type(ct)) {
			unsigned char *ctt;
			if (!get_http_code(head, &code, NULL) && code >= 300)
				goto no_code_by_extension;
			ctt =
			    get_content_type_by_header_and_extension(head, url);
			if (ctt) {
				free(ct);
				return ctt;
			}
		}
no_code_by_extension:
		if (!*ct)
			free(ct);
		else
			return ct;
	}
	if (!get_http_code(head, &code, NULL) && code >= 300)
		return stracpy(cast_uchar "text/html");
	ct = get_content_type_by_header_and_extension(head, url);
	if (ct)
		return ct;
	return !force_html ? stracpy(cast_uchar "text/plain")
	                   : stracpy(cast_uchar "text/html");
}

unsigned char *
get_content_encoding(unsigned char *head, unsigned char *url, int just_ce)
{
	unsigned char *ce, *ct, *ext;
	char *extd;
	unsigned char *u;
	int code;
	if ((ce = parse_http_header(head, cast_uchar "Content-Encoding", NULL)))
		return ce;
	if (just_ce)
		return NULL;
	if ((ct = parse_http_header(head, cast_uchar "Content-Type", NULL))) {
		unsigned char *s;
		if ((s = cast_uchar strchr(cast_const_char ct, ';')))
			*s = 0;
		ce = get_content_encoding_from_content_type(ct);
		if (ce) {
			free(ct);
			return stracpy(ce);
		}
		if (is_html_type(ct)) {
			free(ct);
			return NULL;
		}
		free(ct);
	}
	if (!get_http_code(head, &code, NULL) && code >= 300)
		return NULL;
	if (!(ext = get_url_data(url)))
		ext = url;
	for (u = ext; *u; u++)
		if (end_of_dir(url, *u))
			goto skip_ext;
	extd = strrchr((char *)ext, '.');
	if (extd) {
		ce = get_compress_by_extension(extd + 1, strchr(extd + 1, 0));
		if (ce)
			return stracpy(ce);
	}
skip_ext:
	if ((ext = get_filename_from_header(head))) {
		extd = strrchr((char *)ext, '.');
		if (extd) {
			ce = get_compress_by_extension(extd + 1,
			                               strchr(extd + 1, 0));
			if (ce) {
				free(ext);
				return stracpy(ce);
			}
		}
		free(ext);
	}
	return NULL;
}

unsigned char *
encoding_2_extension(unsigned char *encoding)
{
	if (!casestrcmp(encoding, cast_uchar "gzip")
	    || !casestrcmp(encoding, cast_uchar "x-gzip"))
		return cast_uchar "gz";
	if (!casestrcmp(encoding, cast_uchar "compress")
	    || !casestrcmp(encoding, cast_uchar "x-compress"))
		return cast_uchar "Z";
	if (!casestrcmp(encoding, cast_uchar "bzip2"))
		return cast_uchar "bz2";
	if (!casestrcmp(encoding, cast_uchar "lzma"))
		return cast_uchar "lzma";
	if (!casestrcmp(encoding, cast_uchar "lzma2"))
		return cast_uchar "xz";
	if (!casestrcmp(encoding, cast_uchar "lzip"))
		return cast_uchar "lz";
	return NULL;
}

/* returns field with associations */
struct assoc *
get_type_assoc(struct terminal *term, unsigned char *type, int *n)
{
	struct assoc *assoc_array;
	struct list *l = NULL;
	struct list_head *ll;
	int count = 0;
	foreach (struct list, l, ll, assoc.list_entry) {
		struct assoc *a = get_struct(l, struct assoc, head);
		if (a->system == SYSTEM_ID
		    && (term->environment & ENV_XWIN ? a->xwin : a->cons)
		    && is_in_list(a->ct, type,
		                  (int)strlen(cast_const_char type))) {
			if (count == INT_MAX)
				overalloc();
			count++;
		}
	}
	*n = count;
	if (!count)
		return NULL;
	if ((unsigned)count > INT_MAX / sizeof(struct assoc))
		overalloc();
	assoc_array = xmalloc(count * sizeof(struct assoc));
	count = 0;
	foreach (struct list, l, ll, assoc.list_entry) {
		struct assoc *a = get_struct(l, struct assoc, head);
		if (a->system == SYSTEM_ID
		    && (term->environment & ENV_XWIN ? a->xwin : a->cons)
		    && is_in_list(a->ct, type,
		                  (int)strlen(cast_const_char type)))
			assoc_array[count++] = *a;
	}
	return assoc_array;
}

int
is_html_type(unsigned char *ct)
{
	return !casestrcmp(ct, cast_uchar "text/html")
	       || !casestrcmp(ct, cast_uchar "text-html")
	       || !casestrcmp(ct, cast_uchar "text/x-server-parsed-html")
	       || !casestrcmp(ct, cast_uchar "text/xml")
	       || !casecmp(ct, cast_uchar "application/xhtml",
	                   strlen("application/xhtml"));
}

unsigned char *
get_filename_from_header(unsigned char *head)
{
	int extended = 0;
	unsigned char *ct, *x, *y, *codepage;
	int ly;
	if ((ct = parse_http_header(head, cast_uchar "Content-Disposition",
	                            NULL))) {
		x = parse_header_param(ct, cast_uchar "filename*", 1);
		if (x)
			extended = 1;
		else
			x = parse_header_param(ct, cast_uchar "filename", 1);
		free(ct);
		if (x) {
			if (*x)
				goto ret_x;
			free(x);
		}
	}
	if ((ct = parse_http_header(head, cast_uchar "Content-Type", NULL))) {
		x = parse_header_param(ct, cast_uchar "name*", 0);
		if (x)
			extended = 1;
		else
			x = parse_header_param(ct, cast_uchar "name", 0);
		free(ct);
		if (x) {
			if (*x)
				goto ret_x;
			free(x);
		}
	}
	return NULL;
ret_x:
	codepage = NULL;
	if (extended) {
		unsigned char *ap1, *ap2;
		ap1 = cast_uchar strchr(cast_const_char x, '\'');
		if (!ap1)
			goto no_extended;
		ap2 = cast_uchar strchr(cast_const_char(ap1 + 1), '\'');
		if (ap2)
			ap2++;
		else
			ap2 = ap1 + 1;
		codepage = memacpy(x, ap1 - x);
		memmove(x, ap2, strlen(cast_const_char ap2) + 1);
	}

no_extended:
	y = init_str();
	ly = 0;
	add_conv_str(&y, &ly, x, (int)strlen(cast_const_char x), -2);
	free(x);
	x = y;

	free(codepage);

	y = convert(0, 0, x, NULL);
	free(x);
	x = y;

	for (y = x; *y; y++)
		if (dir_sep(*y))
			*y = '-';
	return x;
}

unsigned char *
get_filename_from_url(unsigned char *url, unsigned char *head, int tmp)
{
	int ll = 0;
	unsigned char *u, *s, *e, *f, *x, *ww;
	unsigned char *ct, *want_ext;
	if (!casecmp(url, cast_uchar "data:", 5))
		url = cast_uchar "data:/data";
	want_ext = stracpy(cast_uchar "");
	f = get_filename_from_header(head);
	if (f)
		goto no_ct;
	if (!(u = get_url_data(url)))
		u = url;
	for (e = s = u; *e && !end_of_dir(url, *e); e++)
		if (dir_sep(*e))
			s = e + 1;
	ll = 0;
	f = init_str();
	add_conv_str(&f, &ll, s, (int)(e - s), -2);
	if (!(ct = parse_http_header(head, cast_uchar "Content-Type", NULL)))
		goto no_ct;
	free(ct);
	ct = get_content_type(head, url);
	if (ct) {
		x = get_extension_by_content_type(ct);
		if (x) {
			add_to_strn(&want_ext, cast_uchar ".");
			add_to_strn(&want_ext, x);
			free(x);
		}
		free(ct);
	}
no_ct:
	if (!*want_ext) {
		x = cast_uchar strrchr(cast_const_char f, '.');
		if (x) {
			free(want_ext);
			want_ext = stracpy(x);
		}
	}
	ct = get_content_encoding(head, url, 0);
	if (ct) {
		x = encoding_2_extension(ct);
		if (!tmp) {
			unsigned char *ct1;
			ct1 = get_content_encoding(head, url, 1);
			if (ct1)
				free(ct1);
			else if (x) {
				unsigned char *w = cast_uchar strrchr(
				    cast_const_char want_ext, '.');
				if (w
				    && (ww = (unsigned char *)
				            canonical_compressed_ext(
						(char *)(w + 1), NULL))
				    && !casestrcmp(x, ww))
					goto skip_want_ext;
				if (w && !casestrcmp(w + 1, x))
					goto skip_want_ext;
				add_to_strn(&want_ext, cast_uchar ".");
				add_to_strn(&want_ext, x);
skip_want_ext:;
			}
		} else if (x) {
			if (strlen(cast_const_char x) + 1
			        < strlen(cast_const_char f)
			    && f[strlen(cast_const_char f)
			         - strlen(cast_const_char x) - 1]
			           == '.'
			    && !casestrcmp(f + strlen(cast_const_char f)
			                       - strlen(cast_const_char x),
			                   x)) {
				f[strlen(cast_const_char f)
				  - strlen(cast_const_char x) - 1] = 0;
			}
		}
		free(ct);
	}
	if (strlen(cast_const_char want_ext) > strlen(cast_const_char f)
	    || casestrcmp(want_ext, f + strlen(cast_const_char f)
	                                - strlen(cast_const_char want_ext))) {
		x = cast_uchar strrchr(cast_const_char f, '.');
		if (x
		    && (ww = (unsigned char *)canonical_compressed_ext(
			    (char *)(x + 1), NULL))
		    && want_ext[0] == '.' && !casestrcmp(want_ext + 1, ww))
			goto skip_tgz_2;
		if (x)
			*x = 0;
		add_to_strn(&f, want_ext);
skip_tgz_2:;
	}
	free(want_ext);
	return f;
}

void
free_types(void)
{
	struct list *l = NULL;
	struct list_head *ll;
	foreach (struct list, l, ll, assoc.list_entry) {
		struct assoc *a = get_struct(l, struct assoc, head);
		free(a->ct);
		free(a->prog);
		free(a->label);
		ll = ll->prev;
		del_from_list(&a->head);
		free(a);
	}
	foreach (struct list, l, ll, extensions.list_entry) {
		struct extension *e = get_struct(l, struct extension, head);
		free(e->ext);
		free(e->ct);
		ll = ll->prev;
		del_from_list(&e->head);
		free(e);
	}

	free_history(ext_search_history);
	free_history(assoc_search_history);
}
