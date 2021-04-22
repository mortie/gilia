#include "parse/parse.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parse/lex.h"
#include "trace.h"

void gil_parse_err(struct gil_parse_error *err, struct gil_token *tok, const char *fmt, ...) {
	err->line = tok->line;
	err->is_static = 0;
	err->ch = tok->ch;

	if (gil_token_get_kind(tok) == GIL_TOK_ERROR) {
		gil_trace("Error token: %s", tok->v.str);
		err->message = tok->v.str;
		err->is_static = 1;
		return;
	}

	char buf[256];
	va_list va;
	va_start(va, fmt);
	int n = vsnprintf(buf, sizeof(buf), fmt, va);

	if (n < 0) {
		err->message = "Failed to generate error message!";
		err->is_static = 1;

		va_end(va);
		gil_trace("Parse error: %s", err->message);
		return;
	} else if ((size_t)n + 1 < sizeof(buf)) {
		err->message = malloc(n + 1);
		if (err->message == NULL) {
			err->message = "Failed to allocate error message!";
			err->is_static = 1;
		} else {
			strcpy(err->message, buf);
		}

		va_end(va);
		gil_trace("Parse error: %s", err->message);
		return;
	}

	// Need to allocate for this one
	err->message = malloc(n + 1);
	if (err->message == NULL) {
		err->message = "Failed to allocate error message!";
		err->is_static = 1;
	} else {
		vsnprintf(err->message, n + 1, fmt, va);
	}

	va_end(va);
	gil_trace("Parse error: %s", err->message);
}

void gil_parse_error_free(struct gil_parse_error *err) {
	if (!err->is_static) {
		free(err->message);
	}
}
