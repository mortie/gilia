#include "parse/parse.h"

#include "trace.h"
#include "gen/gen.h"

static int is_end_tok(struct l2_token *tok) {
	return
		tok->kind == L2_TOK_CLOSE_PAREN || tok->kind == L2_TOK_CLOSE_BRACE ||
		tok->kind == L2_TOK_CLOSE_BRACKET || tok->kind == L2_TOK_EOL ||
		tok->kind == L2_TOK_EOF;
}

static int is_sub_expr_tok(struct l2_token *tok) {
	return
		tok->kind == L2_TOK_IDENT || tok->kind == L2_TOK_OPEN_PAREN ||
		tok->kind == L2_TOK_OPEN_BRACE || tok->kind == L2_TOK_OPEN_BRACKET ||
		tok->kind == L2_TOK_STRING || tok->kind == L2_TOK_NUMBER;
}

static int parse_expression(
		struct l2_lexer *lexer, struct l2_generator *gen, struct l2_parse_error *err);

static int parse_object(
		struct l2_lexer *lexer, struct l2_generator *gen, struct l2_parse_error *err) {
	l2_trace_func();
	// { and EOL are already skipped

	l2_gen_namespace(gen);
	while (1) {
		struct l2_token *tok = l2_lexer_peek(lexer, 1);
		if (tok->kind == L2_TOK_EOF) {
			l2_parse_err(err, tok, "In object literal: Unexpected EOF");
			return -1;
		} else if (tok->kind == L2_TOK_CLOSE_BRACE) {
			l2_lexer_consume(lexer); // }
			break;
		}

		l2_trace_scope("object key/val");

		if (tok->kind != L2_TOK_IDENT) {
			l2_parse_err(err, tok, "In object literal: Expected identifier, got %s\n",
					l2_token_kind_name(tok->kind));
		}

		char *key = l2_token_extract_str(tok);
		l2_lexer_consume(lexer); // ident

		tok = l2_lexer_peek(lexer, 1);
		if (tok->kind != L2_TOK_COLON) {
			l2_parse_err(err, tok, "In object literal: Expected colon, got %s\n",
					l2_token_kind_name(tok->kind));
		}

		l2_lexer_consume(lexer); // :

		if (parse_expression(lexer, gen, err) < 0) {
			return -1;
		}

		l2_gen_namespace_set(gen, &key);
		l2_gen_pop(gen);

		tok = l2_lexer_peek(lexer, 1);
		if (tok->kind != L2_TOK_EOL) {
			l2_parse_err(err, tok, "In object literal: Expected EOL, got %s\n",
					l2_token_kind_name(tok->kind));
			return -1;
		}

		l2_lexer_consume(lexer); // EOL
	}

	return 0;
}

static int parse_function_impl(
		struct l2_lexer *lexer, struct l2_generator *gen, struct l2_parse_error *err) {
	l2_trace_func();
	// { and EOL are already skipped

	int first = 1;
	while (1) {
		l2_trace_scope("function expr");
		struct l2_token *tok = l2_lexer_peek(lexer, 1);
		if (tok->kind == L2_TOK_EOF) {
			l2_parse_err(err, tok, "In function: Unexpected EOF");
			return -1;
		} else if (tok->kind == L2_TOK_CLOSE_BRACE) {
			l2_lexer_consume(lexer); // }
			break;
		}

		// The previous expr left a value on the stack which we have to pop
		if (!first) {
			l2_gen_pop(gen);
		}

		if (parse_expression(lexer, gen, err) < 0) {
			return -1;
		}

		l2_lexer_skip_opt(lexer, L2_TOK_EOL);
		first = 0;
	}

	// Empty function bodies must still return something; just return 0 (none)
	if (first) {
		l2_gen_push(gen, 0);
	}

	l2_gen_ret(gen);
	return 0;
}

