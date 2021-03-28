#include "gen/gen.h"

#include <string.h>

#include "bytecode.h"
#include "parse/lex.h"
#include "parse/parse.h"
#include "module.h"

static void put(struct gil_generator *gen, unsigned char ch) {
	gil_bufio_put(&gen->writer, ch);
	gen->pos += 1;
}

static int encode_uint(gil_word word, unsigned char out[5]) {
	unsigned char bytes[4];

	int count = 0;
	while (word >= 0x80) {
		bytes[count++] = word & 0x7f;
		word >>= 7;
	}
	bytes[count++] = word;

	int dest = 0;
	for (int i = count - 1; i > 0; --i) {
		out[dest++] = bytes[i] | 0x80;
	}

	out[dest++] = bytes[0];

	return count;
}

static void put_uint(struct gil_generator *gen, gil_word word) {
	unsigned char bytes[5];
	int count = encode_uint(word, bytes);
	gil_bufio_put_n(&gen->writer, bytes, count);
	gen->pos += count;
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

static gil_word alloc_name(void *ptr, const char *name) {
	struct gil_generator *gen = ptr;
	return gil_strset_put_copy(&gen->atomset, name);
}

void gil_gen_register_module(struct gil_generator *gen, struct gil_module *mod) {
	gil_word id = gil_strset_put_copy(&gen->atomset, mod->name);
	mod->init(mod, alloc_name, gen);

	if (gen->moduleslen + 1 >= gen->modulessize) {
		if (gen->modulessize == 0) {
			gen->modulessize = 16;
		} else do {
			gen->modulessize *= 2;
		} while (gen->moduleslen + 1 >= gen->modulessize);

		gen->modules = realloc(gen->modules, gen->modulessize * sizeof(*gen->modules));
	}

	gen->modules[gen->moduleslen++] = id;
}

void gil_gen_flush(struct gil_generator *gen) {
	gil_bufio_flush(&gen->writer);
}

void gil_gen_free(struct gil_generator *gen) {
	gil_strset_free(&gen->atomset);
	gil_strset_free(&gen->stringset);
	free(gen->strings);
}

static int gen_cmodule(struct gil_generator *gen, const char *str) {
	gil_word id = gil_strset_get(&gen->atomset, str);
	if (!id) {
		return 0;
	}

	int found = 0;
	for (size_t i = 0; i < gen->moduleslen; ++i) {
		if (gen->modules[i] == id) {
			found = 1;
			break;
		}
	}

	if (!found) {
		return 0;
	}


}

int gil_gen_import(
		struct gil_generator *gen, char **str,
		int (*callback)(void *data), void *data) {
	int ret = gen_cmodule(gen, *str);
	if (ret != 0) {
		free(*str);
		*str = NULL;
		if (ret < 0) {
			return -1;
		} else {
			return 0;
		}
	}
}

int gil_gen_import_copy(
		struct gil_generator *gen, const char *str,
		int (*callback)(void *data), void *data) {
	int ret = gen_cmodule(gen, str);
	if (ret != 0) {
		if (ret < 0) {
			return -1;
		} else {
			return 0;
		}
	}
}

void gil_gen_halt(struct gil_generator *gen) {
	put(gen, GIL_OP_HALT);
}

void gil_gen_rjmp(struct gil_generator *gen, gil_word len) {
	put(gen, GIL_OP_RJMP);
	put_uint(gen, len);
}

void gil_gen_rjmp_placeholder(struct gil_generator *gen) {
	put(gen, GIL_OP_RJMP_U4);
	put(gen, 0);
	put(gen, 0);
	put(gen, 0);
	put(gen, 0);
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
	put(gen, GIL_OP_ALLOC_REAL);
	put_d8le(gen, num);
}

void gil_gen_atom(struct gil_generator *gen, char **str) {
	size_t id = gil_strset_put(&gen->atomset, str);
	put(gen, GIL_OP_ALLOC_ATOM);
	put_uint(gen, id);
}

void gil_gen_atom_copy(struct gil_generator *gen, char *str) {
	size_t id = gil_strset_put_copy(&gen->atomset, str);
	put(gen, GIL_OP_ALLOC_ATOM);
	put_uint(gen, id);
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

		put(gen, GIL_OP_ALLOC_BUFFER_STATIC);
		put_uint(gen, len);
		put_uint(gen, pos);
	} else {
		free(*str);
		struct gil_generator_string *s = &gen->strings[id - 1];
		put(gen, GIL_OP_ALLOC_BUFFER_STATIC);
		put_uint(gen, s->length);
		put_uint(gen, s->pos);
	}
}

