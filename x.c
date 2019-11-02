/* x.c
 * (c) 2002 Petr 'Brain' Kulhavy
 * This file is a part of the Links program, released under GPL.
 */

/* Takovej mensi problemek se scrollovanim:
 *
 * Mikulas a Xy zpusobili, ze scrollovani a prekreslovani je asynchronni. To znamena, ze
 * je v tom peknej bordylek. Kdyz BFU scrollne s oknem, tak se zavola funkce scroll. Ta
 * posle Xum XCopyArea (prekopiruje prislusny kus okna) a vygeneruje eventu
 * (GraphicsExpose) na postizenou (odkrytou) oblast. Funkce XCopyArea pripadne vygeneruje
 * dalsi GraphicsExpose eventu na postizenou oblast, ktera se muze objevit, kdyz je
 * linksove okno prekryto jinym oknem.
 *
 * Funkce scroll skonci. V event handleru se nekdy v budoucnosti (treba za tyden)
 * zpracuji eventy od Xu, mezi nimi i GraphicsExpose - tedy prekreslovani postizenych
 * oblasti.
 *
 * Problem je v tom, ze v okamziku, kdy scroll skonci, neni obrazovka prekreslena do
 * konzistentniho stavu (misty je garbaz) a navic se muze volat dalsi scroll. Tedy
 * XCopyArea muze posunout garbaz nekam do cudu a az se dostane na radu prekreslovani
 * postizenych oblasti, garbaz se uz nikdy neprekresli.
 *
 * Ja jsem navrhoval udelat scrollovani synchronni, to znamena, ze v okamziku, kdy scroll
 * skonci, bude okno v konzistentnim stavu. To by znamenalo volat ze scrollu zpracovavani
 * eventu (alespon GraphicsExpose). To by ovsem nepomohlo, protoze prekreslovaci funkce
 * neprekresluje, ale registruje si bottom halfy a podobny ptakoviny a prekresluje az
 * nekdy v budoucnosti. Navic Mikulas rikal, ze prekreslovaci funkce muze generovat dalsi
 * prekreslovani (sice jsem nepochopil jak, ale hlavne, ze je vecirek), takze by to
 * neslo.
 *
 * Proto Mikulas vymyslel genialni tah - takzvany "genitah". Ve funkci scroll se projede
 * fronta eventu od Xserveru a vyberou se GraphicsExp(l)ose eventy a ulozi se do zvlastni
 * fronty. Ve funkci na zpracovani Xovych eventu se zpracuji eventy z teto fronty. Na
 * zacatku scrollovaci funkce se projedou vsechny eventy ve zvlastni fronte a updatuji se
 * jim souradnice podle prislusneho scrollu.
 *
 * Na to jsem ja vymyslel uzasnou vymluvu: co kdyz 1. scroll vyrobi nejake postizene
 * oblasti a 2. scroll bude mit jinou clipovaci oblast, ktera bude tu postizenou oblast
 * zasahovat z casti. Tak to se bude jako ta postizena oblast stipat na casti a ty casti
 * se posunou podle toho, jestli jsou zasazene tim 2. scrollem? Tim jsem ho utrel, jak
 * zpoceny celo.
 *
 * Takze se to nakonec udela tak, ze ze scrollu vratim hromadu rectanglu, ktere se maji
 * prekreslit, a Mikulas si s tim udela, co bude chtit. Podobne jako ve svgalib, kde se
 * vrati 1 a Mikulas si prislusnou odkrytou oblast prekresli sam. Doufam jen, ze to je
 * posledni verze a ze nevzniknou dalsi problemy.
 *
 * Ve funkci scroll tedy pribude argument struct rect_set **.
 */

/* Pozor: po kazdem XSync nebo XFlush se musi dat
 * X_SCHEDULE_PROCESS_EVENTS
 * jinak to bude cekat na filedescriptoru, i kdyz to ma eventy uz ve fronte.
 *	-- mikulas
 */

#include "links.h"

#include <langinfo.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xlocale.h>
#include <X11/Xproto.h>
#include <X11/Xutil.h>

#define X_BORDER_WIDTH 4
#define X_HASH_TABLE_SIZE 64

#define X_MAX_CLIPBOARD_SIZE	(15*1024*1024)

static int x_default_window_width;
static int x_default_window_height;

static long (*x_get_color_function)(int);
static void x_translate_colors(unsigned char *data, int x, int y, int skip);

static void selection_request(XEvent *event);

static Display *x_display = NULL;   /* display */
static Visual *x_default_visual;
static XVisualInfo vinfo;

static int x_fd;    /* x socket */
static unsigned char *x_display_string = NULL;
static int x_screen;   /* screen */
static int x_display_height, x_display_width;   /* screen dimensions */
static unsigned long x_black_pixel;  /* black pixel */
static int x_depth, x_bitmap_bpp;   /* bits per pixel and bytes per pixel */
static int x_bitmap_scanline_pad; /* bitmap scanline_padding in bytes */
static int x_bitmap_bit_order;

static XColor *static_color_map = NULL;
static unsigned char x_have_palette;
static unsigned char x_use_static_color_table;
static unsigned char *static_color_table = NULL;
struct {
	unsigned char failure;
	unsigned char extra_allocated;
	unsigned short alloc_count;
} *static_color_struct = NULL;

#define X_STATIC_COLORS                (x_depth < 8 ? 8 : 216)
#define X_STATIC_CODE          (x_depth < 8 ? 801 : 833)

static unsigned short *color_map_16bit = NULL;
static unsigned *color_map_24bit = NULL;

static Window x_root_window, fake_window;
static int fake_window_initialized = 0;
static GC x_normal_gc = 0, x_copy_gc = 0, x_drawbitmap_gc = 0, x_scroll_gc = 0;
static long x_normal_gc_color;
static struct rect x_scroll_gc_rect;
static Colormap x_default_colormap, x_colormap;
static Atom x_delete_window_atom, x_wm_protocols_atom, x_sel_atom, x_targets_atom, x_utf8_string_atom;

static XIM xim = NULL;

extern struct graphics_driver x_driver;

static unsigned char *x_driver_param = NULL;
static int n_wins = 0; /* number of windows */

static struct list_head bitmaps = { &bitmaps, &bitmaps };

#define XPIXMAPP(a) ((struct x_pixmapa *)(a))

#define X_TYPE_NOTHING 0
#define X_TYPE_PIXMAP  1
#define X_TYPE_IMAGE   2

struct x_pixmapa {
	unsigned char type;
	union
	{
		XImage *image;
		Pixmap pixmap;
	} data;
	list_entry_1st
	list_entry_last
};


static struct {
	unsigned char count;
	struct graphics_device **pointer;
} x_hash_table[X_HASH_TABLE_SIZE];

/* string in clipboard is in UTF-8 */
static unsigned char *x_my_clipboard = NULL;

struct window_info {
	XIC xic;
	Window window;
};

static inline struct window_info *get_window_info(struct graphics_device *dev)
{
	return dev->driver_data;
}

/*----------------------------------------------------------------------*/

/* Tyhle opicarny tu jsou pro zvyseni rychlosti. Flush se nedela pri kazde operaci, ale
 * rekne se, ze je potreba udelat flush, a zaregistruje se bottom-half, ktery flush
 * provede. Takze jakmile se vrati rizeni do select smycky, tak se provede flush.
 */

static void x_wait_for_event(void)
{
	can_read_timeout(x_fd, -1);
}

static void x_process_events(void *data);

static unsigned char flush_in_progress = 0;
static unsigned char process_events_in_progress = 0;

static inline void X_SCHEDULE_PROCESS_EVENTS(void)
{
	if (!process_events_in_progress) {
		register_bottom_half(x_process_events, NULL);
		process_events_in_progress = 1;
	}
}

static void x_do_flush(void *ignore)
{
	/* kdyz budu mit zaregistrovanej bottom-half na tuhle funkci a nekdo mi
	 * tipne Xy, tak se nic nedeje, maximalne se zavola XFlush na blbej
	 * display, ale Xy se nepodelaj */

	flush_in_progress = 0;
	XFlush(x_display);
	X_SCHEDULE_PROCESS_EVENTS();
}

static inline void X_FLUSH(void)
{
	if (!flush_in_progress) {
		register_bottom_half(x_do_flush, NULL);
		flush_in_progress = 1;
	}
}

static void x_pixmap_failed(void *ignore);

static int (*original_error_handler)(Display *, XErrorEvent *) = NULL;
static unsigned char pixmap_mode = 0;

static int pixmap_handler(Display *d, XErrorEvent *e)
{
	if (pixmap_mode == 2) {
		if (e->request_code == X_CreatePixmap) {
			pixmap_mode = 1;
			register_bottom_half(x_pixmap_failed, NULL);
			return 0;
		}
	}
	if (pixmap_mode == 1) {
		if (e->request_code == X_CreatePixmap
		|| e->request_code == X_PutImage
		|| e->request_code == X_CopyArea
		|| e->request_code == X_FreePixmap) {
			return 0;
		}
	}
	return original_error_handler(d, e);
}

static void x_pixmap_failed(void *no_reinit)
{
	struct x_pixmapa *p;
	struct list_head *lp;

	foreach(struct x_pixmapa, p, lp, bitmaps) {
		if (p->type == X_TYPE_PIXMAP) {
			XFreePixmap(x_display, p->data.pixmap);
			p->type = X_TYPE_NOTHING;
		}
	}

	XSync(x_display, False);
	X_SCHEDULE_PROCESS_EVENTS();

	XSetErrorHandler(original_error_handler);
	original_error_handler = NULL;
	pixmap_mode = 0;

	if (!no_reinit)
		flush_bitmaps(1, 1, 1);
}

static void setup_pixmap_handler(void)
{
	original_error_handler = XSetErrorHandler(pixmap_handler);
	pixmap_mode = 2;
}

static void undo_pixmap_handler(void)
{
	if (pixmap_mode == 2) {
		XSetErrorHandler(original_error_handler);
		original_error_handler = NULL;
		pixmap_mode = 0;
	}
	if (pixmap_mode == 1) {
		x_pixmap_failed(NULL);
	}
	unregister_bottom_half(x_pixmap_failed, NULL);
}

static int (*old_error_handler)(Display *, XErrorEvent *) = NULL;
static unsigned char failure_happened;
static unsigned char test_request_code;

static int failure_handler(Display *d, XErrorEvent *e)
{
	if (e->request_code != test_request_code)
		return old_error_handler(d, e);
	failure_happened = 1;
	return 0;
}

static void x_prepare_for_failure(unsigned char code)
{
	if (old_error_handler)
		internal("x_prepare_for_failure: double call");
	failure_happened = 0;
	test_request_code = code;
	old_error_handler = XSetErrorHandler(failure_handler);
}

static int x_test_for_failure(void)
{
	XSync(x_display, False);
	X_SCHEDULE_PROCESS_EVENTS();
	XSetErrorHandler(old_error_handler);
	old_error_handler = NULL;
	return failure_happened;
}

