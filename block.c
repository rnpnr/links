#include "links.h"

static struct list *block_new_item(void *ignore);
static void block_delete_item(struct list *data);
static void block_copy_item(struct list *in, struct list *out);
static unsigned char *block_type_item(struct terminal *term, struct list *data, int x);
static void block_edit_item(struct dialog_data *dlg, struct list *data, void (*ok_fn)(struct dialog_data *, struct list *, struct list *, struct list_description *), struct list *ok_arg, unsigned char dlg_title);
static struct list *block_find_item(struct list *start, unsigned char *str, int direction);

static struct history block_search_histroy = { 0, {&block_search_histroy.items, &block_search_histroy.items} };

struct list blocks = { init_list_1st(&blocks.list_entry) 0, -1, NULL, init_list_last(&blocks.list_entry) };

static struct list_description blocks_ld = {
	0,			/* flat */
	&blocks,		/* list head */
	block_new_item,
	block_edit_item,
	NULL,
	block_delete_item,
	block_copy_item,
	block_type_item,
	block_find_item,
	&block_search_histroy,
	0,			/* this is set in init_assoc function */
	15,			/* # of items in main window */
	T_BLOCKED_IMAGE,	/* item title */
	T_BLOCK_LIST_IN_USE,	/* Already open message */
	T_BLOCK_LIST_MANAGER,	/* Window title */
	T_BLOCK_DELETE,
	0,	/* no button */
	NULL,	/* no button */
	NULL,	/* no save */

	NULL, NULL, 0, 0,  /* internal vars */
	0, /* modified */
	NULL,
	NULL,
	0,
};


static struct list *block_new_item(void *ignore)
{
	/*Default constructor*/
	struct block *neww;

	neww = xmalloc(sizeof(struct block));
	neww->url = stracpy(cast_uchar "");
	return &neww->head;
}

static void block_delete_item(struct list *data)
{
	/*Destructor */
	struct block *del = get_struct(data, struct block, head);
	if (del->head.list_entry.next) del_from_list(&del->head);
	free(del->url);
	free(del);
}

static void block_copy_item(struct list *in, struct list *out)
{
	/*Copy construction */
	struct block *item_in = get_struct(in, struct block, head);
	struct block *item_out = get_struct(out, struct block, head);

	free(item_out->url);
	item_out->url = stracpy(item_in->url);
}

/*This is used to display the items in the menu*/
static unsigned char *block_type_item(struct terminal *term, struct list *data, int x)
{
	unsigned char *txt, *txt1;
	struct block *item;

	if (data == &blocks) return stracpy(get_text_translation(TEXT_(T_BLOCK_LIST), term));

	item = get_struct(data, struct block, head);
	txt = stracpy(item->url);

	/*I have no idea what this does, but it os copied from working code in types.c*/
	txt1 = convert(blocks_ld.codepage, term_charset(term), txt, NULL);
	free(txt);

	return txt1;
}

struct assoc_ok_struct {
	void (*fn)(struct dialog_data *, struct list *, struct list *, struct list_description *);
	struct list *data;
	struct dialog_data *dlg;
};

/* destroys an item, this function is called when edit window is aborted */
static void block_edit_abort(struct dialog_data *data)
{
	struct block *item = (struct block *)data->dlg->udata;
	struct dialog *dlg = data->dlg;

	free(dlg->udata2);
	if (item)
		block_delete_item(&item->head);
}

/* Puts url into the block list */
static void block_edit_done(void *data)
{
	/*Copied from types.c*/
	struct dialog *d = (struct dialog *)data;
	struct block *item = (struct block *)d->udata;
	struct assoc_ok_struct *s = (struct assoc_ok_struct *)d->udata2;
	unsigned char *txt;
	unsigned char *url;

	/*See block_edit_item*/
	url = (unsigned char *)&d->items[4];

	txt = convert(term_charset(s->dlg->win->term), blocks_ld.codepage, url, NULL);
	free(item->url);
	item->url = txt;

	s->fn(s->dlg, s->data, &item->head, &blocks_ld);
	d->udata = NULL;  /* for abort function */
}

