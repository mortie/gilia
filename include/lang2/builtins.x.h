// X macro: Define a macro named X, then include this file, then undef X.

#ifdef XNAME
XNAME("none", knone)
#endif

#ifdef XATOM
XATOM("true", ktrue)
XATOM("false", kfalse)
XATOM("stop", kstop)
#endif

#ifdef XFUNCTION
XFUNCTION("+", l2_builtin_add)
XFUNCTION("-", l2_builtin_sub)
XFUNCTION("*", l2_builtin_mul)
XFUNCTION("/", l2_builtin_div)
XFUNCTION("==", l2_builtin_eq)
XFUNCTION("!=", l2_builtin_neq)
XFUNCTION("<", l2_builtin_lt)
XFUNCTION("<=", l2_builtin_lteq)
XFUNCTION(">", l2_builtin_gt)
XFUNCTION(">=", l2_builtin_gteq)
XFUNCTION("&&", l2_builtin_land)
XFUNCTION("||", l2_builtin_lor)
XFUNCTION("??", l2_builtin_first)
XFUNCTION("print", l2_builtin_print)
XFUNCTION("len", l2_builtin_len)
XFUNCTION("if", l2_builtin_if)
XFUNCTION("loop", l2_builtin_loop)
XFUNCTION("while", l2_builtin_while)
XFUNCTION("for", l2_builtin_for)
#endif