/* suppose l<h */
static void x_clip_number(int *n,int l,int h)
{
	if (*n < l)
		*n = l;
	if (*n > h)
		*n = h;
}

static const unsigned char alloc_sequence_216[216] = { 0, 215, 30, 185, 5, 35, 180, 210, 86, 203, 23, 98, 192, 119, 126, 43, 173, 12, 74, 140, 188, 161, 27, 54, 95, 204, 17, 114, 198, 11, 107, 113, 60, 65, 68, 81, 102, 108, 120, 132, 146, 164, 20, 89, 92, 158, 170, 191, 6, 37, 40, 46, 49, 52, 57, 123, 129, 135, 151, 154, 175, 178, 195, 207, 24, 71, 77, 78, 101, 143, 149, 167, 2, 8, 14, 18, 29, 32, 59, 62, 66, 72, 83, 84, 90, 96, 104, 110, 116, 125, 131, 137, 138, 144, 156, 162, 168, 182, 186, 197, 200, 209, 212, 1, 3, 4, 7, 9, 10, 13, 15, 16, 19, 21, 22, 25, 26, 28, 31, 33, 34, 36, 38, 39, 41, 42, 44, 45, 47, 48, 50, 51, 53, 55, 56, 58, 61, 63, 64, 67, 69, 70, 73, 75, 76, 79, 80, 82, 85, 87, 88, 91, 93, 94, 97, 99, 100, 103, 105, 106, 109, 111, 112, 115, 117, 118, 121, 122, 124, 127, 128, 130, 133, 134, 136, 139, 141, 142, 145, 147, 148, 150, 152, 153, 155, 157, 159, 160, 163, 165, 166, 169, 171, 172, 174, 176, 177, 179, 181, 183, 184, 187, 189, 190, 193, 194, 196, 199, 201, 202, 205, 206, 208, 211, 213, 214 };

static void x_fill_color_table(XColor colors[256], unsigned q)
{
	unsigned int a;
	unsigned rgb[3];
	unsigned int limit = 1U << x_depth;

	for (a = 0; a < limit; a++) {
		q_palette(q, a, 65535, rgb);
		colors[a].red = rgb[0];
		colors[a].green = rgb[1];
		colors[a].blue = rgb[2];
		colors[a].pixel = a;
		colors[a].flags = DoRed | DoGreen | DoBlue;
	}
}

static double rgb_distance_with_border(int r1, int g1, int b1, int r2, int g2, int b2)
{
	double distance = rgb_distance(r1, g1, b1, r2, g2, b2);
	if (!r1)
		distance += r2 * 4294967296.;
	if (!g1)
		distance += g2 * 4294967296.;
	if (!b1)
		distance += b2 * 4294967296.;
	if (r1 == 0xffff)
		distance += (0xffff - r2) * 4294967296.;
	if (g1 == 0xffff)
		distance += (0xffff - g2) * 4294967296.;
	if (b1 == 0xffff)
		distance += (0xffff - b2) * 4294967296.;
	return distance;
}

static int get_nearest_color(unsigned rgb[3])
{
	int j;
	double distance, best_distance = 0;
	int best = -1;
	for (j = 0; j < 1 << x_depth; j++) {
		if (static_color_struct[j].failure)
			continue;
		distance = rgb_distance_with_border(rgb[0], rgb[1], rgb[2], static_color_map[j].red, static_color_map[j].green, static_color_map[j].blue);
		if (best < 0 || distance < best_distance) {
			best = j;
			best_distance = distance;
		}
	}
	return best;
}

static int swap_color(int c, XColor *xc, int test_count, double *distance, unsigned rgb[3])
{
	unsigned long old_px, new_px;
	double new_distance;

	/*
	 * The manual page on XAllocColor says that it returns the closest
	 * color.
	 * In reality, it fails if it can't allocate the color.
	 *
	 * In case there were some other implementations that return the
	 * closest color, we check the distance after allocation and we fail if
	 * the distance would be increased.
	 */
	if (!XAllocColor(x_display, x_default_colormap, xc))
		return -1;
	new_px = xc->pixel;
	new_distance = rgb_distance(xc->red, xc->green, xc->blue, rgb[0], rgb[1], rgb[2]);
	if (new_px >= 256
	|| (test_count && (static_color_struct[new_px].alloc_count || static_color_struct[new_px].extra_allocated))
	|| new_distance > *distance) {
		XFreeColors(x_display, x_default_colormap, &xc->pixel, 1, 0);
		return -1;
	}

	old_px = static_color_table[c];
	static_color_struct[old_px].alloc_count--;
	XFreeColors(x_display, x_default_colormap, &old_px, 1, 0);

	static_color_map[new_px] = *xc;
	static_color_struct[new_px].alloc_count++;
	static_color_struct[new_px].failure = 0;
	static_color_struct[new_px].extra_allocated = 1;
	static_color_table[c] = (unsigned char)new_px;
	*distance = new_distance;

	return 0;
}

static unsigned char *x_query_palette(void)
{
	unsigned rgb[3];
	int i;
	int some_valid = 0;
	int do_alloc = vinfo.class == PseudoColor;
 retry:
	if (sizeof(XColor) > INT_MAX >> x_depth) overalloc();
	if (sizeof(*static_color_struct) > INT_MAX >> x_depth) overalloc();
	if (!static_color_map)
		static_color_map = xmalloc(sizeof(XColor) << x_depth);
	if (!static_color_struct)
		static_color_struct = xmalloc(sizeof(*static_color_struct) << x_depth);

	memset(static_color_map, 0, sizeof(XColor) << x_depth);
	memset(static_color_struct, 0, sizeof(*static_color_struct) << x_depth);
	for (i = 0; i < 1 << x_depth; i++)
		static_color_map[i].pixel = i;
	x_prepare_for_failure(X_QueryColors);
	XQueryColors(x_display, x_default_colormap, static_color_map, 1 << x_depth);
	if (!x_test_for_failure()) {
		for (i = 0; i < 1 << x_depth; i++) {
			if ((static_color_map[i].flags & (DoRed | DoGreen | DoBlue)) == (DoRed | DoGreen | DoBlue))
				some_valid = 1;
			else
				static_color_struct[i].failure = 1;
		}
	}

	if (!some_valid) {
		memset(static_color_map, 0, sizeof(XColor) << x_depth);
		memset(static_color_struct, 0, sizeof(*static_color_struct) << x_depth);
		for (i = 0; i < 1 << x_depth; i++) {
			static_color_map[i].pixel = i;
			x_prepare_for_failure(X_QueryColors);
			XQueryColor(x_display, x_default_colormap, &static_color_map[i]);
			if (!x_test_for_failure() && (static_color_map[i].flags & (DoRed | DoGreen | DoBlue)) == (DoRed | DoGreen | DoBlue))
				some_valid = 1;
			else
				static_color_struct[i].failure = 1;
		}
	}

	if (!some_valid) {
		if (vinfo.class == StaticColor) {
			return stracpy(cast_uchar "Could not query static colormap.\n");
		} else {
			x_fill_color_table(static_color_map, X_STATIC_COLORS);
			do_alloc = 0;
		}
	}
	if (!static_color_table)
		static_color_table = xmalloc(256);
	memset(static_color_table, 0, 256);
	for (i = 0; i < X_STATIC_COLORS; i++) {
		int idx = X_STATIC_COLORS == 216 ? alloc_sequence_216[i] : i;
		int c;
		q_palette(X_STATIC_COLORS, idx, 65535, rgb);
 another_nearest:
		c = get_nearest_color(rgb);
		if (c < 0) {
			do_alloc = 0;
			goto retry;
		}
		if (do_alloc) {
			XColor xc;
			memset(&xc, 0, sizeof xc);
			xc.red = static_color_map[c].red;
			xc.green = static_color_map[c].green;
			xc.blue = static_color_map[c].blue;
			xc.flags = DoRed | DoGreen | DoBlue;
			if (!XAllocColor(x_display, x_default_colormap, &xc)) {
				if (0) {
 allocated_invalid:
					XFreeColors(x_display, x_default_colormap, &xc.pixel, 1, 0);
				}
				static_color_struct[c].failure = 1;
				goto another_nearest;
			} else {
				if (xc.pixel >= 256)
					goto allocated_invalid;
				c = (unsigned char)xc.pixel;
				static_color_map[c] = xc;
				static_color_struct[c].alloc_count++;
			}
		}
		static_color_table[idx] = (unsigned char)c;
	}
	if (do_alloc) {
		double distances[256];
		double max_dist;
		int c;
		for (i = 0; i < X_STATIC_COLORS; i++) {
			unsigned char q = static_color_table[i];
			q_palette(X_STATIC_COLORS, i, 65535, rgb);
			distances[i] = rgb_distance(static_color_map[q].red, static_color_map[q].green, static_color_map[q].blue, rgb[0], rgb[1], rgb[2]);
		}
 try_alloc_another:
		max_dist = 0;
		c = -1;
		for (i = 0; i < X_STATIC_COLORS; i++) {
			int idx = X_STATIC_COLORS == 216 ? alloc_sequence_216[i] : i;
			if (distances[idx] > max_dist) {
				max_dist = distances[idx];
				c = idx;
			}
		}
		if (c >= 0) {
			XColor xc;
			memset(&xc, 0, sizeof xc);
			q_palette(X_STATIC_COLORS, c, 65535, rgb);
			xc.red = rgb[0];
			xc.green = rgb[1];
			xc.blue = rgb[2];
			xc.flags = DoRed | DoGreen | DoBlue;
			if (swap_color(c, &xc, 1, &distances[c], rgb))
				goto no_more_alloc;

			for (i = 0; i < X_STATIC_COLORS; i++) {
				double d1, d2;
				unsigned char cidx = static_color_table[i];
				q_palette(X_STATIC_COLORS, i, 65535, rgb);
				d1 = rgb_distance_with_border(rgb[0], rgb[1], rgb[2], static_color_map[cidx].red, static_color_map[cidx].green, static_color_map[cidx].blue);
				d2 = rgb_distance_with_border(rgb[0], rgb[1], rgb[2], xc.red, xc.green, xc.blue);
				if (d2 < d1) {
					if (distances[i] < rgb_distance(xc.red, xc.green, xc.blue, rgb[0], rgb[1], rgb[2]))
						continue;
					if (swap_color(i, &xc, 0, &distances[i], rgb))
						goto no_more_alloc;
				}
			}

			goto try_alloc_another;
		}
	}
 no_more_alloc:
	return NULL;
}

static void x_free_colors(void) {
	unsigned long pixels[256];
	int n_pixels = 0;
	int i;
	if (!static_color_struct)
		return;
	for (i = 0; i < 1 << x_depth; i++) {
		while (static_color_struct[i].alloc_count) {
			static_color_struct[i].alloc_count--;
			pixels[n_pixels++] = i;
		}
	}
	if (n_pixels)
		XFreeColors(x_display, x_default_colormap, pixels, n_pixels, 0);
}

