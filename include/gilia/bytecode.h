#ifndef GIL_BYTECODE_H
#define GIL_BYTECODE_H

#include <stdint.h>

typedef uint32_t gil_word;

#define gil_bytecode_version 2

// These atoms have a special meaning which all modules, the code generator,
// the VM, and other consumers of bytecode must understand
#define GIL_BYTECODE_ATOMS \
	X("true", ktrue) \
	X("false", kfalse) \
	X("stop", kstop)

enum gil_opcode {
	/*
	 * Do nothing.
	 */
	GIL_OP_NOP,

	/*
	 * Discard the top element from the stack.
	 * Pop <word>
	 */
	GIL_OP_DISCARD,

	/*
	 * Swap the top and second-top elements, then pop the new top element.
	 * Pop <word1>
	 * Pop <word2>
	 * Push <word1>
	 */
	GIL_OP_SWAP_DISCARD,

	/*
	 * Call a function; func_call <argc>
	 * Pop <argc> times
	 * Pop <func>
	 * Push array with args
	 * Call <func>
	 * (Before returning, the function will push a return value onto the stack)
	 */
	GIL_OP_FUNC_CALL,

	/*
	 * Call an infix function
	 * Pop <rhs>
	 * Pop <func>
	 * Pop <lhs>
	 * Call <func>
	 * (Before returning, the function will push a return value onto the stack)
	 */
	GIL_OP_FUNC_CALL_INFIX,

	/*
	 * Jump relative; rjmp <count>
	 * Jump <count> words forwards
	 */
	GIL_OP_RJMP,

	/* 
	 * Jump relative; rjmp <count:u4>
	 * Jump <count> words forwards
	 * (Like GIL_OP_RJMP, but takes a u4 rather than a variable length number)
	 */
	GIL_OP_RJMP_U4,

	/* Get the arguments array from the stack frame
	 * Push <arguments array>
	 */
	GIL_OP_STACK_FRAME_GET_ARGS,

	/* Get the arguments array from the stack frame; stack_frame_get_arg <idx>
	 * Push <argument <idx>>
	 */
	GIL_OP_STACK_FRAME_GET_ARG,

	/*
	 * Look up a value from the current stack frame; stack_frame_lookup <key>
	 * Find <val> in stack frame using <key>
	 * Push <val>
	 */
	GIL_OP_STACK_FRAME_LOOKUP,

	/*
	 * Set a value in the current stack frame; stack_frame_set <key>
	 * Read <val>
	 * Assign <val> to stack frame at <key>
	 */
	GIL_OP_STACK_FRAME_SET,

	/*
	 * Replace a value on the stack; stack_frame_replace <key>
	 * Read <val>
	 * Assign <val> to stack frame at <key>
	 */
	GIL_OP_STACK_FRAME_REPLACE,

	/*
	 * Assert that the top of the stack is true; stack_frame_assert
	 * Pop <val>
	 * Error if <val> is false
	 */
	GIL_OP_ASSERT,

	/*
	 * Add a named parameter to the stack frame; named_param <key> <idx>
	 * Assign arguments[idk] to stack frame at <key>
	 */
	GIL_OP_NAMED_PARAM,

	/*
	 * Return from a function.
	 * Pop <retval>
	 * FSPop
	 * Reset stack pointer to <stack base>
	 * Push <retval>
	 * Jump to <return address>
	 */
	GIL_OP_RET,

	/*
	 * Put a reference to none at the top of the stack.
	 * Push 0
	 */
	GIL_OP_ALLOC_NONE,

	/*
	 * Allocate an atom from one word; alloc_atom <word>
	 * Alloc atom <var> from <word>
	 * Push <var>
	 */
	GIL_OP_ALLOC_ATOM,

	/*
	 * Allocate a real from two words; alloc_real <double:u8>
	 * Alloc real <var> from <double>
	 * Push <var>
	 */
	GIL_OP_ALLOC_REAL,

	/*
	 * Allocate a buffer from static data; alloc_buffer_static <length> <offset>
	 * Alloc buffer <var> with <length> and <offset>
	 * Push <var>
	 */
	GIL_OP_ALLOC_BUFFER_STATIC,

	/*
	 * Allocate an array; <count:u4>
	 * Pop <count> times
	 * Alloc array <var>
	 * Push <var>
	 */
	GIL_OP_ALLOC_ARRAY,

	/*
	 * Allocate an integer->value map.
	 * Alloc namespace <var>
	 * Push <var>
	 */
	GIL_OP_ALLOC_NAMESPACE,

	/*
	 * Allocate a function; alloc_function <pos>
	 * Alloc function <var> pointing to location <word>
	 * Push <var>
	 */
	GIL_OP_ALLOC_FUNCTION,

	/*
	 * Set a namespace's name to a value; namespace_set <key>
	 * Read <val>
	 * Read <ns>
	 * Assign <val> to <ns[<key>]>
	 */
	GIL_OP_NAMESPACE_SET,

	/*
	 * Lookup a value from a namespace; namespace_lookup <key>
	 * Pop <ns>
	 * Push <ns[<key>]>
	 */
	GIL_OP_NAMESPACE_LOOKUP,

	/*
	 * Look up a value from an array; array_lookup <key>
	 * Pop <arr>
	 * Push <arr[<key>]>
	 */
	GIL_OP_ARRAY_LOOKUP,

	/*
	 * Set a value in an array; array_set <key>
	 * Read <val>
	 * Read <arr>
	 * Assign <val> to <arr[<key>]>
	 */
	GIL_OP_ARRAY_SET,

	/*
	 * Look up a runtime value in an array or object.
	 * Pop <key>
	 * Pop <container>
	 * Push <container[<key>]>
	 */
	GIL_OP_DYNAMIC_LOOKUP,

	/*
	 * Set a value in an array or object.
	 * Pop <val>
	 * Pop <key>
	 * Pop <arr>
	 * Assign <val> to <arr[<key>]>
	 */
	GIL_OP_DYNAMIC_SET,

	/*
	 * Load a C module; load_cmodule <id>
	 */
	GIL_OP_LOAD_CMODULE,

	/*
	 * Halt execution.
	 */
	GIL_OP_HALT,
};

#endif
