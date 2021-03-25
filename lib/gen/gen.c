#include "gen/gen.h"

#include <string.h>

#include "bytecode.h"

static void put(struct gil_generator *gen, unsigned char ch) {
	gil_bufio_put(&gen->writer, ch);
	gen->pos += 1;
}

static void put_u4le(struct gil_generator *gen, gil_word word) {
	char data[4] = {
		(word >> 0) & 0xff,
		(word >> 8) & 0xff,
		(word >> 16) & 0xff,
		(word >> 24) & 0xff,
	};

	gil_bufio_put_n(&gen->writer, data, 4);
	gen->pos += 4;
}

static void put_d8le(struct gil_generator *gen, double num) {
	uint64_t integer;
	memcpy(&integer, &num, 8);

	char data[8] = {
		(integer >> 0) & 0xff,
		(integer >> 8) & 0xff,
		(integer >> 16) & 0xff,
		(integer >> 24) & 0xff,
		(integer >> 32) & 0xff,
		(integer >> 40) & 0xff,
		(integer >> 48) & 0xff,
		(integer >> 56) & 0xff,
	};

	gil_bufio_put_n(&gen->writer, data, 8);
	gen->pos += 8;
}

void gil_gen_init(struct gil_generator *gen, struct gil_io_writer *w) {
	gil_strset_init(&gen->atomset);
	gil_strset_init(&gen->stringset);
	gen->strings = NULL;
	gen->pos = 0;
	gil_bufio_writer_init(&gen->writer, w);

	// Register atoms for all builtins
#define XNAME(name, k) \
	gil_strset_put_copy(&gen->atomset, name);
#define XATOM(name, k) \
	gil_strset_put_copy(&gen->atomset, name);
#define XFUNCTION(name, f) \
	gil_strset_put_copy(&gen->atomset, name);
#include "builtins.x.h"
#undef XNAME
#undef XATOM
#undef XFUNCTION
}

void gil_gen_flush(struct gil_generator *gen) {
	gil_bufio_flush(&gen->writer);
}

void gil_gen_free(struct gil_generator *gen) {
	gil_strset_free(&gen->atomset);
	gil_strset_free(&gen->stringset);
	free(gen->strings);
}

void gil_gen_halt(struct gil_generator *gen) {
	put(gen, GIL_OP_HALT);
}

void gil_gen_rjmp(struct gil_generator *gen, gil_word len) {
	if (len <= 0xff) {
		put(gen, GIL_OP_RJMP_U1);
		put(gen, len);
	} else {
		put(gen, GIL_OP_RJMP_U4);
		put_u4le(gen, len);
	}
}

void gil_gen_rjmp_placeholder(struct gil_generator *gen) {
	put(gen, GIL_OP_RJMP_U4);
	put_u4le(gen, 0);
}

void gil_gen_discard(struct gil_generator *gen) {
	put(gen, GIL_OP_DISCARD);
}

void gil_gen_swap_discard(struct gil_generator *gen) {
	put(gen, GIL_OP_SWAP_DISCARD);
}

void gil_gen_ret(struct gil_generator *gen) {
	put(gen, GIL_OP_RET);
}

void gil_gen_none(struct gil_generator *gen) {
	put(gen, GIL_OP_ALLOC_NONE);
}

void gil_gen_number(struct gil_generator *gen, double num) {
	uint64_t n;
	memcpy(&n, &num, sizeof(num));
	put(gen, GIL_OP_ALLOC_REAL_D8);
	put_d8le(gen, num);
}

void gil_gen_atom(struct gil_generator *gen, char **str) {
	size_t id = gil_strset_put(&gen->atomset, str);
	if (id <= 0xff) { 
		put(gen, GIL_OP_ALLOC_ATOM_U1);
		put(gen, id);
	} else {
		put(gen, GIL_OP_ALLOC_ATOM_U4);
		put_u4le(gen, id);
	}
}

void gil_gen_atom_copy(struct gil_generator *gen, char *str) {
	size_t id = gil_strset_put_copy(&gen->atomset, str);
	if (id <= 0xff) {
		put(gen, GIL_OP_ALLOC_ATOM_U1);
		put(gen, id);
	} else {
		put(gen, GIL_OP_ALLOC_ATOM_U4);
		put_u4le(gen, id);
	}
}

