/* drivers.c
 * (c) 2002 Mikulas Patocka
 * This file is a part of the Links program, released under GPL
 */

#ifdef G

#include "links.h"

int F = 0;

struct graphics_driver *drv = NULL;

extern struct graphics_driver x_driver;

static struct graphics_driver *graphics_drivers[] = {
	&x_driver,
	NULL
};

int dummy_block(struct graphics_device *dev)
{
	return 0;
}

int dummy_unblock(struct graphics_device *dev)
{
	return 0;
}

/* Driver je jednorazovy argument, kterej se preda grafickymu driveru, nikde se dal
 * neuklada.  Param se skladuje v default_driver param a uklada se do konfiguraku. Pred
 * ukoncenim grafickeho driveru se nastavi default_driver_param podle
 * drv->get_driver_param.
 */
static unsigned char *init_graphics_driver(struct graphics_driver *gd, unsigned char *param, unsigned char *display)
{
	unsigned char *r;
	unsigned char *p = param;
	struct driver_param *dp = get_driver_param(gd->name);
	if (!param || !*param) p = dp->param;
	gd->kbd_codepage = dp->kbd_codepage;
	gd->shell = mem_calloc(MAX_STR_LEN);
	if (dp->shell) safe_strncpy(gd->shell, dp->shell, MAX_STR_LEN);
	drv = gd;
	r = gd->init_driver(p,display);
	if (r) mem_free(gd->shell), gd->shell = NULL, drv = NULL;
	else F = 1;
	return r;
}


void add_graphics_drivers(unsigned char **s, int *l)
{
	struct graphics_driver **gd;
	for (gd = graphics_drivers; *gd; gd++) {
		if (gd != graphics_drivers) add_to_str(s, l, cast_uchar ", ");
		add_to_str(s, l, (*gd)->name);
	}
}

unsigned char *init_graphics(unsigned char *driver, unsigned char *param, unsigned char *display)
{
	unsigned char *s = init_str();
	int l = 0;
	struct graphics_driver **gd;
	for (gd = graphics_drivers; *gd; gd++) {
		if (!driver || !*driver || !casestrcmp((*gd)->name, driver)) {
			unsigned char *r;
			if ((!driver || !*driver) && (*gd)->flags & GD_NOAUTO) continue;
			if (!(r = init_graphics_driver(*gd, param, display))) {
				mem_free(s);
				return NULL;
			}
			if (!l) {
				if (!driver || !*driver) add_to_str(&s, &l, cast_uchar "Could not initialize any graphics driver. Tried the following drivers:\n");
				else add_to_str(&s, &l, cast_uchar "Could not initialize graphics driver ");
			}
			add_to_str(&s, &l, (*gd)->name);
			add_to_str(&s, &l, cast_uchar ":\n");
			add_to_str(&s, &l, r);
			mem_free(r);
		}
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
	if (drv)
	{
		if (drv->shell) mem_free(drv->shell);
		drv->shutdown_driver();
		drv = NULL;
		F = 0;
	}
}

void update_driver_param(void)
{
	if (drv) {
		struct driver_param *dp = get_driver_param(drv->name);
		dp->kbd_codepage = drv->kbd_codepage;
		if (dp->param) mem_free(dp->param);
		dp->param=stracpy(drv->get_driver_param());
		if (dp->shell) mem_free(dp->shell);
		dp->shell = stracpy(drv->shell);
		dp->nosave = 0;
	}
}

int g_kbd_codepage(struct graphics_driver *drv)
{
	if (drv->kbd_codepage >= 0)
		return drv->kbd_codepage;
	return get_default_charset();
}

void generic_set_clip_area(struct graphics_device *dev, struct rect *r)
{
	memcpy(&dev->clip, r, sizeof(struct rect));
	if (dev->clip.x1 < 0) dev->clip.x1 = 0;
	if (dev->clip.x2 > dev->size.x2) dev->clip.x2 = dev->size.x2;
	if (dev->clip.y1 < 0) dev->clip.y1 = 0;
	if (dev->clip.y2 > dev->size.y2) dev->clip.y2 = dev->size.y2;
	if (dev->clip.x1 >= dev->clip.x2 || dev->clip.y1 >= dev->clip.y2) {
		/* Empty region */
		dev->clip.x1 = dev->clip.x2 = dev->clip.y1 = dev->clip.y2 = 0;
	}
}

#endif
