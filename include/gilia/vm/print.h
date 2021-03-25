#ifndef GIL_VM_PRINT_H
#define GIL_VM_PRINT_H

#include "vm.h"

void gil_vm_print_val(struct gil_vm_value *val);
void gil_vm_print_state(struct gil_vm *vm);
void gil_vm_print_heap(struct gil_vm *vm);
void gil_vm_print_stack(struct gil_vm *vm);
void gil_vm_print_fstack(struct gil_vm *vm);

void gil_vm_print_op(unsigned char *ops, size_t opcount, size_t *ptr);
void gil_vm_print_bytecode(unsigned char *ops, size_t opcount);

#endif
