#include "gen/gen.h"

#include "bytecode.h"

static void put(struct l2_bufio_writer *writer, l2_word word) {
	l2_bufio_put_n(writer, &word, sizeof(word));
}

void l2_gen_stack_frame(struct l2_bufio_writer *writer) {
	put(writer, L2_OP_ALLOC_NAMESPACE);
}
