#ifndef GIL_TRACE_H
#define GIL_TRACE_H

#if !GIL_ENABLE_TRACE

#define gil_trace_scope(name) do {} while (0)
#define gil_trace_func() do {} while (0)
#define gil_trace_push(name) do {} while (0)
#define gil_trace_pop() do {} while (0)
#define gil_trace(...) do {} while (0)

#else

extern int gil_trace_depth;

#ifdef __GNUC__
#define gil_trace_scope(name) \
	gil_trace_push(name); \
	__attribute__((unused,cleanup(gil_trace_cleanup))) int gil_trace_scope
#define gil_trace_func() gil_trace_scope(__func__)
#else
#define gil_trace_scope(name) gil_trace(name)
#define gil_trace_func() gil_trace(__func__)
#endif

void gil_trace_push(const char *name);
void gil_trace_pop();
void gil_trace(const char *fmt, ...);
void gil_trace_cleanup(void *unused);

#endif

#endif
