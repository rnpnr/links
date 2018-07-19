/* lru.c
 * LRU cache
 * (c) 2002 Karel 'Clock' Kulhavy
 * This file is a part of the Links program, released under GPL.
 */

#ifdef G

#include "links.h"

static inline void row_insert(struct lru_entry **row, struct lru_entry *ptr)
{
	ptr->next = *row;
	if (ptr->next) ptr->next->previous = &ptr->next;
	ptr->previous = row;
	*row = ptr;
}

static inline void row_delete(struct lru_entry *ptr)
{
	if (ptr->next) ptr->next->previous = ptr->previous;
	*ptr->previous = ptr->next;
}

void lru_insert(struct lru *cache, void *entry, struct lru_entry **row, unsigned bytes_consumed)
{
	struct lru_entry *new_entry = mem_alloc(sizeof(*new_entry));

	new_entry->above = NULL;
	new_entry->below = cache->top;
	new_entry->data = entry;
	new_entry->bytes_consumed = bytes_consumed;
	if (new_entry->below)
		new_entry->below->above = new_entry;
	else
		cache->bottom = new_entry;
	row_insert(row, new_entry);
	cache->top = new_entry;
	cache->bytes += bytes_consumed;
	cache->items++;
}

/* Returns bottom (or NULL if the cache is empty) but doesn't
 * unlink it.
 */
void *lru_get_bottom(struct lru *cache)
{
	if (!cache->bottom) return NULL;
	return cache->bottom->data;
}

/* Destroys bottom on nonempty cache. If the cache is empty, segmentation
 * fault results.
 */
void lru_destroy_bottom(struct lru *cache)
{
	struct lru_entry *it = cache->bottom;

	cache->bytes -= cache->bottom->bytes_consumed;
	cache->items--;
	cache->bottom = it->above;
	if (cache->bottom)
		cache->bottom->below = NULL;
	else
		cache->top = NULL;

	row_delete(it);
	mem_free(it);
}

/* Returns a value of "data"
 * template is what we search for.
 */
void *lru_lookup(struct lru *cache, void *templat, struct lru_entry **row)
{
	struct lru_entry *ptr = *row;
	while (ptr) {
		if (!cache->compare_function(ptr->data,templat)) {
			/* Found */
			if (ptr->above) {
				if (ptr->below) {
					ptr->below->above = ptr->above;
				} else {
					cache->bottom = ptr->above;
				}
				ptr->above->below = ptr->below;
				ptr->above = NULL;
				ptr->below = cache->top;
				cache->top->above = ptr;
				cache->top = ptr;
			}
			if (ptr != *row) {
				row_delete(ptr);
				row_insert(row, ptr);
			}
			return ptr->data;
		}
		ptr = ptr->next;
	}
	return NULL;
}

void lru_init(struct lru *cache, int (*compare_function)(void *entry, void *templat))
{
	cache->compare_function = compare_function;
	cache->top = NULL;
	cache->bottom = NULL;
	cache->bytes = 0;
	cache->items = 0;
}

#endif
