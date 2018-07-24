#include "links.h"

#include <zlib.h>

#ifdef write
#undef write
#endif

my_uintptr_t decompressed_cache_size = 0;

static int display_error(struct terminal *term, unsigned char *msg, int *errp)
{
	if (errp) *errp = 1;
	if (!term) return 0;
	if (!errp) if (find_msg_box(term, msg, NULL, NULL)) return 0;
	return 1;
}

static void decoder_memory_init(unsigned char **p, size_t *size, off_t init_length)
{
	if (init_length > 0 && init_length < MAXINT) *size = (int)init_length;
	else *size = 4096;
	*p = xmalloc(*size);
}

static int decoder_memory_expand(unsigned char **p, size_t size, size_t *addsize)
{
	unsigned char *pp;
	size_t add = size / 4 + 1;
	if (size + add < size) {
		if (add > 1) add >>= 1;
		else overalloc();
	}
	pp = mem_realloc_mayfail(*p, size + add);
	if (!pp) {
		*addsize = 0;
		return -1;
	}
	*addsize = add;
	*p = pp;
	return 0;
}

static void decompress_error(struct terminal *term, struct cache_entry *ce, unsigned char *lib, unsigned char *msg, int *errp)
{
	unsigned char *u, *server;
	if ((u = parse_http_header(ce->head, cast_uchar "Content-Encoding", NULL))) {
		free(u);
		if ((server = get_host_name(ce->url))) {
			add_blacklist_entry(server, BL_NO_COMPRESSION);
			free(server);
		}
	}
	if (!display_error(term, TEXT_(T_DECOMPRESSION_ERROR), errp)) return;
	u = display_url(term, ce->url, 1);
	msg_box(term, getml(u, NULL), TEXT_(T_DECOMPRESSION_ERROR), AL_CENTER, TEXT_(T_ERROR_DECOMPRESSING_), u, TEXT_(T__wITH_), lib, cast_uchar ": ", msg, MSG_BOX_END, NULL, 1, TEXT_(T_CANCEL), msg_box_null, B_ENTER | B_ESC);
}

