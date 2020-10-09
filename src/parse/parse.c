#include "parse/parse.h"

#include "gen/gen.h"

void l2_parse_program(struct l2_parse_state *state) {
	l2_gen_stack_frame(&state->writer);
}
