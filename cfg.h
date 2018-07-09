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

#if !defined(G) && DEBUGLEVEL >= 0 && defined(HAVE_SETJMP_H)
#undef HAVE_SETJMP_H
#endif

#ifndef HAVE_VOLATILE
#define volatile
#endif

#if defined(HAVE_RESTRICT)
#define my_restrict	restrict
#elif defined(HAVE___RESTRICT)
#define my_restrict	__restrict
#else
#define my_restrict
#endif

typedef int cfg_h_no_empty_unit;

#endif
