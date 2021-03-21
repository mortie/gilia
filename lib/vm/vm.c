#include "vm/vm.h"

#include <string.h>
#include <stdio.h>
#include <stdarg.h>

#include "vm/builtins.h"

static int stdio_inited = 0;
static struct l2_io_file_writer std_output;
static struct l2_io_file_writer std_error;

static l2_word alloc_val(struct l2_vm *vm) {
	size_t id = l2_bitset_set_next(&vm->valueset);
	if (id + 16 >= vm->valuessize) {
		if (id >= vm->valuessize) {
			if (vm->valuessize == 0) {
				vm->valuessize = 64;
			}

			while (id >= vm->valuessize) {
				vm->valuessize *= 2;
			}

			vm->values = realloc(vm->values, sizeof(*vm->values) * vm->valuessize);
		} else {
			vm->gc_scheduled = 1;
		}
	}

	return (l2_word)id;
}

static void gc_mark_array(struct l2_vm *vm, struct l2_vm_value *val);
static void gc_mark_namespace(struct l2_vm *vm, struct l2_vm_value *val);

static void gc_mark(struct l2_vm *vm, l2_word id) {
	struct l2_vm_value *val = &vm->values[id];
	if (val->flags & L2_VAL_MARKED) {
		return;
	}

	val->flags |= L2_VAL_MARKED;

	int typ = l2_value_get_type(val);
	if (typ == L2_VAL_TYPE_ARRAY) {
		gc_mark_array(vm, val);
	} else if (typ == L2_VAL_TYPE_NAMESPACE) {
		gc_mark_namespace(vm, val);
	} else if (typ == L2_VAL_TYPE_FUNCTION) {
		gc_mark(vm, val->func.ns);
	} else if (typ == L2_VAL_TYPE_CONTINUATION && val->cont != NULL) {
		if (val->cont->marker != NULL) {
			val->cont->marker(vm, val->cont, gc_mark);
		}
		if (val->cont->args != 0) {
			gc_mark(vm, val->cont->args);
		}
	}
}

static void gc_mark_array(struct l2_vm *vm, struct l2_vm_value *val) {
	l2_word *data;
	if (val->flags & L2_VAL_SBO) {
		data = val->shortarray;
	} else {
		data = val->array->data;
	}

	for (size_t i = 0; i < val->extra.arr_length; ++i) {
		gc_mark(vm, data[i]);
	}
}

static void gc_mark_namespace(struct l2_vm *vm, struct l2_vm_value *val) {
	if (val->extra.ns_parent != 0) {
		gc_mark(vm, val->extra.ns_parent);
	}

	if (val->ns == NULL) {
		return;
	}

	for (size_t i = 0; i < val->ns->size; ++i) {
		l2_word key = val->ns->data[i];
		if (key == 0 || key == ~(l2_word)0) {
			continue;
		}

		gc_mark(vm, val->ns->data[val->ns->size + i]);
	}
}

static void gc_free(struct l2_vm *vm, l2_word id) {
	struct l2_vm_value *val = &vm->values[id];
	l2_bitset_unset(&vm->valueset, id);

	// Don't need to do anything more; the next round of GC will free
	// whichever values were only referenced by the array
	int typ = l2_value_get_type(val);
	if (typ == L2_VAL_TYPE_ARRAY && !(val->flags & L2_VAL_SBO)) {
		free(val->array);
	} else if (typ == L2_VAL_TYPE_BUFFER) {
		free(val->buffer);
	} else if (typ == L2_VAL_TYPE_NAMESPACE) {
		free(val->ns);
	} else if (typ == L2_VAL_TYPE_ERROR) {
		free(val->error);
	} else if (typ == L2_VAL_TYPE_CONTINUATION && val->cont) {
		free(val->cont);
	}
}

