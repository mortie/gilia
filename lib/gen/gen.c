#include "gen/gen.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "bytecode.h"
#include "io.h"
#include "module.h"
#include "strset.h"
#include "str.h"

#define GIL_TRACER_NAME "bc"
#include "trace.h"

#define bctrace(fmt, ...) gil_trace("%04u: " fmt, gen->pos, __VA_ARGS__)
#define bctrace1(fmt) gil_trace("%04u: " fmt, gen->pos)

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

static gil_word builtin_init_alloc(void *ptr, const char *name) {
	struct gil_generator *gen = ptr;
	return gil_strset_put_copy(&gen->atomset, name);
}

void gil_gen_init(
		struct gil_generator *gen, struct gil_io_writer *w,
		struct gil_module *builtins, struct gil_generator_resolver *resolver) {
	gil_strset_init(&gen->atomset);
	gil_strset_init(&gen->stringset);
	gen->strings = NULL;
	gen->pos = 0;
	gil_bufio_writer_init(&gen->writer, w);

	gen->relocs = NULL;
	gen->relocslen = 0;

	gen->resolver = resolver;

	gen->cmodules = NULL;
	gen->cmoduleslen = 0;

	gil_strset_init(&gen->moduleset);

#define X(name, k) gil_strset_put_copy(&gen->atomset, name);
	GIL_BYTECODE_ATOMS
#undef X

	builtins->init(builtins, builtin_init_alloc, gen);
}

static gil_word alloc_name(void *ptr, const char *name) {
	struct gil_generator *gen = ptr;
	return gil_strset_put_copy(&gen->atomset, name);
}

void gil_gen_register_module(struct gil_generator *gen, struct gil_module *mod) {
	gil_word id = gil_strset_put_copy(&gen->atomset, mod->name);
	mod->init(mod, alloc_name, gen);

	gen->cmoduleslen += 1;
	gen->cmodules = realloc(gen->cmodules, gen->cmoduleslen * sizeof(*gen->cmodules));
	gen->cmodules[gen->cmoduleslen - 1] = id;
}

void gil_gen_flush(struct gil_generator *gen) {
	gil_bufio_flush(&gen->writer);
}

void gil_gen_free(struct gil_generator *gen) {
	gil_strset_free(&gen->atomset);
	gil_strset_free(&gen->stringset);
	free(gen->strings);
	free(gen->cmodules);
	gil_strset_free(&gen->moduleset);
	free(gen->relocs);
}

void gil_gen_add_reloc(struct gil_generator *gen, gil_word pos, gil_word rep) {
	gil_trace("relocation: %u -> %u", pos, rep);
	gen->relocslen += 1;
	gen->relocs = realloc(gen->relocs, gen->relocslen * sizeof(*gen->relocs));
	gen->relocs[gen->relocslen - 1].pos = pos;
	gen->relocs[gen->relocslen - 1].replacement = rep;
}

void gil_gen_fixup_reloc(unsigned char *mem, gil_word rep) {
	mem[0] = rep & 0xff;
	mem[1] = (rep >> 8) & 0xff;
	mem[2] = (rep >> 16) & 0xff;
	mem[3] = (rep >> 24) & 0xff;
}

static int gen_cmodule(struct gil_generator *gen, const char *str) {
	gil_word id = gil_strset_get(&gen->atomset, str);
	if (!id) {
		return 0;
	}

	for (size_t i = 0; i < gen->cmoduleslen; ++i) {
		if (gen->cmodules[i] == id) {
			bctrace("LOAD_CMODULE %u", id);
			put(gen, GIL_OP_LOAD_CMODULE);
			put_uint(gen, id);
			return 1;
		}
	}

	return 0;
}