static void x_set_palette(void)
{
	unsigned limit = 1U << x_depth;

	x_free_colors();

	if (x_use_static_color_table) {
		x_query_palette();
		XStoreColors(x_display, x_colormap, static_color_map, (int)limit);
	} else {
		XColor colors[256];
		x_fill_color_table(colors, limit);
		XStoreColors(x_display, x_colormap, colors, (int)limit);
	}

	X_FLUSH();
}

static void mask_to_bitfield(unsigned long mask, int bitfield[8])
{
	int i;
	int bit = 0;
	for (i = sizeof(unsigned long) * 8 - 1; i >= 0; i--) {
		if (mask & 1UL << i) {
			bitfield[bit++] = i;
			if (bit >= 8)
				return;
		}
	}
	while (bit < 8)
		bitfield[bit++] = -1;
}

static unsigned long apply_bitfield(int bitfield[8], int bits, unsigned int value)
{
	unsigned long result = 0;
	int bf_index = 0;
	while (bits-- && bf_index < 8) {
		if (value & 1 << bits) {
			if (bitfield[bf_index] >= 0)
				result |= 1UL << bitfield[bf_index];
		}
		bf_index++;
	}
	return result;
}

static int create_16bit_mapping(unsigned long red_mask, unsigned long green_mask, unsigned long blue_mask)
{
	int i;
	int red_bitfield[8];
	int green_bitfield[8];
	int blue_bitfield[8];

	if ((red_mask | green_mask | blue_mask) >= 0x10000)
		return 0;
	if (!red_mask || !green_mask || !blue_mask)
		return 0;

	mask_to_bitfield(red_mask, red_bitfield);
	mask_to_bitfield(green_mask, green_bitfield);
	mask_to_bitfield(blue_mask, blue_bitfield);

	color_map_16bit = mem_calloc(256 * 2 * sizeof(unsigned short));
	for (i = 0; i < 256; i++) {
		unsigned int g, b;
		unsigned short ag, ab, result;

		g = i >> 5;
		b = i & 31;

		ag = (unsigned short)apply_bitfield(green_bitfield, x_depth - 10, g);
		ab = (unsigned short)apply_bitfield(blue_bitfield, 5, b);

		result = ag | ab;

		color_map_16bit[i + (x_bitmap_bit_order == MSBFirst) * 256] = (unsigned short)result;
	}
	for (i = 0; i < (x_depth == 15 ? 128 : 256); i++) {
		unsigned int r, g;
		unsigned short ar, ag, result;

		r = i >> (x_depth - 13);
		g = (i << (x_depth - 13)) & ((1 << (x_depth - 10)) - 1);

		ar = (unsigned short)apply_bitfield(red_bitfield, 5, r);
		ag = (unsigned short)apply_bitfield(green_bitfield, x_depth - 10, g);

		result = ar | ag;

		color_map_16bit[i + (x_bitmap_bit_order == LSBFirst) * 256] = (unsigned short)result;
	}

	return 1;
}

static void create_24bit_component_mapping(unsigned long mask, unsigned int *ptr)
{
	int i;
	int bitfield[8];

	mask_to_bitfield(mask, bitfield);

	for (i = 0; i < 256; i++) {
		unsigned int result = (unsigned int)apply_bitfield(bitfield, 8, i);
		if (x_bitmap_bit_order == MSBFirst && x_bitmap_bpp == 4)
			result = (result >> 24) | ((result & 0xff0000) >> 8) | ((result & 0xff00) << 8) | ((result & 0xff) << 24);
		ptr[i] = result;
	}
}

static int create_24bit_mapping(unsigned long red_mask, unsigned long green_mask, unsigned long blue_mask)
{
	if ((red_mask | green_mask | blue_mask) > 0xFFFFFFFFUL)
		return 0;
	if (!red_mask || !green_mask || !blue_mask)
		return 0;

	color_map_24bit = xmalloc(256 * 3 * sizeof(unsigned int));

	create_24bit_component_mapping(blue_mask, color_map_24bit);
	create_24bit_component_mapping(green_mask, color_map_24bit + 256);
	create_24bit_component_mapping(red_mask, color_map_24bit + 256 * 2);

	return 1;
}

static inline int trans_key(unsigned char *str)
{
	int a;
	GET_UTF_8(str, a);
	return a;
}


/* translates X keys to links representation */
/* return value: 1=valid key, 0=nothing */
static int x_translate_key(struct graphics_device *dev, XKeyEvent *e, int *key, int *flag)
{
	KeySym ks = 0;
	static XComposeStatus comp = { NULL, 0 };
	static char str[16];
#define str_size	((int)(sizeof(str) - 1))
	int len;

	if (get_window_info(dev)->xic) {
		Status status;
		len = Xutf8LookupString(get_window_info(dev)->xic, e, str, str_size, &ks, &status);
	} else
		len = XLookupString(e, str, str_size, &ks, &comp);

	str[len > str_size ? str_size : len] = 0;
	if (!len) {
		str[0] = (char)ks;
		str[1] = 0;
	}
	*flag = 0;
	*key = 0;

	/* alt, control, shift ... */
	if (e->state & ShiftMask) *flag |= KBD_SHIFT;
	if (e->state & ControlMask) *flag |= KBD_CTRL;
	if (e->state & Mod1Mask) *flag |= KBD_ALT;

	/* alt-f4 */
	if (*flag & KBD_ALT && ks == XK_F4) { *key = KBD_CTRL_C; *flag = 0; return 1; }

	/* ctrl-c */
	if (*flag & KBD_CTRL && (ks == XK_c || ks == XK_C)) {*key = KBD_CTRL_C; *flag = 0; return 1; }

	if (ks == NoSymbol) { return 0;
	} else if (ks == XK_Return) { *key = KBD_ENTER;
	} else if (ks == XK_BackSpace) { *key = KBD_BS;
	} else if (ks == XK_Tab
#ifdef XK_KP_Tab
		|| ks == XK_KP_Tab
#endif
		) { *key = KBD_TAB;
	} else if (ks == XK_Escape) {
		*key = KBD_ESC;
	} else if (ks == XK_Left
#ifdef XK_KP_Left
		|| ks == XK_KP_Left
#endif
		) { *key = KBD_LEFT;
	} else if (ks == XK_Right
#ifdef XK_KP_Right
		|| ks == XK_KP_Right
#endif
		) { *key = KBD_RIGHT;
	} else if (ks == XK_Up
#ifdef XK_KP_Up
		|| ks == XK_KP_Up
#endif
		) { *key = KBD_UP;
	} else if (ks == XK_Down
#ifdef XK_KP_Down
		|| ks == XK_KP_Down
#endif
		) { *key = KBD_DOWN;
	} else if (ks == XK_Insert
#ifdef XK_KP_Insert
		|| ks == XK_KP_Insert
#endif
		) { *key = KBD_INS;
	} else if (ks == XK_Delete
#ifdef XK_KP_Delete
		|| ks == XK_KP_Delete
#endif
		) { *key = KBD_DEL;
	} else if (ks == XK_Home
#ifdef XK_KP_Home
		|| ks == XK_KP_Home
#endif
		) { *key = KBD_HOME;
	} else if (ks == XK_End
#ifdef XK_KP_End
		|| ks == XK_KP_End
#endif
		) { *key = KBD_END;
	} else if (0
#ifdef XK_KP_Page_Up
		|| ks == XK_KP_Page_Up
#endif
#ifdef XK_Page_Up
		|| ks == XK_Page_Up
#endif
		) { *key = KBD_PAGE_UP;
	} else if (0
#ifdef XK_KP_Page_Down
		|| ks == XK_KP_Page_Down
#endif
#ifdef XK_Page_Down
		|| ks == XK_Page_Down
#endif
		) { *key = KBD_PAGE_DOWN;
	} else if (ks == XK_F1
#ifdef XK_KP_F1
		|| ks == XK_KP_F1
#endif
		) { *key = KBD_F1;
	} else if (ks == XK_F2
#ifdef XK_KP_F2
		|| ks == XK_KP_F2
#endif
		) { *key = KBD_F2;
	} else if (ks == XK_F3
#ifdef XK_KP_F3
		|| ks == XK_KP_F3
#endif
		) { *key = KBD_F3;
	} else if (ks == XK_F4
#ifdef XK_KP_F4
		|| ks == XK_KP_F4
#endif
		) { *key = KBD_F4;
	} else if (ks == XK_F5) { *key = KBD_F5;
	} else if (ks == XK_F6) { *key = KBD_F6;
	} else if (ks == XK_F7) { *key = KBD_F7;
	} else if (ks == XK_F8) { *key = KBD_F8;
	} else if (ks == XK_F9) { *key = KBD_F9;
	} else if (ks == XK_F10) { *key = KBD_F10;
	} else if (ks == XK_F11) { *key = KBD_F11;
	} else if (ks == XK_F12) { *key = KBD_F12;
	} else if (ks == XK_KP_Subtract) { *key = '-';
	} else if (ks == XK_KP_Decimal) { *key = '.';
	} else if (ks == XK_KP_Divide) { *key = '/';
	} else if (ks == XK_KP_Space) { *key = ' ';
	} else if (ks == XK_KP_Enter) { *key = KBD_ENTER;
	} else if (ks == XK_KP_Equal) { *key = '=';
	} else if (ks == XK_KP_Multiply) { *key = '*';
	} else if (ks == XK_KP_Add) { *key = '+';
	} else if (ks == XK_KP_0) { *key = '0';
	} else if (ks == XK_KP_1) { *key = '1';
	} else if (ks == XK_KP_2) { *key = '2';
	} else if (ks == XK_KP_3) { *key = '3';
	} else if (ks == XK_KP_4) { *key = '4';
	} else if (ks == XK_KP_5) { *key = '5';
	} else if (ks == XK_KP_6) { *key = '6';
	} else if (ks == XK_KP_7) { *key = '7';
	} else if (ks == XK_KP_8) { *key = '8';
	} else if (ks == XK_KP_9) { *key = '9';
#ifdef XK_Select
	} else if (ks == XK_Select) { *key = KBD_SELECT;
#endif
#ifdef XK_Undo
	} else if (ks == XK_Undo) { *key = KBD_UNDO;
#endif
#ifdef XK_Redo
	} else if (ks == XK_Redo) { *key = KBD_REDO;
#endif
#ifdef XK_Menu
	} else if (ks == XK_Menu) { *key = KBD_MENU;
#endif
#ifdef XK_Find
	} else if (ks == XK_Find) { *key = KBD_FIND;
#endif
#ifdef XK_Cancel
	} else if (ks == XK_Cancel) { *key = KBD_STOP;
#endif
#ifdef XK_Help
	} else if (ks == XK_Help) { *key = KBD_HELP;
#endif
	} else if (ks & 0x8000) {
		unsigned char *str = (unsigned char *)XKeysymToString(ks);
		if (str) {
			if (!casestrcmp(str, cast_uchar "XF86Copy")) { *key = KBD_COPY; return 1; }
			if (!casestrcmp(str, cast_uchar "XF86Paste")) { *key = KBD_PASTE; return 1; }
			if (!casestrcmp(str, cast_uchar "XF86Cut")) { *key = KBD_CUT; return 1; }
			if (!casestrcmp(str, cast_uchar "SunProps")) { *key = KBD_PROPS; return 1; }
			if (!casestrcmp(str, cast_uchar "SunFront")) { *key = KBD_FRONT; return 1; }
			if (!casestrcmp(str, cast_uchar "SunOpen")) { *key = KBD_OPEN; return 1; }
			if (!casestrcmp(str, cast_uchar "XF86Search")) { *key = KBD_FIND; return 1; }
			if (!casestrcmp(str, cast_uchar "XF86Favorites")) { *key = KBD_BOOKMARKS; return 1; }
			if (!casestrcmp(str, cast_uchar "XF86Reload")) { *key = KBD_RELOAD; return 1; }
			if (!casestrcmp(str, cast_uchar "XF86Stop")) { *key = KBD_STOP; return 1; }
			if (!casestrcmp(str, cast_uchar "XF86Forward")) { *key = KBD_FORWARD; return 1; }
			if (!casestrcmp(str, cast_uchar "XF86Back")) { *key = KBD_BACK; return 1; }
			if (!casestrcmp(str, cast_uchar "XF86Open")) { *key = KBD_OPEN; return 1; }
			if (!casestrcmp(str, cast_uchar "XF86OpenURL")) { *key = KBD_OPEN; return 1; }
			if (!casestrcmp(str, cast_uchar "apLineDel")) { *key = KBD_DEL; return 1; }
		}
		return 0;
	} else {
		*key = *flag & KBD_CTRL ? (int)ks & 255 : trans_key(cast_uchar str);
	}
	return 1;
}

