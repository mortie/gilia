#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#if defined(__unix__) || defined(__unix) || (defined(__APPLE__) && defined(__MACH__))
#define USE_READLINE
#define USE_POSIX
#include <time.h>
#include <unistd.h>
#include <readline/readline.h>
#include <readline/history.h>
#endif

#include "bytecode.h"
#include "gen/gen.h"
#include "gen/fs_resolver.h"
#include "io.h"
#include "loader.h"
#include "modules/builtins.h"
#include "modules/fs.h"
#include "parse/lex.h"
#include "parse/parse.h"
#include "vm/print.h"
#include "vm/vm.h"
#include "trace.h"

struct gil_module;

static int do_print_bytecode = 0;
static int do_step = 0;
static int do_serialize_bytecode = 0;
static int do_repl = 0;
static char *input_filename = "-";

static struct gil_mod_builtins builtins;
static struct gil_module **modules;
static size_t moduleslen;
static struct gil_fs_resolver resolver;

static int parse_text(FILE *inf, struct gil_io_mem_writer *w) {
	// Init lexer with its input reader
	struct gil_io_file_reader r;
	r.r.read = gil_io_file_read;
	r.f = inf;
	struct gil_lexer lexer;
	gil_lexer_init(&lexer, &r.r);

	// Init gen with its output writer
	struct gil_generator gen;
	gil_gen_init(&gen, &w->w, &builtins.base, &resolver.base);

	for (size_t i = 0; i < moduleslen; ++i) {
		gil_gen_register_module(&gen, modules[i]);
	}

	struct gil_parse_error err;
	struct gil_parse_context ctx = {&lexer, &gen, &err};
	if (gil_parse_program(&ctx) < 0) {
		fprintf(stderr, "Parse error: %s:%i:%i: %s\n",
				input_filename, err.line, err.ch, err.message);
		gil_parse_error_free(&err);
		gil_gen_free(&gen);
		return -1;
	}

	for (gil_word i = 0; i < gen.relocslen; ++i) {
		gil_word pos = gen.relocs[i].pos;
		gil_word rep = gen.relocs[i].replacement;
		unsigned char *mem = &((unsigned char *)w->mem)[pos];
		mem[0] = rep & 0xff;
		mem[1] = (rep >> 8) & 0xff;
		mem[2] = (rep >> 16) & 0xff;
		mem[3] = (rep >> 24) & 0xff;
	}

	gil_gen_free(&gen);
	return 0;
}

static void step_through(struct gil_vm *vm) {
	printf("=====\n\nInitial state:\n");
	struct gil_io_file_writer w = {
		.w.write =gil_io_file_write,
		.f = stdout,
	};
	gil_vm_print_state(&w.w, vm);

	char buf[16];
	while (!vm->halted) {
		size_t iptr = vm->iptr;
		printf("\n======\n\n(%d) Will run instr: ", vm->iptr);
		if (vm->need_check_retval) {
			printf("(internal)\n");
		} else {
			gil_vm_print_op(&w.w, vm->ops, vm->opslen, &iptr);
			printf("\n");
		}

		if (fgets(buf, sizeof(buf), stdin) == NULL) {
			break;
		}

		gil_vm_step(vm);
		gil_vm_gc(vm);
		gil_vm_print_state(&w.w, vm);
	}
}

