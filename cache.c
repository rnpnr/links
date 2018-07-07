/* cache.c
 * (c) 2002 Mikulas Patocka
 * This file is a part of the Links program, released under GPL.
 */

#include "links.h"

#if defined(HAVE_SEARCH_H) && defined(HAVE_TDELETE) && defined(HAVE_TFIND) && defined(HAVE_TSEARCH)
#define USE_TREE
#endif

static struct list_head cache = {&cache, &cache};

static my_uintptr_t cache_size = 0;

static tcount cache_count = 1;


#ifdef USE_TREE

static void *cache_root = NULL;

static int ce_compare(const void *p1, const void *p2)
{
	const unsigned char *u1 = (const unsigned char *)p1;
	const unsigned char *u2 = (const unsigned char *)p2;
	if (u1 == u2) return 0;
	return strcmp(cast_const_char u1, cast_const_char u2);
}

static void cache_add_to_tree(struct cache_entry *e)
{
	void **p;

	if (!e->url[0]) return;

#if !defined(__GLIBC__)
	if (!cache_root) {
		/*
		 * Some implementations misbehave when deleting the last
		 * element. They leak memory or return NULL from tdelete.
		 * To guard against misbehavior, we insert one static element
		 * and never delete it.
		 * Glibc doesn't have this bug.
		 */
		static unsigned char empty = 0;
retry_static:
		p = tsearch(&empty, &cache_root, ce_compare);
		if (!p) {
			out_of_memory(0, cast_uchar "tsearch static", 0);
			goto retry_static;
		}
		if ((unsigned char *)*p != &empty) internal("cache_add_to_tree: static entry not added: %p, %p", *p, &empty);
	}
#endif

retry:
	p = tsearch(e->url, &cache_root, ce_compare);
	if (!p) {
		out_of_memory(0, cast_uchar "tsearch", 0);
		goto retry;
	}
	if ((unsigned char *)*p != e->url) internal("cache_add_to_tree: url '%s' is already present", e->url);
}

static void cache_delete_from_tree(struct cache_entry *e)
{
	void *p;

	if (!e->url[0]) return;

	p = tdelete(e->url, &cache_root, ce_compare);
	if (!p) internal("cache_delete_from_tree: url '%s' not found", e->url);
}

static struct cache_entry *cache_search_tree(unsigned char *url)
{
	void **p;

	p = tfind(url, &cache_root, ce_compare);
	if (!p) return NULL;
	return get_struct(*p, struct cache_entry, url);
}

#else

static void cache_add_to_tree(struct cache_entry *e) { }
static void cache_delete_from_tree(struct cache_entry *e) { }

static struct cache_entry *cache_search_tree(unsigned char *url)
{
	struct cache_entry *e;
	struct list_head *le;
	foreach(struct cache_entry, e, le, cache)
		if (!strcmp(cast_const_char e->url, cast_const_char url))
			return e;
	return NULL;
}

#endif


my_uintptr_t cache_info(int type)
{
	my_uintptr_t i = 0;
	struct cache_entry *ce;
	struct list_head *lce;
	switch (type) {
		case CI_BYTES:
			return cache_size;
		case CI_FILES:
			return (my_uintptr_t)list_size(&cache);
		case CI_LOCKED:
			foreach(struct cache_entry, ce, lce, cache) i += !!ce->refcount;
			return i;
		case CI_LOADING:
			foreach(struct cache_entry, ce, lce, cache) i += is_entry_used(ce);
			return i;
		default:
			internal("cache_info: bad request");
	}
	return 0;
}

my_uintptr_t decompress_info(int type)
{
	my_uintptr_t i = 0;
	struct cache_entry *ce;
	struct list_head *lce;
	switch (type) {
		case CI_BYTES:
			return decompressed_cache_size;
		case CI_FILES:
			foreach(struct cache_entry, ce, lce, cache) i += !!ce->decompressed;
			return i;
		case CI_LOCKED:
			foreach(struct cache_entry, ce, lce, cache) i += ce->decompressed && ce->refcount;
			return i;
		default:
			internal("compress_info: bad request");
	}
	return 0;
}

