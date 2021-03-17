#include "parse/lex.h"

#include <stdlib.h>

static void log_token(struct l2_token *tok) {
	switch (l2_token_get_kind(tok)) {
	case L2_TOK_STRING:
	case L2_TOK_IDENT:
	case L2_TOK_ERROR:
		if (l2_token_is_small(tok)) {
			printf("%i:%i\t%s '%s'\n", tok->line, tok->ch,
					l2_token_get_name(tok), tok->v.strbuf);
		} else {
			printf("%i:%i\t%s '%s'\n", tok->line, tok->ch,
					l2_token_get_name(tok), tok->v.str);
		}
		break;

	case L2_TOK_NUMBER:
		printf("%i:%i\t%s '%g'\n", tok->line, tok->ch,
				l2_token_get_name(tok), tok->v.num);
		break;

	default:
		printf("%i:%i\t%s\n", tok->line, tok->ch,
				l2_token_get_name(tok));
		break;
	}
}

const char *l2_token_kind_name(enum l2_token_kind kind) {
	switch (kind) {
	case L2_TOK_OPEN_PAREN_NS:
		return "open-paren-no-space";
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
	case L2_TOK_QUOT:
		return "single-quote";
	case L2_TOK_COMMA:
		return "comma";
	case L2_TOK_PERIOD:
		return "period";
	case L2_TOK_DOT_NUMBER:
		return "dot-number";
	case L2_TOK_COLON:
		return "colon";
	case L2_TOK_COLON_EQ:
		return "colon-equals";
	case L2_TOK_EQUALS:
		return "equals";
	case L2_TOK_EOL:
		return "end-of-line";
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

	return "(unknown)";
}

void l2_token_free(struct l2_token *tok) {
	enum l2_token_kind kind = l2_token_get_kind(tok);
	if (
			(kind == L2_TOK_STRING || kind == L2_TOK_IDENT) &&
			!l2_token_is_small(tok)) {
		free(tok->v.str);
	}
}

struct l2_token_value l2_token_extract_val(struct l2_token *tok) {
	struct l2_token_value v = tok->v;
	tok->v.str = NULL;
	return v;
}

void l2_lexer_init(struct l2_lexer *lexer, struct l2_io_reader *r) {
	lexer->toks[0].v.flags = L2_TOK_EOF,
	lexer->tokidx = 0;
	lexer->line = 1;
	lexer->ch = 1;
	lexer->parens = 0;
	lexer->do_log_tokens = 0;
	l2_bufio_reader_init(&lexer->reader, r);
}

static int peek_ch(struct l2_lexer *lexer) {
	int ch = l2_bufio_peek(&lexer->reader, 1);
	return ch;
}

static int peek_ch_n(struct l2_lexer *lexer, int n) {
	int ch = l2_bufio_peek(&lexer->reader, n);
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

static int is_numeric(int ch) {
	return ch >= '0' && ch <= '9';
}

static int is_ident(int ch) {
	return !is_whitespace(ch) && ch != EOF &&
		ch != '(' && ch != ')' &&
		ch != '{' && ch != '}' &&
		ch != '[' && ch != ']' &&
		ch != '\'' &&
		ch != ',' && ch != '.' &&
		ch != ':' && ch != ';';
}

static void skip_whitespace(struct l2_lexer *lexer, int *nl, int *skipped) {
	while (1) {
		if (is_whitespace(peek_ch(lexer))) {
			*skipped = 1;
			do {
				int ch = read_ch(lexer);
				if (ch == '\n') {
					*nl = 1;
				}
			} while (is_whitespace(peek_ch(lexer)));
		}

		if (peek_ch(lexer) == '#') {
			*nl = 1;
			while (read_ch(lexer) != '\n');
		} else {
			break;
		}

		*skipped = 1;
	}
}

static int read_integer(struct l2_lexer *lexer, long long *num, long long *base, char **err) {
	unsigned char buffer[32]; // Should be enough
	size_t len = 0;

	long long b = -1;
	while (len < sizeof(buffer)) {
		int ch = peek_ch(lexer);
		if (is_numeric(ch)) {
			buffer[len++] = ch;
		} else if (ch != '\'') {
			 break;
		}

		read_ch(lexer);
	}

	int ch = peek_ch(lexer);
	int abbrev_prefix = len == 1 && buffer[0] == '0';
	if (abbrev_prefix && ch == 'b') {
		b = 2;
	} else if (abbrev_prefix && ch == 'o') {
		b = 8;
	} else if (abbrev_prefix && ch == 'x') {
		b = 16;
	} else {
		// Need to parse the number as base 10 now
		long long n = 0;
		long long pow = 1;
		for (ssize_t i = len - 1; i >= 0; --i) {
			n += (buffer[i] - '0') * pow;
			pow *= 10;
		}

		if (ch == 'r') {
			b = n;
		} else {
			*num = n;
			*base = 10;
			return 0;
		}
	}

	if (b < 2) {
		*err = "Number with base lower than 2";
		return -1;
	} else if (b > 36) {
		*err = "Number with base higher than 36";
		return -1;
	}

	// Now that we know the base, we can read in the next part of the number
	read_ch(lexer); // Skip the base marker ('x', 'r', etc)
	len = 0;
	while (len < sizeof(buffer)) {
		int ch = peek_ch(lexer);
		if (ch == '\'') {
			read_ch(lexer);
			continue;
		}

		int digit;
		if (ch >= '0' && ch <= '9') {
			digit = ch - '0';
		} else if (ch >= 'a' && ch <= 'z') {
			digit = ch - 'a' + 10;
		} else if (ch >= 'A' && ch <= 'Z') {
			digit = ch - 'A' + 10;
		} else {
			break;
		}

		if (digit >= b) {
			*err = "Number with digit too big for the base";
			return -1;
		}

		buffer[len++] = digit;
		read_ch(lexer);
	}

	if (len < 1) {
		*err = "Number with no digits";
	}

	long long n = 0;
	long long pow = 1;
	for (ssize_t i = len - 1; i >= 0; --i) {
		n += buffer[i] * pow;
		pow *= b;
	}

	*num = n;
	*base = b;
	return 0;
}

static void read_number(struct l2_lexer *lexer, struct l2_token *tok) {
	tok->v.flags = L2_TOK_NUMBER;

	float sign = 1;
	if (peek_ch(lexer) == '-') {
		sign = -1;
		read_ch(lexer);
	}

	if (!is_numeric(peek_ch(lexer))) {
		tok->v.flags = L2_TOK_ERROR;
		tok->v.str = "No number in number literal";
		return;
	}

	long long integral;
	long long base;
	char *err;
	if (read_integer(lexer, &integral, &base, &err) < 0) {
		tok->v.flags = L2_TOK_ERROR;
		tok->v.str = err;
		return;
	}

	if (peek_ch(lexer) != '.') {
		tok->v.num = (double)integral * sign;
		return;
	}

	read_ch(lexer); // '.'

	unsigned char buffer[32];
	size_t fraction_len = 0;
	while (fraction_len < sizeof(buffer)) {
		int ch = peek_ch(lexer);
		if (ch == '\'') {
			read_ch(lexer);
			continue;
		}

		int digit;
		if (ch >= '0' && ch <= '9') {
			digit = ch - '0';
		} else if (ch >= 'a' && ch <= 'z') {
			digit = ch - 'a' + 10;
		} else if (ch >= 'A' && ch <= 'Z') {
			digit = ch - 'A' + 10;
		} else {
			break;
		}

		if (digit >= base) {
			tok->v.flags = L2_TOK_ERROR;
			tok->v.str = "Number with digits too big for the base";
			return;
		}

		buffer[fraction_len++] = digit;
		read_ch(lexer);
	}

	if (fraction_len < 1) {
		tok->v.flags = L2_TOK_ERROR;
		tok->v.str = "Trailing dot in number literal";
		return;
	}

	long long fraction = 0;
	long long fraction_power = 1;
	for (ssize_t i = fraction_len - 1; (ssize_t)i >= 0; --i) {
		fraction += buffer[i] * fraction_power;
		fraction_power *= base;
	}

	double num = (double)integral + ((double)fraction / (double)fraction_power);
	tok->v.num = num * sign;
}

static void read_string(struct l2_lexer *lexer, struct l2_token *tok) {
	tok->v.flags = L2_TOK_STRING | L2_TOK_SMALL;

	char *dest = tok->v.strbuf;
	size_t size = sizeof(tok->v.strbuf);
	size_t idx = 0;

	while (1) {
		int ch = read_ch(lexer);
		if (ch == '"') {
			dest[idx] = '\0';
			return;
		} else if (ch == EOF) {
			if (!l2_token_is_small(tok)) {
				free(tok->v.str);
			}
			tok->v.flags = L2_TOK_ERROR;
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
				if (!l2_token_is_small(tok)) {
					free(tok->v.str);
				}
				tok->v.flags = L2_TOK_ERROR;
				tok->v.str = "Unexpected EOF";
				return;

			default:
				ch = ch2;
				break;
			}
		}

		dest[idx++] = (char)ch;

		// The first time we run out of space, we have to switch away from
		// the small-string optimization and malloc memory.
		if (idx + 1 >= size) {
			char *newbuf;
			if (l2_token_is_small(tok)) {
				tok->v.flags &= ~L2_TOK_SMALL;
				size = 32;
				newbuf = malloc(size);
				if (newbuf == NULL) {
					tok->v.flags = L2_TOK_ERROR;
					tok->v.str = "Allocation failure";
					return;
				}
				memcpy(newbuf, tok->v.strbuf, idx);
			} else {
				size *= 2;
				newbuf = realloc(tok->v.str, size);
				if (newbuf == NULL) {
					free(tok->v.str);
					tok->v.flags = L2_TOK_ERROR;
					tok->v.str = "Allocation failure";
					return;
				}
			}

			tok->v.str = newbuf;
			dest = newbuf;
		}
	}
}

static void read_ident(struct l2_lexer *lexer, struct l2_token *tok) {
	tok->v.flags = L2_TOK_IDENT | L2_TOK_SMALL;

	char *dest = tok->v.strbuf;
	size_t size = sizeof(tok->v.strbuf);
	size_t idx = 0;

	while (1) {
		int ch = peek_ch(lexer);

		if (!is_ident(ch)) {
			dest[idx] = '\0';
			return;
		}

		dest[idx++] = (char)read_ch(lexer);

		// The first time we run out of space, we have to switch away from
		// the small-string optimization and malloc memory.
		if (idx + 1 >= size) {
			char *newbuf;
			if (l2_token_is_small(tok)) {
				tok->v.flags &= ~L2_TOK_SMALL;
				size = 32;
				newbuf = malloc(size);
				if (newbuf == NULL) {
					tok->v.flags = L2_TOK_ERROR;
					tok->v.str = "Allocation failure";
					return;
				}
				memcpy(newbuf, tok->v.strbuf, idx);
			} else {
				size *= 2;
				newbuf = realloc(tok->v.str, size);
				if (newbuf == NULL) {
					free(tok->v.str);
					tok->v.flags = L2_TOK_ERROR;
					tok->v.str = "Allocation failure";
					return;
				}
			}

			tok->v.str = newbuf;
			dest = newbuf;
		}
	}
}

static void read_tok(struct l2_lexer *lexer, struct l2_token *tok) {
	tok->line = lexer->line;
	tok->ch = lexer->ch;
	int nl = 0, skipped_whitespace = 0;
	skip_whitespace(lexer, &nl, &skipped_whitespace);

	if (nl && lexer->parens == 0) {
		tok->v.flags = L2_TOK_EOL;
		return;
	}

	int ch = peek_ch(lexer);
	switch (ch) {
	case '(':
		read_ch(lexer);
		if (skipped_whitespace) {
			tok->v.flags = L2_TOK_OPEN_PAREN;
		} else {
			tok->v.flags =L2_TOK_OPEN_PAREN_NS;
		}
		lexer->parens += 1;
		break;

	case ')':
		read_ch(lexer);
		tok->v.flags = L2_TOK_CLOSE_PAREN;
		lexer->parens -= 1;
		break;

	case '{':
		read_ch(lexer);
		tok->v.flags = L2_TOK_OPEN_BRACE;
		break;

	case '}':
		read_ch(lexer);
		tok->v.flags = L2_TOK_CLOSE_BRACE;
		break;

	case '[':
		read_ch(lexer);
		tok->v.flags = L2_TOK_OPEN_BRACKET;
		break;

	case ']':
		read_ch(lexer);
		tok->v.flags = L2_TOK_CLOSE_BRACKET;
		break;

	case ';':
		tok->v.flags = L2_TOK_EOL;
		do {
			read_ch(lexer);
			skip_whitespace(lexer, &nl, &skipped_whitespace);
		} while (peek_ch(lexer) == ';');
		break;

	case '\'':
		read_ch(lexer);
		tok->v.flags = L2_TOK_QUOT;
		break;

	case ',':
		read_ch(lexer);
		tok->v.flags = L2_TOK_COMMA;
		break;

	case '.':
		read_ch(lexer);
		if (is_numeric(peek_ch(lexer))) {
			tok->v.flags = L2_TOK_DOT_NUMBER;
			long long num, base;
			char *err;
			if (read_integer(lexer, &num, &base, &err) < 0) {
				tok->v.flags = L2_TOK_ERROR;
				tok->v.str = err;
			} else {
				tok->v.integer = (int)num;
			}
		} else {
			tok->v.flags = L2_TOK_PERIOD;
		}
		break;

	case ':':
		read_ch(lexer);
		ch = peek_ch(lexer);
		switch (ch) {
		case '=':
			read_ch(lexer);
			tok->v.flags = L2_TOK_COLON_EQ;
			break;

		default:
			tok->v.flags = L2_TOK_COLON;
			break;
		}
		break;

	case EOF:
		tok->v.flags = L2_TOK_EOF;
		break;

	case '"':
		read_ch(lexer);
		read_string(lexer, tok);
		break;

	default:
		{
			int ch2 = peek_ch_n(lexer, 2);

			if (
					is_numeric(ch) ||
					(ch == '-' && is_numeric(ch2))) {
				read_number(lexer, tok);
				break;
			}

			tok->v.flags = L2_TOK_IDENT;
			read_ident(lexer, tok);

			if (l2_token_is_small(tok) && strcmp(tok->v.strbuf, "=") == 0) {
				tok->v.flags = L2_TOK_EQUALS;
			}
		}
	}
}

struct l2_token *l2_lexer_peek(struct l2_lexer *lexer, int count) {
	int offset = count - 1;

	while (offset >= lexer->tokidx) {
		read_tok(lexer, &lexer->toks[lexer->tokidx++]);
		if (lexer->do_log_tokens) {
			log_token(&lexer->toks[lexer->tokidx - 1]);
		}
	}

	return &lexer->toks[offset];
}

void l2_lexer_consume(struct l2_lexer *lexer) {
	l2_token_free(&lexer->toks[0]);
	lexer->tokidx -= 1;
	memmove(lexer->toks, lexer->toks + 1, lexer->tokidx * sizeof(*lexer->toks));
}

void l2_lexer_skip_opt(struct l2_lexer *lexer, enum l2_token_kind kind) {
	if (l2_token_get_kind(l2_lexer_peek(lexer, 1)) == kind) {
		l2_lexer_consume(lexer);
	}
}
