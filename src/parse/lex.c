#include "parse/lex.h"

#include <stdlib.h>

void l2_token_free(struct l2_token *tok) {
	if (tok->kind == L2_TOK_STRING) {
		free(tok->v.str);
	}
}

struct l2_token l2_token_move(struct l2_token *tok) {
	struct l2_token dup = *tok;
	if (tok->kind == L2_TOK_STRING) {
		tok->v.str = NULL;
	}

	return dup;
}

void l2_lexer_init(struct l2_lexer *lexer, struct l2_io_reader *r) {
	lexer->currtok.kind = L2_TOK_EOF,
	lexer->tokidx = 0;
	lexer->line = 1;
	lexer->ch = 1;
	l2_bufio_reader_init(&lexer->reader, r);
}

static int read_ch(struct l2_lexer *lexer) {
	int ch = l2_bufio_get(&lexer->reader);
	lexer->ch += 1;
	if (ch == '\n') {
		lexer->ch = 1;
		lexer->line += 1;
	}

	return ch;
}

static int is_whitespace(int ch) {
	return ch == ' ' || ch == '\r' || ch == '\n' || ch == '\t';
}

static void skip_whitespace(struct l2_lexer *lexer) {
	while (is_whitespace(l2_bufio_peek(&lexer->reader, 1))) read_ch(lexer);
}

static void read_string(struct l2_lexer *lexer, struct l2_token *tok) {
	tok->kind = L2_TOK_STRING;
	tok->v.str = malloc(16);
	if (tok->v.str == NULL) {
		tok->kind = L2_TOK_ERROR;
		tok->v.str = "Allocaton failure";
		return;
	}

	size_t size = 16;
	size_t idx = 0;

	while (1) {
		int ch = read_ch(lexer);
		if (ch == '"') {
			return;
		} else if (ch == EOF) {
			tok->kind = L2_TOK_EOF;
			free(tok->v.str);
			tok->v.str = "Unexpected EOF";
			return;
		} else if (ch == '\\') {
			int ch2 = read_ch(lexer);
			switch (ch2) {
			case 'n':
				ch = '\n';
				break;

			case 'r':
				ch = '\r';
				break;

			case 't':
				ch = '\t';
				break;

			case EOF:
				tok->kind = L2_TOK_EOF;
				free(tok->v.str);
				tok->v.str = "Unexpected EOF";
				return;

			default:
				ch = ch2;
				break;
			}
		}

		tok->v.str[idx++] = (char)ch;
		if (idx >= size) {
			size *= 2;
			char *newbuf = realloc(tok->v.str, size);
			if (newbuf == NULL) {
				free(tok->v.str);
				tok->kind = L2_TOK_ERROR;
				tok->v.str = "Allocation failure";
				return;
			}

			tok->v.str = newbuf;
		}
	}
}

static void read_tok(struct l2_lexer *lexer, struct l2_token *tok) {
	skip_whitespace(lexer);

	tok->line = lexer->line;
	tok->ch = lexer->ch;

	int ch = read_ch(lexer);
	switch (ch) {
	case '(':
		tok->kind = L2_TOK_OPEN_PAREN;
		break;

	case ')':
		tok->kind = L2_TOK_CLOSE_PAREN;
		break;

	case '{':
		tok->kind = L2_TOK_OPEN_BRACE;
		break;

	case '}':
		tok->kind = L2_TOK_CLOSE_BRACE;
		break;

	case '[':
		tok->kind = L2_TOK_OPEN_BRACKET;
		break;

	case ']':
		tok->kind = L2_TOK_CLOSE_BRACKET;
		break;

	case ',':
		tok->kind = L2_TOK_COMMA;
		break;

	case '.':
		tok->kind = L2_TOK_PERIOD;
		break;

	case EOF:
		tok->kind = L2_TOK_EOF;
		break;

	case '"':
		read_string(lexer, tok);
		break;
	}
}

struct l2_token *l2_lexer_peek(struct l2_lexer *lexer, int count) {
	int offset = count - 1;

	while (offset >= lexer->tokidx) {
		read_tok(lexer, &lexer->toks[lexer->tokidx++]);
	}

	return &lexer->toks[offset];
}

struct l2_token *l2_lexer_get(struct l2_lexer *lexer) {
	l2_token_free(&lexer->currtok);

	if (lexer->tokidx == 0) {
		read_tok(lexer, &lexer->currtok);
	} else {
		memmove(lexer->toks, lexer->toks + 1, lexer->tokidx - 1);
		lexer->tokidx -= 1;
	}

	return &lexer->currtok;
}
