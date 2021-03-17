#include "parse/parse.h"

#include "trace.h"
#include "gen/gen.h"

static int tok_is_end(struct l2_token *tok) {
	enum l2_token_kind kind = l2_token_get_kind(tok);
	return
		kind == L2_TOK_CLOSE_BRACE || kind == L2_TOK_CLOSE_BRACKET ||
		kind == L2_TOK_CLOSE_PAREN || kind == L2_TOK_EOF ||
		kind == L2_TOK_EOL;
}

static int tok_is_infix(struct l2_token *tok) {
	if (l2_token_get_kind(tok) != L2_TOK_IDENT) return 0;
	char *str;
	if (l2_token_is_small(tok)) {
		str = tok->v.strbuf;
	} else {
		str = tok->v.str;
	}

	return
			(str[0] == '$' && str[1] != '\0') ||
			strcmp(str, "+") == 0 ||
			strcmp(str, "-") == 0 ||
			strcmp(str, "*") == 0 ||
			strcmp(str, "/") == 0 ||
			strcmp(str, "==") == 0 ||
			strcmp(str, "!=") == 0 ||
			strcmp(str, "<") == 0 ||
			strcmp(str, "<=") == 0 ||
			strcmp(str, ">") == 0 ||
			strcmp(str, ">=") == 0;
}

static int parse_expression(
		struct l2_lexer *lexer, struct l2_generator *gen, struct l2_parse_error *err);

static int parse_arg_level_expression(
		struct l2_lexer *lexer, struct l2_generator *gen, struct l2_parse_error *err);

static int parse_object_literal(
		struct l2_lexer *lexer, struct l2_generator *gen, struct l2_parse_error *err) {
	l2_trace_scope("object literal");
	// '{' and EOL already skipped by parse_object_or_function_literal

	l2_gen_namespace(gen);

	while (1) {
		struct l2_token *tok = l2_lexer_peek(lexer, 1);
		if (l2_token_get_kind(tok) == L2_TOK_CLOSE_BRACE) {
			l2_lexer_consume(lexer); // '}'
			break;
		} else if (l2_token_get_kind(tok) != L2_TOK_IDENT) {
			l2_parse_err(err, tok, "In object literal: Expected identifier, got %s",
					l2_token_get_name(tok));
			return -1;
		}

		l2_trace("key: '%s'", tok->v.str);
		struct l2_token_value key = l2_token_extract_val(tok);
		l2_lexer_consume(lexer); // ident

		tok = l2_lexer_peek(lexer, 1);
		if (l2_token_get_kind(tok) != L2_TOK_COLON) {
			if (!(key.flags & L2_TOK_SMALL)) free(key.str);
			l2_parse_err(err, tok, "In object literal: Expected ':', got %s",
					l2_token_get_name(tok));
			return -1;
		}

		l2_lexer_consume(lexer); // ':'

		if (parse_expression(lexer, gen, err) < 0) {
			if (!(key.flags & L2_TOK_SMALL)) free(key.str);
			return -1;
		}

		if (key.flags & L2_TOK_SMALL) {
			l2_gen_namespace_set_copy(gen, key.strbuf);
		} else {
			l2_gen_namespace_set_copy(gen, key.str);
		}

		l2_gen_discard(gen);

		tok = l2_lexer_peek(lexer, 1);
		if (
				l2_token_get_kind(tok) != L2_TOK_EOL &&
				l2_token_get_kind(tok) != L2_TOK_CLOSE_BRACE) {
			l2_parse_err(err, tok, "In object literal: Expected EOL or '}', got %s",
					l2_token_get_name(tok));
			return -1;
		}

		if (l2_token_get_kind(tok) == L2_TOK_EOL) {
			l2_lexer_consume(lexer); // EOL
		}
	}

	return 0;
}

