/* dip.c
 * Digital Image Processing routines
 * (c) 2000-2002 Karel 'Clock' Kulhavy
 * This file is a part of the Links program, released under GPL.
 * This does various utilities for digital image processing including font
 * rendering.
 */

#ifdef G

#include "links.h"

#include <math.h>

/* #define this if you want to report missing letters to stderr.
 * Leave it commented up for normal operation and releases! */
/* #define REPORT_UNKNOWN 1 */

unsigned aspect=65536; /* aspect=65536 for 320x240
			* aspect=157286 for 640x200 (tall pixels)
			* Defines aspect ratio of screen pixels.
			* aspect=(196608*xsize+ysize<<1)/(ysize<<1);
			* Default is 65536 because we assume square pixel
			* when not specified otherwise. Aspect is ratio of
			* the pixel height (in milimeters) to pixel width,
			* multiplied by 65536. */

/* Limitation: No input image's dimension may exceed 2^(32-1-8) pixels.
 */

/* Each input byte represents 1 byte (gray). The question whether 0 is
 * black or 255 is black doesn't matter.
 */

/* These constants represent contrast-enhancement and sharpen filter (which is one filter
 * together) that is applied onto the letters to enhance their appearance at low height.
 * They were determined by experiment for several values, interpolated, checked and tuned.
 * If you change them, don't wonder if the letters start to look strange.
 * The numers in the comments denote which height the line applies for.
 */
static const float fancy_constants[64]={
	(float)0,(float)3,	/*  1 */
	(float).1,(float)3,	/*  2 */
	(float).2,(float)3,	/*  3 */
	(float).3,(float)2.9,	/*  4 */
	(float).4,(float)2.7,	/*  5 */
	(float).4,(float)2.5,	/*  6 */
	(float).4,(float)2,	/*  7 */
	(float).5,(float)2,	/*  8 */
	(float).4,(float)2,	/*  9 */
	(float).38,(float)1.9,	/* 10 */
	(float).36,(float)1.8,	/* 11 */
	(float).33,(float)1.7,	/* 12 */
	(float).30,(float)1.6,	/* 13 */
	(float).25,(float)1.5,	/* 14 */
	(float).20,(float)1.5,	/* 15 */
	(float).15,(float)1.5,	/* 16 */
	(float).14,(float)1.5,	/* 17 */
	(float).13,(float)1.5,	/* 18 */
	(float).12,(float)1.5,	/* 19 */
	(float).12,(float)1.5,	/* 20 */
	(float).12,(float)1.5,	/* 21 */
	(float).12,(float)1.5,	/* 22 */
	(float).11,(float)1.5,	/* 23 */
	(float).10,(float)1.4,	/* 24 */
	(float).09,(float)1.3,	/* 25 */
	(float).08,(float)1.3,	/* 26 */
	(float).04,(float)1.2,	/* 27 */
	(float).04,(float)1.2,	/* 28 */
	(float).02,(float)1.1,	/* 29 */
	(float).02,(float)1.1,	/* 30 */
	(float).01,(float)1,	/* 31 */
	(float).01,(float)1	/* 32 */
};


/* This shall be hopefully reasonably fast and portable
 * We assume ix is <65536. If not, the letters will be smaller in
 * horizontal dimension (because of overflow) but this will not cause
 * a segfault. 65536 pixels wide bitmaps are not normal and will anyway
 * exhaust the memory.
 */
static inline int compute_width(int ix, int iy, int required_height)
{
	int width;
	unsigned long reg;

	reg=(unsigned long)aspect*(unsigned long)required_height;

	if (reg>=0x1000000UL){
		/* It's big */
		reg=(reg+32768)>>16;
		width=(int)((reg*ix+(iy>>1))/iy);
	}else{
		/* It's small */
		reg=(reg+128)>>8;
		iy<<=8;
		width=(int)((reg*ix+(iy>>1))/iy);
	}
	if (width<1) width=1;
	return width;
}

static struct lru font_cache;
			/* This is a cache for screen-ready colored bitmaps
			 * of lettrs and/or alpha channels for these (are the
			 * same size, only one byte per pixel and are used
			 * for letters with an image under them )
			 */

typedef double scale_t;

/* Each input byte represents 1 byte (gray). The question whether 0 is
 * black or 255 is black doesn't matter.
 */

static void add_col_gray(unsigned *restrict col_buf, unsigned char *restrict ptr, int
		line_skip, int n, unsigned weight)
{
	for (;n;n--){
		*col_buf+=weight*(*ptr);
		ptr+=line_skip;
		col_buf++;
	}
}

 /* We assume unsigned short holds at least 16 bits. */
static void add_row_gray(unsigned *restrict row_buf, unsigned char *restrict ptr, int n,
	unsigned weight)
{
	for (;n;n--){
		*row_buf+=weight**ptr;
		ptr++;
		row_buf++;
	}
}

/* line_skip is in pixels. The column contains the whole pixels (R G B)
 * We assume unsigned short holds at least 16 bits. */
static void add_col_color(scale_t *restrict col_buf, unsigned short *restrict ptr,
	int line_skip, int n, ulonglong weight)
{
	if (!weight) return;
	{
		scale_t w = (scale_t)(longlong)weight;
		for (;n;n--){
			col_buf[0]+=w*ptr[0];
			col_buf[1]+=w*ptr[1];
			col_buf[2]+=w*ptr[2];
			ptr+=line_skip;
			col_buf+=3;
		}
	}
}

/* n is in pixels. pixel is 3 unsigned shorts in series */
 /* We assume unsigned short holds at least 16 bits. */
static void add_row_color(scale_t *restrict row_buf, unsigned short *restrict ptr,
	int n, ulonglong weight)
{
	if (!weight) return;
	{
		scale_t w = (scale_t)(longlong)weight;
		for (;n;n--){
			row_buf[0]+=w*ptr[0];
			row_buf[1]+=w*ptr[1];
			row_buf[2]+=w*ptr[2];
			ptr+=3;
			row_buf+=3;
		}
	}
}

/* We assume unsigned holds at least 32 bits */
static void emit_and_bias_col_gray(unsigned *restrict col_buf, unsigned char *restrict out,
	int line_skip, int n, unsigned weight)
{
	unsigned half=weight>>1;

	for (;n;n--){
		*out=*col_buf/weight;
		out+=line_skip;
		*col_buf++=half;
	}
}

/* We assume unsigned holds at least 32 bits */
static void emit_and_bias_row_gray(unsigned *restrict row_buf, unsigned char *restrict out,
	int n, unsigned weight)
{
	unsigned half=weight>>1;

	for (;n;n--){
		*out++=*row_buf/weight;
		*row_buf++=half;
	}
}

/* We assume unsigned holds at least 32 bits */
static void bias_buf_gray(unsigned *restrict col_buf, int n, unsigned half)
{
	for (;n;n--) *col_buf++=half;
}

/* We assume unsigned holds at least 32 bits */
static void bias_buf_color(scale_t *restrict col_buf, int n, scale_t half)
{
	for (;n;n--){
		col_buf[0]=half;
		col_buf[1]=half;
		col_buf[2]=half;
		col_buf+=3;
	}
	/* System activated */
}

#define	op(x)	((x) * inv_weight)

/* line skip is in pixels. Pixel is 3*unsigned short */
/* We assume unsigned holds at least 32 bits */
/* We assume unsigned short holds at least 16 bits. */
static void emit_and_bias_col_color(scale_t *restrict col_buf,
	unsigned short *restrict out, int line_skip, int n, unsigned weight)
{
	scale_t half=(scale_t)weight / 2;
	scale_t inv_weight = (scale_t)1 / weight;
	for (;n;n--){
		out[0]=(unsigned short)op(col_buf[0]);
		col_buf[0]=half;
		out[1]=(unsigned short)op(col_buf[1]);
		col_buf[1]=half;
		/* The following line is an enemy of the State and will be
		 * prosecuted according to the Constitution of The United States
		 * Cap. 20/3 ix. Sel. Bill 12/1920
		 * Moses 12/20 Erizea farizea 2:2:1:14
		 */
		out[2]=(unsigned short)op(col_buf[2]);
		col_buf[2]=half;
		out+=line_skip;
		col_buf+=3;
	}
}

/* n is in pixels. pixel is 3 unsigned shorts in series. */
/* We assume unsigned holds at least 32 bits */
/* We assume unsigned short holds at least 16 bits. */
static void emit_and_bias_row_color(scale_t *restrict row_buf,
	unsigned short *restrict out, int n, unsigned weight)
{
	scale_t half = (scale_t)weight / 2;
	scale_t inv_weight = (scale_t)1 / weight;
	for (;n;n--){
		out[0]=(unsigned short)op(row_buf[0]);
		row_buf[0]=half;
		out[1]=(unsigned short)op(row_buf[1]);
		row_buf[1]=half;
		out[2]=(unsigned short)op(row_buf[2]);
		row_buf[2]=half;
		out+=3;
		row_buf+=3;
	}
}

#undef op

/* For enlargement only -- does linear filtering.
 * Allocates output and frees input.
 * We assume unsigned holds at least 32 bits */
