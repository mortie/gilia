#include "vm/vm.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "bitset.h"
#include "bytecode.h"
#include "io.h"
#include "module.h"
#include "strset.h"

#ifdef GIL_ENABLE_TRACE
#include "vm/print.h"
#endif

#define GIL_TRACER_NAME "vm"
#include "trace.h"

static int stdio_inited = 0;
static struct gil_io_file_writer std_output;
static struct gil_io_file_writer std_error;

static gil_word alloc_val(struct gil_vm *vm) {
	// The idea here is:
	// * If there are less than 32 slots left, trigger a GC.
	// * If there are less than 16 slots left, realloc.
	// * If the realloc failed, halt the vm. This gives us some runway,
	//   because things may continue to allocate and use values
	//   after the alloc_val and before we get back to the main loop.
	// When we move to a generational GC,
	// all of this logic has to be rewritten.
	size_t id = gil_bitset_set_next(&vm->valueset);
	if (id + 32 >= vm->valuessize) {
		if (id + 16 >= vm->valuessize) {
			size_t valuessize = vm->valuessize;
			while (id + 16 >= valuessize) {
				valuessize *= 2;
			}

			struct gil_vm_value *newvalues = realloc(
					vm->values, sizeof(*vm->values) * valuessize);
			if (newvalues != NULL) {
				vm->values = newvalues;
				vm->valuessize = valuessize;
			} else if (!vm->halted) {
				gil_io_printf(vm->std_error, "Allocation failure\n");
				vm->halted = 1;
			} else if (id == vm->valuessize) {
				// This is bad. If we get here more than once,
				// we may leak memory for real.
				gil_io_printf(vm->std_error, "Critical allocation failure!\n");
				id = vm->knone;
			}
		}

		vm->need_gc = 1;
	}

	return (gil_word)id;
}

static void gc_mark_array(struct gil_vm *vm, struct gil_vm_value *val, int depth);
static void gc_mark_namespace(struct gil_vm *vm, struct gil_vm_value *val, int depth);

static void gc_mark(struct gil_vm *vm, gil_word id, int depth) {
	struct gil_vm_value *val = &vm->values[id];
	if (val->flags & GIL_VAL_MARKED) {
		return;
	}

	if (depth > GIL_MAX_STACK_DEPTH) {
		if (!vm->halted) {
			gil_io_printf(vm->std_error, "GC recursion limit reached\n");
			vm->halted = 1;
		}

		return;
	}

	val->flags |= GIL_VAL_MARKED;

	int typ = gil_value_get_type(val);
	if (typ == GIL_VAL_TYPE_ARRAY) {
		gc_mark_array(vm, val, depth + 1);
	} else if (typ == GIL_VAL_TYPE_NAMESPACE) {
		gc_mark_namespace(vm, val, depth + 1);
	} else if (typ == GIL_VAL_TYPE_FUNCTION) {
		gc_mark(vm, val->func.ns, depth + 1);
		gc_mark(vm, val->func.self, depth + 1);
	} else if (typ == GIL_VAL_TYPE_CFUNCTION) {
		gc_mark(vm, val->cfunc.self, depth + 1);
	} else if (typ == GIL_VAL_TYPE_CONTINUATION) {
		gc_mark(vm, val->cont.call, depth + 1);
		if (val->cont.cont != NULL) {
			if (val->cont.cont->marker != NULL) {
				val->cont.cont->marker(vm, val->cont.cont, depth + 1, gc_mark);
			}
			if (val->cont.cont->args != 0) {
				gc_mark(vm, val->cont.cont->args, depth + 1);
			}
		}
	}
}

static void gc_mark_array(struct gil_vm *vm, struct gil_vm_value *val, int depth) {
	gil_word *data;
	if (val->flags & GIL_VAL_SBO) {
		data = val->array.shortarray;
	} else {
		data = val->array.array->data;
	}

	for (size_t i = 0; i < val->array.length; ++i) {
		gc_mark(vm, data[i], depth + 1);
	}
}

static void gc_mark_namespace(struct gil_vm *vm, struct gil_vm_value *val, int depth) {
	if (val->ns.parent != 0) {
		gc_mark(vm, val->ns.parent, depth + 1);
	}

	if (val->ns.ns == NULL) {
		return;
	}

	for (size_t i = 0; i < val->ns.ns->size; ++i) {
		gil_word key = val->ns.ns->data[i];
		if (key == 0 || key == ~(gil_word)0) {
			continue;
		}

		gc_mark(vm, val->ns.ns->data[val->ns.ns->size + i], depth + 1);
	}
}

static void gc_mark_base(struct gil_vm *vm, gil_word id) {
	gc_mark(vm, id, 0);
}

static void gc_free(struct gil_vm *vm, gil_word id) {
	struct gil_vm_value *val = &vm->values[id];
	gil_bitset_unset(&vm->valueset, id);

	// Don't need to do anything more; the next round of GC will free
	// whichever values were only referenced by the array
	int typ = gil_value_get_type(val);
	if (typ == GIL_VAL_TYPE_ARRAY && !(val->flags & GIL_VAL_SBO)) {
		free(val->array.array);
	} else if (typ == GIL_VAL_TYPE_BUFFER) {
		free(val->buffer.buffer);
	} else if (typ == GIL_VAL_TYPE_NAMESPACE) {
		free(val->ns.ns);
	} else if (typ == GIL_VAL_TYPE_ERROR) {
		free(val->error.error);
	} else if (typ == GIL_VAL_TYPE_CONTINUATION && val->cont.cont) {
		free(val->cont.cont);
	}
}

