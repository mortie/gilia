#include "vm.h"

#define XFUNCTION(name, f) \
	l2_word f(struct l2_vm *vm, l2_word argc, l2_word *argv);
#include "../builtins.x.h"
#undef XFUNCTION