static void enlarge_gray_horizontal(unsigned char *in, int ix, int y,
	unsigned char ** out, int ox)
{
	unsigned *col_buf;
	int total;
	int out_pos,in_pos,in_begin,in_end;
	unsigned half=(ox-1)>>1;
	unsigned char *outptr;
	unsigned char *inptr;

	if (ox && (unsigned)ox * (unsigned)y / (unsigned)ox != (unsigned)y) overalloc();
	if ((unsigned)ox * (unsigned)y > INT_MAX) overalloc();
	outptr = xmalloc(ox * y);
	inptr=in;
	*out=outptr;
	if (ix==1){
		/* Dull copying */
		for (;y;y--){
			memset(outptr,*inptr,ox);
			outptr+=ox;
			inptr++;
		}
		free(in);
	}else{
		total=(ix-1)*(ox-1);
		if ((unsigned)y > INT_MAX / sizeof(*col_buf)) overalloc();
		col_buf = xmalloc(y * sizeof(*col_buf));
		bias_buf_gray(col_buf, y, half);
		out_pos=0;
		in_pos=0;
		again:
		in_begin=in_pos;
		in_end=in_pos+ox-1;
		add_col_gray(col_buf,inptr, ix, y, in_end-out_pos);
		add_col_gray(col_buf,inptr+1, ix, y, out_pos-in_begin);
		emit_and_bias_col_gray(col_buf,outptr,ox,y,ox-1);
		outptr++;
		out_pos+=ix-1;
		if (out_pos>in_end){
			in_pos=in_end;
			inptr++;
		}
		if (out_pos>total){
			free(in);
			free(col_buf);
			return;
		}
		goto again;
	}
	/* Rohan, oh Rohan... */
	/* ztracena zeme :) */
}

/* For enlargement only -- does linear filtering
 * Frees input and allocates output.
 * We assume unsigned holds at least 32 bits
 */
static void enlarge_color_horizontal(unsigned short *in, int ix, int y,
	unsigned short **outa, int ox)
{
	int alloc_size;
	scale_t *col_buf;
	int a;
	unsigned skip=3*ix;
	unsigned oskip=3*ox;
	unsigned short *out;

	if (!in) {
		*outa = NULL;
		return;
	}

	if (ix==ox){
		*outa=in;
		return;
	}
	if (ox && (unsigned)ox * (unsigned)y / (unsigned)ox != (unsigned)y) overalloc();
	if ((unsigned)ox * (unsigned)y > INT_MAX / 3 / sizeof(*out)) overalloc();
	out = xmalloc(sizeof(*out) * 3 * ox * y);
	*outa=out;
	if (!out) {
		free(in);
		return;
	}
	if (ix==1){
		unsigned short *inp = in;
		for (;y;y--,inp+=3) for (a=ox;a;a--,out+=3){
			out[0]=inp[0];
			out[1]=inp[1];
			out[2]=inp[2];
		}
		free(in);
		return;
	}

	if ((unsigned)y > (INT_MAX) / 3 / sizeof(*col_buf))
		overalloc();
	alloc_size = (int)(y*3*sizeof(*col_buf));
	alloc_size = (alloc_size) & ~(0);
	if (alloc_size > INT_MAX)
		overalloc();
	col_buf = xmalloc(alloc_size);
	{
		scale_t *thread_col_buf;
		int out_idx;
		thread_col_buf = (scale_t *)((char *)col_buf);
		bias_buf_color(thread_col_buf, y, (scale_t)(ox - 1) / 2);
		for (out_idx = 0; out_idx <= ox - 1; out_idx++) {
			ulonglong out_pos, in_pos, in_end;
			int in_idx;
			out_pos = (ulonglong)out_idx * (ix - 1);
			if (out_idx)
				in_idx = (int)((out_pos - 1) / (ox - 1));
			else
				in_idx = 0;
			in_pos = (ulonglong)in_idx * (ox - 1);
			in_end = in_pos + (ox - 1);
			add_col_color(thread_col_buf, in + in_idx * 3, skip, y,
				in_end - out_pos);
			add_col_color(thread_col_buf, in + (in_idx + 1) * 3, skip, y,
				out_pos - in_pos);
			emit_and_bias_col_color(thread_col_buf, out + out_idx * 3, oskip, y, ox - 1);
		}
	}
	free(in);
	if (col_buf) 
		free(col_buf);
	else {
		free(out);
		*outa = NULL;
	}
}

/* Works for both enlarging and diminishing. Linear resample, no low pass.
 * Automatically mem_frees the "in" and allocates "out". */
/* We assume unsigned holds at least 32 bits */
static void scale_gray_horizontal(unsigned char *in, int ix, int y,
	unsigned char ** out, int ox)
{
	unsigned *col_buf;
	int total=ix*ox;
	int out_pos,in_pos,in_begin,in_end,out_end;
	unsigned char *outptr;
	unsigned char *inptr;

	if (ix<ox){
		enlarge_gray_horizontal(in,ix,y,out,ox);
		return;
	}else if (ix==ox){
		*out=in;
		return;
	}
	if (ox && (unsigned)ox * (unsigned)y / (unsigned)ox != (unsigned)y) overalloc();
	if ((unsigned)ox * (unsigned)y > INT_MAX) overalloc();
	outptr = xmalloc(ox * y);
	inptr=in;
	*out=outptr;
	if ((unsigned)y > INT_MAX / sizeof(*col_buf)) overalloc();
	col_buf = xmalloc(y * sizeof(*col_buf));
	bias_buf_gray(col_buf, y, ix>>1);
	out_pos=0;
	in_pos=0;
	again:
	in_begin=in_pos;
	in_end=in_pos+ox;
	out_end=out_pos+ix;
	if (in_begin<out_pos)in_begin=out_pos;
	if (in_end>out_end)in_end=out_end;
	add_col_gray(col_buf,inptr,ix,y,in_end-in_begin);
	in_end=in_pos+ox;
	if (out_end>=in_end){
		in_pos=in_end;
		inptr++;
	}
	if (out_end<=in_end){
			emit_and_bias_col_gray(col_buf,outptr,ox,y,ix);
			out_pos=out_pos+ix;
			outptr++;
	}
	if (out_pos==total) {
		free(in);
		free(col_buf);
		return;
	}
	goto again;
}

/* Works for both enlarging and diminishing. Linear resample, no low pass.
 * Does only one color component.
 * Frees ina and allocates outa.
 * If ox*3<=ix, and display_optimize, performs optimization for LCD.
 */
static void scale_color_horizontal(unsigned short *in, int ix, int y,
		unsigned short **outa, int ox)
{
	int alloc_size;
	scale_t *col_buf;
	unsigned skip=3*ix;
	unsigned oskip=3*ox;
	unsigned short *out;

	if (!in) {
		*outa = NULL;
		return;
	}

	if (ix==ox){
		*outa=in;
		return;
	}
	if (ix<ox){
		enlarge_color_horizontal(in,ix,y,outa,ox);
		return;
	}
	if (ox && (unsigned)ox * (unsigned)y / (unsigned)ox != (unsigned)y) overalloc();
	if ((unsigned)ox * (unsigned)y > INT_MAX / 3 / sizeof(*out)) overalloc();
	out = xmalloc(sizeof(*out) * 3 * ox * y);
	*outa=out;
	if (!out) {
		free(in);
		return;
	}

	if ((unsigned)y > (INT_MAX) / 3 / sizeof(*col_buf))
		overalloc();
	alloc_size = (int)(y*3*sizeof(*col_buf));
	alloc_size = (alloc_size) & ~(0);
	if (alloc_size > INT_MAX)
		overalloc();
	col_buf = xmalloc(alloc_size);
	{
		scale_t *thread_col_buf;
		int out_idx;
		thread_col_buf = (scale_t *)((char *)col_buf);
		bias_buf_color(thread_col_buf, y, (scale_t)ix / 2);
		for (out_idx = 0; out_idx < ox; out_idx++) {
			ulonglong out_pos, out_end, in_pos;
			int in_idx;
			out_pos = (ulonglong)out_idx * ix;
			out_end = out_pos + ix;
			in_idx = (int)(out_pos / ox);
			in_pos = (ulonglong)in_idx * ox;
			do {
				ulonglong in_begin, in_end;
				in_begin = in_pos;
				in_end = in_pos + ox;
				if (in_begin < out_pos) in_begin = out_pos;
				if (in_end > out_end) in_end = out_end;
				add_col_color(thread_col_buf, in + in_idx * 3, skip, y, in_end - in_begin);
				in_idx++;
				in_pos += ox;
			} while (in_pos < out_end);
			emit_and_bias_col_color(thread_col_buf, out + out_idx * 3, oskip, y, ix);
		}
	}
	free(in);
	if (col_buf)
		free(col_buf);
	else {
		free(out);
		*outa = NULL;
	}
}

/* For magnification only. Does linear filtering. */
/* We assume unsigned holds at least 32 bits */
static void enlarge_gray_vertical(unsigned char *in, int x, int iy,
	unsigned char ** out ,int oy)
{
	unsigned *row_buf;
	int total;
	int out_pos,in_pos,in_begin,in_end;
	int half=(oy-1)>>1;
	unsigned char *outptr;
	unsigned char *inptr;

	if (iy==1){
		if (x && (unsigned)x * (unsigned)oy / (unsigned)x != (unsigned)oy) overalloc();
		if ((unsigned)x * (unsigned)oy > INT_MAX) overalloc();
		outptr = xmalloc(oy * x);
		*out=outptr;
		for(;oy;oy--,outptr+=x)
			memcpy(outptr,in,x);
		free(in);
	}
	else if (iy==oy){
		*out=in;
	}else{
		if (x && (unsigned)x * (unsigned)oy / (unsigned)x != (unsigned)oy) overalloc();
		if ((unsigned)x * (unsigned)oy > INT_MAX) overalloc();
		outptr = xmalloc(oy*x);
		inptr=in;
		*out=outptr;
		total=(iy-1)*(oy-1);
		if ((unsigned)x > INT_MAX / sizeof(*row_buf)) overalloc();
		row_buf = xmalloc(x * sizeof(*row_buf));
		bias_buf_gray(row_buf, x, half);
		out_pos=0;
		in_pos=0;
		again:
		in_begin=in_pos;
		in_end=in_pos+oy-1;
		add_row_gray(row_buf, inptr, x, in_end-out_pos);
		add_row_gray(row_buf, inptr+x, x, out_pos-in_begin);
		emit_and_bias_row_gray(row_buf, outptr, x, oy-1);
		outptr+=x;
		out_pos+=iy-1;
		if (out_pos>in_end){
			in_pos=in_end;
			inptr+=x;
		}
		if (out_pos>total){
			free(in);
			free(row_buf);
			return;
		}
		goto again;
	}
}

