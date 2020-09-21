#ifndef L2_VM_H
#define L2_VM_H

#include <stdlib.h>

#include "bytecode.h"
#include "bitset.h"

struct l2_vm_value {
	enum l2_value_flags {
		L2_VAL_TYPE_INTEGER,
		L2_VAL_TYPE_REAL,
		L2_VAL_TYPE_STRING,
		L2_VAL_TYPE_ARRAY,
		L2_VAL_MARKED = 1 << 7,
	} flags;
	union {
		int64_t integer;
		double real;
		void *data;
	};
};

struct l2_vm_string {
	struct l2_vm_value val;
	size_t len;
};

struct l2_vm_array {
	struct l2_vm_value val;
	size_t len;
	size_t size;
};

struct l2_vm {
	struct l2_op *ops;
	size_t opcount;

	struct l2_vm_value *values;
	size_t valuessize;
	struct l2_bitset valueset;

	l2_word stack[1024];
	l2_word iptr;
	l2_word sptr;
};

void l2_vm_init(struct l2_vm *vm, struct l2_op *ops, size_t opcount);
void l2_vm_step(struct l2_vm *vm);

#endif
