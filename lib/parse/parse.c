#include "parse/parse.h"

#include "trace.h"
#include "gen/gen.h"

static int tok_is_end(struct l2_token *tok) {
	return
		tok->kind == L2_TOK_CLOSE_BRACE || tok->kind == L2_TOK_CLOSE_BRACKET ||
		tok->kind == L2_TOK_CLOSE_PAREN || tok->kind == L2_TOK_EOF ||
		tok->kind == L2_TOK_EOL;
}

static int parse_expression(
		struct l2_lexer *lexer, struct l2_generator *gen, struct l2_parse_error *err);

static int parse_object_literal(
		struct l2_lexer *lexer, struct l2_generator *gen, struct l2_parse_error *err) {
	l2_trace_scope("object literal");
	// '{' and EOL already skipped by parse_object_or_function_literal

	while (1) {
		struct l2_token *tok = l2_lexer_peek(lexer, 1);
		if (tok->kind == L2_TOK_CLOSE_BRACE) {
			l2_lexer_consume(lexer); // '}'
			break;
		} else if (tok->kind != L2_TOK_IDENT) {
			l2_parse_err(err, tok, "In object literal: Expected identifier, got %s",
					l2_token_kind_name(tok->kind));
			return -1;
		}

		l2_trace("key: '%s'", tok->v.str);
		l2_lexer_consume(lexer); // ident

		tok = l2_lexer_peek(lexer, 1);
		if (tok->kind != L2_TOK_COLON) {
			l2_parse_err(err, tok, "In object literal: Expected ':', got %s",
					l2_token_kind_name(tok->kind));
			return -1;
		}

		l2_lexer_consume(lexer); // ':'

		if (parse_expression(lexer, gen, err) < 0) {
			return -1;
		}

		l2_lexer_skip_opt(lexer, L2_TOK_EOL);
	}

	return 0;
}

static int parse_function_literal(
		struct l2_lexer *lexer, struct l2_generator *gen, struct l2_parse_error *err) {
	l2_trace_scope("function literal");
	// '{' and EOL already skipped by parse_object_or_function_literal

	while (1) {
		if (l2_lexer_peek(lexer, 1)->kind == L2_TOK_CLOSE_BRACE) {
			l2_lexer_consume(lexer); // '}'
			break;
		}

		l2_trace_scope("function literal expression");
		if (parse_expression(lexer, gen, err) < 0) {
			return -1;
		}

		l2_lexer_skip_opt(lexer, L2_TOK_EOL);
	}

	return 0;
}

static int parse_object_or_function_literal(
		struct l2_lexer *lexer, struct l2_generator *gen, struct l2_parse_error *err) {
	l2_trace_scope("object or function literal");
	l2_lexer_consume(lexer); // '{'
	l2_lexer_skip_opt(lexer, L2_TOK_EOL);
	struct l2_token *tok = l2_lexer_peek(lexer, 1);
	struct l2_token *tok2 = l2_lexer_peek(lexer, 2);

	if (tok->kind == L2_TOK_CLOSE_BRACE) {
		l2_trace_scope("empty object literal");
		l2_lexer_consume(lexer); // '}'
	} else if (tok->kind == L2_TOK_IDENT && tok2->kind == L2_TOK_COLON) {
		if (parse_object_literal(lexer, gen, err) < 0) {
			return -1;
		}
	} else {
		if (parse_function_literal(lexer, gen, err) < 0) {
			return -1;
		}
	}

	return 0;
}

