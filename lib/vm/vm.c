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

static double u32s_to_double(uint32_t high, uint32_t low) {
	double d;
	uint64_t num = (uint64_t)high << 32 | (uint64_t)low;
	memcpy(&d, &num, sizeof(num));
	return d;
}

static void gc_mark_array(struct l2_vm *vm, struct l2_vm_value *val);
static void gc_mark_namespace(struct l2_vm *vm, struct l2_vm_value *val);

static void gc_mark(struct l2_vm *vm, l2_word id) {
	struct l2_vm_value *val = &vm->values[id];
	if (val->flags & L2_VAL_MARKED) {
		return;
	}

	val->flags |= L2_VAL_MARKED;

	int typ = l2_vm_value_type(val);
	if (typ == L2_VAL_TYPE_ARRAY) {
		gc_mark_array(vm, val);
	} else if (typ == L2_VAL_TYPE_NAMESPACE) {
		gc_mark_namespace(vm, val);
	} else if (typ == L2_VAL_TYPE_FUNCTION) {
		gc_mark(vm, val->func.ns);
	}
}

static void gc_mark_array(struct l2_vm *vm, struct l2_vm_value *val) {
	if (val->array == NULL) {
		return;
	}

	for (size_t i = 0; i < val->array->len; ++i) {
		gc_mark(vm, val->array->data[i]);
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
	int typ = l2_vm_value_type(val);
	if (typ == L2_VAL_TYPE_ARRAY) {
		free(val->array);
	} else if (typ == L2_VAL_TYPE_BUFFER) {
		free(val->buffer);
	} else if (typ == L2_VAL_TYPE_NAMESPACE) {
		free(val->ns);
	} else if (typ == L2_VAL_TYPE_ERROR) {
		free(val->error);
	}
}

static size_t gc_sweep(struct l2_vm *vm) {
	size_t freed = 0;
	// Skip ID 0, because that should always exist
	for (size_t i = 1; i < vm->valuessize; ++i) {
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
	}

	return "(unknown)";
}

void l2_vm_init(struct l2_vm *vm, l2_word *ops, size_t opcount) {
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
	vm->opcount = opcount;
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
	vm->fsptr += 1;

	// Need to allocate a root namespace
	l2_word root = alloc_val(vm);
	vm->values[root].extra.ns_parent = builtins;
	vm->values[root].ns = NULL;
	vm->values[root].flags = L2_VAL_TYPE_NAMESPACE;
	vm->fstack[vm->fsptr].ns = root;
	vm->fstack[vm->fsptr].retptr = 0;
	vm->fsptr += 1;

	// Define a C function variable for every builtin
	l2_word id;
	l2_word key = 1;
#define Y(name, k) \
	if (strcmp(#k, "knone") == 0) { \
		id = 0; \
	} else { \
		id = alloc_val(vm); \
		vm->values[id].flags = L2_VAL_TYPE_ATOM | L2_VAL_CONST; \
		vm->values[id].atom = key; \
	} \
	vm->k = id; \
	l2_vm_namespace_set(&vm->values[builtins], key++, id);
#define X(name, f) \
	id = alloc_val(vm); \
	vm->values[id].flags = L2_VAL_TYPE_CFUNCTION | L2_VAL_CONST; \
	vm->values[id].cfunc = f; \
	l2_vm_namespace_set(&vm->values[builtins], key++, id);
#include "builtins.x.h"
#undef X
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
	return l2_vm_error(vm, "Unexpected type %s", l2_value_type_name(l2_vm_value_type(val)));
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

	for (l2_word fsptr = 0; fsptr < vm->fsptr; ++fsptr) {
		gc_mark(vm, vm->fstack[fsptr].ns);
	}

	return gc_sweep(vm);
}

void l2_vm_run(struct l2_vm *vm) {
	while (!vm->halted) {
		l2_vm_step(vm);
	}
}

// The 'call_func' function assumes that all relevant values have been popped off
// the stack, so that the return value can be pushed to the top of the stack
// straight away
static void call_func(
		struct l2_vm *vm, l2_word func_id,
		l2_word argc, l2_word *argv) {
	l2_word stack_base = vm->sptr;

	struct l2_vm_value *func = &vm->values[func_id];
	enum l2_value_type typ = l2_vm_value_type(func);

	// C functions are called differently from language functions
	if (typ == L2_VAL_TYPE_CFUNCTION) {
		vm->stack[vm->sptr++] = func->cfunc(vm, argc, argv);
		return;
	}

	// Don't interpret a non-function as a function
	if (typ != L2_VAL_TYPE_FUNCTION) {
		vm->stack[vm->sptr++] = l2_vm_error(vm, "Attempt to call non-function");
		return;
	}

	l2_word arr_id = alloc_val(vm);
	vm->values[arr_id].flags = L2_VAL_TYPE_ARRAY;
	vm->values[arr_id].array = malloc(
			sizeof(struct l2_vm_array) + sizeof(l2_word) * argc);
	struct l2_vm_array *arr = vm->values[arr_id].array;
	arr->len = argc;
	arr->size = argc;

	for (l2_word i = 0; i < argc; ++i) {
		arr->data[i] = argv[i];
	}

	vm->stack[vm->sptr++] = arr_id;

	l2_word ns_id = alloc_val(vm);
	func = &vm->values[func_id]; // func might be stale after alloc
	vm->values[ns_id].extra.ns_parent = func->func.ns;
	vm->values[ns_id].ns = NULL;
	vm->values[ns_id].flags = L2_VAL_TYPE_NAMESPACE;
	vm->fstack[vm->fsptr].ns = ns_id;
	vm->fstack[vm->fsptr].retptr = vm->iptr;
	vm->fstack[vm->fsptr].sptr = stack_base;
	vm->fsptr += 1;

	vm->iptr = func->func.pos;
}

void l2_vm_step(struct l2_vm *vm) {
	enum l2_opcode opcode = (enum l2_opcode)vm->ops[vm->iptr++];

	l2_word word;
	switch (opcode) {
	case L2_OP_NOP:
		break;

	case L2_OP_DISCARD:
		vm->sptr -= 1;
		if (l2_vm_value_type(&vm->values[vm->stack[vm->sptr]]) == L2_VAL_TYPE_ERROR) {
			l2_io_printf(vm->std_error, "Error: %s\n", vm->values[vm->stack[vm->sptr]].error);
			vm->halted = 1;
		}
		break;

	case L2_OP_SWAP_DISCARD:
		vm->stack[vm->sptr - 2] = vm->stack[vm->sptr - 1];
		vm->sptr -= 1;
		if (l2_vm_value_type(&vm->values[vm->stack[vm->sptr]]) == L2_VAL_TYPE_ERROR) {
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

	case L2_OP_FUNC_CALL:
		{
			l2_word argc = vm->ops[vm->iptr++];
			vm->sptr -= argc;
			l2_word *argv = vm->stack + vm->sptr;

			l2_word func_id = vm->stack[--vm->sptr];
			call_func(vm, func_id, argc, argv);
		}
		break;

	case L2_OP_RJMP:
		vm->iptr += vm->ops[vm->iptr] + 1;
		break;

	case L2_OP_STACK_FRAME_LOOKUP:
		{
			l2_word key = vm->ops[vm->iptr++];
			struct l2_vm_value *ns = &vm->values[vm->fstack[vm->fsptr - 1].ns];
			vm->stack[vm->sptr++] = l2_vm_namespace_get(vm, ns, key);
		}
		break;

	case L2_OP_STACK_FRAME_SET:
		{
			l2_word key = vm->ops[vm->iptr++];
			l2_word val = vm->stack[vm->sptr - 1];
			struct l2_vm_value *ns = &vm->values[vm->fstack[vm->fsptr - 1].ns];
			l2_vm_namespace_set(ns, key, val);
		}
		break;

	case L2_OP_STACK_FRAME_REPLACE:
		{
			l2_word key = vm->ops[vm->iptr++];
			l2_word val = vm->stack[vm->sptr - 1];
			struct l2_vm_value *ns = &vm->values[vm->fstack[vm->fsptr - 1].ns];
			l2_vm_namespace_replace(vm, ns, key, val); // TODO: error if returns -1
		}
		break;

	case L2_OP_RET:
		{
			l2_word retval = vm->stack[--vm->sptr];
			l2_word retptr = vm->fstack[vm->fsptr - 1].retptr;
			l2_word sptr = vm->fstack[vm->fsptr - 1].sptr;
			vm->fsptr -= 1;
			vm->sptr = sptr;
			vm->stack[vm->sptr++] = retval;
			vm->iptr = retptr;
		}
		break;

	case L2_OP_ALLOC_NONE:
		vm->stack[vm->sptr++] = 0;
		break;

	case L2_OP_ALLOC_ATOM:
		word = alloc_val(vm);
		vm->values[word].flags = L2_VAL_TYPE_ATOM;
		vm->values[word].atom = vm->ops[vm->iptr++];
		vm->stack[vm->sptr++] = word;
		break;

	case L2_OP_ALLOC_REAL:
		{
			word = alloc_val(vm);
			l2_word high = vm->ops[vm->iptr++];
			l2_word low = vm->ops[vm->iptr++];
			vm->values[word].flags = L2_VAL_TYPE_REAL;
			vm->values[word].real = u32s_to_double(high, low);
			vm->stack[vm->sptr++] = word;
		}
		break;

	case L2_OP_ALLOC_BUFFER_STATIC:
		{
			word = alloc_val(vm);
			l2_word length = vm->ops[vm->iptr++];
			l2_word offset = vm->ops[vm->iptr++];
			vm->values[word].flags = L2_VAL_TYPE_BUFFER;
			vm->values[word].buffer = malloc(sizeof(struct l2_vm_buffer) + length);
			vm->values[word].buffer->len = length;
			memcpy(
				(unsigned char *)vm->values[word].buffer + sizeof(struct l2_vm_buffer),
				vm->ops + offset, length);
			vm->stack[vm->sptr] = word;
			vm->sptr += 1;
		}
		break;

	case L2_OP_ALLOC_ARRAY:
		{
			l2_word count = vm->ops[vm->iptr++];
			l2_word arr_id = alloc_val(vm);
			struct l2_vm_value *arr = &vm->values[arr_id];
			arr->flags = L2_VAL_TYPE_ARRAY;
			if (count == 0) {
				arr->array = NULL;
				vm->stack[vm->sptr++] = arr_id;
				break;
			}

			arr->array = malloc(sizeof(struct l2_vm_array) + count * sizeof(l2_word));
			arr->array->len = count;
			arr->array->size = count;
			for (l2_word i = 0; i < count; ++i) {
				arr->array->data[count - 1 - i] = vm->stack[--vm->sptr];
			}

			vm->stack[vm->sptr++] = arr_id;
		}
		break;

	case L2_OP_ALLOC_NAMESPACE:
		word = alloc_val(vm);
		vm->values[word].flags = L2_VAL_TYPE_NAMESPACE;
		vm->values[word].extra.ns_parent = 0;
		vm->values[word].ns = NULL; // Will be allocated on first insert
		vm->stack[vm->sptr] = word;
		vm->sptr += 1;
		break;

	case L2_OP_ALLOC_FUNCTION:
		word = alloc_val(vm);
		vm->values[word].flags = L2_VAL_TYPE_FUNCTION;
		vm->values[word].func.pos = vm->ops[vm->iptr++];
		vm->values[word].func.ns = vm->fstack[vm->fsptr - 1].ns;
		vm->stack[vm->sptr] = word;
		vm->sptr += 1;
		break;

	case L2_OP_NAMESPACE_SET:
		{
			l2_word key = vm->ops[vm->iptr++];
			l2_word val = vm->stack[vm->sptr - 1];
			l2_word ns = vm->stack[vm->sptr - 2];
			l2_vm_namespace_set(&vm->values[ns], key, val);
		}
		break;

	case L2_OP_NAMESPACE_LOOKUP:
		{
			l2_word key = vm->ops[vm->iptr++];
			l2_word ns = vm->stack[--vm->sptr];
			vm->stack[vm->sptr++] = l2_vm_namespace_get(vm, &vm->values[ns], key);
		}
		break;

	case L2_OP_ARRAY_LOOKUP:
		{
			l2_word key = vm->ops[vm->iptr++];
			l2_word arr = vm->stack[--vm->sptr];
			// TODO: Error if out of bounds or incorrect type
			vm->stack[vm->sptr++] = vm->values[arr].array->data[key];
		}
		break;

	case L2_OP_ARRAY_SET:
		{
			l2_word key = vm->ops[vm->iptr++];
			l2_word val = vm->stack[vm->sptr - 1];
			l2_word arr = vm->stack[vm->sptr - 2];
			// TODO: Error if out of bounds or incorrect type
			vm->values[arr].array->data[key] = val;
		}
		break;

	case L2_OP_DYNAMIC_LOOKUP:
		{
			l2_word key_id = vm->stack[--vm->sptr];
			l2_word container_id = vm->stack[--vm->sptr];

			struct l2_vm_value *key = &vm->values[key_id];
			struct l2_vm_value *container = &vm->values[container_id];
			if (
					l2_vm_value_type(key) == L2_VAL_TYPE_REAL &&
					l2_vm_value_type(container) == L2_VAL_TYPE_ARRAY) {
				// TODO: Error if out of bounds
				vm->stack[vm->sptr++] = container->array->data[(size_t)key->real];
			} else if (
					l2_vm_value_type(key) == L2_VAL_TYPE_ATOM &&
					l2_vm_value_type(container) == L2_VAL_TYPE_NAMESPACE) {
				// TODO: Error if out of bounds
				vm->stack[vm->sptr++] = l2_vm_namespace_get(vm, container, key->atom);
			} else {
				// TODO: error
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

			if (
					l2_vm_value_type(key) == L2_VAL_TYPE_REAL &&
					l2_vm_value_type(container) == L2_VAL_TYPE_ARRAY) {
				// TODO: Error if out of bounds
				container->array->data[(size_t)key->real] = val;
			} else if (
					l2_vm_value_type(key) == L2_VAL_TYPE_ATOM &&
					l2_vm_value_type(container) == L2_VAL_TYPE_NAMESPACE) {
				// TODO: Error if out of bounds
				l2_vm_namespace_set(container, key->atom, val);
			} else {
				// TODO: error
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
