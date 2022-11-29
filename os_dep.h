/* os_dep.h
 * (c) 2002 Mikulas Patocka
 * This file is a part of the Links program, released under GPL.
 */

static inline int
dir_sep(unsigned char x)
{
	return x == '/';
}
#define SYSTEM_ID         1
#define SYSTEM_NAME       "Unix"
#define SHARED_CONFIG_DIR "/etc/"
