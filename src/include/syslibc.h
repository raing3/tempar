#ifndef SYSLIBC_H
#define SYSLIBC_H

#include <stdarg.h>

typedef struct {
	char *buf;
	const size_t *len;
	size_t cpylen;
} xprintf_ctx;

int vsnprintf(char *buf, size_t n, const char *fmt, va_list ap);

int snprintf(char *buf, size_t n, const char *fmt, ...);

int strcasecmp(const char *s1, const char *s2);

#endif