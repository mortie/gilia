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
static struct l2_parse_error err;

static struct l2_vm_value *var_lookup(const char *name) {
	l2_word atom_id = l2_strset_get(&gen.atoms, name);
	l2_word id = l2_vm_namespace_get(&vm.values[vm.nstack[0]], atom_id);
	return &vm.values[id];
}

static int exec(const char *str) {
	r.r.read = l2_io_mem_read;
	r.idx = 0;
	r.len = strlen(str);
	r.mem = str;
	l2_lexer_init(&lex, &r.r);

	w.w.write = l2_io_mem_write;
	w.len = 0;
	w.mem = NULL;
	l2_gen_init(&gen, (struct l2_io_writer *)&w);

	if (l2_parse_program(&lex, &gen, &err) < 0) {
		free(w.mem);
		return -1;
	}

	l2_vm_init(&vm, (l2_word *)w.mem, w.len / sizeof(l2_word));
	l2_vm_run(&vm);

	free(w.mem);
	return 0;
}

describe(exec) {
	test("exec assignment") {
		exec("foo := 10");
		defer(l2_vm_free(&vm));
		defer(l2_gen_free(&gen));

		assert(l2_vm_value_type(var_lookup("foo")) == L2_VAL_TYPE_REAL);
		assert(var_lookup("foo")->real == 10);
	}
}