static size_t gc_sweep(struct l2_vm *vm) {
	size_t freed = 0;
	for (size_t i = vm->gc_start; i < vm->valuessize; ++i) {
		if (!l2_bitset_get(&vm->valueset, i)) {
			continue;
		}

		struct l2_vm_value *val = &vm->values[i];
		if (!(val->flags & L2_VAL_MARKED)) {
			l2_bitset_unset(&vm->valueset, i);
			gc_free(vm, i);
			freed += 1;
		} else {
			val->flags &= ~L2_VAL_MARKED;
		}
	}

	// Normal variables are unmarked by the above loop,
	// but builtins don't go through that loop
	for (size_t i = 0; i < vm->gc_start; ++i) {
		vm->values[i].flags &= ~L2_VAL_MARKED;
	}

	return freed;
}

const char *l2_value_type_name(enum l2_value_type typ) {
	switch (typ) {
	case L2_VAL_TYPE_NONE: return "NONE";
	case L2_VAL_TYPE_ATOM: return "ATOM";
	case L2_VAL_TYPE_REAL: return "REAL";
	case L2_VAL_TYPE_BUFFER: return "BUFFER";
	case L2_VAL_TYPE_ARRAY: return "ARRAY";
	case L2_VAL_TYPE_NAMESPACE: return "NAMESPACE";
	case L2_VAL_TYPE_FUNCTION: return "FUNCTION";
	case L2_VAL_TYPE_CFUNCTION: return "CFUNCTION";
	case L2_VAL_TYPE_ERROR: return "ERROR";
	case L2_VAL_TYPE_CONTINUATION: return "CONTINUATION";
	}

	return "(unknown)";
}

l2_word l2_value_arr_get(struct l2_vm *vm, struct l2_vm_value *val, l2_word k) {
	if (k >= val->extra.arr_length) {
		return l2_vm_error(vm, "Array index out of bounds");
	}

	if (val->flags & L2_VAL_SBO) {
		return val->shortarray[k];
	}

	return val->array->data[k];
}

l2_word l2_value_arr_set(struct l2_vm *vm, struct l2_vm_value *val, l2_word k, l2_word v) {
	if (k >= val->extra.arr_length) {
		return l2_vm_error(vm, "Array index out of bounds");
	}

	if (val->flags & L2_VAL_SBO) {
		return val->shortarray[k] = v;
	}

	return val->array->data[k] = v;
}

void l2_vm_init(struct l2_vm *vm, unsigned char *ops, size_t opslen) {
	if (!stdio_inited) {
		std_output.w.write = l2_io_file_write;
		std_output.f = stdout;
		std_error.w.write = l2_io_file_write;
		std_error.f = stderr;
		stdio_inited = 1;
	}

	vm->std_output = &std_output.w;
	vm->std_error = &std_error.w;

	vm->halted = 0;
	vm->gc_scheduled = 0;
	vm->ops = ops;
	vm->opslen = opslen;
	vm->iptr = 0;
	vm->sptr = 0;
	vm->fsptr = 0;

	vm->values = NULL;
	vm->valuessize = 0;
	l2_bitset_init(&vm->valueset);

	// It's wasteful to allocate new 'none' variables all the time,
	// variable ID 0 should be the only 'none' variable in the system
	l2_word none_id = alloc_val(vm);
	vm->values[none_id].flags = L2_VAL_TYPE_NONE | L2_VAL_CONST;

	// Need to allocate a builtins namespace
	l2_word builtins = alloc_val(vm);
	vm->values[builtins].extra.ns_parent = 0;
	vm->values[builtins].ns = NULL; // Will be allocated on first insert
	vm->values[builtins].flags = L2_VAL_TYPE_NAMESPACE;
	vm->fstack[vm->fsptr].ns = builtins;
	vm->fstack[vm->fsptr].retptr = 0;
	vm->fstack[vm->fsptr].sptr = 0;
	vm->fsptr += 1;

	// Need to allocate a root namespace
	l2_word root = alloc_val(vm);
	vm->values[root].extra.ns_parent = builtins;
	vm->values[root].ns = NULL;
	vm->values[root].flags = L2_VAL_TYPE_NAMESPACE;
	vm->fstack[vm->fsptr].ns = root;
	vm->fstack[vm->fsptr].retptr = 0;
	vm->fstack[vm->fsptr].sptr = 0;
	vm->fsptr += 1;

	// None is always at 0
	vm->knone = 0;
	vm->values[vm->knone].flags = L2_VAL_TYPE_NONE | L2_VAL_CONST;

	// Define a C function variable for every builtin
	l2_word id;
	l2_word key = 1;
#define XNAME(name, k) \
	l2_vm_namespace_set(&vm->values[builtins], key, vm->k); \
	key += 1;
#define XATOM(name, k) \
	id = alloc_val(vm); \
	vm->values[id].flags = L2_VAL_TYPE_ATOM | L2_VAL_CONST; \
	vm->values[id].atom = key; \
	vm->k = id; \
	key += 1;
#define XFUNCTION(name, f) \
	id = alloc_val(vm); \
	vm->values[id].flags = L2_VAL_TYPE_CFUNCTION | L2_VAL_CONST; \
	vm->values[id].cfunc = f; \
	l2_vm_namespace_set(&vm->values[builtins], key, id); \
	key += 1;
#include "builtins.x.h"
#undef XNAME
#undef XATOM
#undef XFUNCTION

	vm->gc_start = id + 1;
}

