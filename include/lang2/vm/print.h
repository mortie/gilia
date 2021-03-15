#ifndef L2_VM_PRINT_H
#define L2_VM_PRINT_H

#include "vm.h"

void l2_vm_print_val(struct l2_vm_value *val);
void l2_vm_print_state(struct l2_vm *vm);
void l2_vm_print_heap(struct l2_vm *vm);
void l2_vm_print_stack(struct l2_vm *vm);
void l2_vm_print_fstack(struct l2_vm *vm);

void l2_vm_print_op(unsigned char *ops, size_t opcount, size_t *ptr);
void l2_vm_print_bytecode(unsigned char *ops, size_t opcount);

#endif
