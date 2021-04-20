#include "modules/fs.h"

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#include "module.h"
#include "vm/vm.h"

struct fs_module {
	struct gil_module base;

	gil_word kopen, kclose, kreadall;

	gil_word nsfile;

	gil_word tfile;
};

static gil_word fs_open(
		struct gil_vm *vm, gil_word mid, gil_word self,
		gil_word argc, gil_word *argv) {
	struct fs_module *mod = (struct fs_module *)vm->modules[mid].mod;

	if (argc != 1 && argc != 2) {
		return gil_vm_error(vm, "Expected 1 or 2 arguments");
	}

	struct gil_vm_value *path = &vm->values[argv[0]];
	if (gil_value_get_type(path) != GIL_VAL_TYPE_BUFFER) {
		return gil_vm_type_error(vm, path);
	}

	const char *modestr = "r";
	if (argc == 2) {
		struct gil_vm_value *mode = &vm->values[argv[1]];
		if (gil_value_get_type(path) != GIL_VAL_TYPE_BUFFER) {
			return gil_vm_type_error(vm, mode);
		}

		modestr = mode->buffer.buffer;
	}

	FILE *f = fopen(path->buffer.buffer, modestr);
	if (f == NULL) {
		return gil_vm_error(vm, "%s: %s", path->buffer, strerror(errno));
	}

	return gil_vm_make_cval(vm, mod->tfile, mod->nsfile, f);
}

static gil_word fs_file_close(
		struct gil_vm *vm, gil_word mid, gil_word self,
		gil_word argc, gil_word *argv) {
	struct fs_module *mod = (struct fs_module *)vm->modules[mid].mod;

	if (argc != 0) {
		return gil_vm_error(vm, "Expected 0 arguments");
	}

	struct gil_vm_value *val = &vm->values[self];
	if (gil_value_get_type(val) != GIL_VAL_TYPE_CVAL) {
		return gil_vm_type_error(vm, val);
	}

	if (val->cval.ctype != mod->tfile) {
		return gil_vm_error(vm, "Expected file argument");
	}

	if (val->cval.cval == NULL) {
		return gil_vm_error(vm, "File already closed");
	}

	fclose(val->cval.cval);
	val->cval.cval = NULL;
	return vm->knone;
}

static gil_word fs_file_read_all(
		struct gil_vm *vm, gil_word mid, gil_word self,
		gil_word argc, gil_word *argv) {
	struct fs_module *mod = (struct fs_module *)vm->modules[mid].mod;

	if (argc != 0) {
		return gil_vm_error(vm, "Expected 0 arguments");
	}

	struct gil_vm_value *val = &vm->values[self];
	if (gil_value_get_type(val) != GIL_VAL_TYPE_CVAL) {
		return gil_vm_type_error(vm, val);
	}

	if (val->cval.ctype != mod->tfile) {
		return gil_vm_error(vm, "Expected file argument");
	}

	if (val->cval.cval == NULL) {
		return gil_vm_error(vm, "File already closed");
	}

	size_t size = 32;
	size_t len = 0;
	char *data = malloc(size + 1);
	while (1) {
		size_t n = fread(data + len, 1, size - len, (FILE *)val->cval.cval);
		if (n == 0) {
			data[len] = '\0';
			break;
		}

		len += n;
		if (len == size) {
			size *= 2;
			data = realloc(data, size + 1);
		}
	}

	return gil_vm_make_buffer(vm, data, len);
}

static void init(
		struct gil_module *ptr,
		gil_word (*alloc)(void *data, const char *name), void *data) {
	struct fs_module *mod = (struct fs_module *)ptr;
	mod->kopen = alloc(data, "open");
	mod->kclose = alloc(data, "close");
	mod->kreadall = alloc(data, "read-all");
}

static gil_word create(struct gil_module *ptr, struct gil_vm *vm, gil_word mid) {
	struct fs_module *mod = (struct fs_module *)ptr;
	mod->tfile = gil_vm_alloc_ctype(vm);

	gil_word id = gil_vm_alloc(vm, GIL_VAL_TYPE_NAMESPACE, 0);
	struct gil_vm_value *ns = &vm->values[id];
	ns->ns.parent = 0;
	ns->ns.ns = NULL;

	mod->nsfile = gil_vm_alloc(vm, GIL_VAL_TYPE_NAMESPACE, 0);
	struct gil_vm_value *nsfile = &vm->values[mod->nsfile];
	nsfile->ns.parent = 0;
	nsfile->ns.ns = NULL;
	gil_vm_namespace_set(nsfile, mod->kclose, gil_vm_make_cfunction(vm, fs_file_close, mid));
	gil_vm_namespace_set(nsfile, mod->kreadall, gil_vm_make_cfunction(vm, fs_file_read_all, mid));

	gil_vm_namespace_set(ns, mod->kopen, gil_vm_make_cfunction(vm, fs_open, mid));

	return id;
}

static void marker(
		struct gil_module *ptr, struct gil_vm *vm,
		void (*mark)(struct gil_vm *vm, gil_word id)) {
	struct fs_module *mod = (struct fs_module *)ptr;
	mark(vm, mod->nsfile);
}

struct gil_module *gil_mod_fs() {
	struct fs_module *mod = malloc(sizeof(*mod));
	mod->base.name = "fs";
	mod->base.init = init;
	mod->base.create = create;
	mod->base.marker = marker;
	return &mod->base;
}
