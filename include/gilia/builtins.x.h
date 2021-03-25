#ifdef XNAME
XNAME("none", knone)
#endif

#ifdef XATOM
XATOM("true", ktrue)
XATOM("false", kfalse)
XATOM("stop", kstop)
#endif

#ifdef XFUNCTION
XFUNCTION("+", gil_builtin_add)
XFUNCTION("-", gil_builtin_sub)
XFUNCTION("*", gil_builtin_mul)
XFUNCTION("/", gil_builtin_div)
XFUNCTION("==", gil_builtin_eq)
XFUNCTION("!=", gil_builtin_neq)
XFUNCTION("<", gil_builtin_lt)
XFUNCTION("<=", gil_builtin_lteq)
XFUNCTION(">", gil_builtin_gt)
XFUNCTION(">=", gil_builtin_gteq)
XFUNCTION("&&", gil_builtin_land)
XFUNCTION("||", gil_builtin_lor)
XFUNCTION("??", gil_builtin_first)
XFUNCTION("print", gil_builtin_print)
XFUNCTION("len", gil_builtin_len)
XFUNCTION("if", gil_builtin_if)
XFUNCTION("loop", gil_builtin_loop)
XFUNCTION("while", gil_builtin_while)
XFUNCTION("for", gil_builtin_for)
XFUNCTION("guard", gil_builtin_guard)
#endif
