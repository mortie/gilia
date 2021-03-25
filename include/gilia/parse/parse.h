#ifndef GIL_PARSE_H
#define GIL_PARSE_H

#include "lex.h"
#include "gen/gen.h"

struct gil_parse_error {
	int line;
	int ch;
	int is_static;
	char *message;
};

void gil_parse_err(struct gil_parse_error *err, struct gil_token *tok, const char *fmt, ...);
void gil_parse_error_free(struct gil_parse_error *err);

int gil_parse_program(
		struct gil_lexer *lexer, struct gil_generator *gen, struct gil_parse_error *err);

#endif
