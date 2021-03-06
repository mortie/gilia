#include "parse/lex.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include "io.h"

#define GIL_TRACER_NAME "lexer"
#include "trace.h"

#ifdef GIL_ENABLE_TRACE
static void trace_token(struct gil_token *tok) {
	switch (gil_token_get_kind(tok)) {
	case GIL_TOK_STRING:
	case GIL_TOK_IDENT:
	case GIL_TOK_ERROR:
		if (gil_token_is_small(tok)) {
			gil_trace("%i:%i %s '%s'", tok->line, tok->ch,
					gil_token_get_name(tok), tok->v.strbuf);
		} else {
			printf("%i:%i %s '%s'", tok->line, tok->ch,
					gil_token_get_name(tok), tok->v.str);
		}
		break;

	case GIL_TOK_NUMBER:
		gil_trace("%i:%i %s '%g'", tok->line, tok->ch,
				gil_token_get_name(tok), tok->v.num);
		break;

	default:
		gil_trace("%i:%i %s", tok->line, tok->ch,
				gil_token_get_name(tok));
		break;
	}
}
#endif

const char *gil_token_kind_name(enum gil_token_kind kind) {
	switch (kind) {
	case GIL_TOK_OPEN_PAREN_NS:
		return "open-paren-no-space";
	case GIL_TOK_OPEN_PAREN:
		return "open-paren";
	case GIL_TOK_CLOSE_PAREN:
		return "close-paren";
	case GIL_TOK_OPEN_BRACE:
		return "open-brace";
	case GIL_TOK_CLOSE_BRACE:
		return "close-brace";
	case GIL_TOK_OPEN_BRACKET:
		return "open-bracket";
	case GIL_TOK_CLOSE_BRACKET:
		return "close-bracket";
	case GIL_TOK_QUOT:
		return "single-quote";
	case GIL_TOK_COMMA:
		return "comma";
	case GIL_TOK_PERIOD:
		return "period";
	case GIL_TOK_DOT_NUMBER:
		return "dot-number";
	case GIL_TOK_COLON:
		return "colon";
	case GIL_TOK_COLON_EQ:
		return "colon-equals";
	case GIL_TOK_EQUALS:
		return "equals";
	case GIL_TOK_PIPE:
		return "pipe";
	case GIL_TOK_EOL:
		return "end-of-line";
	case GIL_TOK_EOF:
		return "end-of-file";
	case GIL_TOK_NUMBER:
		return "number";
	case GIL_TOK_STRING:
		return "string";
	case GIL_TOK_IDENT:
		return "ident";
	case GIL_TOK_ERROR:
		return "error";
	}

	return "(unknown)";
}

void gil_token_free(struct gil_token *tok) {
	enum gil_token_kind kind = gil_token_get_kind(tok);
	if (
			(kind == GIL_TOK_STRING || kind == GIL_TOK_IDENT) &&
			!gil_token_is_small(tok)) {
		free(tok->v.str);
	}
}

struct gil_token_value gil_token_extract_val(struct gil_token *tok) {
	struct gil_token_value v = tok->v;
	tok->v.str = NULL;
	return v;
}

const char *gil_token_get_str(struct gil_token_value *val) {
	if (val->flags & GIL_TOK_SMALL) {
		return val->strbuf;
	} else {
		return val->str;
	}
}

void gil_lexer_init(struct gil_lexer *lexer, struct gil_io_reader *r) {
	lexer->toks[0].v.flags = GIL_TOK_EOF,
	lexer->tokidx = 0;
	lexer->line = 1;
	lexer->ch = 1;
	lexer->parens = 0;
	lexer->prev_tok_is_expr = 0;
	gil_bufio_reader_init(&lexer->reader, r);
}

static int peek_ch(struct gil_lexer *lexer) {
	int ch = gil_bufio_peek(&lexer->reader, 1);
	return ch;
}

static int peek_ch_n(struct gil_lexer *lexer, int n) {
	int ch = gil_bufio_peek(&lexer->reader, n);
	return ch;
}

static int read_ch(struct gil_lexer *lexer) {
	int ch = gil_bufio_get(&lexer->reader);
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
		ch != '\'' && ch != '|' &&
		ch != ',' && ch != '.' &&
		ch != ':' && ch != ';';
}

