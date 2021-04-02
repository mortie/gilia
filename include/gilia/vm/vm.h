#ifndef GIL_VM_H
#define GIL_VM_H

#include <stdlib.h>

#include "../bytecode.h"
#include "../bitset.h"
#include "../io.h"

struct gil_vm;
struct gil_vm_array;
typedef gil_word (*gil_vm_cfunction)(struct gil_vm *vm, gil_word argc, gil_word *argv);
typedef gil_word (*gil_vm_contcallback)(struct gil_vm *vm, gil_word retval, gil_word cont);
typedef void (*gil_vm_gcmarker)(
		struct gil_vm *vm, void *data, void (*mark)(struct gil_vm *vm, gil_word id));

enum gil_value_type {
	GIL_VAL_TYPE_NONE,
	GIL_VAL_TYPE_ATOM,
	GIL_VAL_TYPE_REAL,
	GIL_VAL_TYPE_BUFFER,
	GIL_VAL_TYPE_ARRAY,
	GIL_VAL_TYPE_NAMESPACE,
	GIL_VAL_TYPE_FUNCTION,
	GIL_VAL_TYPE_CFUNCTION,
	GIL_VAL_TYPE_CONTINUATION,
	GIL_VAL_TYPE_RETURN,
	GIL_VAL_TYPE_ERROR,
};

const char *gil_value_type_name(enum gil_value_type typ);

enum gil_value_flags {
	GIL_VAL_MARKED = 1 << 7,
	GIL_VAL_CONST = 1 << 6,
	GIL_VAL_SBO = 1 << 5,
};

struct gil_vm_contcontext {
	gil_vm_contcallback callback;
	gil_vm_gcmarker marker;
	gil_word args;
};

// The smallest size an gil_vm_value can be is 16 bytes on common platforms.
// Pointers are 8 bytes, and since we need to store _at least_ 1 pointer +
// 1 byte for flags, it's going to be padded up to 16 bytes anyways.
// Might as well store some useful extra info in here.
struct gil_vm_value {
	union {
		uint8_t flags;

		struct {
			uint8_t padding;
			gil_word atom;
		} atom;

		struct {
			uint8_t padding;
			double real;
		} real;

		struct {
			uint8_t padding;
			gil_word length;
			char *buffer;
		} buffer;

		struct {
			uint8_t padding;
			gil_word length;

			union {
				struct gil_vm_array *array;
				gil_word shortarray[2];
			};
		} array;

		struct {
			uint8_t padding;
			gil_word parent;
			struct gil_vm_namespace *ns;
		} ns;

		struct {
			uint8_t padding;
			gil_word pos;
			gil_word ns;
		} func;

		struct {
			uint8_t padding;
			gil_vm_cfunction func;
		} cfunc;

		struct {
			uint8_t padding;
			gil_word call;
			struct gil_vm_contcontext *cont;
		} cont;

		struct {
			uint8_t padding;
			gil_word ret;
		} ret;

		struct {
			uint8_t padding;
			char *error;
		} error;
	};
};

#define gil_value_get_type(val) ((enum gil_value_type)((val)->flags & 0x0f))

struct gil_vm_array {
	size_t size;
	gil_word data[];
};

gil_word *gil_vm_array_data(struct gil_vm *vm, struct gil_vm_value *val);
gil_word gil_vm_array_get(struct gil_vm *vm, struct gil_vm_value *val, gil_word k);
gil_word gil_vm_array_set(struct gil_vm *vm, struct gil_vm_value *val, gil_word k, gil_word v);

struct gil_vm_namespace {
	size_t len;
	size_t size;
	gil_word mask;
	gil_word data[];
};

gil_word gil_vm_namespace_get(struct gil_vm *vm, struct gil_vm_value *ns, gil_word key);
void gil_vm_namespace_set(struct gil_vm_value *ns, gil_word key, gil_word val);
int gil_vm_namespace_replace(struct gil_vm *vm, struct gil_vm_value *ns, gil_word key, gil_word val);

struct gil_vm_stack_frame {
	gil_word ns;
	gil_word sptr;
	gil_word retptr;
	gil_word args;
};

struct gil_vm {
	int halted;
	int need_gc;
	int need_check_retval;
	unsigned char *ops;
	size_t opslen;
	gil_word iptr;

	struct gil_io_writer *std_output;
	struct gil_io_writer *std_error;

	struct gil_vm_value *values;
	size_t valuessize;
	struct gil_bitset valueset;

	gil_word stack[1024];
	gil_word sptr;

	struct gil_vm_stack_frame fstack[1024];
	gil_word fsptr;

	gil_word knone, ktrue, kfalse, kstop;
	gil_word gc_start;
};

void gil_vm_init(struct gil_vm *vm, unsigned char *ops, size_t opslen);
gil_word gil_vm_alloc(struct gil_vm *vm, enum gil_value_type typ, enum gil_value_flags flags);
gil_word gil_vm_error(struct gil_vm *vm, const char *fmt, ...);
gil_word gil_vm_type_error(struct gil_vm *vm, struct gil_vm_value *val);
void gil_vm_free(struct gil_vm *vm);
void gil_vm_step(struct gil_vm *vm);
void gil_vm_run(struct gil_vm *vm);
size_t gil_vm_gc(struct gil_vm *vm);
int gil_vm_val_is_true(struct gil_vm *vm, struct gil_vm_value *val);

#endif