static void repl() {
	struct gil_io_mem_writer w = {
		.w.write = gil_io_mem_write,
	};

	struct gil_io_mem_reader r = {
		.r.read = gil_io_mem_read,
	};

	struct gil_lexer lexer;

	struct gil_generator gen;
	gil_gen_init(&gen, &w.w, &builtins.base, NULL);

	struct gil_vm vm;
	gil_vm_init(&vm, NULL, 0, &builtins.base);

	struct gil_io_file_writer stdout_writer = {
		.w.write = gil_io_file_write,
		.f = stdout,
	};

	struct gil_parse_error err;
	struct gil_parse_context ctx = {&lexer, &gen, &err};

	while (1) {
		char line[4096];
#ifdef USE_READLINE
		char *rline = readline("> ");
		if (rline == NULL) goto out;
		if (rline[0] == '\0') goto next;
		add_history(rline);
		snprintf(line, sizeof(line), "$$ := %s", rline);
#else
		char rline[4096];
		if (fgets(rline, sizeof(rline), stdin) == NULL) goto out;
		if (rline[0] == '\n' && rline[1] == '\0') goto next;
		snprintf(line, sizeof(line), "$$ := %s", rline);
#endif

		if (strncmp(rline, "\\state", strlen("\\state")) == 0) {
			gil_vm_print_state(&stdout_writer.w, &vm);
			goto next;
		}

		// The next instruction should be the code we're about to generate
		vm.iptr = w.len;

		// Generate code for the user's line, store the output
		// in the variable '$$'
		r.idx = 0;
		r.mem = line;
		r.len = strlen(r.mem);
		gil_lexer_init(&lexer, &r.r);
		if (gil_parse_program(&ctx) < 0) {
			fprintf(stderr, "Parse error: %s\n -- %s\n", err.message, line);
			gil_parse_error_free(&err);

			// Jump past the generated code
			vm.iptr = w.len;
			goto next;
		}

		// Skip the last HALT, because we're gonna writing more code
		// to the buffer which we want to run as well
		w.len -= 1;

		// Generate code for printing the result from variable '$$'
		r.idx = 0;
		r.mem = "print $$\n";
		r.len = strlen(r.mem);
		gil_lexer_init(&lexer, &r.r);
		if (gil_parse_program(&ctx) < 0) {
			fprintf(stderr, "%s\n", err.message);
			gil_parse_error_free(&err);

			// Jump past the generated code
			vm.iptr = w.len;
			goto next;
		}

		// Run the resulting code
		vm.ops = w.mem;
		vm.opslen = w.len;
		vm.halted = 0;
		while (!vm.halted) {
			gil_vm_step(&vm);
		}

		gil_vm_gc(&vm);

next: ;
#ifdef USE_READLINE
		free(rline);
#endif
	}

out:
	gil_gen_free(&gen);
	gil_vm_free(&vm);
}

static void usage(const char *argv0) {
	printf("Usage: %s [options] [input|-]\n", argv0);
	printf("\n");
	printf("Options:\n");
	printf("  --help:            Print this help text\n");
	printf("  --bytecode:        Print the generated bytecode, don't execute\n");
	printf("  --step:            Step through the program\n");
	printf("  --repl:            Start a repl\n");
	printf("  --output,-o <out>: Write bytecode to file\n");
#ifdef USE_POSIX
	printf("  --timeout <secs>:  Run instructions for <secs> seconds\n");
#endif
#ifdef GIL_ENABLE_TRACE
	printf("  --trace-lexer:     Trace the lexer\n");
	printf("  --trace-parser:    Trace the parser\n");
	printf("  --trace-vm:        Trace the lexer\n");
#endif
}