static int parse_function(
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
	int ret = parse_function_impl(lexer, gen, err);
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

static int parse_function_or_object(
		struct l2_lexer *lexer, struct l2_generator *gen, struct l2_parse_error *err) {
	l2_trace_func();
	l2_lexer_consume(lexer); // {
	l2_lexer_skip_opt(lexer, L2_TOK_EOL);

	struct l2_token *tok = l2_lexer_peek(lexer, 1);
	struct l2_token *tok2 = l2_lexer_peek(lexer, 2);

	if (tok->kind == L2_TOK_CLOSE_BRACE) {
		l2_lexer_consume(lexer); // }
		l2_gen_namespace(gen);
		return 0;
	} else if (tok->kind == L2_TOK_IDENT && tok2->kind == L2_TOK_COLON) {
		return parse_object(lexer, gen, err);
	} else {
		return parse_function(lexer, gen, err);
	}
}

static int parse_sub_expression(
		struct l2_lexer *lexer, struct l2_generator *gen, struct l2_parse_error *err);

static int parse_opt_post_sub_expr(
		struct l2_lexer *lexer, struct l2_generator *gen, struct l2_parse_error *err) {
	l2_trace_func();
	struct l2_token *tok = l2_lexer_peek(lexer, 1);
	struct l2_token *tok2 = l2_lexer_peek(lexer, 2);

	if (tok->kind == L2_TOK_OPEN_PAREN && tok2->kind == L2_TOK_CLOSE_PAREN) {
		l2_trace_scope("noadic func call");
		l2_lexer_consume(lexer); // (
		l2_lexer_consume(lexer); // )
		l2_gen_push(gen, 0);
		l2_gen_func_call(gen);
	} else if (is_sub_expr_tok(tok)) {
		l2_trace_scope("func call");
		l2_word count = 0;
		while (!is_end_tok(l2_lexer_peek(lexer, 1))) {
			count += 1;
			if (parse_sub_expression(lexer, gen, err) < 0) {
				return -1;
			}
		}

		l2_gen_push(gen, count);
		l2_gen_func_call(gen);
	} else if (tok->kind == L2_TOK_PERIOD && tok2->kind == L2_TOK_IDENT) {
		l2_trace_scope("lookup");
		char *ident = l2_token_extract_str(tok2);
		l2_lexer_consume(lexer); // .
		l2_lexer_consume(lexer); // ident
		l2_gen_namespace_lookup(gen, &ident);
	} else {
		l2_parse_err(err, tok, "In post expression: Unexpected token %s",
				l2_token_kind_name(tok->kind));
		return -1;
	}

	return 0;
}

static int parse_sub_expression(
		struct l2_lexer *lexer, struct l2_generator *gen, struct l2_parse_error *err) {
	l2_trace_func();
	struct l2_token *tok = l2_lexer_peek(lexer, 1);
	struct l2_token *tok2 = l2_lexer_peek(lexer, 2);

	if (tok->kind == L2_TOK_OPEN_PAREN) {
		l2_lexer_consume(lexer); // (
		tok = l2_lexer_peek(lexer, 1);
		tok2 = l2_lexer_peek(lexer, 2);

		l2_trace_scope("parenthesized expression");
		if (parse_expression(lexer, gen, err) < 0) {
			return -1;
		}

		if (tok->kind != L2_TOK_CLOSE_PAREN) {
			l2_parse_err(err, tok, "In paren expression: Expected close paren, got %s",
					l2_token_kind_name(tok->kind));
			return -1;
		}

		l2_lexer_consume(lexer); // )
	} else if (tok->kind == L2_TOK_NUMBER) {
		l2_trace_scope("number");
		l2_gen_number(gen, tok->v.num);
		l2_lexer_consume(lexer); // number
	} else if (tok->kind == L2_TOK_IDENT) {
		l2_trace_scope("ident");
		char *ident = l2_token_extract_str(tok);
		l2_lexer_consume(lexer); // ident
		l2_gen_stack_frame_lookup(gen, &ident);
	} else if (tok->kind == L2_TOK_QUOT && tok2->kind == L2_TOK_IDENT) {
		l2_trace_scope("atom");
		char *str = l2_token_extract_str(tok2);
		l2_lexer_consume(lexer); // '
		l2_lexer_consume(lexer); // ident
		l2_gen_atom(gen, &str);
	} else if (tok->kind == L2_TOK_STRING) {
		l2_trace_scope("string");
		char *str = l2_token_extract_str(tok);
		l2_lexer_consume(lexer); // string
		l2_gen_string(gen, &str);
	} else if (tok->kind == L2_TOK_OPEN_BRACE) {
		if (parse_function_or_object(lexer, gen, err) < 0) {
			return -1;
		}
	} else {
		l2_parse_err(err, tok, "In expression: Unexpected token %s",
				l2_token_kind_name(tok->kind));
		return -1;
	}

	while (!is_end_tok(l2_lexer_peek(lexer, 1))) {
		if (parse_opt_post_sub_expr(lexer, gen, err) < 0) {
			return -1;
		}
	}

	return 0;
}

static int parse_expression(
		struct l2_lexer *lexer, struct l2_generator *gen, struct l2_parse_error *err) {
	l2_trace_func();
	struct l2_token *tok = l2_lexer_peek(lexer, 1);
	struct l2_token *tok2 = l2_lexer_peek(lexer, 2);

	if (tok->kind == L2_TOK_IDENT && tok2->kind == L2_TOK_COLON_EQ) {
		l2_trace_scope("assignment");
		char *ident = l2_token_extract_str(tok);
		l2_lexer_consume(lexer); // ident
		l2_lexer_consume(lexer); // :=

		if (parse_expression(lexer, gen, err) < 0) {
			free(ident);
			return -1;
		}

		l2_gen_assignment(gen, &ident);
	} else {
		if (parse_sub_expression(lexer, gen, err) < 0) {
			return -1;
		}
	}

	return 0;
}

int l2_parse_program(
		struct l2_lexer *lexer, struct l2_generator *gen, struct l2_parse_error *err) {
	l2_trace_func();
	l2_lexer_skip_opt(lexer, L2_TOK_EOL);

	int first = 1;
	while (1) {
		struct l2_token *tok = l2_lexer_peek(lexer, 1);
		if (tok->kind == L2_TOK_EOF) {
			break;
		}

		// The previous expr left a value on the stack which we have to pop
		if (!first) {
			l2_gen_pop(gen);
		}

		if (parse_expression(lexer, gen, err) < 0) {
			l2_gen_halt(gen);
			l2_gen_flush(gen);
			return -1;
		}

		l2_lexer_skip_opt(lexer, L2_TOK_EOL);
		first = 0;
	}

	l2_gen_halt(gen);
	l2_gen_flush(gen);
	return 0;
}
