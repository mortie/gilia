#include "vm/print.h"

#include <stdio.h>
#include <string.h>

void l2_vm_print_val(struct l2_vm_value *val) {
	switch (val->flags & 0x0f) {
	case L2_VAL_TYPE_NONE:
		printf("NONE\n");
		break;

	case L2_VAL_TYPE_INTEGER:
		printf("INTEGER %zi\n", val->integer);
		break;

	case L2_VAL_TYPE_REAL:
		printf("REAL %f\n", val->real);
		break;

	case L2_VAL_TYPE_ARRAY:
		{
			if (val->data == NULL) {
				printf("ARRAY, empty\n");
				return;
			}

			struct l2_vm_array *arr = (struct l2_vm_array *)val->data;
			printf("ARRAY, len %zu\n", arr->len);
			for (size_t i = 0; i < arr->len; ++i) {
				printf("    %zu: %u\n", i, arr->data[i]);
			}
		}
		break;

	case L2_VAL_TYPE_BUFFER:
		{
			if (val->data == NULL) {
				printf("BUFFER, empty\n");
				return;
			}

			struct l2_vm_buffer *buf = (struct l2_vm_buffer *)val->data;
			printf("BUFFER, len %zu\n", buf->len);
			for (size_t i = 0; i < buf->len; ++i) {
				printf("    %zu: %c\n", i, buf->data[i]);
			}
		}
		break;

	case L2_VAL_TYPE_NAMESPACE:
		{
			if (val->data == NULL) {
				printf("NAMESPACE, empty\n");
				return;
			}

			struct l2_vm_namespace *ns = (struct l2_vm_namespace *)val->data;
			printf("NAMESPACE, len %zu\n", ns->len);
		}
		break;

	case L2_VAL_TYPE_FUNCTION:
		printf("FUNCTION, pos %u, ns %u\n", val->func.pos, val->func.namespace);
		break;
	}
}

void l2_vm_print_state(struct l2_vm *vm) {
	printf("Stack:\n");
	l2_vm_print_stack(vm);
	printf("Heap:\n");
	l2_vm_print_heap(vm);
	printf("NStack:\n");
	l2_vm_print_nstack(vm);
}

void l2_vm_print_heap(struct l2_vm *vm) {
	for (l2_word i = 0; i < vm->valuessize; ++i) {
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

void l2_vm_print_nstack(struct l2_vm *vm) {
	for (l2_word i = 0; i < vm->nsptr; ++i) {
		printf("  %i: %i\n", i, vm->nstack[i]);
	}
}

static void print_op_num(l2_word *ops, size_t opcount, size_t ptr) {
	if (ptr >= opcount) {
		printf("<EOF>");
	} else {
		printf("%i", ops[ptr]);
	}
}

void l2_vm_print_op(l2_word *ops, size_t opcount, size_t *ptr) {
	enum l2_opcode opcode = (enum l2_opcode)ops[(*ptr)++];

	switch (opcode) {
	case L2_OP_NOP:
		printf("NOP\n");
		break;

	case L2_OP_PUSH:
		printf("PUSH ");
		print_op_num(ops, opcount, (*ptr)++);
		printf("\n");
		break;

	case L2_OP_PUSH_2:
		printf("PUSH2 ");
		print_op_num(ops, opcount, (*ptr)++);
		printf(" ");
		print_op_num(ops, opcount, (*ptr)++);
		printf("\n");
		break;

	case L2_OP_POP:
		printf("POP\n");
		break;

	case L2_OP_DUP:
		printf("DUP\n");
		break;

	case L2_OP_ADD:
		printf("ADD\n");
		break;

	case L2_OP_CALL:
		printf("CALL\n");
		break;

	case L2_OP_RJMP:
		printf("RJMP\n");
		break;

	case L2_OP_GEN_STACK_FRAME:
		printf("GEN_STACK_FRAME\n");
		break;

	case L2_OP_STACK_FRAME_LOOKUP:
		printf("STACK_FRAME_LOOKUP\n");
		break;

	case L2_OP_STACK_FRAME_SET:
		printf("STACK_FRAME_SET\n");
		break;

	case L2_OP_RET:
		printf("RET\n");
		break;

	case L2_OP_ALLOC_INTEGER_32:
		printf("ALLOC_INTEGER_32\n");
		break;

	case L2_OP_ALLOC_INTEGER_64:
		printf("ALLOC_INTEGER_64\n");
		break;

	case L2_OP_ALLOC_REAL_32:
		printf("ALLOC_REAL_32\n");
		break;

	case L2_OP_ALLOC_REAL_64:
		printf("ALLOC_REAL_64\n");
		break;

	case L2_OP_ALLOC_BUFFER_STATIC:
		printf("ALLOC_BUFFER_STATIC\n");
		break;

	case L2_OP_ALLOC_BUFFER_ZERO:
		printf("ALLOC_BUFFER_ZERO\n");
		break;

	case L2_OP_ALLOC_ARRAY:
		printf("ALLOC_ARRAY\n");
		break;

	case L2_OP_ALLOC_NAMESPACE:
		printf("ALLOC_NAMESPACE\n");
		break;

	case L2_OP_ALLOC_FUNCTION:
		printf("ALLOC_FUNCTION\n");
		break;

	case L2_OP_NAMESPACE_SET:
		printf("NAMESPACE_SET\n");
		break;

	case L2_OP_HALT:
		printf("HALT\n");
		break;

	default:
		{
			l2_word word = (l2_word)opcode;
			char bytes[sizeof(word)];
			memcpy(&bytes, &word, sizeof(word));
			printf("?");
			for (size_t i = 0; i < sizeof(bytes); ++i) {
				printf(" %02x", bytes[i]);
			}
			printf("\n");
		}
	}
}

void l2_vm_print_bytecode(l2_word *ops, size_t opcount) {
	size_t ptr = 0;
	while (ptr < opcount) {
		l2_vm_print_op(ops, opcount, &ptr);
	}
}