/* For magnification only. Does linear filtering */
/* We assume unsigned holds at least 32 bits */
static void enlarge_color_vertical(unsigned short *in, int x, int iy,
	unsigned short **outa ,int oy)
{
	int alloc_size;
	scale_t *row_buf;
	unsigned short *out;

	if (!in) {
		*outa = NULL;
		return;
	}

	if (iy==oy){
		*outa=in;
		return;
	}
	/* Rivendell */
	if (x && (unsigned)x * (unsigned)oy / (unsigned)x != (unsigned)oy) overalloc();
	if ((unsigned)x * (unsigned)oy > INT_MAX / 3 / sizeof(*out)) overalloc();
	out = xmalloc(sizeof(*out) * 3 * oy * x);
	*outa=out;
	if (!out) {
		free(in);
		return;
	}
	if (iy==1){
		for (;oy;oy--){
			memcpy(out,in,3*x*sizeof(*out));
			out+=3*x;
		}
		free(in);
		return;
	}

	if ((unsigned)x > (INT_MAX) / 3 / sizeof(*row_buf))
		overalloc();
	alloc_size = (int)(x*3*sizeof(*row_buf));
	alloc_size = (alloc_size) & ~(0);
	if (alloc_size > INT_MAX)
		overalloc();
	row_buf = xmalloc(alloc_size);
	{
		scale_t *thread_row_buf;
		int out_idx;
		thread_row_buf = (scale_t *)((char *)row_buf);
		bias_buf_color(thread_row_buf,x,(scale_t)(oy-1) / 2);
		for (out_idx = 0; out_idx <= oy - 1; out_idx++) {
			ulonglong out_pos, in_pos, in_end;
			int in_idx;
			out_pos = (ulonglong)out_idx * (iy - 1);
			if (out_idx)
				in_idx = (int)((out_pos - 1) / (oy - 1));
			else
				in_idx = 0;
			in_pos = (ulonglong)in_idx * (oy - 1);
			in_end = in_pos + oy - 1;
			add_row_color(thread_row_buf, in + in_idx * 3 * x, x,
				in_end - out_pos);
			add_row_color(thread_row_buf, in + (in_idx + 1) * 3 * x, x,
				out_pos - in_pos);
			emit_and_bias_row_color(thread_row_buf, out + out_idx * 3 * x, x, oy - 1);
		}
	}
	free(in);
	if (row_buf)
		free(row_buf);
	else {
		free(out);
		*outa = NULL;
	}
}

/* Both enlarges and diminishes. Linear filtering.
 * Automatically allocates output and frees input.
 * We assume unsigned holds at least 32 bits */
static void scale_gray_vertical(unsigned char *in, int x, int iy,
	unsigned char ** out ,int oy)
{
	unsigned *row_buf;
	int total=iy*oy;
	int out_pos,in_pos,in_begin,in_end,out_end;
	unsigned char *outptr;
	unsigned char *inptr;

	/* Snow White, Snow White... */
	if (iy<oy){
		enlarge_gray_vertical(in,x,iy,out,oy);
		return;
	}
	if (iy==oy){
		*out=in;
		return;
	}
	if (x && (unsigned)x * (unsigned)oy / (unsigned)x != (unsigned)oy) overalloc();
	if ((unsigned)x * (unsigned)oy > INT_MAX) overalloc();
	outptr = xmalloc(x * oy);
	inptr=in;
	*out=outptr;
	if ((unsigned)x > INT_MAX / sizeof(*row_buf)) overalloc();
	row_buf=mem_calloc(x*sizeof(*row_buf));
	bias_buf_gray(row_buf, x, iy>>1);
	out_pos=0;
	in_pos=0;
	again:
	in_begin=in_pos;
	in_end=in_pos+oy;
	out_end=out_pos+iy;
	if (in_begin<out_pos)in_begin=out_pos;
	if (in_end>out_end)in_end=out_end;
	add_row_gray(row_buf,inptr,x,in_end-in_begin);
	in_end=in_pos+oy;
	if (out_end>=in_end){
		in_pos=in_end;
		inptr+=x;
	}
	if (out_end<=in_end){
			emit_and_bias_row_gray(row_buf,outptr,x,iy);
			out_pos=out_pos+iy;
			outptr+=x;
	}
	if (out_pos==total){
		free(in);
		free(row_buf);
		return;
	}
	goto again;
}

/* Both enlarges and diminishes. Linear filtering. Sizes are
   in pixels. Sizes are not in bytes. 1 pixel=3 unsigned shorts.
   We assume unsigned short can hold at least 16 bits.
   We assume unsigned holds at least 32 bits.
 */
static void scale_color_vertical(unsigned short *in, int x, int iy,
	unsigned short **outa, int oy)
{
	int alloc_size;
	scale_t *row_buf;
	unsigned short *out;

	if (!in) {
		*outa = NULL;
		return;
	}

	if (iy==oy){
		*outa=in;
		return;
	}
	if (iy<oy){
		enlarge_color_vertical(in,x,iy,outa,oy);
		return;
	}
	if (x && (unsigned)x * (unsigned)oy / (unsigned)x != (unsigned)oy) overalloc();
	if ((unsigned)x * (unsigned)oy > INT_MAX / 3 / sizeof(*out)) overalloc();
	out = xmalloc(sizeof(*out) * 3 * oy * x);
	*outa=out;
	if (!out) {
		free(in);
		return;
	}
	if ((unsigned)x > (INT_MAX) / 3 / sizeof(*row_buf))
		overalloc();
	alloc_size = (int)(x*3*sizeof(*row_buf));
	alloc_size = (alloc_size) & ~(0);
	if (alloc_size > INT_MAX)
		overalloc();
	row_buf = xmalloc(alloc_size);
	{
		scale_t *thread_row_buf;
		int out_idx;
		thread_row_buf = (scale_t *)((char *)row_buf);
		bias_buf_color(thread_row_buf,x,(scale_t)iy / 2);
		for (out_idx = 0; out_idx < oy; out_idx++) {
			ulonglong out_pos, out_end, in_pos;
			int in_idx;
			out_pos = (ulonglong)out_idx * iy;
			out_end = out_pos + iy;
			in_idx = (int)(out_pos / oy);
			in_pos = (ulonglong)in_idx * oy;
			do {
				ulonglong in_begin, in_end;
				in_begin = in_pos;
				in_end = in_pos + oy;
				if (in_begin < out_pos) in_begin = out_pos;
				if (in_end > out_end) in_end = out_end;
				add_row_color(thread_row_buf, in + in_idx * 3 * x, x, in_end - in_begin);
				in_idx++;
				in_pos += oy;
			} while (in_pos < out_end);
			emit_and_bias_row_color(thread_row_buf, out + out_idx * 3 * x, x, iy);
		}
	}
	free(in);
	if (row_buf) 
		free(row_buf);
	else {
		free(out);
		*outa = NULL;
	}
}


/* Scales grayscale 8-bit map. Both enlarges and diminishes. Uses either low
 * pass or bilinear filtering. Automatically mem_frees the "in".
 * Automatically allocates "out".
 */
static void scale_gray(unsigned char *in, int ix, int iy,
	unsigned char **out, int ox, int oy)
{
	unsigned char *intermediate_buffer;

	if (!ix||!iy){
		free(in);
		if (ox && (unsigned)ox * (unsigned)oy / (unsigned)ox != (unsigned)oy) overalloc();
		if ((unsigned)ox * (unsigned)oy > INT_MAX) overalloc();
		*out=mem_calloc(ox*oy);
		return;
	}
	if (ix*oy<ox*iy){
		scale_gray_vertical(in,ix,iy,&intermediate_buffer,oy);
		scale_gray_horizontal(intermediate_buffer,ix,oy,out,ox);
	}else{
		scale_gray_horizontal(in,ix,iy,&intermediate_buffer,ox);
		scale_gray_vertical(intermediate_buffer,ox,iy,out,oy);
	}
}

/* To be called only when global variable display_optimize is 1 or 2.
 * Performs a decimation according to this variable. Data shrink to 1/3
 * and x is the smaller width.
 * There must be 9*x*y unsigned shorts of data.
 * x must be >=1.
 * Performs realloc onto the buffer after decimation to save memory.
 */