int find_in_cache(unsigned char *url, struct cache_entry **f)
{
	struct cache_entry *e;
	url = remove_proxy_prefix(url);
	e = cache_search_tree(url);
	if (e) {
		e->refcount++;
		del_from_list(e);
		add_to_list(cache, e);
		*f = e;
		return 0;
	}
	return -1;
}

static int get_cache_entry(unsigned char *url, struct cache_entry **f)
{
	if (!find_in_cache(url, f)) return 0;
	return new_cache_entry(url, f);
}

int get_connection_cache_entry(struct connection *c)
{
	struct cache_entry *e;
	if (get_cache_entry(c->url, &c->cache))
		return -1;
	e = c->cache;
	if (e->ip_address) mem_free(e->ip_address), e->ip_address = NULL;
	if (!*c->socks_proxy && !is_proxy_url(c->url) && c->last_lookup_state.addr.n) {
		unsigned char *a;
		unsigned char *s = init_str();
		int l = 0;
		a = print_address(&c->last_lookup_state.addr.a[c->last_lookup_state.addr_index]);
		if (a)
			add_to_str(&s, &l, a);
		if (c->last_lookup_state.addr.n > 1) {
			int i, d = 0;
			if (l) add_to_str(&s, &l, cast_uchar " ");
			add_to_str(&s, &l, cast_uchar "(");
			for (i = 0; i < c->last_lookup_state.addr.n; i++) {
				if (i == c->last_lookup_state.addr_index)
					continue;
				a = print_address(&c->last_lookup_state.addr.a[i]);
				if (a) {
					if (d)
						add_to_str(&s, &l, cast_uchar ", ");
					add_to_str(&s, &l, a);
					d = 1;
				}
			}
			add_to_str(&s, &l, cast_uchar ")");
		}
		e->ip_address = s;
	}
	return 0;
}

int new_cache_entry(unsigned char *url, struct cache_entry **f)
{
	struct cache_entry *e;
	shrink_memory(SH_CHECK_QUOTA, 0);
	url = remove_proxy_prefix(url);
	e = mem_calloc_mayfail(sizeof(struct cache_entry) + strlen(cast_const_char url));
	if (!e)
		return -1;
	strcpy(cast_char e->url, cast_const_char url);
	e->length = 0;
	e->incomplete = 1;
	e->data_size = 0;
	e->http_code = -1;
	init_list(e->frag);
	e->count = cache_count++;
	e->count2 = cache_count++;
	e->refcount = 1;
	e->decompressed = NULL;
	e->decompressed_len = 0;
	cache_add_to_tree(e);
	add_to_list(cache, e);
	*f = e;
	return 0;
}

void detach_cache_entry(struct cache_entry *e)
{
	cache_delete_from_tree(e);
	e->url[0] = 0;
}

static void mem_free_fragment(struct fragment *f)
{
	size_t s = (size_t)f->length;
	mem_free(f);
	s += sizeof(struct fragment);
	mem_freed_large(s);
}

#define sf(x) e->data_size += (x), cache_size += (my_uintptr_t)(x)

int page_size = 4096;

#define C_ALIGN(x) ((((x) + sizeof(struct fragment) + alloc_overhead) | (page_size - 1)) - sizeof(struct fragment) - alloc_overhead)

