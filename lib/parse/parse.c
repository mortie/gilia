#include "parse/parse.h"

#include "gen/gen.h"

static int is_end_tok(struct l2_token *tok) {
	return
		tok->kind == L2_TOK_CLOSE_PAREN || tok->kind == L2_TOK_CLOSE_BRACE ||
		tok->kind == L2_TOK_CLOSE_BRACKET || tok->kind == L2_TOK_EOL ||
		tok->kind == L2_TOK_EOF;
}

static int parse_expression(
		struct l2_lexer *lexer, struct l2_generator *gen, struct l2_parse_error *err);

static int parse_function_impl(
		struct l2_lexer *lexer, struct l2_generator *gen, struct l2_parse_error *err) {
	l2_lexer_consume(lexer); // {
	l2_lexer_skip_opt(lexer, L2_TOK_EOL);

	while (1) {
		struct l2_token *tok = l2_lexer_peek(lexer, 1);
		if (tok->kind == L2_TOK_EOF) {
			l2_parse_err(err, tok, "In function: Unexpected EOF");
			return -1;
		} else if (tok->kind == L2_TOK_CLOSE_BRACE) {
			l2_lexer_consume(lexer); // }
			break;
		}

		if (parse_expression(lexer, gen, err) < 0) {
			return -1;
		}

		l2_lexer_skip_opt(lexer, L2_TOK_EOL);
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

static int parse_sub_expression(
		struct l2_lexer *lexer, struct l2_generator *gen, struct l2_parse_error *err) {
	struct l2_token *tok = l2_lexer_peek(lexer, 1);

	if (tok->kind == L2_TOK_OPEN_PAREN) {
		l2_lexer_consume(lexer); // (
		if (parse_expression(lexer, gen, err) < 0) {
			return -1;
		}

		tok = l2_lexer_peek(lexer, 1);
		if (tok->kind != L2_TOK_CLOSE_PAREN) {
			l2_parse_err(err, tok, "In paren expression: Expected close paren, got %s",
					l2_token_kind_name(tok->kind));
			return -1;
		}

		l2_lexer_consume(lexer); // )
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
	} else if (tok->kind == L2_TOK_STRING) {
		char *str = l2_token_extract_str(tok);
		l2_lexer_consume(lexer); // string
		l2_gen_string(gen, &str);
		return 0;
	} else if (tok->kind == L2_TOK_OPEN_BRACE) {
		return parse_function(lexer, gen, err);
	}

	l2_parse_err(err, tok, "In expression: Unexpected token %s",
			l2_token_kind_name(tok->kind));
	return -1;
}

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
	} else if (tok->kind == L2_TOK_IDENT && !is_end_tok(tok2)) {
		char *ident = l2_token_extract_str(tok);
		l2_lexer_consume(lexer);

		l2_word count = 0;
		while (!is_end_tok(l2_lexer_peek(lexer, 1))) {
			count += 1;
			if (parse_sub_expression(lexer, gen, err) < 0) {
				return -1;
			}
		}

		l2_gen_push(gen, count);
		l2_gen_namespace_lookup(gen, &ident);
		l2_gen_func_call(gen);
		return 0;
	}

	return parse_sub_expression(lexer, gen, err);
}

int l2_parse_program(
		struct l2_lexer *lexer, struct l2_generator *gen, struct l2_parse_error *err) {
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