static void decimate_3(unsigned short **data0, int x, int y)
{
	unsigned short *data=*data0;
	unsigned short *ahead=data;
	int i, futuresize;
	if (!data)
		return;
	if (x && (unsigned)x * (unsigned)y / (unsigned)x != (unsigned)y) overalloc();
	if ((unsigned)x * (unsigned)y > INT_MAX / 3 / sizeof(**data0)) overalloc();
	futuresize=x*y*3*(int)sizeof(**data0);

#ifdef DEBUG
	if (!(x>0&&y>0)) internal("zero width or height in decimate_3");
#endif /* #Ifdef DEBUG */
	if (display_optimize==1){
		if (x==1){
			for (;y;y--,ahead+=9,data+=3){
				data[0]=(ahead[0]+ahead[0]+ahead[3])/3;
				data[1]=(ahead[1]+ahead[4]+ahead[7])/3;
				data[2]=(ahead[5]+ahead[8]+ahead[8])/3;
			}
		}else{
			for (;y;y--){
				data[0]=(ahead[0]+ahead[0]+ahead[3])/3;
				data[1]=(ahead[1]+ahead[4]+ahead[7])/3;
				data[2]=(ahead[5]+ahead[8]+ahead[11])/3;
				for (ahead+=9,data+=3,i=x-2;i;i--,ahead+=9,data+=3){
					data[0]=(ahead[-3]+ahead[0]+ahead[3])/3;
					data[1]=(ahead[1]+ahead[4]+ahead[7])/3;
					data[2]=(ahead[5]+ahead[8]+ahead[11])/3;
				}
				data[0]=(ahead[-3]+ahead[0]+ahead[3])/3;
				data[1]=(ahead[1]+ahead[4]+ahead[7])/3;
				data[2]=(ahead[5]+ahead[8]+ahead[8])/3;
				ahead += 9;
				data += 3;
			}
		}
	}else{
		/* display_optimize==2 */
		if (x==1){
			for (;y;y--,ahead+=9,data+=3){
				data[0]=(ahead[3]+ahead[6]+ahead[6])/3;
				data[1]=(ahead[1]+ahead[4]+ahead[7])/3;
				data[2]=(ahead[2]+ahead[2]+ahead[5])/3;
			}
		}else{
			for (;y;y--){
				data[0]=(ahead[3]+ahead[6]+ahead[9])/3;
				data[1]=(ahead[1]+ahead[4]+ahead[7])/3;
				data[2]=(ahead[2]+ahead[2]+ahead[5])/3;
				for (ahead+=9,data+=3,i=x-2;i;i--,ahead+=9,data+=3){
					data[0]=(ahead[3]+ahead[6]+ahead[9])/3;
					data[1]=(ahead[1]+ahead[4]+ahead[7])/3;
					data[2]=(ahead[-1]+ahead[2]+ahead[5])/3;
				}
				data[0]=(ahead[3]+ahead[6]+ahead[6])/3;
				data[1]=(ahead[1]+ahead[4]+ahead[7])/3;
				data[2]=(ahead[-1]+ahead[2]+ahead[5])/3;
				ahead += 9;
				data += 3;
			}
		}
	}
	*data0 = xrealloc(*data0, futuresize);
}

/* Scales color 48-bits-per-pixel bitmap. Both enlarges and diminishes. Uses
 * either low pass or bilinear filtering. The memory organization for both
 * input and output are red, green, blue. All three of them are unsigned shorts 0-65535.
 * Allocates output and frees input
 * We assume unsigned short holds at least 16 bits.
 */
void scale_color(unsigned short *in, int ix, int iy, unsigned short **out,
	int ox, int oy)
{
	unsigned short *intermediate_buffer;
	int do_optimize;
	int ox0=ox;

	if (!ix||!iy){
		free(in);
		if (ox && (unsigned)ox * (unsigned)oy / (unsigned)ox != (unsigned)oy) overalloc();
		if ((unsigned)ox * (unsigned)oy > INT_MAX / 3 / sizeof(**out)) overalloc();
		*out = mem_calloc(ox * oy * sizeof(**out) * 3);
		return;
	}
	if (display_optimize&&ox*3<=ix){
		do_optimize=1;
		ox0=ox;
		ox*=3;
	}else do_optimize=0;
	if (ix*oy<ox*iy){
		scale_color_vertical(in,ix,iy,&intermediate_buffer,oy);
		scale_color_horizontal(intermediate_buffer,ix,oy,out,ox);
	}else{
		scale_color_horizontal(in,ix,iy,&intermediate_buffer,ox);
		scale_color_vertical(intermediate_buffer,ox,iy,out,oy);
	}
	if (do_optimize)
		decimate_3(out, ox0, oy);
}

/* Fills a block with given color. length is number of pixels. pixel is a
 * tribyte. 24 bits per pixel.
 */
void mix_one_color_24(unsigned char *restrict dest, int length,
		   unsigned char r, unsigned char g, unsigned char b)
{
	for (;length;length--){
		dest[0]=r;
		dest[1]=g;
		dest[2]=b;
		dest+=3;
	}
}

/* Fills a block with given color. length is number of pixels. pixel is a
 * tribyte. 48 bits per pixel.
 * We assume unsigned short holds at least 16 bits.
 */
void mix_one_color_48(unsigned short *restrict dest, int length,
		   unsigned short r, unsigned short g, unsigned short b)
{
	for (;length;length--){
		dest[0]=r;
		dest[1]=g;
		dest[2]=b;
		dest+=3;
	}
}

/* Mixes ink and paper of a letter, using alpha as alpha mask.
 * Only mixing in photon space makes physical sense so that the input values
 * must always be equivalent to photons and not to electrons!
 * length is number of pixels. pixel is a tribyte
 * alpha is 8-bit, rgb are all 16-bit
 * We assume unsigned short holds at least 16 bits.
 */
static void mix_two_colors(unsigned short *restrict dest, unsigned char *restrict alpha,
	int length ,unsigned short r0, unsigned short g0, unsigned short b0,
	unsigned short r255, unsigned short g255, unsigned short b255)
{
	unsigned char mask, cmask;

	for (;length;length--){
		mask=*alpha++;
		if (((mask+1)&255)>=2){
			cmask=255-mask;
			dest[0]=(mask*r255+cmask*r0+127)/255;
			dest[1]=(mask*g255+cmask*g0+127)/255;
			dest[2]=(mask*b255+cmask*b0+127)/255;
		}else{
			if (mask){
				dest[0]=r255;
				dest[1]=g255;
				dest[2]=b255;
			}else{
				dest[0]=r0;
				dest[1]=g0;
				dest[2]=b0;
			}
		}
		dest+=3;
	}
}

/* We assume unsigned short holds at least 16 bits. */
void agx_and_uc_32_to_48_table(unsigned short *restrict dest,
		const unsigned char *restrict src, int lenght, unsigned short *restrict table,
		unsigned short rb, unsigned short gb, unsigned short bb)
{
	unsigned char alpha, calpha;
	unsigned short ri, gi, bi;

	for (;lenght;lenght--)
	{
		ri=table[src[0]];
		gi=table[src[1]+256];
		bi=table[src[2]+512];
		alpha=src[3];
		src+=4;
		if (((alpha+1)&255)>=2){
			calpha=255-alpha;
			dest[0]=(ri*alpha+calpha*rb+127)/255;
			dest[1]=(gi*alpha+calpha*gb+127)/255;
			dest[2]=(bi*alpha+calpha*bb+127)/255;
		}else{
			if (alpha){
				dest[0]=ri;
				dest[1]=gi;
				dest[2]=bi;
			}else{
				dest[0]=rb;
				dest[1]=gb;
				dest[2]=bb;
			}
		}
		dest+=3;
	}
}

/* src is a block of four-bytes RGBA. All bytes are gamma corrected. length is
 * number of pixels. output is input powered to the given gamma, passed into
 * dest. src and dest may be identical and it will work. rb, gb, bb are 0-65535
 * in linear monitor output photon space
 */
/* We assume unsigned short holds at least 16 bits. */
void agx_and_uc_32_to_48(unsigned short *restrict dest,
		const unsigned char *restrict src, int lenght, float red_gamma,
		float green_gamma, float blue_gamma, unsigned short rb,
		unsigned short gb, unsigned short bb)
{
	float r,g,b;
	unsigned char alpha, calpha;
	unsigned short ri,gi,bi;
	const float inv_255=(float)(1/255.);

	for (;lenght;lenght--)
	{
		r=src[0];
		g=src[1];
		b=src[2];
		alpha=src[3];
		src+=4;
		r*=inv_255;
		g*=inv_255;
		b*=inv_255;
		r=powf(r,red_gamma);
		g=powf(g,green_gamma);
		b=powf(b,blue_gamma);
		ri=(unsigned)((r*65535)+(float)0.5);
		gi=(unsigned)((g*65535)+(float)0.5);
		bi=(unsigned)((b*65535)+(float)0.5);
		if (((alpha + 1) & 255) >= 2) {
			calpha=255-alpha;
			dest[0]=(unsigned short)((ri*alpha+calpha*rb+127U)/255U);
			dest[1]=(unsigned short)((gi*alpha+calpha*gb+127U)/255U);
			dest[2]=(unsigned short)((bi*alpha+calpha*bb+127U)/255U);
		}else{
			if (alpha){
				dest[0]=ri;
				dest[1]=gi;
				dest[2]=bi;
			}else{
				dest[0]=rb;
				dest[1]=gb;
				dest[2]=bb;
			}
		}
		dest+=3;
	}
}
/* src is a block of four-bytes RGBA. All bytes are gamma corrected. length is
 * number of pixels. output is input powered to the given gamma, passed into
 * dest. src and dest may be identical and it will work. rb, gb, bb are 0-65535
 * in linear monitor output photon space. alpha 255 means full image no background.
 */
/* We assume unsigned short holds at least 16 bits. */
void agx_and_uc_64_to_48(unsigned short *restrict dest,
		const unsigned short *restrict src, int lenght, float red_gamma,
		float green_gamma, float blue_gamma, unsigned short rb,
		unsigned short gb, unsigned short bb)
{
	float r,g,b;
	unsigned short alpha, calpha;
	unsigned short ri,gi,bi;
	const float inv_65535=(float)(1/65535.);

	for (;lenght;lenght--)
	{
		r=src[0];
		g=src[1];
		b=src[2];
		alpha=src[3];
		src+=4;
		r*=inv_65535;
		g*=inv_65535;
		b*=inv_65535;
		r=powf(r,red_gamma);
		g=powf(g,green_gamma);
		b=powf(b,blue_gamma);
		ri=(unsigned short)(r*65535+(float)0.5);
		gi=(unsigned short)(g*65535+(float)0.5);
		bi=(unsigned short)(b*65535+(float)0.5);
		if (((alpha + 1) & 65535) >= 2) {
			calpha=65535-alpha;
			dest[0]=(unsigned short)((ri*alpha+calpha*rb+32767U)/65535U);
			dest[1]=(unsigned short)((gi*alpha+calpha*gb+32767U)/65535U);
			dest[2]=(unsigned short)((bi*alpha+calpha*bb+32767U)/65535U);
		}else{
			if (alpha){
				dest[0]=ri;
				dest[1]=gi;
				dest[2]=bi;
			}else{
				dest[0]=rb;
				dest[1]=gb;
				dest[2]=bb;
			}
		}
		dest+=3;
	}
}

