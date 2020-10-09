#ifndef L2_PARSE_H
#define L2_PARSE_H

#include "lex.h"
#include "../io.h"

struct l2_parse_state {
	struct l2_lexer *lexer;
	struct l2_bufio_writer writer;
};

void l2_parse_program(struct l2_parse_state *state);

#endif
