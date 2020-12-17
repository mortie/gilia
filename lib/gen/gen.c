#include "gen/gen.h"

#include "bytecode.h"

static void put(struct l2_generator *gen, l2_word word) {
	l2_bufio_put_n(&gen->writer, &word, sizeof(word));
}

void l2_gen_init(struct l2_generator *gen, struct l2_io_writer *w) {
	l2_strset_init(&gen->atoms);
	l2_strset_init(&gen->strings);
	l2_bufio_writer_init(&gen->writer, w);
}

void l2_gen_flush(struct l2_generator *gen) {
	l2_bufio_flush(&gen->writer);
}

void l2_gen_free(struct l2_generator *gen) {
	l2_strset_free(&gen->atoms);
	l2_strset_free(&gen->strings);
}

// Postconditions:
// * Execution is halted
void l2_gen_halt(struct l2_generator *gen) {
	put(gen, L2_OP_HALT);
}

// Postconditions:
// * NStack(0) is a namespace value
void l2_gen_stack_frame(struct l2_generator *gen) {
	put(gen, L2_OP_GEN_STACK_FRAME);
}

// Preconditions:
// * Stack(0) is any value
// Postconditions:
// * The namespace contains the new value under key 'ident'
// * Stack(0) is untouched
void l2_gen_assignment(struct l2_generator *gen, char **ident) {
	size_t atom_id = l2_strset_put(&gen->atoms, ident);
	put(gen, L2_OP_PUSH);
	put(gen, atom_id);
	put(gen, L2_OP_STACK_FRAME_SET);
}


// Postconditions;
// * Stack(0) is changed to a number value
void l2_gen_number(struct l2_generator *gen, double num) {
	uint64_t n;
	memcpy(&n, &num, sizeof(num));
	put(gen, L2_OP_PUSH_2);
	put(gen, n);
	put(gen, n >> 32);
	put(gen, L2_OP_ALLOC_REAL_64);
}

// Postconditions:
// * Stack(0) is any value
void l2_gen_namespace_lookup(struct l2_generator *gen, char **ident) {
	size_t atom_id = l2_strset_put(&gen->atoms, ident);
	put(gen, L2_OP_PUSH);
	put(gen, atom_id);
	put(gen, L2_OP_STACK_FRAME_LOOKUP);
}
