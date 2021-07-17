#include "vm/print.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "io.h"
#include "bitset.h"
#include "bytecode.h"
#include "vm/vm.h"

static gil_word read_uint(unsigned char *ops, size_t *ptr) {
	gil_word word = 0;
	while (ops[*ptr] >= 0x80) {
		word |= ops[(*ptr)++] & 0x7f;
		word <<= 7;
	}

	word |= ops[(*ptr)++];
	return word;
}

static gil_word read_u4le(unsigned char *ops, size_t *ptr) {
	unsigned char *data = &ops[*ptr];
	gil_word integer = 0 |
		(uint64_t)data[0] |
		(uint64_t)data[1] << 8 |
		(uint64_t)data[2] << 16 |
		(uint64_t)data[3] << 24;

	*ptr += 4;
	return integer;
}

static double read_d8le(unsigned char *ops, size_t *ptr) {
	unsigned char *data = &ops[*ptr];
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
	*ptr += 8;
	return num;
}

void gil_vm_print_val(struct gil_io_writer *w, struct gil_vm_value *val) {
	switch (gil_value_get_type(val)) {
	case GIL_VAL_TYPE_NONE:
		gil_io_printf(w, "NONE");
		break;

	case GIL_VAL_TYPE_ATOM:
		gil_io_printf(w, "ATOM %u", val->atom.atom);
		break;

	case GIL_VAL_TYPE_REAL:
		gil_io_printf(w, "REAL %f", val->real.real);
		break;

	case GIL_VAL_TYPE_ARRAY: {
		gil_io_printf(w, "ARRAY, len %u", val->array.length);
		gil_word *data;
		if (val->flags & GIL_VAL_SBO) {
			data = val->array.shortarray;
		} else {
			data = val->array.array->data;
		}
		for (size_t i = 0; i < val->array.length; ++i) {
			gil_io_printf(w, "    %zu: %u\n", i, data[i]);
		}
	}
		break;

	case GIL_VAL_TYPE_BUFFER: {
		if (val->buffer.buffer == NULL) {
			gil_io_printf(w, "BUFFER, empty");
			return;
		}

		gil_io_printf(w, "BUFFER, len %u", val->buffer.length);
		for (size_t i = 0; i < val->buffer.length; ++i) {
			gil_io_printf(w, "\n    %zu: %c", i, val->buffer.buffer[i]);
		}
	}
		break;

	case GIL_VAL_TYPE_NAMESPACE: {
		if (val->ns.ns == NULL) {
			gil_io_printf(w, "NAMESPACE, empty, parent %u", val->ns.parent);
			return;
		}

		gil_io_printf(w, "NAMESPACE, len %zu, parent %u", val->ns.ns->len, val->ns.parent);
		for (size_t i = 0; i < val->ns.ns->size; ++i) {
			gil_word key = val->ns.ns->data[i];
			gil_word v = val->ns.ns->data[val->ns.ns->size + i];
			if (key == 0 || key == ~(gil_word)0) continue;
			gil_io_printf(w, "\n    %u: %u", key, v);
		}
	}
		break;

	case GIL_VAL_TYPE_FUNCTION:
		gil_io_printf(w, "FUNCTION, pos %u, ns %u", val->func.pos, val->func.ns);
		break;

	case GIL_VAL_TYPE_CFUNCTION:
		// ISO C doesn't let you cast a function pointer to void*.
		gil_io_printf(w, "C FUNCTION, 0x%08jx, mod %u, self %u",
				(uintmax_t)val->cfunc.func, val->cfunc.mod, val->cfunc.self);
		break;

	case GIL_VAL_TYPE_CVAL:
		// ISO C doesn't let you cast a function pointer to void*.
		gil_io_printf(w, "C VALUE, 0x%08jx, type %u, ns %u",
				(uintmax_t)val->cval.cval, val->cval.ctype, val->cval.ns);
		break;

	case GIL_VAL_TYPE_CONTINUATION:
		gil_io_printf(w, "CONTINUATION, call %u, cont 0x%08jx",
				val->cont.call, (uintmax_t)val->cont.cont);
		break;

	case GIL_VAL_TYPE_RETURN:
		gil_io_printf(w, "RETURN, ret %u", val->ret.ret);
		break;

	case GIL_VAL_TYPE_ERROR:
		gil_io_printf(w, "ERROR, %s", val->error.error);
		break;
	}
}

void gil_vm_print_state(struct gil_io_writer *w, struct gil_vm *vm) {
	gil_io_printf(w, "Stack:\n");
	gil_vm_print_stack(w, vm);
	gil_io_printf(w, "Heap:\n");
	gil_vm_print_heap(w, vm);
	gil_io_printf(w, "Frame Stack:\n");
	gil_vm_print_fstack(w, vm);
}

void gil_vm_print_heap(struct gil_io_writer *w, struct gil_vm *vm) {
	for (gil_word i = 0; i < vm->valuessize; ++i) {
		if (gil_bitset_get(&vm->valueset, i)) {
			gil_io_printf(w, "  %u: ", i);
			gil_vm_print_val(w, &vm->values[i]);
			gil_io_printf(w, "\n");
		}
	}
}

void gil_vm_print_stack(struct gil_io_writer *w, struct gil_vm *vm) {
	for (gil_word i = 0; i < vm->sptr; ++i) {
		gil_io_printf(w, "  %i: %i\n", i, vm->stack[i]);
	}
}

void gil_vm_print_fstack(struct gil_io_writer *w, struct gil_vm *vm) {
	for (gil_word i = 0; i < vm->fsptr; ++i) {
		gil_io_printf(w, "  %i: %i, ret %i, stack base %u, args %u\n",
				i, vm->fstack[i].ns, (int)vm->fstack[i].retptr,
				vm->fstack[i].sptr, vm->fstack[i].args);
	}
}