l2_word l2_vm_alloc(struct l2_vm *vm, enum l2_value_type typ, enum l2_value_flags flags) {
	l2_word id = alloc_val(vm);
	memset(&vm->values[id], 0, sizeof(vm->values[id]));
	vm->values[id].flags = typ | flags;
	return id;
}

l2_word l2_vm_error(struct l2_vm *vm, const char *fmt, ...) {
	l2_word id = alloc_val(vm);
	struct l2_vm_value *val = &vm->values[id];
	val->flags = L2_VAL_CONST | L2_VAL_TYPE_ERROR;

	char buf[256];

	va_list va;
	va_start(va, fmt);
	int n = vsnprintf(buf, sizeof(buf), fmt, va);

	if (n < 0) {
		const char *message = "Failed to generate error message!";
		val->error = malloc(strlen(message) + 1);
		strcpy(val->error, message);
		va_end(va);
		return id;
	} else if ((size_t)n + 1 < sizeof(buf)) {
		val->error = malloc(n + 1);
		strcpy(val->error, buf);
		va_end(va);
		return id;
	}

	val->error = malloc(n + 1);
	vsnprintf(val->error, n + 1, fmt, va);
	va_end(va);
	return id;
}

l2_word l2_vm_type_error(struct l2_vm *vm, struct l2_vm_value *val) {
	enum l2_value_type typ = l2_value_get_type(val);
	if (typ == L2_VAL_TYPE_ERROR) {
		return val - vm->values;
	}

	return l2_vm_error(vm, "Unexpected type %s", l2_value_type_name(l2_value_get_type(val)));
}

void l2_vm_free(struct l2_vm *vm) {
	// Skip ID 0, because that's always NONE
	for (size_t i = 1; i < vm->valuessize; ++i) {
		if (!l2_bitset_get(&vm->valueset, i)) {
			continue;
		}

		gc_free(vm, i);
	}

	free(vm->values);
	l2_bitset_free(&vm->valueset);
}

size_t l2_vm_gc(struct l2_vm *vm) {
	for (l2_word sptr = 0; sptr < vm->sptr; ++sptr) {
		gc_mark(vm, vm->stack[sptr]);
	}

	// Don't need to mark the first stack frame, since that's where all the
	// builtins live, and they aren't sweeped anyways
	for (l2_word fsptr = 1; fsptr < vm->fsptr; ++fsptr) {
		gc_mark(vm, vm->fstack[fsptr].ns);
	}

	return gc_sweep(vm);
}

