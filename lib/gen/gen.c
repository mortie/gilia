#include "gen/gen.h"

#include "bytecode.h"

static void put(struct l2_generator *gen, l2_word word) {
	l2_bufio_put_n(&gen->writer, &word, sizeof(word));
}

void l2_gen_init(struct l2_generator *gen) {
	l2_strset_init(&gen->atoms);
	l2_strset_init(&gen->strings);
}

void l2_gen_free(struct l2_generator *gen) {
	l2_strset_free(&gen->atoms);
	l2_strset_free(&gen->strings);
}

void l2_gen_stack_frame(struct l2_generator *gen) {
	put(gen, L2_OP_ALLOC_NAMESPACE);
}

void l2_gen_assignment(struct l2_generator *gen, char **ident) {
	size_t atom_id = l2_strset_put(&gen->atoms, ident);
	put(gen, L2_OP_PUSH);
	put(gen, atom_id);
	put(gen, L2_OP_NAMESPACE_SET);
}
