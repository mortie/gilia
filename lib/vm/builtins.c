#include "vm/builtins.h"

#include <stdio.h>

static void print_val(struct gil_vm *vm, struct gil_io_writer *out, struct gil_vm_value *val) {
	switch (gil_value_get_type(val)) {
		case GIL_VAL_TYPE_NONE:
			gil_io_printf(out, "(none)");
			break;

		case GIL_VAL_TYPE_ATOM:
			if (val->atom == vm->values[vm->ktrue].atom) {
				gil_io_printf(out, "(true)");
			} else if (val->atom == vm->values[vm->kfalse].atom) {
				gil_io_printf(out, "(false)");
			} else {
				gil_io_printf(out, "(atom %u)", val->atom);
			}
			break;

		case GIL_VAL_TYPE_REAL:
			gil_io_printf(out, "%g", val->real);
			break;

		case GIL_VAL_TYPE_BUFFER:
			if (val->buffer != NULL) {
				out->write(out, val->buffer, val->extra.buf_length);
			}
			break;

		case GIL_VAL_TYPE_ARRAY:
			out->write(out, "[", 1);
			gil_word *data;
			if (val->flags & GIL_VAL_SBO) {
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

		case GIL_VAL_TYPE_NAMESPACE:
			gil_io_printf(out, "(namespace)");
			break;

		case GIL_VAL_TYPE_FUNCTION:
		case GIL_VAL_TYPE_CFUNCTION:
			gil_io_printf(out, "(function)");
			break;

		case GIL_VAL_TYPE_CVAL:
			gil_io_printf(out, "(C value)");
			break;

		case GIL_VAL_TYPE_CONTINUATION:
			gil_io_printf(out, "(continuation)");
			break;

		case GIL_VAL_TYPE_RETURN:
			gil_io_printf(out, "(return)");
			break;

		case GIL_VAL_TYPE_ERROR:
			gil_io_printf(out, "(error: %s)", val->error);
			break;
	}
}

#define X(name, identity, op) \
gil_word name(struct gil_vm *vm, gil_word _, gil_word argc, gil_word *argv) { \
	if (argc == 0) { \
		gil_word id = gil_vm_alloc(vm, GIL_VAL_TYPE_REAL, 0); \
		vm->values[id].real = identity; \
		return id; \
	} \
	struct gil_vm_value *first = &vm->values[argv[0]]; \
	if (gil_value_get_type(first) != GIL_VAL_TYPE_REAL) { \
		return gil_vm_type_error(vm, first); \
	} \
	if (argc == 1) { \
		gil_word id = gil_vm_alloc(vm, GIL_VAL_TYPE_REAL, 0); \
		vm->values[id].real = identity op first->real; \
		return id; \
	} \
	double sum = first->real; \
	for (gil_word i = 1; i < argc; ++i) { \
		struct gil_vm_value *val = &vm->values[argv[i]]; \
		if (gil_value_get_type(val) != GIL_VAL_TYPE_REAL) { \
			return gil_vm_type_error(vm, val); \
		} \
		sum = sum op val->real; \
	} \
	gil_word id = gil_vm_alloc(vm, GIL_VAL_TYPE_REAL, 0); \
	vm->values[id].real = sum; \
	return id; \
}
X(gil_builtin_add, 0, +)
X(gil_builtin_sub, 0, -)
X(gil_builtin_mul, 1, *)
X(gil_builtin_div, 1, /)
#undef X

gil_word gil_builtin_eq(struct gil_vm *vm, gil_word _, gil_word argc, gil_word *argv) {
	if (argc < 2) {
		return vm->ktrue;
	}

	for (gil_word i = 1; i < argc; ++i) {
		if (argv[i - 1] == argv[i]) continue;
		struct gil_vm_value *a = &vm->values[argv[i - 1]];
		struct gil_vm_value *b = &vm->values[argv[i]];
		if (gil_value_get_type(a) != gil_value_get_type(b)) {
			return vm->kfalse;
		}

		enum gil_value_type typ = gil_value_get_type(a);
		if (typ == GIL_VAL_TYPE_ATOM) {
			if (a->atom != b->atom) {
				return vm->kfalse;
			}
		} else if (typ == GIL_VAL_TYPE_REAL) {
			if (a->real != b->real) {
				return vm->kfalse;
			}
		} else if (typ == GIL_VAL_TYPE_BUFFER) {
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

gil_word gil_builtin_neq(struct gil_vm *vm, gil_word _, gil_word argc, gil_word *argv) {
	gil_word ret_id = gil_builtin_eq(vm, 0, argc, argv);
	if (ret_id == vm->ktrue) {
		return vm->kfalse;
	} else if (ret_id == vm->kfalse) {
		return vm->ktrue;
	} else {
		return ret_id;
	}
}

#define X(name, op) \
gil_word name(struct gil_vm *vm, gil_word _, gil_word argc, gil_word *argv) { \
	if (argc < 2) { \
		return vm->ktrue; \
	} \
	struct gil_vm_value *lhs = &vm->values[argv[0]]; \
	if (gil_value_get_type(lhs) != GIL_VAL_TYPE_REAL) { \
		return gil_vm_type_error(vm, lhs); \
	} \
	for (gil_word i = 1; i < argc; ++i) { \
		struct gil_vm_value *rhs = &vm->values[argv[i]]; \
		if (gil_value_get_type(rhs) != GIL_VAL_TYPE_REAL) { \
			return gil_vm_type_error(vm, rhs); \
		} \
		if (!(lhs->real op rhs->real)) { \
			return vm->kfalse; \
		} \
		lhs = rhs; \
	} \
	return vm->ktrue; \
}
X(gil_builtin_lt, <)
X(gil_builtin_lteq, <=)
X(gil_builtin_gt, >)
X(gil_builtin_gteq, >=)
#undef X

gil_word gil_builtin_land(struct gil_vm *vm, gil_word _, gil_word argc, gil_word *argv) {
	for (gil_word i = 0; i < argc; ++i) {
		struct gil_vm_value *val = &vm->values[argv[i]];
		if (gil_value_get_type(val) == GIL_VAL_TYPE_ERROR) {
			return argv[i];
		}

		if (!gil_vm_val_is_true(vm, val)) {
			return vm->kfalse;
		}
	}

	return vm->ktrue;
}

gil_word gil_builtin_lor(struct gil_vm *vm, gil_word _, gil_word argc, gil_word *argv) {
	for (gil_word i = 0; i < argc; ++i) {
		struct gil_vm_value *val = &vm->values[argv[i]];
		if (gil_value_get_type(val) == GIL_VAL_TYPE_ERROR) {
			return argv[i];
		}

		if (gil_vm_val_is_true(vm, val)) {
			return vm->ktrue;
		}
	}

	return vm->kfalse;
}

gil_word gil_builtin_first(struct gil_vm *vm, gil_word _, gil_word argc, gil_word *argv) {
	for (gil_word i = 0; i < argc; ++i) {
		if (gil_value_get_type(&vm->values[argv[i]]) != GIL_VAL_TYPE_NONE) {
			return argv[i];
		}
	}

	return vm->knone;
}

gil_word gil_builtin_print(struct gil_vm *vm, gil_word _, gil_word argc, gil_word *argv) {
	for (size_t i = 0; i < argc; ++i) {
		if (i != 0) {
			vm->std_output->write(vm->std_output, " ", 1);
		}

		struct gil_vm_value *val = &vm->values[argv[i]];
		print_val(vm, vm->std_output, val);
	}

	vm->std_output->write(vm->std_output, "\n", 1);
	return vm->knone;
}

gil_word gil_builtin_write(struct gil_vm *vm, gil_word _, gil_word argc, gil_word *argv) {
	for (size_t i = 0; i < argc; ++i) {
		struct gil_vm_value *val = &vm->values[argv[i]];
		print_val(vm, vm->std_output, val);
	}

	return vm->knone;
}

gil_word gil_builtin_len(struct gil_vm *vm, gil_word _, gil_word argc, gil_word *argv) {
	if (argc != 1) {
		return gil_vm_error(vm, "Expected 1 argument");
	}

	gil_word ret_id = gil_vm_alloc(vm, GIL_VAL_TYPE_REAL, 0);
	struct gil_vm_value *ret = &vm->values[ret_id];
	ret->real = 0;

	struct gil_vm_value *val = &vm->values[argv[0]];
	switch (gil_value_get_type(val)) {
	case GIL_VAL_TYPE_NONE:
	case GIL_VAL_TYPE_ATOM:
	case GIL_VAL_TYPE_REAL:
	case GIL_VAL_TYPE_FUNCTION:
	case GIL_VAL_TYPE_CFUNCTION:
	case GIL_VAL_TYPE_CVAL:
	case GIL_VAL_TYPE_CONTINUATION:
	case GIL_VAL_TYPE_RETURN:
	case GIL_VAL_TYPE_ERROR:
		break;

	case GIL_VAL_TYPE_BUFFER:
		ret->real = val->extra.buf_length;
		break;

	case GIL_VAL_TYPE_ARRAY:
		ret->real = val->extra.arr_length;
		break;

	case GIL_VAL_TYPE_NAMESPACE:
		if (val->ns) {
			ret->real = val->ns->len;
		}
		break;
	}

	return ret_id;
}

gil_word gil_builtin_if(struct gil_vm *vm, gil_word _, gil_word argc, gil_word *argv) {
	if (argc != 2 && argc != 3) {
		return gil_vm_error(vm, "Expected 2 or 3 arguments");
	}

	if (gil_vm_val_is_true(vm, &vm->values[argv[0]])) {
		gil_word ret_id = gil_vm_alloc(vm, GIL_VAL_TYPE_CONTINUATION, 0);
		struct gil_vm_value *ret = &vm->values[ret_id];
		ret->extra.cont_call = argv[1];
		return ret_id;
	} else if (argc == 3) {
		gil_word ret_id = gil_vm_alloc(vm, GIL_VAL_TYPE_CONTINUATION, 0);
		struct gil_vm_value *ret = &vm->values[ret_id];
		ret->extra.cont_call = argv[2];
		return ret_id;
	} else {
		return 0;
	}
}

struct loop_context {
	struct gil_vm_contcontext base;
	gil_word func;
};

static gil_word loop_callback(struct gil_vm *vm, gil_word retval, gil_word cont) {
	struct gil_vm_value *val = &vm->values[retval];
	if (gil_value_get_type(val) == GIL_VAL_TYPE_ERROR) {
		return retval;
	} else if (
			gil_value_get_type(val) == GIL_VAL_TYPE_ATOM &&
			val->atom == vm->values[vm->kstop].atom) {
		return vm->knone;
	} else {
		return cont;
	}
}

static void loop_marker(
		struct gil_vm *vm, void *data, void (*mark)(struct gil_vm *vm, gil_word id)) {
	struct loop_context *ctx = data;
	mark(vm, ctx->func);
}

gil_word gil_builtin_loop(struct gil_vm *vm, gil_word _, gil_word argc, gil_word *argv) {
	if (argc != 1) {
		return gil_vm_error(vm, "Expected 1 argument");
	}

	struct loop_context *ctx = malloc(sizeof(*ctx));
	if (ctx == NULL) {
		return gil_vm_error(vm, "Allocation failure");
	}

	ctx->base.callback = loop_callback;
	ctx->base.marker = loop_marker;
	ctx->base.args = vm->knone;
	ctx->func = argv[0];

	gil_word cont_id = gil_vm_alloc(vm, GIL_VAL_TYPE_CONTINUATION, 0);
	struct gil_vm_value *cont = &vm->values[cont_id];
	cont->extra.cont_call = ctx->func;
	cont->cont = &ctx->base;
	return cont_id;
}

struct while_context {
	struct gil_vm_contcontext base;
	gil_word cond, body;
};

static gil_word while_callback(struct gil_vm *vm, gil_word retval, gil_word cont_id) {
	struct gil_vm_value *cont = &vm->values[cont_id];
	struct while_context *ctx = (struct while_context *)cont->cont;
	struct gil_vm_value *ret = &vm->values[retval];

	if (gil_value_get_type(ret) == GIL_VAL_TYPE_ERROR) {
		return retval;
	}

	if (cont->extra.cont_call == ctx->cond) {
		if (gil_vm_val_is_true(vm, ret)) {
			cont->extra.cont_call = ctx->body;
			return cont_id;
		} else {
			return vm->knone;
		}
	} else {
		if (gil_value_get_type(ret) == GIL_VAL_TYPE_ERROR) {
			return retval;
		} else {
			cont->extra.cont_call = ctx->cond;
			return cont_id;
		}
	}
}

static void while_marker(
		struct gil_vm *vm, void *data, void (*mark)(struct gil_vm *vm, gil_word id)) {
	struct while_context *ctx = data;
	mark(vm, ctx->cond);
	mark(vm, ctx->body);
}

gil_word gil_builtin_while(struct gil_vm *vm, gil_word _, gil_word argc, gil_word *argv) {
	if (argc != 2) {
		return gil_vm_error(vm, "Expected 2 arguments");
	}

	struct while_context *ctx = malloc(sizeof(*ctx));
	if (ctx == NULL) {
		return gil_vm_error(vm, "Allocation failure");
	}

	ctx->base.callback = while_callback;
	ctx->base.marker = while_marker;
	ctx->base.args = vm->knone;
	ctx->cond = argv[0];
	ctx->body = argv[1];

	gil_word cont_id = gil_vm_alloc(vm, GIL_VAL_TYPE_CONTINUATION, 0);
	struct gil_vm_value *cont = &vm->values[cont_id];
	cont->extra.cont_call = ctx->cond;
	cont->cont = &ctx->base;
	return cont_id;
}

struct for_context {
	struct gil_vm_contcontext base;
	gil_word iter;
	gil_word func;
};

static gil_word for_callback(struct gil_vm *vm, gil_word retval, gil_word cont_id) {
	struct gil_vm_value *cont = &vm->values[cont_id];
	struct for_context *ctx = (struct for_context *)cont->cont;
	struct gil_vm_value *ret = &vm->values[retval];

	if (gil_value_get_type(ret) == GIL_VAL_TYPE_ERROR) {
		return retval;
	}

	struct gil_vm_value *args = &vm->values[cont->cont->args];
	if (cont->extra.cont_call == ctx->iter) {
		if (
				gil_value_get_type(ret) == GIL_VAL_TYPE_ATOM &&
				ret->atom == vm->values[vm->kstop].atom) {
			return vm->knone;
		} else {
			cont->extra.cont_call = ctx->func;
			args->extra.arr_length = 1;
			args->shortarray[0] = retval;
			return cont_id;
		}
	} else {
		cont->extra.cont_call = ctx->iter;
		args->extra.arr_length = 0;
		return cont_id;
	}
}

static void for_marker(
		struct gil_vm *vm, void *data, void (*mark)(struct gil_vm *vm, gil_word id)) {
	struct for_context *ctx = data;
	mark(vm, ctx->iter);
	mark(vm, ctx->func);
}

gil_word gil_builtin_for(struct gil_vm *vm, gil_word _, gil_word argc, gil_word *argv) {
	if (argc != 2) {
		return gil_vm_error(vm, "Expected 2 arguments");
	}

	gil_word args_id = gil_vm_alloc(vm, GIL_VAL_TYPE_ARRAY, GIL_VAL_SBO);
	struct gil_vm_value *args = &vm->values[args_id];
	args->extra.arr_length = 0;

	struct for_context *ctx = malloc(sizeof(*ctx));
	ctx->base.callback = for_callback;
	ctx->base.marker = for_marker;
	ctx->base.args = args_id;
	ctx->iter = argv[0];
	ctx->func = argv[1];

	gil_word cont_id = gil_vm_alloc(vm, GIL_VAL_TYPE_CONTINUATION, 0);
	struct gil_vm_value *cont = &vm->values[cont_id];
	cont->extra.cont_call = ctx->iter;
	cont->cont = &ctx->base;
	return cont_id;
}

static gil_word guard_callback(struct gil_vm *vm, gil_word retval, gil_word cont_id) {
	struct gil_vm_value *ret = &vm->values[cont_id];
	free(ret->cont);
	ret->flags = GIL_VAL_TYPE_RETURN;
	ret->ret = retval;
	return cont_id;
}

gil_word gil_builtin_guard(struct gil_vm *vm, gil_word _, gil_word argc, gil_word *argv) {
	if (argc != 1 && argc != 2) {
		return gil_vm_error(vm, "Expected 1 or 2 arguments");
	}

	struct gil_vm_value *cond = &vm->values[argv[0]];
	if (gil_value_get_type(cond) == GIL_VAL_TYPE_ERROR) {
		return argv[0];
	}

	if (argc == 1) {
		if (!gil_vm_val_is_true(vm, cond)) {
			return vm->knone;
		}

		gil_word ret_id = gil_vm_alloc(vm, GIL_VAL_TYPE_RETURN, 0);
		vm->values[ret_id].ret = vm->knone;
		return ret_id;
	}

	struct gil_vm_value *body = &vm->values[argv[1]];
	if (gil_value_get_type(body) == GIL_VAL_TYPE_ERROR) {
		return argv[1];
	}

	if (!gil_vm_val_is_true(vm, cond)) {
		return vm->knone;
	}

	struct gil_vm_contcontext *ctx = malloc(sizeof(*ctx));
	if (ctx == NULL) {
		return gil_vm_error(vm, "Allocation failure");
	}

	ctx->callback = guard_callback;
	ctx->marker = NULL;
	ctx->args = vm->knone;

	gil_word cont_id = gil_vm_alloc(vm, GIL_VAL_TYPE_CONTINUATION, 0);
	struct gil_vm_value *cont = &vm->values[cont_id];
	cont->extra.cont_call = argv[1];
	cont->cont = ctx;
	return cont_id;
}
