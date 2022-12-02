/* listedit.c
 * (c) 2002 Petr 'Brain' Kulhavy
 * This file is a part of the Links program, released under GPL.
 */

#include "links.h"

/*
                                    (#####)
                                  (#########)
                             ))))  (######)
                            ,C--O   (###)      _________________
                            |`:, \  ~~        |.  ,       v  , .|
                            `-__o  ~~  ;      |  ZAKAZ KOURENI  |
                            / _  \     `      |.               .|
                           | ( \__`_=k==      `~~~~~~~~~~~~~~~~~'
                           | `-/__---'
                           |=====C/
                           `.   ),'
                             \  /
                             ||_|
                             || |__
                             ||____)

                         v v  v   ,       ,v
                     KONECNE NEJAKE KOMENTARE...
*/

/* Klikani myssi (nebo mysijou, jak rika Mikulas) v list-okne:
 * (hrozne dulezity komentar ;-P )
 *
 * Klikani je vyreseno nasledovne, pokud ma nekdo lepsi napad, nebo nejake
 * vyhrady, tak at mi je posle.
 *
 * Prostredni tlacitko+pohyb je scroll nahoru/dolu. Levym tlacitkem se nastavi
 * kurzor (cerna lista) na konkretni polozku. Kdyz se levym klikne na adresar
 * (na ty graficke nesmysly, ne na ten text), tak se adresar toggle
 * otevre/zavre. Prave tlacitko oznaci/odznaci polozku/adresar.
 */

/* Premistovani polozek:
 *
 * Pravym tlacitkem se oznaci/odznaci polozka. Cudlikem "Odznacit vse" se
 * vsechny polozky odznaci. Cudlik "Prestehovat" presune vsechny oznacene
 * polozky za aktualni pozici, v tom poradi, jak jsou v seznamu. Pri zavreni
 * adresare se vsechny polozky v adresari odznaci.
 */

/* Prekreslovani grafickych nesmyslu v okenku je samozrejme bez jedineho
 *                                                               v
 * bliknuti. Ne jako nejmenovane browsery... Proste obraz jako BIC (TM)
 */

/* Ovladani klavesnici:
 * sipky, page up, page down, home end			pohyb
 * +							otevri adresar
 * -							zavri adresar
 * mezera						toggle adresar
 * ins, *, 8, i						toggle oznacit
 * ?, /, N, n						hledani nahoru, dolu,
 * znova, znova na druhou stranu
 */

/*
 * Struktura struct list_decription obsahuje popis seznamu. Tenhle file
 * obsahuje obecne funkce k obsluze seznamu. Pomoci struct list_description se
 * seznam customizuje. Obecne funkce volaji funkce z list_description.
 *
 * Jedina funkce z tohoto filu, ktera se vola zvenku, je create_list_window. Ta
 * vyrobi a obstarava okno pro obsluhu seznamu.
 *
 * Obecny list neresi veci jako nahravani seznamu z filu, ukladani na disk
 * atd.(tyhle funkce si uzivatel musi napsat sam). Resi vlastne jenom to velke
 * okno na manipulaci se seznamem.
 */

/*
 * Aby bylo konzistentni pridavani a editovani polozek, tak se musi pytlacit.
 *
 * To znamena, ze pri pridavani polozky do listu se vyrobi nova polozka
 * (NEPRIDA se do seznamu), pusti se edit a po zmacknuti OK se polozka prida do
 * seznamu. Pri zmacknuti cancel, se polozka smaze.
 *
 * Pri editovani polozky se vyrobi nova polozka, zkopiruje se do ni obsah te
 * puvodni (od toho tam je funkce copy_item), pak se zavola edit a podobne jako
 * v predchozim pripade: pri cancel se polozka smaze, pri OK se zkopiruje do
 * puvodni polozky a smaze se taky.
 *
 * O smazani polozky pri cancelu se bude starat uzivatelska funkce edit_item
 * sama. Funkce edit_item po zmacknuti OK zavola funkci, kterou dostane. Jako
 * 1. argument ji da data, ktera dostane, jako 2. argument ji preda pointer na
 * item.
 */

/*
 * Seznam je definovan ve struct list. Muze byt bud placaty nebo stromovy.
 *
 * K placatemu asi neni co dodat. U placateho se ignoruje hloubka a neexistuji
 * adresare - vsechny polozky jsou si rovny (typ polozky se ignoruje).
 *
 * Stromovy seznam:
 * Kazdy clen seznamu ma flag sbaleno/rozbaleno. U itemy se to ignoruje, u
 * adresare to znamena, zda se zobrazuje obsah nebo ne. Aby rozbaleno/sbaleno
 * bylo u vsech prvku adresare, to by neslo: kdybych mel adresar a v nem dalsi
 * adresar, tak bych u toho vnoreneho adresare nevedel, jestli to
 * sbaleno/rozbaleno je od toho vnoreneho adresare, nebo od toho nad nim.
 *
 * Clenove seznamu maji hloubku - cislo od 0 vyse. Cim je prvek hloubeji ve
 * strukture, tim je hloubka vyssi. Obsah adresare s hloubkou x je souvisly blok
 * nasledujicich prvku s hloubkou >x.
 *
 * Hlava ma hloubku -1 a zobrazuje se taky jako clen seznamu (aby se dal
 * zobrazit prazdny seznam a dalo se do nej pridavat), takze se da vlastne cely
 * seznam zabalit/rozbalit. Hlava sice nema data, ale funkce type_item ji musi
 * umet zobrazit. Jako popis bude psat fixni text, napriklad "Bookmarks".
 *
 * Pro urychleni vykreslovani kazdy prvek v seznamu s adresarema obsahuje
 * pointer na otce (polozka fotr). U plocheho seznamu se tento pointer
 * ignoruje.
 *
 * Strukturu stromu bude vykreslovat obecna funkce (v tomto filu), protoze v
 * obecnem listu je struktura uz zachycena.
 */

/*
 * V hlavnim okne se da nadefinovat 1 uzivatelske tlacitko. Polozka button ve
 * struct list_description obsahuje popisku tlacitka (kod stringu v
 * prekodovavacich tabulkach). Funkce button_fn je zavolana pri stisku
 * tlacitka, jako argument (void *) dostane aktualni polozku.  Pokud je
 * button_fn NULL, tlacitko se nekona.
 *
 * Toto tlacitko se da vyuzit napriklad u bookmarku, kde je potreba [ Goto ].
 *
 * Kdyz bude potreba predavat obsluzne funkci tlacitka nejake dalsi argumenty,
 * tak se pripadne definice obsluzne funkce prepise.
 *
 * Tlacitko funguje jen na polozky. Nefunguje na adresare (pokud se jedna o
 * stromovy list) ani na hlavu.
 */

/* Jak funguje default_value:
 * kdyz se zmackne tlacitko add, zavola se funkce default_value, ktera si
 * naalokuje nejaky data pro new_item. Do funkce default_value se treba u
 * bookmarku umisti okopirovani altualniho nazvu a url stranky. Pak se zavola
 * new_item, ktera si prislusne hodnoty dekoduje a pomoci nich vyplni novou
 * polozku. Funkce new_item pak MUSI data dealokovat. Pokud funkce new_item
 * dostane misto pointeru s daty NULL, vyrobi prazdnou polozku.
 *
 * Default value musi vratit hodnoty v kodovani uvedenem v list_description
 */

