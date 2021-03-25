#include "strset.h"

#include <string.h>
#include <stdio.h>

// sdbm from http://www.cse.yorku.ca/~oz/hash.html;
// there are probably better algorithms out there
static size_t hash(const void *ptr) {
	const unsigned char *str = ptr;
	size_t h = 0;
	int c;

	while ((c = *(str++)) != 0) {
		h = c + (h << 6) + (h << 16) - h;
	}

	return h;
}

void grow(struct gil_strset *set) {
	struct gil_strset old = *set;
	set->len = 0;
	set->size *= 2;
	set->mask = (set->mask << 1) | set->mask;

	set->keys = calloc(set->size, sizeof(*set->keys));
	set->vals = calloc(set->size, sizeof(*set->vals));

	for (size_t i = 0; i < old.size; ++i) {
		char *oldkey = old.keys[i];
		if (oldkey == NULL) {
			continue;
		}

		size_t h = hash(old.keys[i]);
		for (size_t j = 0; ; ++j) {
			size_t index = (h + j) & set->mask;
			char *k = set->keys[index];
			if (k != NULL) {
				continue;
			}

			set->keys[index] = oldkey;
			set->vals[index] = old.vals[i];
			break;
		}
	}

	free(old.keys);
	free(old.vals);
}

void gil_strset_init(struct gil_strset *set) {
	set->next = 1;
	set->len = 0;
	set->size = 16;
	set->mask = 0x0f;
	set->keys = calloc(set->size, sizeof(*set->keys));
	set->vals = calloc(set->size, sizeof(*set->vals));
}

void gil_strset_free(struct gil_strset *set) {
	for (size_t i = 0; i < set->size; ++i) {
		free(set->keys[i]);
	}

	free(set->keys);
	free(set->vals);
}

size_t gil_strset_put(struct gil_strset *set, char **str) {
	if (set->len >= set->size / 2) {
		grow(set);
	}

	size_t h = hash(*str);
	for (size_t i = 0; ; ++i) {
		size_t index = (h + i) & set->mask;
		char *k = set->keys[index];
		if (k == NULL) {
			set->keys[index] = *str;
			set->vals[index] = set->next++;
			set->len += 1;
			*str = NULL;
			return set->vals[index];
		} else if (strcmp(*str, k) == 0) {
			free(*str);
			*str = NULL;
			return set->vals[index];
		}
	}
}

size_t gil_strset_put_copy(struct gil_strset *set, const char *str) {
	if (set->len >= set->size / 2) {
		grow(set);
	}

	size_t h = hash(str);
	for (size_t i = 0; ; ++i) {
		size_t index = (h + i) & set->mask;
		char *k = set->keys[index];
		if (k == NULL) {
			set->keys[index] = strdup(str);
			set->vals[index] = set->next++;
			set->len += 1;
			return set->vals[index];
		} else if (strcmp(str, k) == 0) {
			return set->vals[index];
		}
	}
}

size_t gil_strset_get(struct gil_strset *set, const char *str) {
	size_t h = hash(str);
	for (size_t i = 0; ; ++i) {
		size_t index = (h + i) & set->mask;
		char *k = set->keys[index];
		if (k == NULL) {
			return 0;
		} else if (strcmp(str, k) == 0) {
			return set->vals[index];
		}
	}
}
