#include "strset.h"

#include <stdio.h>
#include <snow/snow.h>

describe(l2_strset) {
	struct l2_strset set;

	before_each() {
		l2_strset_init(&set);
	}

	after_each() {
		l2_strset_free(&set);
	}

	test("basic functionality") {
		asserteq(l2_strset_put_copy(&set, "hello"), 1);
		asserteq(l2_strset_put_copy(&set, "world"), 2);
		asserteq(l2_strset_get(&set, "hello"), 1);
		asserteq(l2_strset_get(&set, "world"), 2);
	}

	it("handles duplicates") {
		asserteq(l2_strset_put_copy(&set, "hello"), 1);
		asserteq(l2_strset_put_copy(&set, "hello"), 1);
		asserteq(l2_strset_put_copy(&set, "world"), 2);
	}

	it("handles a whole bunch of values") {
		char buf[128];
		for (int i = 0; i < 1000; ++i) {
			sprintf(buf, "test-%d", i);
			asserteq(l2_strset_put_copy(&set, buf), i + 1);
		}

		for (int i = 0; i < 1000; ++i) {
			sprintf(buf, "test-%d", i);
			asserteq(l2_strset_get(&set, buf), i + 1);
		}
	}
}
