#include "vm/builtins.h"

#include <stdio.h>

static void print_val(struct l2_vm *vm, struct l2_io_writer *out, struct l2_vm_value *val) {
	switch (l2_value_get_type(val)) {
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
				out->write(out, val->buffer, val->extra.buf_length);
			}
			break;

		case L2_VAL_TYPE_ARRAY:
			out->write(out, "[", 1);
			l2_word *data;
			if (val->flags & L2_VAL_SBO) {
				data = val->shortarray;
			} else {
				data = val->array->data;
			}

			for (size_t i = 0; i < val->extra.arr_length; ++i) {
				if (i != 0) {
					out->write(out, " ", 1);
				}

				print_val(vm, out, &vm->values[data[i]]);
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

#define X(name, identity, op) \
l2_word name(struct l2_vm *vm, l2_word argc, l2_word *argv) { \
	if (argc == 0) { \
		l2_word id = l2_vm_alloc(vm, L2_VAL_TYPE_REAL, 0); \
		vm->values[id].real = identity; \
		return id; \
	} \
	struct l2_vm_value *first = &vm->values[argv[0]]; \
	if (l2_value_get_type(first) != L2_VAL_TYPE_REAL) { \
		return l2_vm_type_error(vm, first); \
	} \
	if (argc == 1) { \
		l2_word id = l2_vm_alloc(vm, L2_VAL_TYPE_REAL, 0); \
		vm->values[id].real = identity op first->real; \
		return id; \
	} \
	double sum = first->real; \
	for (l2_word i = 1; i < argc; ++i) { \
		struct l2_vm_value *val = &vm->values[argv[i]]; \
		if (l2_value_get_type(val) != L2_VAL_TYPE_REAL) { \
			return l2_vm_type_error(vm, val); \
		} \
		sum = sum op val->real; \
	} \
	l2_word id = l2_vm_alloc(vm, L2_VAL_TYPE_REAL, 0); \
	vm->values[id].real = sum; \
	return id; \
}
X(l2_builtin_add, 0, +)
X(l2_builtin_sub, 0, -)
X(l2_builtin_mul, 1, *)
X(l2_builtin_div, 1, /)
#undef X

l2_word l2_builtin_eq(struct l2_vm *vm, l2_word argc, l2_word *argv) {
	if (argc < 2) {
		return vm->ktrue;
	}

	for (l2_word i = 1; i < argc; ++i) {
		if (argv[i - 1] == argv[i]) continue;
		struct l2_vm_value *a = &vm->values[argv[i - 1]];
		struct l2_vm_value *b = &vm->values[argv[i]];
		if (l2_value_get_type(a) != l2_value_get_type(b)) {
			return vm->kfalse;
		}

		enum l2_value_type typ = l2_value_get_type(a);
		if (typ == L2_VAL_TYPE_ATOM) {
			if (a->atom != b->atom) {
				return vm->kfalse;
			}
		} else if (typ == L2_VAL_TYPE_REAL) {
			if (a->real != b->real) {
				return vm->kfalse;
			}
		} else if (typ == L2_VAL_TYPE_BUFFER) {
			if (a->buffer == NULL && b->buffer == NULL) continue;
			if (a->buffer == NULL || b->buffer == NULL) {
				return vm->kfalse;
			}

			if (a->extra.buf_length != b->extra.buf_length) {
				return vm->kfalse;
			}

			if (memcmp(a->buffer, b->buffer, a->extra.buf_length) != 0) {
				return vm->kfalse;
			}
		} else {
			return vm->kfalse;
		}
	}

	return vm->ktrue;
}

l2_word l2_builtin_neq(struct l2_vm *vm, l2_word argc, l2_word *argv) {
	l2_word ret_id = l2_builtin_eq(vm, argc, argv);
	if (ret_id == vm->ktrue) {
		return vm->kfalse;
	} else if (ret_id == vm->kfalse) {
		return vm->ktrue;
	} else {
		return ret_id;
	}
}

#define X(name, op) \
l2_word name(struct l2_vm *vm, l2_word argc, l2_word *argv) { \
	if (argc < 2) { \
		return vm->ktrue; \
	} \
	struct l2_vm_value *lhs = &vm->values[argv[0]]; \
	if (l2_value_get_type(lhs) != L2_VAL_TYPE_REAL) { \
		return l2_vm_type_error(vm, lhs); \
	} \
	for (l2_word i = 1; i < argc; ++i) { \
		struct l2_vm_value *rhs = &vm->values[argv[i]]; \
		if (l2_value_get_type(rhs) != L2_VAL_TYPE_REAL) { \
			return l2_vm_type_error(vm, rhs); \
		} \
		if (!(lhs->real op rhs->real)) { \
			return vm->kfalse; \
		} \
		lhs = rhs; \
	} \
	return vm->ktrue; \
}
X(l2_builtin_lt, <)
X(l2_builtin_lteq, <=)
X(l2_builtin_gt, >)
X(l2_builtin_gteq, >=)
#undef X

l2_word l2_builtin_land(struct l2_vm *vm, l2_word argc, l2_word *argv) {
	for (l2_word i = 0; i < argc; ++i) {
		struct l2_vm_value *val = &vm->values[argv[i]];
		if (l2_value_get_type(val) == L2_VAL_TYPE_ERROR) {
			return argv[i];
		}

		if (!l2_vm_val_is_true(vm, val)) {
			return vm->kfalse;
		}
	}

	return vm->ktrue;
}

l2_word l2_builtin_lor(struct l2_vm *vm, l2_word argc, l2_word *argv) {
	for (l2_word i = 0; i < argc; ++i) {
		struct l2_vm_value *val = &vm->values[argv[i]];
		if (l2_value_get_type(val) == L2_VAL_TYPE_ERROR) {
			return argv[i];
		}

		if (l2_vm_val_is_true(vm, val)) {
			return vm->ktrue;
		}
	}

	return vm->kfalse;
}

l2_word l2_builtin_first(struct l2_vm *vm, l2_word argc, l2_word *argv) {
	for (l2_word i = 0; i < argc; ++i) {
		if (l2_value_get_type(&vm->values[argv[i]]) != L2_VAL_TYPE_NONE) {
			return argv[i];
		}
	}

	return vm->knone;
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
	switch (l2_value_get_type(val)) {
	case L2_VAL_TYPE_NONE:
	case L2_VAL_TYPE_ATOM:
	case L2_VAL_TYPE_REAL:
	case L2_VAL_TYPE_FUNCTION:
	case L2_VAL_TYPE_CFUNCTION:
	case L2_VAL_TYPE_ERROR:
	case L2_VAL_TYPE_CONTINUATION:
		break;

	case L2_VAL_TYPE_BUFFER:
		ret->real = val->extra.buf_length;
		break;

	case L2_VAL_TYPE_ARRAY:
		ret->real = val->extra.arr_length;
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

	if (l2_vm_val_is_true(vm, &vm->values[argv[0]])) {
		l2_word ret_id = l2_vm_alloc(vm, L2_VAL_TYPE_CONTINUATION, 0);
		struct l2_vm_value *ret = &vm->values[ret_id];
		ret->extra.cont_call = argv[1];
		return ret_id;
	} else if (argc == 3) {
		l2_word ret_id = l2_vm_alloc(vm, L2_VAL_TYPE_CONTINUATION, 0);
		struct l2_vm_value *ret = &vm->values[ret_id];
		ret->extra.cont_call = argv[2];
		return ret_id;
	} else {
		return 0;
	}
}

struct loop_context {
	struct l2_vm_contcontext base;
	l2_word func;
};

static l2_word loop_callback(struct l2_vm *vm, l2_word retval, l2_word cont) {
	struct l2_vm_value *val = &vm->values[retval];
	if (l2_value_get_type(val) == L2_VAL_TYPE_ATOM && val->atom == vm->values[vm->kstop].atom) {
		return vm->knone;
	} else {
		return cont;
	}
}

static void loop_marker(
		struct l2_vm *vm, void *data, void (*mark)(struct l2_vm *vm, l2_word id)) {
	struct loop_context *ctx = data;
	mark(vm, ctx->func);
}

l2_word l2_builtin_loop(struct l2_vm *vm, l2_word argc, l2_word *argv) {
	if (argc != 1) {
		return l2_vm_error(vm, "Expected 1 argument");
	}

	struct loop_context *ctx = malloc(sizeof(*ctx));
	if (ctx == NULL) {
		return l2_vm_error(vm, "Allocation failure");
	}

	ctx->base.callback = loop_callback;
	ctx->base.marker = loop_marker;
	ctx->func = argv[0];

	l2_word cont_id = l2_vm_alloc(vm, L2_VAL_TYPE_CONTINUATION, 0);
	struct l2_vm_value *cont = &vm->values[cont_id];
	cont->extra.cont_call = ctx->func;
	cont->cont = &ctx->base;
	return cont_id;
}

struct while_context {
	struct l2_vm_contcontext base;
	l2_word cond, body;
};

static l2_word while_callback(struct l2_vm *vm, l2_word retval, l2_word cont_id) {
	struct l2_vm_value *cont = &vm->values[cont_id];
	struct while_context *ctx = (struct while_context *)cont->cont;

	if (cont->extra.cont_call == ctx->cond) {
		if (l2_vm_val_is_true(vm, &vm->values[retval])) {
			cont->extra.cont_call = ctx->body;
			return cont_id;
		} else {
			return retval;
		}
	} else {
		struct l2_vm_value *ret = &vm->values[retval];
		if (l2_value_get_type(ret) == L2_VAL_TYPE_ERROR) {
			return retval;
		} else {
			cont->extra.cont_call = ctx->cond;
			return cont_id;
		}
	}
}

static void while_marker(
		struct l2_vm *vm, void *data, void (*mark)(struct l2_vm *vm, l2_word id)) {
	struct while_context *ctx = data;
	mark(vm, ctx->cond);
	mark(vm, ctx->body);
}

l2_word l2_builtin_while(struct l2_vm *vm, l2_word argc, l2_word *argv) {
	if (argc != 2) {
		return l2_vm_error(vm, "Expected 2 arguments");
	}

	struct while_context *ctx = malloc(sizeof(*ctx));
	if (ctx == NULL) {
		return l2_vm_error(vm, "Allocation failure");
	}

	ctx->base.callback = while_callback;
	ctx->base.marker = while_marker;
	ctx->cond = argv[0];
	ctx->body = argv[1];

	l2_word cont_id = l2_vm_alloc(vm, L2_VAL_TYPE_CONTINUATION, 0);
	struct l2_vm_value *cont = &vm->values[cont_id];
	cont->extra.cont_call = ctx->cond;
	cont->cont = &ctx->base;
	return cont_id;
}