static size_t gc_sweep(struct gil_vm *vm) {
	size_t freed = 0;
	struct gil_bitset_iterator it;
	gil_bitset_iterator_init_from(&it, &vm->valueset, vm->gc_start);
	size_t id;
	while (gil_bitset_iterator_next(&it, &vm->valueset, &id)) {
		struct gil_vm_value *val = &vm->values[id];
		if (!(val->flags & GIL_VAL_MARKED)) {
			gil_bitset_unset(&vm->valueset, id);
			gc_free(vm, id);
			freed += 1;
		} else {
			val->flags &= ~GIL_VAL_MARKED;
		}
	}

	return freed;
}

const char *gil_value_type_name(enum gil_value_type typ) {
	switch (typ) {
	case GIL_VAL_TYPE_NONE: return "NONE";
	case GIL_VAL_TYPE_ATOM: return "ATOM";
	case GIL_VAL_TYPE_REAL: return "REAL";
	case GIL_VAL_TYPE_BUFFER: return "BUFFER";
	case GIL_VAL_TYPE_ARRAY: return "ARRAY";
	case GIL_VAL_TYPE_NAMESPACE: return "NAMESPACE";
	case GIL_VAL_TYPE_FUNCTION: return "FUNCTION";
	case GIL_VAL_TYPE_CFUNCTION: return "CFUNCTION";
	case GIL_VAL_TYPE_CVAL: return "CVAL";
	case GIL_VAL_TYPE_CONTINUATION: return "CONTINUATION";
	case GIL_VAL_TYPE_RETURN: return "RETURN";
	case GIL_VAL_TYPE_ERROR: return "ERROR";
	}

	return "(unknown)";
}

gil_word *gil_vm_array_data(struct gil_vm *vm, struct gil_vm_value *val) {
	if (val->flags & GIL_VAL_SBO) {
		return val->array.shortarray;
	} else if (val->array.array) {
		return val->array.array->data;
	} else {
		return NULL;
	}
}

gil_word gil_vm_array_get(struct gil_vm *vm, struct gil_vm_value *val, gil_word k) {
	if (k >= val->array.length) {
		return vm->knone;
	}

	if (val->flags & GIL_VAL_SBO) {
		return val->array.shortarray[k];
	}

	return val->array.array->data[k];
}

gil_word gil_vm_array_set(struct gil_vm *vm, struct gil_vm_value *val, gil_word k, gil_word v) {
	if (k >= val->array.length) {
		return gil_vm_error(vm, "Array index out of bounds");
	}

	if (val->flags & GIL_VAL_SBO) {
		return val->array.shortarray[k] = v;
	}

	return val->array.array->data[k] = v;
}

static gil_word mod_init_alloc(void *ptr, const char *name) {
	struct gil_vm *vm = ptr;
	return gil_strset_put_copy(&vm->atomset, name);
}

void gil_vm_init(
		struct gil_vm *vm, unsigned char *ops, size_t opslen, struct gil_module *builtins) {
	if (!stdio_inited) {
		std_output.w.write = gil_io_file_write;
		std_output.f = stdout;
		std_error.w.write = gil_io_file_write;
		std_error.f = stderr;
		stdio_inited = 1;
	}

	vm->std_output = &std_output.w;
	vm->std_error = &std_error.w;

	vm->halted = 0;
	vm->need_gc = 0;
	vm->need_check_retval = 0;
	vm->ops = ops;
	vm->opslen = opslen;
	vm->iptr = 0;
	vm->sptr = 0;
	vm->fsptr = 0;

	vm->valuessize = 128;
	vm->values = malloc(sizeof(*vm->values) * vm->valuessize);
	if (vm->values == NULL) {
		gil_io_printf(vm->std_error, "Allocation failure\n");
		vm->halted = 1;
		return;
	}
	gil_bitset_init(&vm->valueset);

	// It's wasteful to allocate new 'none' variables all the time,
	// variable ID 0 should be the only 'none' variable in the system
	gil_word none_id = alloc_val(vm);
	assert(none_id == 0);
	vm->values[none_id].flags = GIL_VAL_TYPE_NONE | GIL_VAL_CONST;
	vm->knone = none_id;
	vm->gc_start = 1;

	gil_strset_init(&vm->atomset);

	vm->next_ctype = 1;
	vm->cmodules = NULL;
	vm->cmoduleslen = 0;

	vm->modules = NULL;
	vm->moduleslen = 0;

#define X(name, k) \
		vm->k = alloc_val(vm); \
		vm->values[vm->k].flags = GIL_VAL_TYPE_ATOM | GIL_VAL_CONST; \
		vm->values[vm->k].atom.atom = gil_strset_put_copy(&vm->atomset, name); \
		vm->gc_start += 1;
	GIL_BYTECODE_ATOMS
#undef X

	builtins->init(builtins, mod_init_alloc, vm);
	gil_word builtins_id = builtins->create(builtins, vm, 0);

	// Need to allocate a frame stack for the builtins
	vm->fstack[vm->fsptr].ns = builtins_id;
	vm->fstack[vm->fsptr].retptr = 0;
	vm->fstack[vm->fsptr].sptr = 0;
	vm->fstack[vm->fsptr].args = 0;
	vm->fsptr += 1;

	// Need to allocate a root namespace
	gil_word root = alloc_val(vm);
	vm->values[root].ns.parent = builtins_id;
	vm->values[root].ns.ns = NULL;
	vm->values[root].flags = GIL_VAL_TYPE_NAMESPACE;
	vm->fstack[vm->fsptr].ns = root;
	vm->fstack[vm->fsptr].retptr = ~(gil_word)0;
	vm->fstack[vm->fsptr].sptr = 0;
	vm->fstack[vm->fsptr].args = 0;
	vm->fsptr += 1;
}