/* Pristupovani z vice linksu:
 *
 * ... se neresi - je zakazano. K tomu slouzi polozka open ve struct
 * list_description, ktera rika, jestli je okno uz otevrene, nebo ne.
 */

/* Prekodovavani znakovych sad:
 *
 * type_item vraci text prekodovany do kodovani terminalu, ktery dostane.
 */

/* struct list *current_pos;   current cursor position in the list */
/* struct list *win_offset;    item at the top of the window */
/* int win_pos;                current y position in the window */

#define BOHNICE "+420-2-84016111"

#define BFU_ELEMENT_EMPTY       0
#define BFU_ELEMENT_PIPE        1
#define BFU_ELEMENT_L           2
#define BFU_ELEMENT_TEE         3
#define BFU_ELEMENT_CLOSED      4
#define BFU_ELEMENT_CLOSED_DOWN 5
#define BFU_ELEMENT_OPEN        6
#define BFU_ELEMENT_OPEN_DOWN   7

/* for mouse scrolling */
static long last_mouse_y;

static void list_edit_toggle(struct dialog_data *dlg,
                             struct list_description *ld);

#define sirka_scrollovadla 0

/* This function uses these defines from setup.h:
 *
 * BFU_ELEMENT_WIDTH
 */

/* draws one of BFU elements: | |- [-] [+] */
/* BFU elements are used in the list window */
/* this function also defines shape and size of the elements */
/* returns width of the BFU element (all elements have the same size, but sizes
 * differ if we're in text mode or in graphics mode) */
static int
draw_bfu_element(struct terminal *term, int x, int y, unsigned char c, long b,
                 long f, unsigned char type, unsigned char selected)
{
	unsigned char vertical = 179;
	unsigned char horizontal = 196;
	unsigned char tee = 195;
	unsigned char l = 192;

	switch (type) {
	case BFU_ELEMENT_EMPTY:
		c |= ATTR_FRAME;
		set_char(term, x, y, ' ', c);
		set_char(term, x + 1, y, ' ', c);
		set_char(term, x + 2, y, ' ', c);
		set_char(term, x + 3, y, ' ', c);
		set_char(term, x + 4, y, ' ', c);
		break;

	case BFU_ELEMENT_PIPE:
		c |= ATTR_FRAME;
		set_char(term, x, y, ' ', c);
		set_char(term, x + 1, y, vertical, c);
		set_char(term, x + 2, y, ' ', c);
		set_char(term, x + 3, y, ' ', c);
		set_char(term, x + 4, y, ' ', c);
		break;

	case BFU_ELEMENT_L:
		c |= ATTR_FRAME;
		set_char(term, x, y, ' ', c);
		set_char(term, x + 1, y, l, c);
		set_char(term, x + 2, y, horizontal, c);
		set_char(term, x + 3, y, horizontal, c);
		set_char(term, x + 4, y, ' ', c);
		break;

	case BFU_ELEMENT_TEE:
		c |= ATTR_FRAME;
		set_char(term, x, y, ' ', c);
		set_char(term, x + 1, y, tee, c);
		set_char(term, x + 2, y, horizontal, c);
		set_char(term, x + 3, y, horizontal, c);
		set_char(term, x + 4, y, ' ', c);
		break;

	case BFU_ELEMENT_CLOSED:
	case BFU_ELEMENT_CLOSED_DOWN:
		set_char(term, x, y, '[', c);
		set_char(term, x + 1, y, '+', c);
		set_char(term, x + 2, y, ']', c);
		c |= ATTR_FRAME;
		set_char(term, x + 3, y, horizontal, c);
		set_char(term, x + 4, y, ' ', c);
		break;

	case BFU_ELEMENT_OPEN:
	case BFU_ELEMENT_OPEN_DOWN:
		set_char(term, x, y, '[', c);
		set_char(term, x + 1, y, '-', c);
		set_char(term, x + 2, y, ']', c);
		c |= ATTR_FRAME;
		set_char(term, x + 3, y, horizontal, c);
		set_char(term, x + 4, y, ' ', c);
		break;

	default:
		internal("draw_bfu_element: unknown BFU element type %d.\n",
		         type);
	}
	if (selected)
		set_char(term, x + 4, y, '*', c);
	return BFU_ELEMENT_WIDTH; /* BFU element size in text mode */
}

/* aux structure for parameter exchange for redrawing list window */
struct redraw_data {
	struct list_description *ld;
	struct dialog_data *dlg;
	int n;
};

static void redraw_list(struct terminal *term, void *bla);

/* returns next visible item in tree list */
/* works only with visible items (head or any item returned by this function) */
/* when list is flat returns next item */
static struct list *
next_in_tree(struct list_description *ld, struct list *item)
{
	int depth = item->depth;

	/* flat list */
	if (!ld->type)
		return list_next(item);

	if (!(item->type & 1) || (item->type & 2)) /* item or opened folder */
		return list_next(item);
	/* skip content of this folder */
	do
		item = list_next(item);
	while (item->depth
	       > depth); /* must stop on head 'cause it's depth is -1 */
	return item;
}

/* returns previous visible item in tree list */
/* works only with visible items (head or any item returned by this function) */
/* when list is flat returns previous item */
static struct list *
prev_in_tree(struct list_description *ld, struct list *item)
{
	struct list *last_closed;
	int depth = item->depth;

	/* flat list */
	if (!ld->type)
		return list_prev(item);

	if (item == ld->list)
		depth = 0;

	/* items with same or lower depth must be visible, because current item
	 * is visible */
	if (list_prev(item)->depth <= item->depth)
		return list_prev(item);

	/* find item followed with opened fotr's only */
	/* searched item is last closed folder (going up from item) or
	 * item->prev */
	last_closed = list_prev(item);
	item = list_prev(item);
	while (1) {
		if ((item->type & 3) == 1) /* closed folder */
			last_closed = item;
		if (item->depth <= depth)
			break;
		item = item->fotr;
	}
	return last_closed;
}

static int
get_win_pos(struct list_description *ld)
{
	struct list *l;
	int count = 0;

	for (l = ld->win_offset; l != ld->current_pos; l = next_in_tree(ld, l))
		count++;

	return count;
}

static void
unselect_in_folder(struct list_description *ld, struct list *l)
{
	int depth;

	depth = l->depth;
	for (l = list_next(l); l != ld->list && l->depth > depth;
	     l = list_next(l))
		l->type &= ~4;
}

/* aux function for list_item_add */
static void
list_insert_behind_item(struct dialog_data *dlg, struct list *pos,
                        struct list *item, struct list_description *ld)
{
	struct list *a;
	struct redraw_data rd;

	/*  BEFORE:  ... <----> pos <--(possible invisible items)--> a <---->
	 * ... */
	/*  AFTER:   ... <----> pos <--(possible invisible items)--> item <---->
	 * a <----> ... */

	a = next_in_tree(ld, pos);
	add_before_pos(a, item);

	/* if list is flat a->fotr will contain nosenses, but it won't crash ;-)
	 */
	if ((pos->type & 3) == 3 || pos->depth == -1) {
		item->fotr = pos;
		item->depth = pos->depth + 1;
	} /* open directory or head */
	else {
		item->fotr = pos->fotr;
		item->depth = pos->depth;
	}

	ld->current_pos =
	    next_in_tree(ld, ld->current_pos); /* ld->current_pos->next==item */
	ld->win_pos++;
	if (ld->win_pos > ld->n_items - 1) { /* scroll down */
		ld->win_pos = ld->n_items - 1;
		ld->win_offset = next_in_tree(ld, ld->win_offset);
	}

