/* os_depx.h
 * (c) 2002 Mikulas Patocka
 * This file is a part of the Links program, released under GPL.
 */

#ifdef HAVE_VALUES_H
#include <values.h>
#endif

#ifndef MAXINT
#ifdef INT_MAX
#define MAXINT INT_MAX
#else
#define MAXINT ((int)((unsigned int)-1 >> 1))
#endif
#endif

#define MAX_SIZE_T	((size_t)-1)

#ifdef HAVE_MAXINT_CONVERSION_BUG
#undef MAXINT
#define MAXINT		0x7FFFF000
#undef MAX_SIZE_T
#define MAX_SIZE_T	((size_t)-0x1000)
#endif

#ifndef SEEK_SET
#ifdef L_SET
#define SEEK_SET	L_SET
#else
#define SEEK_SET	0
#endif
#endif
#ifndef SEEK_CUR
#ifdef L_INCR
#define SEEK_CUR	L_INCR
#else
#define SEEK_CUR	1
#endif
#endif
#ifndef SEEK_END
#ifdef L_XTND
#define SEEK_END	L_XTND
#else
#define SEEK_END	2
#endif
#endif

#ifndef S_ISUID
#define S_ISUID		04000
#endif
#ifndef S_ISGID
#define S_ISGID		02000
#endif
#ifndef S_ISVTX
#define S_ISVTX		01000
#endif
#ifndef S_IRUSR
#define S_IRUSR		00400
#endif
#ifndef S_IWUSR
#define S_IWUSR		00200
#endif
#ifndef S_IXUSR
#define S_IXUSR		00100
#endif
#ifndef S_IRGRP
#define S_IRGRP		00040
#endif
#ifndef S_IWGRP
#define S_IWGRP		00020
#endif
#ifndef S_IXGRP
#define S_IXGRP		00010
#endif
#ifndef S_IROTH
#define S_IROTH		00004
#endif
#ifndef S_IWOTH
#define S_IWOTH		00002
#endif
#ifndef S_IXOTH
#define S_IXOTH		00001
#endif

#if !defined(S_IFMT) && defined(_S_IFMT)
#define S_IFMT		_S_IFMT
#endif

#if !defined(S_ISREG) && defined(S_IFMT) && defined(S_IFREG)
#define S_ISREG(mode)	(((mode) & S_IFMT) == S_IFREG)
#endif

#if !defined(S_ISDIR) && defined(S_IFMT) && defined(S_IFDIR)
#define S_ISDIR(mode)	(((mode) & S_IFMT) == S_IFDIR)
#endif

#if !defined(S_ISBLK) && defined(S_IFMT) && defined(S_IFBLK)
#define S_ISBLK(mode)	(((mode) & S_IFMT) == S_IFBLK)
#endif

#if !defined(S_ISCHR) && defined(S_IFMT) && defined(S_IFCHR)
#define S_ISCHR(mode)	(((mode) & S_IFMT) == S_IFCHR)
#endif

#if !defined(S_ISLNK) && defined(S_IFMT) && defined(S_IFLNK)
#define S_ISLNK(mode)	(((mode) & S_IFMT) == S_IFLNK)
#endif

#if !defined(S_ISSOCK) && defined(S_IFMT) && defined(S_IFSOCK)
#define S_ISSOCK(mode)	(((mode) & S_IFMT) == S_IFSOCK)
#endif

#ifndef O_NOCTTY
#define O_NOCTTY	0
#endif

#ifndef SIG_ERR
#define SIG_ERR		((int (*)())-1)
#endif

#ifndef SA_RESTART
#define SA_RESTART	0
#endif

#ifndef PF_INET
#define PF_INET AF_INET
#endif
#ifndef PF_UNIX
#define PF_UNIX AF_UNIX
#endif

#define my_intptr_t long
#define my_uintptr_t unsigned long

#ifndef PF_INET6
#define PF_INET6 AF_INET6
#endif

#if !defined(INET_ADDRSTRLEN)
#define INET_ADDRSTRLEN		16
#endif
#if !defined(INET6_ADDRSTRLEN)
#define INET6_ADDRSTRLEN	46
#endif

#ifdef G
#define float_double float
#define fd_pow powf
#endif

#define static_const	static const

typedef const char *const_char_ptr;

#if defined(HAVE_PTHREADS)
#define EXEC_IN_THREADS
#endif
