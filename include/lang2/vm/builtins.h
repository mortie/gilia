#include "vm.h"

#define X(name, f) \
	l2_word f(struct l2_vm *vm, struct l2_vm_array *args);
#include "../builtins.x.h"
#undef X
