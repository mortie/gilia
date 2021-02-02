#ifndef L2_VM_H
#define L2_VM_H

#include <stdlib.h>

#include "../bytecode.h"
#include "../bitset.h"

struct l2_vm_value {
	enum l2_value_flags {
		L2_VAL_TYPE_NONE,
		L2_VAL_TYPE_INTEGER,
		L2_VAL_TYPE_REAL,
		L2_VAL_TYPE_BUFFER,
		L2_VAL_TYPE_ARRAY,
		L2_VAL_TYPE_NAMESPACE,
		L2_VAL_TYPE_FUNCTION,
		L2_VAL_MARKED = 1 << 7,
		L2_VAL_CONST = 1 << 8,
	} flags;
	union {
		int64_t integer;
		double real;
		struct {
			l2_word pos;
			l2_word namespace;
		} func;
		void *data;
	};
};

#define l2_vm_value_type(val) ((val)->flags & 0x0f)

struct l2_vm_buffer {
	size_t len;
	char data[];
};

struct l2_vm_array {
	size_t len;
	size_t size;
	l2_word data[];
};

struct l2_vm_namespace {
	struct l2_vm_value *parent;
	size_t len;
	size_t size;
	l2_word mask;
	l2_word data[];
};

void l2_vm_namespace_set(struct l2_vm_value *ns, l2_word key, l2_word val);
l2_word l2_vm_namespace_get(struct l2_vm_value *ns, l2_word key);

struct l2_vm {
	l2_word *ops;
	size_t opcount;
	l2_word iptr;

	struct l2_vm_value *values;
	size_t valuessize;
	struct l2_bitset valueset;

	l2_word stack[1024];
	l2_word sptr;

	l2_word nstack[1024];
	l2_word nsptr;
};

void l2_vm_init(struct l2_vm *vm, l2_word *ops, size_t opcount);
void l2_vm_free(struct l2_vm *vm);
void l2_vm_step(struct l2_vm *vm);
void l2_vm_run(struct l2_vm *vm);
size_t l2_vm_gc(struct l2_vm *vm);

#endif
