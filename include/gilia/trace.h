#if defined(GIL_TRACE_H)
#error "trace.h can only be included once"
#else
#define GIL_TRACE_H

#if !defined(GIL_ENABLE_TRACE)

#define gil_trace_enable(tracer) do {} while (0)

#define gil_trace_scope(name) do {} while (0)
#define gil_trace_func() do {} while (0)
#define gil_trace_push(name) do {} while (0)
#define gil_trace_pop() do {} while (0)
#define gil_trace(...) do {} while (0)

#elif defined(GIL_ENABLE_TRACE) && defined(GIL_TRACER_NAME)

#ifdef __GNUC__
#define gil_trace_scope(name) \
	gil_trace_push(name); \
	__attribute__((unused,cleanup(gil_trace_cleanup_func_impl))) \
		const char *gil_trace_scope_ ## __COUNTER__ = GIL_TRACER_NAME;
#define gil_trace_func() gil_trace_scope(__func__)
#else
#define gil_trace_scope(name) gil_trace(name)
#define gil_trace_func() gil_trace(__func__)
#endif

#define gil_trace_push(name) gil_trace_push_impl(GIL_TRACER_NAME, name)
#define gil_trace_pop() gil_trace_pop_impl(GIL_TRACER_NAME)
#define gil_trace(...) gil_trace_impl(GIL_TRACER_NAME, __VA_ARGS__)

#define gil_tracer_enabled() gil_tracer_enabled_impl(GIL_TRACER_NAME)

#endif

#ifdef GIL_ENABLE_TRACE

void gil_trace_enable(const char *tracer);
int gil_tracer_enabled_impl(const char *tracer);

void gil_trace_push_impl(const char *tracer, const char *name);
void gil_trace_pop_impl(const char *tracer);
void gil_trace_impl(const char *tracer, const char *fmt, ...);
void gil_trace_cleanup_func_impl(const char **tracer);

#endif

#endif
