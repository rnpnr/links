/* n is in bytes. dest must begin on pixel boundary. If n is not a whole number
 * of pixels, rounding is performed downwards.
 * if bmpixelsize is 1, no alignment is required.
 * if bmpixelsize is 2, dest must be aligned to 2 bytes.
 * if bmpixelsize is 3, no alignment is required.
 * if bmpixelsize is 4, dest must be aligned to 4 bytes.
 * -- The following do not occur, this is only for forward compatibility.
 * if bmpixelsize is 5, no alignment is required.
 * if bmpixelsize is 6, dest must be aligned to 2 bytes.
 * if bmpixelsize is 7, no alignment is required.
 * if bmpixelsize is 8, dest must be aligned to 8 bytes.
 */

static inline void pixel_set(unsigned char *dest, int n,void * pattern)
{
	switch(fb_pixelsize)
	{
		case 1:
		memset(dest,*(unsigned char *)pattern,n);
		break;

		case 2:
		{
			unsigned short v=*(unsigned short *)do_not_optimize_here(pattern);
			/* ICC has an optimization bug here */
			icc_volatile int a;

			if ((v & 255) == ((v >> 8) & 255)) {
				memset(dest, v, n);
			} else {
				unsigned long vvvv=((unsigned long)v << 48) | ((unsigned long)v << 32) | ((unsigned long)v << 16) | v;
				a = n >> 1;
				while (a) {
					if (!((unsigned long)dest & 7) && a >= 4) {
						do {
							*((unsigned long *)dest) = vvvv;
							dest += 8;
							a -= 4;
						} while (a >= 4);
					} else {
						*((unsigned short *)dest) = v;
						dest += 2;
						a--;
					}
				}
			}
		}
		break;

		case 3:
		{
			unsigned char a,b,c;
			int i;

			a=*(unsigned char*)pattern;
			b=((unsigned char*)pattern)[1];
			c=((unsigned char*)pattern)[2];
			if (a == b && b == c) memset(dest, a, n);
			else for (i=0;i<=n-3;i+=3){
				dest[i]=a;
				dest[i+1]=b;
				dest[i+2]=c;
			}
		}
		break;

		case 4:
		if (((unsigned char *)pattern)[1] == ((unsigned char *)pattern)[2] &&
		    ((unsigned char *)pattern)[1] == ((unsigned char *)pattern)[drv->depth & ~255 ? 3 : 0]) {
			memset(dest, ((unsigned char *)pattern)[1], n);
		} else {
			unsigned v=*(unsigned *)do_not_optimize_here(pattern);
			/* ICC has an optimization bug here */
			icc_volatile int a;

			{
				unsigned long vv = ((unsigned long)v << 32) | v;
				a = n >> 2;
				while (a) {
					if (!((unsigned long)dest & 7) && a >= 2) {
						do {
							*((unsigned long *)dest) = vv;
							dest += 8;
							a -= 2;
						} while (a >= 2);
					} else {
						*(unsigned *)dest = v;
						dest += 4;
						a--;
					}
				}
			}
		}
		break;

#if 0
		default:
		{
			int a;
			for (a=0;a<n/fb_pixelsize;a++,dest+=fb_pixelsize) memcpy(dest,pattern,fb_pixelsize);
		}
		break;
#endif
	}
}

