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
	L2_TOK_NUMBER,
	L2_TOK_EOF,
	L2_TOK_STRING,
	L2_TOK_ERROR,
};

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
struct l2_token l2_token_move(struct l2_token *tok);

struct l2_lexer {
	struct l2_token currtok;
	struct l2_token toks[2];
	int tokidx;
	int line;
	int ch;

	struct l2_bufio_reader reader;
};

void l2_lexer_init(struct l2_lexer *lexer, struct l2_io_reader *r);
struct l2_token *l2_lexer_peek(struct l2_lexer *lexer, int count);
struct l2_token *l2_lexer_get(struct l2_lexer *lexer);

#endif
