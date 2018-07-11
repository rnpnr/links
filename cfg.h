/* cfg.h
 * (c) 2002 Mikulas Patocka
 * This file is a part of the Links program, released under GPL.
 */

#ifndef CFG_H
#define CFG_H
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#ifdef HAVE_CONFIG2_H
#include "config2.h"
#endif

#if defined(G)
#define HAVE_SETJMP_H 1
#endif

#define my_restrict	__restrict

typedef int cfg_h_no_empty_unit;

#endif