int add_fragment(struct cache_entry *e, off_t offset, const unsigned char *data, off_t length)
{
	struct fragment *f;
	struct list_head *lf;
	struct fragment *nf;
	int trunc = 0;
	icc_volatile off_t ca;
	if (!length) return 0;
	free_decompressed_data(e);
	e->incomplete = 1;
	if ((off_t)(0UL + offset + length) < 0 || (off_t)(0UL + offset + length) < offset) return S_LARGE_FILE;
	if ((off_t)(0UL + offset + (off_t)C_ALIGN(length)) < 0 || (off_t)(0UL + offset + (off_t)C_ALIGN(length)) < offset) return S_LARGE_FILE;
	if (e->length < offset + length) e->length = offset + length;
	e->count = cache_count++;
	if (list_empty(e->frag)) e->count2 = cache_count++;
	else {
		f = list_struct(e->frag.prev, struct fragment);
		if (f->offset + f->length != offset) e->count2 = cache_count++;
		else {
			lf = &f->list_entry;
			goto have_f;
		}
	}
	foreach(struct fragment, f, lf, e->frag) {
have_f:
		if (f->offset > offset) break;
		if (f->offset <= offset && f->offset + f->length >= offset) {
			if (offset+length > f->offset + f->length) {
				if (memcmp(f->data + offset - f->offset, data, (size_t)(f->offset + f->length - offset))) trunc = 1;
				if (offset - f->offset + length <= f->real_length) {
					sf((offset + length) - (f->offset + f->length));
					f->length = offset - f->offset + length;
				} else {
					sf(-(f->offset + f->length - offset));
					f->length = offset - f->offset;
					lf = f->list_entry.next;
					break;
				}
			} else {
				if (memcmp(f->data + offset-f->offset, data, (size_t)length)) trunc = 1;
			}
			memcpy(f->data+offset - f->offset, data, (size_t)length);
			goto ch_o;
		}
	}
/* Intel C 9 has a bug and miscompiles this statement (< 0 test is true) */
	/*if (C_ALIGN(length) > MAXINT - sizeof(struct fragment) || C_ALIGN(length) < 0) overalloc();*/
	ca = C_ALIGN(length);
	if (ca > MAXINT - (int)sizeof(struct fragment) || ca < 0) return S_LARGE_FILE;
	nf = mem_alloc_mayfail(sizeof(struct fragment) + (size_t)ca);
	if (!nf) return S_OUT_OF_MEM;
	sf(length);
	nf->offset = offset;
	nf->length = length;
	nf->real_length = C_ALIGN(length);
	memcpy(nf->data, data, (size_t)length);
	add_before_list_entry(lf, &nf->list_entry);
	f = nf;
	ch_o:
	while (f->list_entry.next != &e->frag && f->offset + f->length > list_struct(f->list_entry.next, struct fragment)->offset) {
		struct fragment *next = list_struct(f->list_entry.next, struct fragment);
		if (f->offset + f->length < next->offset + next->length) {
			f = mem_realloc(f, sizeof(struct fragment) + (size_t)(next->offset - f->offset + next->length));
			fix_list_after_realloc(f);
			if (memcmp(f->data + next->offset - f->offset, next->data, (size_t)(f->offset + f->length - next->offset))) trunc = 1;
			memcpy(f->data + f->length, next->data + f->offset + f->length - next->offset, (size_t)(next->offset + next->length - f->offset - f->length));
			sf(next->offset + next->length - f->offset - f->length);
			f->length = f->real_length = next->offset + next->length - f->offset;
		} else {
			if (memcmp(f->data + next->offset - f->offset, next->data, (size_t)next->length)) trunc = 1;
		}
		del_from_list(next);
		sf(-next->length);
		mem_free_fragment(next);
	}
	if (trunc) truncate_entry(e, offset + length, 0);
	if (e->length > e->max_length) {
		e->max_length = e->length;
		return 1;
	}
	return 0;
}

