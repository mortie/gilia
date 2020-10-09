#ifndef L2_STRSET_H
#define L2_STRSET_H

#include <stdlib.h>

struct l2_strset {
	size_t next;
	size_t len;
	size_t size;
	size_t mask;
	char **keys;
	size_t *vals;
};

void l2_strset_init(struct l2_strset *set);
void l2_strset_free(struct l2_strset *set);
size_t l2_strset_put_move(struct l2_strset *set, char **str);
size_t l2_strset_put_copy(struct l2_strset *set, const char *str);
size_t l2_strset_get(struct l2_strset *set, const char *str);

#endif
