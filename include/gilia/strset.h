#ifndef GIL_STRSET_H
#define GIL_STRSET_H

#include <stdlib.h>

struct gil_strset {
	size_t next;
	size_t len;
	size_t size;
	size_t mask;
	char **keys;
	size_t *vals;
};

void gil_strset_init(struct gil_strset *set);
void gil_strset_free(struct gil_strset *set);
size_t gil_strset_put(struct gil_strset *set, char **str);
size_t gil_strset_put_copy(struct gil_strset *set, const char *str);
size_t gil_strset_get(struct gil_strset *set, const char *str);

#endif
