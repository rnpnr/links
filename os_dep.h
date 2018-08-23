/* os_dep.h
 * (c) 2002 Mikulas Patocka
 * This file is a part of the Links program, released under GPL.
 */

#ifndef OS_DEP_H
#define OS_DEP_H

/* hardcoded limit of 10 OSes in default.c */

#ifdef UNIX
#undef UNIX
#endif
#define UNIX

#if defined(UNIX)

static inline int dir_sep(unsigned char x) { return x == '/'; }
#define SYSTEM_ID 1
#define SYSTEM_NAME "Unix"
#define DEFAULT_SHELL "/bin/sh"
#define GETSHELL getenv("SHELL")
#define SHARED_CONFIG_DIR "/etc/"

#endif

#endif /* #ifndef OS_DEP_H */