	ld->modified = 1;

	rd.ld = ld;
	rd.dlg = dlg;
	rd.n = 0;

	draw_to_window(dlg->win, redraw_list, &rd);
}

/* aux function for list_item_edit */
/* copies data of src to dest and calls free on the src */
/* first argument is argument passed to user function */
static void
list_copy_item(struct dialog_data *dlg, struct list *dest, struct list *src,
               struct list_description *ld)
{
	struct redraw_data rd;

	ld->copy_item(src, dest);
	ld->delete_item(src);

	ld->modified = 1; /* called after an successful edit */
	rd.ld = ld;
	rd.dlg = dlg;
	rd.n = 0;

	draw_to_window(dlg->win, redraw_list, &rd);
}

/* creates new item (calling new_item function) and calls edit_item function */
static int
list_item_add(struct dialog_data *dlg, struct dialog_item_data *useless)
{
	struct list_description *ld =
	    (struct list_description *)dlg->dlg->udata2;
	struct list *item = ld->current_pos;
	struct list *new_item;

	if (!(new_item = ld->new_item(ld->default_value ? ld->default_value(
					  (struct session *)dlg->dlg->udata, 0)
	                                                : NULL)))
		return 1;
	new_item->list_entry.prev = NULL;
	new_item->list_entry.next = NULL;
	new_item->type = 0;
	new_item->depth = 0;

	ld->edit_item(dlg, new_item, list_insert_behind_item, item, TITLE_ADD);
	return 0;
}

/* like list_item_add but creates folder */
static int
list_folder_add(struct dialog_data *dlg, struct dialog_item_data *useless)
{
	struct list_description *ld =
	    (struct list_description *)dlg->dlg->udata2;
	struct list *item = ld->current_pos;
	struct list *new_item;

	if (!(new_item = ld->new_item(NULL)))
		return 1;
	new_item->list_entry.prev = NULL;
	new_item->list_entry.next = NULL;
	new_item->type = 1 | 2;
	new_item->depth = 0;

	ld->edit_item(dlg, new_item, list_insert_behind_item, item, TITLE_ADD);
	return 0;
}

static int
list_item_edit(struct dialog_data *dlg, struct dialog_item_data *useless)
{
	struct list_description *ld =
	    (struct list_description *)dlg->dlg->udata2;
	struct list *item = ld->current_pos;
	struct list *new_item;

	if (item == ld->list)
		return 0; /* head */
	if (!(new_item = ld->new_item(NULL)))
		return 1;
	new_item->list_entry.prev = NULL;
	new_item->list_entry.next = NULL;

	ld->copy_item(item, new_item);
	ld->edit_item(dlg, new_item, list_copy_item, item, TITLE_EDIT);

	return 0;
}

static inline int
is_parent(struct list_description *ld, struct list *item, struct list *parent)
{
	struct list *l;

	if (ld->type)
		for (l = item; l->depth >= 0; l = l->fotr)
			if (l == parent)
				return 1;
	return 0;
}

static int
list_item_move(struct dialog_data *dlg, struct dialog_item_data *useless)
{
	struct list_description *ld =
	    (struct list_description *)dlg->dlg->udata2;
	struct list *i;
	struct list *behind = ld->current_pos;
	struct redraw_data rd;
	int window_moved = 0;
	int count = 0;

	if (ld->current_pos->type
	    & 4) { /* odznacime current_pos, kdyby nahodou byla oznacena */
		count++;
		ld->current_pos->type &= ~4;
	}

	for (i = list_next(ld->list); i != ld->list;) {
		struct list *next = next_in_tree(ld, i);
		struct list *prev = list_prev(i);
		struct list *behind_next = next_in_tree(
		    ld, behind); /* to se musi pocitat pokazdy, protoze by se
		                    nam mohlo stat, ze to je taky oznaceny */
		struct list *item_last =
		    list_prev(next); /* last item of moved block */

		if (!(i->type & 4)) {
			i = next;
			continue;
		}
		if (is_parent(
			ld, ld->current_pos,
			i)) { /* we're moving directory into itself - let's
			         behave like item was not selected */
			i->type &= ~4;
			i = next;
			count++;
			continue;
		}

		if ((i->type & 3) == 3) { /* dirty trick */
			i->type &= ~2;
			next = next_in_tree(ld, i);
			prev = list_prev(i);
			item_last = list_prev(next);
			i->type |= 2;
		}

		if (i == ld->win_offset) {
			window_moved = 1;
			if (next != ld->list)
				ld->win_offset = next;
		}

		/* upravime fotrisko a hloubku */
		if (ld->type) {
			int a = i->depth;
			struct list *l = i;

			if ((behind->type & 3) == 3
			    || behind == ld->list) { /* open folder or head */
				i->fotr = behind;
				i->depth = behind->depth + 1;
			} else {
				i->fotr = behind->fotr;
				i->depth = behind->depth;
			}
			a = i->depth - a;

			/* poopravime hloubku v adresari */
			while (l != item_last) {
				l = list_next(l);
				l->depth += a;
			}
		}

		if (behind_next == i)
			goto predratovano; /* to uz je vsechno, akorat menime
			                      hloubku */

		/* predratujeme ukazatele kolem bloku na stare pozici */
		prev->list_entry.next = &next->list_entry;
		next->list_entry.prev = &prev->list_entry;

		/* posuneme na novou pozici (tesne pred behind_next) */
		i->list_entry.prev = behind_next->list_entry.prev;
		behind_next->list_entry.prev->next = &i->list_entry;
		item_last->list_entry.next = &behind_next->list_entry;
		behind_next->list_entry.prev = &item_last->list_entry;

predratovano:
		/* odznacime */
		i->type &= ~4;
		unselect_in_folder(ld, i);

		/* upravime pointery pro dalsi krok */
		behind = i;
		i = next;
		count++;
	}

	if (window_moved) {
		ld->current_pos = ld->win_offset;
		ld->win_pos = 0;
	} else
		ld->win_pos = get_win_pos(ld);

	if (!count) {
		msg_box(dlg->win->term, /* terminal */
		        NULL,           /* blocks to free */
		        TEXT_(T_MOVE),  /* title */
		        AL_CENTER,      /* alignment */
		        TEXT_(T_NO_ITEMS_SELECTED), MSG_BOX_END, /* text */
		        NULL,                                    /* data */
		        1, /* # of buttons */
		        TEXT_(T_CANCEL), msg_box_null,
		        B_ESC | B_ENTER /* button1 */
		);
	} else {
		ld->modified = 1;
		rd.ld = ld;
		rd.dlg = dlg;
		rd.n = 0;
		draw_to_window(dlg->win, redraw_list, &rd);
	}
	return 0;
}

/* unselect all items */
static int
list_item_unselect(struct dialog_data *dlg, struct dialog_item_data *useless)
{
	struct list_description *ld =
	    (struct list_description *)dlg->dlg->udata2;
	struct list *item;
	struct redraw_data rd;

	item = ld->list;
	do {
		item->type &= ~4;
		item = list_next(item);
	} while (item != ld->list);

	rd.ld = ld;
	rd.dlg = dlg;
	rd.n = 0;

	draw_to_window(dlg->win, redraw_list, &rd);
	return 0;
}

