#include "vm/vm.h"

#include <string.h>

#include <stdio.h>

static l2_word alloc_val(struct l2_vm *vm) {
	size_t id = l2_bitset_set_next(&vm->valueset);
	if (id >= vm->valuessize) {
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

static float u32_to_float(uint32_t num) {
	float f;
	memcpy(&f, &num, sizeof(num));
	return f;
}

static double u32s_to_double(uint32_t high, uint32_t low) {
	double d;
	uint64_t num = (uint64_t)high << 32 | (uint64_t)low;
	memcpy(&d, &num, sizeof(num));
	return d;
}

static void gc_mark(struct l2_vm *vm, l2_word id) {
	printf("GC MARK %i\n", id);
	struct l2_vm_value *val = &vm->values[id];
	val->flags |= L2_VAL_MARKED;

	int typ = val->flags & 0x0f;
	if (typ == L2_VAL_TYPE_ARRAY) {
		struct l2_vm_array *arr = (struct l2_vm_array *)val->data;
		l2_word *ids = (l2_word *)((char *)arr + sizeof(struct l2_vm_array));
		for (size_t i = 0; i < arr->len; ++i) {
			gc_mark(vm, ids[i]);
		}
	}
}

static void gc_free(struct l2_vm *vm, l2_word id) {
	printf("GC FREE %i\n", id);
	struct l2_vm_value *val = &vm->values[id];
	l2_bitset_unset(&vm->valueset, id);

	int typ = val->flags & 0x0f;
	if (typ == L2_VAL_TYPE_ARRAY || typ == L2_VAL_TYPE_BUFFER) {
		free(val->data);
		// Don't need to do anything more; the next round of GC will free
		// whichever values were only referenced by the array
	}
}

static size_t gc_sweep(struct l2_vm *vm) {
	size_t freed = 0;
	// Skip ID 0, because that should always exist
	for (size_t i = 1; i < vm->valuessize; ++i) {
		if (!l2_bitset_get(&vm->valueset, i)) {
			continue;
		}

		struct l2_vm_value *val = &vm->values[i];
		if (!(val->flags & L2_VAL_MARKED)) {
			gc_free(vm, i);
			freed += 1;
		} else {
			val->flags &= ~L2_VAL_MARKED;
		}
	}
	return freed;
}

void l2_vm_init(struct l2_vm *vm, l2_word *ops, size_t opcount) {
	vm->ops = ops;
	vm->opcount = opcount;
	vm->iptr = 0;
	vm->sptr = 0;

	vm->values = NULL;
	vm->valuessize = 0;
	l2_bitset_init(&vm->valueset);

	// It's wasteful to allocate new 'none' variables all the time,
	// variable ID 0 should be the only 'none' variable in the system
	l2_word none_id = alloc_val(vm);
	vm->values[none_id].flags = L2_VAL_TYPE_NONE | L2_VAL_CONST;
}

size_t l2_vm_gc(struct l2_vm *vm) {
	for (l2_word sptr = 0; sptr < vm->sptr; ++sptr) {
		if (vm->stackflags[sptr]) {
			gc_mark(vm, vm->stack[sptr]);
		}
	}

	return gc_sweep(vm);
}

void l2_vm_run(struct l2_vm *vm) {
	while ((enum l2_opcode)vm->ops[vm->iptr] != L2_OP_HALT) {
		l2_vm_step(vm);
	}
}

void l2_vm_step(struct l2_vm *vm) {
	enum l2_opcode opcode = (enum l2_opcode)vm->ops[vm->iptr++];

	l2_word word;
	switch (opcode) {
	case L2_OP_NOP:
		break;

	case L2_OP_PUSH:
		vm->stack[vm->sptr] = vm->ops[vm->iptr];
		vm->stackflags[vm->sptr] = 0;
		vm->sptr += 1;
		vm->iptr += 1;
		break;

	case L2_OP_POP:
		vm->sptr -= 1;
		break;

	case L2_OP_DUP:
		vm->stack[vm->sptr] = vm->ops[vm->sptr - 1];
		vm->stackflags[vm->sptr] = 0;
		vm->sptr += 1;
		break;

	case L2_OP_ADD:
		vm->stack[vm->sptr - 2] += vm->stack[vm->sptr - 1];
		vm->stackflags[vm->sptr - 2] = 0;
		vm->sptr -= 1;
		break;

	case L2_OP_JUMP:
		vm->iptr = vm->stack[vm->sptr - 1];
		vm->stackflags[vm->sptr - 1] = 0;
		vm->sptr -= 1;
		break;

	case L2_OP_CALL:
		word = vm->stack[vm->sptr - 1];
		vm->stack[vm->sptr - 1] = vm->iptr + 1;
		vm->stackflags[vm->sptr - 1] = 0;
		vm->iptr = word;
		break;

	case L2_OP_RET:
		vm->iptr = vm->stack[vm->sptr - 1];
		vm->sptr -= 1;
		break;

	case L2_OP_ALLOC_INTEGER_32:
		word = alloc_val(vm);
		vm->values[word].flags = L2_VAL_TYPE_INTEGER;
		vm->values[word].integer = vm->stack[--vm->sptr];
		vm->stack[vm->sptr] = word;
		vm->stackflags[vm->sptr] = 1;
		vm->sptr += 1;
		break;

	case L2_OP_ALLOC_INTEGER_64:
		word = alloc_val(vm);
		vm->values[word].flags = L2_VAL_TYPE_INTEGER;
		vm->values[word].integer = (int64_t)(
			(uint64_t)vm->stack[vm->sptr - 1] << 32 |
			(uint64_t)vm->stack[vm->sptr - 2]);
		vm->sptr -= 2;
		vm->stack[vm->sptr] = word;
		vm->stackflags[vm->sptr] = 1;
		vm->sptr += 1;
		break;

	case L2_OP_ALLOC_REAL_32:
		word = alloc_val(vm);
		vm->values[word].flags = L2_VAL_TYPE_REAL;
		vm->values[word].real = u32_to_float(vm->stack[--vm->sptr]);
		vm->stack[vm->sptr] = word;
		vm->stackflags[vm->sptr] = 1;
		vm->sptr += 1;
		break;

	case L2_OP_ALLOC_REAL_64:
		word = alloc_val(vm);
		vm->values[word].flags = L2_VAL_TYPE_REAL;
		vm->values[word].real = u32s_to_double(vm->stack[vm->sptr - 1], vm->stack[vm->sptr - 2]);
		vm->sptr -= 2;
		vm->stack[vm->sptr] = word;
		vm->stackflags[vm->sptr] = 1;
		vm->sptr += 1;
		break;

	case L2_OP_ALLOC_ARRAY:
		word = alloc_val(vm);
		vm->values[word].flags = L2_VAL_TYPE_ARRAY;
		vm->values[word].data = calloc(1, sizeof(struct l2_vm_array));
		vm->stack[vm->sptr] = word;
		vm->stackflags[vm->sptr] = 1;
		vm->sptr += 1;
		break;

	case L2_OP_ALLOC_BUFFER:
		word = alloc_val(vm);
		vm->values[word].flags = L2_VAL_TYPE_BUFFER;
		vm->values[word].data = calloc(1, sizeof(struct l2_vm_buffer));
		vm->stack[vm->sptr] = word;
		vm->stackflags[vm->sptr] = 1;
		vm->sptr += 1;
		break;

	case L2_OP_ALLOC_BUFFER_CONST:
		{
			word = alloc_val(vm);
			l2_word length = vm->stack[--vm->sptr];
			l2_word offset = vm->stack[--vm->sptr];
			vm->values[word].flags = L2_VAL_TYPE_BUFFER;
			vm->values[word].data = malloc(sizeof(struct l2_vm_buffer) + length);
			((struct l2_vm_buffer *)vm->values[word].data)->len = length;
			memcpy(
				(unsigned char *)vm->values[word].data + sizeof(struct l2_vm_buffer),
				vm->ops + offset, length);
			vm->stack[vm->sptr] = word;
			vm->stackflags[vm->sptr] = 1;
			vm->sptr += 1;
		}
		break;

	case L2_OP_HALT:
		break;
	}
}
