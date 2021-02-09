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
	 * Discard the top element from the stack.
	 * Pop <word>
	 */
	L2_OP_POP,

	/*
	 * Swap the top and second-top elements, then pop the new top element.
	 * Pop <word1>
	 * Pop <word2>
	 * Push <word1>
	 */
	L2_OP_SWAP_POP,

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
	 * Call a function; func_call <argc>
	 * Pop <argc> times
	 * Pop <func>
	 * Push array with args
	 * Call <func>
	 * (Before returning, the function will push a return value onto the stack)
	 */
	L2_OP_FUNC_CALL,

	/*
	 * Jump relative; rjmp <count>
	 * Jump <count> words forwards
	 */
	L2_OP_RJMP,

	/*
	 * Look up a value from the current stack frame; stack_frame_lookup <key>
	 * Find <val> in stack frame using <key>
	 * Push <val>
	 */
	L2_OP_STACK_FRAME_LOOKUP,

	/*
	 * Set a value in the current stack frame; stack_frame_set <key>
	 * Read <val>
	 * Assign <val> to stack frame at <key>
	 */
	L2_OP_STACK_FRAME_SET,

	/*
	 * Replace a value on the stack; stack_frame_replace <key>
	 * Read <val>
	 * Assign <val> to stack frame at <key>
	 */
	L2_OP_STACK_FRAME_REPLACE,

	/*
	 * Return from a function.
	 * FSPop
	 * Pop (discard args array)
	 * Jump to <return address>
	 */
	L2_OP_RET,

	/*
	 * Put a reference to none at the top of the stack.
	 * Push 0
	 */
	L2_OP_ALLOC_NONE,

	/*
	 * Allocate an atom from one word; alloc_atom <word>
	 * Alloc atom <var> from <word>
	 * Push <var>
	 */
	L2_OP_ALLOC_ATOM,

	/*
	 * Allocate a real from two words; alloc_real <high> <low>
	 * Alloc real <var> from <high> << 32 | <low>
	 * Push <var>
	 */
	L2_OP_ALLOC_REAL,

	/*
	 * Allocate a buffer from static data; alloc_buffer_static <length> <offset>
	 * Alloc buffer <var> with <length> and <offset>
	 * Push <var>
	 */
	L2_OP_ALLOC_BUFFER_STATIC,

	/*
	 * Allocate an array; <count>
	 * Pop <count> times
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
	 * Allocate a function; alloc_function <pos>
	 * Alloc function <var> pointing to location <word>
	 * Push <var>
	 */
	L2_OP_ALLOC_FUNCTION,

	/*
	 * Set a namespace's name to a value; namespace_set <key>
	 * Read <val>
	 * Read <ns>
	 * Assign <val> to <ns[<key>]>
	 */
	L2_OP_NAMESPACE_SET,

	/*
	 * Lookup a value from a namespace; namespace_lookup <key>
	 * Pop <ns>
	 * Push <ns[<key>]>
	 */
	L2_OP_NAMESPACE_LOOKUP,

	/*
	 * Look up a value from an array; array_lookup <key>
	 * Pop <arr>
	 * Push <arr[<key>]>
	 */
	L2_OP_ARRAY_LOOKUP,

	/*
	 * Set a value in an array; array_set <key>
	 * Read <val>
	 * Read <arr>
	 * Assign <val> to <arr[<key>]>
	 */
	L2_OP_ARRAY_SET,

	/*
	 * Look up a runtime value in an array or object.
	 * Pop <key>
	 * Pop <container>
	 * Push <container[<key>]>
	 */
	L2_OP_DYNAMIC_LOOKUP,

	/*
	 * Set a value in an array or object.
	 * Pop <val>
	 * Pop <key>
	 * Pop <arr>
	 * Assign <val> to <arr[<key>]>
	 */
	L2_OP_DYNAMIC_SET,

	/*
	 * Halt execution.
	 */
	L2_OP_HALT,
};

#endif
