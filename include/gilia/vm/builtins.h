#include "vm.h"

#define XFUNCTION(name, f) \
	gil_word f(struct gil_vm *vm, gil_word mid, gil_word argc, gil_word *argv);
#include "../builtins.x.h"
#undef XFUNCTION