int gil_gen_import(
		struct gil_generator *gen, char **str, char **err,
		int (*callback)(struct gil_io_reader *reader, void *data, int depth),
		void *data, int depth) {
	if (gen_cmodule(gen, *str)) {
		free(*str);
		*str = NULL;
		return 0;
	}

	if (gen->resolver == NULL) {
		*err = gil_strf("No such module");
		return -1;
	}

	char *normalized_path = gen->resolver->normalize(gen->resolver, *str, err);
	if (normalized_path == NULL) {
		free(*str);
		*str = NULL;
		return -1;
	}

	gil_word id = gil_strset_get(&gen->moduleset, normalized_path);
	if (id) {
		bctrace("LOAD_MODULE %u", id);
		put(gen, GIL_OP_LOAD_MODULE);
		put_uint(gen, id);
		free(normalized_path);
		free(*str);
		*str = NULL;
		return 0;
	}

	struct gil_io_reader *reader = gen->resolver->create_reader(
			gen->resolver, normalized_path, err);
	if (reader == NULL) {
		free(normalized_path);
		free(*str);
		*str = NULL;
		return -1;
	}

	int ret = callback(reader, data, depth);
	gen->resolver->destroy_reader(gen->resolver, reader);
	if (ret < 0) {
		free(normalized_path);
		*err = NULL;
		free(*str);
		*str = NULL;
		return -1;
	} else {
		free(normalized_path);
		free(*str);
		*str = NULL;
		return 0;
	}
}

int gil_gen_import_copy(
		struct gil_generator *gen, const char *str, char **err,
		int (*callback)(struct gil_io_reader *reader, void *data, int depth),
		void *data, int depth) {
	if (gen_cmodule(gen, str)) {
		return 0;
	}

	/*
	 * Yeah, I need to fix all this duplication.
	 * I'll get around to it, k.
	 * Shush.
	 */

	if (gen->resolver == NULL) {
		*err = gil_strf("No such module");
		return -1;
	} char *normalized_path = gen->resolver->normalize(gen->resolver, str, err); if (normalized_path == NULL) { return -1;
	}

	gil_word id = gil_strset_get(&gen->moduleset, normalized_path);
	if (id) {
		bctrace("LOAD_MODULE %u", id);
		put(gen, GIL_OP_LOAD_MODULE);
		put_uint(gen, id);
		free(normalized_path);
		return 0;
	}

	struct gil_io_reader *reader = gen->resolver->create_reader(
			gen->resolver, normalized_path, err);
	if (reader == NULL) {
		free(normalized_path);
		return -1;
	}

	int ret = callback(reader, data, depth);
	gen->resolver->destroy_reader(gen->resolver, reader);
	if (ret < 0) {
		free(normalized_path);
		*err = NULL;
		return -1;
	} else {
		free(normalized_path);
		return 0;
	}
}

void gil_gen_named_param(
		struct gil_generator *gen, gil_word idx, char **ident) {
	size_t atom_id = gil_strset_put(&gen->atomset, ident);
	bctrace("NAMED_PARAM %u %u", atom_id, idx);
	put(gen, GIL_OP_NAMED_PARAM);
	put_uint(gen, atom_id);
	put_uint(gen, idx);
}

void gil_gen_named_param_copy(
		struct gil_generator *gen, gil_word idx, const char *ident) {
	size_t atom_id = gil_strset_put_copy(&gen->atomset, ident);
	bctrace("NAMED_PARAM %u %u", atom_id, idx);
	put(gen, GIL_OP_NAMED_PARAM);
	put_uint(gen, atom_id);
	put_uint(gen, idx);
}

void gil_gen_halt(struct gil_generator *gen) {
	bctrace1("HALT");
	put(gen, GIL_OP_HALT);
}

void gil_gen_rjmp(struct gil_generator *gen, gil_word len) {
	bctrace("RJMP %u", len);
	put(gen, GIL_OP_RJMP);
	put_uint(gen, len);
}

void gil_gen_rjmp_placeholder(struct gil_generator *gen) {
	bctrace1("PLACEHOLDER RJMP");
	put(gen, GIL_OP_RJMP_U4);
	put(gen, 0);
	put(gen, 0);
	put(gen, 0);
	put(gen, 0);
}

void gil_gen_discard(struct gil_generator *gen) {
	bctrace1("DISCARD");
	put(gen, GIL_OP_DISCARD);
}

void gil_gen_swap_discard(struct gil_generator *gen) {
	bctrace1("SWAP_DISCARD");
	put(gen, GIL_OP_SWAP_DISCARD);
}

void gil_gen_ret(struct gil_generator *gen) {
	bctrace1("RET");
	put(gen, GIL_OP_RET);
}