void gil_vm_register_module(struct gil_vm *vm, struct gil_module *mod) {
	gil_word id = gil_strset_put_copy(&vm->atomset, mod->name);
	mod->init(mod, mod_init_alloc, vm);

	vm->cmoduleslen += 1;
	vm->cmodules = realloc(vm->cmodules, vm->cmoduleslen * sizeof(*vm->cmodules));
	struct gil_vm_cmodule *cmod = &vm->cmodules[vm->cmoduleslen - 1];
	cmod->id = id;
	cmod->ns = 0;
	cmod->mod = mod;
}

gil_word gil_vm_alloc(struct gil_vm *vm, enum gil_value_type typ, enum gil_value_flags flags) {
	gil_word id = alloc_val(vm);
	memset(&vm->values[id], 0, sizeof(vm->values[id]));
	vm->values[id].flags = typ | flags;
	return id;
}

gil_word gil_vm_alloc_ctype(struct gil_vm *vm) {
	return vm->next_ctype++;
}

gil_word gil_vm_error(struct gil_vm *vm, const char *fmt, ...) {
	gil_word id = alloc_val(vm);
	struct gil_vm_value *val = &vm->values[id];
	val->flags = GIL_VAL_CONST | GIL_VAL_TYPE_ERROR;

	char buf[256];

	va_list va;
	va_start(va, fmt);
	int n = vsnprintf(buf, sizeof(buf), fmt, va);

	if (n < 0) {
		const char *message = "Failed to generate error message!";
		val->error.error = malloc(strlen(message) + 1);
		if (val->error.error == NULL) {
			gil_io_printf(vm->std_error, "Allocation failure\n");
			vm->halted = 1;
			va_end(va);
			return vm->knone;
		}

		strcpy(val->error.error, message);
		va_end(va);
		return id;
	} else if ((size_t)n + 1 < sizeof(buf)) {
		val->error.error = malloc(n + 1);
		if (val->error.error == NULL) {
			gil_io_printf(vm->std_error, "Allocation failure\n");
			vm->halted = 1;
			va_end(va);
			return vm->knone;
		}

		strcpy(val->error.error, buf);
		va_end(va);
		return id;
	}

	val->error.error = malloc(n + 1);
	if (val->error.error == NULL) {
		gil_io_printf(vm->std_error, "Allocation failure\n");
		vm->halted = 1;
		va_end(va);
		return vm->knone;
	}

	vsnprintf(val->error.error, n + 1, fmt, va);
	va_end(va);
	return id;
}

gil_word gil_vm_type_error(struct gil_vm *vm, struct gil_vm_value *val) {
	enum gil_value_type typ = gil_value_get_type(val);
	if (typ == GIL_VAL_TYPE_ERROR) {
		return val - vm->values;
	}

	return gil_vm_error(vm, "Unexpected type %s", gil_value_type_name(gil_value_get_type(val)));
}

void gil_vm_free(struct gil_vm *vm) {
	struct gil_bitset_iterator it;
	gil_bitset_iterator_init(&it, &vm->valueset);
	size_t id;
	while (gil_bitset_iterator_next(&it, &vm->valueset, &id)) {
		gc_free(vm, id);
	}

	free(vm->values);
	gil_bitset_free(&vm->valueset);
	gil_strset_free(&vm->atomset);
	free(vm->cmodules);
}

size_t gil_vm_gc(struct gil_vm *vm) {
	for (gil_word sptr = 0; sptr < vm->sptr; ++sptr) {
		gc_mark_base(vm, vm->stack[sptr]);
	}

	for (gil_word fsptr = 0; fsptr < vm->fsptr; ++fsptr) {
		gc_mark_base(vm, vm->fstack[fsptr].ns);
		gc_mark_base(vm, vm->fstack[fsptr].args);
	}

	// Mark for all loaded modules
	for (size_t i = 0; i < vm->cmoduleslen; ++i) {
		if (vm->cmodules[i].ns) {
			gc_mark_base(vm, vm->cmodules[i].ns);
			vm->cmodules[i].mod->marker(vm->cmodules[i].mod, vm, gc_mark_base);
		}
	}

	return gc_sweep(vm);
}

void gil_vm_run(struct gil_vm *vm) {
	while (!vm->halted) {
		gil_vm_step(vm);
	}
}

static void call_func_with_args(
		struct gil_vm *vm, gil_word func_id, gil_word args_id);
static void call_func(
		struct gil_vm *vm, gil_word func_id,
		gil_word argc, gil_word *argv);

static void after_cfunc_return(struct gil_vm *vm) {
	if (
			gil_value_get_type(&vm->values[vm->stack[vm->sptr - 1]]) ==
				GIL_VAL_TYPE_RETURN ||
			gil_value_get_type(&vm->values[vm->stack[vm->sptr - 1]]) ==
				GIL_VAL_TYPE_CONTINUATION ||
			(vm->sptr >= 2 &&
				gil_value_get_type(&vm->values[vm->stack[vm->sptr - 2]]) ==
					GIL_VAL_TYPE_CONTINUATION)) {
		vm->need_check_retval = 1;
	}
}