/* user button function - calls button_fn with current item */
static int
list_item_button(struct dialog_data *dlg, struct dialog_item_data *useless)
{
	struct list_description *ld =
	    (struct list_description *)dlg->dlg->udata2;
	struct list *item = ld->current_pos;
	struct session *ses = (struct session *)dlg->dlg->udata;

	if (!ld->button_fn)
		internal("Links got schizophrenia! Call " BOHNICE ".\n");

	if (item == ld->list || list_empty(item->list_entry))
		return 0; /* head or empty list */

	if (ld->type && (item->type & 1)) {
		list_edit_toggle(dlg, ld);
		return 0; /* this is tree list and item is directory */
	}

	ld->button_fn(ses, item);
	cancel_dialog(dlg, useless);
	return 0;
}

struct ve_skodarne_je_jeste_vetsi_narez {
	struct list_description *ld;
	struct dialog_data *dlg;
	struct list *item;
};

/* when delete is confirmed adjusts current_pos and calls delete function */
static void
delete_ok(void *data)
{
	struct ve_skodarne_je_jeste_vetsi_narez *skd =
	    (struct ve_skodarne_je_jeste_vetsi_narez *)data;
	struct list_description *ld = skd->ld;
	struct list *item = skd->item;
	struct dialog_data *dlg = skd->dlg;
	struct redraw_data rd;

	/* strong premise: we can't delete head of the list */
	if (list_next(ld->current_pos) != ld->list) {
		if (ld->current_pos == ld->win_offset)
			ld->win_offset = list_next(ld->current_pos);
		ld->current_pos = list_next(ld->current_pos);
	} else {                  /* last item */
		if (!ld->win_pos) /* only one line on the screen */
			ld->win_offset = prev_in_tree(ld, ld->win_offset);
		else
			ld->win_pos--;
		ld->current_pos = prev_in_tree(ld, ld->current_pos);
	}

	ld->delete_item(item);

	rd.ld = ld;
	rd.dlg = dlg;
	rd.n = 0;

	ld->modified = 1;
	draw_to_window(dlg->win, redraw_list, &rd);
}

/* when delete folder is confirmed adjusts current_pos and calls delete function
 */
static void
delete_folder_recursively(void *data)
{
	struct ve_skodarne_je_jeste_vetsi_narez *skd =
	    (struct ve_skodarne_je_jeste_vetsi_narez *)data;
	struct list_description *ld = skd->ld;
	struct list *item = skd->item;
	struct dialog_data *dlg = skd->dlg;
	struct redraw_data rd;
	struct list *i, *j;
	int depth;

	for (i = list_next(item), depth = item->depth;
	     i != ld->list && i->depth > depth;) {
		j = i;
		i = list_next(i);
		ld->delete_item(j);
	}

	/* strong premise: we can't delete head of the list */
	if (list_next(ld->current_pos) != ld->list) {
		if (ld->current_pos == ld->win_offset)
			ld->win_offset = list_next(ld->current_pos);
		ld->current_pos = list_next(ld->current_pos);
	} else {                  /* last item */
		if (!ld->win_pos) /* only one line on the screen */
			ld->win_offset = prev_in_tree(ld, ld->win_offset);
		else
			ld->win_pos--;
		ld->current_pos = prev_in_tree(ld, ld->current_pos);
	}

	ld->delete_item(item);

	rd.ld = ld;
	rd.dlg = dlg;
	rd.n = 0;

	ld->modified = 1;
	draw_to_window(dlg->win, redraw_list, &rd);
}

/* tests if directory is emty */
static int
is_empty_dir(struct list_description *ld, struct list *dir)
{
	if (!ld->type)
		return 1; /* flat list */
	if (!(dir->type & 1))
		return 1; /* not a directory */

	return list_next(dir)->depth <= dir->depth; /* head depth is -1 */
}

/* delete dialog */
static int
list_item_delete(struct dialog_data *dlg, struct dialog_item_data *useless)
{
	struct terminal *term = dlg->win->term;
	struct list_description *ld =
	    (struct list_description *)(dlg->dlg->udata2);
	struct list *item = ld->current_pos;
	unsigned char *txt;
	struct ve_skodarne_je_jeste_vetsi_narez *narez;

	if (item == ld->list || list_empty(item->list_entry))
		return 0; /* head or empty list */

	narez = xmalloc(sizeof(struct ve_skodarne_je_jeste_vetsi_narez));
	narez->ld = ld;
	narez->item = item;
	narez->dlg = dlg;

	txt = ld->type_item(term, item, 0);
	if (!txt) {
		txt = stracpy(cast_uchar "");
	}

	if ((item->type) & 1) /* folder */
	{
		if (!is_empty_dir(ld, item))
			msg_box(term,                    /* terminal */
			        getml(txt, narez, NULL), /* blocks to free */
			        TEXT_(T_DELETE_FOLDER),  /* title */
			        AL_CENTER,               /* alignment */
			        TEXT_(T_FOLDER), cast_uchar " \"", txt,
			        cast_uchar "\" ",
			        TEXT_(T_NOT_EMPTY_SURE_DELETE),
			        MSG_BOX_END,   /* text */
			        (void *)narez, /* data for ld->delete_item */
			        2,             /* # of buttons */
			        TEXT_(T_NO), msg_box_null, B_ESC, /* button1 */
			        TEXT_(T_YES), delete_folder_recursively,
			        B_ENTER /* button2 */
			);
		else
			msg_box(term,                    /* terminal */
			        getml(txt, narez, NULL), /* blocks to free */
			        TEXT_(T_DELETE_FOLDER),  /* title */
			        AL_CENTER,               /* alignment */
			        TEXT_(T_SURE_DELETE), cast_uchar " ",
			        TEXT_(T_fOLDER), cast_uchar " \"", txt,
			        cast_uchar "\"?",
			        MSG_BOX_END,   /* null-terminated text */
			        (void *)narez, /* data for ld->delete_item */
			        2,             /* # of buttons */
			        TEXT_(T_YES), delete_ok, B_ENTER, /* button1 */
			        TEXT_(T_NO), msg_box_null, B_ESC  /* button2 */
			);
	} else                                          /* item */
		msg_box(term,                           /* terminal */
		        getml(txt, narez, NULL),        /* blocks to free */
		        TEXT_(ld->delete_dialog_title), /* title */
		        AL_CENTER,                      /* alignment */
		        TEXT_(T_SURE_DELETE), cast_uchar " ",
		        TEXT_(ld->item_description), cast_uchar " \"", txt,
		        cast_uchar "\"?",
		        MSG_BOX_END,   /* null-terminated text */
		        (void *)narez, /* data for ld->delete_item */
		        2,             /* # of buttons */
		        TEXT_(T_YES), delete_ok, B_ENTER, /* button1 */
		        TEXT_(T_NO), msg_box_null, B_ESC  /* button2 */
		);
	return 0;
}

