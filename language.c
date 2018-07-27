/* language.c
 * (c) 2002 Mikulas Patocka
 * This file is a part of the Links program, released under GPL.
 */

#include "links.h"

struct translation {
	int code;
	const char *name;
};

struct translation_desc {
	const struct translation *t;
};

unsigned char dummyarray[T__N_TEXTS];

#include "language.inc"

static unsigned char **translation_array[N_CODEPAGES];

static int current_lang_charset;

void init_trans(void)
{
	int j;
	for (j = 0; j < N_CODEPAGES; j++)
		translation_array[j] = NULL;
	set_language();
}

void shutdown_trans(void)
{
	int j, k;
	for (j = 0; j < N_CODEPAGES; j++) if (translation_array[j]) {
		for (k = 0; k < T__N_TEXTS; k++) {
			unsigned char *txt = translation_array[j][k];
			if (txt &&
			    txt != cast_uchar translation_english[k].name)
				free(txt);
		}
		free(translation_array[j]);
	}
}

int get_language_from_lang(unsigned char *lang)
{
	unsigned char *p;
	lang = stracpy(lang);
	lang[strcspn(cast_const_char lang, ".@")] = 0;
	if (!casestrcmp(lang, cast_uchar "nn_NO"))
		strcpy(cast_char lang, "no");
	for (p = lang; *p; p++) {
		if (*p >= 'A' && *p <= 'Z')
			*p += 'a' - 'A';
		if (*p == '_')
			*p = '-';
	}
search_again:
	p = cast_uchar translations[0].t[T__ACCEPT_LANGUAGE].name;
	p = stracpy(p);
	p[strcspn(cast_const_char p, ",;")] = 0;
	if (!casestrcmp(lang, p)) {
		free(p);
		free(lang);
		return 0;
	}
	free(p);

	if ((p = cast_uchar strchr(cast_const_char lang, '-'))) {
		*p = 0;
		goto search_again;
	}
	free(lang);
	return -1;
}

int get_default_charset(void)
{
	unsigned char *lang, *p;
	int i;

	lang = cast_uchar getenv("LC_CTYPE");
	if (!lang)
		lang = cast_uchar getenv("LANG");
	if (!lang) {
		return 0;
	}
	if ((p = cast_uchar strchr(cast_const_char lang, '.'))) {
		p++;
	} else {
		if (strlen(cast_const_char lang) > 5 && !casestrcmp(cast_uchar (strchr(cast_const_char lang, 0) - 5), cast_uchar "@euro")) {
			p = cast_uchar "ISO-8859-15";
		} else {
			p = cast_uchar translations[0].t[T__DEFAULT_CHAR_SET].name;
			if (!p)
				p = cast_uchar "";
		}
	}
	i = get_cp_index(p);

	if (i < 0)
		return 0;

	return i;
}

int get_commandline_charset(void)
{
	return dump_codepage == -1 ? get_default_charset() : dump_codepage;
}

static inline int is_direct_text(unsigned char *text)
{
/* Do not compare to dummyarray directly - thwart some misoptimizations */
	unsigned char * volatile dm = dummyarray;
	return !((my_uintptr_t)text >= (my_uintptr_t)dm && (my_uintptr_t)text < (my_uintptr_t)(dm + T__N_TEXTS));
}

unsigned char *get_text_translation(unsigned char *text, struct terminal *term)
{
	unsigned char **current_tra;
	unsigned char *trn;
	int charset;
	if (!term)
		charset = 0;
	else if (term->spec)
		charset = term_charset(term);
	else
		charset = utf8_table;
	if (is_direct_text(text))
		return text;
	if ((current_tra = translation_array[charset])) {
		unsigned char *tt;
		if ((trn = current_tra[text - dummyarray]))
			return trn;
		tr:
		if (!(tt = cast_uchar translations[0].t[text - dummyarray].name))
			trn = cast_uchar translation_english[text - dummyarray].name;
		else {
			struct document_options l_opt;
			memset(&l_opt, 0, sizeof(l_opt));
			l_opt.plain = 0;
			l_opt.cp = charset;
			trn = convert(current_lang_charset, charset, tt, &l_opt);
			if (!strcmp(cast_const_char trn, cast_const_char tt)) {
				free(trn);
				trn = tt;
			}
		}
		current_tra[text - dummyarray] = trn;
	} else {
		if (current_lang_charset && charset != current_lang_charset) {
			current_tra = translation_array[charset] = xmalloc(sizeof (unsigned char *) * T__N_TEXTS);
			memset(current_tra, 0, sizeof (unsigned char *) * T__N_TEXTS);
			goto tr;
		}
		if (!(trn = cast_uchar translations[0].t[text - dummyarray].name)) {
			trn = cast_uchar translation_english[text - dummyarray].name;
		}
	}
	return trn;
}

unsigned char *get_english_translation(unsigned char *text)
{
	if (is_direct_text(text))
		return text;
	return cast_uchar translation_english[text - dummyarray].name;
}

void set_language(void)
{
	int i;
	unsigned char *cp;
	for (i = 0; i < T__N_TEXTS; i++)
		if (translations[0].t[i].code != i) {
			internal("Bad table for language %s. Run script synclang.",
					translations[0].t[T__LANGUAGE].name);
			return;
		}
	cp = cast_uchar translations[0].t[T__CHAR_SET].name;
	i = get_cp_index(cp);
	if (i == -1) {
		internal("Unknown charset for language %s.", translations[0].t[T__LANGUAGE].name);
		i = 0;
	}
	current_lang_charset = i;
}
