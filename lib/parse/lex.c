#include "parse/lex.h"

#include <stdlib.h>

static int parse_number(const char *str, double *num) {
	// TODO: Floats
	size_t len = strlen(str);
	*num = 0;
	int power = 1;
	for (int i = (int)len - 1; i >= 0; --i) {
		char ch = str[i];
		if (ch >= '0' && ch <= '9') {
			*num += (ch - '0') * power;
			power *= 10;
		} else {
			return -1;
		}
	}

	return 0;
}

const char *l2_token_kind_name(enum l2_token_kind kind) {
	switch (kind) {
	case L2_TOK_OPEN_PAREN:
		return "open-paren";
	case L2_TOK_CLOSE_PAREN:
		return "close-paren";
	case L2_TOK_OPEN_BRACE:
		return "open-brace";
	case L2_TOK_CLOSE_BRACE:
		return "close-brace";
	case L2_TOK_OPEN_BRACKET:
		return "open-bracket";
	case L2_TOK_CLOSE_BRACKET:
		return "close-bracket";
	case L2_TOK_COMMA:
		return "comma";
	case L2_TOK_PERIOD:
		return "period";
	case L2_TOK_COLON_EQ:
		return "period";
	case L2_TOK_EOF:
		return "end-of-file";
	case L2_TOK_NUMBER:
		return "number";
	case L2_TOK_STRING:
		return "string";
	case L2_TOK_IDENT:
		return "ident";
	case L2_TOK_ERROR:
		return "error";
	}
}

void l2_token_free(struct l2_token *tok) {
	if (tok->kind == L2_TOK_STRING || tok->kind == L2_TOK_IDENT) {
		free(tok->v.str);
	}
}

char *l2_token_extract_str(struct l2_token *tok) {
	char *str = tok->v.str;
	tok->v.str = NULL;
	return str;
}

void l2_lexer_init(struct l2_lexer *lexer, struct l2_io_reader *r) {
	lexer->toks[0].kind = L2_TOK_EOF,
	lexer->tokidx = 0;
	lexer->line = 1;
	lexer->ch = 1;
	l2_bufio_reader_init(&lexer->reader, r);
}

static int peek_ch(struct l2_lexer *lexer) {
	int ch = l2_bufio_peek(&lexer->reader, 1);
	return ch;
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

static void read_ident(struct l2_lexer *lexer, struct l2_token *tok) {
	tok->kind = L2_TOK_IDENT;
	tok->v.str = malloc(16);
	if (tok->v.str == NULL) {
		tok->kind = L2_TOK_ERROR;
		tok->v.str = "Allocaton failure";
		return;
	}

	size_t size = 16;
	size_t idx = 0;

	while (1) {
		int ch = peek_ch(lexer);

		if (is_whitespace(ch)) {
			tok->v.str[idx] = '\0';
			return;
		}

		switch (ch) {
		case '(':
		case ')':
		case '{':
		case '}':
		case '[':
		case ']':
		case ',':
		case '.':
		case ':':
		case EOF:
			tok->v.str[idx] = '\0';
			return;
		}

		tok->v.str[idx++] = (char)read_ch(lexer);
		if (idx + 1 >= size) {
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

	int ch = peek_ch(lexer);
	switch (ch) {
	case '(':
		read_ch(lexer);
		tok->kind = L2_TOK_OPEN_PAREN;
		break;

	case ')':
		read_ch(lexer);
		tok->kind = L2_TOK_CLOSE_PAREN;
		break;

	case '{':
		read_ch(lexer);
		tok->kind = L2_TOK_OPEN_BRACE;
		break;

	case '}':
		read_ch(lexer);
		tok->kind = L2_TOK_CLOSE_BRACE;
		break;

	case '[':
		read_ch(lexer);
		tok->kind = L2_TOK_OPEN_BRACKET;
		break;

	case ']':
		read_ch(lexer);
		tok->kind = L2_TOK_CLOSE_BRACKET;
		break;

	case ',':
		read_ch(lexer);
		tok->kind = L2_TOK_COMMA;
		break;

	case '.':
		read_ch(lexer);
		tok->kind = L2_TOK_PERIOD;
		break;

	case ':':
		read_ch(lexer);
		{
			ch = read_ch(lexer);
			switch (ch) {
			case '=':
				tok->kind = L2_TOK_COLON_EQ;
				break;

			default:
				tok->kind = L2_TOK_ERROR;
				tok->v.str = "Unexpected character";
				break;
			}
		}
		break;

	case EOF:
		tok->kind = L2_TOK_EOF;
		break;

	case '"':
		read_ch(lexer);
		read_string(lexer, tok);
		break;

	default:
		read_ident(lexer, tok);
		if (tok->kind != L2_TOK_IDENT) {
			break;
		}

		double num;
		if (parse_number(tok->v.str, &num) >= 0) {
			free(tok->v.str);
			tok->kind = L2_TOK_NUMBER;
			tok->v.num = num;
		}
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

void l2_lexer_consume(struct l2_lexer *lexer) {
	l2_token_free(&lexer->toks[0]);
	lexer->tokidx -= 1;
	memmove(lexer->toks, lexer->toks + 1, lexer->tokidx * sizeof(*lexer->toks));
}