static void
redraw_list_element(struct terminal *term, struct dialog_data *dlg, int y,
                    int w, struct list_description *ld, struct list *l)
{
	struct list *lx;
	unsigned char *xp;
	int xd;
	unsigned char *txt;
	int x = 0;
	unsigned char color = 0;
	long bgcolor = 0, fgcolor = 0;
	int b;
	unsigned char element;

	color = l == ld->current_pos ? COLOR_MENU_SELECTED : COLOR_MENU_TEXT;

	txt = ld->type_item(term, l, 1);
	if (!txt) {
		txt = stracpy(cast_uchar "");
	}

	/* everything except head */

	if (l != ld->list) {
		switch (ld->type) {
		case 0:
			element = BFU_ELEMENT_TEE;
			if (list_next(l) == ld->list)
				element = BFU_ELEMENT_L;
			x += draw_bfu_element(term, dlg->x + DIALOG_LB, y,
			                      color, bgcolor, fgcolor, element,
			                      l->type & 4);
			break;
		case 1:
			xp = xmalloc(l->depth + 1);
			memset(xp, 0, l->depth + 1);
			xd = l->depth + 1;
			for (lx = list_next(l); lx != ld->list;
			     lx = list_next(lx)) {
				if (lx->depth < xd) {
					xd = lx->depth;
					xp[xd] = 1;
					if (!xd)
						break;
				}
			}
			for (b = 0; b < l->depth; b++)
				x += draw_bfu_element(term,
				                      dlg->x + DIALOG_LB + x, y,
				                      color, bgcolor, fgcolor,
				                      xp[b] ? BFU_ELEMENT_PIPE
				                            : BFU_ELEMENT_EMPTY,
				                      0);
			if (l->depth >= 0) { /* everything except head */
				int o = xp[l->depth];
				switch (l->type & 1) {
				case 0: /* item */
					element =
					    o ? BFU_ELEMENT_TEE : BFU_ELEMENT_L;
					break;

				case 1: /* directory */
					if (l->type & 2) {
						element =
						    o ? BFU_ELEMENT_OPEN_DOWN
						      : BFU_ELEMENT_OPEN;
					} else {
						element =
						    o ? BFU_ELEMENT_CLOSED_DOWN
						      : BFU_ELEMENT_CLOSED;
					}
					break;

				default: /* this should never happen */
					internal("=8-Q  lunacy level too high! "
					         "Call " BOHNICE ".\n");
					element = BFU_ELEMENT_EMPTY;
				}
				x += draw_bfu_element(
				    term, dlg->x + DIALOG_LB + x, y, color,
				    bgcolor, fgcolor, element, l->type & 4);
			}
			free(xp);
			break;
		default:
			internal("Invalid list description type.\n"
			         "Somebody's probably shooting into memory.\n"
			         "_______________\n"
			         "`--|_____|--|___ `\\\n"
			         "             \"  \\___\\\n");
		}
	}

	print_text(term, dlg->x + x + DIALOG_LB, y, w - x, txt, color);
	x += strlen((char *)txt);
	fill_area(term, dlg->x + DIALOG_LB + x, y, w - x, 1, ' ', 0);
	set_line_color(term, dlg->x + DIALOG_LB + x, y, w - x, color);
	free(txt);
}

/* redraws list */
static void
redraw_list(struct terminal *term, void *bla)
{
	struct redraw_data *rd = (struct redraw_data *)bla;
	struct list_description *ld = rd->ld;
	struct dialog_data *dlg = rd->dlg;
	int y, a;
	struct list *l;
	int w = dlg->xw - 2 * DIALOG_LB;
	y = dlg->y + DIALOG_TB;

	for (a = 0, l = ld->win_offset; a < ld->n_items;) {
		redraw_list_element(term, dlg, y, w, ld, l);
		l = next_in_tree(ld, l);
		a++;
		y++;
		if (l == ld->list)
			break;
	}
	fill_area(term, dlg->x + DIALOG_LB, y, w, ld->n_items - a, ' ',
	          COLOR_MENU_TEXT);
}

/* moves cursor from old position to a new one */
/* direction: -1=old is previous, +1=old is next */
static void
redraw_list_line(struct terminal *term, void *bla)
{
	struct redraw_data *rd = (struct redraw_data *)bla;
	struct list_description *ld = rd->ld;
	struct dialog_data *dlg = rd->dlg;
	int direction = rd->n;
	int w = dlg->xw - 2 * DIALOG_LB;
	int y = dlg->y + DIALOG_TB + ld->win_pos;
	struct list *l;

	redraw_list_element(term, dlg, y, w, ld, ld->current_pos);
	if (!term->spec->block_cursor) {
		set_cursor(term, dlg->x + DIALOG_LB, y, dlg->x + DIALOG_LB, y);
	}
	y += direction;
	switch (direction) {
	case 0:
		l = NULL;
		break;
	case 1:
		l = next_in_tree(ld, ld->current_pos);
		break;
	case -1:
		l = prev_in_tree(ld, ld->current_pos);
		break;
	default:
		internal("redraw_list_line: invalid direction %d", direction);
		l = NULL;
		break;
	}
	if (l)
		redraw_list_element(term, dlg, y, w, ld, l);
}

/* like redraw_list, but scrolls window, prints new line to top/bottom */
/* in text mode calls redraw list */
/* direction: -1=up, 1=down */
static void
scroll_list(struct terminal *term, void *bla)
{
	redraw_list(term, bla);
}

static void
list_find_next(struct redraw_data *rd, int direction)
{
	struct list_description *ld = rd->ld;
	struct dialog_data *dlg = rd->dlg;
	struct session *ses = (struct session *)(dlg->dlg->udata);
	struct list *item;

	if (!ld->search_word) {
		msg_box(ses->term, NULL, TEXT_(T_SEARCH), AL_CENTER,
		        TEXT_(T_NO_PREVIOUS_SEARCH), MSG_BOX_END, NULL, 1,
		        TEXT_(T_CANCEL), msg_box_null, B_ENTER | B_ESC);
		return;
	}

	if ((item =
	         ld->find_item(ld->current_pos, ld->search_word, direction))) {
		struct list *l;
		ld->current_pos = item;
		ld->win_offset = item;
		ld->win_pos = 0;
		if (ld->type)
			for (l = item; l->depth >= 0; l = l->fotr)
				if (l != item)
					l->type |= 2;

		draw_to_window(dlg->win, redraw_list, rd);
		if (!ses->term->spec->block_cursor)
			set_cursor(ses->term, dlg->x + DIALOG_LB,
			           dlg->y + DIALOG_TB + ld->win_pos,
			           dlg->x + DIALOG_LB,
			           dlg->y + DIALOG_TB + ld->win_pos);
	} else
		msg_box(ses->term, NULL, TEXT_(T_SEARCH), AL_CENTER,
		        TEXT_(T_SEARCH_STRING_NOT_FOUND), MSG_BOX_END, NULL, 1,
		        TEXT_(T_CANCEL), msg_box_null, B_ENTER | B_ESC);
}

static void
list_search_for_back(void *rd_, unsigned char *str)
{
	struct redraw_data *rd = (struct redraw_data *)rd_;
	struct list_description *ld = rd->ld;

	if (!*str)
		return;
	if (!ld->open)
		return;

	free(ld->search_word);
	ld->search_word = to_utf8_upcase(str, 0);
	ld->search_direction = -1;

	list_find_next(rd, ld->search_direction);
}

static void
list_search_for(void *rd_, unsigned char *str)
{
	struct redraw_data *rd = (struct redraw_data *)rd_;
	struct list_description *ld = rd->ld;

	if (!*str)
		return;
	if (!ld->open)
		return;

	free(ld->search_word);
	ld->search_word = to_utf8_upcase(str, 0);
	ld->search_direction = 1;

	list_find_next(rd, ld->search_direction);
}

static void
list_edit_toggle(struct dialog_data *dlg, struct list_description *ld)
{
	static struct redraw_data rd;
	ld->current_pos->type ^= 2;
	if (!(ld->current_pos->type & 2))
		unselect_in_folder(ld, ld->current_pos);
	rd.ld = ld;
	rd.dlg = dlg;
	rd.n = 0;
	draw_to_window(dlg->win, redraw_list, &rd);
	draw_to_window(dlg->win, redraw_list_line, &rd); /* set cursor */
}