static void x_init_hash_table(void)
{
	int a;

	for (a = 0; a < X_HASH_TABLE_SIZE; a++) {
		x_hash_table[a].count = 0;
		x_hash_table[a].pointer = NULL;
	}
}

static void x_clear_clipboard(void);

static void x_free_hash_table(void)
{
	int a;

	process_events_in_progress = 0;
	flush_in_progress = 0;
	unregister_bottom_half(x_process_events, NULL);
	unregister_bottom_half(x_do_flush, NULL);

	for (a = 0; a < X_HASH_TABLE_SIZE; a++) {
		if (x_hash_table[a].count)
			internal("x_free_hash_table: the table is not empty");
		free(x_hash_table[a].pointer);
	}

	x_clear_clipboard();

	x_free_colors();

	free(static_color_table);
	free(static_color_map);
	free(static_color_struct);
	free(color_map_16bit);
	free(color_map_24bit);
	static_color_table = NULL;
	static_color_map = NULL;
	static_color_struct = NULL;
	color_map_16bit = NULL;
	color_map_24bit = NULL;

	if (x_display) {
		if (fake_window_initialized) {
			XDestroyWindow(x_display, fake_window);
			fake_window_initialized = 0;
		}
		if (x_normal_gc) {
			XFreeGC(x_display, x_normal_gc);
			x_normal_gc = 0;
		}
		if (x_copy_gc) {
			XFreeGC(x_display, x_copy_gc);
			x_copy_gc = 0;
		}
		if (x_drawbitmap_gc) {
			XFreeGC(x_display, x_drawbitmap_gc);
			x_drawbitmap_gc = 0;
		}
		if (x_scroll_gc) {
			XFreeGC(x_display, x_scroll_gc);
			x_scroll_gc = 0;
		}
		if (xim) {
			XCloseIM(xim);
			xim = NULL;
		}
		XCloseDisplay(x_display);
		x_display = NULL;
	}

	free(x_driver_param);
	free(x_display_string);
	x_driver_param = NULL;
	x_display_string = NULL;

	undo_pixmap_handler();

	process_events_in_progress = 0;
	flush_in_progress = 0;
	unregister_bottom_half(x_process_events, NULL);
	unregister_bottom_half(x_do_flush, NULL);
}

/* returns graphics device structure which belonging to the window */
static struct graphics_device *x_find_gd(Window win)
{
	int a, b;

	a=(int)win & (X_HASH_TABLE_SIZE - 1);
	for (b = 0; b < x_hash_table[a].count; b++) {
		if (get_window_info(x_hash_table[a].pointer[b])->window == win)
			return x_hash_table[a].pointer[b];
	}
	return NULL;
}

/* adds graphics device to hash table */
static void x_add_to_table(struct graphics_device *dev)
{
	int a = (int)get_window_info(dev)->window & (X_HASH_TABLE_SIZE - 1);
	int c = x_hash_table[a].count;

	if ((unsigned)c > INT_MAX / sizeof(struct graphics_device *) - 1)
		overalloc();
	x_hash_table[a].pointer = xrealloc(x_hash_table[a].pointer,
				(c + 1) * sizeof(struct graphics_device *));

	x_hash_table[a].pointer[c] = dev;
	x_hash_table[a].count++;
}

/* removes graphics device from table */
static void x_remove_from_table(Window win)
{
	int a = (int)win & (X_HASH_TABLE_SIZE - 1);
	int b;

	for (b = 0; b < x_hash_table[a].count; b++) {
		if (get_window_info(x_hash_table[a].pointer[b])->window == win) {
			memmove(x_hash_table[a].pointer + b, x_hash_table[a].pointer + b + 1, (x_hash_table[a].count - b - 1) * sizeof(struct graphics_device *));
			x_hash_table[a].count--;
			x_hash_table[a].pointer = xrealloc(x_hash_table[a].pointer, x_hash_table[a].count * sizeof(struct graphics_device *));
			return;
		}
	}
	internal("x_remove_from_table: window not found");
}


static void x_clear_clipboard(void)
{
	free(x_my_clipboard);
	x_my_clipboard = NULL;
}


static void x_update_driver_param(int w, int h)
{
	int l=0;

	if (n_wins != 1) return;

	x_default_window_width = w;
	x_default_window_height = h;

	free(x_driver_param);
	x_driver_param = init_str();
	add_num_to_str(&x_driver_param, &l, x_default_window_width);
	add_chr_to_str(&x_driver_param, &l, 'x');
	add_num_to_str(&x_driver_param, &l, x_default_window_height);
}


static int x_decode_button(int b)
{
	switch (b) {
		case 1: return B_LEFT;
		case 3: return B_RIGHT;
		case 2: return B_MIDDLE;
		case 4: return B_WHEELUP;
		case 5: return B_WHEELDOWN;
		case 6: return B_WHEELLEFT;
		case 7: return B_WHEELRIGHT;
		case 8: return B_FOURTH;
		case 9: return B_FIFTH;
		case 10: return B_SIXTH;

	}
	return -1;
}

static void x_process_events(void *data)
{
	XEvent event;
	XEvent last_event;
	struct graphics_device *dev;
	int last_was_mouse;
	int replay_event = 0;

	process_events_in_progress = 0;

	memset(&last_event, 0, sizeof last_event);	/* against warning */
	last_was_mouse=0;
	while (XPending(x_display) || replay_event)
	{
		if (replay_event) replay_event = 0;
		else XNextEvent(x_display, &event);
		if (last_was_mouse&&(event.type==ButtonPress||event.type==ButtonRelease))  /* this is end of mouse move block --- call mouse handler */
		{
			int a,b;

			last_was_mouse=0;
			dev = x_find_gd(last_event.xmotion.window);
			if (!dev) break;
			a=B_LEFT;
			b=B_MOVE;
			if ((last_event.xmotion.state)&Button1Mask)
			{
				a=B_LEFT;
				b=B_DRAG;
			}
			if ((last_event.xmotion.state)&Button2Mask)
			{
				a=B_MIDDLE;
				b=B_DRAG;
			}
			if ((last_event.xmotion.state)&Button3Mask)
			{
				a=B_RIGHT;
				b=B_DRAG;
			}
			x_clip_number(&last_event.xmotion.x, dev->size.x1, dev->size.x2 - 1);
			x_clip_number(&last_event.xmotion.y, dev->size.y1, dev->size.y2 - 1);
			dev->mouse_handler(dev, last_event.xmotion.x, last_event.xmotion.y, a | b);
		}

		switch (event.type) {
		/* redraw uncovered area during scroll */
		case GraphicsExpose:
		{
			struct rect r;

			dev = x_find_gd(event.xgraphicsexpose.drawable);
			if (!dev)
				break;
			r.x1 = event.xgraphicsexpose.x;
			r.y1 = event.xgraphicsexpose.y;
			r.x2 = event.xgraphicsexpose.x + event.xgraphicsexpose.width;
			r.y2 = event.xgraphicsexpose.y + event.xgraphicsexpose.height;
			dev->redraw_handler(dev, &r);
		}
			break;

		/* redraw part of the window */
		case Expose:
		{
			struct rect r;

			dev = x_find_gd(event.xexpose.window);
			if (!dev)
				break;
			r.x1 = event.xexpose.x;
			r.y1 = event.xexpose.y;
			r.x2 = event.xexpose.x + event.xexpose.width;
			r.y2 = event.xexpose.y + event.xexpose.height;
			dev->redraw_handler(dev, &r);
		}
			break;

		/* resize window */
		case ConfigureNotify:
			dev = x_find_gd(event.xconfigure.window);
			if (!dev)
				break;
			/* when window only moved and size is the same, do nothing */
			if (dev->size.x2 == event.xconfigure.width
			&& dev->size.y2 == event.xconfigure.height)
				break;
 configure_notify_again:
			dev->size.x2 = event.xconfigure.width;
			dev->size.y2 = event.xconfigure.height;
			x_update_driver_param(event.xconfigure.width, event.xconfigure.height);
			while (XCheckWindowEvent(x_display,
					get_window_info(dev)->window,
					ExposureMask, &event) == True);
			if (XCheckWindowEvent(x_display,
					get_window_info(dev)->window,
					StructureNotifyMask, &event) == True) {
				if (event.type == ConfigureNotify)
					goto configure_notify_again;
				replay_event = 1;
			}
			dev->resize_handler(dev);
			break;

		case KeyPress:
		{
			int f, k;
			if (XFilterEvent(&event, None))
				break;
			dev = x_find_gd(event.xkey.window);
			if (!dev)
				break;
			if (x_translate_key(dev, (XKeyEvent *)&event, &k, &f))
				dev->keyboard_handler(dev, k, f);
		}
			break;

		case ClientMessage:
			if (event.xclient.format != 32
			|| event.xclient.message_type != x_wm_protocols_atom
			|| (Atom)event.xclient.data.l[0] != x_delete_window_atom)
				break;
			/*else fallthrough*/

		case DestroyNotify:
			dev = x_find_gd(event.xkey.window);
			if (!dev)
				break;

			dev->keyboard_handler(dev, KBD_CLOSE, 0);
			break;

		case ButtonRelease:
		{
			int a;
			dev = x_find_gd(event.xbutton.window);
			if (!dev)
				break;
			last_was_mouse = 0;
			if ((a = x_decode_button(event.xbutton.button)) >= 0 && !BM_IS_WHEEL(a)) {
				x_clip_number(&event.xmotion.x, dev->size.x1, dev->size.x2 - 1);
				x_clip_number(&event.xmotion.y, dev->size.y1, dev->size.y2 - 1);
				dev->mouse_handler(dev, event.xbutton.x, event.xbutton.y, a | B_UP);
			}
		}
			break;

		case ButtonPress:
		{
			int a;
			dev = x_find_gd(event.xbutton.window);
			if (!dev)
				break;
			last_was_mouse = 0;
			if ((a = x_decode_button(event.xbutton.button)) >= 0) {
				x_clip_number(&(event.xmotion.x), dev->size.x1, dev->size.x2 - 1);
				x_clip_number(&(event.xmotion.y), dev->size.y1, dev->size.y2 - 1);
				dev->mouse_handler(dev, event.xbutton.x, event.xbutton.y, a | (!BM_IS_WHEEL(a) ? B_DOWN : B_MOVE));
			}
		}
				break;

		case MotionNotify:
			/* just sign, that this was mouse event */
			last_was_mouse = 1;
			last_event = event;
			/* fix lag when using remote X and dragging over some text */
			XSync(x_display, False);
			break;

		/* read clipboard */
		case SelectionNotify:
			/* handled in x_get_clipboard_text */
			break;

/* This long code must be here in order to implement copying of stuff into the clipboard */
		case SelectionRequest:
			selection_request(&event);
			break;

		case MapNotify:
			XFlush (x_display);
			break;

		default:
			break;
		}
	}

	/* that was end of mouse move block --- call mouse handler */
	if (last_was_mouse)  {
		int a, b;

		last_was_mouse = 0;
		dev = x_find_gd(last_event.xmotion.window);
		if (!dev)
			goto ret;
		a = B_LEFT;
		b = B_MOVE;
		if ((last_event.xmotion.state) & Button1Mask) {
			a = B_LEFT;
			b = B_DRAG;
		}
		if ((last_event.xmotion.state) & Button2Mask) {
			a = B_MIDDLE;
			b = B_DRAG;
		}
		if ((last_event.xmotion.state) & Button3Mask) {
			a = B_RIGHT;
			b = B_DRAG;
		}
		x_clip_number(&last_event.xmotion.x, dev->size.x1, dev->size.x2 - 1);
		x_clip_number(&last_event.xmotion.y, dev->size.y1, dev->size.y2 - 1);
		dev->mouse_handler(dev, last_event.xmotion.x,
			last_event.xmotion.y, a | b);
	}
ret:;
}


