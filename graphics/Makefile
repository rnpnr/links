CFLAGS=-g -Wall -O2
#-Wno-pointer-sign
LDFLAGS=

all: improcess genps pbm2png clip generate_font arrow

clean:
	rm -f improcess genps core pbm2png clip generate_font arrow

improcess: improcess.c
	gcc -o improcess improcess.c $(CFLAGS) $(LDFLAFGS) -lpng -lz -lm

pbm2png: pbm2png.c
	gcc -o pbm2png pbm2png.c $(CFLAGS) $(LDFLAGS) -lpng -lz -lm

genps: genps.c
	gcc -o genps genps.c $(CFLAGS) $(LDFLAGS)

clip: clip.c
	gcc -o clip clip.c $(CFLAGS) $(LDFLAGS)

generate_font: generate_font.c
	gcc -o generate_font generate_font.c $(CFLAGS) $(LDFLAGS) -lpng -lz -lm

arrow: arrow.c
	gcc -o arrow arrow.c $(CFLAGS) $(LDFLAGS) -lpng -lz -lm
