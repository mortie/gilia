// X macro: Define a macro named X, then include this file, then undef X.

#ifdef Y
Y("none", knone)
Y("true", ktrue)
Y("false", kfalse)
#endif

#ifdef X
X("+", l2_builtin_add)
X("-", l2_builtin_sub)
X("*", l2_builtin_mul)
X("/", l2_builtin_div)
X("==", l2_builtin_eq)
X("!=", l2_builtin_neq)
X("<", l2_builtin_lt)
X("<=", l2_builtin_lteq)
X(">", l2_builtin_gt)
X(">=", l2_builtin_gteq)
X("print", l2_builtin_print)
X("len", l2_builtin_len)
X("if", l2_builtin_if)
X("loop", l2_builtin_loop)
#endif