int defrag_entry(struct cache_entry *e)
{
	struct fragment *f, *n;
	struct list_head *g, *h;
	off_t l;
	if (list_empty(e->frag)) {
		return 0;
	}
	f = list_struct(e->frag.next, struct fragment);
	if (f->offset) {
		return 0;
	}
	for (g = f->list_entry.next;
		g != &e->frag &&
		list_struct(g, struct fragment)->offset <= list_struct(g->prev, struct fragment)->offset + list_struct(g->prev, struct fragment)->length; g = g->next)
			if (list_struct(g, struct fragment)->offset < list_struct(g->prev, struct fragment)->offset + list_struct(g->prev, struct fragment)->length) {
				internal("fragments overlay");
				return S_INTERNAL;
			}
	if (g == f->list_entry.next) {
		if (f->length != f->real_length) {
			f = mem_realloc_mayfail(f, sizeof(struct fragment) + (size_t)f->length);
			if (f) {
				f->real_length = f->length;
				fix_list_after_realloc(f);
			}
		}
		return 0;
	}
	for (l = 0, h = &f->list_entry; h != g; h = h->next) {
		if ((off_t)(0UL + l + list_struct(h, struct fragment)->length) < 0 || (off_t)(0UL + l + list_struct(h, struct fragment)->length) < l) return S_LARGE_FILE;
		l += list_struct(h, struct fragment)->length;
	}
	if (l > MAXINT - (int)sizeof(struct fragment)) return S_LARGE_FILE;
	n = mem_alloc_mayfail(sizeof(struct fragment) + (size_t)l);
	if (!n) return S_OUT_OF_MEM;
	n->offset = 0;
	n->length = l;
	n->real_length = l;
	for (l = 0, h = &f->list_entry; h != g; h = h->next) {
		struct fragment *hf = list_struct(h, struct fragment);
		memcpy(n->data + l, hf->data, (size_t)hf->length);
		l += hf->length;
		h = h->prev;
		del_from_list(hf);
		mem_free_fragment(hf);
	}
	add_to_list(e->frag, n);
	return 0;
}

void truncate_entry(struct cache_entry *e, off_t off, int final)
{
	int modified = final == 2;
	struct fragment *f, *g;
	struct list_head *lf;
	if (e->length > off) e->length = off, e->incomplete = 1;
	foreach(struct fragment, f, lf, e->frag) {
		if (f->offset >= off) {
			modified = 1;
			sf(-f->length);
			lf = lf->prev;
			del_from_list(f);
			mem_free_fragment(f);
			continue;
		}
		if (f->offset + f->length > off) {
			modified = 1;
			sf(-(f->offset + f->length - off));
			f->length = off - f->offset;
			if (final) {
				g = mem_realloc_mayfail(f, sizeof(struct fragment) + (size_t)f->length);
				if (g) {
					f = g;
					fix_list_after_realloc(f);
					f->real_length = f->length;
					lf = &f->list_entry;
				}
			}
		}
	}
	if (modified) {
		free_decompressed_data(e);
		e->count = cache_count++;
		e->count2 = cache_count++;
	}
}

void free_entry_to(struct cache_entry *e, off_t off)
{
	struct fragment *f;
	struct list_head *lf;
	e->incomplete = 1;
	free_decompressed_data(e);
	foreach(struct fragment, f, lf, e->frag) {
		if (f->offset + f->length <= off) {
			sf(-f->length);
			lf = lf->prev;
			del_from_list(f);
			mem_free_fragment(f);
		} else if (f->offset < off) {
			sf(f->offset - off);
			memmove(f->data, f->data + (off - f->offset), (size_t)(f->length -= off - f->offset));
			f->offset = off;
		} else break;
	}
}

void delete_entry_content(struct cache_entry *e)
{
	truncate_entry(e, 0, 2);
	if (e->last_modified) {
		mem_free(e->last_modified);
		e->last_modified = NULL;
	}
}

void trim_cache_entry(struct cache_entry *e)
{
	struct fragment *f, *nf;
	struct list_head *lf;
	foreach(struct fragment, f, lf, e->frag) {
		if (f->length != f->real_length) {
			nf = mem_realloc_mayfail(f, sizeof(struct fragment) + (size_t)f->length);
			if (nf) {
				f = nf;
				fix_list_after_realloc(f);
				f->real_length = f->length;
				lf = &f->list_entry;
			}
		}
	}
}

void delete_cache_entry(struct cache_entry *e)
{
	if (e->refcount) internal("deleting locked cache entry");
#ifdef DEBUG
	if (is_entry_used(e)) internal("deleting loading cache entry");
#endif
	cache_delete_from_tree(e);
	delete_entry_content(e);
	del_from_list(e);
	if (e->head) mem_free(e->head);
	if (e->redirect) mem_free(e->redirect);
	if (e->ip_address) mem_free(e->ip_address);
#ifdef HAVE_SSL
	if (e->ssl_info) mem_free(e->ssl_info);
#endif
	mem_free(e);
}

