#include "parse/parse.h"

#include "gen/gen.h"

void l2_parse_init(
		struct l2_parse_state *state,
		struct l2_lexer *lexer, struct l2_io_writer *w) {
	state->lexer = lexer;
	l2_bufio_writer_init(&state->writer, w);
	l2_strset_init(&state->atoms);
	l2_strset_init(&state->strings);
}

void l2_parse_free(struct l2_parse_state *state) {
	l2_strset_free(&state->atoms);
	l2_strset_free(&state->strings);
}

void l2_parse_program(struct l2_parse_state *state) {
	l2_gen_stack_frame(&state->writer);
}
