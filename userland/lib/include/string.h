#ifndef LIBC_STRING_H
# define LIBC_STRING_H

# include <stdint.h>

# ifndef NULL
#  define NULL ((void *)0)
# endif /* !NULL */

size_t strlen(const char *s);

char *strcpy(char *dest, const char *src);
int strcmp(const char *str1, const char *str2);

void *memcpy(void *dest, const void *src, size_t size);
void *memset(void *ptr, int value, size_t num);

char *strtok_r(char *str, const char *delim, char **saveptr);

#endif /* !LIBC_STRING_H */