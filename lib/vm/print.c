#include "vm/print.h"

#include <stdio.h>
#include <string.h>
#include <stdint.h>

static gil_word read_uint(unsigned char *ops, size_t *ptr) {
	gil_word word = 0;
	while (ops[*ptr] >= 0x80) {
		word |= ops[(*ptr)++] & 0x7f;
		word <<= 7;
	}

	word |= ops[(*ptr)++];
	return word;
}

static gil_word read_u4le(unsigned char *ops, size_t *ptr) {
	unsigned char *data = &ops[*ptr];
	gil_word integer = 0 |
		(uint64_t)data[0] |
		(uint64_t)data[1] << 8 |
		(uint64_t)data[2] << 16 |
		(uint64_t)data[3] << 24;

	*ptr += 4;
	return integer;
}

static double read_d8le(unsigned char *ops, size_t *ptr) {
	unsigned char *data = &ops[*ptr];
	uint64_t integer = 0 |
		(uint64_t)data[0] |
		(uint64_t)data[1] << 8 |
		(uint64_t)data[2] << 16 |
		(uint64_t)data[3] << 24 |
		(uint64_t)data[4] << 32 |
		(uint64_t)data[5] << 40 |
		(uint64_t)data[6] << 48 |
		(uint64_t)data[7] << 56;

	double num;
	memcpy(&num, &integer, 8);
	*ptr += 8;
	return num;
}

void gil_vm_print_val(struct gil_vm_value *val) {
	switch (gil_value_get_type(val)) {
	case GIL_VAL_TYPE_NONE:
		printf("NONE\n");
		break;

	case GIL_VAL_TYPE_ATOM:
		printf("ATOM %u\n", val->atom);
		break;

	case GIL_VAL_TYPE_REAL:
		printf("REAL %f\n", val->real);
		break;

	case GIL_VAL_TYPE_ARRAY: {
		printf("ARRAY, len %u\n", val->extra.arr_length);
		gil_word *data = val->flags & GIL_VAL_SBO ? val->shortarray : val->array->data;
		for (size_t i = 0; i < val->extra.arr_length; ++i) {
			printf("    %zu: %u\n", i, data[i]);
		}
	}
		break;

	case GIL_VAL_TYPE_BUFFER: {
		if (val->buffer == NULL) {
			printf("BUFFER, empty\n");
			return;
		}

		printf("BUFFER, len %u\n", val->extra.buf_length);
		for (size_t i = 0; i < val->extra.buf_length; ++i) {
			printf("    %zu: %c\n", i, val->buffer[i]);
		}
	}
		break;

	case GIL_VAL_TYPE_NAMESPACE: {
		if (val->ns == NULL) {
			printf("NAMESPACE, empty, parent %u\n", val->extra.ns_parent);
			return;
		}

		printf("NAMESPACE, len %zu, parent %u\n", val->ns->len, val->extra.ns_parent);
		for (size_t i = 0; i < val->ns->size; ++i) {
			gil_word key = val->ns->data[i];
			gil_word v = val->ns->data[val->ns->size + i];
			if (key == 0 || key == ~(gil_word)0) continue;
			printf("    %u: %u\n", key, v);
		}
	}
		break;

	case GIL_VAL_TYPE_FUNCTION:
		printf("FUNCTION, pos %u, ns %u\n", val->func.pos, val->func.ns);
		break;

	case GIL_VAL_TYPE_CFUNCTION:
		// ISO C doesn't let you cast a function pointer to void*.
		printf("C FUNCTION, %8jx\n", (uintmax_t)val->cfunc);
		break;

	case GIL_VAL_TYPE_CONTINUATION:
		printf("CONTINUATION, call %u, cont %08jx\n",
				val->extra.cont_call, (uintmax_t)val->cont);
		break;

	case GIL_VAL_TYPE_RETURN:
		printf("RETURN, ret %u\n", val->ret);
		break;

	case GIL_VAL_TYPE_ERROR:
		printf("ERROR, %s\n", val->error);
		break;
	}
}

void gil_vm_print_state(struct gil_vm *vm) {
	printf("Stack:\n");
	gil_vm_print_stack(vm);
	printf("Heap:\n");
	gil_vm_print_heap(vm);
	printf("Frame Stack:\n");
	gil_vm_print_fstack(vm);
}

void gil_vm_print_heap(struct gil_vm *vm) {
	printf("  Root: ");
	gil_vm_print_val(&vm->values[2]);
	printf("  0-%u: (builtins)\n", vm->gc_start - 1);
	for (gil_word i = vm->gc_start; i < vm->valuessize; ++i) {
		if (gil_bitset_get(&vm->valueset, i)) {
			printf("  %u: ", i);
			gil_vm_print_val(&vm->values[i]);
		}
	}
}