void l2_vm_run(struct l2_vm *vm) {
	while (!vm->halted) {
		l2_vm_step(vm);
	}
}

static void call_func_with_args(struct l2_vm *vm, l2_word func_id, l2_word args_id) {
	l2_word stack_base = vm->sptr;
	vm->stack[vm->sptr++] = args_id;

	l2_word ns_id = alloc_val(vm);
	struct l2_vm_value *func = &vm->values[func_id]; // func might be stale after alloc_val
	vm->values[ns_id].extra.ns_parent = func->func.ns;
	vm->values[ns_id].ns = NULL;
	vm->values[ns_id].flags = L2_VAL_TYPE_NAMESPACE;
	vm->fstack[vm->fsptr].ns = ns_id;
	vm->fstack[vm->fsptr].retptr = vm->iptr;
	vm->fstack[vm->fsptr].sptr = stack_base;
	vm->fsptr += 1;

	vm->iptr = func->func.pos;
}

// The 'call_func' function assumes that all relevant values have been popped off
// the stack, so that the return value can be pushed to the top of the stack
// straight away
static void call_func(
		struct l2_vm *vm, l2_word func_id,
		l2_word argc, l2_word *argv) {
	struct l2_vm_value *func = &vm->values[func_id];
	enum l2_value_type typ = l2_value_get_type(func);

	// C functions are called differently from language functions
	if (typ == L2_VAL_TYPE_CFUNCTION) {
		// Make this a while loop, because using call_func would
		// make the call stack depth unbounded
		vm->stack[vm->sptr++] = func->cfunc(vm, argc, argv);
		while (1) {
			l2_word cont_id = vm->stack[vm->sptr - 1];
			struct l2_vm_value *cont = &vm->values[cont_id];
			if (l2_value_get_type(cont) != L2_VAL_TYPE_CONTINUATION) {
				break;
			}

			// If there's no callback it's easy, just call the function
			// it wants us to call
			l2_word call_id = cont->extra.cont_call;
			if (cont->cont == NULL) {
				vm->sptr -= 1;
				call_func(vm, call_id, 0, NULL);
				break;
			}

			struct l2_vm_value *call = &vm->values[call_id];

			if (l2_value_get_type(call) == L2_VAL_TYPE_CFUNCTION) {
				int argc = 0;
				l2_word *argv = NULL;
				if (cont->cont && cont->cont->args != 0) {
					struct l2_vm_value *args = &vm->values[cont->cont->args];
					if (l2_value_get_type(args) != L2_VAL_TYPE_ARRAY) {
						vm->stack[vm->sptr - 1] = l2_vm_type_error(vm, args);
						break;
					}

					argc = args->extra.arr_length;
					if (args->flags & L2_VAL_SBO) {
						argv = args->shortarray;
					} else {
						argv = args->array->data;
					}
				}

				l2_word retval = call->cfunc(vm, argc, argv);
				vm->stack[vm->sptr - 1] = cont->cont->callback(vm, retval, cont_id);
			} else if (l2_value_get_type(call) == L2_VAL_TYPE_FUNCTION) {
				// Leave the continuation on the stack,
				// let the L2_OP_RET code deal with it
				cont->flags |= L2_VAL_CONT_CALLBACK;
				if (cont->cont && cont->cont->args) {
					call_func_with_args(vm, call_id, cont->cont->args);
				} else {
					call_func(vm, call_id, 0, NULL);
				}
				break;
			} else {
				l2_word err = l2_vm_type_error(vm, call);
				vm->stack[vm->sptr - 1] = cont->cont->callback(vm, err, cont_id);
			}
		}

		return;
	}

	// Don't interpret a non-function as a function
	if (typ != L2_VAL_TYPE_FUNCTION) {
		vm->stack[vm->sptr++] = l2_vm_error(vm, "Attempt to call non-function");
		return;
	}

	l2_word args_id = alloc_val(vm);
	struct l2_vm_value *args = &vm->values[args_id];
	args->extra.arr_length = argc;
	if (argc <= 2) {
		args->flags = L2_VAL_TYPE_ARRAY | L2_VAL_SBO;
		memcpy(args->shortarray, argv, argc * sizeof(l2_word));
	} else {
		args->flags = L2_VAL_TYPE_ARRAY;
		args->array = malloc(
				sizeof(struct l2_vm_array) + sizeof(l2_word) * argc);
		args->array->size = argc;
		memcpy(args->array->data, argv, argc * sizeof(l2_word));
	}

	call_func_with_args(vm, func_id, args_id);
}

