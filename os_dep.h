/* os_dep.h
 * (c) 2002 Mikulas Patocka
 * This file is a part of the Links program, released under GPL.
 */

#ifndef OS_DEP_H
#define OS_DEP_H

#define SYS_UNIX	1
#define SYS_OS2		2
#define SYS_WIN_32	3
#define SYS_BEOS	4
#define SYS_RISCOS	5
#define SYS_ATHEOS	6
#define SYS_SPAD	7
#define SYS_INTERIX	8
#define SYS_OPENVMS	9
#define SYS_DOS		10

/* hardcoded limit of 10 OSes in default.c */

#ifdef UNIX
#undef UNIX
#endif
#define UNIX

#if defined(UNIX)

static inline int dir_sep(unsigned char x) { return x == '/'; }
#define SYSTEM_ID SYS_UNIX
#define SYSTEM_NAME "Unix"
#define DEFAULT_SHELL "/bin/sh"
#define GETSHELL getenv("SHELL")
#define SHARED_CONFIG_DIR "/etc/"

#endif

#endif /* #ifndef OS_DEP_H */
