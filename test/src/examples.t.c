#ifdef __unix__

#include "vm/vm.h"
#include "parse/parse.h"

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

	struct l2_io_file_reader input ={
		.r.read = l2_io_file_read,
		.f = inf,
	};

	struct l2_io_mem_writer bytecode = {
		.w.write = l2_io_mem_write,
		.len = 0,
		.mem = NULL,
	};

	struct l2_lexer lexer;
	l2_lexer_init(&lexer, &input.r);

	struct l2_generator gen;
	l2_gen_init(&gen, &bytecode.w);

	struct l2_parse_error err;
	if (l2_parse_program(&lexer, &gen, &err) < 0) {
		free(bytecode.mem);
		fclose(input.f);
		error_message = err.message;
		snow_fail("%s: %s", example_path, err.message);
	}

	l2_gen_free(&gen);
	fclose(inf);

	FILE *outf = fopen(example_actual_path, "w");
	if (outf == NULL) {
		snow_fail("%s: %s", example_actual_path, strerror(errno));
	}

	struct l2_io_file_writer output = {
		.w.write = l2_io_file_write,
		.f = outf,
	};

	struct l2_vm vm;
	l2_vm_init(&vm, bytecode.mem, bytecode.len / sizeof(l2_word));
	vm.std_output = &output.w;

	// Run a GC after every instruction to uncover potential GC issues
	while (vm.ops[vm.iptr] != L2_OP_HALT) {
		l2_vm_step(&vm);
		l2_vm_gc(&vm);
	}

	l2_vm_free(&vm);
	free(bytecode.mem);
	fclose(output.f);

	check_diff(example_expected_path, example_actual_path);
}

#define check(name) do { \
	snow_fail_update(); \
	test(name) { check_impl(name); } \
} while (0)

describe(exaples) {
	check("namespaces.l2");
	check("arrays.l2");
	check("functions.l2");
	check("dynamic-lookups.l2");

	if (error_message != NULL) {
		free(error_message);
	}
}

#endif
