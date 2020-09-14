#include "io.h"
#include "parse/lex.h"

int main() {
	struct l2_io_mem_reader r = { l2_io_mem_read };
	r.mem = "  \"Hello\", [], {}.";
	r.len = strlen(r.mem);

	struct l2_lexer lexer;
	l2_lexer_init(&lexer, &r.r);

	while (1) {
		struct l2_token *tok = l2_lexer_get(&lexer);
		printf("%s\n", l2_token_kind_name(tok->kind));
		if (tok->kind == L2_TOK_EOF) {
			break;
		}
	}
}
