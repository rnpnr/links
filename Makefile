include config.mk

SRC = \
	af_unix.c\
	auth.c\
	bfu.c\
	block.c\
	bookmark.c\
	cache.c\
	charsets.c\
	compress.c\
	connect.c\
	cookies.c\
	data.c\
	default.c\
	dns.c\
	drivers.c\
	error.c\
	file.c\
	finger.c\
	fn_impl.c\
	ftp.c\
	html.c\
	html_gr.c\
	html_r.c\
	html_tbl.c\
	http.c\
	https.c\
	img.c\
	jsint.c\
	kbd.c\
	language.c\
	listedit.c\
	lru.c\
	mailto.c\
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

XSRC = \
	dip.c\
	dither.c\
	font_inc.c\
	gif.c\
	imgcache.c\
	jpeg.c\
	png.c\
	view_gr.c\
	x.c\
	xbm.c
XOBJ = $(XSRC:.c=.o)

all: linksg

links: $(OBJ)
	$(CC) -o $@ $(OBJ) $(LDFLAGS)

linksg: $(OBJ) $(XOBJ)
	$(CC) -o links $(OBJ) $(XOBJ) $(LDFLAGS)

clean:
	rm -f *.o links linksg
