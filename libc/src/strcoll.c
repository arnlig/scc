/* See LICENSE file for copyright and license details. */

#include <string.h>

int
strcoll(const char *s1, const char *s2)
{
	while (*s1 && *s2 && *s1 != *s2)
		++s1, ++s2;
	return *(unsigned char *) s1 - *(unsigned char *) s2;
}