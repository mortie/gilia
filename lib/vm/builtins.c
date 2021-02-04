#include "vm/builtins.h"

#include <stdio.h>

l2_word l2_builtin_print(struct l2_vm *vm, struct l2_vm_array *args) {
	printf("hey this is test\n");
	return 0;
}
