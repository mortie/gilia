#include "vm/vm.h"
#include "bitset.h"

#include <stdio.h>
#include <assert.h>

int main() {
	struct l2_bitset bs;
	l2_bitset_init(&bs);

	for (size_t i = 0; i < 8191; ++i) {
		size_t id = l2_bitset_set_next(&bs);
		assert(id == i);
		assert(l2_bitset_get(&bs, i));
	}

	for (size_t i = 0; i < 10000; ++i) {
		if (i < 8191) {
			assert(l2_bitset_get(&bs, i));
		} else {
			assert(!l2_bitset_get(&bs, i));
		}
	}

	l2_bitset_unset(&bs, 100);
	assert(l2_bitset_set_next(&bs) == 8191);
	assert(l2_bitset_set_next(&bs) == 100);

	l2_bitset_free(&bs);
}

/*
int main() {
	struct l2_op ops[] = {
		{ L2_OP_PUSH, 100 },
		{ L2_OP_PUSH, 100 },
		{ L2_OP_ADD },
		{ L2_OP_HALT },
	};

	struct l2_vm vm;
	l2_vm_init(&vm, ops, sizeof(ops) / sizeof(*ops));

	while (vm.ops[vm.iptr].code != L2_OP_HALT) {
		printf("Exec %i\n", vm.ops[vm.iptr].code);
		l2_vm_step(&vm);
	}

	printf("Done. Stack:\n");
	for (l2_word i = 0; i < vm.sptr; ++i) {
		printf("  %i: %i\n", i, vm.stack[i]);
	}
}
*/
