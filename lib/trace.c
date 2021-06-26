#include "trace.h"

#ifdef GIL_ENABLE_TRACE

#include <stdio.h>
#include <stdarg.h>
#include <string.h>

static int gil_trace_depth = 0;

static const char *tracetbl[8];
static int tracercount = 0;

int should_trace(const char *tracer) {
	for (int i = 0; i < tracercount; ++i) {
		if (strcmp(tracetbl[i], tracer) == 0) {
			return 1;
		}
	}

	return 0;
}

void gil_trace_enable(const char *tracer) {
	if (tracercount >= (int)(sizeof(tracetbl) / sizeof(*tracetbl))) {
		return;
	}

	tracetbl[tracercount++] = tracer;
}

int gil_tracer_enabled_impl(const char *tracer) {
	return should_trace(tracer);
}

void gil_trace_push_impl(const char *tracer, const char *name) {
	if (!should_trace(tracer)) {
		return;
	}

	for (int i = 0; i < gil_trace_depth; ++i) {
		fprintf(stderr, "    ");
	}

	fprintf(stderr, "[%s] %s {\n", tracer, name);
	gil_trace_depth += 1;
}

void gil_trace_pop_impl(const char *tracer) {
	if (!should_trace(tracer)) {
		return;
	}

	gil_trace_depth -= 1;
	for (int i = 0; i < gil_trace_depth; ++i) {
		fprintf(stderr, "    ");
	}

	fprintf(stderr, "}\n");
}

void gil_trace_impl(const char *tracer, const char *fmt, ...) {
	if (!should_trace(tracer)) {
		return;
	}

	for (int i = 0; i < gil_trace_depth; ++i) {
		fprintf(stderr, "    ");
	}

	fprintf(stderr, "[%s] ", tracer);

	va_list va;
	va_start(va, fmt);
	vfprintf(stderr, fmt, va);
	fprintf(stderr, "\n");
}

void gil_trace_cleanup_func_impl(const char **tracer) {
	gil_trace_pop_impl(*tracer);
}

#endif

// ISO C forbids an empty translation unit
// (and GCC warns on usude variables, but __attribute__((unused)) isn't ISO C)
#ifdef __GNUC__
__attribute__((unused)) static int make_compiler_happy;
#else
static int make_compiler_happy;
#endif
