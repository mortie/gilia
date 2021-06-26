#ifndef GIL_VM_PRINT_H
#define GIL_VM_PRINT_H

#include <stddef.h>

struct gil_vm;
struct gil_vm_value;
struct gil_io_writer;

void gil_vm_print_val(struct gil_io_writer *w, struct gil_vm_value *val);
void gil_vm_print_state(struct gil_io_writer *w, struct gil_vm *vm);
void gil_vm_print_heap(struct gil_io_writer *w, struct gil_vm *vm);
void gil_vm_print_stack(struct gil_io_writer *w, struct gil_vm *vm);
void gil_vm_print_fstack(struct gil_io_writer *w, struct gil_vm *vm);

void gil_vm_print_op(struct gil_io_writer *w, unsigned char *ops, size_t opcount, size_t *ptr);
void gil_vm_print_bytecode(struct gil_io_writer *w, unsigned char *ops, size_t opcount);

#endif
