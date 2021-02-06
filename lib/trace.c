#include "trace.h"

#if L2_ENABLE_TRACE

#include <stdio.h>
#include <stdarg.h>

int l2_trace_depth = 0;

void l2_trace_push(const char *name) {
	for (int i = 0; i < l2_trace_depth; ++i) {
		fprintf(stderr, "    ");
	}

	fprintf(stderr, "%s {\n", name);
	l2_trace_depth += 1;
}

void l2_trace_pop() {
	l2_trace_depth -= 1;
	for (int i = 0; i < l2_trace_depth; ++i) {
		fprintf(stderr, "    ");
	}

	fprintf(stderr, "}\n");
}

void l2_trace(const char *fmt, ...) {
	for (int i = 0; i < l2_trace_depth; ++i) {
		fprintf(stderr, "    ");
	}

	va_list va;
	va_start(va, fmt);
	vfprintf(stderr, fmt, va);
	fprintf(stderr, "\n");
}

void l2_trace_cleanup(void *unused) {
	l2_trace_pop();
}

#endif
