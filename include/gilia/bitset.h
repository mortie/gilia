#ifndef GIL_BITSET_H
#define GIL_BITSET_H

#include <stdlib.h>

typedef unsigned long long int gil_bitset_entry;

struct gil_bitset {
	gil_bitset_entry *tables;
	size_t tableslen;
	size_t currtable;

	gil_bitset_entry *dirs;
	size_t dirslen;
	size_t currdir;
};

void gil_bitset_init(struct gil_bitset *bs);
void gil_bitset_free(struct gil_bitset *bs);
int gil_bitset_get(struct gil_bitset *bs, size_t id);
size_t gil_bitset_set_next(struct gil_bitset *bs);
void gil_bitset_unset(struct gil_bitset *bs, size_t id);

struct gil_bitset_iterator {
	size_t tableidx;
	gil_bitset_entry table;
};

void gil_bitset_iterator_init(
		struct gil_bitset_iterator *it, struct gil_bitset *bs);
void gil_bitset_iterator_init_from(
		struct gil_bitset_iterator *it, struct gil_bitset *bs, size_t start);
int gil_bitset_iterator_next(
		struct gil_bitset_iterator *it, struct gil_bitset *bs, size_t *val);

#endif
