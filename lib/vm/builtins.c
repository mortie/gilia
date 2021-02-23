#include "vm/builtins.h"

#include <stdio.h>

static void print_val(struct l2_vm *vm, struct l2_io_writer *out, struct l2_vm_value *val) {
	switch (l2_vm_value_type(val)) {
		case L2_VAL_TYPE_NONE:
			l2_io_printf(out, "(none)");
			break;

		case L2_VAL_TYPE_ATOM:
			l2_io_printf(out, "(atom %u)", val->atom);
			break;

		case L2_VAL_TYPE_REAL:
			l2_io_printf(out, "%g", val->real);
			break;

		case L2_VAL_TYPE_BUFFER:
			if (val->buffer != NULL) {
				out->write(out, val->buffer->data, val->buffer->len);
			}
			break;

		case L2_VAL_TYPE_ARRAY:
			if (val->array == NULL) {
				out->write(out, "[]", 2);
				break;
			}

			out->write(out, "[", 1);
			for (size_t i = 0; i < val->array->len; ++i) {
				if (i != 0) {
					out->write(out, " ", 1);
				}

				print_val(vm, out, &vm->values[val->array->data[i]]);
			}
			out->write(out, "]", 1);
			break;

		case L2_VAL_TYPE_NAMESPACE:
			l2_io_printf(out, "(namespace)");
			break;

		case L2_VAL_TYPE_FUNCTION:
		case L2_VAL_TYPE_CFUNCTION:
			l2_io_printf(out, "(function)");
			break;

		case L2_VAL_TYPE_ERROR:
			l2_io_printf(out, "(error: %s)", val->error);
			break;
	}
}

l2_word l2_builtin_add(struct l2_vm *vm, struct l2_vm_array *args) {
	double sum = 0;
	for (size_t i = 0; i < args->len; ++i) {
		struct l2_vm_value *val = &vm->values[args->data[i]];
		if (l2_vm_value_type(val) != L2_VAL_TYPE_REAL) {
			return l2_vm_type_error(vm, val);
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
			vm->std_output->write(vm->std_output, " ", 1);
		}

		struct l2_vm_value *val = &vm->values[args->data[i]];
		print_val(vm, vm->std_output, val);
	}

	vm->std_output->write(vm->std_output, "\n", 1);
	return 0;
}

l2_word l2_builtin_len(struct l2_vm *vm, struct l2_vm_array *args) {
	// TODO: error if wrong argc
	l2_word ret_id = l2_vm_alloc(vm, L2_VAL_TYPE_REAL, 0);
	struct l2_vm_value *ret = &vm->values[ret_id];
	ret->real = 0;

	struct l2_vm_value *val = &vm->values[args->data[0]];
	switch (l2_vm_value_type(val)) {
	case L2_VAL_TYPE_NONE:
	case L2_VAL_TYPE_ATOM:
	case L2_VAL_TYPE_REAL:
	case L2_VAL_TYPE_FUNCTION:
	case L2_VAL_TYPE_CFUNCTION:
	case L2_VAL_TYPE_ERROR:
		break;

	case L2_VAL_TYPE_BUFFER:
		if (val->buffer) {
			ret->real = val->buffer->len;
		}
		break;

	case L2_VAL_TYPE_ARRAY:
		if (val->array) {
			ret->real = val->array->len;
		}
		break;

	case L2_VAL_TYPE_NAMESPACE:
		if (val->ns) {
			ret->real = val->ns->len;
		}
	}

	return ret_id;
}
