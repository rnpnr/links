# links - lynx-like alternative character mode WWW browser
.POSIX:

include config.mk

SRC = \
	auth.c\
	bfu.c\
	bookmark.c\
	cache.c\
	charsets.c\
	compress.c\
	connect.c\
	cookies.c\
	data.c\
	default.c\
	dns.c\
	error.c\
	file.c\
	html.c\
	html_r.c\
	html_tbl.c\
	http.c\
	https.c\
	kbd.c\
	language.c\
	listedit.c\
	main.c\
	memory.c\
	menu.c\
	objreq.c\
	os_dep.c\
	sched.c\
	select.c\
	session.c\
	string.c\
	suffix.c\
	terminal.c\
	types.c\
	url.c\
	view.c
OBJ = $(SRC:.c=.o)

all: options links

options:
	@echo "CFLAGS  = $(CFLAGS)"
	@echo "LDFLAGS = $(LDFLAGS)"
	@echo "CC      = $(CC)"

.c.o:
	$(CC) $(CFLAGS) $(INCS) $(CPPFLAGS) -c $<

config.h:
	cp config.def.h $@

$(OBJ): config.h config.mk

links: $(OBJ)
	$(CC) -o $@ $(OBJ) $(LDFLAGS)

clean:
	rm -f *.o links

install: all
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	cp -f links $(DESTDIR)$(PREFIX)/bin
	chmod 755 $(DESTDIR)$(PREFIX)/bin/links
	mkdir -p $(DESTDIR)$(MANPREFIX)/man1
	sed s:VERSION:$(VERSION): <links.1 >$(DESTDIR)$(MANPREFIX)/man1/links.1
	chmod 644 $(DESTDIR)$(MANPREFIX)/man1/links.1

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/links
	rm -f $(DESTDIR)$(MANPREFIX)/man1/links.1

.PHONY: all options clean install uninstall
