#ifndef L2_PARSE_LEX_H
#define L2_PARSE_LEX_H

#include "../io.h"

enum l2_token_kind {
	L2_TOK_OPEN_PAREN_NS,
	L2_TOK_OPEN_PAREN,
	L2_TOK_CLOSE_PAREN,
	L2_TOK_OPEN_BRACE,
	L2_TOK_CLOSE_BRACE,
	L2_TOK_OPEN_BRACKET,
	L2_TOK_CLOSE_BRACKET,
	L2_TOK_QUOT,
	L2_TOK_COMMA,
	L2_TOK_PERIOD,
	L2_TOK_DOT_NUMBER,
	L2_TOK_COLON,
	L2_TOK_COLON_EQ,
	L2_TOK_EQUALS,
	L2_TOK_EOL,
	L2_TOK_EOF,
	L2_TOK_NUMBER,
	L2_TOK_STRING,
	L2_TOK_IDENT,
	L2_TOK_ERROR,
};

enum l2_token_flags {
	L2_TOK_SMALL = 1 << 7,
};

const char *l2_token_kind_name(enum l2_token_kind kind);

struct l2_token_value {
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

struct l2_token {
	int line;
	int ch;

	struct l2_token_value v;
};

#define l2_token_get_kind(tok) ((enum l2_token_kind)((tok)->v.flags & ~(1 << 7)))
#define l2_token_get_name(tok) (l2_token_kind_name(l2_token_get_kind(tok)))
#define l2_token_is_small(tok) ((tok)->v.flags & (1 << 7))
void l2_token_free(struct l2_token *tok);
struct l2_token_value l2_token_extract_val(struct l2_token *tok);
void l2_token_print(struct l2_token *tok, struct l2_io_writer *w);

struct l2_lexer {
	struct l2_token toks[4];
	int tokidx;
	int line;
	int ch;
	int parens;
	int do_log_tokens;

	struct l2_bufio_reader reader;
};

void l2_lexer_init(struct l2_lexer *lexer, struct l2_io_reader *r);
struct l2_token *l2_lexer_peek(struct l2_lexer *lexer, int count);
void l2_lexer_consume(struct l2_lexer *lexer);
void l2_lexer_skip_opt(struct l2_lexer *lexer, enum l2_token_kind kind);

#endif
