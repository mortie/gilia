#include "vm/vm.h"

void l2_vm_init(struct l2_vm *vm, struct l2_op *ops, size_t opcount) {
	vm->ops = ops;
	vm->opcount = opcount;
	vm->iptr = 0;
	vm->sptr = 0;
}

static l2_word alloc(struct l2_vm *vm, size_t size) {
	l2_word id = vm->allocslen++;
	vm->allocs[id] = malloc(size);
	return id | 1 << 31;
}

void l2_vm_step(struct l2_vm *vm) {
	struct l2_op op = vm->ops[vm->iptr++];

	switch (op.code) {
	case L2_OP_PUSH:
		vm->stack[vm->sptr++] = op.val;
		break;

	case L2_OP_ADD:
		vm->sptr -= 1;
		vm->stack[vm->sptr - 1] += vm->stack[vm->sptr];
		break;

	case L2_OP_JUMP:
		vm->iptr = vm->stack[--vm->sptr];
		break;

	case L2_OP_HALT:
		break;
	}
}
