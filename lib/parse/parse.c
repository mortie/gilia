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

	l2_gen_namespace(gen);

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
		char *key = l2_token_extract_str(tok);
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

		l2_gen_namespace_set(gen, &key);
		l2_gen_pop(gen);

		tok = l2_lexer_peek(lexer, 1);
		if (tok->kind != L2_TOK_EOL && tok->kind != L2_TOK_CLOSE_BRACE) {
			l2_parse_err(err, tok, "In object literal: Expected EOL or '}', got %s",
					l2_token_kind_name(tok->kind));
			return -1;
		}

		if (tok->kind == L2_TOK_EOL) {
			l2_lexer_consume(lexer); // EOL
		}
	}

	return 0;
}

static int parse_function_literal_impl(
		struct l2_lexer *lexer, struct l2_generator *gen, struct l2_parse_error *err) {
	l2_trace_scope("function literal");
	// '{' and EOL already skipped by parse_object_or_function_literal

	int first = 1;
	while (1) {
		if (l2_lexer_peek(lexer, 1)->kind == L2_TOK_CLOSE_BRACE) {
			l2_lexer_consume(lexer); // '}'
			break;
		}

		if (!first) {
			l2_gen_pop(gen);
		}

		l2_trace_scope("function literal expression");
		if (parse_expression(lexer, gen, err) < 0) {
			return -1;
		}

		l2_lexer_skip_opt(lexer, L2_TOK_EOL);
		first = 0;
	}

	// All functions must put _something_ on the stack
	if (first) {
		l2_gen_push(gen, 0);
	}

	l2_gen_ret(gen);
	return 0;
}

static int parse_function_literal(
		struct l2_lexer *lexer, struct l2_generator *gen, struct l2_parse_error *err) {
	l2_gen_flush(gen);

	struct l2_io_writer *prev_writer = gen->writer.w;

	// Generate the function to a buffer in memory
	struct l2_io_mem_writer w = {0};
	w.w.write = l2_io_mem_write;
	gen->writer.w = &w.w;

	// Generates three words; PUSH, 0, RJMP
	l2_gen_rjmp(gen, 0);

	l2_word pos = gen->pos;

	// Generate the function body itself
	int ret = parse_function_literal_impl(lexer, gen, err);
	l2_gen_flush(gen);
	gen->writer.w = prev_writer;
	if (ret < 0) {
		free(w.mem);
		return -1;
	}

	l2_word *ops = (l2_word *)w.mem;
	l2_word opcount = w.len / sizeof(l2_word);

	// Due to the earlier gen_rjmp, the second word will be the argument to RJMP.
	// Need to set it properly to skip the function body.
	// The '- 3' is because we don't skip the PUSH, <count>, RJMP sequence.
	ops[1] = opcount - 3;

	l2_bufio_put_n(&gen->writer, ops, opcount * sizeof(l2_word));
	free(w.mem);

	l2_gen_function(gen, pos);
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
		char *ident = l2_token_extract_str(tok);
		l2_lexer_consume(lexer); // ident

		l2_gen_stack_frame_lookup(gen, &ident);
	} else if (tok->kind == L2_TOK_NUMBER) {
		l2_trace_scope("number literal");
		l2_trace("number %g", tok->v.num);
		double number = tok->v.num;
		l2_lexer_consume(lexer); // number

		l2_gen_number(gen, number);
	} else if (tok->kind == L2_TOK_STRING) {
		l2_trace_scope("string literal");
		l2_trace("string '%s'", tok->v.str);
		char *str = l2_token_extract_str(tok);
		l2_lexer_consume(lexer); // string

		l2_gen_string(gen, &str);
	} else if (tok->kind == L2_TOK_QUOT && tok2->kind == L2_TOK_IDENT) {
		l2_trace_scope("atom literal");
		l2_trace("atom '%s'", tok->v.str);
		char *ident = l2_token_extract_str(tok2);
		l2_lexer_consume(lexer); // "'"
		l2_lexer_consume(lexer); // ident

		l2_gen_atom(gen, &ident);
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
			l2_trace_scope("niladic func call");
			l2_lexer_consume(lexer); // '('
			l2_lexer_consume(lexer); // ')'

			l2_gen_push(gen, 0);
			l2_gen_func_call(gen);
		} else if (tok->kind == L2_TOK_PERIOD && tok2->kind == L2_TOK_IDENT) {
			l2_trace_scope("lookup");
			l2_trace("ident '%s'", tok2->v.str);
			char *ident = l2_token_extract_str(tok2);
			l2_lexer_consume(lexer); // '.'
			l2_lexer_consume(lexer); // ident

			l2_gen_namespace_lookup(gen, &ident);
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
	l2_gen_push(gen, argc);
	l2_gen_func_call(gen);

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
		char *ident = l2_token_extract_str(tok);
		l2_lexer_consume(lexer); // ident
		l2_lexer_consume(lexer); // :=

		if (parse_expression(lexer, gen, err) < 0) {
			return -1;
		}

		l2_gen_assignment(gen, &ident);
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
			break;
		}

		if (parse_expression(lexer, gen, err) < 0) {
			l2_gen_halt(gen);
			l2_gen_flush(gen);
			return -1;
		}

		l2_gen_pop(gen);
	}

	l2_gen_halt(gen);
	l2_gen_flush(gen);
	return 0;
}
