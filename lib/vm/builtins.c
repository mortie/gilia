#include "vm/builtins.h"

#include <stdio.h>

static void print_val(struct l2_vm *vm, struct l2_io_writer *out, struct l2_vm_value *val) {
	switch (l2_vm_value_type(val)) {
		case L2_VAL_TYPE_NONE:
			l2_io_printf(out, "(none)");
			break;

		case L2_VAL_TYPE_ATOM:
			if (val->atom == vm->values[vm->ktrue].atom) {
				l2_io_printf(out, "(true)");
			} else if (val->atom == vm->values[vm->kfalse].atom) {
				l2_io_printf(out, "(false)");
			} else {
				l2_io_printf(out, "(atom %u)", val->atom);
			}
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

		case L2_VAL_TYPE_CONTINUATION:
			l2_io_printf(out, "(continuation)");
			break;
	}
}

l2_word l2_builtin_add(struct l2_vm *vm, l2_word argc, l2_word *argv) {
	if (argc < 1) {
		return 0;
	}

	struct l2_vm_value *val = &vm->values[argv[0]];
	if (l2_vm_value_type(val) != L2_VAL_TYPE_REAL) {
		return l2_vm_type_error(vm, val);
	}

	double sum = val->real;
	for (l2_word i = 1; i < argc; ++i) {
		val = &vm->values[argv[i]];
		if (l2_vm_value_type(val) != L2_VAL_TYPE_REAL) {
			return l2_vm_type_error(vm, val);
		}

		sum += val->real;
	}

	l2_word id = l2_vm_alloc(vm, L2_VAL_TYPE_REAL, 0);
	vm->values[id].real = sum;
	return id;
}

l2_word l2_builtin_sub(struct l2_vm *vm, l2_word argc, l2_word *argv) {
	if (argc < 1) {
		return 0;
	}

	struct l2_vm_value *val = &vm->values[argv[0]];
	if (l2_vm_value_type(val) != L2_VAL_TYPE_REAL) {
		return l2_vm_type_error(vm, val);
	}

	double sum = val->real;
	for (l2_word i = 1; i < argc; ++i) {
		val = &vm->values[argv[i]];
		if (l2_vm_value_type(val) != L2_VAL_TYPE_REAL) {
			return l2_vm_type_error(vm, val);
		}

		sum -= val->real;
	}

	l2_word id = l2_vm_alloc(vm, L2_VAL_TYPE_REAL, 0);
	vm->values[id].real = sum;
	return id;
}

l2_word l2_builtin_mul(struct l2_vm *vm, l2_word argc, l2_word *argv) {
	if (argc < 1) {
		return 0;
	}

	struct l2_vm_value *val = &vm->values[argv[0]];
	if (l2_vm_value_type(val) != L2_VAL_TYPE_REAL) {
		return l2_vm_type_error(vm, val);
	}

	double sum = val->real;
	for (l2_word i = 1; i < argc; ++i) {
		val = &vm->values[argv[i]];
		if (l2_vm_value_type(val) != L2_VAL_TYPE_REAL) {
			return l2_vm_type_error(vm, val);
		}

		sum *= val->real;
	}

	l2_word id = l2_vm_alloc(vm, L2_VAL_TYPE_REAL, 0);
	vm->values[id].real = sum;
	return id;
}

l2_word l2_builtin_div(struct l2_vm *vm, l2_word argc, l2_word *argv) {
	if (argc < 1) {
		return 0;
	}

	struct l2_vm_value *val = &vm->values[argv[0]];
	if (l2_vm_value_type(val) != L2_VAL_TYPE_REAL) {
		return l2_vm_type_error(vm, val);
	}

	double sum = val->real;
	for (l2_word i = 1; i < argc; ++i) {
		val = &vm->values[argv[i]];
		if (l2_vm_value_type(val) != L2_VAL_TYPE_REAL) {
			return l2_vm_type_error(vm, val);
		}

		sum /= val->real;
	}

	l2_word id = l2_vm_alloc(vm, L2_VAL_TYPE_REAL, 0);
	vm->values[id].real = sum;
	return id;
}

l2_word l2_builtin_print(struct l2_vm *vm, l2_word argc, l2_word *argv) {
	for (size_t i = 0; i < argc; ++i) {
		if (i != 0) {
			vm->std_output->write(vm->std_output, " ", 1);
		}

		struct l2_vm_value *val = &vm->values[argv[i]];
		print_val(vm, vm->std_output, val);
	}

	vm->std_output->write(vm->std_output, "\n", 1);
	return 0;
}

l2_word l2_builtin_len(struct l2_vm *vm, l2_word argc, l2_word *argv) {
	if (argc != 1) {
		return l2_vm_error(vm, "Expected 1 argument");
	}

	l2_word ret_id = l2_vm_alloc(vm, L2_VAL_TYPE_REAL, 0);
	struct l2_vm_value *ret = &vm->values[ret_id];
	ret->real = 0;

	struct l2_vm_value *val = &vm->values[argv[0]];
	switch (l2_vm_value_type(val)) {
	case L2_VAL_TYPE_NONE:
	case L2_VAL_TYPE_ATOM:
	case L2_VAL_TYPE_REAL:
	case L2_VAL_TYPE_FUNCTION:
	case L2_VAL_TYPE_CFUNCTION:
	case L2_VAL_TYPE_ERROR:
	case L2_VAL_TYPE_CONTINUATION:
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

l2_word l2_builtin_if(struct l2_vm *vm, l2_word argc, l2_word *argv) {
	if (argc != 2 && argc != 3) {
		return l2_vm_error(vm, "Expected 2 or 3 arguments");
	}

	struct l2_vm_value *cond = &vm->values[argv[0]];

	if (
			l2_vm_value_type(cond) == L2_VAL_TYPE_ATOM &&
			cond->atom == vm->values[vm->ktrue].atom) {
		l2_word ret_id = l2_vm_alloc(vm, L2_VAL_TYPE_CONTINUATION, 0);
		struct l2_vm_value *ret = &vm->values[ret_id];
		ret->cont.call = argv[1];
		ret->cont.arg = 0;
		return ret_id;
	} else if (argc == 3) {
		l2_word ret_id = l2_vm_alloc(vm, L2_VAL_TYPE_CONTINUATION, 0);
		struct l2_vm_value *ret = &vm->values[ret_id];
		ret->cont.call = argv[2];
		ret->cont.arg = 0;
		return ret_id;
	} else {
		return 0;
	}
}
