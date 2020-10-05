#include "vm/vm.h"

#include <stdio.h>

static const l2_word tombstone = ~(l2_word)0;

static struct l2_vm_namespace *set(struct l2_vm_namespace *ns, l2_word key, l2_word val);

static struct l2_vm_namespace *alloc(size_t size, l2_word mask) {
	printf("alloc, size %zu, mask allows up to [%u]\n", size, mask);
	struct l2_vm_namespace *ns = calloc(
			1, sizeof(struct l2_vm_namespace) + sizeof(l2_word) * size * 2);
	ns->size = size;
	ns->mask = mask;
	return ns;
}

static struct l2_vm_namespace *grow(struct l2_vm_namespace *ns) {
	struct l2_vm_namespace *newns = alloc(ns->size * 2, (ns->mask << 1) | ns->mask);

	for (size_t i = 0; i < ns->size; ++i) {
		l2_word key = ns->data[i];
		if (key == 0 || key == tombstone) {
			continue;
		}

		l2_word val = ns->data[ns->size + i];
		for (l2_word i = 0; ; ++i) {
			l2_word hash = (key + i) & newns->mask;
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

static void del(struct l2_vm_namespace *ns, l2_word key) {
	if (ns == NULL) {
		return;
	}

	for (l2_word i = 0; ; ++i) {
		l2_word hash = (key + i) & ns->mask;
		l2_word k = ns->data[hash];
		if (k == 0) {
			return;
		} else if (k == key) {
			ns->data[hash] = tombstone;
			return;
		}
	}
}

static struct l2_vm_namespace *set(struct l2_vm_namespace *ns, l2_word key, l2_word val) {
	if (ns == NULL) {
		ns = alloc(16, 0x0f);
		printf("alloc to %zu\n", ns->size);
	} else if (ns->len >= ns->size / 2) {
		ns = grow(ns);
		printf("grew to %zu\n", ns->size);
	}

	for (l2_word i = 0; ; ++i) {
		l2_word hash = (key + i) & ns->mask;
		l2_word k = ns->data[hash];
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

static l2_word get(struct l2_vm_namespace *ns, l2_word key) {
	if (ns == NULL) {
		return 0;
	}

	for (l2_word i = 0; ; ++i) {
		l2_word hash = (key + i) & ns->mask;
		l2_word k = ns->data[hash];
		if (k == 0) {
			return 0;
		} else if (k == key) {
			printf("found, %i collisions\n", i);
			return ns->data[ns->size + hash];
		}
	}
}

void l2_vm_namespace_set(struct l2_vm_value *ns, l2_word key, l2_word val) {
	if (val == 0) {
		del(ns->data, key);
	} else {
		ns->data = set(ns->data, key, val);
	}
}

l2_word l2_vm_namespace_get(struct l2_vm_value *ns, l2_word key) {
	return get(ns->data, key);
}
