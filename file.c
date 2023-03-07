/* file.c
 * (c) 2002 Mikulas Patocka
 * This file is a part of the Links program, released under GPL.
 */

#include <limits.h>
#include <sys/stat.h>

#include "links.h"

static void
setrwx(unsigned m, unsigned char *p)
{
	if (m & S_IRUSR)
		p[0] = 'r';
	if (m & S_IWUSR)
		p[1] = 'w';
	if (m & S_IXUSR)
		p[2] = 'x';
}

static void
setst(unsigned m, unsigned char *p)
{
#ifdef S_ISUID
	if (m & S_ISUID) {
		p[2] = 'S';
		if (m & S_IXUSR)
			p[2] = 's';
	}
#endif
#ifdef S_ISGID
	if (m & S_ISGID) {
		p[5] = 'S';
		if (m & S_IXGRP)
			p[5] = 's';
	}
#endif
#ifdef S_ISVTX
	if (m & S_ISVTX) {
		p[8] = 'T';
		if (m & S_IXOTH)
			p[8] = 't';
	}
#endif
}

static size_t
stat_mode(unsigned char **p, size_t l, struct stat *stp)
{
	unsigned char c = '?';
	unsigned char rwx[10] = "---------";
	if (stp) {
		if (0) {
		}
#ifdef S_ISBLK
		else if (S_ISBLK(stp->st_mode))
			c = 'b';
#endif
#ifdef S_ISCHR
		else if (S_ISCHR(stp->st_mode))
			c = 'c';
#endif
		else if (S_ISDIR(stp->st_mode))
			c = 'd';
		else if (S_ISREG(stp->st_mode))
			c = '-';
#ifdef S_ISFIFO
		else if (S_ISFIFO(stp->st_mode))
			c = 'p';
#endif
#ifdef S_ISLNK
		else if (S_ISLNK(stp->st_mode))
			c = 'l';
#endif
#ifdef S_ISSOCK
		else if (S_ISSOCK(stp->st_mode))
			c = 's';
#endif
#ifdef S_ISNWK
		else if (S_ISNWK(stp->st_mode))
			c = 'n';
#endif
	}
	l = add_chr_to_str(p, l, c);
	if (stp) {
		unsigned mode = stp->st_mode;
		setrwx(mode << 0, &rwx[0]);
		setrwx(mode << 3, &rwx[3]);
		setrwx(mode << 6, &rwx[6]);
		setst(mode, rwx);
	}
	l = add_to_str(p, l, rwx);
	return add_chr_to_str(p, l, ' ');
}

static size_t
stat_links(unsigned char **p, size_t l, struct stat *stp)
{
	unsigned char lnk[64];
	if (!stp)
		return add_to_str(p, l, cast_uchar "    ");

	sprintf(cast_char lnk, "%3ld ", (unsigned long)stp->st_nlink);
	return add_to_str(p, l, lnk);
}

static int last_uid = -1;
static unsigned char last_user[64];

static int last_gid = -1;
static unsigned char last_group[64];

static size_t
stat_user(unsigned char **p, size_t l, struct stat *stp, int g)
{
	struct passwd *pwd;
	struct group *grp;
	int id;
	unsigned char *pp;
	size_t i;
	if (!stp)
		return add_to_str(p, l, cast_uchar "         ");
	id = !g ? stp->st_uid : stp->st_gid;
	pp = !g ? last_user : last_group;
	if (!g && id == last_uid && last_uid != -1)
		goto a;
	if (g && id == last_gid && last_gid != -1)
		goto a;
	if (!g) {
		ENULLLOOP(pwd, getpwuid(id));
		if (!pwd || !pwd->pw_name)
			sprintf(cast_char pp, "%d", id);
		else
			sprintf(cast_char pp, "%.8s", pwd->pw_name);
		last_uid = id;
	} else {
		ENULLLOOP(grp, getgrgid(id));
		if (!grp || !grp->gr_name)
			sprintf(cast_char pp, "%d", id);
		else
			sprintf(cast_char pp, "%.8s", grp->gr_name);
		last_gid = id;
	}
a:
	l = add_to_str(p, l, pp);
	for (i = strlen(cast_const_char pp); i < 8; i++)
		l = add_chr_to_str(p, l, ' ');
	return add_chr_to_str(p, l, ' ');
}