void gil_gen_string(struct gil_generator *gen, char **str) {
	size_t id = gil_strset_get(&gen->stringset, *str);
	if (id == 0) {
		size_t len = strlen(*str);

		gil_gen_rjmp(gen, len);
		gil_word pos = gen->pos;

		gen->pos += len;
		gil_bufio_put_n(&gen->writer, *str, len);

		id = gil_strset_put(&gen->stringset, str);
		gen->strings = realloc(gen->strings, id * sizeof(*gen->strings));
		gen->strings[id - 1].length = len;
		gen->strings[id - 1].pos = pos;

		if (len <= 0xff && pos <= 0xff) {
			put(gen, GIL_OP_ALLOC_BUFFER_STATIC_U1);
			put(gen, len);
			put(gen, pos);
		} else {
			put(gen, GIL_OP_ALLOC_BUFFER_STATIC_U4);
			put_u4le(gen, len);
			put_u4le(gen, pos);
		}
	} else {
		free(*str);
		struct gil_generator_string *s = &gen->strings[id - 1];
		if (s->length <= 0xff && s->pos <= 0xff) {
			put(gen, GIL_OP_ALLOC_BUFFER_STATIC_U1);
			put(gen, s->length);
			put(gen, s->pos);
		} else {
			put(gen, GIL_OP_ALLOC_BUFFER_STATIC_U4);
			put_u4le(gen, s->length);
			put_u4le(gen, s->pos);
		}
	}
}

void gil_gen_string_copy(struct gil_generator *gen, char *str) {
	size_t id = gil_strset_get(&gen->stringset, str);
	if (id == 0) {
		char *s = strdup(str);
		gil_gen_string(gen, &s);
	} else {
		struct gil_generator_string *s = &gen->strings[id - 1];
		if (s->length <= 0xff && s->pos <= 0xff) {
			put(gen, GIL_OP_ALLOC_BUFFER_STATIC_U1);
			put(gen, s->length);
			put(gen, s->pos);
		} else {
			put(gen, GIL_OP_ALLOC_BUFFER_STATIC_U4);
			put_u4le(gen, s->length);
			put_u4le(gen, s->pos);
		}
	}
}

void gil_gen_function(struct gil_generator *gen, gil_word pos) {
	if (pos <= 0xff) {
		put(gen, GIL_OP_ALLOC_FUNCTION_U1);
		put(gen, pos);
	} else {
		put(gen, GIL_OP_ALLOC_FUNCTION_U4);
		put_u4le(gen, pos);
	}
}

void gil_gen_array(struct gil_generator *gen, gil_word count) {
	if (count <= 0xff) {
		put(gen, GIL_OP_ALLOC_ARRAY_U1);
		put(gen, count);
	} else {
		put(gen, GIL_OP_ALLOC_ARRAY_U4);
		put_u4le(gen, count);
	}
}

void gil_gen_namespace(struct gil_generator *gen) {
	put(gen, GIL_OP_ALLOC_NAMESPACE);
}

void gil_gen_namespace_set(struct gil_generator *gen, char **ident) {
	size_t atom_id = gil_strset_put(&gen->atomset, ident);
	if (atom_id <= 0xff) {
		put(gen, GIL_OP_NAMESPACE_SET_U1);
		put(gen, atom_id);
	} else {
		put(gen, GIL_OP_NAMESPACE_SET_U4);
		put_u4le(gen, atom_id);
	}
}

void gil_gen_namespace_set_copy(struct gil_generator *gen, char *ident) {
	size_t atom_id = gil_strset_put_copy(&gen->atomset, ident);
	if (atom_id <= 0xff) {
		put(gen, GIL_OP_NAMESPACE_SET_U1);
		put(gen, atom_id);
	} else {
		put(gen, GIL_OP_NAMESPACE_SET_U4);
		put_u4le(gen, atom_id);
	}
}

void gil_gen_namespace_lookup(struct gil_generator *gen, char **ident) {
	size_t atom_id = gil_strset_put(&gen->atomset, ident);
	if (atom_id <= 0xff) {
		put(gen, GIL_OP_NAMESPACE_LOOKUP_U1);
		put(gen, atom_id);
	} else {
		put(gen, GIL_OP_NAMESPACE_LOOKUP_U4);
		put_u4le(gen, atom_id);
	}
}

void gil_gen_namespace_lookup_copy(struct gil_generator *gen, char *ident) {
	size_t atom_id = gil_strset_put_copy(&gen->atomset, ident);
	if (atom_id <= 0xff) {
		put(gen, GIL_OP_NAMESPACE_LOOKUP_U1);
		put(gen, atom_id);
	} else {
		put(gen, GIL_OP_NAMESPACE_LOOKUP_U4);
		put_u4le(gen, atom_id);
	}
}

