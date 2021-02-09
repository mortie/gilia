#include "parse/parse.h"
#include "vm/vm.h"
#include "vm/print.h"

#include <stdio.h>
#include <snow/snow.h>

static struct l2_lexer lex;
static struct l2_io_mem_reader r;
static struct l2_generator gen;
static struct l2_io_mem_writer w;
static struct l2_vm vm;

static struct l2_vm_value *var_lookup(const char *name) {
	l2_word atom_id = l2_strset_get(&gen.atomset, name);
	l2_word id = l2_vm_namespace_get(&vm, &vm.values[vm.nstack[1]], atom_id);
	return &vm.values[id];
}

static int eval_impl(const char *str, struct l2_parse_error *err) {
	r.r.read = l2_io_mem_read;
	r.idx = 0;
	r.len = strlen(str);
	r.mem = str;
	l2_lexer_init(&lex, &r.r);

	w.w.write = l2_io_mem_write;
	w.len = 0;
	w.mem = NULL;
	l2_gen_init(&gen, (struct l2_io_writer *)&w);

	if (l2_parse_program(&lex, &gen, err) < 0) {
		free(w.mem);
		return -1;
	}

	l2_vm_init(&vm, (l2_word *)w.mem, w.len / sizeof(l2_word));
	l2_vm_run(&vm);

	free(w.mem);
	return 0;
}

#define eval(str) do { \
	snow_fail_update(); \
	struct l2_parse_error err; \
	if (eval_impl(str, &err) < 0) { \
		snow_fail("Parsing failed: %i:%i: %s", err.line, err.ch, err.message); \
	} \
} while (0)

describe(eval) {
	test("assignment") {
		eval("foo := 10");
		defer(l2_vm_free(&vm));
		defer(l2_gen_free(&gen));

		asserteq(l2_vm_value_type(var_lookup("foo")), L2_VAL_TYPE_REAL);
		asserteq(var_lookup("foo")->real, 10);
	}

	test("var deref assignment") {
		eval("foo := 10\nbar := foo");
		defer(l2_vm_free(&vm));
		defer(l2_gen_free(&gen));

		asserteq(l2_vm_value_type(var_lookup("foo")), L2_VAL_TYPE_REAL);
		asserteq(var_lookup("foo")->real, 10);
		asserteq(l2_vm_value_type(var_lookup("bar")), L2_VAL_TYPE_REAL);
		asserteq(var_lookup("bar")->real, 10);
	}

	test("string assignment") {
		eval("foo := \"hello world\"");
		defer(l2_vm_free(&vm));
		defer(l2_gen_free(&gen));

		asserteq(l2_vm_value_type(var_lookup("foo")), L2_VAL_TYPE_BUFFER);
		struct l2_vm_buffer *buf = var_lookup("foo")->buffer;
		asserteq(buf->len, 11);
		assert(strncmp(buf->data, "hello world", 11) == 0);
	}
}