static size_t
stat_size(unsigned char **p, size_t l, struct stat *stp)
{
	unsigned char num[64];
	const int digits = 8;
	size_t i;
	if (!stp)
		num[0] = 0;
	else
		snzprint(num, sizeof num, stp->st_size);
	for (i = strlen(cast_const_char num); i < digits; i++)
		l = add_chr_to_str(p, l, ' ');
	l = add_to_str(p, l, num);
	return add_chr_to_str(p, l, ' ');
}

static size_t
stat_date(unsigned char **p, size_t l, struct stat *stp)
{
	time_t current_time;
	time_t when;
	struct tm *when_local;
	unsigned char *fmt, *e;
	unsigned char str[13];
	static unsigned char fmt1[] = "%b %e  %Y";
	static unsigned char fmt2[] = "%b %e %H:%M";
	size_t wr;
	EINTRLOOPX(current_time, time(NULL), (time_t)-1);
	if (!stp) {
		wr = 0;
		goto set_empty;
	}
	when = stp->st_mtime;
	when_local = localtime(&when);
	if ((ulonglong)current_time
	        > (ulonglong)when + 6L * 30L * 24L * 60L * 60L
	    || (ulonglong)current_time < (ulonglong)when - 60L * 60L)
		fmt = fmt1;
	else
		fmt = fmt2;
again:
	wr = strftime(cast_char str, 13, cast_const_char fmt, when_local);
	if (wr && strstr(cast_const_char str, " e ")
	    && ((e = cast_uchar strchr(cast_const_char fmt, 'e')))) {
		*e = 'd';
		goto again;
	}
set_empty:
	while (wr < 12)
		str[wr++] = ' ';
	str[12] = 0;
	l = add_to_str(p, l, str);
	return add_chr_to_str(p, l, ' ');
}

static unsigned char *
get_filename(unsigned char *url)
{
	unsigned char *p, *m;
	for (p = url + 7; *p && *p != POST_CHAR; p++)
		;
	m = NULL;
	add_conv_str(&m, 0, url + 7, (int)(p - url - 7), -2);
	return m;
}

struct dirs {
	unsigned char *s;
	unsigned char *f;
};

static int
comp_de(const void *d1_, const void *d2_)
{
	const struct dirs *d1 = (const struct dirs *)d1_;
	const struct dirs *d2 = (const struct dirs *)d2_;
	if (d1->f[0] == '.' && d1->f[1] == '.' && !d1->f[2])
		return -1;
	if (d2->f[0] == '.' && d2->f[1] == '.' && !d2->f[2])
		return 1;
	if (d1->s[0] == 'd' && d2->s[0] != 'd')
		return -1;
	if (d1->s[0] != 'd' && d2->s[0] == 'd')
		return 1;
	return strcmp(cast_const_char d1->f, cast_const_char d2->f);
}

