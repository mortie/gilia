#include "vm/vm.h"
#include "bitset.h"

#include <stdio.h>
#include <string.h>

void print_var(struct l2_vm_value *val) {
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
	}
}

int main() {
	l2_word ops[] = {
		L2_OP_PUSH, 100,
		L2_OP_PUSH, 100,
		L2_OP_ADD,
		L2_OP_ALLOC_INTEGER_32,
		L2_OP_PUSH, 21 /* offset */,
		L2_OP_PUSH, 5  /* length */,
		L2_OP_ALLOC_BUFFER_STATIC,
		L2_OP_POP,
		L2_OP_PUSH, 16,
		L2_OP_CALL,
		L2_OP_HALT,
		L2_OP_PUSH, 53,
		L2_OP_ALLOC_INTEGER_32,
		L2_OP_ALLOC_NAMESPACE,
		L2_OP_HALT,
		0, 0,
	};
	memcpy(&ops[21], "Hello", 5);

	struct l2_vm vm;
	l2_vm_init(&vm, ops, sizeof(ops) / sizeof(*ops));

	l2_vm_run(&vm);

	printf("Stack:\n");
	for (l2_word i = 0; i < vm.sptr; ++i) {
		printf("  %i: %i\n", i, vm.stack[i]);
	}

	printf("Heap:\n");
	for (l2_word i = 0; i < vm.valuessize; ++i) {
		if (l2_bitset_get(&vm.valueset, i)) {
			printf("  %u: ", i);
			print_var(&vm.values[i]);
		}
	}

	l2_vm_gc(&vm);

	printf("Heap:\n");
	for (l2_word i = 0; i < vm.valuessize; ++i) {
		if (l2_bitset_get(&vm.valueset, i)) {
			printf("  %u: ", i);
			print_var(&vm.values[i]);
		}
	}

	l2_vm_free(&vm);
}
