// X macro: Define a macro named X, then include this file, then undef X.

#ifdef X
X("+", l2_builtin_add);
X("-", l2_builtin_sub);
X("*", l2_builtin_mul);
X("/", l2_builtin_div);
X("print", l2_builtin_print);
X("len", l2_builtin_len);
#endif