static void skip_whitespace(struct gil_lexer *lexer, int *nl, int *skipped) {
	while (1) {
		if (is_whitespace(peek_ch(lexer))) {
			*skipped = 1;
			do {
				int ch = read_ch(lexer);
				if (ch == '\n') {

					// We found a newline. If the first non-whitespace on the
					// next line is '->', then don't set *nl.
					while (is_whitespace(peek_ch(lexer)) && peek_ch(lexer) != '\n') {
						read_ch(lexer);
					}

					if (peek_ch(lexer) == '-' && peek_ch_n(lexer, 2) == '>') {
						read_ch(lexer); // '-'
						read_ch(lexer); // '>'
					} else {
						*nl = 1;
					}
				}
			} while (is_whitespace(peek_ch(lexer)));
		}

		if (peek_ch(lexer) == '#') {
			*nl = 1;
			int ch = read_ch(lexer);
			while (ch != '\n' && ch != EOF) ch = read_ch(lexer);
		} else {
			break;
		}

		*skipped = 1;
	}
}

static int read_integer(struct gil_lexer *lexer, long long *num, long long *base, char **err) {
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

static void read_number(struct gil_lexer *lexer, struct gil_token *tok) {
	tok->v.flags = GIL_TOK_NUMBER;

	float sign = 1;
	if (peek_ch(lexer) == '-') {
		sign = -1;
		read_ch(lexer);
	}

	if (!is_numeric(peek_ch(lexer))) {
		tok->v.flags = GIL_TOK_ERROR;
		tok->v.str = "No number in number literal";
		return;
	}

	long long integral;
	long long base;
	char *err;
	if (read_integer(lexer, &integral, &base, &err) < 0) {
		tok->v.flags = GIL_TOK_ERROR;
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
			tok->v.flags = GIL_TOK_ERROR;
			tok->v.str = "Number with digits too big for the base";
			return;
		}

		buffer[fraction_len++] = digit;
		read_ch(lexer);
	}

	if (fraction_len < 1) {
		tok->v.flags = GIL_TOK_ERROR;
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

static void read_string(struct gil_lexer *lexer, struct gil_token *tok) {
	tok->v.flags = GIL_TOK_STRING | GIL_TOK_SMALL;

	char *dest = tok->v.strbuf;
	size_t size = sizeof(tok->v.strbuf);
	size_t idx = 0;

	while (1) {
		int ch = read_ch(lexer);
		if (ch == '"') {
			dest[idx] = '\0';
			return;
		} else if (ch == EOF) {
			if (!gil_token_is_small(tok)) {
				free(tok->v.str);
			}
			tok->v.flags = GIL_TOK_ERROR;
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
				if (!gil_token_is_small(tok)) {
					free(tok->v.str);
				}
				tok->v.flags = GIL_TOK_ERROR;
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
			if (gil_token_is_small(tok)) {
				tok->v.flags &= ~GIL_TOK_SMALL;
				size = 32;
				newbuf = malloc(size);
				if (newbuf == NULL) {
					tok->v.flags = GIL_TOK_ERROR;
					tok->v.str = "Allocation failure";
					return;
				}
				memcpy(newbuf, tok->v.strbuf, idx);
			} else {
				size *= 2;
				newbuf = realloc(tok->v.str, size);
				if (newbuf == NULL) {
					free(tok->v.str);
					tok->v.flags = GIL_TOK_ERROR;
					tok->v.str = "Allocation failure";
					return;
				}
			}

			tok->v.str = newbuf;
			dest = newbuf;
		}
	}
}

static void read_ident(struct gil_lexer *lexer, struct gil_token *tok) {
	tok->v.flags = GIL_TOK_IDENT | GIL_TOK_SMALL;

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
			if (gil_token_is_small(tok)) {
				tok->v.flags &= ~GIL_TOK_SMALL;
				size = 32;
				newbuf = malloc(size);
				if (newbuf == NULL) {
					tok->v.flags = GIL_TOK_ERROR;
					tok->v.str = "Allocation failure";
					return;
				}
				memcpy(newbuf, tok->v.strbuf, idx);
			} else {
				size *= 2;
				newbuf = realloc(tok->v.str, size);
				if (newbuf == NULL) {
					free(tok->v.str);
					tok->v.flags = GIL_TOK_ERROR;
					tok->v.str = "Allocation failure";
					return;
				}
			}

			tok->v.str = newbuf;
			dest = newbuf;
		}
	}
}

