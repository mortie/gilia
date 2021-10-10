#include "str.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <snow/snow.h>

describe(gil_strf) {
	char *str = NULL;
	after_each() {
		free(str);
	}

	test("basic functionality") {
		str = gil_strf("Hello World");
		assert(str);
		asserteq(str, "Hello World");
	}

	test("formats") {
		str = gil_strf("%d + %.2f = %g", 10, 20.0, 30.0);
		assert(str);
		asserteq(str, "10 + 20.00 = 30");
	}

	test("long strings") {
		const char *initial = "THIS IS A LONG STRING ";
		str = gil_strf("%s", initial);
		assert(str);
		for (size_t i = 0; i < 4; ++i) {
			char *nstr = gil_strf("%s%s", str, initial);
			assert(nstr);
			free(str);
			str = nstr;
		}

		size_t initial_len = strlen(initial);
		asserteq(strlen(str), initial_len * 5);
		for (size_t i = 0; str[i]; ++i) {
			asserteq(str[i], initial[i % initial_len]);
		}
	}
}
