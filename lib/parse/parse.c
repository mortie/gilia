#include "parse/parse.h"

#include "trace.h"
#include "gen/gen.h"

static int tok_is_end(struct gil_token *tok) {
	enum gil_token_kind kind = gil_token_get_kind(tok);
	return
		kind == GIL_TOK_CLOSE_BRACE || kind == GIL_TOK_CLOSE_BRACKET ||
		kind == GIL_TOK_CLOSE_PAREN || kind == GIL_TOK_EOF ||
		kind == GIL_TOK_EOL;
}

static int tok_is_infix(struct gil_token *tok) {
	if (gil_token_get_kind(tok) != GIL_TOK_IDENT) return 0;
	const char *str = gil_token_get_str(tok);

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
			strcmp(str, ">=") == 0 ||
			strcmp(str, "&&") == 0 ||
			strcmp(str, "||") == 0 ||
			strcmp(str, "??") == 0;
}

static int parse_expression(
		struct gil_lexer *lexer, struct gil_generator *gen, struct gil_parse_error *err);

static int parse_arg_level_expression(
		struct gil_lexer *lexer, struct gil_generator *gen, struct gil_parse_error *err);

static int parse_object_literal(
		struct gil_lexer *lexer, struct gil_generator *gen, struct gil_parse_error *err) {
	gil_trace_scope("object literal");
	// '{' and EOL already skipped by parse_object_or_function_literal

	gil_gen_namespace(gen);

	while (1) {
		struct gil_token *tok = gil_lexer_peek(lexer, 1);
		if (gil_token_get_kind(tok) == GIL_TOK_CLOSE_BRACE) {
			gil_lexer_consume(lexer); // '}'
			break;
		} else if (gil_token_get_kind(tok) != GIL_TOK_IDENT) {
			gil_parse_err(err, tok, "In object literal: Expected identifier, got %s",
					gil_token_get_name(tok));
			return -1;
		}

		gil_trace("key: '%s'", gil_token_get_str(tok));
		struct gil_token_value key = gil_token_extract_val(tok);
		gil_lexer_consume(lexer); // ident

		tok = gil_lexer_peek(lexer, 1);
		if (gil_token_get_kind(tok) != GIL_TOK_COLON) {
			if (!(key.flags & GIL_TOK_SMALL)) free(key.str);
			gil_parse_err(err, tok, "In object literal: Expected ':', got %s",
					gil_token_get_name(tok));
			return -1;
		}

		gil_lexer_consume(lexer); // ':'

		if (parse_expression(lexer, gen, err) < 0) {
			if (!(key.flags & GIL_TOK_SMALL)) free(key.str);
			return -1;
		}

		if (key.flags & GIL_TOK_SMALL) {
			gil_gen_namespace_set_copy(gen, key.strbuf);
		} else {
			gil_gen_namespace_set_copy(gen, key.str);
		}

		gil_gen_discard(gen);

		tok = gil_lexer_peek(lexer, 1);
		if (
				gil_token_get_kind(tok) != GIL_TOK_EOL &&
				gil_token_get_kind(tok) != GIL_TOK_CLOSE_BRACE) {
			gil_parse_err(err, tok, "In object literal: Expected EOL or '}', got %s",
					gil_token_get_name(tok));
			return -1;
		}

		if (gil_token_get_kind(tok) == GIL_TOK_EOL) {
			gil_lexer_consume(lexer); // EOL
		}
	}

	return 0;
}

static int parse_function_literal_impl(
		struct gil_lexer *lexer, struct gil_generator *gen, struct gil_parse_error *err) {
	gil_trace_scope("function literal");
	// '{' and EOL already skipped by parse_object_or_function_literal

	int first = 1;
	while (1) {
		if (gil_token_get_kind(gil_lexer_peek(lexer, 1)) == GIL_TOK_CLOSE_BRACE) {
			gil_lexer_consume(lexer); // '}'
			break;
		}

		if (!first) {
			gil_gen_discard(gen);
		}

		gil_trace_scope("function literal expression");
		if (parse_expression(lexer, gen, err) < 0) {
			return -1;
		}

		gil_lexer_skip_opt(lexer, GIL_TOK_EOL);
		first = 0;
	}

	// All functions must put _something_ on the stack
	if (first) {
		gil_gen_none(gen);
	}

	gil_gen_ret(gen);
	return 0;
}