int main(int argc, char **argv) {
#ifdef USE_POSIX
	double timeout = -1;
#endif

	int was_inf_set = 0;
	FILE *inf = stdin;
	FILE *outbc = NULL;

#ifdef USE_POSIX
	do_repl = isatty(0);
#endif

	int dashes = 0;
	for (int i = 1; i < argc; ++i) {
		if (!dashes && strcmp(argv[i], "--help") == 0) {
			usage(argv[0]);
			return 0;
		} else if (!dashes && strcmp(argv[i], "--bytecode") == 0) {
			do_print_bytecode = 1;
		} else if (!dashes && strcmp(argv[i], "--step") == 0) {
			do_step = 1;
		} else if (!dashes && strcmp(argv[i], "--repl") == 0) {
			do_repl = 1;
		} else if (!dashes && (
				strcmp(argv[i], "--output") == 0 || strcmp(argv[i], "-o") == 0)) {
			if (i == argc - 1) {
				fprintf(stderr, "%s expects an argument\n", argv[i]);
				return 1;
			}

			do_serialize_bytecode = 1;
			i += 1;
			if (strcmp(argv[i], "-") == 0) {
				outbc = stdout;
			} else {
				outbc = fopen(argv[i], "w");
				if (outbc == NULL) {
					perror(argv[i]);
					return 1;
				}
			}
#ifdef USE_POSIX
		} else if (!dashes && strcmp(argv[i], "--timeout") == 0) {
			if (i == argc - 1) {
				fprintf(stderr, "%s expects an argument\n", argv[i]);
				return 1;
			}

			i += 1;
			timeout = strtod(argv[i], NULL);
#endif
#ifdef GIL_ENABLE_TRACE
		} else if (!dashes && strcmp(argv[i], "--trace-lexer") == 0) {
			gil_trace_enable("lexer");
		} else if (!dashes && strcmp(argv[i], "--trace-parser") == 0) {
			gil_trace_enable("parser");
		} else if (!dashes && strcmp(argv[i], "--trace-vm") == 0) {
			gil_trace_enable("vm");
#endif
		} else if (strcmp(argv[i], "--") == 0) {
			dashes = 1;
		} else if (strcmp(argv[i], "-") == 0) {
			do_repl = 0;
			input_filename = "-";
			inf = stdin;
		} else if (!was_inf_set) {
			do_repl = 0;
			input_filename = argv[i];
			inf = fopen(argv[i], "r");
			was_inf_set = 1;
			if (inf == NULL) {
				perror(argv[i]);
				return 1;
			}
		} else {
			printf("Unexpected argument: %s\n", argv[i]);
			usage(argv[0]);
			return 1;
		}
	}

	gil_mod_builtins_init(&builtins);

	struct gil_mod_fs mod_fs;
	gil_mod_fs_init(&mod_fs);

	struct gil_module *mods[] = {
		&mod_fs.base,
	};
	modules = mods;
	moduleslen = sizeof(mods) / sizeof(*mods); // NOLINT(bugprone-sizeof-expression)

	gil_fs_resolver_init(&resolver, input_filename);

	if (do_repl) {
		repl();
		printf("\n");
		gil_fs_resolver_destroy(&resolver);
		return 0;
	}

	struct gil_io_mem_writer bytecode_writer = {
		.w.write = gil_io_mem_write,
	};

	int headerbyte = fgetc(inf);
	if (headerbyte != EOF) {
		if (ungetc(headerbyte, inf) == EOF) {
			fprintf(stderr, "%s: Reading file failed.\n", input_filename);
			gil_fs_resolver_destroy(&resolver);
			return 1;
		}
	}

	// Detect whether input is compiled bytecode or not
	// (compile bytecode starts with (ESC) 'g' 'l' 'c')
	unsigned char header[4];
	if (
			headerbyte == 0x1b &&
			fread(header, 1, 4, inf) >= 4 &&
			header[0] == 0x1b && header[1] == 0x67 &&
			header[2] == 0x6c && header[3] == 0x63) {
		if (gil_bc_load(inf, &bytecode_writer.w) < 0) {
			return 1;
		}
	} else if (headerbyte == 0x1b) {
		fprintf(
				stderr, "%s: Corrupt file? Start byte is 0x1b (ESC)"
				"but the header is wrong\n",
				input_filename);
		return 1;
	} else {
		if (parse_text(inf, &bytecode_writer) < 0) {
			fclose(inf);
			free(bytecode_writer.mem);
			return 1;
		}
	}
	fclose(inf);

	if (do_print_bytecode) {
		struct gil_io_file_writer w = {
			.w.write = gil_io_file_write,
			.f = stdout,
		};
		gil_vm_print_bytecode(&w.w, bytecode_writer.mem, bytecode_writer.len);
	}

	if (do_serialize_bytecode) {
		if (gil_bc_serialize(outbc, bytecode_writer.mem, bytecode_writer.len) < 0) {
			return 1;
		}
	}

	if (do_print_bytecode || do_serialize_bytecode) {
		free(bytecode_writer.mem);
		return 0;
	}

	struct gil_vm vm;
	gil_vm_init(&vm, bytecode_writer.mem, bytecode_writer.len, &builtins.base);

	for (size_t i = 0; i < moduleslen; ++i) {
		gil_vm_register_module(&vm, modules[i]);
	}

	if (do_step) {
		step_through(&vm);
#ifdef USE_POSIX
	} else if (timeout > 0) {
		// I usually like to work with time as doubles containing seconds,
		// but we're gonna have to check if we're over after every single instruction,
		// so we do some work ahead of time to make that check really cheap
		// in the hot loop
		struct timespec end, now;
		if (clock_gettime(CLOCK_MONOTONIC, &end) < 0) {
			perror("clock_gettime");
			gil_vm_free(&vm);
			free(bytecode_writer.mem);
			return 1;
		}

		end.tv_sec += (time_t)timeout;
		end.tv_nsec += (long)((timeout - (long)timeout) * 1000000000);
		if (end.tv_nsec > 1000000000) {
			end.tv_nsec -= 1000000000;
			end.tv_sec += 1;
		}

		while (!vm.halted) {
			gil_vm_step(&vm);

			if (clock_gettime(CLOCK_MONOTONIC, &now) < 0) {
				perror("clock_gettime");
				gil_vm_free(&vm);
				free(bytecode_writer.mem);
				return 1;
			} else if (
					now.tv_sec > end.tv_sec ||
					(now.tv_sec == end.tv_sec && now.tv_nsec >= end.tv_nsec)) {
				fprintf(stderr, "Timeout reached.\n");
				break;
			}
		}
#endif
	} else {
		gil_vm_run(&vm);
	}

	gil_vm_free(&vm);
	free(bytecode_writer.mem);
}
