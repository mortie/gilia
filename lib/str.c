#include "str.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

char *gil_strf(const char *fmt, ...) {
	va_list va;
	va_start(va, fmt);
	char *str = gil_vstrf(fmt, va);
	va_end(va);
	return str;
}

char *gil_vstrf(const char *fmt, va_list va) {
	char buf[1024];
	int n = vsnprintf(buf, sizeof(buf), fmt, va);
	if (n < 0) {
		return NULL;
	}

	// If the buffer was big enough, just use that
	if ((size_t)n + 1 < sizeof(buf)) {
		char *str = malloc((size_t)n + 1);
		if (str == NULL) {
			return NULL;
		} else {
			strcpy(str, buf);
			return str;
		}
	}

	// Otherwise, we have to allocate more space
	char *str = malloc((size_t)n + 1);
	if (str == NULL) {
		return NULL;
	}

	n = vsnprintf(str, (size_t)n + 1, fmt, va);
	if (n < 0) {
		free(str);
		return NULL;
	}

	return str;
}
