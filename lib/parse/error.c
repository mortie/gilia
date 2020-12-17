#include "parse/parse.h"

#include <stdio.h>
#include <stdarg.h>

void l2_parse_err(struct l2_parse_error *err, struct l2_token *tok, const char *fmt, ...) {
	err->line = tok->line;
	err->ch = tok->ch;
	char buf[256];

	va_list va;
	va_start(va, fmt);
	int n = vsnprintf(buf, sizeof(buf), fmt, va);

	if (n < 0) {
		const char *message = "Failed to generate error message!";
		err->message = malloc(strlen(message) + 1);
		strcpy(err->message, message);
		va_end(va);
		return;
	} else if (n + 1 < sizeof(buf)) {
		err->message = malloc(n + 1);
		strcpy(err->message, buf);
		va_end(va);
		return;
	}

	// Need to allocate for this one
	err->message = malloc(n + 1);
	vsnprintf(err->message, n + 1, fmt, va);
	va_end(va);
}

void l2_parse_error_free(struct l2_parse_error *err) {
	free(err->message);
}