static int parse_function_literal(
		struct gil_lexer *lexer, struct gil_generator *gen, struct gil_parse_error *err) {
	gil_gen_flush(gen);

	struct gil_io_writer *prev_writer = gen->writer.w;

	// Generate the function to a buffer in memory
	struct gil_io_mem_writer w = {0};
	w.w.write = gil_io_mem_write;
	gen->writer.w = &w.w;

	// Generates five bytes; RJMP, then 4 byte counter
	gil_gen_rjmp_placeholder(gen);

	gil_word pos = gen->pos;

	// Generate the function body itself
	int ret = parse_function_literal_impl(lexer, gen, err);
	gil_gen_flush(gen);
	gen->writer.w = prev_writer;
	if (ret < 0) {
		free(w.mem);
		return -1;
	}

	unsigned char *bc = w.mem;
	gil_word jdist = w.len - 5;

	// Write the jump distance (little endian)
	bc[1] = (jdist >> 0) & 0xff;
	bc[2] = (jdist >> 8) & 0xff;
	bc[3] = (jdist >> 16) & 0xff;
	bc[4] = (jdist >> 24) & 0xff;

	gil_bufio_put_n(&gen->writer, bc, w.len);
	free(w.mem);

	gil_gen_function(gen, pos);
	return 0;
}

