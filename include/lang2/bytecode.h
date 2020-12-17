#ifndef L2_BYTECODE_H
#define L2_BYTECODE_H

#include <stdint.h>

typedef uint32_t l2_word;

enum l2_opcode {
	/*
	 * Do nothing.
	 */
	L2_OP_NOP,

	/*
	 * Push a value to the stack.
	 * Push <word>
	 */
	L2_OP_PUSH,

	/*
	 * Push a value to the stack.
	 * Push <word1>
	 * Push <word2>
	 */
	L2_OP_PUSH_2,

	/*
	 * Discard the top element from the stack.
	 * Pop <word>
	 */
	L2_OP_POP,

	/*
	 * Duplicate the top element on the stack.
	 * Push <word at <sptr> - 1>
	 */
	L2_OP_DUP,

	/*
	 * Add two words.
	 * Pop <word1>
	 * Pop <word2>
	 * Push <word1> + <word2>
	 */
	L2_OP_ADD,

	/*
	 * Call a function.
	 * Pop <word>
	 * Push <iptr> + 1
	 * Jump to <word>
	 */
	L2_OP_CALL,

	/*
	 * Generate a stack frame.
	 * Alloc namespace <var>
	 * NSPush <var>
	 */
	L2_OP_GEN_STACK_FRAME,

	/*
	 * Look up a value from the current stack frame.
	 * Pop <word>
	 * Find <val> in stack frame using <word>
	 * Push <val>
	 */
	L2_OP_STACK_FRAME_LOOKUP,

	/*
	 * Set a value in the current stack frame.
	 * Pop <key>
	 * Read <val>
	 * Assign <val> to stack frame
	 */
	L2_OP_STACK_FRAME_SET,

	/*
	 * Return from a function.
	 * NSPop
	 * Pop <word>
	 * Jump to <word>
	 */
	L2_OP_RET,

	/*
	 * Allocate an integer from one word.
	 * Pop <word>
	 * Alloc integer <var> from <word>
	 * Push <var>
	 */
	L2_OP_ALLOC_INTEGER_32,

	/*
	 * Allocate an integer from two words.
	 * Pop <word1>
	 * Pop <word2>
	 * Alloc integer <var> from <word1> << 32 | <word2>
	 * Push <var>
	 */
	L2_OP_ALLOC_INTEGER_64,

	/*
	 * Allocate a real from one word.
	 * Pop <word>
	 * Alloc real <var> from <word>
	 * Push <var>
	 */
	L2_OP_ALLOC_REAL_32,

	/*
	 * Allocate a real from two words.
	 * Pop <high>
	 * Pop <low>
	 * Alloc real <var> from <high> << 32 | <low>
	 * Push <var>
	 */
	L2_OP_ALLOC_REAL_64,

	/*
	 * Allocate a buffer from static data.
	 * Pop <word1>
	 * Pop <word2>
	 * Alloc buffer <var> with length=<word1>, offset=<word2>
	 * Push <var>
	 */
	L2_OP_ALLOC_BUFFER_STATIC,

	/*
	 * Allocate a zeroed buffer.
	 * Pop <word>
	 * Alloc buffer <var> with length=<word>
	 * Push <var>
	 */
	L2_OP_ALLOC_BUFFER_ZERO,

	/*
	 * Allocate an array.
	 * Alloc array <var>
	 * Push <var>
	 */
	L2_OP_ALLOC_ARRAY,

	/*
	 * Allocate an integer->value map.
	 * Alloc namespace <var>
	 * Push <var>
	 */
	L2_OP_ALLOC_NAMESPACE,

	/*
	 * Set a namespace's name to a value.
	 * Pop <key>
	 * Read <val>
	 * Read <ns>
	 * Assign <val> to <ns[<key>]>
	 */
	L2_OP_NAMESPACE_SET,

	/*
	 * Halt execution.
	 */
	L2_OP_HALT,
};

#endif