/* src is a block of four-bytes RGBA. All bytes are gamma corrected. length is
 * number of pixels. output is input powered to the given gamma, passed into
 * dest. src and dest may be identical and it will work. rb, gb, bb are 0-65535
 * in linear monitor output photon space. alpha 255 means full image no background.
 * We assume unsigned short holds at least 16 bits. */
void agx_and_uc_64_to_48_table(unsigned short *restrict dest,
		const unsigned short *restrict src, int lenght, unsigned short *restrict gamma_table,
		unsigned short rb, unsigned short gb, unsigned short bb)
{
	unsigned short alpha, calpha;
	unsigned short ri,gi,bi;

	for (;lenght;lenght--)
	{
		ri=gamma_table[*src];
		gi=gamma_table[src[1]+65536];
		bi=gamma_table[src[2]+131072];
		alpha=src[3];
		src+=4;
		if (((alpha+1)&65535)>=2){
			calpha=65535-alpha;
			dest[0]=(ri*alpha+calpha*rb+32767)/65535;
			dest[1]=(gi*alpha+calpha*gb+32767)/65535;
			dest[2]=(bi*alpha+calpha*bb+32767)/65535;
		}else{
			if (alpha){
				dest[0]=ri;
				dest[1]=gi;
				dest[2]=bi;
			}else{
				dest[0]=rb;
				dest[1]=gb;
				dest[2]=bb;
			}
		}
		dest+=3;
	}
}

/* src is a block of three-bytes. All bytes are gamma corrected. length is
 * number of triplets. output is input powered to the given gamma, passed into
 * dest. src and dest may be identical and it will work.
 * We assume unsigned short holds at least 16 bits. */
void agx_48_to_48(unsigned short *restrict dest,
		const unsigned short *restrict src, int lenght, float red_gamma,
		float green_gamma, float blue_gamma)
{
	float a;
	const float inv_65535=(float)(1/65535.);

	for (;lenght;lenght--,src+=3,dest+=3)
	{
		a=*src;
		a*=inv_65535;
		a=powf(a,red_gamma);
		dest[0]=(unsigned short)((a*65535)+(float)0.5);
		a=src[1];
		a*=inv_65535;
		a=powf(a,green_gamma);
		dest[1]=(unsigned short)((a*65535)+(float)0.5);
		a=src[2];
		a*=inv_65535;
		a=powf(a,blue_gamma);
		dest[2]=(unsigned short)((a*65535)+(float)0.5);
	}
}

/* src is a block of three-bytes. All bytes are gamma corrected. length is
 * number of triples. output is input powered to the given gamma, passed into
 * dest. src and dest may be identical and it will work.
 * We assume unsigned short holds at least 16 bits. */
void agx_48_to_48_table(unsigned short *restrict dest,
		const unsigned short *restrict src, int lenght, unsigned short *restrict table)
{
	for (;lenght;lenght--,src+=3,dest+=3)
	{
		dest[0]=table[*src];
		dest[1]=table[src[1]+65536];
		dest[2]=table[src[2]+131072];
	}
}

/* src is a block of three-bytes. All bytes are gamma corrected. length is
 * number of triples. output is input powered to the given gamma, passed into
 * dest. src and dest may be identical and it will work.
 * We assume unsigned short holds at least 16 bits. */
void agx_24_to_48(unsigned short *restrict dest, const unsigned char *restrict src, int
			  lenght, float red_gamma, float green_gamma, float
			  blue_gamma)
{
	float a;
	const float inv_255=(float)(1/255.);

	for (;lenght;lenght--,src+=3,dest+=3)
	{
		a=*src;
		a*=inv_255;
		a=powf(a,red_gamma);
		dest[0]=(unsigned short)((a*65535)+(float)0.5);
		a=src[1];
		a*=inv_255;
		a=powf(a,green_gamma);
		dest[1]=(unsigned short)((a*65535)+(float)0.5);
		a=src[2];
		a*=inv_255;
		a=powf(a,blue_gamma);
		dest[2]=(unsigned short)((a*65535)+(float)0.5);
	}
}

/* Allocates new gamma_table and fills it with mapping 8 bits ->
 * power to user_gamma/cimg->*_gamma -> 16 bits
 * We assume unsigned short holds at least 16 bits. */
void make_gamma_table(struct cached_image *cimg)
{
	float rg=(float)((float)user_gamma/cimg->red_gamma);
	float gg=(float)((float)user_gamma/cimg->green_gamma);
	float bg=(float)((float)user_gamma/cimg->blue_gamma);
	int a;
	unsigned short *ptr_16;
	unsigned short last_val;
	const float inv_255=(float)(1/255.);
	const float inv_65535=(float)(1/65535.);

	if (cimg->buffer_bytes_per_pixel<=4){
		/* 8-bit */
		ptr_16 = xmalloc(768 * sizeof(*(cimg->gamma_table)));
		cimg->gamma_table=ptr_16;
		for (a=0;a<256;a++,ptr_16++){
			last_val = (unsigned short)(65535*powf((float)a*inv_255,rg)+(float)0.5);
			*ptr_16 = last_val;
		}
		for (a=0;a<256;a++,ptr_16++){
			last_val = (unsigned short)(65535*powf((float)a*inv_255,gg)+(float)0.5);
			*ptr_16 = last_val;
		}
		for (a=0;a<256;a++,ptr_16++){
			last_val = (unsigned short)(65535*powf((float)a*inv_255,bg)+(float)0.5);
			*ptr_16 = last_val;
		}
	}else{
		int x_slow_fpu;
		if (gamma_bits != 2) x_slow_fpu = !gamma_bits;
		else x_slow_fpu = slow_fpu == 1;

		last_val = 0;	/* against warning */

		/* 16-bit */
		ptr_16 = xmalloc(196608 * sizeof(*(cimg->gamma_table)));
		cimg->gamma_table=ptr_16;
		for (a=0;a<0x10000;a++,ptr_16++){
			if (!x_slow_fpu || !(a & 0xff))
				last_val = (unsigned short)(65535*powf((float)a*inv_65535,rg)+(float)0.5);
			*ptr_16 = last_val;
		}
		for (a=0;a<0x10000;a++,ptr_16++){
			if (!x_slow_fpu || !(a & 0xff))
				last_val = (unsigned short)(65535*powf((float)a*inv_65535,gg)+(float)0.5);
			*ptr_16 = last_val;
		}
		for (a=0;a<0x10000;a++,ptr_16++){
			if (!x_slow_fpu || !(a & 0xff))
				last_val = (unsigned short)(65535*powf((float)a*inv_65535,bg)+(float)0.5);
			*ptr_16 = last_val;
		}
	}
}

/* We assume unsigned short holds at least 16 bits. */
void agx_24_to_48_table(unsigned short *restrict dest, const unsigned char *restrict src, int
			  lenght, unsigned short *restrict table)
{
	for (;lenght;lenght--,src+=3,dest+=3)
	{
		dest[0]=table[src[0]];
		dest[1]=table[src[1]+256];
		dest[2]=table[src[2]+512];
	}
}

#if 0
/* Input is 0-255 (8-bit). Output is 0-255 (8-bit)*/
unsigned char ags_8_to_8(unsigned char input, float gamma)
{
	const float inv_255=1/255.;
	return 255*powf((float)input*inv_255,gamma)+(float)0.5;
}
#endif

/* Input is 0-255 (8-bit). Output is 0-65535 (16-bit)*/
/* We assume unsigned short holds at least 16 bits. */
unsigned short ags_8_to_16(unsigned char input, float gamma)
{
	float a=input;
	unsigned short retval;
	const float inv_255=(float)(1/255.);

	a*=inv_255;
	a=powf(a,gamma);
	a*=65535;
	retval = (unsigned short)(a+(float)0.5);
	return retval;
}

/* Input is 0-65535 (16-bit). Output is 0-255 (8-bit)*/
/* We assume unsigned short holds at least 16 bits. */
unsigned char ags_16_to_8(unsigned short input, float gamma)
{
	const float inv_65535=(float)(1/65535.);
	return (unsigned char)(powf((float)input*inv_65535,gamma)*255+(float)0.5);
}

/* Input is 0-65535 (16-bit). Output is 0-255 (8-bit)*/
unsigned short ags_16_to_16(unsigned short input, float gamma)
{
	unsigned short retval;
	const float inv_65535=(float)(1/65535.);

	retval = (unsigned short)(65535*powf((float)input*inv_65535,gamma)+(float)0.5);
	return retval;
}

/* Returns a pointer to a structure describing the letter found or NULL
 * if the letter is not found. Tries all possibilities in the style table
 * before returning NULL.
 */
static struct letter *find_stored_letter(int *style_table, int letter_number)
{
	int first, last, half, diff, font_index, font_number;

	for (font_index=n_fonts-1;font_index;font_index--)
	{
		font_number=*style_table++;

		first=font_table[font_number].begin;
		last=font_table[font_number].length+first-1;

		while(first<=last){
			half=(first+last)>>1;
			diff=letter_data[half].code-letter_number;
			if (diff>=0){
				if (diff==0){
					return letter_data+half;
				}else{
					/* Value in table is bigger */
					last=half-1;
				}
			}else{
				/* Value in the table is smaller */
				first=half+1;
			}
		}
	}

	/* 0 is system font, 0 is blotch char. This must be present
	 * or we segfault :) */
#ifdef REPORT_UNKNOWN
	fprintf(stderr,"letter 0x%04x not found\n",letter_number);
#endif /* #ifdef REPORT_UNKNOWN */
	return letter_data+font_table[0].begin;
}

