/* drivers.c
 * (c) 2002 Mikulas Patocka
 * This file is a part of the Links program, released under GPL
 */

#ifdef G

#include "links.h"

int F = 0;

struct graphics_driver *drv = NULL;

extern struct graphics_driver x_driver;

/* Driver je jednorazovy argument, kterej se preda grafickymu driveru, nikde se dal
 * neuklada.  Param se skladuje v default_driver param a uklada se do konfiguraku. Pred
 * ukoncenim grafickeho driveru se nastavi default_driver_param podle
 * drv->get_driver_param.
 */
static unsigned char *init_graphics_driver(unsigned char *p, unsigned char *display)
{
	unsigned char *r;
	struct driver_param *dp = get_driver_param(x_driver.name);
	if (!p || !*p)
		p = dp->param;
	drv = &x_driver;
	if ((r = x_driver.init_driver(p, display)))
		drv = NULL;
	else
		F = 1;
	return r;
}


void add_graphics_drivers(unsigned char **s, int *l)
{
	add_to_str(s, l, x_driver.name);
}

unsigned char *init_graphics(unsigned char *driver, unsigned char *param, unsigned char *display)
{
	unsigned char *s = init_str();
	int l = 0;
	if (!driver || !*driver || !casestrcmp(x_driver.name, driver)) {
		unsigned char *r;
		if (!(r = init_graphics_driver(param, display))) {
			free(s);
			return NULL;
		}
		if (!driver || !*driver)
			add_to_str(&s, &l, cast_uchar "Could not initialize any graphics driver. Tried the following drivers:\n");
		else
			add_to_str(&s, &l, cast_uchar "Could not initialize graphics driver ");
		add_to_str(&s, &l, x_driver.name);
		add_to_str(&s, &l, cast_uchar ":\n");
		add_to_str(&s, &l, r);
		free(r);
	}
	if (!l) {
		add_to_str(&s, &l, cast_uchar "Unknown graphics driver ");
		if (driver) add_to_str(&s, &l, driver);
		add_to_str(&s, &l, cast_uchar ".\nThe following graphics drivers are supported:\n");
		add_graphics_drivers(&s, &l);
		add_to_str(&s, &l, cast_uchar "\n");
	}
	return s;
}

void shutdown_graphics(void)
{
	if (drv) {
		drv->shutdown_driver();
		drv = NULL;
		F = 0;
	}
}

void update_driver_param(void)
{
	if (drv && drv->param) {
		struct driver_param *dp = drv->param;
		free(dp->param);
		dp->param = NULL;
		if (drv->get_driver_param)
			dp->param = stracpy(drv->get_driver_param());
		dp->nosave = 0;
	}
}

int g_kbd_codepage(struct graphics_driver *drv)
{
	if (drv->param->kbd_codepage > 0)
		return drv->param->kbd_codepage;
	return 0;
}

/* FIXME */
int do_rects_intersect(struct rect *r1, struct rect *r2)
{
	return (r1->x1 > r2->x1 ? r1->x1 : r2->x1) < (r1->x2 > r2->x2 ? r2->x2 : r1->x2) && (r1->y1 > r2->y1 ? r1->y1 : r2->y1) < (r1->y2 > r2->y2 ? r2->y2 : r1->y2);
}

void intersect_rect(struct rect *v, struct rect *r1, struct rect *r2)
{
	v->x1 = r1->x1 > r2->x1 ? r1->x1 : r2->x1;
	v->x2 = r1->x2 > r2->x2 ? r2->x2 : r1->x2;
	v->y1 = r1->y1 > r2->y1 ? r1->y1 : r2->y1;
	v->y2 = r1->y2 > r2->y2 ? r2->y2 : r1->y2;
}

void unite_rect(struct rect *v, struct rect *r1, struct rect *r2)
{
	if (!is_rect_valid(r1)) {
		if (v != r2) memcpy(v, r2, sizeof(struct rect));
		return;
	}
	if (!is_rect_valid(r2)) {
		if (v != r1) memcpy(v, r1, sizeof(struct rect));
		return;
	}
	v->x1 = r1->x1 < r2->x1 ? r1->x1 : r2->x1;
	v->x2 = r1->x2 < r2->x2 ? r2->x2 : r1->x2;
	v->y1 = r1->y1 < r2->y1 ? r1->y1 : r2->y1;
	v->y2 = r1->y2 < r2->y2 ? r2->y2 : r1->y2;
}

#define R_GR   8

struct rect_set *init_rect_set(void)
{
	struct rect_set *s;
	s = mem_calloc(sizeof(struct rect_set) + sizeof(struct rect) * R_GR);
	s->rl = R_GR;
	s->m = 0;
	return s;
}

