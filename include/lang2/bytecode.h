#ifndef L2_BYTECODE_H
#define L2_BYTECODE_H

#include <stdint.h>

typedef uint32_t l2_word;

enum l2_opcode {
	L2_OP_NOP,
	L2_OP_PUSH,
	L2_OP_POP,
	L2_OP_DUP,
	L2_OP_ADD,
	L2_OP_JUMP,
	L2_OP_CALL,
	L2_OP_RET,
	L2_OP_ALLOC_INTEGER_32,
	L2_OP_ALLOC_INTEGER_64,
	L2_OP_ALLOC_REAL_32,
	L2_OP_ALLOC_REAL_64,
	L2_OP_ALLOC_ARRAY,
	L2_OP_ALLOC_BUFFER,
	L2_OP_ALLOC_BUFFER_CONST,
	L2_OP_HALT,
};

#endif
