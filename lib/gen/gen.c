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
	put(gen, L2_OP_RJMP);
	put(gen, len);
}

void l2_gen_discard(struct l2_generator *gen) {
	put(gen, L2_OP_DISCARD);
}

void l2_gen_swap_discard(struct l2_generator *gen) {
	put(gen, L2_OP_SWAP_DISCARD);
}

void l2_gen_ret(struct l2_generator *gen) {
	put(gen, L2_OP_RET);
}

void l2_gen_none(struct l2_generator *gen) {
	put(gen, L2_OP_ALLOC_NONE);
}

void l2_gen_number(struct l2_generator *gen, double num) {
	uint64_t n;
	memcpy(&n, &num, sizeof(num));
	put(gen, L2_OP_ALLOC_REAL);
	put(gen, n >> 32);
	put(gen, n);
}

void l2_gen_atom(struct l2_generator *gen, char **str) {
	size_t id = l2_strset_put(&gen->atomset, str);
	put(gen, L2_OP_ALLOC_ATOM);
	put(gen, id);
}

void l2_gen_atom_copy(struct l2_generator *gen, char *str) {
	size_t id = l2_strset_put_copy(&gen->atomset, str);
	put(gen, L2_OP_ALLOC_ATOM);
	put(gen, id);
}

void l2_gen_string(struct l2_generator *gen, char **str) {
	size_t id = l2_strset_get(&gen->stringset, *str);
	if (id == 0) {
		size_t len = strlen(*str);
		size_t aligned = len;
		if (aligned % sizeof(l2_word) != 0) {
			aligned += sizeof(l2_word) - (aligned % sizeof(l2_word));
		}

		put(gen, L2_OP_RJMP);
		put(gen, aligned / sizeof(l2_word));
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

		put(gen, L2_OP_ALLOC_BUFFER_STATIC);
		put(gen, len);
		put(gen, pos);
	} else {
		free(*str);
		struct l2_generator_string *s = &gen->strings[id - 1];
		put(gen, L2_OP_ALLOC_BUFFER_STATIC);
		put(gen, s->length);
		put(gen, s->pos);
	}
}

void l2_gen_string_copy(struct l2_generator *gen, char *str) {
	size_t id = l2_strset_get(&gen->stringset, str);
	if (id == 0) {
		char *s = strdup(str);
		l2_gen_string(gen, &s);
	} else {
		struct l2_generator_string *s = &gen->strings[id - 1];
		put(gen, L2_OP_ALLOC_BUFFER_STATIC);
		put(gen, s->length);
		put(gen, s->pos);
	}
}

void l2_gen_function(struct l2_generator *gen, l2_word pos) {
	put(gen, L2_OP_ALLOC_FUNCTION);
	put(gen, pos);
}

void l2_gen_array(struct l2_generator *gen, l2_word count) {
	put(gen, L2_OP_ALLOC_ARRAY);
	put(gen, count);
}

void l2_gen_namespace(struct l2_generator *gen) {
	put(gen, L2_OP_ALLOC_NAMESPACE);
}

void l2_gen_namespace_set(struct l2_generator *gen, char **ident) {
	size_t atom_id = l2_strset_put(&gen->atomset, ident);
	put(gen, L2_OP_NAMESPACE_SET);
	put(gen, atom_id);
}

void l2_gen_namespace_set_copy(struct l2_generator *gen, char *ident) {
	size_t atom_id = l2_strset_put_copy(&gen->atomset, ident);
	put(gen, L2_OP_NAMESPACE_SET);
	put(gen, atom_id);
}

void l2_gen_namespace_lookup(struct l2_generator *gen, char **ident) {
	size_t atom_id = l2_strset_put(&gen->atomset, ident);
	put(gen, L2_OP_NAMESPACE_LOOKUP);
	put(gen, atom_id);
}

void l2_gen_namespace_lookup_copy(struct l2_generator *gen, char *ident) {
	size_t atom_id = l2_strset_put_copy(&gen->atomset, ident);
	put(gen, L2_OP_NAMESPACE_LOOKUP);
	put(gen, atom_id);
}

void l2_gen_array_lookup(struct l2_generator *gen, int number) {
	put(gen, L2_OP_ARRAY_LOOKUP);
	put(gen, number);
}

void l2_gen_array_set(struct l2_generator *gen, int number) {
	put(gen, L2_OP_ARRAY_SET);
	put(gen, number);
}

void l2_gen_dynamic_lookup(struct l2_generator *gen) {
	put(gen, L2_OP_DYNAMIC_LOOKUP);
}

void l2_gen_dynamic_set(struct l2_generator *gen) {
	put(gen, L2_OP_DYNAMIC_SET);
}

void l2_gen_stack_frame_lookup(struct l2_generator *gen, char **ident) {
	size_t atom_id = l2_strset_put(&gen->atomset, ident);
	put(gen, L2_OP_STACK_FRAME_LOOKUP);
	put(gen, atom_id);
}

void l2_gen_stack_frame_lookup_copy(struct l2_generator *gen, char *ident) {
	size_t atom_id = l2_strset_put_copy(&gen->atomset, ident);
	put(gen, L2_OP_STACK_FRAME_LOOKUP);
	put(gen, atom_id);
}

void l2_gen_stack_frame_set(struct l2_generator *gen, char **ident) {
	size_t atom_id = l2_strset_put(&gen->atomset, ident);
	put(gen, L2_OP_STACK_FRAME_SET);
	put(gen, atom_id);
}

void l2_gen_stack_frame_set_copy(struct l2_generator *gen, char *ident) {
	size_t atom_id = l2_strset_put_copy(&gen->atomset, ident);
	put(gen, L2_OP_STACK_FRAME_SET);
	put(gen, atom_id);
}

void l2_gen_stack_frame_replace(struct l2_generator *gen, char **ident) {
	size_t atom_id = l2_strset_put(&gen->atomset, ident);
	put(gen, L2_OP_STACK_FRAME_REPLACE);
	put(gen, atom_id);
}

void l2_gen_stack_frame_replace_copy(struct l2_generator *gen, char *ident) {
	size_t atom_id = l2_strset_put_copy(&gen->atomset, ident);
	put(gen, L2_OP_STACK_FRAME_REPLACE);
	put(gen, atom_id);
}

void l2_gen_func_call(struct l2_generator *gen, l2_word argc) {
	put(gen, L2_OP_FUNC_CALL);
	put(gen, argc);
}
