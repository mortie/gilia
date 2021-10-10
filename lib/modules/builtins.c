#include "modules/builtins.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "io.h"
#include "module.h"
#include "vm/vm.h"

static void print_val(struct gil_vm *vm, struct gil_io_writer *out, struct gil_vm_value *val, int depth) {
	if (depth > GIL_MAX_STACK_DEPTH) {
		gil_io_printf(out, "Print recursion limit reached");
		return;
	}

	switch (gil_value_get_type(val)) {
		case GIL_VAL_TYPE_NONE:
			gil_io_printf(out, "(none)");
			break;

		case GIL_VAL_TYPE_ATOM:
			if (val->atom.atom == vm->values[vm->ktrue].atom.atom) {
				gil_io_printf(out, "(true)");
			} else if (val->atom.atom == vm->values[vm->kfalse].atom.atom) {
				gil_io_printf(out, "(false)");
			} else {
				gil_io_printf(out, "(atom %u)", val->atom.atom);
			}
			break;

		case GIL_VAL_TYPE_REAL:
			gil_io_printf(out, "%g", val->real.real);
			break;

		case GIL_VAL_TYPE_BUFFER:
			if (val->buffer.buffer != NULL) {
				out->write(out, val->buffer.buffer, val->buffer.length);
			}
			break;

		case GIL_VAL_TYPE_ARRAY:
			out->write(out, "[", 1);
			gil_word *data;
			if (val->flags & GIL_VAL_SBO) {
				data = val->array.shortarray;
			} else {
				data = val->array.array->data;
			}

			for (size_t i = 0; i < val->array.length; ++i) {
				if (i != 0) {
					out->write(out, " ", 1);
				}

				print_val(vm, out, &vm->values[data[i]], depth + 1);
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
			gil_io_printf(out, "(error: %s)", val->error.error);
			break;
	}
}

#define X(name, identity, op) \
static gil_word name(\
		struct gil_vm *vm, gil_word mid, gil_word self, \
		gil_word argc, gil_word *argv) { \
	if (argc == 0) { \
		gil_word id = gil_vm_alloc(vm, GIL_VAL_TYPE_REAL, 0); \
		vm->values[id].real.real = identity; \
		return id; \
	} \
	struct gil_vm_value *first = &vm->values[argv[0]]; \
	if (gil_value_get_type(first) != GIL_VAL_TYPE_REAL) { \
		return gil_vm_type_error(vm, first); \
	} \
	if (argc == 1) { \
		gil_word id = gil_vm_alloc(vm, GIL_VAL_TYPE_REAL, 0); \
		vm->values[id].real.real = identity op first->real.real; \
		return id; \
	} \
	double sum = first->real.real; \
	for (gil_word i = 1; i < argc; ++i) { \
		struct gil_vm_value *val = &vm->values[argv[i]]; \
		if (gil_value_get_type(val) != GIL_VAL_TYPE_REAL) { \
			return gil_vm_type_error(vm, val); \
		} \
		sum = sum op val->real.real; \
	} \
	gil_word id = gil_vm_alloc(vm, GIL_VAL_TYPE_REAL, 0); \
	vm->values[id].real.real = sum; \
	return id; \
}
X(builtin_add, 0, +)
X(builtin_sub, 0, -)
X(builtin_mul, 1, *)
X(builtin_div, 1, /)
#undef X

static gil_word builtin_eq(
		struct gil_vm *vm, gil_word mid, gil_word self,
		gil_word argc, gil_word *argv) {
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
			if (a->atom.atom != b->atom.atom) {
				return vm->kfalse;
			}
		} else if (typ == GIL_VAL_TYPE_REAL) {
			if (a->real.real != b->real.real) {
				return vm->kfalse;
			}
		} else if (typ == GIL_VAL_TYPE_BUFFER) {
			if (a->buffer.buffer == NULL && b->buffer.buffer == NULL) continue;
			if (a->buffer.buffer == NULL || b->buffer.buffer == NULL) {
				return vm->kfalse;
			}

			if (a->buffer.length != b->buffer.length) {
				return vm->kfalse;
			}

			if (memcmp(a->buffer.buffer, b->buffer.buffer, a->buffer.length) != 0) {
				return vm->kfalse;
			}
		} else {
			return vm->kfalse;
		}
	}

	return vm->ktrue;
}

static gil_word builtin_neq(
		struct gil_vm *vm, gil_word mid, gil_word self,
		gil_word argc, gil_word *argv) {
	gil_word ret_id = builtin_eq(vm, mid, self, argc, argv);
	if (ret_id == vm->ktrue) {
		return vm->kfalse;
	} else if (ret_id == vm->kfalse) {
		return vm->ktrue;
	} else {
		return ret_id;
	}
}