/* returns pointer to string with driver parameter or NULL */
static unsigned char *x_get_driver_param(void)
{
	return x_driver_param;
}

static unsigned char *x_get_af_unix_name(void)
{
	return x_display_string;
}

static XIC x_open_xic(Window w);

/* initiate connection with X server */
static unsigned char *x_init_driver(unsigned char *param, unsigned char *display)
{
	unsigned char *err;
	int l;

	XGCValues gcv;
	XSetWindowAttributes win_attr;
	int misordered = -1;

	x_init_hash_table();

#if defined(LC_CTYPE)
	setlocale(LC_CTYPE, "");
#endif
	if (!display || !*display)
		display = NULL;

/*
	X documentation says on XOpenDisplay(display_name) :

	display_name
		Specifies the hardware display name, which determines the dis-
		play and communications domain to be used.  On a POSIX-confor-
		mant system, if the display_name is NULL, it defaults to the
		value of the DISPLAY environment variable.

	But OS/2 has problems when display_name is NULL ...
*/
	x_display_string = stracpy(display ? display : cast_uchar "");

	if (display) {
		char *xx_display = strndup((char *)display, strlen(cast_const_char display) + 1);
		x_display = XOpenDisplay(xx_display);
		free(xx_display);
	} else {
		x_display = XOpenDisplay(NULL);
	}
	if (!x_display) {
		err = init_str();
		l = 0;

		if (display) {
			add_to_str(&err, &l, cast_uchar "Can't open display \"");
			add_to_str(&err, &l, display);
			add_to_str(&err, &l, cast_uchar "\"\n");
		} else {
			add_to_str(&err, &l, cast_uchar "Can't open default display\n");
		}
		goto ret_err;
	}

	x_bitmap_bit_order = BitmapBitOrder(x_display);
	x_screen = DefaultScreen(x_display);
	x_display_height = DisplayHeight(x_display, x_screen);
	x_display_width = DisplayWidth(x_display, x_screen);
	x_root_window = RootWindow(x_display, x_screen);
	x_default_colormap = XDefaultColormap(x_display, x_screen);

	x_default_window_width = x_display_width;
	if (x_default_window_width >= 100)
		x_default_window_width -= 50;
	x_default_window_height = x_display_height;
	if (x_default_window_height >= 150)
		x_default_window_height -= 75;

	x_driver_param = NULL;

	if (param && *param) {
		unsigned char *e;
		char *end_c;
		unsigned long w, h;

		x_driver_param = stracpy(param);

		if (*x_driver_param < '0' || *x_driver_param > '9') {
invalid_param:
			err = stracpy(cast_uchar "Invalid parameter.\n");
			goto ret_err;
		}
		w = strtoul(cast_const_char x_driver_param, &end_c, 10);
		e = cast_uchar end_c;
		if (upcase(*e) != 'X')
			goto invalid_param;
		e++;
		if (*e < '0' || *e > '9')
			goto invalid_param;
		h = strtoul(cast_const_char e, &end_c, 10);
		e = cast_uchar end_c;
		if (*e)
			goto invalid_param;
		if (w && h && w <= 30000 && h <= 30000) {
			x_default_window_width = (int)w;
			x_default_window_height = (int)h;
		}
	}

	/* find best visual */
	{
		static int depths[] = { 24, 16, 15, 8, 4 };
		static int classes[] = { TrueColor, PseudoColor, StaticColor }; /* FIXME: dodelat DirectColor */
		int a, b;

		for (a = 0; a < array_elements(depths); a++)
			for (b = 0; b < array_elements(classes); b++) {
				if ((classes[b] == PseudoColor || classes[b] == StaticColor) && depths[a] > 8)
					continue;
				if (classes[b] == TrueColor && depths[a] <= 8)
					continue;

				if (XMatchVisualInfo(x_display, x_screen, depths[a], classes[b], &vinfo)) {
					XPixmapFormatValues *pfm;
					int n, i;

					x_default_visual = vinfo.visual;
					x_depth = vinfo.depth;

					/* determine bytes per pixel */
					pfm = XListPixmapFormats(x_display, &n);
					for (i = 0; i < n; i++)
						if (pfm[i].depth == x_depth) {
							x_bitmap_bpp = pfm[i].bits_per_pixel < 8 ? 1 : pfm[i].bits_per_pixel >> 3;
							x_bitmap_scanline_pad = pfm[i].scanline_pad >> 3;
							XFree(pfm);
							goto bytes_per_pixel_found;
						}
					if (n)
						XFree(pfm);
					continue;
 bytes_per_pixel_found:
					/* test misordered flag */
					switch(x_depth) {
					case 4:
					case 8:
						if (x_bitmap_bpp != 1)
							break;
						if (vinfo.class == StaticColor || vinfo.class == PseudoColor) {
							misordered = 0;
							goto visual_found;
						}
						break;

					case 15:
					case 16:
						if (x_bitmap_bpp != 2)
							break;
						if (x_depth == 16
						&& vinfo.red_mask == 0xf800
						&& vinfo.green_mask == 0x7e0
						&& vinfo.blue_mask == 0x1f) {
							misordered = x_bitmap_bit_order == MSBFirst ? 256 : 0;
							goto visual_found;
						}
						if (x_depth == 15
						&& vinfo.red_mask == 0x7c00
						&& vinfo.green_mask == 0x3e0
						&& vinfo.blue_mask == 0x1f) {
							misordered = x_bitmap_bit_order == MSBFirst ? 256 : 0;
							goto visual_found;
						}
						if (create_16bit_mapping(vinfo.red_mask, vinfo.green_mask, vinfo.blue_mask)) {
							misordered = 0;
							goto visual_found;
						}
						break;

					case 24:
						if (x_bitmap_bpp != 3
						&& x_bitmap_bpp != 4)
							break;
						if (vinfo.red_mask == 0xff0000
						&& vinfo.green_mask == 0xff00
						&& vinfo.blue_mask == 0xff) {
							misordered = x_bitmap_bpp == 4 && x_bitmap_bit_order == MSBFirst ? 512 : 0;
							goto visual_found;
						}
						if (create_24bit_mapping(vinfo.red_mask, vinfo.green_mask, vinfo.blue_mask)) {
							misordered = 0;
							goto visual_found;
						}
						break;
					}
				}
			}

		err = stracpy(cast_uchar "No supported color depth found.\n");
		goto ret_err;
	}

 visual_found:

	x_driver.flags &= ~GD_SWITCH_PALETTE;
	x_have_palette = 0;
	x_use_static_color_table = 0;
	if (vinfo.class == StaticColor) {
		if (x_depth > 8)
			return stracpy(cast_uchar "Static color supported for up to 8-bit depth.\n");
		if ((err = x_query_palette()))
			goto ret_err;
		x_use_static_color_table = 1;
	}
	if (vinfo.class == PseudoColor) {
		if (x_depth > 8)
			return stracpy(cast_uchar "Static color supported for up to 8-bit depth.\n");
		x_use_static_color_table = !x_driver.param->palette_mode;
		x_have_palette = 1;
		x_colormap = XCreateColormap(x_display, x_root_window, x_default_visual, AllocAll);
		x_set_palette();
		x_driver.flags |= GD_SWITCH_PALETTE;
	}

	x_driver.depth = 0;
	x_driver.depth |= x_bitmap_bpp;
	x_driver.depth |= x_depth << 3;
	x_driver.depth |= misordered;

	/* check if depth is sane */
	if (x_use_static_color_table)
		x_driver.depth = X_STATIC_CODE;
	x_get_color_function = get_color_fn(x_driver.depth);
	if (!x_get_color_function) {
		unsigned char nerr[MAX_STR_LEN];
		snprintf(cast_char nerr, MAX_STR_LEN, "Unsupported graphics mode: x_depth=%d, bits_per_pixel=%d, bytes_per_pixel=%d\n",x_driver.depth, x_depth, x_bitmap_bpp);
		err = stracpy(nerr);
		goto ret_err;
	}

	x_black_pixel = BlackPixel(x_display, x_screen);

	gcv.function = GXcopy;
	/* we want to receive GraphicsExpose events when uninitialized area is discovered during scroll */
	gcv.graphics_exposures = True;
	gcv.fill_style = FillSolid;
	gcv.background = x_black_pixel;

	x_delete_window_atom = XInternAtom(x_display, "WM_DELETE_WINDOW", False);
	x_wm_protocols_atom = XInternAtom(x_display, "WM_PROTOCOLS", False);
	x_sel_atom = XInternAtom(x_display, "SEL_PROP", False);
	x_targets_atom = XInternAtom(x_display, "TARGETS", False);
	x_utf8_string_atom = XInternAtom(x_display, "UTF8_STRING", False);

	if (x_have_palette)
		win_attr.colormap = x_colormap;
	else
		win_attr.colormap = x_default_colormap;

	fake_window = XCreateWindow(x_display, x_root_window, 0, 0, 10, 10, 0,
				x_depth, CopyFromParent, x_default_visual,
				CWColormap, &win_attr);

	fake_window_initialized = 1;

	x_normal_gc = XCreateGC(x_display, fake_window,
			GCFillStyle | GCBackground, &gcv);
	if (!x_normal_gc) {
		err = stracpy(cast_uchar "Cannot create graphic context.\n");
		goto ret_err;
	}
	x_normal_gc_color = 0;
	XSetForeground(x_display, x_normal_gc, x_normal_gc_color);
	XSetLineAttributes(x_display, x_normal_gc, 1, LineSolid, CapRound, JoinRound);

	x_copy_gc = XCreateGC(x_display, fake_window, GCFunction, &gcv);
	if (!x_copy_gc) {
		err = stracpy(cast_uchar "Cannot create graphic context.\n");
		goto ret_err;
	}

	x_drawbitmap_gc = XCreateGC(x_display, fake_window, GCFunction, &gcv);
	if (!x_drawbitmap_gc) {
		err = stracpy(cast_uchar "Cannot create graphic context.\n");
		goto ret_err;
	}

	x_scroll_gc = XCreateGC(x_display, fake_window,
			GCGraphicsExposures | GCBackground, &gcv);
	if (!x_scroll_gc) {
		err = stracpy(cast_uchar "Cannot create graphic context.\n");
		goto ret_err;
	}
	x_scroll_gc_rect.x1 = x_scroll_gc_rect.x2 = x_scroll_gc_rect.y1 = x_scroll_gc_rect.y2 = -1;

	{
#if defined(LC_CTYPE)
		/*
		 * Unfortunatelly, dead keys are translated according to
		 * current locale, even if we use Xutf8LookupString.
		 * So, try to set locale to utf8 for the input method.
		 */
		unsigned char *l;
		int len;
		l = cast_uchar setlocale(LC_CTYPE, "");
		len = l ? (int)strlen((char *)l) : 0;
		if (l
		&& !(len >= 5 && !casestrcmp(l + len - 5, cast_uchar ".utf8"))
		&& !(len >= 6 && !casestrcmp(l + len - 6, cast_uchar ".utf-8"))) {
			unsigned char *m = memacpy(l, strcspn(cast_const_char l, "."));
			add_to_strn(&m, cast_uchar ".UTF-8");
			l = cast_uchar setlocale(LC_CTYPE, (char *)m);
			free(m);
		}
		if (!l)
			l = cast_uchar setlocale(LC_CTYPE, "en_US.UTF-8");
#endif
		xim = XOpenIM(x_display, NULL, NULL, NULL);
#if defined(LC_CTYPE)
		if (!xim) {
			if (!l) l = cast_uchar setlocale(LC_CTYPE, "en_US.UTF-8");
			xim = XOpenIM(x_display, NULL, NULL, NULL);
		}
#endif
		if (xim) {
			XIC xic = x_open_xic(fake_window);
			if (xic)
				XDestroyIC(xic);
			else {
				XCloseIM(xim);
				xim = NULL;
			}
		}
#if defined(LC_CTYPE)
		setlocale(LC_CTYPE, "");
#endif
	}

	x_fd = XConnectionNumber(x_display);
	set_handlers(x_fd, x_process_events, NULL, NULL);

	setup_pixmap_handler();

	XSync(x_display, False);
	X_SCHEDULE_PROCESS_EVENTS();
	return NULL;

 ret_err:
	x_free_hash_table();
	return err;
}


