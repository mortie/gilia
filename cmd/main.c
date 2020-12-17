#include "vm/vm.h"
#include "vm/print.h"
#include "bitset.h"

#include <stdio.h>
#include <string.h>

int main() {
	l2_word ops[] = {
		L2_OP_PUSH, 100,
		L2_OP_PUSH, 100,
		L2_OP_ADD,
		L2_OP_ALLOC_INTEGER_32,
		L2_OP_PUSH, 21 /* offset */,
		L2_OP_PUSH, 5  /* length */,
		L2_OP_ALLOC_BUFFER_STATIC,
		L2_OP_POP,
		L2_OP_PUSH, 16,
		L2_OP_CALL,
		L2_OP_HALT,
		L2_OP_PUSH, 53,
		L2_OP_ALLOC_INTEGER_32,
		L2_OP_ALLOC_NAMESPACE,
		L2_OP_HALT,
		0, 0,
	};
	memcpy(&ops[21], "Hello", 5);

	struct l2_vm vm;
	l2_vm_init(&vm, ops, sizeof(ops) / sizeof(*ops));

	l2_vm_run(&vm);

	l2_vm_print_state(&vm);

	l2_vm_gc(&vm);

	printf("Heap:\n");
	l2_vm_print_stack(&vm);

	l2_vm_free(&vm);
}
