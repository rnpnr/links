/* imgcache.c
 * Image cache
 * (c) 2002 Mikulas Patocka
 * This file is a part of the Links program, released under GPL.
 */

#ifdef G

#include "links.h"

static struct list_head image_cache = { &image_cache, &image_cache };

/* xyw_meaning either MEANING_DIMS or MEANING_AUTOSCALE. */
struct cached_image *find_cached_image(int bg, unsigned char *url, int xw, int yw, int xyw_meaning, int scale, unsigned aspect)
{
	struct cached_image *i;
	struct list_head *li;
	if (xw >= 0 && yw >= 0 && xyw_meaning == MEANING_DIMS) {
		/* The xw and yw is already scaled so that scale and
		 * aspect don't matter.
		 */
		foreach(struct cached_image, i, li, image_cache) {
			if (i->background_color == bg
				&& !strcmp(cast_const_char i->url, cast_const_char url)
				&& i->wanted_xw == xw
				&& i->wanted_yw == yw
				&& i->wanted_xyw_meaning == xyw_meaning
				) goto hit;
		}
	}else{
		foreach(struct cached_image, i, li, image_cache) {
			if (i->background_color == bg
				&& !strcmp(cast_const_char i->url, cast_const_char url)
				&& i->wanted_xw == xw
				&& i->wanted_yw == yw
				&& i->wanted_xyw_meaning == xyw_meaning
				&& i->scale == scale
				&& i->aspect == aspect
				) goto hit;
		}
	}
	return NULL;

hit:
	i->refcount++;
	del_from_list(i);
	add_to_list(image_cache, i);
	return i;
}

void add_image_to_cache(struct cached_image *ci)
{
	add_to_list(image_cache, ci);
}

static unsigned long image_size(struct cached_image *cimg)
{
	unsigned long siz = sizeof(struct cached_image);
	switch(cimg->state){
		case 0:
		case 1:
		case 2:
		case 3:
		case 8:
		case 9:
		case 10:
		case 11:
		break;

		case 12:
		case 14:
		siz+=(unsigned long)cimg->width*cimg->height*cimg->buffer_bytes_per_pixel;
		if (cimg->bmp_used){
			case 13:
			case 15:
			siz+=(unsigned long)cimg->bmp.x*cimg->bmp.y*(drv->depth&7);
		}
		break;

#ifdef DEBUG
		default:
		fprintf(stderr,"cimg->state=%d\n",cimg->state);
		internal("Invalid cimg->state in image_size\n");
		break;
#endif /* #ifdef DEBUG */
	}
	return siz;
}

static int shrink_image_cache(int u)
{
	struct cached_image *i;
	struct list_head *li;
	longlong si = 0;
	int r = 0;
	foreach(struct cached_image, i, li, image_cache) if (!i->refcount) si += image_size(i);
	foreachback(struct cached_image, i, li, image_cache) {
		if (si <= image_cache_size && u == SH_CHECK_QUOTA)
			break;
		if (i->refcount)
			continue;
		li = li->next;
		r = ST_SOMETHING_FREED;
		si -= image_size(i);
		del_from_list(i);
		img_destruct_cached_image(i);
		if (u == SH_FREE_SOMETHING) break;
	}
	return r | (list_empty(image_cache) ? ST_CACHE_EMPTY : 0);
}

my_uintptr_t imgcache_info(int type)
{
	struct cached_image *i;
	struct list_head *li;
	my_uintptr_t n = 0;
	foreach(struct cached_image, i, li, image_cache) {
		switch (type) {
			case CI_BYTES:
				n += image_size(i);
				break;
			case CI_LOCKED:
				if (!i->refcount) break;
				/*-fallthrough*/
			case CI_FILES:
				n++;
				break;
			default:
				internal("imgcache_info: query %d", type);
		}
	}
	return n;
}

void init_imgcache(void)
{
	register_cache_upcall(shrink_image_cache, MF_GPI, cast_uchar "imgcache");
}

#endif