static void block_edit_item_fn(struct dialog_data *dlg)
{
	/*Copied from input_field. I don't know how most of it works.*/
#define LL gf_val(1, G_BFU_FONT_SIZE)
	struct terminal *term = dlg->win->term;
	int max = 0, min = 0;
	int w, rw;
	int y = gf_val(-1, -G_BFU_FONT_SIZE);
	unsigned char *text = TEXT_(T_ENTER_URL);


	max_text_width(term, text, &max, AL_LEFT);
	min_text_width(term, text, &min, AL_LEFT);
	max_buttons_width(term, dlg->items + 1, 2, &max);
	min_buttons_width(term, dlg->items + 1, 2, &min);
	if (max < dlg->dlg->items->dlen)
		max = dlg->dlg->items->dlen;
	w = term->x * 9 / 10 - 2 * DIALOG_LB;
	if (w > max)
		w = max;
	if (w < min)
		w = min;
	rw = w;
	dlg_format_text_and_field(dlg, NULL, text, dlg->items, 0, &y, w, &rw, COLOR_DIALOG_TEXT, AL_LEFT);
	y += LL;
	dlg_format_buttons(dlg, NULL, dlg->items + 1, 2, 0, &y, w, &rw, AL_CENTER);
	w = rw;
	dlg->xw = rw + 2 * DIALOG_LB;
	dlg->yw = y + 2 * DIALOG_TB;
	center_dlg(dlg);
	draw_dlg(dlg);
	y = dlg->y + DIALOG_TB;
	dlg_format_text_and_field(dlg, term, text, dlg->items, dlg->x + DIALOG_LB, &y, w, NULL, COLOR_DIALOG_TEXT, AL_LEFT);
	y += LL;
	dlg_format_buttons(dlg, term, dlg->items + 1, 2, dlg->x + DIALOG_LB, &y, w, NULL, AL_CENTER);
}

static void block_edit_item(struct dialog_data *dlg, struct list *data, void (*ok_fn)(struct dialog_data *, struct list *, struct list *, struct list_description *), struct list *ok_arg, unsigned char dlg_title)
{
	/*Copied from types.c */
	/*Data is a new item generated by the "default" function*/
	struct block *neww = get_struct(data, struct block, head);

	struct terminal *term = dlg->win->term;
	struct dialog *d;
	struct assoc_ok_struct *s;
	unsigned char *url, *txt;

	/*Allocate space for dialog, 4 items followed by 1 string*/
	d = xmalloc(sizeof(struct dialog) + 4 * sizeof(struct dialog_item)
			+ 1 * MAX_STR_LEN);
	memset(d, 0, sizeof(struct dialog) + 4 * sizeof(struct dialog_item)
			+ 1 * MAX_STR_LEN);

	/*Set up this string */
	url = (unsigned char *)&d->items[4];
	txt = convert(blocks_ld.codepage, term_charset(dlg->win->term), neww->url, NULL);
	safe_strncpy(url, txt, MAX_STR_LEN);
	free(txt);

	/* Create the dialog */
	s = xmalloc(sizeof(struct assoc_ok_struct));

	s->fn = ok_fn;
	s->data = ok_arg;
	s->dlg = dlg;

	switch (dlg_title) {
	case TITLE_EDIT:
		d->title = TEXT_(T_BLOCK_EDIT);
		break;

	case TITLE_ADD:
		d->title = TEXT_(T_BLOCK_ADD);
		break;

	default:
		internal("Unsupported dialog title.\n");
	}

	d->udata = neww;
	d->udata2 = s;
	d->fn = block_edit_item_fn;
	d->abort = block_edit_abort;
	d->refresh = block_edit_done;
	d->refresh_data = d;
	d->items[0].type = D_FIELD;
	d->items[0].dlen = MAX_STR_LEN;
	d->items[0].data = url;
	d->items[0].fn = check_nonempty;
	d->items[1].type = D_BUTTON;
	d->items[1].gid = B_ENTER;
	d->items[1].fn = ok_dialog;
	d->items[1].text = TEXT_(T_OK);
	d->items[2].type = D_BUTTON;
	d->items[2].gid = B_ESC;
	d->items[2].text = TEXT_(T_CANCEL);
	d->items[2].fn = cancel_dialog;
	d->items[3].type = D_END;
	do_dialog(term, d, getml(d, NULL));
}

