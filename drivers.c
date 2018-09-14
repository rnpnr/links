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
	x_driver.kbd_codepage = dp->kbd_codepage;
	x_driver.shell = mem_calloc(MAX_STR_LEN);
	if (dp->shell)
		safe_strncpy(x_driver.shell, dp->shell, MAX_STR_LEN);
	drv = &x_driver;
	if ((r = x_driver.init_driver(p, display))) {
		free(x_driver.shell);
		x_driver.shell = NULL;
		drv = NULL;
	} else
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
		free(drv->shell);
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
		free(dp->param);
		dp->param = stracpy(drv->get_driver_param());
		free(dp->shell);
		dp->shell = stracpy(drv->shell);
		dp->nosave = 0;
	}
}

int g_kbd_codepage(struct graphics_driver *drv)
{
	if (drv->kbd_codepage >= 0)
		return drv->kbd_codepage;
	return 0;
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
