#include "parse/parse.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parse/lex.h"
#include "str.h"

#define GIL_TRACER_NAME "parser"
#include "trace.h"

void gil_parse_err(struct gil_parse_error *err, struct gil_token *tok, const char *fmt, ...) {
	err->line = tok->line;
	err->is_static = 0;
	err->ch = tok->ch;

	if (gil_token_get_kind(tok) == GIL_TOK_ERROR) {
		gil_trace("error token: %s", tok->v.str);
		err->message = tok->v.str;
		err->is_static = 1;
		return;
	}

	va_list va;
	va_start(va, fmt);
	err->message = gil_vstrf(fmt, va);
	va_end(va);

	if (err->message == NULL) {
		err->message = "Failed to create error message";
		err->is_static = 1;
	}

	gil_trace("error: %s", err->message);
}

void gil_parse_error_free(struct gil_parse_error *err) {
	if (!err->is_static) {
		free(err->message);
	}
}
