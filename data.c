#include "links.h"

static void
base64_decode(unsigned char **d, int *dl, unsigned char *s, int sl)
{
	int bits = 0;
	unsigned tmp = 0;
	for (; sl > 0; s++, sl--) {
		unsigned char val;
		unsigned char c = *s;
		if (c >= 'A' && c <= 'Z')
			val = c - 'A';
		else if (c >= 'a' && c <= 'z')
			val = c - 'a' + 26;
		else if (c >= '0' && c <= '9')
			val = c - '0' + 52;
		else if (c == '+')
			val = 62;
		else if (c == '/')
			val = 63;
		else
			continue;
		tmp <<= 6;
		tmp |= val;
		bits += 6;
		if (bits >= 8) {
			bits -= 8;
			*dl = add_chr_to_str(d, *dl, tmp >> bits);
			tmp &= (1 << bits) - 1;
		}
	}
}

void
data_func(struct connection *c)
{
	unsigned char *data, *flags, *mime, *str;
	size_t length, strl;
	struct cache_entry *e;
	int r;

	int base64 = 0;
	int was_charset = 0;

	flags = cast_uchar strchr(cast_const_char c->url, ':');
	if (!flags) {
bad_url:
		setcstate(c, S_BAD_URL);
		abort_connection(c);
		return;
	}
	flags++;
	while (*flags == '/')
		flags++;

	length = strcspn(cast_const_char flags, ";,");
	mime = memacpy(flags, length);

	while (*(flags += length) == ';') {
		unsigned char *arg;
		flags++;
		length = strcspn(cast_const_char flags, ";,");
		arg = memacpy(flags, length);
		if (!casestrcmp(arg, cast_uchar "base64")) {
			base64 = 1;
		} else if (!casecmp(arg, cast_uchar "charset=", 8)) {
			if (!was_charset) {
				add_to_strn(&mime, cast_uchar ";");
				add_to_strn(&mime, arg);
				was_charset = 1;
			}
		}
		free(arg);
	}

	if (*flags != ',') {
		free(mime);
		goto bad_url;
	}
	data = flags + 1;

	if (!c->cache) {
		if (get_connection_cache_entry(c)) {
			free(mime);
			setcstate(c, S_OUT_OF_MEM);
			abort_connection(c);
			return;
		}
		c->cache->refcount--;
	}
	e = c->cache;
	free(e->head);
	e->head = stracpy(cast_uchar "");
	if (*mime) {
		add_to_strn(&e->head, cast_uchar "\r\nContent-type: ");
		add_to_strn(&e->head, mime);
		add_to_strn(&e->head, cast_uchar "\r\n");
	}
	free(mime);

	str = NULL;

	strl =
	    add_conv_str(&str, 0, data, (int)strlen(cast_const_char data), -2);

	if (!base64) {
		r = add_fragment(e, 0, str, strl);
	} else {
		unsigned char *b64 = NULL;
		int b64l = 0;

		base64_decode(&b64, &b64l, str, strl);

		r = add_fragment(e, 0, b64, b64l);
		free(b64);
	}
	free(str);
	if (r < 0) {
		setcstate(c, r);
		abort_connection(c);
		return;
	}
	truncate_entry(e, strl, 1);
	c->cache->incomplete = 0;

	setcstate(c, S__OK);
	abort_connection(c);
}
