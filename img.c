/* img.c
 * Generic image decoding and PNG and JPG decoders.
 * (c) 2002 Karel 'Clock' Kulhavy
 * This is a part of the Links program, released under GPL.

 * Used in graphics mode of Links only
 TODO: odstranit zbytecne ditherovani z strip_optimized header_dimensions_known,
       protoze pozadi obrazku musi byt stejne jako pozadi stranky, a to se nikdy
       neditheruje, protoze je to vzdy jednolita barva. Kdyz uz to nepujde
       odstranit tak tam aspon dat fixne zaokrouhlovani.
 TODO: dodelat stripy do jpegu a png a tiff.
 */

#include "links.h"

int
known_image_type(unsigned char *type)
{
	return 0;
}
