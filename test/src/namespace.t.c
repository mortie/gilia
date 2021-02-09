#include "vm/vm.h"

#include <snow/snow.h>

static struct l2_vm vm = {0};

describe(l2_vm_namespace) {
	struct l2_vm_value val = {0};

	after_each() {
		free(val.ns);
		val.ns = NULL;
	}

	test("basic functionality") {
		l2_vm_namespace_set(&val, 100, 50);
		l2_vm_namespace_set(&val, 30, 600);
		asserteq(l2_vm_namespace_get(&vm, &val, 100), 50);
		asserteq(l2_vm_namespace_get(&vm, &val, 30), 600);
	}

	it("handles duplicates") {
		l2_vm_namespace_set(&val, 536, 600);
		l2_vm_namespace_set(&val, 100, 400);
		l2_vm_namespace_set(&val, 536, 45);
		asserteq(l2_vm_namespace_get(&vm, &val, 100), 400);
		asserteq(l2_vm_namespace_get(&vm, &val, 536), 45);
	}

	it("handles a whole bunch of values") {
		for (int i = 1; i < 500; ++i) {
			l2_vm_namespace_set(&val, i, i + 50);
			asserteq(l2_vm_namespace_get(&vm, &val, i), i + 50);
		}

		for (int i = 1; i < 500; ++i) {
			asserteq(l2_vm_namespace_get(&vm, &val, i), i + 50);
		}
	}
}