static void after_func_return(struct gil_vm *vm) {
	struct gil_vm_value *ret = &vm->values[vm->stack[vm->sptr - 1]];

	if (gil_value_get_type(ret) == GIL_VAL_TYPE_RETURN) {
		gil_word retval = ret->ret.ret;
		gil_word retptr = vm->fstack[vm->fsptr - 1].retptr;
		gil_word sptr = vm->fstack[vm->fsptr - 1].sptr;
		if (retptr == ~(gil_word)0) {
			vm->halted = 1;
			return;
		}

		vm->fsptr -= 1;
		vm->sptr = sptr;
		vm->iptr = retptr;
		vm->stack[vm->sptr++] = retval;

		after_func_return(vm);
		return;
	}

	// If the function returns a continuation, we leave that continuation
	// on the stack to be handled later, then call the function
	if (gil_value_get_type(ret) == GIL_VAL_TYPE_CONTINUATION) {
		if (ret->cont.cont && ret->cont.cont->args != vm->knone) {
			struct gil_vm_value *args = &vm->values[ret->cont.cont->args];
			struct gil_vm_value *func = &vm->values[ret->cont.call];

			// We can optimize calling a non-C function by re-using the
			// args array rather than allocating a new one
			if (gil_value_get_type(func) == GIL_VAL_TYPE_FUNCTION) {
				call_func_with_args(vm, ret->cont.call, ret->cont.cont->args);
			} else {
				call_func(
						vm, ret->cont.call, args->array.length,
						gil_vm_array_data(vm, args));
			}
		} else {
			call_func(vm, ret->cont.call, 0, NULL);
		}

		return;
	}

	// If the value right below the returned value is a continuation,
	// it's a continuation left on the stack to be handled later - i.e now
	if (
			vm->sptr >= 2 &&
			gil_value_get_type(&vm->values[vm->stack[vm->sptr - 2]]) ==
				GIL_VAL_TYPE_CONTINUATION) {
		struct gil_vm_value *cont = &vm->values[vm->stack[vm->sptr - 2]];

		// If it's just a basic continuation, don't need to do anything
		if (cont->cont.cont == NULL || cont->cont.cont->callback == NULL) {
			// Return the return value of the function, discard the continuation
			vm->stack[vm->sptr - 2] = vm->stack[vm->sptr - 1];
			vm->sptr -= 1;
			return;
		}

		// After this, the original return value and the continuation
		// are both replaced by whatever the callback returned
		gil_word contret = cont->cont.cont->callback(
				vm, vm->stack[vm->sptr - 1], vm->stack[vm->sptr - 2]);
		vm->stack[vm->sptr - 2] = contret;
		vm->sptr -= 1;

		after_func_return(vm);
		return;
	}
}

static void call_func_with_args(struct gil_vm *vm, gil_word func_id, gil_word args_id) {
	gil_word ns_id = alloc_val(vm);
	struct gil_vm_value *func = &vm->values[func_id]; // func might be stale after alloc_val
	vm->values[ns_id].ns.parent = func->func.ns;
	vm->values[ns_id].ns.ns = NULL;
	vm->values[ns_id].flags = GIL_VAL_TYPE_NAMESPACE;
	vm->fstack[vm->fsptr].ns = ns_id;
	vm->fstack[vm->fsptr].retptr = vm->iptr;
	vm->fstack[vm->fsptr].sptr = vm->sptr;
	vm->fstack[vm->fsptr].args = args_id;
	vm->fsptr += 1;

	vm->iptr = func->func.pos;

	// We need one value as a buffer between the previous stack frame and the new.
	// This is mostly due to the way continuations are handled.
	// This wouldn't be necessary if continuations were stored in the stack
	// frame struct rather than as a value on the stack.
	vm->stack[vm->sptr++] = vm->knone;
}

// The 'call_func' function assumes that all relevant values have been popped off
// the stack, so that the return value can be pushed to the top of the stack
// straight away
static void call_func(
		struct gil_vm *vm, gil_word func_id,
		gil_word argc, gil_word *argv) {
	struct gil_vm_value *func = &vm->values[func_id];
	enum gil_value_type typ = gil_value_get_type(func);

	// C functions are called differently from language functions
	if (typ == GIL_VAL_TYPE_CFUNCTION) {
		vm->stack[vm->sptr++] = func->cfunc.func(
				vm, func->cfunc.mod, func->cfunc.self, argc, argv);
		after_cfunc_return(vm);
		return;
	}

	// Don't interpret a non-function as a function
	if (typ != GIL_VAL_TYPE_FUNCTION) {
		vm->stack[vm->sptr++] = gil_vm_error(vm, "Attempt to call non-function");
		return;
	}

	gil_word args_id = alloc_val(vm);
	struct gil_vm_value *args = &vm->values[args_id];
	args->flags = GIL_VAL_TYPE_ARRAY | GIL_VAL_SBO;
	args->array.length = argc;
	if (argc > 0) {
		if (argc <= 2) {
			memcpy(args->array.shortarray, argv, argc * sizeof(gil_word));
		} else {
			args->flags = GIL_VAL_TYPE_ARRAY;
			args->array.array = malloc(
					sizeof(struct gil_vm_array) + sizeof(gil_word) * argc);
			if (args->array.array == NULL) {
				gil_io_printf(vm->std_error, "Allocation failure\n");
				vm->stack[vm->sptr++] = vm->knone;
				vm->halted = 1;
				return;
			}

			args->array.array->size = argc;
			memcpy(args->array.array->data, argv, argc * sizeof(gil_word));
		}
	}

	call_func_with_args(vm, func_id, args_id);
}