struct read_work {
	const unsigned char *pointer;
	int length;
};

static void read_stored_data(png_structp png_ptr, png_bytep data, png_size_t length)
{
	struct read_work *work;

	length=(png_uint_32)length;

	work=png_get_io_ptr(png_ptr);
	if (length>(png_uint_32)work->length) png_error(png_ptr,"Ran out of input data");
	memcpy(data,work->pointer,length);
	work->length-=(int)length;
	work->pointer+=length;
}

static void my_png_warning(png_structp a, png_const_charp b)
{
}

static jmp_buf png_error_jump;
static int png_error_jump_valid = 0;

static void my_png_error(png_structp a, png_const_charp error_string)
{
	if (png_error_jump_valid)
		longjmp(png_error_jump, 1);
	error("Error when loading compiled-in font: %s.", error_string);
}

/* Loads width and height of the PNG (nothing is scaled). Style table is
 * already incremented.
 */
static void load_metric(int *x, int *y, int char_number, int *style_table)
{
	struct letter *l;

	l=find_stored_letter(style_table,char_number);
	if (!l){
		*x=0;
		*y=0;
	}else{
		*x=l->xsize;
		*y=l->ysize;
	}
	return;
}

#ifdef PNG_USER_MEM_SUPPORTED
void *my_png_alloc(png_structp png_ptr, png_size_t size)
{
	void *ptr = xmalloc(size);
	return ptr;
}
void my_png_free(png_structp png_ptr, void *ptr)
{
	free(ptr);
}
#endif

static void dip_set_gamma(png_structp png_ptr, png_infop info_ptr)
{
	png_error_jump_valid = 1;
	if (!setjmp(png_error_jump)) {
		double gamma;
		if (png_get_gAMA(png_ptr, info_ptr, &gamma))
			png_set_gamma(png_ptr, 1.0, gamma);
		else
			png_set_gamma(png_ptr, 1.0, sRGB_gamma);
	}
	png_error_jump_valid = 0;
}

/* The data tha fall out of this function express this: 0 is paper. 255 is ink. 34
 * is 34/255ink+(255-34)paper. No gamma is involved in this formula, as you can see.
 * The multiplications and additions take place in photon space.
 */
static void load_char(unsigned char **dest, int *x, int *y,
const unsigned char *png_data, int png_length)
{
	png_structp png_ptr;
	png_infop info_ptr;
	int y1,number_of_passes;
	unsigned char **ptrs;
	struct read_work work;

	work.pointer = png_data;
	work.length = png_length;

	retry1:
#ifdef PNG_USER_MEM_SUPPORTED
	png_ptr=png_create_read_struct_2(PNG_LIBPNG_VER_STRING,
			NULL, my_png_error, my_png_warning,
			NULL, my_png_alloc, my_png_free);
#else
	png_ptr=png_create_read_struct(PNG_LIBPNG_VER_STRING,
			NULL, my_png_error, my_png_warning);
#endif
	if (!png_ptr) {
		if (out_of_memory())
			goto retry1;
		fatal_exit("png_create_read_struct failed");
	}
	retry2:
	info_ptr=png_create_info_struct(png_ptr);
	if (!info_ptr) {
		if (out_of_memory())
			goto retry2;
		fatal_exit("png_create_info_struct failed");
	}
	png_set_read_fn(png_ptr, &work, (png_rw_ptr)read_stored_data);
	png_read_info(png_ptr, info_ptr);
	*x=(int)png_get_image_width(png_ptr,info_ptr);
	*y=(int)png_get_image_height(png_ptr,info_ptr);
	dip_set_gamma(png_ptr, info_ptr);
	{
		int bit_depth;
		int color_type;

		color_type=png_get_color_type(png_ptr, info_ptr);
		bit_depth=png_get_bit_depth(png_ptr, info_ptr);
		if (color_type==PNG_COLOR_TYPE_GRAY){
			if (bit_depth<8){
				 png_set_expand(png_ptr);
			}
			if (bit_depth==16){
				 png_set_strip_16(png_ptr);
			}
		}
		if (color_type==PNG_COLOR_TYPE_PALETTE){
			png_set_expand(png_ptr);
			png_set_rgb_to_gray(png_ptr, 1, -1, -1);
		}
		if (color_type & PNG_COLOR_MASK_ALPHA){
			png_set_strip_alpha(png_ptr);
		}
		if (color_type==PNG_COLOR_TYPE_RGB ||
			color_type==PNG_COLOR_TYPE_RGB_ALPHA){
			png_set_rgb_to_gray(png_ptr, 1, -1, -1);
		}
	}
	/* If the depth is different from 8 bits/gray, make the libpng expand
	 * it to 8 bit gray.
	 */
	number_of_passes=png_set_interlace_handling(png_ptr);
	png_read_update_info(png_ptr,info_ptr);
	if (*x && (unsigned)*x * (unsigned)*y / (unsigned)*x != (unsigned)*y) overalloc();
	if ((unsigned)*x * (unsigned)*y > INT_MAX) overalloc();
	*dest = xmalloc(*x * (*y));
	if ((unsigned)*y > INT_MAX / sizeof(*ptrs)) overalloc();
	ptrs = xmalloc(*y * sizeof(*ptrs));
	for (y1=0;y1<*y;y1++) ptrs[y1]=*dest+*x*y1;
	for (;number_of_passes;number_of_passes--){
		png_read_rows(png_ptr, ptrs, NULL, *y);
	}
	png_read_end(png_ptr, NULL);
	png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
	free(ptrs);
	return;
}

/* Like load_char, but we dictate the y.
 */
static void load_scaled_char(void **dest, int *x, int y,
const unsigned char *png_data, int png_length, struct style *style)
{
	unsigned char *interm;
	unsigned char *interm2;
	unsigned char *i2ptr,*dptr;
	int ix, iy, y0, x0, c;
	float conv0, conv1,sharpness,contrast;

	load_char(&interm, &ix,&iy,png_data, png_length);
	if (style->mono_space>=0)
		*x=compute_width(style->mono_space, style->mono_height, y);
	else
		*x=compute_width(ix,iy,y);
	if (display_optimize) *x*=3;
	scale_gray(interm, ix,iy, (unsigned char **)dest, *x, y);
	if (y>32||y<=0) return ; /* No convolution */
	ix=*x+2; /* There is one-pixel border around */
	iy=y+2;
	if (ix && (unsigned)ix * (unsigned)iy / (unsigned)ix != (unsigned)iy) overalloc();
	if ((unsigned)ix * (unsigned)iy > INT_MAX) overalloc();
	interm2 = xmalloc(ix * iy);
	i2ptr=interm2+ix+1;
	dptr=*dest;
	memset(interm2,0,ix);
	memset(interm2+(iy-1)*ix,0,ix);
	for (y0=y;y0;y0--){
		i2ptr[-1]=0;
		memcpy(i2ptr,dptr,*x);
		i2ptr[*x]=0;
		i2ptr+=ix;
		dptr+=*x;
	}
	i2ptr=interm2+ix+1;
	dptr=*dest;

	/* Determine the sharpness and contrast */
	sharpness=fancy_constants[2*y-2];
	contrast=fancy_constants[2*y-1];

	/* Compute the matrix constants from contrast and sharpness */
	conv0=(1+sharpness)*contrast;
	conv1=(float)(-sharpness*(float)0.25*contrast);

	for (y0=y;y0;y0--){
		for (x0=*x;x0;x0--){
			/* Convolution */
			c=(int)(((*i2ptr)*conv0)+i2ptr[-ix]*conv1+i2ptr[-1]*conv1+i2ptr[1]*conv1+i2ptr[ix]*conv1+(float)0.5);
			if (c & ~255) c=c<0?0:255;
			*dptr=(unsigned char)c;
			dptr++;
			i2ptr++;
		}
		i2ptr+=2;
	}
	free(interm2);
}

static struct font_cache_entry *locked_color_entry = NULL;

/* Adds required entry into font_cache and returns pointer to the entry.
 */
static struct font_cache_entry *supply_color_cache_entry(struct style *style, struct letter *letter)
{
	struct font_cache_entry *found, *neww;
	unsigned short *primary_data;
	unsigned short red, green, blue;
	unsigned bytes_consumed;

	found = xmalloc(sizeof(*found));
	found->bitmap.y=style->height;
	load_scaled_char(&(found->bitmap.data),&(found->bitmap.x),
		found->bitmap.y, letter->begin,
		letter->length, style);

	neww = xmalloc(sizeof(*neww));
	locked_color_entry = neww;
	neww->bitmap=found->bitmap;
	neww->r0=style->r0;
	neww->g0=style->g0;
	neww->b0=style->b0;
	neww->r1=style->r1;
	neww->g1=style->g1;
	neww->b1=style->b1;
	neww->mono_space=style->mono_space;
	neww->mono_height=style->mono_height;

	if (neww->bitmap.x && (unsigned)neww->bitmap.x * (unsigned)neww->bitmap.y / (unsigned)neww->bitmap.x != (unsigned)neww->bitmap.y) overalloc();
	if ((unsigned)neww->bitmap.x * (unsigned)neww->bitmap.y > INT_MAX / 3 / sizeof(*primary_data)) overalloc();
	primary_data = xmalloc(3 * neww->bitmap.x * neww->bitmap.y * sizeof(*primary_data));

