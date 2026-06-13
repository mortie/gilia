#if defined(__unix__) || defined(__unix) || (defined(__APPLE__) && defined(__MACH__))

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
static char *error_message = NULL;

static void check_diff(
	const char *name,
	const char *expected, size_t expected_len,
	const char *actual, size_t actual_len)
{
	size_t len = expected_len > actual_len ? expected_len : actual_len;
	int line = 1;
	int ch = 1;
	for (size_t i = 0; i < len; ++i) {
		int ech = i >= expected_len ? EOF : expected[i];
		int ach = i >= actual_len ? EOF : actual[i];
		if (ech != ach) {

			char namebuf[512];
			snprintf(namebuf, sizeof(namebuf), "%s.expected", name);
			FILE *f = fopen(namebuf, "w");
			if (f) {
				fwrite(expected, 1, expected_len, f);
				fclose(f);
			}

			snprintf(namebuf, sizeof(namebuf), "%s.actual", name);
			f = fopen(namebuf, "w");
			if (f) {
				fwrite(actual, 1, actual_len, f);
				fclose(f);
			}

			snow_fail(
				"%s differs at location %i:%i:\n"
				"    Expected %c(%i), got %c(%i).\n"
				"    Wrote output to: %s.{expected,actual}.",
				name, line, ch, ech, ech, ach, ach, name);
		}

		if (ech == '\n') {
			line += 1;
			ch = 1;
		} else {
			ch += 1;
		}
	}
}

static void check_impl(const char *name) {
	char fname[] = __FILE__;
	char *dname = dirname(fname);
	snprintf(example_path, sizeof(example_path), "%s/../examples/%s", dname, name);

	FILE *inf = fopen(example_path, "r");
	if (inf == NULL) {
		snow_fail("%s: %s", example_path, strerror(errno));
	}

	// Compute expected output.
	// We do this by finding lines starting with '# => ' in the input file.
	struct gil_io_mem_writer expected_output = {
		.w.write = gil_io_mem_write,
	};
	char *line = NULL;
	size_t linecap = 0;
	while (1) {
		ssize_t n = getline(&line, &linecap, inf);
		if (n <= 0) {
			break;
		}

		if (strncmp(line, "# => ", 5) != 0) {
			continue;
		}

		gil_io_mem_write(&expected_output.w, line + 5, n - 5);
	}

	fseek(inf, 0, SEEK_SET);
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

	struct gil_io_mem_writer actual_output = {
		.w.write = gil_io_mem_write,
	};

	struct gil_vm vm;
	gil_vm_init(&vm, bytecode.mem, bytecode.len / sizeof(gil_word), &builtins.base);
	vm.std_output = &actual_output.w;

	// Run a GC after every instruction to uncover potential GC issues
	while (!vm.halted) {
		gil_vm_step(&vm);
		gil_vm_gc(&vm);
	}

	gil_vm_free(&vm);
	free(bytecode.mem);

	check_diff(
		example_path,
		expected_output.mem, expected_output.len,
		actual_output.mem, actual_output.len);
}

#define check(name) do { \
	snow_fail_update(); \
	test(name) { check_impl(name); } \
} while (0)

describe(exaples) {
	check("arrays.g");
	check("builtins.g");
	check("control-flow.g");
	check("dynamic-lookups.g");
	check("func-equals.g");
	check("functions.g");
	check("namespaces.g");
	check("readme.g");

	if (error_message != NULL) {
		free(error_message);
	}
}

#endif
