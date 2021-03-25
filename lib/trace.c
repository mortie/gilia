#include "trace.h"

#if GIL_ENABLE_TRACE

#include <stdio.h>
#include <stdarg.h>

int gil_trace_depth = 0;

void gil_trace_push(const char *name) {
	for (int i = 0; i < gil_trace_depth; ++i) {
		fprintf(stderr, "    ");
	}

	fprintf(stderr, "%s {\n", name);
	gil_trace_depth += 1;
}

void gil_trace_pop() {
	gil_trace_depth -= 1;
	for (int i = 0; i < gil_trace_depth; ++i) {
		fprintf(stderr, "    ");
	}

	fprintf(stderr, "}\n");
}

void gil_trace(const char *fmt, ...) {
	for (int i = 0; i < gil_trace_depth; ++i) {
		fprintf(stderr, "    ");
	}

	va_list va;
	va_start(va, fmt);
	vfprintf(stderr, fmt, va);
	fprintf(stderr, "\n");
}

void gil_trace_cleanup(void *unused) {
	gil_trace_pop();
}

#endif

// ISO C forbids an empty translation unit
// (and GCC warns on usude variables, but __attribute__((unused)) isn't ISO C)
#ifdef __GNUC__
__attribute__((unused)) static int make_compiler_happy;
#else
static int make_compiler_happy;
#endif
