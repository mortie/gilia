#ifndef GIL_PARSE_LEX_H
#define GIL_PARSE_LEX_H

#include "../io.h"

enum gil_token_kind {
	GIL_TOK_OPEN_PAREN_NS,
	GIL_TOK_OPEN_PAREN,
	GIL_TOK_CLOSE_PAREN,
	GIL_TOK_OPEN_BRACE,
	GIL_TOK_CLOSE_BRACE,
	GIL_TOK_OPEN_BRACKET,
	GIL_TOK_CLOSE_BRACKET,
	GIL_TOK_QUOT,
	GIL_TOK_COMMA,
	GIL_TOK_PERIOD,
	GIL_TOK_DOT_NUMBER,
	GIL_TOK_COLON,
	GIL_TOK_COLON_EQ,
	GIL_TOK_EQUALS,
	GIL_TOK_PIPE,
	GIL_TOK_EOL,
	GIL_TOK_EOF,
	GIL_TOK_NUMBER,
	GIL_TOK_STRING,
	GIL_TOK_IDENT,
	GIL_TOK_ERROR,
};

enum gil_token_flags {
	GIL_TOK_SMALL = 1 << 7,
};

const char *gil_token_kind_name(enum gil_token_kind kind);

struct gil_token_value {
	union {
		struct {
			unsigned char flags;
			union {
				char *str;
				double num;
				int integer;
			};
		};

		struct {
			unsigned char padding;
			char strbuf[15];
		};
	};
};

struct gil_token {
	int line;
	int ch;

	struct gil_token_value v;
};

#define gil_token_get_kind(tok) ((enum gil_token_kind)((tok)->v.flags & ~(1 << 7)))
#define gil_token_get_name(tok) (gil_token_kind_name(gil_token_get_kind(tok)))
#define gil_token_is_small(tok) ((tok)->v.flags & GIL_TOK_SMALL)
void gil_token_free(struct gil_token *tok);
struct gil_token_value gil_token_extract_val(struct gil_token *tok);
const char *gil_token_get_str(struct gil_token_value *val);
void gil_token_print(struct gil_token *tok, struct gil_io_writer *w);

struct gil_lexer {
	struct gil_token toks[4];
	int tokidx;
	int line;
	int ch;
	int parens;
	int prev_tok_is_expr;

	struct gil_bufio_reader reader;
};

void gil_lexer_init(struct gil_lexer *lexer, struct gil_io_reader *r);
struct gil_token *gil_lexer_peek(struct gil_lexer *lexer, int count);
void gil_lexer_consume(struct gil_lexer *lexer);
void gil_lexer_skip_opt(struct gil_lexer *lexer, enum gil_token_kind kind);

#endif
