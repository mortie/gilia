#include "gen/gen.h"

#include "bytecode.h"

static void put(struct l2_generator *gen, l2_word word) {
	l2_bufio_put_n(&gen->writer, &word, sizeof(word));
	gen->pos += 1;
}

void l2_gen_init(struct l2_generator *gen, struct l2_io_writer *w) {
	l2_strset_init(&gen->atomset);
	l2_strset_init(&gen->stringset);
	gen->strings = NULL;
	gen->pos = 0;
	l2_bufio_writer_init(&gen->writer, w);

	// Register atoms for all builtins
#define X(name, f) \
	l2_strset_put_copy(&gen->atomset, name);
#include "builtins.x.h"
#undef X
}

void l2_gen_flush(struct l2_generator *gen) {
	l2_bufio_flush(&gen->writer);
}

void l2_gen_free(struct l2_generator *gen) {
	l2_strset_free(&gen->atomset);
	l2_strset_free(&gen->stringset);
	free(gen->strings);
}

void l2_gen_halt(struct l2_generator *gen) {
	put(gen, L2_OP_HALT);
}

void l2_gen_rjmp(struct l2_generator *gen, l2_word len) {
	put(gen, L2_OP_PUSH);
	put(gen, len);
	put(gen, L2_OP_RJMP);
}

void l2_gen_pop(struct l2_generator *gen) {
	put(gen, L2_OP_POP);
}

void l2_gen_push(struct l2_generator *gen, l2_word word) {
	put(gen, L2_OP_PUSH);
	put(gen, word);
}

void l2_gen_ret(struct l2_generator *gen) {
	put(gen, L2_OP_RET);
}

void l2_gen_assignment(struct l2_generator *gen, char **ident) {
	size_t atom_id = l2_strset_put(&gen->atomset, ident);
	put(gen, L2_OP_PUSH);
	put(gen, atom_id);
	put(gen, L2_OP_STACK_FRAME_SET);
}

void l2_gen_number(struct l2_generator *gen, double num) {
	uint64_t n;
	memcpy(&n, &num, sizeof(num));
	put(gen, L2_OP_PUSH_2);
	put(gen, n);
	put(gen, n >> 32);
	put(gen, L2_OP_ALLOC_REAL);
}

void l2_gen_atom(struct l2_generator *gen, char **str) {
	size_t id = l2_strset_put(&gen->atomset, str);
	put(gen, L2_OP_PUSH);
	put(gen, id);
	put(gen, L2_OP_ALLOC_ATOM);
}

void l2_gen_string(struct l2_generator *gen, char **str) {
	size_t id = l2_strset_get(&gen->stringset, *str);
	if (id == 0) {
		size_t len = strlen(*str);
		size_t aligned = len;
		if (aligned % sizeof(l2_word) != 0) {
			aligned += sizeof(l2_word) - (aligned % sizeof(l2_word));
		}

		put(gen, L2_OP_PUSH);
		put(gen, aligned / sizeof(l2_word));
		put(gen, L2_OP_RJMP);
		l2_word pos = gen->pos;

		gen->pos += aligned / sizeof(l2_word);
		l2_bufio_put_n(&gen->writer, *str, len);
		for (size_t i = len; i < aligned; ++i) {
			l2_bufio_put(&gen->writer, '\0');
		}

		id = l2_strset_put(&gen->stringset, str);
		gen->strings = realloc(gen->strings, id * sizeof(*gen->strings));
		gen->strings[id - 1].length = len;
		gen->strings[id - 1].pos = pos;

		put(gen, L2_OP_PUSH_2);
		put(gen, pos);
		put(gen, len);
		put(gen, L2_OP_ALLOC_BUFFER_STATIC);
	} else {
		free(*str);
		struct l2_generator_string *s = &gen->strings[id - 1];
		put(gen, L2_OP_PUSH_2);
		put(gen, s->pos);
		put(gen, s->length);
		put(gen, L2_OP_ALLOC_BUFFER_STATIC);
	}
}

void l2_gen_function(struct l2_generator *gen, l2_word pos) {
	put(gen, L2_OP_PUSH);
	put(gen, pos);
	put(gen, L2_OP_ALLOC_FUNCTION);
}

void l2_gen_namespace(struct l2_generator *gen) {
	put(gen, L2_OP_ALLOC_NAMESPACE);
}

void l2_gen_namespace_set(struct l2_generator *gen, char **ident) {
	size_t atom_id = l2_strset_put(&gen->atomset, ident);
	put(gen, L2_OP_PUSH);
	put(gen, atom_id);
	put(gen, L2_OP_NAMESPACE_SET);
}

void l2_gen_namespace_lookup(struct l2_generator *gen, char **ident) {
	size_t atom_id = l2_strset_put(&gen->atomset, ident);
	put(gen, L2_OP_PUSH);
	put(gen, atom_id);
	put(gen, L2_OP_NAMESPACE_LOOKUP);
}

void l2_gen_direct_array_lookup(struct l2_generator *gen, int number) {
	put(gen, L2_OP_PUSH);
	put(gen, number);
	put(gen, L2_OP_DIRECT_ARRAY_LOOKUP);
}

void l2_gen_stack_frame_lookup(struct l2_generator *gen, char **ident) {
	size_t atom_id = l2_strset_put(&gen->atomset, ident);
	put(gen, L2_OP_PUSH);
	put(gen, atom_id);
	put(gen, L2_OP_STACK_FRAME_LOOKUP);
}

void l2_gen_func_call(struct l2_generator *gen) {
	put(gen, L2_OP_FUNC_CALL);
}