void gil_gen_mod_ret(struct gil_generator *gen) {
	bctrace1("MOD_RET");
	put(gen, GIL_OP_MOD_RET);
}

void gil_gen_none(struct gil_generator *gen) {
	bctrace1("ALLOC_NONE");
	put(gen, GIL_OP_ALLOC_NONE);
}

void gil_gen_number(struct gil_generator *gen, double num) {
	uint64_t n;
	memcpy(&n, &num, sizeof(num));
	bctrace("ALLOC_REAL %u", num);
	put(gen, GIL_OP_ALLOC_REAL);
	put_d8le(gen, num);
}

void gil_gen_atom(struct gil_generator *gen, char **str) {
	size_t id = gil_strset_put(&gen->atomset, str);
	bctrace("ALLOC_ATOM %u", id);
	put(gen, GIL_OP_ALLOC_ATOM);
	put_uint(gen, id);
}

void gil_gen_atom_copy(struct gil_generator *gen, char *str) {
	size_t id = gil_strset_put_copy(&gen->atomset, str);
	bctrace("ALLOC_ATOM %u", id);
	put(gen, GIL_OP_ALLOC_ATOM);
	put_uint(gen, id);
}

void gil_gen_string(struct gil_generator *gen, char **str) {
	size_t id = gil_strset_get(&gen->stringset, *str);
	if (id == 0) {
		size_t len = strlen(*str);

		bctrace("RJMP %u", len);
		gil_gen_rjmp(gen, len);
		gil_word pos = gen->pos;

		bctrace("STRING DATA %.*s", *str, len);
		gen->pos += len;
		gil_bufio_put_n(&gen->writer, *str, len);

		id = gil_strset_put(&gen->stringset, str);
		gen->strings = realloc(gen->strings, id * sizeof(*gen->strings));
		gen->strings[id - 1].length = len;
		gen->strings[id - 1].pos = pos;

		bctrace("ALLOC_BUFFER_STATIC %u %u", len, pos);
		put(gen, GIL_OP_ALLOC_BUFFER_STATIC);
		put_uint(gen, len);
		put_uint(gen, pos);
	} else {
		free(*str);
		struct gil_generator_string *s = &gen->strings[id - 1];
		bctrace("ALLOC_BUFFER_STATIC %u %u", s->length, s->pos);
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
		bctrace("ALLOC_BUFFER_STATIC %u %u", s->length, s->pos);
		put(gen, GIL_OP_ALLOC_BUFFER_STATIC);
		put_uint(gen, s->length);
		put_uint(gen, s->pos);
	}
}

void gil_gen_function(struct gil_generator *gen, gil_word pos) {
	bctrace("ALLOC_FUNCTION %u", pos);
	put(gen, GIL_OP_ALLOC_FUNCTION);
	put_uint(gen, pos);
}

void gil_gen_array(struct gil_generator *gen, gil_word count) {
	bctrace("ALLOC_ARRAY %u", count);
	put(gen, GIL_OP_ALLOC_ARRAY);
	put_uint(gen, count);
}

void gil_gen_module(struct gil_generator *gen, gil_word pos) {
	bctrace("LOAD_MODULE %u", pos);
	put(gen, GIL_OP_LOAD_MODULE);
	put_uint(gen, pos);
}

void gil_gen_namespace(struct gil_generator *gen) {
	bctrace1("ALLOC_NAMESPACE");
	put(gen, GIL_OP_ALLOC_NAMESPACE);
}

void gil_gen_namespace_set(struct gil_generator *gen, char **ident) {
	size_t atom_id = gil_strset_put(&gen->atomset, ident);
	bctrace("NAMESPACE_SET %u", atom_id);
	put(gen, GIL_OP_NAMESPACE_SET);
	put_uint(gen, atom_id);
}

void gil_gen_namespace_set_copy(struct gil_generator *gen, char *ident) {
	size_t atom_id = gil_strset_put_copy(&gen->atomset, ident);
	bctrace("NAMESPACE_SET %u", atom_id);
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
	bctrace("NAMESPACE_LOOKUP %u", atom_id);
	put(gen, GIL_OP_NAMESPACE_LOOKUP);
	put_uint(gen, atom_id);
}

