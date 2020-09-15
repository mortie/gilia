#include "bitmap.h"

#include <string.h>
#include <strings.h>
#include <limits.h>

#include <stdio.h>

#define ENTSIZ (sizeof(l2_bitmap_entry) * CHAR_BIT)

static l2_bitmap_entry first_unset_bit(l2_bitmap_entry n) {
	return ~n & (n + 1);
}

#if defined(__GLIBC__) && ( \
		(__GLIBC__>= 2 && __GLIBC_MINOR__ >= 27) || \
		_GNU_SOURCE)
#define first_set ffsll
#else
static int first_set(l2_bitmap_entry n) {
}
#endif

static void expand_tables(struct l2_bitmap *bm) {
	while (bm->currtable >= bm->tableslen) {
		bm->tables = realloc(bm->tables, bm->tableslen * 2 * sizeof(*bm->tables));
		memset(bm->tables + bm->tableslen, 0, sizeof(*bm->tables) * bm->tableslen);
		bm->tableslen *= 2;
	}
}

void l2_bitmap_init(struct l2_bitmap *bm) {
	bm->tableslen = 4;
	bm->tables = calloc(bm->tableslen, sizeof(*bm->tables));
	bm->currtable = 0;
	bm->dirslen = 1;
	bm->dirs = calloc(bm->dirslen, sizeof(*bm->dirs));
	bm->currdir = 0;
}

void l2_bitmap_free(struct l2_bitmap *bm) {
	free(bm->tables);
	free(bm->dirs);
}

int l2_bitmap_get(struct l2_bitmap *bm, size_t id) {
	size_t tblidx = id / ENTSIZ;
	size_t tblbit = id % ENTSIZ;
	if (tblidx >= bm->tableslen) {
		return 0;
	}

	return !!(bm->tables[tblidx] & (l2_bitmap_entry)1 << tblbit);
}

size_t l2_bitmap_set_next(struct l2_bitmap *bm) {
	l2_bitmap_entry *table = &bm->tables[bm->currtable];
	l2_bitmap_entry bit = first_unset_bit(*table);
	*table |= bit;
	size_t ret = bm->currtable * ENTSIZ + first_set(bit) - 1;

	// Still free space?
	if (*table != ~(l2_bitmap_entry)0) {
		return ret;
	}

	// Ok, this entry is full then...
	l2_bitmap_entry *dir = &bm->dirs[bm->currdir];
	*dir |= (l2_bitmap_entry)1 << (bm->currtable % ENTSIZ);

	// Is there still space in this directory?
	if (*dir != ~(l2_bitmap_entry)0) {
		bm->currtable = bm->currdir * ENTSIZ + first_set(first_unset_bit(*dir)) - 1;
		expand_tables(bm);
		return ret;
	}

	// Is there a directory with free space?
	for (size_t i = 0; i < bm->dirslen; ++i) {
		dir = &bm->dirs[i];
		if (*dir == ~(l2_bitmap_entry)0) {
			continue;
		}

		bm->currdir = i;
		bm->currtable = bm->currdir * ENTSIZ + first_set(first_unset_bit(*dir)) - 1;
		expand_tables(bm);

		return ret;
	}

	// Ok, we gotta make a new dir then
	bm->currdir = bm->dirslen;
	bm->currtable = bm->currdir * ENTSIZ;
	bm->dirs = realloc(bm->dirs, bm->dirslen * 2 * sizeof(*bm->dirs));
	memset(bm->dirs + bm->dirslen, 0, sizeof(*bm->dirs) * bm->dirslen);
	bm->dirslen *= 2;
	expand_tables(bm);
	return ret;
}

void l2_bitmap_unset(struct l2_bitmap *bm, size_t id) {
	size_t tblidx = id / ENTSIZ;
	size_t tblbit = id % ENTSIZ;
	size_t diridx = id / (ENTSIZ * ENTSIZ);
	size_t dirbit = (id / ENTSIZ) % ENTSIZ;

	if (tblidx >= bm->tableslen) {
		return;
	}

	bm->tables[tblidx] &= ~((l2_bitmap_entry)1 << tblbit);
	bm->dirs[diridx] &= ~((l2_bitmap_entry)1 << dirbit);
}