static int test_entry(struct list *e, unsigned char *str)
{
	return casestrstr(get_struct(e, struct block, head)->url, str);
}

static struct list *block_find_item(struct list *s, unsigned char *str, int direction)
{
	struct list *e;

	if (direction >= 0) {
		for (e = list_next(s); e != s; e = list_next(e)) {
			if (e->depth >= 0 && test_entry(e, str))
				return e;
		}
	} else {
		for (e = list_prev(s); e != s; e = list_prev(e)) {
			if (e->depth >= 0 && test_entry(e, str))
				return e;
		}
	}

	if (e->depth >= 0 && test_entry(e, str))
		return e;

	return NULL;
}


void block_manager(struct terminal *term, void *fcp, void *ses_)
{
	struct session *ses = (struct session *)ses_;
	create_list_window(&blocks_ld, &blocks, term, ses);
}


void *block_url_add(void *ses_, unsigned char *url)
{
	struct session *ses = (struct session *)ses_;
	/*Callback from the dialog box created from the link menu*/
	struct list *new_list;
	struct block *new_b;
	struct terminal *term = ses ? ses->term : NULL;

	if (test_list_window_in_use(&blocks_ld, term))
		return NULL;

	new_list = block_new_item(0);
	new_b = get_struct(new_list, struct block, head);

	free(new_b->url);
	new_b->url = stracpy(url);
	new_b->head.type = 0;

	add_to_list(blocks.list_entry, &new_b->head);
	return NULL;
}

void block_url_query(struct session *ses, unsigned char *u)
{
	if (test_list_window_in_use(&blocks_ld, ses->term))
		return;

	input_field(ses->term, NULL, TEXT_(T_BLOCK_URL), TEXT_(T_BLOCK_ADD), ses, 0, MAX_INPUT_URL_LEN, u, 0, 0, NULL, 2, TEXT_(T_OK), block_url_add, TEXT_(T_CANCEL), input_field_null);

}

static unsigned char *find_first_match(unsigned char *s, unsigned char *p, unsigned *ii)
{
	unsigned i;
	retry:
	for (i = 0; s[i] && p[i] && p[i] != '*'; i++) {
		if (s[i] != p[i] && p[i] != '?') {
			s++;
			goto retry;
		}
	}
	*ii = i;
	if (!p[i] || p[i] == '*')
		return s;
	return NULL;
}

static int simple_glob_match(unsigned char *s, unsigned char *p)
{
	unsigned i;
	if (find_first_match(s, p, &i) != s)
		return 0;
	if (!p[i])
		return !s[i];
	while (1) {
		s += i;
		p += i + 1;
		if (!(s = find_first_match(s, p, &i)))
			return 0;
		if (!p[i]) {
			s += strlen(cast_const_char s) - i;
			return !!find_first_match(s, p, &i);
		}
	}
}


int is_url_blocked(unsigned char *url)
{
	struct list *b;
	struct list_head *lb;

	foreach(struct list, b, lb, blocks.list_entry) {
		if (simple_glob_match(url, get_struct(b, struct block, head)->url))
			return 1;
	}

	return 0;
}

void init_blocks(void)
{
	blocks_ld.codepage = utf8_table;
}

void free_blocks(void)
{
	/*List destructor */
	struct list *b;
	struct list_head *lb;

	foreach(struct list, b, lb, blocks.list_entry) {
		struct block *bm = get_struct(b, struct block, head);
		free(bm->url);
		lb = lb->prev;
		del_from_list(b);
		free(bm);
	}

	free_history(block_search_histroy);
}