static int
list_event_handler(struct dialog_data *dlg, struct links_event *ev)
{
	struct list_description *ld =
	    (struct list_description *)(dlg->dlg->udata2);
	static struct redraw_data rd;
	struct session *ses = (struct session *)(dlg->dlg->udata);

	rd.ld = ld;
	rd.dlg = dlg;
	rd.n = 0;

	switch ((int)ev->ev) {
	case EV_KBD:
		if (ev->y & KBD_PASTING)
			break;
		if (ld->type == 1) /* tree list */
		{
			if (ev->x == ' ') /* toggle folder */
			{
				if (!((ld->current_pos->type) & 1))
					return EVENT_PROCESSED; /* item */

				list_edit_toggle(dlg, ld);
				return EVENT_PROCESSED;
			}
			if (ev->x == '+' || ev->x == '=') /* open folder */
			{
				if (!((ld->current_pos->type) & 1))
					return EVENT_PROCESSED; /* item */
				if ((ld->current_pos->type) & 2)
					return EVENT_PROCESSED; /* already open
					                         */
				list_edit_toggle(dlg, ld);
				return EVENT_PROCESSED;
			}
			if (ev->x == '-') /* close folder */
			{
				if (!((ld->current_pos->type) & 1))
					return EVENT_PROCESSED; /* item */
				if (!((ld->current_pos->type) & 2))
					return EVENT_PROCESSED; /* already
					                           closed */
				list_edit_toggle(dlg, ld);
				return EVENT_PROCESSED;
			}
		}
		if (ev->x == '/'
		    || (ev->x == KBD_FIND
		        && !(ev->y
		             & (KBD_SHIFT | KBD_CTRL
		                | KBD_ALT)))) /* search forward */
		{
			struct redraw_data *r;

			r = xmalloc(sizeof(struct redraw_data));
			r->ld = ld;
			r->dlg = dlg;

			input_field(ses->term, getml(r, NULL), TEXT_(T_SEARCH),
			            TEXT_(T_SEARCH_FOR_TEXT), r,
			            ld->search_history, MAX_INPUT_URL_LEN,
			            cast_uchar "", 0, 0, NULL, 2, TEXT_(T_OK),
			            list_search_for, TEXT_(T_CANCEL),
			            input_field_null);
			return EVENT_PROCESSED;
		}
		if (ev->x == '?'
		    || (ev->x == KBD_FIND
		        && ev->y
		               & (KBD_SHIFT | KBD_CTRL
		                  | KBD_ALT))) /* search back */
		{
			struct redraw_data *r;

			r = xmalloc(sizeof(struct redraw_data));
			r->ld = ld;
			r->dlg = dlg;

			input_field(
			    ses->term, getml(r, NULL), TEXT_(T_SEARCH_BACK),
			    TEXT_(T_SEARCH_FOR_TEXT), r, ld->search_history,
			    MAX_INPUT_URL_LEN, cast_uchar "", 0, 0, NULL, 2,
			    TEXT_(T_OK), list_search_for_back, TEXT_(T_CANCEL),
			    input_field_null);
			return EVENT_PROCESSED;
		}
		if ((ev->x == 'n' && !(ev->y & KBD_ALT))
		    || ev->x == KBD_REDO) /* find next */
		{
			list_find_next(&rd, ld->search_direction);
			return EVENT_PROCESSED;
		}
		if ((ev->x == 'N' && !(ev->y & KBD_ALT))
		    || ev->x == KBD_UNDO) /* find prev */
		{
			list_find_next(&rd, -ld->search_direction);
			return EVENT_PROCESSED;
		}
		if (ev->x == KBD_UP) {
			if (ld->current_pos == ld->list)
				goto kbd_up_redraw_exit; /* already on the top
				                          */
			ld->current_pos = prev_in_tree(ld, ld->current_pos);
			ld->win_pos--;
			rd.n = 1;
			if (ld->win_pos < 0) /* scroll up */
			{
				ld->win_pos = 0;
				ld->win_offset =
				    prev_in_tree(ld, ld->win_offset);
				draw_to_window(dlg->win, scroll_list, &rd);
			}
kbd_up_redraw_exit:
			draw_to_window(dlg->win, redraw_list_line, &rd);
			return EVENT_PROCESSED;
		}
		if (ev->x == 'i' || ev->x == '*' || ev->x == '8'
		    || ev->x == KBD_INS || ev->x == KBD_SELECT) {
			if (ld->current_pos != ld->list)
				ld->current_pos->type ^= 4;
			rd.n = -1;
			if (next_in_tree(ld, ld->current_pos)
			    == ld->list) /* already at the bottom */
			{
				draw_to_window(dlg->win, redraw_list_line, &rd);
				return EVENT_PROCESSED;
			}
			ld->current_pos = next_in_tree(ld, ld->current_pos);
			ld->win_pos++;
			if (ld->win_pos > ld->n_items - 1) /* scroll down */
			{
				ld->win_pos = ld->n_items - 1;
				ld->win_offset =
				    next_in_tree(ld, ld->win_offset);
				draw_to_window(dlg->win, scroll_list, &rd);
			}
			draw_to_window(dlg->win, redraw_list_line, &rd);
			return EVENT_PROCESSED;
		}
		if (ev->x == KBD_DOWN) {
			if (next_in_tree(ld, ld->current_pos) == ld->list)
				goto kbd_down_redraw_exit; /* already at the
				                              bottom */
			ld->current_pos = next_in_tree(ld, ld->current_pos);
			ld->win_pos++;
			rd.n = -1;
			if (ld->win_pos > ld->n_items - 1) /* scroll down */
			{
				ld->win_pos = ld->n_items - 1;
				ld->win_offset =
				    next_in_tree(ld, ld->win_offset);
				draw_to_window(dlg->win, scroll_list, &rd);
			}
kbd_down_redraw_exit:
			draw_to_window(dlg->win, redraw_list_line, &rd);
			return EVENT_PROCESSED;
		}
		if (ev->x == KBD_HOME
		    || (upcase(ev->x) == 'A' && ev->y & KBD_CTRL)) {
			if (ld->current_pos == ld->list)
				goto kbd_home_redraw_exit; /* already on the top
				                            */
			ld->win_offset = ld->list;
			ld->current_pos = ld->win_offset;
			ld->win_pos = 0;
			rd.n = 0;
			draw_to_window(dlg->win, redraw_list, &rd);
kbd_home_redraw_exit:
			draw_to_window(dlg->win, redraw_list_line,
			               &rd); /* set cursor */
			return EVENT_PROCESSED;
		}
		if (ev->x == KBD_END
		    || (upcase(ev->x) == 'E' && ev->y & KBD_CTRL)) {
			int a;

			if (ld->current_pos == prev_in_tree(ld, ld->list))
				goto kbd_end_redraw_exit; /* already on the top
				                           */
			ld->win_offset = prev_in_tree(ld, ld->list);
			for (a = 1;
			     a < ld->n_items && ld->win_offset != ld->list; a++)
				ld->win_offset =
				    prev_in_tree(ld, ld->win_offset);
			ld->current_pos = prev_in_tree(ld, ld->list);
			ld->win_pos = a - 1;
			rd.n = 0;
			draw_to_window(dlg->win, redraw_list, &rd);
kbd_end_redraw_exit:
			draw_to_window(dlg->win, redraw_list_line,
			               &rd); /* set cursor */
			return EVENT_PROCESSED;
		}
		if (ev->x == KBD_PAGE_UP
		    || (upcase(ev->x) == 'B' && ev->y & KBD_CTRL)) {
			int a;

			if (ld->current_pos == ld->list)
				goto kbd_page_up_redraw_exit; /* already on the
				                                 top */
			for (a = 0;
			     a < ld->n_items && ld->win_offset != ld->list;
			     a++) {
				ld->win_offset =
				    prev_in_tree(ld, ld->win_offset);
				ld->current_pos =
				    prev_in_tree(ld, ld->current_pos);
			}
			if (a < ld->n_items) {
				ld->current_pos = ld->win_offset;
				ld->win_pos = 0;
			}
			rd.n = 0;
			draw_to_window(dlg->win, redraw_list, &rd);
kbd_page_up_redraw_exit:
			draw_to_window(dlg->win, redraw_list_line,
			               &rd); /* set cursor */
			return EVENT_PROCESSED;
		}
		if (ev->x == KBD_PAGE_DOWN
		    || (upcase(ev->x) == 'F' && ev->y & KBD_CTRL)) {
			int a;
			struct list *p = ld->win_offset;

			if (ld->current_pos == prev_in_tree(ld, ld->list))
				goto kbd_page_down_redraw_exit; /* already on
				                                   the bottom */
			for (a = 0;
			     a < ld->n_items && ld->list != next_in_tree(ld, p);
			     a++)
				p = next_in_tree(ld, p);
			if (a < ld->n_items) /* already last screen */
			{
				ld->current_pos = p;
				ld->win_pos = a;
				rd.n = 0;
				draw_to_window(dlg->win, redraw_list, &rd);
				draw_to_window(dlg->win, redraw_list_line,
				               &rd); /* set cursor */
				return EVENT_PROCESSED;
			}
			/* here is whole screen only - the window was full
			 * before pressing the page-down key */
			/* p is pointing behind last item of the window (behind
			 * last visible item in the window) */
			for (a = 0; a < ld->n_items && p != ld->list; a++) {
				ld->win_offset =
				    next_in_tree(ld, ld->win_offset);
				ld->current_pos =
				    next_in_tree(ld, ld->current_pos);
				p = next_in_tree(ld, p);
			}
			if (a < ld->n_items) {
				ld->current_pos = prev_in_tree(ld, ld->list);
				ld->win_pos = ld->n_items - 1;
			}
			rd.n = 0;
			draw_to_window(dlg->win, redraw_list, &rd);
kbd_page_down_redraw_exit:
			draw_to_window(dlg->win, redraw_list_line,
			               &rd); /* set cursor */
			return EVENT_PROCESSED;
		}
		break;

	case EV_MOUSE:
		/* toggle select item */
		if ((ev->b & BM_ACT) == B_DOWN
		    && (ev->b & BM_BUTT) == B_RIGHT) {
			int n, a;
			struct list *l = ld->win_offset;

			last_mouse_y = ev->y;

			if ((ev->y) < (dlg->y + DIALOG_TB)
			    || ev->y >= (dlg->y + DIALOG_TB + ld->n_items)
			    || (ev->x) < (dlg->x + DIALOG_LB)
			    || (ev->x) > (dlg->x + dlg->xw - DIALOG_LB))
				break; /* out of the dialog */

			n = ev->y - dlg->y - DIALOG_TB;
			for (a = 0; a < n; a++) {
				struct list *l1;
				l1 =
				    next_in_tree(ld, l); /* current item under
				                            the mouse pointer */
				if (l1 == ld->list)
					goto break2;
				else
					l = l1;
			}

			l->type ^= 4;
			ld->current_pos = l;
			ld->win_pos = n;
			rd.n = 0;
			draw_to_window(dlg->win, redraw_list, &rd);
			draw_to_window(dlg->win, redraw_list_line,
			               &rd); /* set cursor */
			return EVENT_PROCESSED;
		}
		/* click on item */
		if (((ev->b & BM_ACT) == B_DOWN || (ev->b & BM_ACT) == B_DRAG)
		    && (ev->b & BM_BUTT) == B_LEFT) {
			int n, a;
			struct list *l = ld->win_offset;

			last_mouse_y = ev->y;

			if ((ev->y) < (dlg->y + DIALOG_TB)
			    || ev->y >= (dlg->y + DIALOG_TB + ld->n_items)
			    || (ev->x) < (dlg->x + DIALOG_LB)
			    || (ev->x) > (dlg->x + dlg->xw - DIALOG_LB))
				goto skip_item_click; /* out of the dialog */

			n = ev->y - dlg->y - DIALOG_TB;
			for (a = 0; a < n; a++) {
				struct list *l1;
				l1 =
				    next_in_tree(ld, l); /* current item under
				                            the mouse pointer */
				if (l1 == ld->list) {
					n = a;
					break;
				} else
					l = l1;
			}
			a = ld->type ? ((l->depth) >= 0 ? (l->depth) + 1 : 0)
			             : (l->depth >= 0);

			ld->current_pos = l;

			/* clicked on directory graphical stuff */
			if ((ev->b & BM_ACT) == B_DOWN && ld->type
			    && ev->x < (dlg->x + DIALOG_LB
			                + a * BFU_ELEMENT_WIDTH)
			    && (l->type & 1)) {
				l->type ^= 2;
				if (!(l->type & 2))
					unselect_in_folder(ld, ld->current_pos);
			}
			ld->win_pos = n;
			rd.n = 0;
			draw_to_window(dlg->win, redraw_list, &rd);
			draw_to_window(dlg->win, redraw_list_line,
			               &rd); /* set cursor */
			return EVENT_PROCESSED;
		}
		/* scroll with the bar */
skip_item_click:
		if ((ev->b & BM_ACT) == B_DRAG
		    && (ev->b & BM_BUTT) == B_MIDDLE) {
			long delta = ev->y - last_mouse_y;

			last_mouse_y = ev->y;
			if (delta > 0) /* scroll down */
			{
				if (next_in_tree(ld, ld->current_pos)
				    == ld->list)
					return EVENT_PROCESSED; /* already at
					                           the bottom */
				ld->current_pos =
				    next_in_tree(ld, ld->current_pos);
				ld->win_pos++;
				rd.n = -1;
				if (ld->win_pos
				    > ld->n_items - 1) /* scroll down */
				{
					ld->win_pos = ld->n_items - 1;
					ld->win_offset =
					    next_in_tree(ld, ld->win_offset);
					draw_to_window(dlg->win, scroll_list,
					               &rd);
				}
				draw_to_window(dlg->win, redraw_list_line, &rd);
			}
			if (delta < 0) /* scroll up */
			{
				if (ld->current_pos == ld->list)
					return EVENT_PROCESSED; /* already on
					                           the top */
				ld->current_pos =
				    prev_in_tree(ld, ld->current_pos);
				ld->win_pos--;
				rd.n = +1;
				if (ld->win_pos < 0) /* scroll up */
				{
					ld->win_pos = 0;
					ld->win_offset =
					    prev_in_tree(ld, ld->win_offset);
					draw_to_window(dlg->win, scroll_list,
					               &rd);
				}
				draw_to_window(dlg->win, redraw_list_line, &rd);
			}
			return EVENT_PROCESSED;
		}
		/* mouse wheel */
		if ((ev->b & BM_ACT) == B_MOVE
		    && ((ev->b & BM_BUTT) == B_WHEELUP
		        || (ev->b & BM_BUTT) == B_WHEELDOWN
		        || (ev->b & BM_BUTT) == B_WHEELDOWN1
		        || (ev->b & BM_BUTT) == B_WHEELUP1)) {
			int button = (int)ev->b & BM_BUTT;
			last_mouse_y = ev->y;

			if (button == B_WHEELDOWN
			    || button == B_WHEELDOWN1) /* scroll down */
			{
				if (next_in_tree(ld, ld->current_pos)
				    == ld->list)
					return EVENT_PROCESSED; /* already at
					                           the bottom */
				ld->current_pos =
				    next_in_tree(ld, ld->current_pos);
				ld->win_pos++;
				rd.n = -1;
				if (ld->win_pos
				    > ld->n_items - 1) /* scroll down */
				{
					ld->win_pos = ld->n_items - 1;
					ld->win_offset =
					    next_in_tree(ld, ld->win_offset);
					draw_to_window(dlg->win, scroll_list,
					               &rd);
				}
				draw_to_window(dlg->win, redraw_list_line, &rd);
			}
			if (button == B_WHEELUP
			    || button == B_WHEELUP1) /* scroll up */
			{
				if (ld->current_pos == ld->list)
					return EVENT_PROCESSED; /* already on
					                           the top */
				ld->current_pos =
				    prev_in_tree(ld, ld->current_pos);
				ld->win_pos--;
				rd.n = +1;
				if (ld->win_pos < 0) /* scroll up */
				{
					ld->win_pos = 0;
					ld->win_offset =
					    prev_in_tree(ld, ld->win_offset);
					draw_to_window(dlg->win, scroll_list,
					               &rd);
				}
				draw_to_window(dlg->win, redraw_list_line, &rd);
			}
			return EVENT_PROCESSED;
		}
break2:
		break;

	case EV_INIT:
	case EV_RESIZE:
	case EV_REDRAW:
	case EV_ABORT:
		break;

	default:
		break;
	}
	return EVENT_NOT_PROCESSED;
}