/* close connection with the X server */
static void x_shutdown_driver(void)
{
	set_handlers(x_fd, NULL, NULL, NULL);
	x_free_hash_table();
}

static XIC x_open_xic(Window w)
{
	return XCreateIC(xim, XNInputStyle,
			XIMPreeditNothing | XIMStatusNothing, XNClientWindow,
			w, XNFocusWindow, w, NULL);
}

/* create new window */
static struct graphics_device *x_init_device(void)
{
	struct graphics_device *dev;
	XWMHints wm_hints;
	XClassHint class_hints;
	XTextProperty windowName;
	char *links_name = "Links";
	XSetWindowAttributes win_attr;
	struct window_info *wi;

	dev = xmalloc(sizeof(struct graphics_device));

	wi = mem_calloc(sizeof(struct window_info));

	dev->size.x1 = 0;
	dev->size.y1 = 0;
	dev->size.x2 = x_default_window_width;
	dev->size.y2 = x_default_window_height;

	if (x_have_palette)
		win_attr.colormap = x_colormap;
	else
		win_attr.colormap = x_default_colormap;
	win_attr.border_pixel = x_black_pixel;

	x_prepare_for_failure(X_CreateWindow);
	wi->window = XCreateWindow(x_display, x_root_window, dev->size.x1,
				dev->size.y1, dev->size.x2, dev->size.y2,
				X_BORDER_WIDTH, x_depth, InputOutput,
				x_default_visual, CWColormap | CWBorderPixel,
				&win_attr);
	if (x_test_for_failure()) {
		x_prepare_for_failure(X_DestroyWindow);
		XDestroyWindow(x_display, wi->window);
		x_test_for_failure();
		free(dev);
		free(wi);
		return NULL;
	}

	wm_hints.flags = InputHint;
	wm_hints.input = True;

	XSetWMHints(x_display, wi->window, &wm_hints);
	class_hints.res_name = links_name;
	class_hints.res_class = links_name;
	XSetClassHint(x_display, wi->window, &class_hints);
	XStringListToTextProperty(&links_name, 1, &windowName);
	XSetWMName(x_display, wi->window, &windowName);
	XStoreName(x_display, wi->window, links_name);
	XSetWMIconName(x_display, wi->window, &windowName);
	XFree(windowName.value);

	XMapWindow(x_display,wi->window);

	dev->clip.x1 = dev->size.x1;
	dev->clip.y1 = dev->size.y1;
	dev->clip.x2 = dev->size.x2;
	dev->clip.y2 = dev->size.y2;
	dev->driver_data = wi;
	dev->user_data = 0;

	XSetWindowBackgroundPixmap(x_display, wi->window, None);
	x_add_to_table(dev);

	XSetWMProtocols(x_display,wi->window,&x_delete_window_atom,1);

	XSelectInput(x_display, wi->window,
		ExposureMask
		| KeyPressMask
		| ButtonPressMask
		| ButtonReleaseMask
		| PointerMotionMask
		| ButtonMotionMask
		| StructureNotifyMask
		| 0);

	if (xim)
		wi->xic = x_open_xic(wi->window);

	XSync(x_display, False);
	X_SCHEDULE_PROCESS_EVENTS();
	n_wins++;
	return dev;
}


static void x_shutdown_device(struct graphics_device *dev)
{
	struct window_info *wi = get_window_info(dev);

	n_wins--;
	XDestroyWindow(x_display, wi->window);
	if (wi->xic)
		XDestroyIC(wi->xic);
	XSync(x_display, False);
	X_SCHEDULE_PROCESS_EVENTS();

	x_remove_from_table(wi->window);
	free(wi);
	free(dev);
}

static void x_update_palette(void)
{
	if (x_use_static_color_table == !x_driver.param->palette_mode)
		return;

	x_use_static_color_table = !x_driver.param->palette_mode;

	if (x_use_static_color_table)
		x_driver.depth = X_STATIC_CODE;
	else
		x_driver.depth = x_bitmap_bpp | (x_depth << 3);
	x_get_color_function = get_color_fn(x_driver.depth);
	if (!x_get_color_function)
		internal("x: unsupported depth %d", x_driver.depth);

	x_set_palette();
}

static unsigned short *x_get_real_colors(void)
{
	int perfect = 1;
	int i;
	unsigned short *v;
	if (!x_use_static_color_table)
		return NULL;
	v = mem_calloc(256 * 3 * sizeof(unsigned short));
	for (i = 0; i < X_STATIC_COLORS; i++) {
		unsigned idx = static_color_table[i];
		v[i * 3 + 0] = static_color_map[idx].red;
		v[i * 3 + 1] = static_color_map[idx].green;
		v[i * 3 + 2] = static_color_map[idx].blue;
		if (perfect) {
			unsigned rgb[256];
			q_palette(X_STATIC_COLORS, i, 65535, rgb);
			if (static_color_map[idx].red != rgb[0]
			|| static_color_map[idx].green != rgb[1]
			|| static_color_map[idx].blue != rgb[2]) {
				perfect = 0;
			}
		}
	}
	if (perfect) {
		free(v);
		return NULL;
	}
	return v;
}

static void x_translate_colors(unsigned char *data, int x, int y, int skip)
{
	int i, j;

	if (color_map_16bit) {
		for (j = 0; j < y; j++) {
			for (i = 0; i < x; i++) {
				unsigned short s = color_map_16bit[data[2*i]] |
						color_map_16bit[data[2*i + 1] + 256];
				data[2*i] = (unsigned char)s;
				data[2*i + 1] = (unsigned char)(s >> 8);
			}
			data += skip;
		}
		return;
	}

	if (color_map_24bit && x_bitmap_bpp == 3) {
		for (j = 0; j < y; j++) {
			for (i = 0; i < x; i++) {
				unsigned int s = color_map_24bit[data[3*i]] |
						color_map_24bit[data[3*i + 1] + 256] |
						color_map_24bit[data[3*i + 2] + 256 * 2];
				data[3*i] = (unsigned char)s;
				data[3*i + 1] = (unsigned char)(s >> 8);
				data[3*i + 2] = (unsigned char)(s >> 16);
			}
			data += skip;
		}
		return;
	}

	if (color_map_24bit && x_bitmap_bpp == 4) {
		for (j = 0; j < y; j++) {
			for (i = 0; i < x; i++) {
				unsigned int s = color_map_24bit[data[4*i]] |
						color_map_24bit[data[4*i + 1] + 256] |
						color_map_24bit[data[4*i + 2] + 256 * 2];
				data[4*i] = (unsigned char)s;
				data[4*i + 1] = (unsigned char)(s >> 8);
				data[4*i + 2] = (unsigned char)(s >> 16);
				data[4*i + 3] = (unsigned char)(s >> 24);
			}
			data += skip;
		}
		return;
	}

	if (x_use_static_color_table) {
		for (j = 0; j < y; j++) {
			for (i = 0; i < x; i++)
				data[i] = static_color_table[data[i]];
			data += skip;
		}
		return;
	}
}

