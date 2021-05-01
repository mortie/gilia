#include "bitset.h"

#include <stdio.h>
#include <snow/snow.h>

describe(gil_bitset) {
	struct gil_bitset set;

	before_each() {
		gil_bitset_init(&set);
	}

	after_each() {
		gil_bitset_free(&set);
	}

	test("set next") {
		asserteq(gil_bitset_set_next(&set), 0);
		asserteq(gil_bitset_set_next(&set), 1);
		asserteq(gil_bitset_set_next(&set), 2);
		asserteq(gil_bitset_set_next(&set), 3);
		asserteq(gil_bitset_set_next(&set), 4);
		asserteq(gil_bitset_set_next(&set), 5);
	}

	test("set a whole bunch of values") {
		for (int i = 0; i < 100000; ++i) {
			asserteq(gil_bitset_set_next(&set), i);
		}
	}

	test("iterating") {
		for (int i = 0; i < 10000; ++i) {
			asserteq(gil_bitset_set_next(&set), i);
		}

		gil_bitset_unset(&set, 100);
		gil_bitset_unset(&set, 50);
		gil_bitset_unset(&set, 37);
		gil_bitset_unset(&set, 32);
		gil_bitset_unset(&set, 31);
		gil_bitset_unset(&set, 33);

		struct gil_bitset_iterator it;
		gil_bitset_iterator_init(&it, &set);
		size_t val;
		size_t expected = 0;
		while (gil_bitset_iterator_next(&it, &set, &val)) {
			asserteq(val, expected);

			expected += 1;
			while (
					expected == 100 || expected == 50 || expected == 37 ||
					expected == 32 || expected == 31 || expected == 33) {
				expected += 1;
			}
		}

		asserteq(expected, 10000);
	}

	test("iterating from a start index") {
		for (int i = 0; i < 10000; ++i) {
			asserteq(gil_bitset_set_next(&set), i);
		}

		gil_bitset_unset(&set, 100);
		gil_bitset_unset(&set, 330);
		gil_bitset_unset(&set, 37);
		gil_bitset_unset(&set, 32);
		gil_bitset_unset(&set, 31);
		gil_bitset_unset(&set, 574);

		struct gil_bitset_iterator it;
		gil_bitset_iterator_init_from(&it, &set, 80);
		size_t val;
		size_t expected = 80;
		while (gil_bitset_iterator_next(&it, &set, &val)) {
			asserteq(val, expected);

			expected += 1;
			while (
					expected == 100 || expected == 330 || expected == 37 ||
					expected == 32 || expected == 31 || expected == 574) {
				expected += 1;
			}
		}

		asserteq(expected, 10000);
	}
}