void gil_vm_print_op(struct gil_io_writer *w, unsigned char *ops, size_t opcount, size_t *ptr) {
	enum gil_opcode opcode = (enum gil_opcode)ops[(*ptr)++];

	switch (opcode) {
	case GIL_OP_NOP:
		gil_io_printf(w, "NOP");
		return;

	case GIL_OP_DISCARD:
		gil_io_printf(w, "DISCARD");
		return;

	case GIL_OP_SWAP_DISCARD:
		gil_io_printf(w, "SWAP_DISCARD");
		return;

	case GIL_OP_FUNC_CALL:
		gil_io_printf(w, "FUNC_CALL %u", read_uint(ops, ptr));
		return;

	case GIL_OP_FUNC_CALL_INFIX:
		gil_io_printf(w, "FUNC_CALL_INFIX");
		return;

	case GIL_OP_RJMP:
		gil_io_printf(w, "RJMP %u", read_uint(ops, ptr));
		return;

	case GIL_OP_RJMP_U4:
		gil_io_printf(w, "RJMP %u", read_u4le(ops, ptr));
		return;

	case GIL_OP_STACK_FRAME_GET_ARGS:
		gil_io_printf(w, "STACK_FRAME_GET_ARGS");
		return;

	case GIL_OP_STACK_FRAME_GET_ARG:
		gil_io_printf(w, "STACK_FRAME_GET_ARG %u", read_uint(ops, ptr));
		return;

	case GIL_OP_STACK_FRAME_LOOKUP:
		gil_io_printf(w, "STACK_FRAME_LOOKUP %u", read_uint(ops, ptr));
		return;

	case GIL_OP_STACK_FRAME_SET:
		gil_io_printf(w, "STACK_FRAME_SET %u", read_uint(ops, ptr));
		return;

	case GIL_OP_STACK_FRAME_REPLACE:
		gil_io_printf(w, "STACK_FRAME_REPLACE %u", read_uint(ops, ptr));
		return;

	case GIL_OP_ASSERT:
		gil_io_printf(w, "ASSERT");
		return;

	case GIL_OP_NAMED_PARAM: {
		gil_word key = read_uint(ops, ptr);
		gil_word idx = read_uint(ops, ptr);
		gil_io_printf(w, "NAMED_PARAM %u %u", key, idx);
	}
		return;

	case GIL_OP_RET:
		gil_io_printf(w, "RET");
		return;

	case GIL_OP_ALLOC_NONE:
		gil_io_printf(w, "ALLOC_NONE");
		return;

	case GIL_OP_ALLOC_ATOM:
		gil_io_printf(w, "ALLOC_ATOM %u", read_uint(ops, ptr));
		return;

	case GIL_OP_ALLOC_REAL:
		gil_io_printf(w, "ALLOC_REAL %f", read_d8le(ops, ptr));
		return;

	case GIL_OP_ALLOC_BUFFER_STATIC: {
		gil_word w1 = read_uint(ops, ptr);
		gil_word w2 = read_uint(ops, ptr);;
		gil_io_printf(w, "ALLOC_BUFFER_STATIC %u %u", w1, w2);
	}
		return;

	case GIL_OP_ALLOC_ARRAY:
		gil_io_printf(w, "ALLOC_ARRAY %u", read_uint(ops, ptr));
		return;

	case GIL_OP_ALLOC_NAMESPACE:
		gil_io_printf(w, "ALLOC_NAMESPACE");
		return;

	case GIL_OP_ALLOC_FUNCTION:
		gil_io_printf(w, "ALLOC_FUNCTION %u", read_uint(ops, ptr));
		return;

	case GIL_OP_NAMESPACE_SET:
		gil_io_printf(w, "NAMESPACE_SET %u", read_uint(ops, ptr));
		return;

	case GIL_OP_NAMESPACE_LOOKUP:
		gil_io_printf(w, "NAMESPACE_LOOKUP %u", read_uint(ops, ptr));
		return;

	case GIL_OP_ARRAY_LOOKUP:
		gil_io_printf(w, "ARRAY_LOOKUP %u", read_uint(ops, ptr));
		return;

	case GIL_OP_ARRAY_SET:
		gil_io_printf(w, "ARRAY_SET %u", read_uint(ops, ptr));
		return;

	case GIL_OP_DYNAMIC_LOOKUP:
		gil_io_printf(w, "DYNAMIC_LOOKUP");
		return;

	case GIL_OP_DYNAMIC_SET:
		gil_io_printf(w, "DYNAMIC_SET");
		return;

	case GIL_OP_LOAD_CMODULE:
		gil_io_printf(w, "LOAD_CMODULE %u", read_uint(ops, ptr));
		return;

	case GIL_OP_REGISTER_MODULE: {
		gil_word w1 = read_uint(ops, ptr);
		gil_word w2 = read_uint(ops, ptr);
		gil_io_printf(w, "LOAD_MODULE %u %u", w1, w2);
	}
		return;

	case GIL_OP_LOAD_MODULE:
		gil_io_printf(w, "LOAD_MODULE %u", read_uint(ops, ptr));
		return;

	case GIL_OP_HALT:
		gil_io_printf(w, "HALT");
		return;
	}

	gil_io_printf(w, "? 0x%02x", opcode);
}

void gil_vm_print_bytecode(struct gil_io_writer *w, unsigned char *ops, size_t opcount) {
	size_t ptr = 0;
	while (ptr < opcount) {
		gil_io_printf(w, "%04zu ", ptr);
		gil_vm_print_op(w, ops, opcount, &ptr);
		gil_io_printf(w, "\n");
	}
}
