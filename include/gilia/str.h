#ifndef GIL_STR_H
#define GIL_STR_H

#include <stdarg.h>

#ifdef __GNUC__
__attribute__((format(printf, 1, 2)))
#endif
char *gil_strf(const char *fmt, ...);
char *gil_vstrf(const char *fmt, va_list va);

#endif
