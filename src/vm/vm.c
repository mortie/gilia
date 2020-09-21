#include "vm/vm.h"

void l2_vm_init(struct l2_vm *vm, struct l2_op *ops, size_t opcount) {
	vm->ops = ops;
	vm->opcount = opcount;
	vm->iptr = 0;
	vm->sptr = 0;

	vm->values = NULL;
	vm->valuessize = 0;
	l2_bitset_init(&vm->valueset);
}

static l2_word alloc_val(struct l2_vm *vm) {
	size_t id = l2_bitset_set_next(&vm->valueset);
	if (id > vm->valuessize) {
		if (vm->valuessize == 0) {
			vm->valuessize = 16;
		}

		while (id > vm->valuessize) {
			vm->valuessize *= 2;
		}

		vm->values = realloc(vm->values, sizeof(*vm->values) * vm->valuessize);
	}

	return (l2_word)id;
}

void l2_vm_step(struct l2_vm *vm) {
	struct l2_op op = vm->ops[vm->iptr++];

	l2_word word;
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

	case L2_OP_CALL:
		word = vm->stack[--vm->sptr];
		vm->stack[vm->sptr++] = vm->iptr + 1;
		vm->iptr = word;
		break;

	case L2_OP_RET:
		vm->iptr = vm->stack[--vm->sptr];
		break;

	case L2_OP_ALLOC_INTEGER:
		word = alloc_val(vm);
		vm->values[word].flags = L2_VAL_TYPE_INTEGER;
		vm->values[word].integer = 0;
		vm->stack[vm->sptr++] = word;
		break;

	case L2_OP_ALLOC_REAL:
		word = alloc_val(vm);
		vm->values[word].flags = L2_VAL_TYPE_REAL;
		vm->values[word].real = 0;
		vm->stack[vm->sptr++] = word;
		break;

	case L2_OP_ALLOC_STRING:
		word = alloc_val(vm);
		vm->values[word].flags = L2_VAL_TYPE_STRING;
		vm->values[word].data = calloc(1, sizeof(struct l2_vm_string));
		vm->stack[vm->sptr++] = word;
		break;

	case L2_OP_ALLOC_ARRAY:
		word = alloc_val(vm);
		vm->values[word].flags = L2_VAL_TYPE_ARRAY;
		vm->values[word].data = calloc(1, sizeof(struct l2_vm_array));
		vm->stack[vm->sptr++] = word;
		break;

	case L2_OP_HALT:
		break;
	}
}
