#include "parse/lex.h"

#include <stdio.h>
#include <snow/snow.h>

static struct l2_lexer lexer;
static struct l2_io_mem_reader r;

static void lex(const char *str) {
	r.r.read = l2_io_mem_read;
	r.idx = 0;
	r.len = strlen(str);
	r.mem = str;
	l2_lexer_init(&lexer, &r.r);
}

describe(lex) {
	test("lex assignment") {
		lex("foo := 10");

		asserteq(l2_token_get_kind(l2_lexer_peek(&lexer, 1)), L2_TOK_IDENT);
		asserteq(l2_lexer_peek(&lexer, 1)->v.strbuf, "foo");
		l2_lexer_consume(&lexer);

		asserteq(l2_token_get_kind(l2_lexer_peek(&lexer, 1)), L2_TOK_COLON_EQ);
		l2_lexer_consume(&lexer);

		asserteq(l2_token_get_kind(l2_lexer_peek(&lexer, 1)), L2_TOK_NUMBER);
		asserteq(l2_lexer_peek(&lexer, 1)->v.num, 10);
		l2_lexer_consume(&lexer);

		asserteq(l2_token_get_kind(l2_lexer_peek(&lexer, 1)), L2_TOK_EOF);
	}

	test("lex assignment, non-sso") {
		lex("foo-very-long-name := 10");

		asserteq(l2_token_get_kind(l2_lexer_peek(&lexer, 1)), L2_TOK_IDENT);
		asserteq(l2_lexer_peek(&lexer, 1)->v.str, "foo-very-long-name");
		l2_lexer_consume(&lexer);

		asserteq(l2_token_get_kind(l2_lexer_peek(&lexer, 1)), L2_TOK_COLON_EQ);
		l2_lexer_consume(&lexer);

		asserteq(l2_token_get_kind(l2_lexer_peek(&lexer, 1)), L2_TOK_NUMBER);
		asserteq(l2_lexer_peek(&lexer, 1)->v.num, 10);
		l2_lexer_consume(&lexer);

		asserteq(l2_token_get_kind(l2_lexer_peek(&lexer, 1)), L2_TOK_EOF);
	}

	test("lex var deref assignment") {
		lex("foo := 10\nbar := foo");

		asserteq(l2_token_get_kind(l2_lexer_peek(&lexer, 1)), L2_TOK_IDENT);
		asserteq(l2_lexer_peek(&lexer, 1)->v.strbuf, "foo");
		l2_lexer_consume(&lexer);

		asserteq(l2_token_get_kind(l2_lexer_peek(&lexer, 1)), L2_TOK_COLON_EQ);
		l2_lexer_consume(&lexer);

		asserteq(l2_token_get_kind(l2_lexer_peek(&lexer, 1)), L2_TOK_NUMBER);
		asserteq(l2_lexer_peek(&lexer, 1)->v.num, 10);
		l2_lexer_consume(&lexer);

		asserteq(l2_token_get_kind(l2_lexer_peek(&lexer, 1)), L2_TOK_EOL);
		l2_lexer_consume(&lexer);

		asserteq(l2_token_get_kind(l2_lexer_peek(&lexer, 1)), L2_TOK_IDENT);
		asserteq(l2_lexer_peek(&lexer, 1)->v.strbuf, "bar");
		l2_lexer_consume(&lexer);

		asserteq(l2_token_get_kind(l2_lexer_peek(&lexer, 1)), L2_TOK_COLON_EQ);
		l2_lexer_consume(&lexer);

		asserteq(l2_token_get_kind(l2_lexer_peek(&lexer, 1)), L2_TOK_IDENT);
		asserteq(l2_lexer_peek(&lexer, 1)->v.strbuf, "foo");
		l2_lexer_consume(&lexer);

		asserteq(l2_token_get_kind(l2_lexer_peek(&lexer, 1)), L2_TOK_EOF);
	}

	test("lex peek multiple") {
		lex("foo := 10");

		l2_lexer_peek(&lexer, 3);

		asserteq(l2_token_get_kind(l2_lexer_peek(&lexer, 1)), L2_TOK_IDENT);
		asserteq(l2_lexer_peek(&lexer, 1)->v.strbuf, "foo");
		l2_lexer_consume(&lexer);

		asserteq(l2_token_get_kind(l2_lexer_peek(&lexer, 1)), L2_TOK_COLON_EQ);
		l2_lexer_consume(&lexer);

		asserteq(l2_token_get_kind(l2_lexer_peek(&lexer, 1)), L2_TOK_NUMBER);
		asserteq(l2_lexer_peek(&lexer, 1)->v.num, 10);
		l2_lexer_consume(&lexer);

		asserteq(l2_token_get_kind(l2_lexer_peek(&lexer, 1)), L2_TOK_EOF);
	}
}