/* display function for the list window */
static void
create_list_window_fn(struct dialog_data *dlg)
{
	struct terminal *term = dlg->win->term;
	struct list_description *ld =
	    (struct list_description *)(dlg->dlg->udata2);
	int min = 0;
	int w, rw, y;
	int n_items;
	struct redraw_data rd;

	int a = 6;

	ld->dlg = dlg;
	if (ld->button_fn)
		a++; /* user button */
	if (ld->type == 1)
		a++; /* add directory button */

	y = 0;
	min_buttons_width(term, dlg->items, a, &min);

	w = term->x * 19 / 20 - 2 * DIALOG_LB;
	if (w < min)
		w = min;
	if (w > term->x - 2 * DIALOG_LB)
		w = term->x - 2 * DIALOG_LB;
	if (w < 5)
		w = 5;

	rw = 0;
	dlg_format_buttons(dlg, NULL, dlg->items, a, 0, &y, w, &rw, AL_CENTER);

	n_items = term->y - y;
	n_items -= 2 * DIALOG_TB + 2;
	if (n_items < 2)
		n_items = 2;
	ld->n_items = n_items;

	while (ld->win_pos > ld->n_items - 1) {
		ld->current_pos = prev_in_tree(ld, ld->current_pos);
		ld->win_pos--;
	}

	y += ld->n_items;

	rw = w;
	dlg->xw = rw + 2 * DIALOG_LB;
	dlg->yw = y + 2 * DIALOG_TB;
	center_dlg(dlg);
	draw_dlg(dlg);

	rd.ld = ld;
	rd.dlg = dlg;
	rd.n = 0;

	draw_to_window(dlg->win, redraw_list, &rd);

	y = dlg->y + DIALOG_TB + ld->n_items + 1;
	dlg_format_buttons(dlg, term, dlg->items, a, dlg->x + DIALOG_LB, &y, w,
	                   &rw, AL_CENTER);
}