void gil_gen_array_lookup(struct gil_generator *gen, int number) {
	bctrace("ARRAY_LOOKUP %u", number);
	put(gen, GIL_OP_ARRAY_LOOKUP);
	put_uint(gen, number);
}

void gil_gen_array_set(struct gil_generator *gen, int number) {
	bctrace("ARRAY_SET %u", number);
	put(gen, GIL_OP_ARRAY_SET);
	put_uint(gen, number);
}

void gil_gen_dynamic_lookup(struct gil_generator *gen) {
	bctrace1("DYNAMIC_LOOKUP");
	put(gen, GIL_OP_DYNAMIC_LOOKUP);
}

void gil_gen_dynamic_set(struct gil_generator *gen) {
	bctrace1("DYNAMIC_SET");
	put(gen, GIL_OP_DYNAMIC_SET);
}

void gil_gen_stack_frame_get_args(struct gil_generator *gen) {
	bctrace1("DYNAMIC_STACK_FRAME_GET_ARGS");
	put(gen, GIL_OP_STACK_FRAME_GET_ARGS);
}

void gil_gen_stack_frame_get_arg(struct gil_generator *gen, gil_word idx) {
	bctrace("DYNAMIC_STACK_FRAME_GET_ARG %u", idx);
	put(gen, GIL_OP_STACK_FRAME_GET_ARG);
	put_uint(gen, idx);
}

void gil_gen_stack_frame_lookup(struct gil_generator *gen, char **ident) {
	size_t atom_id = gil_strset_put(&gen->atomset, ident);
	bctrace("DYNAMIC_STACK_FRAME_LOOKUP %u", atom_id);
	put(gen, GIL_OP_STACK_FRAME_LOOKUP);
	put_uint(gen, atom_id);
}

void gil_gen_stack_frame_lookup_copy(struct gil_generator *gen, char *ident) {
	size_t atom_id = gil_strset_put_copy(&gen->atomset, ident);
	bctrace("DYNAMIC_STACK_FRAME_LOOKUP %u", atom_id);
	put(gen, GIL_OP_STACK_FRAME_LOOKUP);
	put_uint(gen, atom_id);
}

void gil_gen_stack_frame_set(struct gil_generator *gen, char **ident) {
	size_t atom_id = gil_strset_put(&gen->atomset, ident);
	bctrace("DYNAMIC_STACK_FRAME_SET %u", atom_id);
	put(gen, GIL_OP_STACK_FRAME_SET);
	put_uint(gen, atom_id);
}

void gil_gen_stack_frame_set_copy(struct gil_generator *gen, char *ident) {
	size_t atom_id = gil_strset_put_copy(&gen->atomset, ident);
	bctrace("DYNAMIC_STACK_FRAME_SET %u", atom_id);
	put(gen, GIL_OP_STACK_FRAME_SET);
	put_uint(gen, atom_id);
}

void gil_gen_stack_frame_replace(struct gil_generator *gen, char **ident) {
	size_t atom_id = gil_strset_put(&gen->atomset, ident);
	bctrace("DYNAMIC_STACK_FRAME_REPLACE %u", atom_id);
	put(gen, GIL_OP_STACK_FRAME_REPLACE);
	put_uint(gen, atom_id);
}

void gil_gen_stack_frame_replace_copy(struct gil_generator *gen, char *ident) {
	size_t atom_id = gil_strset_put_copy(&gen->atomset, ident);
	bctrace("DYNAMIC_STACK_FRAME_REPLACE %u", atom_id);
	put(gen, GIL_OP_STACK_FRAME_REPLACE);
	put_uint(gen, atom_id);
}

void gil_gen_assert(struct gil_generator *gen) {
	bctrace1("ASSERT");
	put(gen, GIL_OP_ASSERT);
}

void gil_gen_func_call(struct gil_generator *gen, gil_word argc) {
	bctrace("FUNC_CALL %u", argc);
	put(gen, GIL_OP_FUNC_CALL);
	put_uint(gen, argc);
}

void gil_gen_func_call_infix(struct gil_generator *gen) {
	bctrace1("FUNC_CALL_INFIX");
	put(gen, GIL_OP_FUNC_CALL_INFIX);
}