void gil_vm_print_stack(struct gil_vm *vm) {
	for (gil_word i = 0; i < vm->sptr; ++i) {
		printf("  %i: %i\n", i, vm->stack[i]);
	}
}

void gil_vm_print_fstack(struct gil_vm *vm) {
	for (gil_word i = 0; i < vm->fsptr; ++i) {
		printf("  %i: %i, ret %i, stack base %u, args %u\n",
				i, vm->fstack[i].ns, (int)vm->fstack[i].retptr,
				vm->fstack[i].sptr, vm->fstack[i].args);
	}
}

void gil_vm_print_op(unsigned char *ops, size_t opcount, size_t *ptr) {
	enum gil_opcode opcode = (enum gil_opcode)ops[(*ptr)++];

	switch (opcode) {
	case GIL_OP_NOP:
		printf("NOP\n");
		return;

	case GIL_OP_DISCARD:
		printf("DISCARD\n");
		return;

	case GIL_OP_SWAP_DISCARD:
		printf("SWAP_DISCARD\n");
		return;

	case GIL_OP_FUNC_CALL:
		printf("FUNC_CALL %u\n", read_uint(ops, ptr));
		return;

	case GIL_OP_FUNC_CALL_INFIX:
		printf("FUNC_CALL_INFIX\n");
		return;

	case GIL_OP_RJMP:
		printf("RJMP %u\n", read_uint(ops, ptr));
		return;

	case GIL_OP_RJMP_U4:
		printf("RJMP %u\n", read_u4le(ops, ptr));
		return;

	case GIL_OP_STACK_FRAME_GET_ARGS:
		printf("STACK_FRAME_GET_ARGS\n");
		return;

	case GIL_OP_STACK_FRAME_LOOKUP:
		printf("STACK_FRAME_LOOKUP %u\n", read_uint(ops, ptr));
		return;

	case GIL_OP_STACK_FRAME_SET:
		printf("STACK_FRAME_SET %u\n", read_uint(ops, ptr));
		return;

	case GIL_OP_STACK_FRAME_REPLACE:
		printf("STACK_FRAME_REPLACE %u\n", read_uint(ops, ptr));
		return;

	case GIL_OP_RET:
		printf("RET\n");
		return;

	case GIL_OP_ALLOC_NONE:
		printf("ALLOC_NONE\n");
		return;

	case GIL_OP_ALLOC_ATOM:
		printf("ALLOC_ATOM %u\n", read_uint(ops, ptr));
		return;

	case GIL_OP_ALLOC_REAL:
		printf("ALLOC_REAL %f\n", read_d8le(ops, ptr));
		return;

	case GIL_OP_ALLOC_BUFFER_STATIC: {
		gil_word w1 = read_uint(ops, ptr);
		gil_word w2 = read_uint(ops, ptr);;
		printf("ALLOC_BUFFER_STATIC %u %u\n", w1, w2);
	}
		return;

	case GIL_OP_ALLOC_ARRAY:
		printf("ALLOC_ARRAY %u\n", read_uint(ops, ptr));
		return;

	case GIL_OP_ALLOC_NAMESPACE:
		printf("ALLOC_NAMESPACE\n");
		return;

	case GIL_OP_ALLOC_FUNCTION:
		printf("ALLOC_FUNCTION %u\n", read_uint(ops, ptr));
		return;

	case GIL_OP_NAMESPACE_SET:
		printf("NAMESPACE_SET %u\n", read_uint(ops, ptr));
		return;

	case GIL_OP_NAMESPACE_LOOKUP:
		printf("NAMESPACE_LOOKUP %u\n", read_uint(ops, ptr));
		return;

	case GIL_OP_ARRAY_LOOKUP:
		printf("ARRAY_LOOKUP %u\n", read_uint(ops, ptr));
		return;

	case GIL_OP_ARRAY_SET:
		printf("ARRAY_SET %u\n", read_uint(ops, ptr));
		return;

	case GIL_OP_DYNAMIC_LOOKUP:
		printf("DYNAMIC_LOOKUP\n");
		return;

	case GIL_OP_DYNAMIC_SET:
		printf("DYNAMIC_SET\n");
		return;

	case GIL_OP_HALT:
		printf("HALT\n");
		return;
	}

	printf("? 0x%02x\n", opcode);
}

void gil_vm_print_bytecode(unsigned char *ops, size_t opcount) {
	size_t ptr = 0;
	while (ptr < opcount) {
		printf("%04zu ", ptr);
		gil_vm_print_op(ops, opcount, &ptr);
	}
}
