#ifndef L2_TRACE_H
#define L2_TRACE_H

#ifndef L2_ENABLE_TRACE

#define l2_trace_scope(name) do {} while (0)
#define l2_trace_func() do {} while (0)
#define l2_trace_push(name) do {} while (0)
#define l2_trace_pop() do {} while (0)
#define l2_trace(name) do {} while (0)

#else

extern int l2_trace_depth;

#ifdef __GNUC__
#define l2_trace_scope(name) \
	l2_trace_push(name); \
	__attribute__((cleanup(l2_trace_cleanup))) int l2_trace_scope
#define l2_trace_func() l2_trace_scope(__func__)
#else
#define l2_trace_scope(name) l2_trace(name)
#define l2_trace_func() l2_trace(__func__)
#endif

void l2_trace_push(const char *name);
void l2_trace_pop();
void l2_trace(const char *name);
void l2_trace_cleanup(void *unused);

#endif

#endif
