#ifndef L2_VM_H
#define L2_VM_H

#include <stdlib.h>

#include "../bytecode.h"
#include "../bitset.h"

struct l2_vm;
struct l2_vm_array;
typedef l2_word (*l2_vm_cfunction)(struct l2_vm *vm, struct l2_vm_array *args);

enum l2_value_type {
	L2_VAL_TYPE_NONE,
	L2_VAL_TYPE_ATOM,
	L2_VAL_TYPE_REAL,
	L2_VAL_TYPE_BUFFER,
	L2_VAL_TYPE_ARRAY,
	L2_VAL_TYPE_NAMESPACE,
	L2_VAL_TYPE_FUNCTION,
	L2_VAL_TYPE_CFUNCTION,
};

enum l2_value_flags {
	L2_VAL_MARKED = 1 << 6,
	L2_VAL_CONST = 1 << 7,
};

// The smallest size an l2_vm_value can be is 16 bytes on common platforms.
// Pointers are 8 bytes, and since we need to store _at least_ 1 pointer +
// 1 byte for flags, it's going to be padded up to 16 bytes anyways.
// Might as well store some useful extra info in here.
struct l2_vm_value {
	// Byte 0: 4 bytes
	union {
		l2_word ns_parent;
	} extra;

	// Byte 4: 1 byte, 3 bytes padding
	uint8_t flags;

	// Byte 8: 8 bytes
	union {
		l2_word atom;
		double real;
		struct l2_vm_buffer *buffer;
		struct l2_vm_array *array;
		struct l2_vm_namespace *ns;
		struct {
			l2_word pos;
			l2_word namespace;
		} func;
		l2_vm_cfunction cfunc;
	};
};

#define l2_vm_value_type(val) ((enum l2_value_type)((val)->flags & 0x0f))

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
	size_t len;
	size_t size;
	l2_word mask;
	l2_word data[];
};

void l2_vm_namespace_set(struct l2_vm_value *ns, l2_word key, l2_word val);
l2_word l2_vm_namespace_get(struct l2_vm *vm, struct l2_vm_value *ns, l2_word key);

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
l2_word l2_vm_alloc(struct l2_vm *vm, enum l2_value_type typ, enum l2_value_flags flags);
void l2_vm_free(struct l2_vm *vm);
void l2_vm_step(struct l2_vm *vm);
void l2_vm_run(struct l2_vm *vm);
size_t l2_vm_gc(struct l2_vm *vm);

#endif