void
file_func(struct connection *c)
{
	struct cache_entry *e;
	unsigned char *file, *name, *head = NULL;
	int fl, flo;
	DIR *d;
	int h, r;
	struct stat stt;
	int rs;
	if (anonymous) {
		setcstate(c, S_BAD_URL);
		abort_connection(c);
		return;
	}
	if (!(name = get_filename(c->url))) {
		setcstate(c, S_OUT_OF_MEM);
		abort_connection(c);
		return;
	}
	EINTRLOOP(rs, stat(cast_const_char name, &stt));
	if (rs) {
		free(name);
		setcstate(c, get_error_from_errno(errno));
		abort_connection(c);
		return;
	}
	if (!S_ISDIR(stt.st_mode) && !S_ISREG(stt.st_mode)) {
		free(name);
		setcstate(c, S_FILE_TYPE);
		abort_connection(c);
		return;
	}
	h = c_open(name, O_RDONLY | O_NOCTTY);
	if (h == -1) {
		int er = errno;
		d = c_opendir(name);
		if (d)
			goto dir;
		free(name);
		setcstate(c, get_error_from_errno(er));
		abort_connection(c);
		return;
	}
	if (S_ISDIR(stt.st_mode)) {
		struct dirs *dir;
		int dirl;
		int i;
		int er;
		struct dirent *de;
		d = c_opendir(name);
		er = errno;
		EINTRLOOP(rs, close(h));
		if (!d) {
			free(name);
			setcstate(c, get_error_from_errno(er));
			abort_connection(c);
			return;
		}
dir:
		dir = NULL;
		dirl = 0;
		if (name[0]
		    && !dir_sep(name[strlen(cast_const_char name) - 1])) {
			if (!c->cache) {
				if (get_connection_cache_entry(c)) {
					free(name);
					closedir(d);
					setcstate(c, S_OUT_OF_MEM);
					abort_connection(c);
					return;
				}
				c->cache->refcount--;
			}
			e = c->cache;
			free(e->redirect);
			e->redirect = stracpy(c->url);
			add_to_strn(&e->redirect, cast_uchar "/");
			free(name);
			closedir(d);
			goto end;
		}
		last_uid = -1;
		last_gid = -1;
		file = NULL;
		fl = add_to_str(&file, 0, cast_uchar "<html><head><title>");
		flo = fl;
		fl = add_conv_str(&file, fl, name,
		                  (int)strlen(cast_const_char name), -1);
		convert_file_charset(&file, &fl, flo);
		fl = add_to_str(&file, fl,
		                cast_uchar
		                "</title></head><body><h2>Directory ");
		flo = fl;
		fl = add_conv_str(&file, fl, name,
		                  (int)strlen(cast_const_char name), -1);
		convert_file_charset(&file, &fl, flo);
		fl = add_to_str(&file, fl, cast_uchar "</h2>\n<pre>");
		while (1) {
			struct stat stt, *stp;
			unsigned char **p;
			int l;
			unsigned char *n;
			ENULLLOOP(de, (void *)readdir(d));
			if (!de)
				break;
			if (!strcmp(cast_const_char de->d_name, "."))
				continue;
			if (!strcmp(cast_const_char de->d_name, "..")) {
				unsigned char *n = name;
				if (strspn(cast_const_char n,
				           dir_sep('\\') ? "/\\" : "/")
				    == strlen(cast_const_char n))
					continue;
			}
			if ((unsigned)dirl > INT_MAX / sizeof(struct dirs) - 1)
				overalloc();
			dir = xrealloc(dir, (dirl + 1) * sizeof(struct dirs));
			dir[dirl].f = stracpy(cast_uchar de->d_name);
			*(p = &dir[dirl++].s) = NULL;
			l = 0;
			n = stracpy(name);
			add_to_strn(&n, cast_uchar de->d_name);
			EINTRLOOP(rs, lstat(cast_const_char n, &stt));
			if (rs)
				stp = NULL;
			else
				stp = &stt;
			free(n);
			l = stat_mode(p, l, stp);
			l = stat_links(p, l, stp);
			l = stat_user(p, l, stp, 0);
			l = stat_user(p, l, stp, 1);
			l = stat_size(p, l, stp);
			l = stat_date(p, l, stp);
		}
		closedir(d);
		if (dirl)
			qsort(dir, dirl, sizeof(struct dirs),
			      (int (*)(const void *, const void *))comp_de);
		for (i = 0; i < dirl; i++) {
			char *lnk = NULL;
			if (dir[i].s[0] == 'l') {
				char *buf = NULL;
				size_t size = 0;
				int r;
				char *n = (char *)stracpy(name);
				add_to_strn((unsigned char **)&n, dir[i].f);
				do {
					free(buf);
					size += ALLOC_GR;
					if (size > INT_MAX)
						overalloc();
					buf = xmalloc(size);
					EINTRLOOP(r, readlink(n, buf, size));
				} while (r == size);
				if (r == -1)
					goto yyy;
				buf[r] = 0;
				lnk = buf;
				goto xxx;
yyy:
				free(buf);
xxx:
				free(n);
			}
			fl = add_to_str(&file, fl, dir[i].s);
			fl = add_to_str(&file, fl, cast_uchar "<a href=\"./");
			fl = add_conv_str(&file, fl, dir[i].f,
			                  (int)strlen(cast_const_char dir[i].f),
			                  1);
			if (dir[i].s[0] == 'd')
				fl = add_chr_to_str(&file, fl, '/');
			else if (lnk) {
				struct stat st;
				unsigned char *n = stracpy(name);
				add_to_strn(&n, dir[i].f);
				EINTRLOOP(rs, stat(cast_const_char n, &st));
				if (!rs && S_ISDIR(st.st_mode))
					fl = add_chr_to_str(&file, fl, '/');
				free(n);
			}
			fl = add_to_str(&file, fl, cast_uchar "\">");
			flo = fl;
			fl = add_conv_str(&file, fl, dir[i].f,
			                  (int)strlen(cast_const_char dir[i].f),
			                  0);
			convert_file_charset(&file, &fl, flo);
			fl = add_to_str(&file, fl, cast_uchar "</a>");
			if (lnk) {
				fl = add_to_str(&file, fl, cast_uchar " -> ");
				fl = add_to_str(&file, fl, cast_uchar lnk);
				free(lnk);
			}
			fl = add_to_str(&file, fl, cast_uchar "\n");
		}
		free(name);
		for (i = 0; i < dirl; i++) {
			free(dir[i].s);
			free(dir[i].f);
		}
		free(dir);
		fl = add_to_str(&file, fl, cast_uchar "</pre></body></html>\n");
		head = stracpy(cast_uchar "\r\nContent-Type: text/html\r\n");
	} else {
		free(name);
		if (stt.st_size < 0 || stt.st_size > INT_MAX) {
			EINTRLOOP(rs, close(h));
			setcstate(c, S_LARGE_FILE);
			abort_connection(c);
			return;
		}
		/* + !stt.st_size is there because of bug in Linux. Read returns
		   -EACCES when reading 0 bytes to invalid address */
		file = xmalloc((size_t)stt.st_size + !stt.st_size);
		if (!file) {
			EINTRLOOP(rs, close(h));
			setcstate(c, S_OUT_OF_MEM);
			abort_connection(c);
			return;
		}
		if ((r = hard_read(h, file, (int)stt.st_size)) != stt.st_size) {
			free(file);
			EINTRLOOP(rs, close(h));
			setcstate(c, r == -1 ? get_error_from_errno(errno)
			                     : S_FILE_ERROR);
			abort_connection(c);
			return;
		}
		fl = r;
		EINTRLOOP(rs, close(h));
		head = stracpy(cast_uchar "");
	}
	if (!c->cache) {
		if (get_connection_cache_entry(c)) {
			free(file);
			free(head);
			setcstate(c, S_OUT_OF_MEM);
			abort_connection(c);
			return;
		}
		c->cache->refcount--;
	}
	e = c->cache;
	free(e->head);
	e->head = head;
	if ((r = add_fragment(e, 0, file, fl)) < 0) {
		free(file);
		setcstate(c, r);
		abort_connection(c);
		return;
	}
	truncate_entry(e, fl, 1);
	free(file);
end:
	c->cache->incomplete = 0;
	setcstate(c, S__OK);
	abort_connection(c);
}