void gil_gen_array_lookup(struct gil_generator *gen, int number) {
	if (number <= 0xff) {
		put(gen, GIL_OP_ARRAY_LOOKUP_U1);
		put(gen, number);
	} else {
		put(gen, GIL_OP_ARRAY_LOOKUP_U4);
		put_u4le(gen, number);
	}
}

void gil_gen_array_set(struct gil_generator *gen, int number) {
	if (number <= 0xff) {
		put(gen, GIL_OP_ARRAY_SET_U1);
		put(gen, number);
	} else {
		put(gen, GIL_OP_ARRAY_SET_U4);
		put_u4le(gen, number);
	}
}

void gil_gen_dynamic_lookup(struct gil_generator *gen) {
	put(gen, GIL_OP_DYNAMIC_LOOKUP);
}

void gil_gen_dynamic_set(struct gil_generator *gen) {
	put(gen, GIL_OP_DYNAMIC_SET);
}

void gil_gen_stack_frame_get_args(struct gil_generator *gen) {
	put(gen, GIL_OP_STACK_FRAME_GET_ARGS);
}

void gil_gen_stack_frame_lookup(struct gil_generator *gen, char **ident) {
	size_t atom_id = gil_strset_put(&gen->atomset, ident);
	if (atom_id <= 0xff) {
		put(gen, GIL_OP_STACK_FRAME_LOOKUP_U1);
		put(gen, atom_id);
	} else {
		put(gen, GIL_OP_STACK_FRAME_LOOKUP_U4);
		put_u4le(gen, atom_id);
	}
}

void gil_gen_stack_frame_lookup_copy(struct gil_generator *gen, char *ident) {
	size_t atom_id = gil_strset_put_copy(&gen->atomset, ident);
	if (atom_id <= 0xff) {
		put(gen, GIL_OP_STACK_FRAME_LOOKUP_U1);
		put(gen, atom_id);
	} else {
		put(gen, GIL_OP_STACK_FRAME_LOOKUP_U4);
		put_u4le(gen, atom_id);
	}
}

void gil_gen_stack_frame_set(struct gil_generator *gen, char **ident) {
	size_t atom_id = gil_strset_put(&gen->atomset, ident);
	if (atom_id <= 0xff) {
		put(gen, GIL_OP_STACK_FRAME_SET_U1);
		put(gen, atom_id);
	} else {
		put(gen, GIL_OP_STACK_FRAME_SET_U4);
		put_u4le(gen, atom_id);
	}
}

void gil_gen_stack_frame_set_copy(struct gil_generator *gen, char *ident) {
	size_t atom_id = gil_strset_put_copy(&gen->atomset, ident);
	if (atom_id <= 0xff) {
		put(gen, GIL_OP_STACK_FRAME_SET_U1);
		put(gen, atom_id);
	} else {
		put(gen, GIL_OP_STACK_FRAME_SET_U4);
		put_u4le(gen, atom_id);
	}
}

void gil_gen_stack_frame_replace(struct gil_generator *gen, char **ident) {
	size_t atom_id = gil_strset_put(&gen->atomset, ident);
	if (atom_id <= 0xff) {
		put(gen, GIL_OP_STACK_FRAME_REPLACE_U1);
		put(gen, atom_id);
	} else {
		put(gen, GIL_OP_STACK_FRAME_REPLACE_U4);
		put_u4le(gen, atom_id);
	}
}

void gil_gen_stack_frame_replace_copy(struct gil_generator *gen, char *ident) {
	size_t atom_id = gil_strset_put_copy(&gen->atomset, ident);
	if (atom_id <= 0xff) {
		put(gen, GIL_OP_STACK_FRAME_REPLACE_U1);
		put(gen, atom_id);
	} else {
		put(gen, GIL_OP_STACK_FRAME_REPLACE_U4);
		put_u4le(gen, atom_id);
	}
}

void gil_gen_func_call(struct gil_generator *gen, gil_word argc) {
	if (argc <= 0xff) {
		put(gen, GIL_OP_FUNC_CALL_U1);
		put(gen, argc);
	} else {
		put(gen, GIL_OP_FUNC_CALL_U4);
		put_u4le(gen, argc);
	}
}

void gil_gen_func_call_infix(struct gil_generator *gen) {
	put(gen, GIL_OP_FUNC_CALL_INFIX);
}