static gil_word read_uint(struct gil_vm *vm) {
	gil_word word = 0;
	while (vm->ops[vm->iptr] >= 0x80) {
		word |= vm->ops[vm->iptr++] & 0x7f;
		word <<= 7;
	}

	word |= vm->ops[vm->iptr++];
	return word;
}

static gil_word read_u4le(struct gil_vm *vm) {
	unsigned char *data = &vm->ops[vm->iptr];
	gil_word integer = 0 |
		(uint64_t)data[0] |
		(uint64_t)data[1] << 8 |
		(uint64_t)data[2] << 16 |
		(uint64_t)data[3] << 24;

	vm->iptr += 4;
	return integer;
}

static double read_d8le(struct gil_vm *vm) {
	unsigned char *data = &vm->ops[vm->iptr];
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
	vm->iptr += 8;
	return num;
}

void gil_vm_step(struct gil_vm *vm) {
	if (vm->need_check_retval) {
		gil_trace("check retval");
		vm->need_check_retval = 0;
		after_func_return(vm);

		if (vm->need_gc) {
			gil_trace("GC");
			vm->need_gc = 0;
			gil_vm_gc(vm);
		}

		return;
	}

#ifdef GIL_ENABLE_TRACE
	if (gil_tracer_enabled()) {
		struct gil_io_mem_writer w = {
			.w.write = gil_io_mem_write,
		};
		size_t iptr = vm->iptr;
		gil_vm_print_op(&w.w, vm->ops, vm->opslen, &iptr);
		w.w.write(&w.w, "\0", 1);
		gil_trace("%zi: %s", vm->iptr, w.mem);
		free(w.mem);
	}
#endif

	if (
			vm->sptr > (sizeof(vm->stack) / sizeof(*vm->stack)) - 32 ||
			vm->fsptr > (sizeof(vm->fstack) / sizeof(*vm->fstack)) - 32) {
		gil_io_printf(vm->std_error, "Stack overflow\n");
		vm->halted = 1;
		return;
	}

	enum gil_opcode opcode = (enum gil_opcode)vm->ops[vm->iptr++];

	gil_word word;
	switch (opcode) {
	case GIL_OP_NOP:
		break;

	case GIL_OP_DISCARD:
		vm->sptr -= 1;
		if (gil_value_get_type(&vm->values[vm->stack[vm->sptr]]) == GIL_VAL_TYPE_ERROR) {
			gil_io_printf(
					vm->std_error, "Error: %s\n",
					vm->values[vm->stack[vm->sptr]].error.error);
			vm->halted = 1;
		}
		break;

	case GIL_OP_SWAP_DISCARD:
		vm->stack[vm->sptr - 2] = vm->stack[vm->sptr - 1];
		vm->sptr -= 1;
		if (gil_value_get_type(&vm->values[vm->stack[vm->sptr]]) == GIL_VAL_TYPE_ERROR) {
			gil_io_printf(
					vm->std_error, "Error: %s\n",
					vm->values[vm->stack[vm->sptr]].error.error);
			vm->halted = 1;
		}
		break;

	case GIL_OP_FUNC_CALL: {
		gil_word argc = read_uint(vm);
		vm->sptr -= argc;
		gil_word *argv = vm->stack + vm->sptr;
		gil_word func_id = vm->stack[--vm->sptr];
		call_func(vm, func_id, argc, argv);
	}
		break;

	case GIL_OP_RJMP:
		word = read_uint(vm);
		vm->iptr += word;
		break;

	case GIL_OP_RJMP_U4:
		word = read_u4le(vm);
		vm->iptr += word;
		break;

	case GIL_OP_STACK_FRAME_GET_ARGS:
		vm->stack[vm->sptr++] = vm->fstack[vm->fsptr - 1].args;
		break;

	case GIL_OP_STACK_FRAME_GET_ARG: {
		gil_word idx = read_uint(vm);
		struct gil_vm_value *args = &vm->values[vm->fstack[vm->fsptr - 1].args];
		if (idx < args->array.length) {
			if (args->flags & GIL_VAL_SBO) {
				vm->stack[vm->sptr++] = args->array.shortarray[idx];
			} else {
				vm->stack[vm->sptr++] = args->array.array->data[idx];
			}
		} else {
			vm->stack[vm->sptr++] = vm->knone;
		}
	}
		break;

	case GIL_OP_STACK_FRAME_LOOKUP: {
		gil_word key = read_uint(vm);
		struct gil_vm_value *ns = &vm->values[vm->fstack[vm->fsptr - 1].ns];
		vm->stack[vm->sptr++] = gil_vm_namespace_get(vm, ns, key);
	}
		break;

	case GIL_OP_STACK_FRAME_SET: {
		gil_word key = read_uint(vm); \
		gil_word val = vm->stack[vm->sptr - 1]; \
		struct gil_vm_value *ns = &vm->values[vm->fstack[vm->fsptr - 1].ns]; \
		gil_vm_namespace_set(ns, key, val);
	}
		break;

	case GIL_OP_STACK_FRAME_REPLACE: {
		gil_word key = read_uint(vm);
		gil_word val = vm->stack[vm->sptr - 1];
		struct gil_vm_value *ns = &vm->values[vm->fstack[vm->fsptr - 1].ns];
		if (gil_vm_namespace_replace(vm, ns, key, val) < 0) {
			vm->stack[vm->sptr - 1] = gil_vm_error(vm, "Variable not found");
		}
	}
		break;

	case GIL_OP_ASSERT:
		vm->sptr -= 1;
		if (!gil_vm_val_is_true(vm, &vm->values[vm->stack[vm->sptr]])) {
			gil_word retval = gil_vm_error(vm, "Assertion failed");
			gil_word retptr = vm->fstack[vm->fsptr - 1].retptr;
			gil_word sptr = vm->fstack[vm->fsptr - 1].sptr;
			vm->fsptr -= 1;
			vm->sptr = sptr;
			vm->iptr = retptr;
			vm->stack[vm->sptr++] = retval;
		}
		break;

	case GIL_OP_NAMED_PARAM: {
		gil_word key = read_uint(vm);
		gil_word idx = read_uint(vm);
		struct gil_vm_stack_frame *frame = &vm->fstack[vm->fsptr - 1];
		struct gil_vm_value *args = &vm->values[frame->args];
		gil_word val = vm->knone;
		if (idx < args->array.length) {
			if (args->flags & GIL_VAL_SBO) {
				val = args->array.shortarray[idx];
			} else {
				val = args->array.array->data[idx];
			}
		}

		struct gil_vm_value *ns = &vm->values[frame->ns];
		gil_vm_namespace_set(ns, key, val);
	}
		break;

	case GIL_OP_RET: {
		gil_word retval = vm->stack[--vm->sptr];
		gil_word retptr = vm->fstack[vm->fsptr - 1].retptr;
		gil_word sptr = vm->fstack[vm->fsptr - 1].sptr;
		vm->fsptr -= 1;
		vm->sptr = sptr;
		vm->iptr = retptr;
		vm->stack[vm->sptr++] = retval;

		after_func_return(vm);
	}
		break;

	case GIL_OP_MOD_RET: {
		gil_word retptr = vm->fstack[vm->fsptr - 1].retptr;
		gil_word sptr = vm->fstack[vm->fsptr - 1].sptr;
		gil_word ns = vm->fstack[vm->fsptr - 1].ns;
		vm->fsptr -= 1;
		vm->sptr = sptr;
		vm->iptr = retptr;
		vm->stack[vm->sptr++] = ns;
	}
		break;

	case GIL_OP_ALLOC_NONE:
		vm->stack[vm->sptr++] = 0;
		break;

	case GIL_OP_ALLOC_ATOM: {
		word = alloc_val(vm);
		vm->values[word].flags = GIL_VAL_TYPE_ATOM;
		vm->values[word].atom.atom = read_uint(vm);
		vm->stack[vm->sptr++] = word;
	}
		break;

	case GIL_OP_ALLOC_REAL: {
		word = alloc_val(vm);
		vm->values[word].flags = GIL_VAL_TYPE_REAL;
		vm->values[word].real.real = read_d8le(vm);
		vm->stack[vm->sptr++] = word;
	}
		break;

	case GIL_OP_ALLOC_BUFFER_STATIC: {
		word = alloc_val(vm);
		gil_word length = read_uint(vm);
		gil_word offset = read_uint(vm);
		vm->values[word].flags = GIL_VAL_TYPE_BUFFER;
		vm->values[word].buffer.buffer = malloc(length + 1);
		if (vm->values[word].buffer.buffer == NULL) {
			gil_io_printf(vm->std_error, "Allocation failure\n");
			vm->halted = 1;
			break;
		}

		vm->values[word].buffer.length = length;
		memcpy(vm->values[word].buffer.buffer, vm->ops + offset, length);
		vm->values[word].buffer.buffer[length] = '\0';
		vm->stack[vm->sptr] = word;
		vm->sptr += 1;
	}
		break;

	case GIL_OP_ALLOC_ARRAY: {
		gil_word count = read_uint(vm);
		gil_word arr_id = alloc_val(vm);
		struct gil_vm_value *arr = &vm->values[arr_id];
		arr->array.length = count;
		gil_word *data;
		if (count <= 2) {
			arr->flags = GIL_VAL_TYPE_ARRAY | GIL_VAL_SBO;
			data = arr->array.shortarray;
		} else {
			arr->flags = GIL_VAL_TYPE_ARRAY;
			arr->array.array = malloc(sizeof(struct gil_vm_array) + count * sizeof(gil_word));
			if (arr->array.array == NULL) {
				gil_io_printf(vm->std_error, "Allocation failure\n");
				vm->halted = 1;
				break;
			}

			arr->array.array->size = count;
			data = arr->array.array->data;
		}
		for (gil_word i = 0; i < count; ++i) {
			data[count - 1 - i] = vm->stack[--vm->sptr];
		}
		vm->stack[vm->sptr++] = arr_id;
	}
		break;

	case GIL_OP_ALLOC_NAMESPACE:
		word = alloc_val(vm);
		vm->values[word].flags = GIL_VAL_TYPE_NAMESPACE;
		vm->values[word].ns.parent = 0;
		vm->values[word].ns.ns = NULL; // Will be allocated on first insert
		vm->stack[vm->sptr] = word;
		vm->sptr += 1;
		break;

	case GIL_OP_ALLOC_FUNCTION:
		word = alloc_val(vm);
		vm->values[word].flags = GIL_VAL_TYPE_FUNCTION;
		vm->values[word].func.pos = read_uint(vm);
		vm->values[word].func.ns = vm->fstack[vm->fsptr - 1].ns;
		vm->values[word].func.self = 0;
		vm->stack[vm->sptr] = word;
		vm->sptr += 1;
		break;

	case GIL_OP_NAMESPACE_SET: {
		gil_word key = read_uint(vm);
		gil_word val = vm->stack[vm->sptr - 1];
		gil_word ns_id = vm->stack[vm->sptr - 2];
		struct gil_vm_value *ns = &vm->values[ns_id];
		if (gil_value_get_type(ns) == GIL_VAL_TYPE_NAMESPACE) {
			gil_vm_namespace_set(ns, key, val);
		} else {
			vm->stack[vm->sptr - 1] = gil_vm_type_error(vm, ns);
		}
	}
		break;

	case GIL_OP_NAMESPACE_LOOKUP: {
		gil_word key = read_uint(vm);
		gil_word ns_id = vm->stack[--vm->sptr];
		struct gil_vm_value *ns = &vm->values[ns_id];
		if (gil_value_get_type(ns) == GIL_VAL_TYPE_NAMESPACE) {
			vm->stack[vm->sptr++] = gil_vm_namespace_get(vm, ns, key);
		} else if (gil_value_get_type(ns) == GIL_VAL_TYPE_CVAL) {
			ns = &vm->values[ns->cval.ns];
			if (gil_value_get_type(ns) == GIL_VAL_TYPE_NAMESPACE) {
				vm->stack[vm->sptr++] = gil_vm_namespace_get(vm, ns, key);
			} else {
				vm->stack[vm->sptr++] = gil_vm_type_error(vm, ns);
			}
		} else {
			vm->stack[vm->sptr++] = gil_vm_type_error(vm, ns);
		}

		struct gil_vm_value *val = &vm->values[vm->stack[vm->sptr - 1]];
		enum gil_value_type typ = gil_value_get_type(val);
		if (typ == GIL_VAL_TYPE_FUNCTION || typ == GIL_VAL_TYPE_CFUNCTION) {
			gil_word nval_id = alloc_val(vm);
			struct gil_vm_value *nval = &vm->values[nval_id];
			nval->flags = val->flags;
			if (typ == GIL_VAL_TYPE_FUNCTION) {
				nval->func.self = ns_id;
				nval->func.pos = val->func.pos;
				nval->func.ns = val->func.ns;
			} else {
				nval->cfunc.mod = val->cfunc.mod;
				nval->cfunc.self = ns_id;
				nval->cfunc.func = val->cfunc.func;
			}
			vm->stack[vm->sptr - 1] = nval_id;
		}
	}
		break;

	case GIL_OP_ARRAY_LOOKUP: {
		gil_word key = read_uint(vm); \
		gil_word arr_id = vm->stack[--vm->sptr]; \
		struct gil_vm_value *arr = &vm->values[arr_id]; \
		if (gil_value_get_type(arr) != GIL_VAL_TYPE_ARRAY) { \
			vm->stack[vm->sptr++] = gil_vm_type_error(vm, arr); \
		} else { \
			vm->stack[vm->sptr++] = gil_vm_array_get(vm, arr, key); \
		}
	}
		break;

	case GIL_OP_ARRAY_SET: {
		gil_word key = read_uint(vm);
		gil_word val = vm->stack[vm->sptr - 1];
		gil_word arr_id = vm->stack[vm->sptr - 2];
		struct gil_vm_value *arr = &vm->values[arr_id];
		if (gil_value_get_type(arr) != GIL_VAL_TYPE_ARRAY) {
			vm->stack[vm->sptr - 1] = gil_vm_type_error(vm, arr);
		} else {
			vm->stack[vm->sptr - 1] = gil_vm_array_set(vm, arr, key, val);
		}
	}
		break;

	case GIL_OP_DYNAMIC_LOOKUP: {
		gil_word key_id = vm->stack[--vm->sptr];
		gil_word container_id = vm->stack[--vm->sptr];

		struct gil_vm_value *key = &vm->values[key_id];
		struct gil_vm_value *container = &vm->values[container_id];
		if (gil_value_get_type(container) == GIL_VAL_TYPE_ARRAY) {
			if (gil_value_get_type(key) != GIL_VAL_TYPE_REAL) {
				vm->stack[vm->sptr++] = gil_vm_type_error(vm, key);
			} else {
				vm->stack[vm->sptr++] = gil_vm_array_get(
						vm, container, (gil_word)key->real.real);
			}
		} else if (gil_value_get_type(container) == GIL_VAL_TYPE_NAMESPACE) {
			if (gil_value_get_type(key) != GIL_VAL_TYPE_ATOM) {
				vm->stack[vm->sptr++] = gil_vm_type_error(vm, key);
			} else {
				vm->stack[vm->sptr++] = gil_vm_namespace_get(vm, container, key->atom.atom);
			}
		} else if (gil_value_get_type(container) == GIL_VAL_TYPE_CVAL) {
			container = &vm->values[container->cval.ns];
			if (gil_value_get_type(container) == GIL_VAL_TYPE_NAMESPACE) {
				if (gil_value_get_type(key) != GIL_VAL_TYPE_ATOM) {
					vm->stack[vm->sptr++] = gil_vm_type_error(vm, key);
				} else {
					vm->stack[vm->sptr++] = gil_vm_namespace_get(vm, container, key->atom.atom);
				}
			} else {
				vm->stack[vm->sptr++] = gil_vm_type_error(vm, container);
			}
		} else {
			vm->stack[vm->sptr++] = gil_vm_type_error(vm, container);
		}
	}
		break;

	case GIL_OP_DYNAMIC_SET: {
		gil_word val = vm->stack[--vm->sptr];
		gil_word key_id = vm->stack[--vm->sptr];
		gil_word container_id = vm->stack[--vm->sptr];
		vm->stack[vm->sptr++] = val;

		struct gil_vm_value *key = &vm->values[key_id];
		struct gil_vm_value *container = &vm->values[container_id];

		if (gil_value_get_type(container) == GIL_VAL_TYPE_ARRAY) {
			if (gil_value_get_type(key) != GIL_VAL_TYPE_REAL) {
				vm->stack[vm->sptr - 1] = gil_vm_type_error(vm, key);
			} else {
				gil_vm_array_set(vm, container, (gil_word)key->real.real, val);
			}
		} else if (gil_value_get_type(container) == GIL_VAL_TYPE_NAMESPACE) {
			if (gil_value_get_type(key) != GIL_VAL_TYPE_ATOM) {
				vm->stack[vm->sptr - 1] = gil_vm_type_error(vm, key);
			} else {
				gil_vm_namespace_set(container, key->atom.atom, val);
			}
		} else {
			vm->stack[vm->sptr - 1] = gil_vm_type_error(vm, container);
		}
	}
		break;

	case GIL_OP_FUNC_CALL_INFIX: {
		gil_word rhs = vm->stack[--vm->sptr];
		gil_word func_id = vm->stack[--vm->sptr];
		gil_word lhs = vm->stack[--vm->sptr];

		gil_word argv[] = {lhs, rhs};
		call_func(vm, func_id, 2, argv);
	}
		break;

	case GIL_OP_LOAD_CMODULE: {
		word = read_uint(vm);
		int found = 0;
		for (size_t i = 0; i < vm->cmoduleslen; ++i) {
			if (vm->cmodules[i].id == word) {
				if (vm->cmodules[i].ns == vm->knone) {
					vm->cmodules[i].ns = vm->cmodules[i].mod->create(
							vm->cmodules[i].mod, vm, i);
				}
				vm->stack[vm->sptr++] = vm->cmodules[i].ns;
				found = 1;
				break;
			}
		}

		if (!found) {
			vm->stack[vm->sptr++] = gil_vm_error(vm, "Module not found");
		}
	}
		break;

	case GIL_OP_LOAD_MODULE: {
		gil_word pos = read_uint(vm);
		int found = 0;
		for (size_t i = 0; i < vm->moduleslen; ++i) {
			if (vm->modules[i].pos == pos) {
				vm->stack[vm->sptr++] = vm->modules[i].ns;
				found = 1;
				break;
			}
		}

		if (found) {
			break;
		}

		// We haven't seen the module before; let's load it
		gil_word ns_id = alloc_val(vm);
		vm->values[ns_id].ns.parent = vm->fstack[1].ns;
		vm->values[ns_id].ns.ns = NULL;
		vm->values[ns_id].flags = GIL_VAL_TYPE_NAMESPACE;
		vm->fstack[vm->fsptr].ns = ns_id;
		vm->fstack[vm->fsptr].retptr = vm->iptr;
		vm->fstack[vm->fsptr].sptr = vm->sptr;
		vm->fstack[vm->fsptr].args = vm->knone;
		vm->fsptr += 1;

		vm->iptr = pos;

		// Alloc a new module for it
		vm->moduleslen += 1;
		vm->modules = realloc(vm->modules, vm->moduleslen * sizeof(*vm->modules));
		vm->modules[vm->moduleslen - 1].pos = pos;
		vm->modules[vm->moduleslen - 1].ns = ns_id;
	}
		break;

	case GIL_OP_HALT:
		vm->halted = 1;
		break;
	}

	if (vm->need_gc) {
		gil_trace("GC");
		vm->need_gc = 0;
		gil_vm_gc(vm);
	}
}

