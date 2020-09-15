#ifndef L2_VM_H
#define L2_VM_H

#include <stdlib.h>

#include "../bytecode.h"

enum {
	L2_VAL_TYPE_STRING = (1 << 30) + 0,
	L2_VAL_TYPE_ARRAY  = (1 << 30) + 1,
	L2_VAL_MARKED = 1 << 29,
};
struct l2_vm_value {
	l2_word flags;
};

struct l2_vm_string {
	struct l2_vm_value val;
	char *mem;
	size_t len;
};

struct l2_vm_array {
	struct l2_vm_value val;
	l2_word *data;
	size_t len;
	size_t size;
};

struct l2_vm {
	struct l2_op *ops;
	size_t opcount;

	struct l2_vm_value *allocs[1024];
	size_t allocslen;

	l2_word stack[1024];
	l2_word iptr;
	l2_word sptr;
};

void l2_vm_init(struct l2_vm *vm, struct l2_op *ops, size_t opcount);
void l2_vm_step(struct l2_vm *vm);

#endif
