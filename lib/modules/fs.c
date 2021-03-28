#include "modules/fs.h"

#include <stdlib.h>

#include "module.h"
#include "vm/vm.h"

struct fs_module {
	struct gil_module base;

	gil_word klol;
};

static void init(
		struct gil_module *ptr,
		gil_word (*alloc)(void *data, const char *name), void *data) {
	struct fs_module *mod = (struct fs_module *)ptr;
	mod->klol = alloc(data, "lol");
}

static gil_word create(struct gil_module *ptr, struct gil_vm *vm) {
	struct fs_module *mod = (struct fs_module *)ptr;
	gil_word id = gil_vm_alloc(vm, GIL_VAL_TYPE_NAMESPACE, 0);
	struct gil_vm_value *val = &vm->values[id];
	val->extra.ns_parent = 0;
	val->ns = NULL;

	gil_vm_namespace_set(val, mod->klol, vm->ktrue);

	return id;
}

struct gil_module *gil_mod_fs() {
	struct fs_module *mod = malloc(sizeof(*mod));
	mod->base.name = "fs";
	mod->base.init = init;
	mod->base.create = create;
	return &mod->base;
}
