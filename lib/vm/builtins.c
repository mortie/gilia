#include "vm/builtins.h"

#include <stdio.h>

static void print_val(struct l2_vm *vm, struct l2_vm_value *val) {
	switch (l2_vm_value_type(val)) {
		case L2_VAL_TYPE_NONE:
			printf("(none)");
			break;

		case L2_VAL_TYPE_INTEGER:
			printf("%zi", val->integer);
			break;

		case L2_VAL_TYPE_REAL:
			printf("%g", val->real);
			break;

		case L2_VAL_TYPE_BUFFER:
			fwrite(val->buffer->data, 1, val->buffer->len, stdout);
			break;

		case L2_VAL_TYPE_ARRAY:
			putchar('[');
			for (size_t i = 0; i < val->array->len; ++i) {
				if (i != 0) {
					printf(", ");
				}

				print_val(vm, &vm->values[val->array->data[i]]);
			}
			putchar(']');
			break;

		case L2_VAL_TYPE_NAMESPACE:
			printf("(namespace)");
			break;

		case L2_VAL_TYPE_FUNCTION:
		case L2_VAL_TYPE_CFUNCTION:
			printf("(function)");
			break;
	}
}

l2_word l2_builtin_add(struct l2_vm *vm, struct l2_vm_array *args) {
	double sum = 0;
	for (size_t i = 0; i < args->len; ++i) {
		struct l2_vm_value *val = &vm->values[args->data[i]];
		if (l2_vm_value_type(val) != L2_VAL_TYPE_REAL) {
			// TODO: Error
		}

		sum += val->real;
	}

	l2_word id = l2_vm_alloc(vm, L2_VAL_TYPE_REAL, 0);
	vm->values[id].real = sum;
	return id;
}

l2_word l2_builtin_sub(struct l2_vm *vm, struct l2_vm_array *args) {
	return 0;
}

l2_word l2_builtin_mul(struct l2_vm *vm, struct l2_vm_array *args) {
	return 0;
}

l2_word l2_builtin_div(struct l2_vm *vm, struct l2_vm_array *args) {
	return 0;
}

l2_word l2_builtin_print(struct l2_vm *vm, struct l2_vm_array *args) {
	for (size_t i = 0; i < args->len; ++i) {
		if (i != 0) {
			putchar(' ');
		}

		struct l2_vm_value *val = &vm->values[args->data[i]];
		print_val(vm, val);
	}

	putchar('\n');
	return 0;
}
