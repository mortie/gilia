#ifndef L2_PARSE_LEX_H
#define L2_PARSE_LEX_H

#include "../io.h"

enum l2_token_kind {
	L2_TOK_OPEN_PAREN,
	L2_TOK_CLOSE_PAREN,
	L2_TOK_OPEN_BRACE,
	L2_TOK_CLOSE_BRACE,
	L2_TOK_OPEN_BRACKET,
	L2_TOK_CLOSE_BRACKET,
	L2_TOK_COMMA,
	L2_TOK_PERIOD,
	L2_TOK_COLON_EQ,
	L2_TOK_EOL,
	L2_TOK_EOF,
	L2_TOK_NUMBER,
	L2_TOK_STRING,
	L2_TOK_IDENT,
	L2_TOK_ERROR,
};

const char *l2_token_kind_name(enum l2_token_kind kind);

struct l2_token {
	enum l2_token_kind kind;
	int line;
	int ch;

	union {
		char *str;
		double num;
	} v;
};

void l2_token_free(struct l2_token *tok);
char *l2_token_extract_str(struct l2_token *tok);
void l2_token_print(struct l2_token *tok, struct l2_io_writer *w);

struct l2_lexer {
	struct l2_token toks[4];
	int tokidx;
	int line;
	int ch;
	int parens;

	struct l2_bufio_reader reader;
};

void l2_lexer_init(struct l2_lexer *lexer, struct l2_io_reader *r);
struct l2_token *l2_lexer_peek(struct l2_lexer *lexer, int count);
void l2_lexer_consume(struct l2_lexer *lexer);
void l2_lexer_skip_opt(struct l2_lexer *lexer, enum l2_token_kind kind);

#endif
