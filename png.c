/* png.c
 * PNG decoding
 * (c) 2002 Karel 'Clock' Kulhavy
 * This is a part of the Links program, released under GPL.
 */
#ifdef G
#include "links.h"

/* Decoder structs */

struct png_decoder{
	png_structp png_ptr;
	png_infop info_ptr;
};

/* Warning for from-web PNG images */
static void img_my_png_warning(png_structp a, png_const_charp b)
{
}

/* Error for from-web PNG images. */
static void img_my_png_error(png_structp png_ptr, png_const_charp error_string)
{
#if (PNG_LIBPNG_VER < 10500)
	longjmp(png_ptr->jmpbuf,1);
#else
	png_longjmp(png_ptr,1);
#endif
}

static void png_info_callback(png_structp png_ptr, png_infop info_ptr)
{
	int bit_depth, color_type, intent;
	double gamma;
	unsigned char bytes_per_pixel=3;
	struct cached_image *cimg;

	cimg=global_cimg;

	bit_depth=png_get_bit_depth(png_ptr, info_ptr);
	color_type=png_get_color_type(png_ptr, info_ptr);
	if (color_type == PNG_COLOR_TYPE_PALETTE)
		png_set_expand(png_ptr);
	if (color_type == PNG_COLOR_TYPE_GRAY &&
		bit_depth < 8) png_set_expand(png_ptr);
	if (png_get_valid(png_ptr, info_ptr,
		PNG_INFO_tRNS)){
		png_set_expand(png_ptr); /* Legacy version of
		png_set_tRNS_to_alpha(png_ptr); */
		bytes_per_pixel++;
	}
	if (color_type == PNG_COLOR_TYPE_GRAY ||
		color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
		png_set_gray_to_rgb(png_ptr);
	if (bit_depth==16){
		/* We use native endianity only if unsigned short is 2-byte
		 * because otherwise we have to reassemble the buffer so we
		 * will leave in the libpng-native big endian.
		 */
		png_set_swap(png_ptr);
		bytes_per_pixel*=(int)sizeof(unsigned short);
	}
	png_set_interlace_handling(png_ptr);
	if (color_type==PNG_COLOR_TYPE_RGB_ALPHA
		||color_type==PNG_COLOR_TYPE_GRAY_ALPHA){
		if (bytes_per_pixel==3
			||bytes_per_pixel==3*sizeof(unsigned short))
			bytes_per_pixel=4*bytes_per_pixel/3;
	}
	cimg->width=(int)png_get_image_width(png_ptr,info_ptr);
	cimg->height=(int)png_get_image_height(png_ptr,info_ptr);
	cimg->buffer_bytes_per_pixel=bytes_per_pixel;
	if (png_get_sRGB(png_ptr, info_ptr, &intent)){
		gamma=sRGB_gamma;
	}
	else
	{
		if (!png_get_gAMA(png_ptr, info_ptr, &gamma)){
			gamma=sRGB_gamma;
		}
	}
	if (gamma < 0.01 || gamma > 100)
		gamma = sRGB_gamma;
	cimg->red_gamma=(float)gamma;
	cimg->green_gamma=(float)gamma;
	cimg->blue_gamma=(float)gamma;
	png_read_update_info(png_ptr,info_ptr);
	cimg->strip_optimized=0;
	if (header_dimensions_known(cimg))
		img_my_png_error(png_ptr, "bad image size");
}

static void
png_row_callback(png_structp png_ptr, png_bytep new_row, png_uint_32 row_num, int pass)
{
	struct cached_image *cimg;

	cimg=global_cimg;
	{
		png_progressive_combine_row(png_ptr,
			cimg->buffer+cimg->buffer_bytes_per_pixel
			*cimg->width*row_num, new_row);
	}
	cimg->rows_added=1;
}

static void png_end_callback(png_structp png_ptr, png_infop info)
{
	end_callback_hit=1;
}

/* Decoder structs */

void png_start(struct cached_image *cimg)
{
	png_structp png_ptr;
	png_infop info_ptr;
	struct png_decoder *decoder;

	retry1:
#ifdef PNG_USER_MEM_SUPPORTED
	png_ptr=png_create_read_struct_2(PNG_LIBPNG_VER_STRING,
			NULL, img_my_png_error, img_my_png_warning,
			NULL, my_png_alloc, my_png_free);
#else
	png_ptr=png_create_read_struct(PNG_LIBPNG_VER_STRING,
			NULL, img_my_png_error, img_my_png_warning);
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
	if (setjmp(png_jmpbuf(png_ptr))){
error:
		png_destroy_read_struct(&png_ptr, &info_ptr,
			(png_infopp)NULL);
		img_end(cimg);
		return;
	}
	png_set_progressive_read_fn(png_ptr, NULL,
				    png_info_callback, &png_row_callback,
				    png_end_callback);
	if (setjmp(png_jmpbuf(png_ptr))) goto error;
	decoder = xmalloc(sizeof(*decoder));
	decoder->png_ptr=png_ptr;
	decoder->info_ptr=info_ptr;
	cimg->decoder=decoder;
}

void png_restart(struct cached_image *cimg, unsigned char *data, int length)
{
	png_structp png_ptr;
	png_infop info_ptr;
	volatile int h;

	h = close_stderr();
	png_ptr=((struct png_decoder *)(cimg->decoder))->png_ptr;
	info_ptr=((struct png_decoder *)(cimg->decoder))->info_ptr;
	end_callback_hit=0;
	if (setjmp(png_jmpbuf(png_ptr))) {
		restore_stderr(h);
		img_end(cimg);
		return;
	}
	png_process_data(png_ptr, info_ptr, data, length);
	restore_stderr(h);
	if (end_callback_hit) img_end(cimg);
}

void png_destroy_decoder(struct cached_image *cimg)
{
	struct png_decoder *decoder = (struct png_decoder *)cimg->decoder;
	png_destroy_read_struct(&decoder->png_ptr, &decoder->info_ptr, NULL);
}

void add_png_version(unsigned char **s, int *l)
{
	add_to_str(s, l, cast_uchar "PNG (");
	add_to_str(s, l, cast_uchar png_get_libpng_ver(NULL));
	add_chr_to_str(s, l, ')');
}

#endif /* #ifdef G */