static int parse_object_or_function_literal(
		struct gil_lexer *lexer, struct gil_generator *gen, struct gil_parse_error *err) {
	gil_trace_scope("object or function literal");
	gil_lexer_consume(lexer); // '{'
	gil_lexer_skip_opt(lexer, GIL_TOK_EOL);
	struct gil_token *tok = gil_lexer_peek(lexer, 1);
	struct gil_token *tok2 = gil_lexer_peek(lexer, 2);

	if (gil_token_get_kind(tok) == GIL_TOK_CLOSE_BRACE) {
		gil_trace_scope("empty object literal");
		gil_lexer_consume(lexer); // '}'

		gil_gen_namespace(gen);
	} else if (
			gil_token_get_kind(tok) == GIL_TOK_IDENT &&
			gil_token_get_kind(tok2) == GIL_TOK_COLON) {
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
		struct gil_lexer *lexer, struct gil_generator *gen, struct gil_parse_error *err) {
	gil_trace_scope("array literal");
	gil_lexer_consume(lexer); // '['
	gil_lexer_skip_opt(lexer, GIL_TOK_EOL);

	int count = 0;
	while (1) {
		if (gil_token_get_kind(gil_lexer_peek(lexer, 1)) == GIL_TOK_CLOSE_BRACKET) {
			gil_lexer_consume(lexer); // ']'
			break;
		}

		count += 1;
		if (parse_arg_level_expression(lexer, gen, err) < 0) {
			return -1;
		}

		gil_lexer_skip_opt(lexer, GIL_TOK_EOL);
	}

	gil_gen_array(gen, count);
	return 0;
}

static int parse_arg_level_expression_base(
		struct gil_lexer *lexer, struct gil_generator *gen, struct gil_parse_error *err) {
	gil_trace_scope("arg level expression base");
	struct gil_token *tok = gil_lexer_peek(lexer, 1);
	struct gil_token *tok2 = gil_lexer_peek(lexer, 2);

	if (gil_token_get_kind(tok) == GIL_TOK_OPEN_PAREN) {
		gil_trace_scope("group expr");
		gil_lexer_consume(lexer); // '('

		if (parse_expression(lexer, gen, err) < 0) {
			return -1;
		}

		tok = gil_lexer_peek(lexer, 1);
		if (gil_token_get_kind(tok) != GIL_TOK_CLOSE_PAREN) {
			gil_parse_err(err, tok, "Expected ')', got %s",
					gil_token_get_name(tok));
			return -1;
		}

		gil_lexer_consume(lexer); // ')'
	} else if (gil_token_get_kind(tok) == GIL_TOK_IDENT) {
		gil_trace_scope("ident");
		gil_trace("ident '%s'", gil_token_get_str(tok));
		struct gil_token_value ident = gil_token_extract_val(tok);
		if (strcmp(gil_token_get_str(tok), "$") == 0) {
			gil_lexer_consume(lexer); // ident
			gil_gen_stack_frame_get_args(gen);
		} else {
			gil_lexer_consume(lexer); // ident

			if (ident.flags & GIL_TOK_SMALL) {
				gil_gen_stack_frame_lookup_copy(gen, ident.strbuf);
			} else {
				gil_gen_stack_frame_lookup(gen, &ident.str);
			}
		}
	} else if (gil_token_get_kind(tok) == GIL_TOK_NUMBER) {
		gil_trace_scope("number literal");
		gil_trace("number %g", tok->v.num);
		double number = tok->v.num;
		gil_lexer_consume(lexer); // number

		gil_gen_number(gen, number);
	} else if (gil_token_get_kind(tok) == GIL_TOK_STRING) {
		gil_trace_scope("string literal");
		gil_trace("string '%s'", gil_token_get_str(tok));
		struct gil_token_value str = gil_token_extract_val(tok);
		gil_lexer_consume(lexer); // string

		if (str.flags & GIL_TOK_SMALL) {
			gil_gen_string_copy(gen, str.strbuf);
		} else {
			gil_gen_string(gen, &str.str);
		}
	} else if (
			gil_token_get_kind(tok) == GIL_TOK_QUOT &&
			gil_token_get_kind(tok2) == GIL_TOK_IDENT) {
		gil_trace_scope("atom literal");
		gil_trace("atom '%s'", gil_token_get_str(tok2));
		struct gil_token_value ident = gil_token_extract_val(tok2);
		gil_lexer_consume(lexer); // "'"
		gil_lexer_consume(lexer); // ident

		if (ident.flags & GIL_TOK_SMALL) {
			gil_gen_atom_copy(gen, ident.strbuf);
		} else {
			gil_gen_atom(gen, &ident.str);
		}
	} else if (gil_token_get_kind(tok) == GIL_TOK_OPEN_BRACE) {
		if (parse_object_or_function_literal(lexer, gen, err) < 0) {
			return -1;
		}
	} else if (gil_token_get_kind(tok) == GIL_TOK_OPEN_BRACKET) {
		if (parse_array_literal(lexer, gen, err) < 0) {
			return -1;
		}
	} else {
		gil_parse_err(err, tok, "Unexpected token %s",
				gil_token_get_name(tok));
		return -1;
	}

	return 0;
}

static int parse_func_call_after_base(
		struct gil_lexer *lexer, struct gil_generator *gen, struct gil_parse_error *err,
		size_t infix_start) {
	gil_trace_scope("func call after base");

	size_t argc = 0;

	do {
		if (argc >= infix_start && tok_is_infix(gil_lexer_peek(lexer, 1))) {
			do {
				// We already have one value (the lhs) on the stack,
				// so we need to parse the operator, then the rhs

				// Operator
				int ret = parse_arg_level_expression(lexer, gen, err);
				if (ret < 0) {
					return -1;
				}

				// If the operator wasn't just the one base expression,
				// abort; we're not doing the infix call
				if (ret == 1) {
					argc += 1;
					break;
				}

				// RHS
				if (parse_arg_level_expression(lexer, gen, err) < 0) {
					return -1;
				}

				gil_gen_func_call_infix(gen);
			} while (tok_is_infix(gil_lexer_peek(lexer, 1)));

			// If this was the "first argument", this wasn't a function call
			// after all, it was just a (series of?) infix calls.
			if (argc == 0) {
				return 0;
			}

			// Don't increment argc here, because after an infix, we have
			// neither added nor removed an arguemnt, just transformed one
		} else {
			gil_trace_scope("func call param");
			if (parse_arg_level_expression(lexer, gen, err) < 0) {
				return -1;
			}

			argc += 1;
		}
	} while (!tok_is_end(gil_lexer_peek(lexer, 1)));

	// The 'argc' previous expressions were arguments, the one before that was the function
	gil_gen_func_call(gen, argc);

	return 0;
}

static int parse_arg_level_expression(
		struct gil_lexer *lexer, struct gil_generator *gen, struct gil_parse_error *err) {
	gil_trace_scope("arg level expression");
	if (parse_arg_level_expression_base(lexer, gen, err) < 0) {
		return -1;
	}

	int ret = 0;
	while (1) {
		struct gil_token *tok = gil_lexer_peek(lexer, 1);
		struct gil_token *tok2 = gil_lexer_peek(lexer, 2);
		struct gil_token *tok3 = gil_lexer_peek(lexer, 3);

		if (gil_token_get_kind(tok) == GIL_TOK_OPEN_PAREN_NS) {
			gil_trace_scope("parenthesized func call");
			gil_lexer_consume(lexer); // '('

			if (gil_token_get_kind(gil_lexer_peek(lexer, 1)) == GIL_TOK_CLOSE_PAREN) {
				gil_lexer_consume(lexer); // ')'
				gil_gen_func_call(gen, 0);
			} else {
				if (parse_func_call_after_base(lexer, gen, err, 1) < 0) {
					return -1;
				}

				tok = gil_lexer_peek(lexer, 1);
				if (gil_token_get_kind(tok) != GIL_TOK_CLOSE_PAREN) {
					gil_parse_err(err, tok, "Expected ')', got %s",
							gil_token_get_name(tok));
					return -1;
				}
				gil_lexer_consume(lexer); // ')'
			}
		} else if (
				gil_token_get_kind(tok) == GIL_TOK_PERIOD &&
				gil_token_get_kind(tok2) == GIL_TOK_IDENT &&
				gil_token_get_kind(tok3) == GIL_TOK_EQUALS) {
			gil_trace_scope("namespace assign");
			gil_trace("ident '%s'", gil_token_get_str(tok2));
			struct gil_token_value ident = gil_token_extract_val(tok2);
			gil_lexer_consume(lexer); // '.'
			gil_lexer_consume(lexer); // ident
			gil_lexer_consume(lexer); // '='

			if (parse_expression(lexer, gen, err) < 0) {
			if (!(ident.flags & GIL_TOK_SMALL)) free(ident.str);
				return -1;
			}

			if (ident.flags & GIL_TOK_SMALL) {
				gil_gen_namespace_set_copy(gen, ident.strbuf);
			} else {
				gil_gen_namespace_set(gen, &ident.str);
			}
			gil_gen_swap_discard(gen);
		} else if (
				gil_token_get_kind(tok) == GIL_TOK_PERIOD &&
				gil_token_get_kind(tok2) == GIL_TOK_IDENT) {
			gil_trace_scope("namespace lookup");
			gil_trace("ident '%s'", gil_token_get_str(tok2));
			struct gil_token_value ident = gil_token_extract_val(tok2);
			gil_lexer_consume(lexer); // '.'
			gil_lexer_consume(lexer); // ident

			if (ident.flags & GIL_TOK_SMALL) {
				gil_gen_namespace_lookup_copy(gen, ident.strbuf);
			} else {
				gil_gen_namespace_lookup(gen, &ident.str);
			}
		} else if (
				gil_token_get_kind(tok) == GIL_TOK_DOT_NUMBER &&
				gil_token_get_kind(tok2) == GIL_TOK_EQUALS) {
			gil_trace_scope("direct array assign");
			int number = tok->v.integer;
			gil_lexer_consume(lexer); // dot-number
			gil_lexer_consume(lexer); // '='

			if (parse_expression(lexer, gen, err) < 0) {
				return -1;
			}

			gil_gen_array_set(gen, number);
			gil_gen_swap_discard(gen);
		} else if (gil_token_get_kind(tok) == GIL_TOK_DOT_NUMBER) {
			gil_trace_scope("direct array lookup");
			int number = tok->v.integer;
			gil_lexer_consume(lexer); // dot-number

			gil_gen_array_lookup(gen, number);
		} else if (
				gil_token_get_kind(tok) == GIL_TOK_PERIOD &&
				gil_token_get_kind(tok2) == GIL_TOK_OPEN_PAREN_NS) {
			gil_trace_scope("dynamic lookup");
			gil_lexer_consume(lexer); // '.'
			gil_lexer_consume(lexer); // '('

			if (parse_expression(lexer, gen, err) < 0) {
				return -1;
			}

			tok = gil_lexer_peek(lexer, 1);
			if (gil_token_get_kind(tok) != GIL_TOK_CLOSE_PAREN) {
				gil_parse_err(err, tok, "Expected ')', got %s",
						gil_token_get_name(tok));
				return -1;
			}
			gil_lexer_consume(lexer); // ')'

			if (gil_token_get_kind(gil_lexer_peek(lexer, 1)) == GIL_TOK_EQUALS) {
				gil_lexer_consume(lexer); // '='
				if (parse_expression(lexer, gen, err) < 0) {
					return -1;
				}

				gil_gen_dynamic_set(gen);
			} else {
				gil_gen_dynamic_lookup(gen);
			}
		} else {
			break;
		}

		ret = 1;
	}

	return ret;
}

static int parse_expression(
		struct gil_lexer *lexer, struct gil_generator *gen, struct gil_parse_error *err) {
	gil_trace_scope("expression");
	struct gil_token *tok = gil_lexer_peek(lexer, 1);
	struct gil_token *tok2 = gil_lexer_peek(lexer, 2);

	if (
			gil_token_get_kind(tok) == GIL_TOK_IDENT &&
			gil_token_get_kind(tok2) == GIL_TOK_COLON_EQ) {
		gil_trace_scope("assign expression");
		gil_trace("ident '%s'", gil_token_get_str(tok));
		struct gil_token_value ident = gil_token_extract_val(tok);
		gil_lexer_consume(lexer); // ident
		gil_lexer_consume(lexer); // :=

		if (parse_expression(lexer, gen, err) < 0) {
			if (!(ident.flags & GIL_TOK_SMALL)) free(ident.str);
			return -1;
		}

		if (ident.flags & GIL_TOK_SMALL) {
			gil_gen_stack_frame_set_copy(gen, ident.strbuf);
		} else {
			gil_gen_stack_frame_set(gen, &ident.str);
		}
	} else if (
			gil_token_get_kind(tok) == GIL_TOK_IDENT &&
			gil_token_get_kind(tok2) == GIL_TOK_EQUALS) {
		gil_trace_scope("replacement assign expression");
		gil_trace("ident '%s'", gil_token_get_str(tok));
		struct gil_token_value ident = gil_token_extract_val(tok);
		gil_lexer_consume(lexer); // ident
		gil_lexer_consume(lexer); // =

		if (parse_expression(lexer, gen, err) < 0) {
			if (!(ident.flags & GIL_TOK_SMALL)) free(ident.str);
			return -1;
		}

		if (ident.flags & GIL_TOK_SMALL) {
			gil_gen_stack_frame_replace_copy(gen, ident.strbuf);
		} else {
			gil_gen_stack_frame_replace(gen, &ident.str);
		}
	} else {
		if (parse_arg_level_expression(lexer, gen, err) < 0) {
			return -1;
		}

		if (!tok_is_end(gil_lexer_peek(lexer, 1))) {
			if (parse_func_call_after_base(lexer, gen, err, 0) < 0) {
				return -1;
			}
		}
	}

	return 0;
}

int gil_parse_program(
		struct gil_lexer *lexer, struct gil_generator *gen, struct gil_parse_error *err) {
	gil_trace_scope("program");
	while (1) {
		gil_lexer_skip_opt(lexer, GIL_TOK_EOL);
		if (gil_token_get_kind(gil_lexer_peek(lexer, 1)) == GIL_TOK_EOF) {
			break;
		}

		if (parse_expression(lexer, gen, err) < 0) {
			gil_gen_halt(gen);
			gil_gen_flush(gen);
			return -1;
		}

		gil_gen_discard(gen);
	}

	gil_gen_halt(gen);
	gil_gen_flush(gen);
	return 0;
}