static int decode_gzip(struct terminal *term, struct cache_entry *ce, int defl, int *errp)
{
	unsigned char err;
	unsigned char memory_error;
	unsigned char skip_gzip_header;
	unsigned char old_zlib;
	z_stream z;
	off_t offset;
	int r;
	unsigned char *p;
	struct fragment *f;
	struct list_head *lf;
	size_t size;

	retry_after_memory_error:
	memory_error = 0;
	decoder_memory_init(&p, &size, ce->length);
	init_again:
	err = 0;
	skip_gzip_header = 0;
	old_zlib = 0;
	memset(&z, 0, sizeof z);
	z.next_in = NULL;
	z.avail_in = 0;
	z.next_out = p;
	z.avail_out = (unsigned)size;
	z.zalloc = NULL;
	z.zfree = NULL;
	z.opaque = NULL;
	r = inflateInit2(&z, defl == 1 ? 15 : defl == 2 ? -15 : 15 + 16);
	init_failed:
	switch (r) {
		case Z_OK:		break;
		case Z_MEM_ERROR:	memory_error = 1;
					err = 1;
					goto after_inflateend;
		case Z_STREAM_ERROR:
					if (!defl && !old_zlib) {
						if (defrag_entry(ce)) {
							memory_error = 1;
							err = 1;
							goto after_inflateend;
						}
						r = inflateInit2(&z, -15);
						skip_gzip_header = 1;
						old_zlib = 1;
						goto init_failed;
					}
					decompress_error(term, ce, cast_uchar "zlib", z.msg ? (unsigned char *)z.msg : (unsigned char *)"Invalid parameter", errp);
					err = 1;
					goto after_inflateend;
		case Z_VERSION_ERROR:	decompress_error(term, ce, cast_uchar "zlib", z.msg ? (unsigned char *)z.msg : (unsigned char *)"Bad zlib version", errp);
					err = 1;
					goto after_inflateend;
		default:		decompress_error(term, ce, cast_uchar "zlib", z.msg ? (unsigned char *)z.msg : (unsigned char *)"Unknown return value on inflateInit2", errp);
					err = 1;
					goto after_inflateend;
	}
	offset = 0;
	foreach(struct fragment, f, lf, ce->frag) {
		if (f->offset != offset) break;
		z.next_in = f->data;
		z.avail_in = (unsigned)f->length;
		if ((off_t)z.avail_in != f->length) overalloc();
		repeat_frag:
		if (skip_gzip_header == 2) {
			if (z.avail_in < 8) goto finish;
			z.next_in = (unsigned char *)z.next_in + 8;
			z.avail_in -= 8;
			skip_gzip_header = 1;
		}
		if (skip_gzip_header) {
			/* if zlib is old, we have to skip gzip header manually
			   otherwise zlib 1.2.x can do it automatically */
			unsigned char *head = z.next_in;
			unsigned headlen = 10;
			if (z.avail_in <= 11) goto finish;
			if (head[0] != 0x1f || head[1] != 0x8b) {
				decompress_error(term, ce, cast_uchar "zlib", TEXT_(T_COMPRESSED_ERROR), errp);
				err = 1;
				goto finish;
			}
			if (head[2] != 8 || head[3] & 0xe0) {
				decompress_error(term, ce, cast_uchar "zlib", TEXT_(T_UNKNOWN_COMPRESSION_METHOD), errp);
				err = 1;
				goto finish;
			}
			if (head[3] & 0x04) {
				headlen += 2 + head[10] + (head[11] << 8);
				if (headlen >= z.avail_in) goto finish;
			}
			if (head[3] & 0x08) {
				do {
					headlen++;
					if (headlen >= z.avail_in) goto finish;
				} while (head[headlen - 1]);
			}
			if (head[3] & 0x10) {
				do {
					headlen++;
					if (headlen >= z.avail_in) goto finish;
				} while (head[headlen - 1]);
			}
			if (head[3] & 0x01) {
				headlen += 2;
				if (headlen >= z.avail_in) goto finish;
			}
			z.next_in = (unsigned char *)z.next_in + headlen;
			z.avail_in -= headlen;
			skip_gzip_header = 0;
		}
		r = inflate(&z, f->list_entry.next == &ce->frag ? Z_SYNC_FLUSH : Z_NO_FLUSH);
		switch (r) {
			case Z_OK:		break;
			case Z_BUF_ERROR:	break;
			case Z_STREAM_END:	r = inflateEnd(&z);
						if (r != Z_OK) goto end_failed;
						r = inflateInit2(&z, old_zlib ? -15 : defl ? 15 : 15 + 16);
						if (r != Z_OK) {
							old_zlib = 0;
							goto init_failed;
						}
						if (old_zlib) {
							skip_gzip_header = 2;
						}
						break;
			case Z_NEED_DICT:
			case Z_DATA_ERROR:	if (defl == 1) {
							defl = 2;
							r = inflateEnd(&z);
							if (r != Z_OK) goto end_failed;
							goto init_again;
						}
						decompress_error(term, ce, cast_uchar "zlib", z.msg ? (unsigned char *)z.msg : TEXT_(T_COMPRESSED_ERROR), errp);
						err = 1;
						goto finish;
			case Z_STREAM_ERROR:	decompress_error(term, ce, cast_uchar "zlib", z.msg ? (unsigned char *)z.msg : (unsigned char *)"Internal error on inflate", errp);
						err = 1;
						goto finish;
			case Z_MEM_ERROR:
			mem_error:		memory_error = 1;
						err = 1;
						goto finish;
			default:		decompress_error(term, ce, cast_uchar "zlib", z.msg ? (unsigned char *)z.msg : (unsigned char *)"Unknown return value on inflate", errp);
						err = 1;
						break;
		}
		if (!z.avail_out) {
			size_t addsize;
			if (decoder_memory_expand(&p, size, &addsize) < 0)
				goto mem_error;
			z.next_out = p + size;
			z.avail_out = (unsigned)addsize;
			size += addsize;
		}
		if (z.avail_in) goto repeat_frag;
		/* In zlib 1.1.3, inflate(Z_SYNC_FLUSH) doesn't work.
		   The following line fixes it --- for last fragment, loop until
		   we get an eof. */
		if (r == Z_OK && f->list_entry.next == &ce->frag) goto repeat_frag;
		offset += f->length;
	}
	finish:
	r = inflateEnd(&z);
	end_failed:
	switch (r) {
		case Z_OK:		break;
		case Z_STREAM_ERROR:	decompress_error(term, ce, cast_uchar "zlib", z.msg ? (unsigned char *)z.msg : (unsigned char *)"Internal error on inflateEnd", errp);
					err = 1;
					break;
		case Z_MEM_ERROR:	memory_error = 1;
					err = 1;
					break;
		default:		decompress_error(term, ce, cast_uchar "zlib", z.msg ? (unsigned char *)z.msg : (unsigned char *)"Unknown return value on inflateEnd", errp);
					err = 1;
					break;
	}
	after_inflateend:
	if (memory_error) {
		free(p);
		if (out_of_memory(0, NULL, 0))
			goto retry_after_memory_error;
		decompress_error(term, ce, cast_uchar "zlib", z.msg ? (unsigned char *)z.msg : TEXT_(T_OUT_OF_MEMORY), errp);
		return 1;
	}
	if (err && (unsigned char *)z.next_out == p) {
		free(p);
		return 1;
	}
	ce->decompressed = p;
	ce->decompressed_len = (unsigned char *)z.next_out - (unsigned char *)p;
	decompressed_cache_size += ce->decompressed_len;
	ce->decompressed = mem_realloc(ce->decompressed, ce->decompressed_len);
	return 0;
}