void gil_gen_string_copy(struct gil_generator *gen, char *str) {
	size_t id = gil_strset_get(&gen->stringset, str);
	if (id == 0) {
		char *s = strdup(str);
		gil_gen_string(gen, &s);
	} else {
		struct gil_generator_string *s = &gen->strings[id - 1];
		put(gen, GIL_OP_ALLOC_BUFFER_STATIC);
		put_uint(gen, s->length);
		put_uint(gen, s->pos);
	}
}

void gil_gen_function(struct gil_generator *gen, gil_word pos) {
	put(gen, GIL_OP_ALLOC_FUNCTION);
	put_uint(gen, pos);
}

void gil_gen_array(struct gil_generator *gen, gil_word count) {
	put(gen, GIL_OP_ALLOC_ARRAY);
	put_uint(gen, count);
}

void gil_gen_namespace(struct gil_generator *gen) {
	put(gen, GIL_OP_ALLOC_NAMESPACE);
}

void gil_gen_namespace_set(struct gil_generator *gen, char **ident) {
	size_t atom_id = gil_strset_put(&gen->atomset, ident);
	put(gen, GIL_OP_NAMESPACE_SET);
	put_uint(gen, atom_id);
}

void gil_gen_namespace_set_copy(struct gil_generator *gen, char *ident) {
	size_t atom_id = gil_strset_put_copy(&gen->atomset, ident);
	put(gen, GIL_OP_NAMESPACE_SET);
	put_uint(gen, atom_id);
}

void gil_gen_namespace_lookup(struct gil_generator *gen, char **ident) {
	size_t atom_id = gil_strset_put(&gen->atomset, ident);
	put(gen, GIL_OP_NAMESPACE_LOOKUP);
	put_uint(gen, atom_id);
}

void gil_gen_namespace_lookup_copy(struct gil_generator *gen, char *ident) {
	size_t atom_id = gil_strset_put_copy(&gen->atomset, ident);
	put(gen, GIL_OP_NAMESPACE_LOOKUP);
	put_uint(gen, atom_id);
}

void gil_gen_array_lookup(struct gil_generator *gen, int number) {
	put(gen, GIL_OP_ARRAY_LOOKUP);
	put_uint(gen, number);
}

void gil_gen_array_set(struct gil_generator *gen, int number) {
	put(gen, GIL_OP_ARRAY_SET);
	put_uint(gen, number);
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
	put(gen, GIL_OP_STACK_FRAME_LOOKUP);
	put_uint(gen, atom_id);
}

void gil_gen_stack_frame_lookup_copy(struct gil_generator *gen, char *ident) {
	size_t atom_id = gil_strset_put_copy(&gen->atomset, ident);
	put(gen, GIL_OP_STACK_FRAME_LOOKUP);
	put_uint(gen, atom_id);
}

void gil_gen_stack_frame_set(struct gil_generator *gen, char **ident) {
	size_t atom_id = gil_strset_put(&gen->atomset, ident);
	put(gen, GIL_OP_STACK_FRAME_SET);
	put_uint(gen, atom_id);
}

void gil_gen_stack_frame_set_copy(struct gil_generator *gen, char *ident) {
	size_t atom_id = gil_strset_put_copy(&gen->atomset, ident);
	put(gen, GIL_OP_STACK_FRAME_SET);
	put_uint(gen, atom_id);
}

void gil_gen_stack_frame_replace(struct gil_generator *gen, char **ident) {
	size_t atom_id = gil_strset_put(&gen->atomset, ident);
	put(gen, GIL_OP_STACK_FRAME_REPLACE);
	put_uint(gen, atom_id);
}

void gil_gen_stack_frame_replace_copy(struct gil_generator *gen, char *ident) {
	size_t atom_id = gil_strset_put_copy(&gen->atomset, ident);
	put(gen, GIL_OP_STACK_FRAME_REPLACE);
	put_uint(gen, atom_id);
}

void gil_gen_func_call(struct gil_generator *gen, gil_word argc) {
	put(gen, GIL_OP_FUNC_CALL);
	put_uint(gen, argc);
}

void gil_gen_func_call_infix(struct gil_generator *gen) {
	put(gen, GIL_OP_FUNC_CALL_INFIX);
}