int gil_vm_val_is_true(struct gil_vm *vm, struct gil_vm_value *val) {
	gil_word true_atom = vm->values[vm->ktrue].atom.atom;
	return gil_value_get_type(val) == GIL_VAL_TYPE_ATOM && val->atom.atom == true_atom;
}

gil_word gil_vm_make_atom(struct gil_vm *vm, gil_word val) {
	gil_word id = gil_vm_alloc(vm, GIL_VAL_TYPE_ATOM, 0);
	vm->values[id].atom.atom = val;
	return id;
}

gil_word gil_vm_make_real(struct gil_vm *vm, double val) {
	gil_word id = gil_vm_alloc(vm, GIL_VAL_TYPE_REAL, 0);
	vm->values[id].real.real = val;
	return id;
}

gil_word gil_vm_make_buffer(struct gil_vm *vm, char *data, size_t len) {
	gil_word id = gil_vm_alloc(vm, GIL_VAL_TYPE_BUFFER, 0);
	vm->values[id].buffer.length = len;
	vm->values[id].buffer.buffer = data;
	return id;
}

gil_word gil_vm_make_cfunction(struct gil_vm *vm, gil_vm_cfunction val, gil_word mod) {
	gil_word id = gil_vm_alloc(vm, GIL_VAL_TYPE_CFUNCTION, 0);
	vm->values[id].cfunc.mod = mod;
	vm->values[id].cfunc.func = val;
	vm->values[id].cfunc.self = 0;
	return id;
}

gil_word gil_vm_make_cval(struct gil_vm *vm, gil_word ctype, gil_word ns, void *val) {
	gil_word id = gil_vm_alloc(vm, GIL_VAL_TYPE_CVAL, 0);
	vm->values[id].cval.ctype = ctype;
	vm->values[id].cval.ns = ns;
	vm->values[id].cval.cval = val;
	return id;
}