int get_file_by_term(struct terminal *term, struct cache_entry *ce, unsigned char **start, unsigned char **end, int *errp)
{
	unsigned char *enc;
	struct fragment *fr;
	int e;
	if (errp) *errp = 0;
	*start = *end = NULL;
	if (!ce) return 1;
	if (ce->decompressed) {
		return_decompressed:
		*start = ce->decompressed;
		*end = ce->decompressed + ce->decompressed_len;
		return 0;
	}
	enc = get_content_encoding(ce->head, ce->url, 0);
	if (enc) {
		if (!casestrcmp(enc, cast_uchar "gzip") || !casestrcmp(enc, cast_uchar "x-gzip") || !casestrcmp(enc, cast_uchar "deflate")) {
			int defl = !casestrcmp(enc, cast_uchar "deflate");
			free(enc);
			if (decode_gzip(term, ce, defl, errp)) goto uncompressed;
			goto return_decompressed;
		}
		free(enc);
		goto uncompressed;
	}
	uncompressed:
	if ((e = defrag_entry(ce)) < 0) {
		unsigned char *msg = get_err_msg(e);
		if (display_error(term, TEXT_(T_ERROR), errp)) {
			unsigned char *u = display_url(term, ce->url, 1);
			msg_box(term, getml(u, NULL), TEXT_(T_ERROR), AL_CENTER, TEXT_(T_ERROR_LOADING), cast_uchar " ", u, cast_uchar ":\n\n", msg, MSG_BOX_END, NULL, 1, TEXT_(T_CANCEL), msg_box_null, B_ENTER | B_ESC);
		}
	}
	if (list_empty(ce->frag)) return 1;
	fr = list_struct(ce->frag.next, struct fragment);
	if (fr->offset || !fr->length) return 1;
	*start = fr->data, *end = fr->data + fr->length;
	return 0;
}

int get_file(struct object_request *o, unsigned char **start, unsigned char **end)
{
	struct terminal *term;
	*start = *end = NULL;
	if (!o) return 1;
	term = find_terminal(o->term);
	return get_file_by_term(term, o->ce, start, end, NULL);
}

void free_decompressed_data(struct cache_entry *e)
{
	if (e->decompressed) {
		if (decompressed_cache_size < e->decompressed_len)
			internal("free_decompressed_data: decompressed_cache_size underflow %lu, %lu", (unsigned long)decompressed_cache_size, (unsigned long)e->decompressed_len);
		decompressed_cache_size -= e->decompressed_len;
		e->decompressed_len = 0;
		free(e->decompressed);
		e->decompressed = NULL;
	}
}

void add_compress_methods(unsigned char **s, int *l)
{
	int cl = 0;
	{
		if (!cl) cl = 1; else add_to_str(s, l, cast_uchar ", ");
		add_to_str(s, l, cast_uchar "ZLIB");
#ifdef zlib_version
		add_to_str(s, l, cast_uchar " (");
		add_to_str(s, l, (unsigned char *)zlib_version);
		add_to_str(s, l, cast_uchar ")");
#endif
	}
}