	/* We assume the gamma of HTML styles is in sRGB space */
	round_color_sRGB_to_48(&red, &green, &blue,
		((style->r0)<<16)|((style->g0)<<8)|(style->b0));
	mix_two_colors(primary_data, found->bitmap.data,
		found->bitmap.x*found->bitmap.y,
		red,green,blue,
		ags_8_to_16(style->r1,(float)((float)user_gamma/(float)sRGB_gamma)),
		ags_8_to_16(style->g1,(float)((float)user_gamma/(float)sRGB_gamma)),
		ags_8_to_16(style->b1,(float)((float)user_gamma/(float)sRGB_gamma))
	);
	if (display_optimize){
		/* A correction for LCD */
		neww->bitmap.x/=3;
		decimate_3(&primary_data,neww->bitmap.x,neww->bitmap.y);
	}
	/* We have a buffer with photons */
	if (drv->get_empty_bitmap(&(neww->bitmap)))
		goto skip_dither;
	if (dither_letters)
		dither(primary_data, &(neww->bitmap));
	else
		(*round_fn)(primary_data,&(neww->bitmap));
	skip_dither:
	free(primary_data);
	drv->register_bitmap(&(neww->bitmap));

	free(found->bitmap.data);
	free(found);

	bytes_consumed=neww->bitmap.x*neww->bitmap.y*(drv->depth&7);
	/* Number of bytes per pixel in passed bitmaps */
	bytes_consumed+=(int)sizeof(*neww);
	bytes_consumed+=(int)sizeof(struct lru_entry);
	lru_insert(&font_cache, neww, &(letter->color_list),
		bytes_consumed);
	return neww;
}

static int destroy_font_cache_bottom(void)
{
	struct font_cache_entry *bottom;
	bottom=lru_get_bottom(&font_cache);
	if (!bottom) return 0;
	if (bottom == locked_color_entry) return 0;
	drv->unregister_bitmap(&(bottom->bitmap));
	free(bottom);
	lru_destroy_bottom(&font_cache);
	return 1;
}

/* Prunes the cache to comply with maximum size */
static int prune_font_cache(void)
{
	int r = 0;

	while (font_cache.bytes > (unsigned)font_cache_size) {
		if (destroy_font_cache_bottom()) {
			r = 1;
		} else {
			break;
		}
	}
	return r;
}

/* Prints a letter to the specified position and
 * returns the width of the printed letter */
static inline int print_letter(struct graphics_device *device, int x, int y, struct style *style, int char_number)

{
	int xw;
	struct font_cache_entry *found;
	struct font_cache_entry templat;
	struct letter *letter;

	/* Find a suitable letter */
	letter=find_stored_letter(style->table,char_number);
#ifdef DEBUG
	if (!letter) internal("print_letter could not find a letter - even not the blotch!");
#endif /* #ifdef DEBUG */
	templat.r0=style->r0;
	templat.r1=style->r1;
	templat.g0=style->g0;
	templat.g1=style->g1;
	templat.b0=style->b0;
	templat.b1=style->b1;
	templat.bitmap.y=style->height;
	templat.mono_space=style->mono_space;
	templat.mono_height=style->mono_height;

	found=lru_lookup(&font_cache, &templat, &letter->color_list);
	if (!found) found=supply_color_cache_entry(style, letter);
	else locked_color_entry = found;
	drv->draw_bitmap(device, &(found->bitmap), x, y);
	xw = found->bitmap.x;
	if (locked_color_entry != found) internal("bad letter lock");
	locked_color_entry = NULL;
	prune_font_cache();
	return xw;
}

/* Must return values that are:
 * >=0
 * <=height
 * at least 1 apart
 * Otherwise g_print_text will print nonsense (but won't segfault)
 */
static void get_underline_pos(int height, int *top, int *bottom)
{
	int thickness, baseline;
	thickness=(height+15)/16;
	baseline=height/7;
	if (baseline<=0) baseline=1;
	if (thickness>baseline) thickness=baseline;
	*top=height-baseline;
	*bottom=*top+thickness;
}

/* *width will be advanced by the width of the text */
void g_print_text(struct graphics_device *device, int x, int y, struct style *style, unsigned char *text, int *width)
{
	int original_flags, top_underline, bottom_underline, original_width,
		my_width;
	struct rect saved_clip;

	if (y+style->height<=device->clip.y1||y>=device->clip.y2) goto o;
	if (style->flags){
		/* Underline */
		if (!width){
		       width=&my_width;
		       *width=0;
		}
		original_flags=style->flags;
		original_width=*width;
		style->flags=0;
		get_underline_pos(style->height, &top_underline, &bottom_underline);
		restrict_clip_area(device, &saved_clip, 0, 0, device->size.x2, y+
			top_underline);
		g_print_text(device, x, y, style, text, width);
		drv->set_clip_area(device, &saved_clip);
		if (bottom_underline-top_underline==1){
			/* Line */
			drv->draw_hline(device, x, y+top_underline,
				safe_add(x,*width)-original_width,
				style->underline_color);
		}else{
			/* Area */
			drv->fill_area(device, x, y+top_underline,
					safe_add(x,*width)-original_width,
					y+bottom_underline,
					style->underline_color);
		}
		if (bottom_underline<style->height){
			/* Do the bottom half only if the underline is above
			 * the bottom of the letters.
			 */
			*width=original_width;
			restrict_clip_area(device, &saved_clip, 0,
				y+bottom_underline, device->size.x2,
				device->size.y2);
			g_print_text(device, x, y, style, text, width);
			drv->set_clip_area(device, &saved_clip);
		}
		style->flags=original_flags;
		return;
	}
	while (*text) {
		int p;
		int u;
		GET_UTF_8(text, u);
		/* 00-09, 0b-1f, 80, 81, 84, 86-9f ignorovat
		 * 0a = LF
		 * 82 = ' '
		 * 83 = nobrk
		 * 85 = radkovy zlom
		 * a0 = NBSP
		 * ad = soft hyphen
		 */
#if 0
		if (	(u>=0x00&&u<=0x09)||
			(u>=0x0b&&u<=0x1f)||
			u==0x80||
			u==0x82||
			u==0x84||
			(u>=0x86&&u<=0x9f)
		)continue;
		if (u==0x82)u=' ';
#endif
		/* stare Mikulasovo patchovani, musim to opravit    -- Brain */
		if (!u || u == 0xad) continue;
		if (u == 0x01 || u == 0xa0) u = ' ';
		p=print_letter(device,x,y,style, u);
		x += p;
		if (width) {
			*width = safe_add(*width, p);
			continue;
		}
		if (x>=device->clip.x2) return;
	}
	return;
	o:
	if (width) {
		int qw = g_text_width(style, text);
		*width = safe_add(*width, qw);
	}
}

/* 0=equality 1=inequality */
static int compare_font_entries(void *entry, void *templat)
{
	struct font_cache_entry*e1=entry;
	struct font_cache_entry*e2=templat;

	if (e1->r0 != e2->r0) return 1;
	if (e1->g0 != e2->g0) return 1;
	if (e1->b0 != e2->b0) return 1;
	if (e1->r1 != e2->r1) return 1;
	if (e1->g1 != e2->g1) return 1;
	if (e1->b1 != e2->b1) return 1;
	if (e1->bitmap.y != e2->bitmap.y) return 1;
	if (e1->mono_space != e2->mono_space) return 1;
	if (e1->mono_space >= 0) {
		if (e1->mono_height != e2->mono_height) return 1;
	}
	return 0;
}

/* If the cache already exists, it is destroyed and reallocated. If you call it with the same
 * size argument, only a cache flush will yield.
 */
static void init_font_cache(void)
{
	lru_init(&font_cache, &compare_font_entries);
}

/* Ensures there are no lru_entry objects allocated - destroys them.
 * Also destroys the bitmaps asociated with them. Does not destruct the
 font_cache per se.
 */
static void destroy_font_cache(void)
{
	while (destroy_font_cache_bottom())
		;
	if (lru_get_bottom(&font_cache))
		internal("destroy_font_cache: cache not freed due to locks");
}

/* Returns 0 in case the char is not found. */
static inline int g_get_width(struct style *style, unsigned charcode)
{
	int x, y, width;

	if (!charcode || charcode == 0xad) return 0;
	if (charcode == 0x01 || charcode == 0xa0) charcode = ' ';
	if (style->mono_space>=0){
		x=style->mono_space;
		y=style->mono_height;
	}else load_metric(&x,&y,charcode,style->table);
	if (!(x&&y)) width=0;
	else width=compute_width(x,y,style->height);
	return width;
}

int g_text_width(struct style *style, unsigned char *text)
{
	int w = 0;
	while (*text) {
		int u;
		int qw;
		GET_UTF_8(text, u);
		qw = g_get_width(style, u);
		w = safe_add(w, qw);
	}
	return w;
}

int g_char_width(struct style *style, unsigned charcode)
{
	return g_get_width(style, charcode);
}

int g_wrap_text(struct wrap_struct *w)
{
	unsigned char *init_text = w->text;
	while (*w->text) {
		int u;
		int s;
		unsigned char *l_text = w->text;
		if (*l_text == ' ') {
			w->last_wrap = l_text;
			w->last_wrap_obj = w->obj;
		}
		GET_UTF_8(w->text, u);
		if (!u) continue;
		s = g_get_width(w->style, u);
		w->pos = safe_add(w->pos, s);
		if (w->pos <= w->width) {
			c:
			if (u != 0xad || *w->text == ' ' || w->force_break) continue;
			s = g_char_width(w->style, '-');
			if (safe_add(w->pos, s) <= w->width || (!w->last_wrap && !w->last_wrap_obj)) {
				w->last_wrap = l_text;
				w->last_wrap_obj = w->obj;
				continue;
			}
		}
		if (w->force_break && !w->last_wrap && l_text != init_text) {
			w->last_wrap = l_text;
			w->last_wrap_obj = w->obj;
		}
		if (!w->last_wrap && !w->last_wrap_obj) goto c;
		return 0;
	}
	return 1;
}

void update_aspect(void)
{
	aspect=(int)(65536*bfu_aspect+0.5);
	destroy_font_cache();
}