static int parse_function_literal_impl(
		struct l2_lexer *lexer, struct l2_generator *gen, struct l2_parse_error *err) {
	l2_trace_scope("function literal");
	// '{' and EOL already skipped by parse_object_or_function_literal

	// The arguments array will be at the top of the stack
	char *ident = "$";
	l2_gen_stack_frame_set_copy(gen, ident);

	int first = 1;
	while (1) {
		if (l2_token_get_kind(l2_lexer_peek(lexer, 1)) == L2_TOK_CLOSE_BRACE) {
			l2_lexer_consume(lexer); // '}'
			break;
		}

		if (!first) {
			l2_gen_discard(gen);
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
		l2_gen_none(gen);
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

	// Generates five bytes; RJMP, then 4 byte counter
	l2_gen_rjmp_placeholder(gen);

	l2_word pos = gen->pos;

	// Generate the function body itself
	int ret = parse_function_literal_impl(lexer, gen, err);
	l2_gen_flush(gen);
	gen->writer.w = prev_writer;
	if (ret < 0) {
		free(w.mem);
		return -1;
	}

	unsigned char *bc = w.mem;
	l2_word jdist = w.len - 5;

	// Write the jump distance (little endian)
	bc[1] = (jdist >> 0) & 0xff;
	bc[2] = (jdist >> 8) & 0xff;
	bc[3] = (jdist >> 16) & 0xff;
	bc[4] = (jdist >> 24) & 0xff;

	l2_bufio_put_n(&gen->writer, bc, w.len);
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

	if (l2_token_get_kind(tok) == L2_TOK_CLOSE_BRACE) {
		l2_trace_scope("empty object literal");
		l2_lexer_consume(lexer); // '}'

		l2_gen_namespace(gen);
	} else if (
			l2_token_get_kind(tok) == L2_TOK_IDENT &&
			l2_token_get_kind(tok2) == L2_TOK_COLON) {
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

static int parse_array_literal(
		struct l2_lexer *lexer, struct l2_generator *gen, struct l2_parse_error *err) {
	l2_trace_scope("array literal");
	l2_lexer_consume(lexer); // '['
	l2_lexer_skip_opt(lexer, L2_TOK_EOL);

	int count = 0;
	while (1) {
		if (l2_token_get_kind(l2_lexer_peek(lexer, 1)) == L2_TOK_CLOSE_BRACKET) {
			l2_lexer_consume(lexer); // ']'
			break;
		}

		count += 1;
		if (parse_arg_level_expression(lexer, gen, err) < 0) {
			return -1;
		}

		l2_lexer_skip_opt(lexer, L2_TOK_EOL);
	}

	l2_gen_array(gen, count);
	return 0;
}

static int parse_arg_level_expression_base(
		struct l2_lexer *lexer, struct l2_generator *gen, struct l2_parse_error *err) {
	l2_trace_scope("arg level expression base");
	struct l2_token *tok = l2_lexer_peek(lexer, 1);
	struct l2_token *tok2 = l2_lexer_peek(lexer, 2);

	if (l2_token_get_kind(tok) == L2_TOK_OPEN_PAREN) {
		l2_trace_scope("group expr");
		l2_lexer_consume(lexer); // '('

		if (parse_expression(lexer, gen, err) < 0) {
			return -1;
		}

		tok = l2_lexer_peek(lexer, 1);
		if (l2_token_get_kind(tok) != L2_TOK_CLOSE_PAREN) {
			l2_parse_err(err, tok, "Expected ')', got %s",
					l2_token_get_name(tok));
			return -1;
		}

		l2_lexer_consume(lexer); // ')'
	} else if (l2_token_get_kind(tok) == L2_TOK_IDENT) {
		l2_trace_scope("ident");
		if (l2_token_is_small(tok)) {
			l2_trace("ident '%s'", tok->v.strbuf);
		} else {
			l2_trace("ident '%s'", tok->v.str);
		}
		struct l2_token_value ident = l2_token_extract_val(tok);
		l2_lexer_consume(lexer); // ident

		if (ident.flags & L2_TOK_SMALL) {
			l2_gen_stack_frame_lookup_copy(gen, ident.strbuf);
		} else {
			l2_gen_stack_frame_lookup(gen, &ident.str);
		}
	} else if (l2_token_get_kind(tok) == L2_TOK_NUMBER) {
		l2_trace_scope("number literal");
		l2_trace("number %g", tok->v.num);
		double number = tok->v.num;
		l2_lexer_consume(lexer); // number

		l2_gen_number(gen, number);
	} else if (l2_token_get_kind(tok) == L2_TOK_STRING) {
		l2_trace_scope("string literal");
		l2_trace("string '%s'", tok->v.str);
		struct l2_token_value str = l2_token_extract_val(tok);
		l2_lexer_consume(lexer); // string

		if (str.flags & L2_TOK_SMALL) {
			l2_gen_string_copy(gen, str.strbuf);
		} else {
			l2_gen_string(gen, &str.str);
		}
	} else if (
			l2_token_get_kind(tok) == L2_TOK_QUOT &&
			l2_token_get_kind(tok2) == L2_TOK_IDENT) {
		l2_trace_scope("atom literal");
		l2_trace("atom '%s'", tok->v.str);
		struct l2_token_value ident = l2_token_extract_val(tok2);
		l2_lexer_consume(lexer); // "'"
		l2_lexer_consume(lexer); // ident

		if (ident.flags & L2_TOK_SMALL) {
			l2_gen_atom_copy(gen, ident.strbuf);
		} else {
			l2_gen_atom(gen, &ident.str);
		}
	} else if (l2_token_get_kind(tok) == L2_TOK_OPEN_BRACE) {
		if (parse_object_or_function_literal(lexer, gen, err) < 0) {
			return -1;
		}
	} else if (l2_token_get_kind(tok) == L2_TOK_OPEN_BRACKET) {
		if (parse_array_literal(lexer, gen, err) < 0) {
			return -1;
		}
	} else {
		l2_parse_err(err, tok, "Unexpected token %s",
				l2_token_get_name(tok));
		return -1;
	}

	return 0;
}

static int parse_func_call_after_base(
		struct l2_lexer *lexer, struct l2_generator *gen, struct l2_parse_error *err,
		size_t infix_start) {
	l2_trace_scope("func call after base");

	size_t argc = 0;

	do {
		if (argc >= infix_start && tok_is_infix(l2_lexer_peek(lexer, 1))) {
			do {
				// We already have one value (the lhs) on the stack,
				// so we need to parse the operator, then the rhs

				// Operator
				if (parse_arg_level_expression(lexer, gen, err) < 0) {
					return -1;
				}

				// RHS
				if (parse_arg_level_expression(lexer, gen, err) < 0) {
					return -1;
				}

				l2_gen_func_call_infix(gen);
			} while (tok_is_infix(l2_lexer_peek(lexer, 1)));

			// If this was the "first argument", this wasn't a function call
			// after all, it was just a (series of?) infix calls.
			if (argc == 0) {
				return 0;
			}

			// Don't increment argc here, because after an infix, we have
			// neither added nor removed an arguemnt, just transformed one
		} else {
			l2_trace_scope("func call param");
			if (parse_arg_level_expression(lexer, gen, err) < 0) {
				return -1;
			}

			argc += 1;
		}
	} while (!tok_is_end(l2_lexer_peek(lexer, 1)));

	// The 'argc' previous expressions were arguments, the one before that was the function
	l2_gen_func_call(gen, argc);

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
		struct l2_token *tok3 = l2_lexer_peek(lexer, 3);

		if (l2_token_get_kind(tok) == L2_TOK_OPEN_PAREN_NS) {
			l2_trace_scope("parenthesized func call");
			l2_lexer_consume(lexer); // '('

			if (l2_token_get_kind(l2_lexer_peek(lexer, 1)) == L2_TOK_CLOSE_PAREN) {
				l2_lexer_consume(lexer); // ')'
				l2_gen_func_call(gen, 0);
			} else {
				if (parse_func_call_after_base(lexer, gen, err, 1) < 0) {
					return -1;
				}

				tok = l2_lexer_peek(lexer, 1);
				if (l2_token_get_kind(tok) != L2_TOK_CLOSE_PAREN) {
					l2_parse_err(err, tok, "Expected ')', got %s",
							l2_token_get_name(tok));
					return -1;
				}
				l2_lexer_consume(lexer); // ')'
			}
		} else if (
				l2_token_get_kind(tok) == L2_TOK_PERIOD &&
				l2_token_get_kind(tok2) == L2_TOK_IDENT &&
				l2_token_get_kind(tok3) == L2_TOK_EQUALS) {
			l2_trace_scope("namespace assign");
			if (l2_token_is_small(tok2)) {
				l2_trace("ident '%s'", tok2->v.strbuf);
			} else {
				l2_trace("ident '%s'", tok2->v.str);
			}
			struct l2_token_value ident = l2_token_extract_val(tok2);
			l2_lexer_consume(lexer); // '.'
			l2_lexer_consume(lexer); // ident
			l2_lexer_consume(lexer); // '='

			if (parse_expression(lexer, gen, err) < 0) {
			if (!(ident.flags & L2_TOK_SMALL)) free(ident.str);
				return -1;
			}

			if (ident.flags & L2_TOK_SMALL) {
				l2_gen_namespace_set_copy(gen, ident.strbuf);
			} else {
				l2_gen_namespace_set(gen, &ident.str);
			}
			l2_gen_swap_discard(gen);
		} else if (
				l2_token_get_kind(tok) == L2_TOK_PERIOD &&
				l2_token_get_kind(tok2) == L2_TOK_IDENT) {
			l2_trace_scope("namespace lookup");
			if (l2_token_is_small(tok2)) {
				l2_trace("ident '%s'", tok2->v.strbuf);
			} else {
				l2_trace("ident '%s'", tok2->v.str);
			}
			struct l2_token_value ident = l2_token_extract_val(tok2);
			l2_lexer_consume(lexer); // '.'
			l2_lexer_consume(lexer); // ident

			if (ident.flags & L2_TOK_SMALL) {
				l2_gen_namespace_lookup_copy(gen, ident.strbuf);
			} else {
				l2_gen_namespace_lookup(gen, &ident.str);
			}
		} else if (
				l2_token_get_kind(tok) == L2_TOK_DOT_NUMBER &&
				l2_token_get_kind(tok2) == L2_TOK_EQUALS) {
			l2_trace_scope("direct array assign");
			int number = tok->v.integer;
			l2_lexer_consume(lexer); // dot-number
			l2_lexer_consume(lexer); // '='

			if (parse_expression(lexer, gen, err) < 0) {
				return -1;
			}

			l2_gen_array_set(gen, number);
			l2_gen_swap_discard(gen);
		} else if (l2_token_get_kind(tok) == L2_TOK_DOT_NUMBER) {
			l2_trace_scope("direct array lookup");
			int number = tok->v.integer;
			l2_lexer_consume(lexer); // dot-number

			l2_gen_array_lookup(gen, number);
		} else if (
				l2_token_get_kind(tok) == L2_TOK_PERIOD &&
				l2_token_get_kind(tok2) == L2_TOK_OPEN_PAREN_NS) {
			l2_trace_scope("dynamic lookup");
			l2_lexer_consume(lexer); // '.'
			l2_lexer_consume(lexer); // '('

			if (parse_expression(lexer, gen, err) < 0) {
				return -1;
			}

			tok = l2_lexer_peek(lexer, 1);
			if (l2_token_get_kind(tok) != L2_TOK_CLOSE_PAREN) {
				l2_parse_err(err, tok, "Expected ')', got %s",
						l2_token_get_name(tok));
				return -1;
			}
			l2_lexer_consume(lexer); // ')'

			if (l2_token_get_kind(l2_lexer_peek(lexer, 1)) == L2_TOK_EQUALS) {
				l2_lexer_consume(lexer); // '='
				if (parse_expression(lexer, gen, err) < 0) {
					return -1;
				}

				l2_gen_dynamic_set(gen);
			} else {
				l2_gen_dynamic_lookup(gen);
			}
		} else {
			break;
		}
	}

	return 0;
}

static int parse_expression(
		struct l2_lexer *lexer, struct l2_generator *gen, struct l2_parse_error *err) {
	l2_trace_scope("expression");
	struct l2_token *tok = l2_lexer_peek(lexer, 1);
	struct l2_token *tok2 = l2_lexer_peek(lexer, 2);

	if (
			l2_token_get_kind(tok) == L2_TOK_IDENT &&
			l2_token_get_kind(tok2) == L2_TOK_COLON_EQ) {
		l2_trace_scope("assign expression");
		if (l2_token_is_small(tok)) {
			l2_trace("ident '%s'", tok->v.strbuf);
		} else {
			l2_trace("ident '%s'", tok->v.str);
		}
		struct l2_token_value ident = l2_token_extract_val(tok);
		l2_lexer_consume(lexer); // ident
		l2_lexer_consume(lexer); // :=

		if (parse_expression(lexer, gen, err) < 0) {
			if (!(ident.flags & L2_TOK_SMALL)) free(ident.str);
			return -1;
		}

		if (ident.flags & L2_TOK_SMALL) {
			l2_gen_stack_frame_set_copy(gen, ident.strbuf);
		} else {
			l2_gen_stack_frame_set(gen, &ident.str);
		}
	} else if (
			l2_token_get_kind(tok) == L2_TOK_IDENT &&
			l2_token_get_kind(tok2) == L2_TOK_EQUALS) {
		l2_trace_scope("replacement assign expression");
		if (l2_token_is_small(tok)) {
			l2_trace("ident '%s'", tok->v.strbuf);
		} else {
			l2_trace("ident '%s'", tok->v.str);
		}
		struct l2_token_value ident = l2_token_extract_val(tok);
		l2_lexer_consume(lexer); // ident
		l2_lexer_consume(lexer); // =

		if (parse_expression(lexer, gen, err) < 0) {
			if (!(ident.flags & L2_TOK_SMALL)) free(ident.str);
			return -1;
		}

		if (ident.flags & L2_TOK_SMALL) {
			l2_gen_stack_frame_replace_copy(gen, ident.strbuf);
		} else {
			l2_gen_stack_frame_replace(gen, &ident.str);
		}
	} else {
		if (parse_arg_level_expression(lexer, gen, err) < 0) {
			return -1;
		}

		if (!tok_is_end(l2_lexer_peek(lexer, 1))) {
			if (parse_func_call_after_base(lexer, gen, err, 0) < 0) {
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
		if (l2_token_get_kind(l2_lexer_peek(lexer, 1)) == L2_TOK_EOF) {
			break;
		}

		if (parse_expression(lexer, gen, err) < 0) {
			l2_gen_halt(gen);
			l2_gen_flush(gen);
			return -1;
		}

		l2_gen_discard(gen);
	}

	l2_gen_halt(gen);
	l2_gen_flush(gen);
	return 0;
}
