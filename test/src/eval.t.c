#include "parse/parse.h"
#include "vm/vm.h"
#include "vm/print.h"

#include <stdio.h>
#include <snow/snow.h>

static struct gil_lexer lex;
static struct gil_io_mem_reader r;
static struct gil_generator gen;
static struct gil_io_mem_writer w;
static struct gil_vm vm;

static struct gil_vm_value *var_lookup(const char *name) {
	gil_word atom_id = gil_strset_get(&gen.atomset, name);
	gil_word id = gil_vm_namespace_get(&vm, &vm.values[vm.fstack[1].ns], atom_id);
	return &vm.values[id];
}

static int eval_impl(const char *str, struct gil_parse_error *err) {
	r.r.read = gil_io_mem_read;
	r.idx = 0;
	r.len = strlen(str);
	r.mem = str;
	gil_lexer_init(&lex, &r.r);

	w.w.write = gil_io_mem_write;
	w.len = 0;
	w.size = 0;
	w.mem = NULL;
	gil_gen_init(&gen, (struct gil_io_writer *)&w);

	struct gil_parse_context ctx = {&lex, &gen, err};
	if (gil_parse_program(&ctx) < 0) {
		free(w.mem);
		return -1;
	}

	gil_vm_init(&vm, w.mem, w.len / sizeof(gil_word));
	gil_vm_run(&vm);

	free(w.mem);
	return 0;
}

#define eval(str) do { \
	snow_fail_update(); \
	struct gil_parse_error err; \
	if (eval_impl(str, &err) < 0) { \
		snow_fail("Parsing failed: %i:%i: %s", err.line, err.ch, err.message); \
	} \
} while (0)

describe(eval) {
	test("assignment") {
		eval("foo := 10");
		defer(gil_vm_free(&vm));
		defer(gil_gen_free(&gen));

		asserteq(gil_value_get_type(var_lookup("foo")), GIL_VAL_TYPE_REAL);
		asserteq(var_lookup("foo")->real.real, 10);
	}

	test("var deref assignment") {
		eval("foo := 10\nbar := foo");
		defer(gil_vm_free(&vm));
		defer(gil_gen_free(&gen));

		asserteq(gil_value_get_type(var_lookup("foo")), GIL_VAL_TYPE_REAL);
		asserteq(var_lookup("foo")->real.real, 10);
		asserteq(gil_value_get_type(var_lookup("bar")), GIL_VAL_TYPE_REAL);
		asserteq(var_lookup("bar")->real.real, 10);
	}

	test("string assignment") {
		eval("foo := \"hello world\"");
		defer(gil_vm_free(&vm));
		defer(gil_gen_free(&gen));

		asserteq(gil_value_get_type(var_lookup("foo")), GIL_VAL_TYPE_BUFFER);
		struct gil_vm_value *buf = var_lookup("foo");
		asserteq(buf->buffer.length, 11);
		assert(strncmp(buf->buffer.buffer, "hello world", 11) == 0);
	}
}