static int x_get_empty_bitmap(struct bitmap *bmp)
{
	struct x_pixmapa *p;
	int pad;
	p = xmalloc(sizeof(struct x_pixmapa));
	p->type = X_TYPE_NOTHING;
	add_to_list(bitmaps, p);
	bmp->data = NULL;
	bmp->flags = p;
	if (bmp->x > (INT_MAX - x_bitmap_scanline_pad) / x_bitmap_bpp)
		return -1;
	pad = x_bitmap_scanline_pad - ((bmp->x * x_bitmap_bpp) % x_bitmap_scanline_pad);
	if (pad == x_bitmap_scanline_pad)
		pad = 0;
	bmp->skip = bmp->x * x_bitmap_bpp + pad;
	if (bmp->skip * bmp->y > INT_MAX)
		return -1;
	bmp->data = xmalloc(bmp->skip * bmp->y);
	return 0;
}

static void x_register_bitmap(struct bitmap *bmp)
{
	struct x_pixmapa *p;
	XImage *image;
	Pixmap pixmap = 0;
	int can_create_pixmap;

	if (!bmp->data || !bmp->x || !bmp->y)
		goto cant_create;

	x_translate_colors(bmp->data, bmp->x, bmp->y, bmp->skip);

	/* alloc XImage in client's memory */
 retry:
	image = XCreateImage(x_display, x_default_visual, x_depth, ZPixmap, 0,
			(char *)bmp->data, bmp->x, bmp->y, x_bitmap_scanline_pad << 3,
			bmp->skip);
	if (!image) {
		if (out_of_memory())
			goto retry;
		goto cant_create;
	}

	/* try to alloc XPixmap in server's memory */
	can_create_pixmap = 1;

	if (bmp->x >= 32768 || bmp->y >= 32768) {
		can_create_pixmap = 0;
		goto no_pixmap;
	}

	if (!pixmap_mode) x_prepare_for_failure(X_CreatePixmap);
	pixmap = XCreatePixmap(x_display, fake_window, bmp->x, bmp->y, x_depth);
	if (!pixmap_mode && x_test_for_failure()) {
		if (pixmap) {
			x_prepare_for_failure(X_FreePixmap);
			XFreePixmap(x_display, pixmap);
			x_test_for_failure();
			pixmap = 0;
		}
	}
	if (!pixmap) {
		can_create_pixmap = 0;
	}

 no_pixmap:

	p = XPIXMAPP(bmp->flags);

	if (can_create_pixmap) {
		XPutImage(x_display, pixmap, x_copy_gc, image, 0, 0, 0, 0,
			bmp->x, bmp->y);
		XDestroyImage(image);
		p->type = X_TYPE_PIXMAP;
		p->data.pixmap = pixmap;
	} else {
		p->type = X_TYPE_IMAGE;
		p->data.image = image;
	}
	bmp->data = NULL;
	return;

 cant_create:
	free(bmp->data);
	bmp->data = NULL;
	return;
}


static void x_unregister_bitmap(struct bitmap *bmp)
{
	struct x_pixmapa *p = XPIXMAPP(bmp->flags);

	switch (p->type) {
	case X_TYPE_NOTHING:
		break;

	case X_TYPE_PIXMAP:
		XFreePixmap(x_display, p->data.pixmap);   /* free XPixmap from server's memory */
		break;

	case X_TYPE_IMAGE:
		XDestroyImage(p->data.image);  /* free XImage from client's memory */
		break;
	default:
		internal("invalid pixmap type %d", (int)p->type);
	}
	del_from_list(p);
	free(p);
}

static long x_get_color(int rgb)
{
	long block;
	unsigned char *b;

	block = x_get_color_function(rgb);
	b = (unsigned char *)&block;

	x_translate_colors(b, 1, 1, 0);

	/*fprintf(stderr, "bitmap bpp %d\n", x_bitmap_bpp);*/
	switch (x_bitmap_bpp) {
	case 1:
		return b[0];
	case 2:
		if (x_bitmap_bit_order == LSBFirst)
			return (unsigned)b[0] | ((unsigned)b[1] << 8);
		return (unsigned)b[1] | ((unsigned)b[0] << 8);
	case 3:
		if (x_bitmap_bit_order == LSBFirst)
			return (unsigned)b[0] | ((unsigned)b[1] << 8) | ((unsigned)b[2] << 16);
		else
			return (unsigned)b[2] | ((unsigned)b[1] << 8) | ((unsigned)b[0] << 16);
	default:
		if (x_bitmap_bit_order == LSBFirst)
			return (unsigned)b[0] | ((unsigned)b[1] << 8) | ((unsigned)b[2] << 16) | ((unsigned)b[3] << 24);
		return (unsigned)b[3] | ((unsigned)b[2] << 8) | ((unsigned)b[1] << 16) | ((unsigned)b[0] << 24);
	}
}

static inline void x_set_color(long color)
{
	if (color != x_normal_gc_color) {
		x_normal_gc_color = color;
		XSetForeground(x_display, x_normal_gc, color);
	}
}

static void x_fill_area(struct graphics_device *dev, int x1, int y1, int x2, int y2, long color)
{
	CLIP_FILL_AREA

	x_set_color(color);
	XFillRectangle(x_display, get_window_info(dev)->window, x_normal_gc, x1,
		y1, x2 - x1, y2 - y1);
	X_FLUSH();
}

static void x_draw_hline(struct graphics_device *dev, int x1, int y, int x2, long color)
{
	CLIP_DRAW_HLINE

	x_set_color(color);
	XDrawLine(x_display, get_window_info(dev)->window, x_normal_gc, x1, y,
		x2 - 1, y);
	X_FLUSH();
}

static void x_draw_vline(struct graphics_device *dev, int x, int y1, int y2, long color)
{
	CLIP_DRAW_VLINE

	x_set_color(color);
	XDrawLine(x_display, get_window_info(dev)->window, x_normal_gc, x, y1,
		x, y2 - 1);
	X_FLUSH();
}

static void x_draw_bitmap(struct graphics_device *dev, struct bitmap *bmp, int x, int y)
{
	struct x_pixmapa *p;
	int bmp_off_x, bmp_off_y, bmp_size_x, bmp_size_y;

	CLIP_DRAW_BITMAP

	bmp_off_x = 0;
	bmp_off_y = 0;
	bmp_size_x = bmp->x;
	bmp_size_y = bmp->y;
	if (x < dev->clip.x1) {
		bmp_off_x = dev->clip.x1 - x;
		bmp_size_x -= dev->clip.x1 - x;
		x = dev->clip.x1;
	}
	if (x + bmp_size_x > dev->clip.x2)
		bmp_size_x = dev->clip.x2 - x;
	if (y < dev->clip.y1) {
		bmp_off_y = dev->clip.y1 - y;
		bmp_size_y -= dev->clip.y1 - y;
		y = dev->clip.y1;
	}
	if (y + bmp_size_y > dev->clip.y2)
		bmp_size_y = dev->clip.y2 - y;

	p = XPIXMAPP(bmp->flags);

	switch (p->type) {
	case X_TYPE_NOTHING:
		break;

	case X_TYPE_PIXMAP:
		XCopyArea(x_display, p->data.pixmap,
			get_window_info(dev)->window, x_drawbitmap_gc,
			bmp_off_x, bmp_off_y, bmp_size_x, bmp_size_y, x, y);
		break;

	case X_TYPE_IMAGE:
		XPutImage(x_display, get_window_info(dev)->window,
			x_drawbitmap_gc, p->data.image,
			bmp_off_x, bmp_off_y, x, y, bmp_size_x, bmp_size_y);
		break;
	default:
		internal("invalid pixmap type %d", (int)p->type);
	}
	X_FLUSH();
}


static void *x_prepare_strip(struct bitmap *bmp, int top, int lines)
{
	struct x_pixmapa *p = XPIXMAPP(bmp->flags);
	XImage *image;
	void *x_data;

	bmp->data = NULL;

	switch (p->type) {
	case X_TYPE_NOTHING:
		return NULL;

	case X_TYPE_PIXMAP:
		x_data = xmalloc(bmp->skip * lines);
		image = XCreateImage(x_display, x_default_visual, x_depth,
				ZPixmap, 0, x_data, bmp->x, lines,
				x_bitmap_scanline_pad << 3, bmp->skip);
		if (!image) {
			free(x_data);
			return NULL;
		}
		bmp->data = image;
		return image->data;

	case X_TYPE_IMAGE:
		return p->data.image->data + (bmp->skip * top);
	}
	internal("invalid pixmap type %d", (int)p->type);
	return NULL;
}

static void x_commit_strip(struct bitmap *bmp, int top, int lines)
{
	struct x_pixmapa *p = XPIXMAPP(bmp->flags);

	bmp->data = NULL;

	switch (p->type) {
	case X_TYPE_NOTHING:
		return;
	case X_TYPE_PIXMAP:
		if (!bmp->data)
			return;
		x_translate_colors((unsigned char *)((XImage*)bmp->data)->data,
			bmp->x, lines, bmp->skip);
		XPutImage(x_display, p->data.pixmap,
			x_copy_gc, (XImage *)bmp->data, 0, 0, 0, top, bmp->x, lines);
		XDestroyImage((XImage *)bmp->data);
		return;
	case X_TYPE_IMAGE:
		x_translate_colors((unsigned char *)p->data.image->data + (bmp->skip * top), bmp->x, lines, bmp->skip);
		/* everything has been done by user */
		return;
	}
	internal("invalid pixmap type %d", (int)p->type);
}

