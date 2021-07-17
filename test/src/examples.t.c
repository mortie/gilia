#ifdef __unix__

#include "vm/vm.h"
#include "parse/parse.h"
#include "parse/lex.h"
#include "gen/gen.h"
#include "modules/builtins.h"

#include <sys/types.h>
#include <libgen.h>
#include <dirent.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <snow/snow.h>

static char example_path[512];
static char example_expected_path[512];
static char example_actual_path[512];
static char *error_message = NULL;

static void check_diff(const char *expected, const char *actual) {
	FILE *e = fopen(expected, "r");
	if (e == NULL) {
		snow_fail("%s: %s\n    Actual result stored in %s", expected, strerror(errno), actual);
	}

	FILE *a = fopen(actual, "r");
	if (a == NULL) {
		fclose(e);
		snow_fail("%s: %s", actual, strerror(errno));
	}

	int line = 1;
	int ch = 1;
	while (1) {
		int ech = fgetc(e);
		int ach = fgetc(a);
		if (ech != ach) {
			fclose(e);
			fclose(a);
			snow_fail("%s differs at location %i:%i:\n    Expected %c(%i), got %c(%i)",
				actual, line, ch, ech, ech, ach, ach);
		}

		if (ech == EOF) {
			break;
		}

		if (ech == '\n') {
			line += 1;
			ch = 1;
		} else {
			ch += 1;
		}
	}

	fclose(e);
	fclose(a);
}

static void check_impl(const char *name) {
	char fname[] = __FILE__;
	char *dname = dirname(fname);
	snprintf(example_path, sizeof(example_path), "%s/../examples/%s", dname, name);
	snprintf(example_expected_path, sizeof(example_path), "%s/../examples/%s.expected", dname, name);
	snprintf(example_actual_path, sizeof(example_path), "%s/../examples/%s.actual", dname, name);

	FILE *inf = fopen(example_path, "r");
	if (inf == NULL) {
		snow_fail("%s: %s", example_path, strerror(errno));
	}

	struct gil_io_file_reader input ={
		.r.read = gil_io_file_read,
		.f = inf,
	};

	struct gil_io_mem_writer bytecode = {
		.w.write = gil_io_mem_write,
	};

	struct gil_lexer lexer;
	gil_lexer_init(&lexer, &input.r);

	struct gil_mod_builtins builtins;
	gil_mod_builtins_init(&builtins);

	struct gil_generator gen;
	gil_gen_init(&gen, &bytecode.w, &builtins.base, NULL);

	struct gil_parse_error err;
	struct gil_parse_context ctx = {&lexer, &gen, &err};
	if (gil_parse_program(&ctx) < 0) {
		free(bytecode.mem);
		fclose(input.f);
		error_message = err.message;
		snow_fail("%s:%i:%i: %s", example_path, err.line, err.ch, err.message);
	}

	for (gil_word i = 0; i < gen.relocslen; ++i) {
		gil_word pos = gen.relocs[i].pos;
		gil_word rep = gen.relocs[i].replacement;
		unsigned char *mem = &((unsigned char *)bytecode.mem)[pos];
		mem[0] = rep & 0xff;
		mem[1] = (rep >> 8) & 0xff;
		mem[2] = (rep >> 16) & 0xff;
		mem[3] = (rep >> 24) & 0xff;
	}

	gil_gen_free(&gen);
	fclose(inf);

	FILE *outf = fopen(example_actual_path, "w");
	if (outf == NULL) {
		snow_fail("%s: %s", example_actual_path, strerror(errno));
	}

	struct gil_io_file_writer output = {
		.w.write = gil_io_file_write,
		.f = outf,
	};

	struct gil_vm vm;
	gil_vm_init(&vm, bytecode.mem, bytecode.len / sizeof(gil_word), &builtins.base);
	vm.std_output = &output.w;

	// Run a GC after every instruction to uncover potential GC issues
	while (!vm.halted) {
		gil_vm_step(&vm);
		gil_vm_gc(&vm);
	}

	gil_vm_free(&vm);
	free(bytecode.mem);
	fclose(output.f);

	check_diff(example_expected_path, example_actual_path);
}

#define check(name) do { \
	snow_fail_update(); \
	test(name) { check_impl(name); } \
} while (0)

describe(exaples) {
	check("namespaces.g");
	check("arrays.g");
	check("functions.g");
	check("dynamic-lookups.g");
	check("builtins.g");
	check("control-flow.g");

	if (error_message != NULL) {
		free(error_message);
	}
}

#endif
