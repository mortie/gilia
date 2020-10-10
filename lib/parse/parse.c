#include "parse/parse.h"

#include "gen/gen.h"

static void parse_expression(struct l2_lexer *lexer, struct l2_generator *gen) {
	struct l2_token *tok = l2_lexer_peek(lexer, 1);
	struct l2_token *tok2 = l2_lexer_peek(lexer, 2);

	if (tok->kind == L2_TOK_IDENT && tok2->kind == L2_TOK_COLON_EQ) {
		parse_expression(lexer, gen);
		l2_gen_assignment(gen, &tok->v.str);
	}
}

void l2_parse_program(struct l2_lexer *lexer, struct l2_generator *gen) {
	l2_gen_stack_frame(gen);

	while (1) {
		struct l2_token *tok = l2_lexer_peek(lexer, 1);
		if (tok->kind == L2_TOK_EOF) {
			break;
		}

		parse_expression(lexer, gen);
	}
}
