#include "vm/vm.h"

#include <snow/snow.h>

static struct gil_vm vm = {0};

describe(gil_vm_namespace) {
	struct gil_vm_value val = {0};

	after_each() {
		free(val.ns);
		val.ns = NULL;
	}

	test("basic functionality") {
		gil_vm_namespace_set(&val, 100, 50);
		gil_vm_namespace_set(&val, 30, 600);
		asserteq(gil_vm_namespace_get(&vm, &val, 100), 50);
		asserteq(gil_vm_namespace_get(&vm, &val, 30), 600);
	}

	it("handles duplicates") {
		gil_vm_namespace_set(&val, 536, 600);
		gil_vm_namespace_set(&val, 100, 400);
		gil_vm_namespace_set(&val, 536, 45);
		asserteq(gil_vm_namespace_get(&vm, &val, 100), 400);
		asserteq(gil_vm_namespace_get(&vm, &val, 536), 45);
	}

	it("handles a whole bunch of values") {
		for (int i = 1; i < 500; ++i) {
			gil_vm_namespace_set(&val, i, i + 50);
			asserteq(gil_vm_namespace_get(&vm, &val, i), i + 50);
		}

		for (int i = 1; i < 500; ++i) {
			asserteq(gil_vm_namespace_get(&vm, &val, i), i + 50);
		}
	}
}
