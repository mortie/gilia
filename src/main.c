#include "vm/vm.h"
#include "bitmap.h"

#include <stdio.h>
#include <assert.h>

int main() {
	struct l2_bitmap bm;
	l2_bitmap_init(&bm);
	for (size_t i = 0; i < 8191; ++i) {
		size_t id = l2_bitmap_set_next(&bm);
		assert(id == i);
		assert(l2_bitmap_get(&bm, i));
	}

	for (size_t i = 0; i < 10000; ++i) {
		if (i < 8191) {
			assert(l2_bitmap_get(&bm, i));
		} else {
			assert(!l2_bitmap_get(&bm, i));
		}
	}

	l2_bitmap_unset(&bm, 100);
	assert(l2_bitmap_set_next(&bm) == 8191);
	assert(l2_bitmap_set_next(&bm) == 100);

	l2_bitmap_free(&bm);
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
