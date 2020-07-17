VERSION = 2.16

PREFIX = /usr/local
MANPREFIX = $(PREFIX)/share/man

INCS = -I. -I/usr/include -I/usr/local/include
LIBS = -L/usr/lib -L/usr/local/lib \
       -lcrypto -levent -lm -lssl -lz

CPPFLAGS = -DVERSION=\"$(VERSION)\" -D_BSD_SOURCE
CFLAGS = -O2 -std=c99 -Wall -pedantic $(CPPFLAGS)
LDFLAGS = $(LIBS) $(INCS)