#define X(name, op) \
static gil_word name( \
		struct gil_vm *vm, gil_word mid, gil_word self, \
		gil_word argc, gil_word *argv) { \
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
		if (!(lhs->real.real op rhs->real.real)) { \
			return vm->kfalse; \
		} \
		lhs = rhs; \
	} \
	return vm->ktrue; \
}
X(builtin_lt, <)
X(builtin_lteq, <=)
X(builtin_gt, >)
X(builtin_gteq, >=)
#undef X

static gil_word builtin_land(
		struct gil_vm *vm, gil_word mid, gil_word self,
		gil_word argc, gil_word *argv) {
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

static gil_word builtin_lor(
		struct gil_vm *vm, gil_word mid, gil_word self,
		gil_word argc, gil_word *argv) {
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

static gil_word builtin_first(
		struct gil_vm *vm, gil_word mid, gil_word self,
		gil_word argc, gil_word *argv) {
	for (gil_word i = 0; i < argc; ++i) {
		if (gil_value_get_type(&vm->values[argv[i]]) != GIL_VAL_TYPE_NONE) {
			return argv[i];
		}
	}

	return vm->knone;
}

static gil_word builtin_print(
		struct gil_vm *vm, gil_word mid, gil_word self,
		gil_word argc, gil_word *argv) {
	for (size_t i = 0; i < argc; ++i) {
		if (i != 0) {
			vm->std_output->write(vm->std_output, " ", 1);
		}

		struct gil_vm_value *val = &vm->values[argv[i]];
		print_val(vm, vm->std_output, val, 0);
	}

	vm->std_output->write(vm->std_output, "\n", 1);
	return vm->knone;
}

static gil_word builtin_write(
		struct gil_vm *vm, gil_word mid, gil_word self,
		gil_word argc, gil_word *argv) {
	for (size_t i = 0; i < argc; ++i) {
		struct gil_vm_value *val = &vm->values[argv[i]];
		print_val(vm, vm->std_output, val, 0);
	}

	return vm->knone;
}

gil_word builtin_len(
		struct gil_vm *vm, gil_word mid, gil_word self,
		gil_word argc, gil_word *argv) {
	if (argc != 1) {
		return gil_vm_error(vm, "Expected 1 argument");
	}

	gil_word ret_id = gil_vm_alloc(vm, GIL_VAL_TYPE_REAL, 0);
	struct gil_vm_value *ret = &vm->values[ret_id];
	ret->real.real = 0;

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
		ret->real.real = val->buffer.length;
		break;

	case GIL_VAL_TYPE_ARRAY:
		ret->real.real = val->array.length;
		break;

	case GIL_VAL_TYPE_NAMESPACE:
		if (val->ns.ns) {
			ret->real.real = val->ns.ns->len;
		}
		break;
	}

	return ret_id;
}

static gil_word builtin_if(
		struct gil_vm *vm, gil_word mid, gil_word self,
		gil_word argc, gil_word *argv) {
	if (argc != 2 && argc != 3) {
		return gil_vm_error(vm, "Expected 2 or 3 arguments");
	}

	if (gil_vm_val_is_true(vm, &vm->values[argv[0]])) {
		gil_word ret_id = gil_vm_alloc(vm, GIL_VAL_TYPE_CONTINUATION, 0);
		struct gil_vm_value *ret = &vm->values[ret_id];
		ret->cont.call = argv[1];
		return ret_id;
	} else if (argc == 3) {
		gil_word ret_id = gil_vm_alloc(vm, GIL_VAL_TYPE_CONTINUATION, 0);
		struct gil_vm_value *ret = &vm->values[ret_id];
		ret->cont.call = argv[2];
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
			val->atom.atom == vm->values[vm->kstop].atom.atom) {
		return vm->knone;
	} else {
		return cont;
	}
}

static void loop_marker(
		struct gil_vm *vm, void *data, int depth,
		void (*mark)(struct gil_vm *vm, gil_word id, int depth)) {
	struct loop_context *ctx = data;
	mark(vm, ctx->func, depth + 1);
}

static gil_word builtin_loop(
		struct gil_vm *vm, gil_word mid, gil_word self,
		gil_word argc, gil_word *argv) {
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
	cont->cont.call = ctx->func;
	cont->cont.cont = &ctx->base;
	return cont_id;
}

struct while_context {
	struct gil_vm_contcontext base;
	gil_word cond, body;
};

static gil_word while_callback(struct gil_vm *vm, gil_word retval, gil_word cont_id) {
	struct gil_vm_value *cont = &vm->values[cont_id];
	struct while_context *ctx = (struct while_context *)cont->cont.cont;
	struct gil_vm_value *ret = &vm->values[retval];

	if (gil_value_get_type(ret) == GIL_VAL_TYPE_ERROR) {
		return retval;
	}

	if (cont->cont.call == ctx->cond) {
		if (gil_vm_val_is_true(vm, ret)) {
			cont->cont.call = ctx->body;
			return cont_id;
		} else {
			return vm->knone;
		}
	} else {
		if (gil_value_get_type(ret) == GIL_VAL_TYPE_ERROR) {
			return retval;
		} else {
			cont->cont.call = ctx->cond;
			return cont_id;
		}
	}
}

static void while_marker(
		struct gil_vm *vm, void *data, int depth,
		void (*mark)(struct gil_vm *vm, gil_word id, int depth)) {
	struct while_context *ctx = data;
	mark(vm, ctx->cond, depth + 1);
	mark(vm, ctx->body, depth + 1);
}

static gil_word builtin_while(
		struct gil_vm *vm, gil_word mid, gil_word self,
		gil_word argc, gil_word *argv) {
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
	cont->cont.call = ctx->cond;
	cont->cont.cont = &ctx->base;
	return cont_id;
}

struct for_context {
	struct gil_vm_contcontext base;
	gil_word iter;
	gil_word func;
};

static gil_word for_callback(struct gil_vm *vm, gil_word retval, gil_word cont_id) {
	struct gil_vm_value *cont = &vm->values[cont_id];
	struct for_context *ctx = (struct for_context *)cont->cont.cont;
	struct gil_vm_value *ret = &vm->values[retval];

	if (gil_value_get_type(ret) == GIL_VAL_TYPE_ERROR) {
		return retval;
	}

	struct gil_vm_value *args = &vm->values[cont->cont.cont->args];
	if (cont->cont.call == ctx->iter) {
		if (
				gil_value_get_type(ret) == GIL_VAL_TYPE_ATOM &&
				ret->atom.atom == vm->values[vm->kstop].atom.atom) {
			return vm->knone;
		} else {
			cont->cont.call = ctx->func;
			args->array.length = 1;
			args->array.shortarray[0] = retval;
			return cont_id;
		}
	} else {
		cont->cont.call = ctx->iter;
		args->array.length = 0;
		return cont_id;
	}
}

static void for_marker(
		struct gil_vm *vm, void *data, int depth,
		void (*mark)(struct gil_vm *vm, gil_word id, int depth)) {
	struct for_context *ctx = data;
	mark(vm, ctx->iter, depth + 1);
	mark(vm, ctx->func, depth + 1);
}

static gil_word builtin_for(
		struct gil_vm *vm, gil_word mid, gil_word self,
		gil_word argc, gil_word *argv) {
	if (argc != 2) {
		return gil_vm_error(vm, "Expected 2 arguments");
	}

	gil_word args_id = gil_vm_alloc(vm, GIL_VAL_TYPE_ARRAY, GIL_VAL_SBO);
	struct gil_vm_value *args = &vm->values[args_id];
	args->array.length = 0;

	struct for_context *ctx = malloc(sizeof(*ctx));
	ctx->base.callback = for_callback;
	ctx->base.marker = for_marker;
	ctx->base.args = args_id;
	ctx->iter = argv[0];
	ctx->func = argv[1];

	gil_word cont_id = gil_vm_alloc(vm, GIL_VAL_TYPE_CONTINUATION, 0);
	struct gil_vm_value *cont = &vm->values[cont_id];
	cont->cont.call = ctx->iter;
	cont->cont.cont = &ctx->base;
	return cont_id;
}

static gil_word guard_callback(struct gil_vm *vm, gil_word retval, gil_word cont_id) {
	struct gil_vm_value *ret = &vm->values[cont_id];
	free(ret->cont.cont);
	ret->flags = GIL_VAL_TYPE_RETURN;
	ret->ret.ret = retval;
	return cont_id;
}

static gil_word builtin_guard(
		struct gil_vm *vm, gil_word mid, gil_word self,
		gil_word argc, gil_word *argv) {
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
		vm->values[ret_id].ret.ret = vm->knone;
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
	cont->cont.call = argv[1];
	cont->cont.cont = ctx;
	return cont_id;
}

static void init(
		struct gil_module *ptr,
		gil_word (*alloc)(void *data, const char *name), void *data) {
	struct gil_mod_builtins *mod = (struct gil_mod_builtins *)ptr;
	mod->kadd = alloc(data, "+");
	mod->ksub = alloc(data, "-");
	mod->kmul = alloc(data, "*");
	mod->kdiv = alloc(data, "/");
	mod->keq = alloc(data, "==");
	mod->kneq = alloc(data, "!=");
	mod->klt = alloc(data, "<");
	mod->klteq = alloc(data, "<=");
	mod->kgt = alloc(data, ">");
	mod->kgteq = alloc(data, ">=");
	mod->kland = alloc(data, "&&");
	mod->klor = alloc(data, "||");
	mod->kfirst = alloc(data, "??");
	mod->kprint = alloc(data, "print");
	mod->kwrite = alloc(data, "write");
	mod->klen = alloc(data, "len");
	mod->kif = alloc(data, "if");
	mod->kloop = alloc(data, "loop");
	mod->kwhile = alloc(data, "while");
	mod->kfor = alloc(data, "for");
	mod->kguard = alloc(data, "guard");
}

static gil_word create(struct gil_module *ptr, struct gil_vm *vm, gil_word mid) {
	struct gil_mod_builtins *mod = (struct gil_mod_builtins *)ptr;

	gil_word id = gil_vm_alloc(vm, GIL_VAL_TYPE_NAMESPACE, 0);
	struct gil_vm_value *ns = &vm->values[id];
	ns->ns.parent = 0;
	ns->ns.ns = NULL;

	gil_vm_namespace_set(ns, mod->kadd,
			gil_vm_make_cfunction(vm, builtin_add, mid));
	gil_vm_namespace_set(ns, mod->ksub,
			gil_vm_make_cfunction(vm, builtin_sub, mid));
	gil_vm_namespace_set(ns, mod->kmul,
			gil_vm_make_cfunction(vm, builtin_mul, mid));
	gil_vm_namespace_set(ns, mod->kdiv,
			gil_vm_make_cfunction(vm, builtin_div, mid));
	gil_vm_namespace_set(ns, mod->keq,
			gil_vm_make_cfunction(vm, builtin_eq, mid));
	gil_vm_namespace_set(ns, mod->kneq,
			gil_vm_make_cfunction(vm, builtin_neq, mid));
	gil_vm_namespace_set(ns, mod->klt,
			gil_vm_make_cfunction(vm, builtin_lt, mid));
	gil_vm_namespace_set(ns, mod->klteq,
			gil_vm_make_cfunction(vm, builtin_lteq, mid));
	gil_vm_namespace_set(ns, mod->kgt,
			gil_vm_make_cfunction(vm, builtin_gt, mid));
	gil_vm_namespace_set(ns, mod->kgteq,
			gil_vm_make_cfunction(vm, builtin_gteq, mid));
	gil_vm_namespace_set(ns, mod->kland,
			gil_vm_make_cfunction(vm, builtin_land, mid));
	gil_vm_namespace_set(ns, mod->klor,
			gil_vm_make_cfunction(vm, builtin_lor, mid));
	gil_vm_namespace_set(ns, mod->kfirst,
			gil_vm_make_cfunction(vm, builtin_first, mid));
	gil_vm_namespace_set(ns, mod->kprint,
			gil_vm_make_cfunction(vm, builtin_print, mid));
	gil_vm_namespace_set(ns, mod->kwrite,
			gil_vm_make_cfunction(vm, builtin_write, mid));
	gil_vm_namespace_set(ns, mod->klen,
			gil_vm_make_cfunction(vm, builtin_len, mid));
	gil_vm_namespace_set(ns, mod->kif,
			gil_vm_make_cfunction(vm, builtin_if, mid));
	gil_vm_namespace_set(ns, mod->kloop,
			gil_vm_make_cfunction(vm, builtin_loop, mid));
	gil_vm_namespace_set(ns, mod->kwhile,
			gil_vm_make_cfunction(vm, builtin_while, mid));
	gil_vm_namespace_set(ns, mod->kfor,
			gil_vm_make_cfunction(vm, builtin_for, mid));
	gil_vm_namespace_set(ns, mod->kguard,
			gil_vm_make_cfunction(vm, builtin_guard, mid));

	return id;
}

static void marker(
		struct gil_module *ptr, struct gil_vm *vm,
		void (*mark)(struct gil_vm *vm, gil_word id)) {
}

void gil_mod_builtins_init(struct gil_mod_builtins *mod) {
	mod->base.name = "fs";
	mod->base.init = init;
	mod->base.create = create;
	mod->base.marker = marker;
}
