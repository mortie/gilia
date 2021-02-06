#include "trace.h"

#ifdef L2_ENABLE_TRACE

#include <stdio.h>

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

void l2_trace(const char *name) {
	fprintf(stderr, "TRACE %s\n", name);
}

void l2_trace_cleanup(void *unused) {
	l2_trace_pop();
}

#endif
