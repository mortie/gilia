// X macro: Define a macro named X, then include this file, then undef X.

#ifdef X
X("print", l2_builtin_print);
#endif
