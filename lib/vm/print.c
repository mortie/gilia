#include "vm/print.h"

#include <stdio.h>
#include <string.h>
#include <stdint.h>

static l2_word read_u4le(unsigned char *ops, size_t *ptr) {
	unsigned char *data = &ops[*ptr];
	l2_word ret =
		(l2_word)data[0] |
		(l2_word)data[1] << 8 |
		(l2_word)data[2] << 16 |
		(l2_word)data[3] << 24;
	*ptr += 4;
	return ret;
}

static l2_word read_u1le(unsigned char *ops, size_t *ptr) {
	return ops[(*ptr)++];
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

void l2_vm_print_val(struct l2_vm_value *val) {
	switch (l2_value_get_type(val)) {
	case L2_VAL_TYPE_NONE:
		printf("NONE\n");
		break;

	case L2_VAL_TYPE_ATOM:
		printf("ATOM %u\n", val->atom);
		break;

	case L2_VAL_TYPE_REAL:
		printf("REAL %f\n", val->real);
		break;

	case L2_VAL_TYPE_ARRAY:
		{
			printf("ARRAY, len %u\n", val->extra.arr_length);
			l2_word *data = val->flags & L2_VAL_SBO ? val->shortarray : val->array->data;
				for (size_t i = 0; i < val->extra.arr_length; ++i) {
					printf("    %zu: %u\n", i, data[i]);
				}
		}
		break;

	case L2_VAL_TYPE_BUFFER:
		{
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

	case L2_VAL_TYPE_NAMESPACE:
		{
			if (val->ns == NULL) {
				printf("NAMESPACE, empty, parent %u\n", val->extra.ns_parent);
				return;
			}

			printf("NAMESPACE, len %zu, parent %u\n", val->ns->len, val->extra.ns_parent);
			for (size_t i = 0; i < val->ns->size; ++i) {
				l2_word key = val->ns->data[i];
				l2_word v = val->ns->data[val->ns->size + i];
				if (key == 0 || key == ~(l2_word)0) continue;
				printf("    %u: %u\n", key, v);
			}
		}
		break;

	case L2_VAL_TYPE_FUNCTION:
		printf("FUNCTION, pos %u, ns %u\n", val->func.pos, val->func.ns);
		break;

	case L2_VAL_TYPE_CFUNCTION:
		// ISO C doesn't let you cast a function pointer to void*.
		printf("C FUNCTION, %8jx\n", (uintmax_t)val->cfunc);
		break;

	case L2_VAL_TYPE_ERROR:
		printf("ERROR, %s\n", val->error);
		break;

	case L2_VAL_TYPE_CONTINUATION:
		printf("CONTINUATION, call %u, cont %08jx\n",
				val->extra.cont_call, (uintmax_t)val->cont);
		break;
	}
}

void l2_vm_print_state(struct l2_vm *vm) {
	printf("Stack:\n");
	l2_vm_print_stack(vm);
	printf("Heap:\n");
	l2_vm_print_heap(vm);
	printf("Frame Stack:\n");
	l2_vm_print_fstack(vm);
}

void l2_vm_print_heap(struct l2_vm *vm) {
	printf("  Root: ");
	l2_vm_print_val(&vm->values[2]);
	printf("  0-%u: (builtins)\n", vm->gc_start - 1);
	for (l2_word i = vm->gc_start; i < vm->valuessize; ++i) {
		if (l2_bitset_get(&vm->valueset, i)) {
			printf("  %u: ", i);
			l2_vm_print_val(&vm->values[i]);
		}
	}
}

void l2_vm_print_stack(struct l2_vm *vm) {
	for (l2_word i = 0; i < vm->sptr; ++i) {
		printf("  %i: %i\n", i, vm->stack[i]);
	}
}

void l2_vm_print_fstack(struct l2_vm *vm) {
	for (l2_word i = 0; i < vm->fsptr; ++i) {
		printf("  %i: %i, ret %i, stack base %i\n",
				i, vm->fstack[i].ns, vm->fstack[i].retptr, vm->fstack[i].sptr);
	}
}

void l2_vm_print_op(unsigned char *ops, size_t opcount, size_t *ptr) {
	enum l2_opcode opcode = (enum l2_opcode)ops[(*ptr)++];

	switch (opcode) {
	case L2_OP_NOP:
		printf("NOP\n");
		return;

	case L2_OP_DISCARD:
		printf("DISCARD\n");
		return;

	case L2_OP_SWAP_DISCARD:
		printf("SWAP_DISCARD\n");
		return;

	case L2_OP_DUP:
		printf("DUP\n");
		return;

	case L2_OP_ADD:
		printf("ADD\n");
		return;

	case L2_OP_FUNC_CALL_U4:
		printf("FUNC_CALL %u\n", read_u4le(ops, ptr));
		return;
	case L2_OP_FUNC_CALL_U1:
		printf("FUNC_CALL %u\n", read_u1le(ops, ptr));
		return;

	case L2_OP_FUNC_CALL_INFIX:
		printf("FUNC_CALL_INFIX\n");
		return;

	case L2_OP_RJMP_U4:
		printf("RJMP %u\n", read_u4le(ops, ptr));
		return;
	case L2_OP_RJMP_U1:
		printf("RJMP %u\n", read_u1le(ops, ptr));
		return;

	case L2_OP_STACK_FRAME_LOOKUP_U4:
		printf("STACK_FRAME_LOOKUP %u\n", read_u4le(ops, ptr));
		return;
	case L2_OP_STACK_FRAME_LOOKUP_U1:
		printf("STACK_FRAME_LOOKUP %u\n", read_u1le(ops, ptr));
		return;

	case L2_OP_STACK_FRAME_SET_U4:
		printf("STACK_FRAME_SET %u\n", read_u4le(ops, ptr));
		return;
	case L2_OP_STACK_FRAME_SET_U1:
		printf("STACK_FRAME_SET %u\n", read_u1le(ops, ptr));
		return;

	case L2_OP_STACK_FRAME_REPLACE_U4:
		printf("STACK_FRAME_REPLACE %u\n", read_u4le(ops, ptr));
		return;
	case L2_OP_STACK_FRAME_REPLACE_U1:
		printf("STACK_FRAME_REPLACE %u\n", read_u1le(ops, ptr));
		return;

	case L2_OP_RET:
		printf("RET\n");
		return;

	case L2_OP_ALLOC_NONE:
		printf("ALLOC_NONE\n");
		return;

	case L2_OP_ALLOC_ATOM_U4:
		printf("ALLOC_ATOM %u\n", read_u4le(ops, ptr));
		return;
	case L2_OP_ALLOC_ATOM_U1:
		printf("ALLOC_ATOM %u\n", read_u1le(ops, ptr));
		return;

	case L2_OP_ALLOC_REAL_D8:
		printf("ALLOC_REAL %f\n", read_d8le(ops, ptr));
		return;

	case L2_OP_ALLOC_BUFFER_STATIC_U4:
		{
			l2_word w1 = read_u4le(ops, ptr);
			l2_word w2 = read_u4le(ops, ptr);;
			printf("ALLOC_BUFFER_STATIC %u %u\n", w1, w2);
		}
		return;
	case L2_OP_ALLOC_BUFFER_STATIC_U1:
		{
			l2_word w1 = read_u1le(ops, ptr);
			l2_word w2 = read_u1le(ops, ptr);;
			printf("ALLOC_BUFFER_STATIC %u %u\n", w1, w2);
		}
		return;

	case L2_OP_ALLOC_ARRAY_U4:
		printf("ALLOC_ARRAY %u\n", read_u4le(ops, ptr));
		return;
	case L2_OP_ALLOC_ARRAY_U1:
		printf("ALLOC_ARRAY %u\n", read_u1le(ops, ptr));
		return;

	case L2_OP_ALLOC_NAMESPACE:
		printf("ALLOC_NAMESPACE\n");
		return;

	case L2_OP_ALLOC_FUNCTION_U4:
		printf("ALLOC_FUNCTION %u\n", read_u4le(ops, ptr));
		return;
	case L2_OP_ALLOC_FUNCTION_U1:
		printf("ALLOC_FUNCTION %u\n", read_u1le(ops, ptr));
		return;

	case L2_OP_NAMESPACE_SET_U4:
		printf("NAMESPACE_SET %u\n", read_u4le(ops, ptr));
		return;
	case L2_OP_NAMESPACE_SET_U1:
		printf("NAMESPACE_SET %u\n", read_u1le(ops, ptr));
		return;

	case L2_OP_NAMESPACE_LOOKUP_U4:
		printf("NAMESPACE_LOOKUP %u\n", read_u4le(ops, ptr));
		return;
	case L2_OP_NAMESPACE_LOOKUP_U1:
		printf("NAMESPACE_LOOKUP %u\n", read_u1le(ops, ptr));
		return;

	case L2_OP_ARRAY_LOOKUP_U4:
		printf("ARRAY_LOOKUP %u\n", read_u4le(ops, ptr));
		return;
	case L2_OP_ARRAY_LOOKUP_U1:
		printf("ARRAY_LOOKUP %u\n", read_u1le(ops, ptr));
		return;

	case L2_OP_ARRAY_SET_U4:
		printf("ARRAY_SET %u\n", read_u4le(ops, ptr));
		return;
	case L2_OP_ARRAY_SET_U1:
		printf("ARRAY_SET %u\n", read_u1le(ops, ptr));
		return;

	case L2_OP_DYNAMIC_LOOKUP:
		printf("DYNAMIC_LOOKUP\n");
		return;

	case L2_OP_DYNAMIC_SET:
		printf("DYNAMIC_SET\n");
		return;

	case L2_OP_HALT:
		printf("HALT\n");
		return;
	}

	printf("? 0x%02x\n", opcode);
}

void l2_vm_print_bytecode(unsigned char *ops, size_t opcount) {
	size_t ptr = 0;
	while (ptr < opcount) {
		printf("%04zu ", ptr);
		l2_vm_print_op(ops, opcount, &ptr);
	}
}