void add_to_rect_set(struct rect_set **s, struct rect *r)
{
	struct rect_set *ss = *s;
	int i;
	if (!is_rect_valid(r)) return;
	for (i = 0; i < ss->rl; i++) if (!ss->r[i].x1 && !ss->r[i].x2 && !ss->r[i].y1 && !ss->r[i].y2) {
 x:
		memcpy(&ss->r[i], r, sizeof(struct rect));
		if (i >= ss->m) ss->m = i + 1;
		return;
	}
	if ((unsigned)ss->rl > (INT_MAX - sizeof(struct rect_set)) / sizeof(struct rect) - R_GR) overalloc();
	ss = xrealloc(ss, sizeof(struct rect_set) + sizeof(struct rect) * (ss->rl + R_GR));
	memset(&(*s = ss)->r[i = (ss->rl += R_GR) - R_GR], 0, sizeof(struct rect) * R_GR);
	goto x;
}

void exclude_rect_from_set(struct rect_set **s, struct rect *r)
{
	int i, a;
	struct rect *rr;
	do {
		a = 0;
		for (i = 0; i < (*s)->m; i++) if (do_rects_intersect(rr = &(*s)->r[i], r)) {
			struct rect r1, r2, r3, r4;
			r1.x1 = rr->x1;
			r1.x2 = rr->x2;
			r1.y1 = rr->y1;
			r1.y2 = r->y1;

			r2.x1 = rr->x1;
			r2.x2 = r->x1;
			r2.y1 = r->y1;
			r2.y2 = r->y2;

			r3.x1 = r->x2;
			r3.x2 = rr->x2;
			r3.y1 = r->y1;
			r3.y2 = r->y2;

			r4.x1 = rr->x1;
			r4.x2 = rr->x2;
			r4.y1 = r->y2;
			r4.y2 = rr->y2;

			intersect_rect(&r2, &r2, rr);
			intersect_rect(&r3, &r3, rr);
			rr->x1 = rr->x2 = rr->y1 = rr->y2 = 0;
			add_to_rect_set(s, &r1);
			add_to_rect_set(s, &r2);
			add_to_rect_set(s, &r3);
			add_to_rect_set(s, &r4);
			a = 1;
		}
	} while (a);
}

void set_clip_area(struct graphics_device *dev, struct rect *r)
{
	dev->clip = *r;
	if (dev->clip.x1 < 0) dev->clip.x1 = 0;
	if (dev->clip.x2 > dev->size.x2) dev->clip.x2 = dev->size.x2;
	if (dev->clip.y1 < 0) dev->clip.y1 = 0;
	if (dev->clip.y2 > dev->size.y2) dev->clip.y2 = dev->size.y2;
	if (!is_rect_valid(&dev->clip)) {
		/* Empty region */
		dev->clip.x1 = dev->clip.x2 = dev->clip.y1 = dev->clip.y2 = 0;
	}
	if (drv->set_clip_area)
		drv->set_clip_area(dev);
}

/* memory address r must contain one struct rect
 * x1 is leftmost pixel that is still valid
 * x2 is leftmost pixel that isn't valid any more
 * y1, y2 analogically
 */
int restrict_clip_area(struct graphics_device *dev, struct rect *r, int x1, int y1, int x2, int y2)
{
	struct rect v, rr;
	rr.x1 = x1, rr.x2 = x2, rr.y1 = y1, rr.y2 = y2;
	if (r) memcpy(r, &dev->clip, sizeof(struct rect));
	intersect_rect(&v, &dev->clip, &rr);
	set_clip_area(dev, &v);
	return is_rect_valid(&v);
}

struct rect_set *g_scroll(struct graphics_device *dev, int scx, int scy)
{
	struct rect_set *rs = init_rect_set();

	if (!scx && !scy)
		return rs;

	if (abs(scx) >= dev->clip.x2 - dev->clip.x1
	|| abs(scy) >= dev->clip.y2 - dev->clip.y1) {
		add_to_rect_set(&rs, &dev->clip);
		return rs;
	}

	if (drv->scroll(dev, &rs, scx, scy)) {
		struct rect q = dev->clip;
		if (scy >= 0)
			q.y2 = q.y1 + scy;
		else
			q.y1 = q.y2 + scy;
		add_to_rect_set(&rs, &q);

		q = dev->clip;
		if (scy >= 0)
			q.y1 += scy;
		else
			q.y2 += scy;
		if (scx >= 0)
			q.x2 = q.x1 + scx;
		else
			q.x1 = q.x2 + scx;
		add_to_rect_set(&rs, &q);
	}

	return rs;
}

#endif
