#ifndef L2_PARSE_H
#define L2_PARSE_H

#include "lex.h"
#include "../io.h"
#include "../strset.h"

struct l2_parse_state {
	struct l2_lexer *lexer;
	struct l2_bufio_writer writer;
	struct l2_strset atoms;
	struct l2_strset strings;
};

void l2_parse_init(
		struct l2_parse_state *state,
		struct l2_lexer *lexer, struct l2_io_writer *w);

void l2_parse_free(struct l2_parse_state *state);

void l2_parse_program(struct l2_parse_state *state);

#endif
