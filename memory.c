/* memory.c
 * (c) 2002 Mikulas Patocka
 * This file is a part of the Links program, released under GPL.
 */

#include "links.h"

struct cache_upcall {
	list_entry_1st
	int (*upcall)(int);
	unsigned char flags;
	list_entry_last
	unsigned char name[1];
};

static struct list_head cache_upcalls = { &cache_upcalls, &cache_upcalls }; /* cache_upcall */

int shrink_memory(int type)
{
	struct cache_upcall *c;
	struct list_head *lc;
	int a = 0;
	foreach(struct cache_upcall, c, lc, cache_upcalls) {
		a |= c->upcall(type);
	}
	return a;
}

void register_cache_upcall(int (*upcall)(int), int flags, unsigned char *name)
{
	struct cache_upcall *c;
	c = xmalloc(sizeof(struct cache_upcall) + strlen(cast_const_char name));
	c->upcall = upcall;
	c->flags = (unsigned char)flags;
	strcpy(cast_char c->name, cast_const_char name);
	add_to_list(cache_upcalls, c);
}

void free_all_caches(void)
{
	struct cache_upcall *c;
	struct list_head *lc;
	int a, b;
	do {
		a = 0;
		b = ~0;
		foreach(struct cache_upcall, c, lc, cache_upcalls) {
			int x = c->upcall(SH_FREE_ALL);
			a |= x;
			b &= x;
		}
	} while (a & ST_SOMETHING_FREED);
	if (!(b & ST_CACHE_EMPTY)) {
		unsigned char *m = init_str();
		int l = 0;
		foreach(struct cache_upcall, c, lc, cache_upcalls) if (!(c->upcall(SH_FREE_ALL) & ST_CACHE_EMPTY)) {
			if (l) add_to_str(&m, &l, cast_uchar ", ");
			add_to_str(&m, &l, c->name);
		}
		internal("could not release entries from caches: %s", m);
		free(m);
	}
	free_list(struct cache_upcall, cache_upcalls);
}

int out_of_memory(void)
{
	if (shrink_memory(SH_FREE_SOMETHING))
		return 1;

	return 0;
}
