#ifndef L2_BYTECODE_H
#define L2_BYTECODE_H

#include <stdint.h>

typedef uint32_t l2_word;

enum l2_opcode {
	L2_OP_PUSH,
	L2_OP_ADD,
	L2_OP_JUMP,
	L2_OP_CALL,
	L2_OP_ALLOC_STRING,
	L2_OP_ALLOC_ARRAY,
	L2_OP_HALT,
};

struct l2_op {
	enum l2_opcode code;
	l2_word val;
};

#endif
