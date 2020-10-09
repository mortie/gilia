#ifndef L2_BITSET_H
#define L2_BITSET_H

#include <stdlib.h>

typedef long long int l2_bitset_entry;

struct l2_bitset {
	l2_bitset_entry *tables;
	size_t tableslen;
	size_t currtable;

	l2_bitset_entry *dirs;
	size_t dirslen;
	size_t currdir;
};

void l2_bitset_init(struct l2_bitset *bs);
void l2_bitset_free(struct l2_bitset *bs);
int l2_bitset_get(struct l2_bitset *bs, size_t id);
size_t l2_bitset_set_next(struct l2_bitset *bs);
void l2_bitset_unset(struct l2_bitset *bs, size_t id);

#endif
