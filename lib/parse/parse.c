#include "parse/parse.h"

#include "gen/gen.h"

static int parse_expression(
		struct l2_lexer *lexer, struct l2_generator *gen, struct l2_parse_error *err) {
	struct l2_token *tok = l2_lexer_peek(lexer, 1);
	struct l2_token *tok2 = l2_lexer_peek(lexer, 2);

	if (tok->kind == L2_TOK_IDENT && tok2->kind == L2_TOK_COLON_EQ) {
		char *ident = l2_token_extract_str(tok);
		l2_lexer_consume(lexer); // ident
		l2_lexer_consume(lexer); // :=

		if (parse_expression(lexer, gen, err) < 0) {
			free(ident);
			return -1;
		}

		l2_gen_assignment(gen, &ident);
		return 0;
	} else if (tok->kind == L2_TOK_NUMBER) {
		l2_gen_number(gen, tok->v.num);
		l2_lexer_consume(lexer); // number
		return 0;
	} else if (tok->kind == L2_TOK_IDENT) {
		char *ident = l2_token_extract_str(tok);
		l2_lexer_consume(lexer); // ident
		l2_gen_namespace_lookup(gen, &ident);
		return 0;
	}

	l2_parse_err(err, tok, "In expression: Unexpected tokens %s, %s",
			l2_token_kind_name(tok->kind), l2_token_kind_name(tok2->kind));
	return -1;
}

int l2_parse_program(
		struct l2_lexer *lexer, struct l2_generator *gen, struct l2_parse_error *err) {
	l2_gen_stack_frame(gen);

	while (1) {
		struct l2_token *tok = l2_lexer_peek(lexer, 1);
		if (tok->kind == L2_TOK_EOF) {
			break;
		}

		if (parse_expression(lexer, gen, err) < 0) {
			l2_gen_halt(gen);
			l2_gen_flush(gen);
			return -1;
		}
	}

	l2_gen_halt(gen);
	l2_gen_flush(gen);
	return 0;
}