static l2_word read_u4le(struct l2_vm *vm) {
	unsigned char *data = &vm->ops[vm->iptr];
	l2_word ret =
		(l2_word)data[0] |
		(l2_word)data[1] << 8 |
		(l2_word)data[2] << 16 |
		(l2_word)data[3] << 24;
	vm->iptr += 4;
	return ret;
}

static l2_word read_u1le(struct l2_vm *vm) {
	return vm->ops[vm->iptr++];
}

static double read_d8le(struct l2_vm *vm) {
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

void l2_vm_step(struct l2_vm *vm) {
	enum l2_opcode opcode = (enum l2_opcode)vm->ops[vm->iptr++];

	l2_word word;
	switch (opcode) {
	case L2_OP_NOP:
		break;

	case L2_OP_DISCARD:
		vm->sptr -= 1;
		if (l2_value_get_type(&vm->values[vm->stack[vm->sptr]]) == L2_VAL_TYPE_ERROR) {
			l2_io_printf(vm->std_error, "Error: %s\n", vm->values[vm->stack[vm->sptr]].error);
			vm->halted = 1;
		}
		break;

	case L2_OP_SWAP_DISCARD:
		vm->stack[vm->sptr - 2] = vm->stack[vm->sptr - 1];
		vm->sptr -= 1;
		if (l2_value_get_type(&vm->values[vm->stack[vm->sptr]]) == L2_VAL_TYPE_ERROR) {
			l2_io_printf(vm->std_error, "Error: %s\n", vm->values[vm->stack[vm->sptr]].error);
			vm->halted = 1;
		}
		break;

	case L2_OP_DUP:
		vm->stack[vm->sptr] = vm->ops[vm->sptr - 1];
		vm->sptr += 1;
		break;

	case L2_OP_ADD:
		vm->stack[vm->sptr - 2] += vm->stack[vm->sptr - 1];
		vm->sptr -= 1;
		break;

#define X(read) \
		l2_word argc = read(vm); \
		vm->sptr -= argc; \
		l2_word *argv = vm->stack + vm->sptr; \
		l2_word func_id = vm->stack[--vm->sptr]; \
		call_func(vm, func_id, argc, argv)
	case L2_OP_FUNC_CALL_U4: { X(read_u4le); } break;
	case L2_OP_FUNC_CALL_U1: { X(read_u1le); } break;
#undef X

#define X(read) word = read(vm); vm->iptr += word;
	case L2_OP_RJMP_U4: { X(read_u4le); } break;
	case L2_OP_RJMP_U1: { X(read_u1le); } break;
#undef X

#define X(read) \
		l2_word key = read(vm); \
		struct l2_vm_value *ns = &vm->values[vm->fstack[vm->fsptr - 1].ns]; \
		vm->stack[vm->sptr++] = l2_vm_namespace_get(vm, ns, key);
	case L2_OP_STACK_FRAME_LOOKUP_U4: { X(read_u4le); } break;
	case L2_OP_STACK_FRAME_LOOKUP_U1: { X(read_u1le); } break;
#undef X

#define X(read) \
		l2_word key = read(vm); \
		l2_word val = vm->stack[vm->sptr - 1]; \
		struct l2_vm_value *ns = &vm->values[vm->fstack[vm->fsptr - 1].ns]; \
		l2_vm_namespace_set(ns, key, val);
	case L2_OP_STACK_FRAME_SET_U4: { X(read_u4le); } break;
	case L2_OP_STACK_FRAME_SET_U1: { X(read_u1le); } break;
#undef X

#define X(read) \
		l2_word key = read(vm); \
		l2_word val = vm->stack[vm->sptr - 1]; \
		struct l2_vm_value *ns = &vm->values[vm->fstack[vm->fsptr - 1].ns]; \
		if (l2_vm_namespace_replace(vm, ns, key, val) < 0) { \
			vm->stack[vm->sptr - 1] = l2_vm_error(vm, "Variable not found"); \
		}
	case L2_OP_STACK_FRAME_REPLACE_U4: { X(read_u4le); } break;
	case L2_OP_STACK_FRAME_REPLACE_U1: { X(read_u1le); } break;
#undef X

	case L2_OP_RET:
		{
			l2_word retval = vm->stack[--vm->sptr];
			l2_word retptr = vm->fstack[vm->fsptr - 1].retptr;
			l2_word sptr = vm->fstack[vm->fsptr - 1].sptr;
			vm->fsptr -= 1;
			vm->sptr = sptr;
			vm->iptr = retptr;

			l2_word cont_id;
			struct l2_vm_value *cont = NULL;
			if (vm->sptr > 0) {
				cont_id = vm->stack[vm->sptr - 1];
				cont = &vm->values[cont_id];
			}

			int iscont =
				cont != NULL && l2_value_get_type(cont) == L2_VAL_TYPE_CONTINUATION;
			int nocallback =
				!iscont || (cont->flags & L2_VAL_CONT_CALLBACK && cont->cont == NULL);
			if (nocallback) {
				if (iscont) {
					vm->stack[vm->sptr - 1] = retval;
				} else {
					vm->stack[vm->sptr++] = retval;
				}
				break;
			}

			if (cont->flags & L2_VAL_CONT_CALLBACK) {
				retval = cont->cont->callback(vm, retval, cont_id);
				cont_id = retval;
				cont = &vm->values[cont_id];

				if (l2_value_get_type(cont) != L2_VAL_TYPE_CONTINUATION) {
					vm->stack[vm->sptr - 1] = retval;
					break;
				}
			}

			cont->flags |= L2_VAL_CONT_CALLBACK;
			if (cont->cont && cont->cont->args != 0) {
				call_func_with_args(vm, cont->extra.cont_call, cont->cont->args);
			} else {
				call_func(vm, cont->extra.cont_call, 0, NULL);
			}
		}
		break;

	case L2_OP_ALLOC_NONE:
		vm->stack[vm->sptr++] = 0;
		break;

#define X(read) \
		word = alloc_val(vm); \
		vm->values[word].flags = L2_VAL_TYPE_ATOM; \
		vm->values[word].atom = read(vm); \
		vm->stack[vm->sptr++] = word;
	case L2_OP_ALLOC_ATOM_U4: { X(read_u4le); } break;
	case L2_OP_ALLOC_ATOM_U1: { X(read_u1le); } break;
#undef X

	case L2_OP_ALLOC_REAL_D8:
		{
			word = alloc_val(vm);
			vm->values[word].flags = L2_VAL_TYPE_REAL;
			vm->values[word].real = read_d8le(vm);
			vm->stack[vm->sptr++] = word;
		}
		break;

#define X(read) \
		word = alloc_val(vm); \
		l2_word length = read(vm); \
		l2_word offset = read(vm); \
		vm->values[word].flags = L2_VAL_TYPE_BUFFER; \
		vm->values[word].buffer = length > 0 ? malloc(length) : NULL; \
		vm->values[word].extra.buf_length = length; \
		memcpy(vm->values[word].buffer, vm->ops + offset, length); \
		vm->stack[vm->sptr] = word; \
		vm->sptr += 1;
	case L2_OP_ALLOC_BUFFER_STATIC_U4: { X(read_u4le); } break;
	case L2_OP_ALLOC_BUFFER_STATIC_U1: { X(read_u1le); } break;
#undef X

#define X(read) \
		l2_word count = read(vm); \
		l2_word arr_id = alloc_val(vm); \
		struct l2_vm_value *arr = &vm->values[arr_id]; \
		arr->extra.arr_length = count; \
		l2_word *data; \
		if (count <= 2) { \
			arr->flags = L2_VAL_TYPE_ARRAY | L2_VAL_SBO; \
			data = arr->shortarray; \
		} else { \
			arr->flags = L2_VAL_TYPE_ARRAY; \
			arr->array = malloc(sizeof(struct l2_vm_array) + count * sizeof(l2_word)); \
			arr->array->size = count; \
			data = arr->array->data; \
		} \
		for (l2_word i = 0; i < count; ++i) { \
			data[count - 1 - i] = vm->stack[--vm->sptr]; \
		} \
		vm->stack[vm->sptr++] = arr_id;
	case L2_OP_ALLOC_ARRAY_U4: { X(read_u4le); } break;
	case L2_OP_ALLOC_ARRAY_U1: { X(read_u1le); } break;
#undef X

	case L2_OP_ALLOC_NAMESPACE:
		word = alloc_val(vm);
		vm->values[word].flags = L2_VAL_TYPE_NAMESPACE;
		vm->values[word].extra.ns_parent = 0;
		vm->values[word].ns = NULL; // Will be allocated on first insert
		vm->stack[vm->sptr] = word;
		vm->sptr += 1;
		break;

#define X(read) \
		word = alloc_val(vm); \
		vm->values[word].flags = L2_VAL_TYPE_FUNCTION; \
		vm->values[word].func.pos = read(vm); \
		vm->values[word].func.ns = vm->fstack[vm->fsptr - 1].ns; \
		vm->stack[vm->sptr] = word; \
		vm->sptr += 1;
	case L2_OP_ALLOC_FUNCTION_U4: { X(read_u4le); } break;
	case L2_OP_ALLOC_FUNCTION_U1: { X(read_u1le); } break;
#undef X

#define X(read) \
		l2_word key = read(vm); \
		l2_word val = vm->stack[vm->sptr - 1]; \
		l2_word ns = vm->stack[vm->sptr - 2]; \
		l2_vm_namespace_set(&vm->values[ns], key, val);
	case L2_OP_NAMESPACE_SET_U4: { X(read_u4le); } break;
	case L2_OP_NAMESPACE_SET_U1: { X(read_u1le); } break;
#undef X

#define X(read) \
		l2_word key = read(vm); \
		l2_word ns = vm->stack[--vm->sptr]; \
		vm->stack[vm->sptr++] = l2_vm_namespace_get(vm, &vm->values[ns], key);
	case L2_OP_NAMESPACE_LOOKUP_U4: { X(read_u4le); } break;
	case L2_OP_NAMESPACE_LOOKUP_U1: { X(read_u1le); } break;
#undef X

#define X(read) \
		l2_word key = read(vm); \
		l2_word arr_id = vm->stack[--vm->sptr]; \
		struct l2_vm_value *arr = &vm->values[arr_id]; \
		if (l2_value_get_type(arr) != L2_VAL_TYPE_ARRAY) { \
			vm->stack[vm->sptr++] = l2_vm_type_error(vm, arr); \
		} else { \
			vm->stack[vm->sptr++] = l2_value_arr_get(vm, arr, key); \
		}
	case L2_OP_ARRAY_LOOKUP_U4: { X(read_u4le); } break;
	case L2_OP_ARRAY_LOOKUP_U1: { X(read_u1le); } break;
#undef X

#define X(read) \
		l2_word key = read(vm); \
		l2_word val = vm->stack[vm->sptr - 1]; \
		l2_word arr_id = vm->stack[vm->sptr - 2]; \
		struct l2_vm_value *arr = &vm->values[arr_id]; \
		if (l2_value_get_type(arr) != L2_VAL_TYPE_ARRAY) { \
			vm->stack[vm->sptr - 1] = l2_vm_type_error(vm, arr); \
		} else { \
			vm->stack[vm->sptr - 1] = l2_value_arr_set(vm, arr, key, val); \
		}
	case L2_OP_ARRAY_SET_U4: { X(read_u4le); } break;
	case L2_OP_ARRAY_SET_U1: { X(read_u1le); } break;

	case L2_OP_DYNAMIC_LOOKUP:
		{
			l2_word key_id = vm->stack[--vm->sptr];
			l2_word container_id = vm->stack[--vm->sptr];

			struct l2_vm_value *key = &vm->values[key_id];
			struct l2_vm_value *container = &vm->values[container_id];
			if (l2_value_get_type(container) == L2_VAL_TYPE_ARRAY) {
				if (l2_value_get_type(key) != L2_VAL_TYPE_REAL) {
					vm->stack[vm->sptr++] = l2_vm_type_error(vm, key);
				} else if (key->real >= container->extra.arr_length) {
					vm->stack[vm->sptr++] = l2_vm_error(vm, "Index out of range");
				} else {
					vm->stack[vm->sptr++] = container->array->data[(l2_word)key->real];
				}
			} else if (l2_value_get_type(container) == L2_VAL_TYPE_NAMESPACE) {
				if (l2_value_get_type(key) != L2_VAL_TYPE_ATOM) {
					vm->stack[vm->sptr++] = l2_vm_type_error(vm, key);
				} else {
					vm->stack[vm->sptr++] = l2_vm_namespace_get(vm, container, key->atom);
				}
			} else {
				vm->stack[vm->sptr++] = l2_vm_type_error(vm, container);
			}
		}
		break;

	case L2_OP_DYNAMIC_SET:
		{
			l2_word val = vm->stack[--vm->sptr];
			l2_word key_id = vm->stack[--vm->sptr];
			l2_word container_id = vm->stack[--vm->sptr];
			vm->stack[vm->sptr++] = val;

			struct l2_vm_value *key = &vm->values[key_id];
			struct l2_vm_value *container = &vm->values[container_id];

			if (l2_value_get_type(container) == L2_VAL_TYPE_ARRAY) {
				if (l2_value_get_type(key) != L2_VAL_TYPE_REAL) {
					vm->stack[vm->sptr - 1] = l2_vm_type_error(vm, key);
				} else if (key->real >= container->extra.arr_length) {
					vm->stack[vm->sptr - 1] = l2_vm_error(vm, "Index out of range");
				} else {
					container->array->data[(size_t)key->real] = val;
				}
			} else if (l2_value_get_type(container) == L2_VAL_TYPE_NAMESPACE) {
				if (l2_value_get_type(key) != L2_VAL_TYPE_ATOM) {
					vm->stack[vm->sptr - 1] = l2_vm_type_error(vm, key);
				} else {
					l2_vm_namespace_set(container, key->atom, val);
				}
			} else {
				vm->stack[vm->sptr - 1] = l2_vm_type_error(vm, container);
			}
		}
		break;

	case L2_OP_FUNC_CALL_INFIX:
		{
			l2_word rhs = vm->stack[--vm->sptr];
			l2_word func_id = vm->stack[--vm->sptr];
			l2_word lhs = vm->stack[--vm->sptr];

			l2_word argv[] = {lhs, rhs};
			call_func(vm, func_id, 2, argv);
		}
		break;

	case L2_OP_HALT:
		vm->halted = 1;
		break;
	}

	if (vm->gc_scheduled) {
		l2_vm_gc(vm);
		vm->gc_scheduled = 0;
	}
}

int l2_vm_val_is_true(struct l2_vm *vm, struct l2_vm_value *val) {
	l2_word true_atom = vm->values[vm->ktrue].atom;
	return l2_value_get_type(val) == L2_VAL_TYPE_ATOM && val->atom == true_atom;
}
