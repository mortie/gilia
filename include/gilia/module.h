#pragma once

#include "bytecode.h"

struct gil_vm;

struct gil_module {
	char *name;
	void (*init)(
			struct gil_module *mod,
			gil_word (*alloc)(void *data, const char *name), void *data);
	gil_word (*create)(struct gil_module *mod, struct gil_vm *vm, gil_word mid);
};