static int shrink_file_cache(int u)
{
	int r = 0;
	struct cache_entry *e, *f;
	struct list_head *le, *lf;
	my_uintptr_t ncs = cache_size;
	my_uintptr_t ccs = 0, ccs2 = 0;

	if (u == SH_CHECK_QUOTA && cache_size + decompressed_cache_size <= (my_uintptr_t)memory_cache_size) goto ret;
	foreachback(struct cache_entry, e, le, cache) {
		if (e->refcount || is_entry_used(e)) {
			if (ncs < (my_uintptr_t)e->data_size) {
				internal("cache_size underflow: %lu, %lu", (unsigned long)ncs, (unsigned long)e->data_size);
			}
			ncs -= (my_uintptr_t)e->data_size;
		} else if (u == SH_FREE_SOMETHING) {
			if (e->decompressed_len) free_decompressed_data(e);
			else delete_cache_entry(e);
			r |= ST_SOMETHING_FREED;
			goto ret;
		}
		if (!e->refcount && e->decompressed_len && cache_size + decompressed_cache_size > (my_uintptr_t)memory_cache_size) {
			free_decompressed_data(e);
			r |= ST_SOMETHING_FREED;
		}
		ccs += (my_uintptr_t)e->data_size;
		ccs2 += e->decompressed_len;
	}
	if (ccs != cache_size) internal("cache size badly computed: %lu != %lu", (unsigned long)cache_size, (unsigned long)ccs), cache_size = ccs;
	if (ccs2 != decompressed_cache_size) internal("decompressed cache size badly computed: %lu != %lu", (unsigned long)decompressed_cache_size, (unsigned long)ccs2), decompressed_cache_size = ccs2;
	if (u == SH_CHECK_QUOTA && ncs <= (my_uintptr_t)memory_cache_size) goto ret;
	foreachback(struct cache_entry, e, le, cache) {
		if (u == SH_CHECK_QUOTA && (longlong)ncs <= (longlong)memory_cache_size * MEMORY_CACHE_GC_PERCENT) goto g;
		if (e->refcount || is_entry_used(e)) {
			e->tgc = 0;
			continue;
		}
		e->tgc = 1;
		if (ncs < (my_uintptr_t)e->data_size) {
			internal("cache_size underflow: %lu, %lu", (unsigned long)ncs, (unsigned long)e->data_size);
		}
		ncs -= (my_uintptr_t)e->data_size;
	}
	if (ncs) internal("cache_size(%lu) is larger than size of all objects(%lu)", (unsigned long)cache_size, (unsigned long)(cache_size - ncs));
	g:
	if (le->next == &cache) goto ret;
	le = le->next;
	if (u == SH_CHECK_QUOTA) {
		foreachfrom(struct cache_entry, f, lf, cache, le) {
			if (f->data_size && (longlong)ncs + f->data_size <= (longlong)memory_cache_size * MEMORY_CACHE_GC_PERCENT) {
				ncs += (my_uintptr_t)f->data_size;
				f->tgc = 0;
			}
		}
	}
	foreachfrom(struct cache_entry, f, lf, cache, le) {
		if (f->tgc) {
			lf = lf->prev;
			delete_cache_entry(f);
			r |= ST_SOMETHING_FREED;
		}
	}
ret:
	return r | (list_empty(cache) ? ST_CACHE_EMPTY : 0);
}

void init_cache(void)
{
#ifdef HAVE_GETPAGESIZE
	int getpg;
	EINTRLOOP(getpg, getpagesize());
	if (getpg > 0 && getpg < 0x10000 && !(getpg & (getpg - 1))) page_size = getpg;
#endif
	register_cache_upcall(shrink_file_cache, 0, cast_uchar "file");
}
