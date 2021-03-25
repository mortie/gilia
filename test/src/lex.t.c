#include "parse/lex.h"

#include <stdio.h>
#include <snow/snow.h>

static struct gil_lexer lexer;
static struct gil_io_mem_reader r;

static void lex(const char *str) {
	r.r.read = gil_io_mem_read;
	r.idx = 0;
	r.len = strlen(str);
	r.mem = str;
	gil_lexer_init(&lexer, &r.r);
}

describe(lex) {
	test("lex assignment") {
		lex("foo := 10");

		asserteq(gil_token_get_kind(gil_lexer_peek(&lexer, 1)), GIL_TOK_IDENT);
		asserteq(gil_lexer_peek(&lexer, 1)->v.strbuf, "foo");
		gil_lexer_consume(&lexer);

		asserteq(gil_token_get_kind(gil_lexer_peek(&lexer, 1)), GIL_TOK_COLON_EQ);
		gil_lexer_consume(&lexer);

		asserteq(gil_token_get_kind(gil_lexer_peek(&lexer, 1)), GIL_TOK_NUMBER);
		asserteq(gil_lexer_peek(&lexer, 1)->v.num, 10);
		gil_lexer_consume(&lexer);

		asserteq(gil_token_get_kind(gil_lexer_peek(&lexer, 1)), GIL_TOK_EOF);
	}

	test("lex assignment, non-sso") {
		lex("foo-very-long-name := 10");

		asserteq(gil_token_get_kind(gil_lexer_peek(&lexer, 1)), GIL_TOK_IDENT);
		asserteq(gil_lexer_peek(&lexer, 1)->v.str, "foo-very-long-name");
		gil_lexer_consume(&lexer);

		asserteq(gil_token_get_kind(gil_lexer_peek(&lexer, 1)), GIL_TOK_COLON_EQ);
		gil_lexer_consume(&lexer);

		asserteq(gil_token_get_kind(gil_lexer_peek(&lexer, 1)), GIL_TOK_NUMBER);
		asserteq(gil_lexer_peek(&lexer, 1)->v.num, 10);
		gil_lexer_consume(&lexer);

		asserteq(gil_token_get_kind(gil_lexer_peek(&lexer, 1)), GIL_TOK_EOF);
	}

	test("lex var deref assignment") {
		lex("foo := 10\nbar := foo");

		asserteq(gil_token_get_kind(gil_lexer_peek(&lexer, 1)), GIL_TOK_IDENT);
		asserteq(gil_lexer_peek(&lexer, 1)->v.strbuf, "foo");
		gil_lexer_consume(&lexer);

		asserteq(gil_token_get_kind(gil_lexer_peek(&lexer, 1)), GIL_TOK_COLON_EQ);
		gil_lexer_consume(&lexer);

		asserteq(gil_token_get_kind(gil_lexer_peek(&lexer, 1)), GIL_TOK_NUMBER);
		asserteq(gil_lexer_peek(&lexer, 1)->v.num, 10);
		gil_lexer_consume(&lexer);

		asserteq(gil_token_get_kind(gil_lexer_peek(&lexer, 1)), GIL_TOK_EOL);
		gil_lexer_consume(&lexer);

		asserteq(gil_token_get_kind(gil_lexer_peek(&lexer, 1)), GIL_TOK_IDENT);
		asserteq(gil_lexer_peek(&lexer, 1)->v.strbuf, "bar");
		gil_lexer_consume(&lexer);

		asserteq(gil_token_get_kind(gil_lexer_peek(&lexer, 1)), GIL_TOK_COLON_EQ);
		gil_lexer_consume(&lexer);

		asserteq(gil_token_get_kind(gil_lexer_peek(&lexer, 1)), GIL_TOK_IDENT);
		asserteq(gil_lexer_peek(&lexer, 1)->v.strbuf, "foo");
		gil_lexer_consume(&lexer);

		asserteq(gil_token_get_kind(gil_lexer_peek(&lexer, 1)), GIL_TOK_EOF);
	}

	test("lex peek multiple") {
		lex("foo := 10");

		gil_lexer_peek(&lexer, 3);

		asserteq(gil_token_get_kind(gil_lexer_peek(&lexer, 1)), GIL_TOK_IDENT);
		asserteq(gil_lexer_peek(&lexer, 1)->v.strbuf, "foo");
		gil_lexer_consume(&lexer);

		asserteq(gil_token_get_kind(gil_lexer_peek(&lexer, 1)), GIL_TOK_COLON_EQ);
		gil_lexer_consume(&lexer);

		asserteq(gil_token_get_kind(gil_lexer_peek(&lexer, 1)), GIL_TOK_NUMBER);
		asserteq(gil_lexer_peek(&lexer, 1)->v.num, 10);
		gil_lexer_consume(&lexer);

		asserteq(gil_token_get_kind(gil_lexer_peek(&lexer, 1)), GIL_TOK_EOF);
	}
}