int fontcache_info(int type)
{
	switch (type) {
		case CI_BYTES:
			return font_cache.bytes;
		case CI_FILES:
			return font_cache.items;
		default:
			internal("fontcache_info: query %d", type);
			return 0;
	}
}

static int shrink_font_cache(int u)
{
	int freed_something = 0;
	int has_something;
	if (u == SH_CHECK_QUOTA) {
		freed_something = prune_font_cache();
	}
	if (u == SH_FREE_ALL) {
		while (destroy_font_cache_bottom())
			freed_something = 1;
	}
	if (u == SH_FREE_SOMETHING) {
		freed_something = destroy_font_cache_bottom();
	}
	has_something = !!lru_get_bottom(&font_cache);
	return	(freed_something ? ST_SOMETHING_FREED : 0) |
		(has_something ? 0 : ST_CACHE_EMPTY);
}

void init_dip(void)
{
	init_font_cache();
	update_aspect();
	register_cache_upcall(shrink_font_cache, MF_GPI, cast_uchar "fontcache");
}

static void recode_font_name(unsigned char **name)
{
	int dashes=0;
	unsigned char *p;

	if (!strcmp(cast_const_char *name,"monospaced")) *name=cast_uchar "courier-medium-roman-serif-mono";
	if (!strcmp(cast_const_char *name,"monospace")) *name=cast_uchar "courier-medium-roman-serif-mono";
	else if (!strcmp(cast_const_char *name,"")) *name=cast_uchar "century_school-medium-roman-serif-vari";
	p=*name;
	while(*p){
		if (*p=='-')dashes++;
		p++;
	}
	if (dashes!=4) *name=cast_uchar "century_school-medium-roman-serif-vari";
}

/* Compares single=a multi=b-c-a as matching.
 * 0 matches
 * 1 doesn't match
 */
static int compare_family(unsigned char *single, unsigned char *multi)
{
	unsigned char *p,*r;
	int single_length=(int)strlen(cast_const_char single);

	r=multi;
	while(1){
		p=r;
		while (*r&&*r!='-')r++;
		if ((r-p==single_length)&&!strncmp(cast_const_char single,cast_const_char p,r-p)) return 0;
		if (!*r) break;
		r++;
	}
	return 1;
}

/* Input name must contain exactly 4 dashes, otherwise the
 * result is undefined (parsing into weight, slant, adstyl, spacing
 * will result deterministically random results).
 * Returns 1 if the font is monospaced or 0 if not.
 */
static int fill_style_table(int * table, unsigned char *name)
{
	unsigned char *p;
	unsigned char *family, *weight, *slant, *adstyl, *spacing;
	int pass,result,f;
	int masks[6]={0x1f,0x1f,0xf,0x7,0x3,0x1};
	int xors[6]={0,0x10,0x8,0x4,0x2,0x1};
	/* Top bit of the values belongs to family, bottom to spacing */
	int monospaced;

	/* Parse the name */
	recode_font_name(&name);
	family=stracpy(name);
	p=family;
	while(*p&&*p!='-') p++;
	*p=0;
	p++;
	weight=p;
	while(*p&&*p!='-') p++;
	*p=0;
	p++;
	slant=p;
	while(*p&&*p!='-') p++;
	*p=0;
	p++;
	adstyl=p;
	while(*p&&*p!='-') p++;
	*p=0;
	p++;
	spacing=p;
	monospaced=!strcmp(cast_const_char spacing,"mono");

	for (pass=0;pass<6;pass++){
		for (f=1;f<n_fonts;f++){
			/* Font 0 must not be int style_table */
			result=compare_family(family,font_table[f].family);
			result<<=1;
			result|=!!strcmp(cast_const_char weight,cast_const_char font_table[f].weight);
			result<<=1;
			result|=!!strcmp(cast_const_char slant,cast_const_char font_table[f].slant);
			result<<=1;
			result|=!!strcmp(cast_const_char adstyl,cast_const_char font_table[f].adstyl);
			result<<=1;
			result|=!!strcmp(cast_const_char spacing,cast_const_char font_table[f].spacing);
			result^=xors[pass];
			result&=masks[pass];
			if (!result) /* Fot complies */
				*table++=f;
		}
	}
	free(family);
	return monospaced;
}

struct style *g_invert_style(struct style *old)
{
	int length;

	struct style *st;
	st = xmalloc(sizeof(struct style));
	st->refcount=1;
	st->r0=old->r1;
	st->g0=old->g1;
	st->b0=old->b1;
	st->r1=old->r0;
	st->g1=old->g0;
	st->b1=old->b0;
	st->height=old->height;
	st->flags=old->flags;
	if (st->flags)
	{
		/* We have to get a foreground color for underlining */
		st->underline_color=dip_get_color_sRGB(
			(st->r1<<16)|(st->g1<<8)|(st->b1));
	}
	if ((unsigned)n_fonts > INT_MAX / sizeof(*st->table)) overalloc();
	length=(int)sizeof(*st->table)*(n_fonts-1);
	st->table = xmalloc(length);
	memcpy(st->table,old->table,length);
	st->mono_space=old->mono_space;
	st->mono_height=old->mono_height;
	return st;
}

int hack_rgb(int rgb)
{
	if (((drv->depth >> 3) & 0x1f) <= 4) {
		int r,g,b;
		r=(rgb>>16)&255;
		g=(rgb>>8)&255;
		b=rgb&255;
		if (r == g && g == b) {
			if (r < 128) return 0;
			else return 0xffffff;
		}
	}
	return rgb;
}

/* Never returns NULL. */
struct style *g_get_style(int fg, int bg, int size, unsigned char *font, int flags)
{
	struct style *st;

	bg = hack_rgb(bg);

	st = xmalloc(sizeof(struct style));
	/* strcpy(st->font, font); */
	st->refcount = 1;
	st->r0 = bg >> 16;
	st->g0 = (bg >> 8) & 255;
	st->b0 = bg & 255;
	st->r1 = fg >> 16;
	st->g1 = (fg >> 8) & 255;
	st->b1 = fg & 255;
	if (size<=0) size=1;
	st->height = size;
	st->flags=flags&FF_UNDERLINE;
	if (st->flags)
	{
		/* We have to get a foreground color for underlining */
		st->underline_color=dip_get_color_sRGB(fg);
	}
	if ((unsigned)n_fonts > INT_MAX / sizeof(*st->table)) overalloc();
	st->table = xmalloc(sizeof(*st->table)*(n_fonts-1));
	if(fill_style_table(st->table, font))
		load_metric(&(st->mono_space), &(st->mono_height),' ',st->table);
	else
		st->mono_space=-1;
	return st;
}

struct style *g_clone_style(struct style *st)
{
	st->refcount++;
	return st;
}

void g_free_style(struct style *st)
{
	if (--st->refcount) return;
	free(st->table);
	free(st);
}

long gamma_cache_color;
int gamma_cache_rgb = -2;

/* IEC 61966-2-1
 * Input gamma: sRGB space (directly from HTML, i. e. unrounded)
 * Output: color index for graphics driver that is closest to the
 * given sRGB value.
 * We assume unsigned short holds at least 16 bits. */
long real_dip_get_color_sRGB(int rgb)
{
	unsigned short r,g,b;
	int new_rgb;

	rgb = hack_rgb(rgb);

	round_color_sRGB_to_48(&r,&g,&b,rgb);
	r=ags_16_to_8(r,(float)(1/(float)display_red_gamma));
	g=ags_16_to_8(g,(float)(1/(float)display_green_gamma));
	b=ags_16_to_8(b,(float)(1/(float)display_blue_gamma));
	new_rgb=b|(g<<8)|(r<<16);
	gamma_cache_rgb = rgb;
	/* The get_color takes values with gamma of display_*_gamma */
	return gamma_cache_color = drv->get_color(new_rgb);
}

void get_links_icon(char **data, int *width, int *height, int *skip, int pad)
{
	struct bitmap b;

	b.x=48;
	b.y=48;
	*width=b.x;
	*height=b.y;
	b.skip=b.x*(drv->depth&7);
	while (b.skip % pad) b.skip++;
	*skip=b.skip;
	b.data = *data = xmalloc(b.skip * b.y);
}

static inline void qb_palette(unsigned r_max, unsigned g_max, unsigned b_max, unsigned r, unsigned g, unsigned b, unsigned scale, unsigned rgb[3])
{
	rgb[0] = (r * scale + r_max / 2) / r_max;
	rgb[1] = (g * scale + g_max / 2) / g_max;
	rgb[2] = (b * scale + b_max / 2) / b_max;
}

void q_palette(unsigned size, unsigned color, unsigned scale, unsigned rgb[3])
{
	switch (size) {
		case 16:
			qb_palette(1, 3, 1, color >> 3, (color >> 1) & 3, color & 1, scale, rgb);
			break;
		case 256:
			qb_palette(7, 7, 3, color >> 5, (color >> 2) & 7, color & 3, scale, rgb);
			break;
		case 32768:
			qb_palette(31, 31, 31, color >> 10, (color >> 5) & 31, color & 31, scale, rgb);
			break;
		case 65536:
			qb_palette(31, 63, 31, color >> 11, (color >> 5) & 63, color & 31, scale, rgb);
			break;
		case 16777216:
			qb_palette(255, 255, 255, color >> 16, (color >> 8) & 255, color & 255, scale, rgb);
			break;
		default:
			internal("q_palette: invalid size %u", size);
	}
}

double rgb_distance(int r1, int g1, int b1, int r2, int g2, int b2)
{
	double diff_r, diff_g, diff_b;
	diff_r = (r1 - r2) * .30;
	diff_r *= diff_r;
	diff_g = (g1 - g2) * .59;
	diff_g *= diff_g;
	diff_b = (b1 - b2) * .11;
	diff_b *= diff_b;
	return diff_r + diff_g + diff_b;
}

#endif /* G */
