VERSION = 2.16

PREFIX = /
MANPREFIX = $(PREFIX)/share/man

X11INC = /usr/X11R6/include
X11LIB = /usr/X11R6/lib

INCS = -I. -I/usr/include -I/usr/local/include -I$(X11INC)
LIBS = -L$(X11LIB) -L/usr/lib -L/usr/local/lib \
       -lX11 -lcrypto -levent -ljpeg -lm -lpng -lssl -lz

CPPFLAGS = -DVERSION=\"$(VERSION)\" -D_BSD_SOURCE
CFLAGS = -O2 -std=c99 -Wall -pedantic $(CPPFLAGS) \
	-DG=1 -DHAVE_JPEG=1 -DHAVE_PNG_H=1
LDFLAGS = $(LIBS) $(INCS)