static void read_tok(struct gil_lexer *lexer, struct gil_token *tok) {
	tok->line = lexer->line;
	tok->ch = lexer->ch;
	int nl = 0, skipped_whitespace = 0;
	skip_whitespace(lexer, &nl, &skipped_whitespace);

	if (nl && lexer->parens == 0) {
		tok->v.flags = GIL_TOK_EOL;
		return;
	}

	int prev_tok_is_expr = lexer->prev_tok_is_expr;
	lexer->prev_tok_is_expr = 0;

	int ch = peek_ch(lexer);
	switch (ch) {
	case '(':
		read_ch(lexer);
		if (prev_tok_is_expr && !skipped_whitespace) {
			tok->v.flags = GIL_TOK_OPEN_PAREN_NS;
		} else {
			tok->v.flags = GIL_TOK_OPEN_PAREN;
		}
		lexer->parens += 1;
		break;

	case ')':
		read_ch(lexer);
		tok->v.flags = GIL_TOK_CLOSE_PAREN;
		lexer->prev_tok_is_expr = 1;
		lexer->parens -= 1;
		break;

	case '{':
		read_ch(lexer);
		tok->v.flags = GIL_TOK_OPEN_BRACE;
		break;

	case '}':
		read_ch(lexer);
		tok->v.flags = GIL_TOK_CLOSE_BRACE;
		lexer->prev_tok_is_expr = 1;
		break;

	case '[':
		read_ch(lexer);
		tok->v.flags = GIL_TOK_OPEN_BRACKET;
		break;

	case ']':
		read_ch(lexer);
		tok->v.flags = GIL_TOK_CLOSE_BRACKET;
		lexer->prev_tok_is_expr = 1;
		break;

	case ';':
		tok->v.flags = GIL_TOK_EOL;
		do {
			read_ch(lexer);
			skip_whitespace(lexer, &nl, &skipped_whitespace);
		} while (peek_ch(lexer) == ';');
		break;

	case '\'':
		read_ch(lexer);
		tok->v.flags = GIL_TOK_QUOT;
		break;

	case ',':
		read_ch(lexer);
		tok->v.flags = GIL_TOK_COMMA;
		break;

	case '.':
		read_ch(lexer);
		if (is_numeric(peek_ch(lexer))) {
			tok->v.flags = GIL_TOK_DOT_NUMBER;
			long long num, base;
			char *err;
			if (read_integer(lexer, &num, &base, &err) < 0) {
				tok->v.flags = GIL_TOK_ERROR;
				tok->v.str = err;
			} else {
				tok->v.integer = (int)num;
				lexer->prev_tok_is_expr = 1;
			}
		} else {
			tok->v.flags = GIL_TOK_PERIOD;
			lexer->prev_tok_is_expr = 1;
		}
		break;

	case ':':
		read_ch(lexer);
		ch = peek_ch(lexer);
		switch (ch) {
		case '=':
			read_ch(lexer);
			tok->v.flags = GIL_TOK_COLON_EQ;
			break;

		default:
			tok->v.flags = GIL_TOK_COLON;
			break;
		}
		break;

	case '|':
		read_ch(lexer);
		// Treat '||' as an identifier
		if (peek_ch(lexer) == '|') {
			read_ch(lexer);
			tok->v.flags = GIL_TOK_IDENT | GIL_TOK_SMALL;
			strcpy(tok->v.strbuf, "||");
			lexer->prev_tok_is_expr = 1;
		} else {
			tok->v.flags = GIL_TOK_PIPE;
		}

		break;

	case EOF:
		tok->v.flags = GIL_TOK_EOF;
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

			tok->v.flags = GIL_TOK_IDENT;
			read_ident(lexer, tok);
			lexer->prev_tok_is_expr = 1;

			if (gil_token_is_small(tok) && strcmp(tok->v.strbuf, "=") == 0) {
				tok->v.flags = GIL_TOK_EQUALS;
			}
		}
	}
}

struct gil_token *gil_lexer_peek(struct gil_lexer *lexer, int count) {
	int offset = count - 1;

	while (offset >= lexer->tokidx) {
		read_tok(lexer, &lexer->toks[lexer->tokidx++]);
#ifdef GIL_ENABLE_TRACE
		if (gil_tracer_enabled()) {
			trace_token(&lexer->toks[lexer->tokidx - 1]);
		}
#endif
	}

	return &lexer->toks[offset];
}

void gil_lexer_consume(struct gil_lexer *lexer) {
	gil_token_free(&lexer->toks[0]);
	lexer->tokidx -= 1;
	memmove(lexer->toks, lexer->toks + 1, lexer->tokidx * sizeof(*lexer->toks));
}

void gil_lexer_skip_opt(struct gil_lexer *lexer, enum gil_token_kind kind) {
	if (gil_token_get_kind(gil_lexer_peek(lexer, 1)) == kind) {
		gil_lexer_consume(lexer);
	}
}