static void
close_list_window(struct dialog_data *dlg)
{
	struct dialog *d = dlg->dlg;
	struct list_description *ld = (struct list_description *)(d->udata2);

	ld->open = 0;
	ld->dlg = NULL;
	free(ld->search_word);
	ld->search_word = NULL;
	if (ld->save)
		ld->save(d->udata);
}

int
test_list_window_in_use(struct list_description *ld, struct terminal *term)
{
	if (ld->open) {
		if (term)
			msg_box(term, NULL, TEXT_(T_INFO), AL_CENTER,
			        TEXT_(ld->already_in_use), MSG_BOX_END, NULL, 1,
			        TEXT_(T_CANCEL), msg_box_null, B_ENTER | B_ESC);
		return 1;
	}
	return 0;
}

/* dialog->udata2  ...  list_description  */
/* dialog->udata   ...  session */
int
create_list_window(struct list_description *ld, struct list *list,
                   struct terminal *term, struct session *ses)
{
	struct dialog *d;
	int a;

	/* zavodime, zavodime... */
	if (test_list_window_in_use(ld, term))
		return 1;
	ld->open = 1;

	if (!ld->current_pos) {
		ld->current_pos = list;
		ld->win_offset = list;
		ld->win_pos = 0;
		ld->dlg = NULL;
	}

	a = 7;
	if (ld->button_fn)
		a++;
	if (ld->type == 1)
		a++;

	d = mem_calloc(sizeof(struct dialog) + a * sizeof(struct dialog_item));

	d->title = TEXT_(ld->window_title);
	d->fn = create_list_window_fn;
	d->abort = close_list_window;
	d->handle_event = list_event_handler;
	d->udata = ses;
	d->udata2 = ld;

	a = 0;

	if (ld->button_fn) {
		d->items[a].type = D_BUTTON;
		d->items[a].fn = list_item_button;
		d->items[a].text = TEXT_(ld->button);
		a++;
	}

	if (ld->type == 1) {
		d->items[a].type = D_BUTTON;
		d->items[a].text = TEXT_(T_FOLDER);
		d->items[a].fn = list_folder_add;
		a++;
	}

	d->items[a].type = D_BUTTON;
	d->items[a].text = TEXT_(T_ADD);
	d->items[a].fn = list_item_add;

	d->items[a + 1].type = D_BUTTON;
	d->items[a + 1].text = TEXT_(T_DELETE);
	d->items[a + 1].fn = list_item_delete;

	d->items[a + 2].type = D_BUTTON;
	d->items[a + 2].text = TEXT_(T_EDIT);
	d->items[a + 2].fn = list_item_edit;

	d->items[a + 3].type = D_BUTTON;
	d->items[a + 3].text = TEXT_(T_MOVE);
	d->items[a + 3].fn = list_item_move;

	d->items[a + 4].type = D_BUTTON;
	d->items[a + 4].text = TEXT_(T_UNSELECT_ALL);
	d->items[a + 4].fn = list_item_unselect;

	d->items[a + 5].type = D_BUTTON;
	d->items[a + 5].gid = B_ESC;
	d->items[a + 5].fn = cancel_dialog;
	d->items[a + 5].text = TEXT_(T_CLOSE);

	d->items[a + 6].type = D_END;
	do_dialog(term, d, getml(d, NULL));
	return 0;
}

void
reinit_list_window(struct list_description *ld)
{
	ld->current_pos = ld->list;
	ld->win_offset = ld->list;
	ld->win_pos = 0;

	if (ld->open)
		internal("reinit_list_window: calling reinit while open");
}