static int parse_arg_level_expression_base(
		struct l2_lexer *lexer, struct l2_generator *gen, struct l2_parse_error *err) {
	l2_trace_scope("arg level expression base");
	struct l2_token *tok = l2_lexer_peek(lexer, 1);
	struct l2_token *tok2 = l2_lexer_peek(lexer, 2);

	if (tok->kind == L2_TOK_OPEN_PAREN) {
		l2_trace_scope("group expr");
		l2_lexer_consume(lexer); // '('

		if (parse_expression(lexer, gen, err) < 0) {
			return -1;
		}

		tok = l2_lexer_peek(lexer, 1);
		if (tok->kind != L2_TOK_CLOSE_PAREN) {
			l2_parse_err(err, tok, "Expected '(', got %s",
					l2_token_kind_name(tok->kind));
			return -1;
		}
	} else if (tok->kind == L2_TOK_IDENT) {
		l2_trace_scope("ident");
		l2_trace("ident '%s'", tok->v.str);
		l2_lexer_consume(lexer); // ident
	} else if (tok->kind == L2_TOK_NUMBER) {
		l2_trace_scope("number literal");
		l2_trace("number %g", tok->v.num);
		l2_lexer_consume(lexer); // number
	} else if (tok->kind == L2_TOK_STRING) {
		l2_trace_scope("string literal");
		l2_trace("string '%s'", tok->v.str);
		l2_lexer_consume(lexer); // string
	} else if (tok->kind == L2_TOK_QUOT && tok2->kind == L2_TOK_IDENT) {
		l2_trace_scope("atom literal");
		l2_trace("atom '%s'", tok->v.str);
		l2_lexer_consume(lexer); // "'"
		l2_lexer_consume(lexer); // ident
	} else if (tok->kind == L2_TOK_OPEN_BRACE) {
		if (parse_object_or_function_literal(lexer, gen, err) < 0) {
			return -1;
		}
	} else {
		l2_parse_err(err, tok, "Unexpected token %s",
				l2_token_kind_name(tok->kind));
		return -1;
	}

	return 0;
}

static int parse_arg_level_expression(
		struct l2_lexer *lexer, struct l2_generator *gen, struct l2_parse_error *err) {
	l2_trace_scope("arg level expression");
	if (parse_arg_level_expression_base(lexer, gen, err) < 0) {
		return -1;
	}

	while (1) {
		struct l2_token *tok = l2_lexer_peek(lexer, 1);
		struct l2_token *tok2 = l2_lexer_peek(lexer, 2);

		if (tok->kind == L2_TOK_OPEN_PAREN && tok2->kind == L2_TOK_CLOSE_PAREN) {
			l2_trace_scope("noadic func call");
			l2_lexer_consume(lexer); // '('
			l2_lexer_consume(lexer); // ')'
		} else if (tok->kind == L2_TOK_PERIOD && tok2->kind == L2_TOK_IDENT) {
			l2_trace_scope("lookup");
			l2_lexer_consume(lexer); // '.'
			l2_lexer_consume(lexer); // ident
		} else {
			break;
		}
	}

	return 0;
}

static int parse_func_call_after_base(
		struct l2_lexer *lexer, struct l2_generator *gen, struct l2_parse_error *err) {
	l2_trace_scope("func call after base");

	size_t argc = 0;

	do {
		argc += 1;
		l2_trace_scope("func call param");
		if (parse_arg_level_expression(lexer, gen, err) < 0) {
			return -1;
		}
	} while (!tok_is_end(l2_lexer_peek(lexer, 1)));

	// The 'argc' previous expressions were arguments, the one before that was the function

	return 0;
}

static int parse_expression(
		struct l2_lexer *lexer, struct l2_generator *gen, struct l2_parse_error *err) {
	l2_trace_scope("expression");
	struct l2_token *tok = l2_lexer_peek(lexer, 1);
	struct l2_token *tok2 = l2_lexer_peek(lexer, 2);

	if (tok->kind == L2_TOK_IDENT && tok2->kind == L2_TOK_COLON_EQ) {
		l2_trace_scope("assign expression");
		l2_trace("ident '%s'", tok->v.str);
		l2_lexer_consume(lexer); // ident
		l2_lexer_consume(lexer); // :=

		if (parse_expression(lexer, gen, err) < 0) {
			return -1;
		}
	} else {
		if (parse_arg_level_expression(lexer, gen, err) < 0) {
			return -1;
		}

		if (!tok_is_end(l2_lexer_peek(lexer, 1))) {
			if (parse_func_call_after_base(lexer, gen, err) < 0) {
				return -1;
			}
		}
	}

	return 0;
}

int l2_parse_program(
		struct l2_lexer *lexer, struct l2_generator *gen, struct l2_parse_error *err) {
	l2_trace_scope("program");
	while (1) {
		l2_lexer_skip_opt(lexer, L2_TOK_EOL);
		if (l2_lexer_peek(lexer, 1)->kind == L2_TOK_EOF) {
			return 0;
		}

		if (parse_expression(lexer, gen, err) < 0) {
			return -1;
		}
	}

	return 0;
}
