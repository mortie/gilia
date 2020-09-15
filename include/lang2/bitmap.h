#ifndef L2_BITMAP_H
#define L2_BITMAP_H

#include <stdlib.h>
#include <stdint.h>

typedef long long int l2_bitmap_entry;

struct l2_bitmap {
	l2_bitmap_entry *tables;
	size_t tableslen;
	size_t currtable;

	l2_bitmap_entry *dirs;
	size_t dirslen;
	size_t currdir;
};

void l2_bitmap_init(struct l2_bitmap *bm);
void l2_bitmap_free(struct l2_bitmap *bm);
int l2_bitmap_get(struct l2_bitmap *bm, size_t id);
size_t l2_bitmap_set_next(struct l2_bitmap *bm);
void l2_bitmap_unset(struct l2_bitmap *bm, size_t id);

#endif
