#ifndef L2_PARSE_H
#define L2_PARSE_H

#include "lex.h"
#include "gen/gen.h"

struct l2_parse_error {
	int line;
	int ch;
	char *message;
};

void l2_parse_err(struct l2_parse_error *err, struct l2_token *tok, const char *fmt, ...);
void l2_parse_error_free(struct l2_parse_error *err);

int l2_parse_program(
		struct l2_lexer *lexer, struct l2_generator *gen, struct l2_parse_error *err);

#endif
