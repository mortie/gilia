#include "vm/vm.h"

#include <stdlib.h>

#include "bytecode.h"

static const gil_word tombstone = ~(gil_word)0;

static struct gil_vm_namespace *set(struct gil_vm_namespace *ns, gil_word key, gil_word val);

static struct gil_vm_namespace *alloc(size_t size, gil_word mask) {
	struct gil_vm_namespace *ns = calloc(
			1, sizeof(struct gil_vm_namespace) + sizeof(gil_word) * size * 2);
	ns->size = size;
	ns->mask = mask;
	return ns;
}

static struct gil_vm_namespace *grow(struct gil_vm_namespace *ns) {
	struct gil_vm_namespace *newns = alloc(ns->size * 2, (ns->mask << 1) | ns->mask);

	for (size_t i = 0; i < ns->size; ++i) {
		gil_word key = ns->data[i];
		if (key == 0 || key == tombstone) {
			continue;
		}

		gil_word val = ns->data[ns->size + i];
		for (gil_word i = 0; ; ++i) {
			gil_word hash = (key + i) & newns->mask;
			if (newns->data[hash] == 0) {
				newns->data[hash] = key;
				newns->data[newns->size + hash] = val;
				newns->len += 1;
				break;
			}
		}
	}

	free(ns);
	return newns;
}

static void del(struct gil_vm_namespace *ns, gil_word key) {
	if (ns == NULL) {
		return;
	}

	for (gil_word i = 0; ; ++i) {
		gil_word hash = (key + i) & ns->mask;
		gil_word k = ns->data[hash];
		if (k == 0) {
			return;
		} else if (k == key) {
			ns->data[hash] = tombstone;
			return;
		}
	}
}

static struct gil_vm_namespace *set(struct gil_vm_namespace *ns, gil_word key, gil_word val) {
	if (ns == NULL) {
		ns = alloc(16, 0x0f);
	} else if (ns->len >= ns->size / 2) {
		ns = grow(ns);
	}

	for (gil_word i = 0; ; ++i) {
		gil_word hash = (key + i) & ns->mask;
		gil_word k = ns->data[hash];
		if (k == tombstone) {
			ns->data[hash] = key;
			ns->data[ns->size + hash] = val;
			break;
		} else if (k == key) {
			ns->data[ns->size + hash] = val;
			break;
		} else if (k == 0) {
			ns->len += 1;
			ns->data[hash] = key;
			ns->data[ns->size + hash] = val;
			break;
		}
	}

	return ns;
}

static gil_word get(struct gil_vm_namespace *ns, gil_word key) {
	if (ns == NULL) {
		return 0;
	}

	for (gil_word i = 0; ; ++i) {
		gil_word hash = (key + i) & ns->mask;
		gil_word k = ns->data[hash];
		if (k == 0 || k == tombstone) {
			return 0;
		} else if (k == key) {
			return ns->data[ns->size + hash];
		}
	}
}

gil_word gil_vm_namespace_get(struct gil_vm *vm, struct gil_vm_value *v, gil_word key) {
	gil_word ret = get(v->ns.ns, key);
	if (ret == 0 && v->ns.parent != 0) {
		return gil_vm_namespace_get(vm, &vm->values[v->ns.parent], key);
	}

	return ret;
}

gil_word gil_vm_namespace_get_or(
		struct gil_vm *vm, struct gil_vm_value *v, gil_word key, gil_word alt) {
	gil_word id = gil_vm_namespace_get(vm, v, key);
	if (id == 0) {
		return alt;
	} else {
		return id;
	}
}

void gil_vm_namespace_set(struct gil_vm_value *v, gil_word key, gil_word val) {
	if (val == 0) {
		del(v->ns.ns, key);
	} else {
		v->ns.ns = set(v->ns.ns, key, val);
	}
}

int gil_vm_namespace_replace(struct gil_vm *vm, struct gil_vm_value *v, gil_word key, gil_word val) {
	if (val == 0) {
		del(v->ns.ns, key);
		return 0;
	} else {
		gil_word ret = get(v->ns.ns, key);
		if (ret != 0) {
			v->ns.ns = set(v->ns.ns, key, val);
			return 0;
		}

		if (v->ns.parent == 0) {
			return -1;
		}

		return gil_vm_namespace_replace(vm, &vm->values[v->ns.parent], key, val);
	}
}