static int x_scroll(struct graphics_device *dev, struct rect_set **set, int scx, int scy)
{
	XEvent ev;
	struct rect r;
	if (memcmp(&dev->clip, &x_scroll_gc_rect, sizeof(struct rect))) {
		XRectangle xr;

		memcpy(&x_scroll_gc_rect, &dev->clip, sizeof(struct rect));

		xr.x = dev->clip.x1;
		xr.y = dev->clip.y1;
		xr.width = dev->clip.x2 - dev->clip.x1;
		xr.height = dev->clip.y2 - dev->clip.y1;

		XSetClipRectangles(x_display, x_scroll_gc, 0, 0, &xr, 1, Unsorted);
	}

	XCopyArea(x_display, get_window_info(dev)->window,
		get_window_info(dev)->window, x_scroll_gc, dev->clip.x1,
		dev->clip.y1, dev->clip.x2 - dev->clip.x1,
		dev->clip.y2 - dev->clip.y1, dev->clip.x1 + scx,
		dev->clip.y1 + scy);
	XSync(x_display, False);
	/* ten sync tady musi byt, protoze potrebuju zarucit, aby vsechny
	* graphics-expose vyvolane timto scrollem byly vraceny v rect-set */

	/* take all graphics expose events for this window and put them into the rect set */
	while (XCheckWindowEvent(x_display, get_window_info(dev)->window, ExposureMask, &ev) == True) {
		switch(ev.type) {
		case GraphicsExpose:
			r.x1 = ev.xgraphicsexpose.x;
			r.y1 = ev.xgraphicsexpose.y;
			r.x2 = ev.xgraphicsexpose.x + ev.xgraphicsexpose.width;
			r.y2 = ev.xgraphicsexpose.y + ev.xgraphicsexpose.height;
			break;

		case Expose:
			r.x1 = ev.xexpose.x;
			r.y1 = ev.xexpose.y;
			r.x2 = ev.xexpose.x + ev.xexpose.width;
			r.y2 = ev.xexpose.y + ev.xexpose.height;
			break;

		default:
			continue;
		}

		if (r.x1 < dev->clip.x1 || r.x2 > dev->clip.x2
		|| r.y1 < dev->clip.y1 || r.y2 > dev->clip.y2) {
			switch(ev.type) {
			case GraphicsExpose:
				ev.xgraphicsexpose.x = 0;
				ev.xgraphicsexpose.y = 0;
				ev.xgraphicsexpose.width = dev->size.x2;
				ev.xgraphicsexpose.height = dev->size.y2;
				break;

			case Expose:
				ev.xexpose.x = 0;
				ev.xexpose.y = 0;
				ev.xexpose.width = dev->size.x2;
				ev.xexpose.height = dev->size.y2;
				break;
			}
			XPutBackEvent(x_display, &ev);
			free(*set);
			*set = init_rect_set();
			break;
		}
		add_to_rect_set(set, &r);
	}
	X_SCHEDULE_PROCESS_EVENTS();
	return 1;
}

static void x_flush(struct graphics_device *dev)
{
	unregister_bottom_half(x_do_flush, NULL);
	x_do_flush(NULL);
}


static void x_set_window_title(struct graphics_device *dev, unsigned char *title)
{
	unsigned char *t;
	char *xx_str;
	XTextProperty windowName;
	Status ret;

	if (!dev)
		internal("x_set_window_title called with NULL graphics_device pointer.\n");
	t = convert(0, 0, title, NULL);
	clr_white(t);

	xx_str = strndup((char *)t, strlen(cast_const_char t) + 1);
	free(t);

	if (XSupportsLocale()) {
		ret = XmbTextListToTextProperty(x_display, &xx_str,
					1, XStdICCTextStyle, &windowName);
#ifdef X_HAVE_UTF8_STRING
		if (ret > 0) {
			XFree(windowName.value);
			ret = XmbTextListToTextProperty(x_display,
						&xx_str, 1,
						XUTF8StringStyle, &windowName);
			if (ret < 0)
				ret = XmbTextListToTextProperty(x_display,
							&xx_str,
							1, XStdICCTextStyle,
							&windowName);
		}
#endif
		if (ret < 0)
			goto retry_print_ascii;
	} else {
 retry_print_ascii:
		ret = XStringListToTextProperty(&xx_str, 1, &windowName);
		if (!ret) {
			free(xx_str);
			return;
		}
	}
	free(xx_str);
	XSetWMName(x_display, get_window_info(dev)->window, &windowName);
	XSetWMIconName(x_display, get_window_info(dev)->window, &windowName);
	XFree(windowName.value);
	X_FLUSH();
}

/* gets string in UTF8 */
static void x_set_clipboard_text(struct graphics_device *dev, unsigned char *text)
{
	x_clear_clipboard();
	if (text) {
		x_my_clipboard = stracpy(text);

		XSetSelectionOwner(x_display, XA_PRIMARY,
			get_window_info(dev)->window, CurrentTime);
		XFlush(x_display);
		X_SCHEDULE_PROCESS_EVENTS();
	}
}

static void selection_request(XEvent *event)
{
	XSelectionRequestEvent *req;
	XSelectionEvent sel;
	size_t l;
	unsigned char *xx_str;

	req = &(event->xselectionrequest);
	sel.type = SelectionNotify;
	sel.requestor = req->requestor;
	sel.selection = XA_PRIMARY;
	sel.target = req->target;
	sel.property = req->property;
	sel.time = req->time;
	sel.display = req->display;
	if (req->target == XA_STRING) {
		unsigned char *str, *p;
		if (!x_my_clipboard)
			str = stracpy(cast_uchar "");
		else
			str = convert(0, 0, x_my_clipboard, NULL);

		for (p = cast_uchar strchr((char *)str, 1); p;
		     p = cast_uchar strchr((char *)(str + 1), 1))
			*p = 0xa0;
		l = strlen(cast_const_char str);
		if (l > X_MAX_CLIPBOARD_SIZE)
			l = X_MAX_CLIPBOARD_SIZE;
		xx_str = (unsigned char *)strndup((char *)str, l);
		XChangeProperty(x_display, sel.requestor, sel.property,
			XA_STRING, 8, PropModeReplace, xx_str, l);
		free(str);
		free(xx_str);
	} else if (req->target == x_utf8_string_atom) {
		l = x_my_clipboard ? strlen((char *)x_my_clipboard) : 0;
		if (l > X_MAX_CLIPBOARD_SIZE)
			l = X_MAX_CLIPBOARD_SIZE;
		xx_str = (unsigned char *)strndup((char *)x_my_clipboard, l);
		XChangeProperty(x_display, sel.requestor, sel.property,
			x_utf8_string_atom, 8, PropModeReplace, xx_str,
			l);
		free(xx_str);
	} else if (req->target == x_targets_atom) {
		unsigned tgt_atoms[3];
		tgt_atoms[0] = (unsigned)x_targets_atom;
		tgt_atoms[1] = XA_STRING;
		tgt_atoms[2] = (unsigned)x_utf8_string_atom;
		XChangeProperty(x_display, sel.requestor, sel.property,
			XA_ATOM, 32, PropModeReplace,
			(unsigned char *)&tgt_atoms, 3);
	} else
		sel.property = None;
	XSendEvent(x_display, sel.requestor, 0, 0, (XEvent*)&sel);
	XFlush(x_display);
	X_SCHEDULE_PROCESS_EVENTS();
}

static unsigned char *x_get_clipboard_text(void)
{
	XEvent event;
	Atom type_atom = x_utf8_string_atom;

 retry:
	XConvertSelection(x_display, XA_PRIMARY, type_atom, x_sel_atom,
			fake_window, CurrentTime);

	while (1) {
		XSync(x_display, False);
		if (XCheckTypedEvent(x_display,SelectionRequest, &event)) {
			selection_request(&event);
			continue;
		}
		if (XCheckTypedEvent(x_display,SelectionNotify, &event))
			break;
		x_wait_for_event();
	}
	if (event.xselection.property) {
		unsigned char *buffer;
		unsigned long pty_size, pty_items;
		int pty_format, ret;
		Atom pty_type;

		if (event.xselection.target != type_atom) goto no_new_sel;
		if (event.xselection.property != x_sel_atom) goto no_new_sel;


		/* Get size and type of property */
		ret = XGetWindowProperty(x_display, fake_window,
				event.xselection.property, 0, 0, False,
				AnyPropertyType, &pty_type, &pty_format,
				&pty_items, &pty_size, &buffer);
		if (ret != Success)
			goto no_new_sel;
		XFree(buffer);

		ret = XGetWindowProperty(x_display, fake_window,
				event.xselection.property, 0, (long)pty_size,
				True, AnyPropertyType, &pty_type, &pty_format,
				&pty_items, &pty_size, &buffer);
		if (ret != Success)
			goto no_new_sel;

		pty_size = (pty_format >> 3) * pty_items;

		x_clear_clipboard();
		if (type_atom == x_utf8_string_atom)
			x_my_clipboard = stracpy(buffer);
		else
			x_my_clipboard = convert(0, 0, buffer, NULL);

		XFree(buffer);
	} else if (type_atom == x_utf8_string_atom) {
			type_atom = XA_STRING;
			goto retry;
	}

 no_new_sel:
	X_SCHEDULE_PROCESS_EVENTS();
	if (!x_my_clipboard)
		return NULL;
	return stracpy(x_my_clipboard);
}

/* This is executed in a helper thread, so we must not use mem_alloc */

static void addchr(char *str, size_t *l, char c)
{
	char *s;
	if (!str)
		return;
	if (str[*l])
		*l = strlen(str);
	if (*l > INT_MAX - 2)
		overalloc();
	s = xrealloc(str, *l + 2);
	if (!s) {
		free(str);
		str = NULL;
		return;
	}
	str = s;
	s[(*l)++] = c;
	s[*l] = 0;
}

static int x_exec(unsigned char *command, int fg)
{
	char *pattern, *final;
	size_t i, j, l;
	int retval;

	if (!fg) {
		retval = system(cast_const_char command);
		return retval;
	}

	l = 0;
	if (*x_driver.param->shell_term)
		pattern = strdup((char *)x_driver.param->shell_term);
	else {
		pattern = strdup((char *)links_xterm());
		if (*command) {
			addchr(pattern, &l, ' ');
			addchr(pattern, &l, '%');
		}
	}
	if (!pattern)
		return -1;

	final = strdup("");
	l = 0;
	for (i = 0; pattern[i]; i++) {
		if (pattern[i] == '%')
			for (j = 0; j < strlen((char *)command); j++)
				addchr(final, &l, (char)command[j]);
		else
			addchr(final, &l, pattern[i]);
	}
	free(pattern);
	if (!final)
		return -1;

	retval = system(final);
	free(final);
	return retval;
}

struct graphics_driver x_driver = {
	cast_uchar "x",
	x_init_driver,
	x_init_device,
	x_shutdown_device,
	x_shutdown_driver,
	x_get_driver_param,
	x_get_af_unix_name,
	x_get_empty_bitmap,
	x_register_bitmap,
	x_prepare_strip,
	x_commit_strip,
	x_unregister_bitmap,
	x_draw_bitmap,
	x_get_color,
	x_fill_area,
	x_draw_hline,
	x_draw_vline,
	x_scroll,
	NULL,
	x_flush,
	x_update_palette,
	x_get_real_colors,
	x_set_window_title,
	x_exec,
	x_set_clipboard_text,
	x_get_clipboard_text,
	0,				/* depth (filled in x_init_driver function) */
	0, 0,				/* size (in X is empty) */
	GD_UNICODE_KEYS,		/* flags */
	NULL,				/* param */
};
