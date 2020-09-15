#include "bitset.h"

#include <string.h>
#include <strings.h>
#include <limits.h>

#include <stdio.h>

#define ENTSIZ (sizeof(l2_bitset_entry) * CHAR_BIT)

static l2_bitset_entry first_unset_bit(l2_bitset_entry n) {
	return ~n & (n + 1);
}

#if defined(__GLIBC__) && ( \
		(__GLIBC__>= 2 && __GLIBC_MINOR__ >= 27) || \
		_GNU_SOURCE)
#define first_set ffsll
#else
static int first_set(l2_bitset_entry n) {
	if (n == 0) {
		return 0;
	}

	int num = 1;
	while ((n & 1) == 0) {
		n >>= 1;
		num += 1;
	}

	return num;
}
#endif

static void expand_tables(struct l2_bitset *bs) {
	while (bs->currtable >= bs->tableslen) {
		bs->tables = realloc(bs->tables, bs->tableslen * 2 * sizeof(*bs->tables));
		memset(bs->tables + bs->tableslen, 0, sizeof(*bs->tables) * bs->tableslen);
		bs->tableslen *= 2;
	}
}

void l2_bitset_init(struct l2_bitset *bs) {
	bs->tableslen = 4;
	bs->tables = calloc(bs->tableslen, sizeof(*bs->tables));
	bs->currtable = 0;
	bs->dirslen = 1;
	bs->dirs = calloc(bs->dirslen, sizeof(*bs->dirs));
	bs->currdir = 0;
}

void l2_bitset_free(struct l2_bitset *bs) {
	free(bs->tables);
	free(bs->dirs);
}

int l2_bitset_get(struct l2_bitset *bs, size_t id) {
	size_t tblidx = id / ENTSIZ;
	size_t tblbit = id % ENTSIZ;
	if (tblidx >= bs->tableslen) {
		return 0;
	}

	return !!(bs->tables[tblidx] & (l2_bitset_entry)1 << tblbit);
}

size_t l2_bitset_set_next(struct l2_bitset *bs) {
	l2_bitset_entry *table = &bs->tables[bs->currtable];
	l2_bitset_entry bit = first_unset_bit(*table);
	*table |= bit;
	size_t ret = bs->currtable * ENTSIZ + first_set(bit) - 1;

	// Still free space?
	if (*table != ~(l2_bitset_entry)0) {
		return ret;
	}

	// Ok, this entry is full then...
	l2_bitset_entry *dir = &bs->dirs[bs->currdir];
	*dir |= (l2_bitset_entry)1 << (bs->currtable % ENTSIZ);

	// Is there still space in this directory?
	if (*dir != ~(l2_bitset_entry)0) {
		bs->currtable = bs->currdir * ENTSIZ + first_set(first_unset_bit(*dir)) - 1;
		expand_tables(bs);
		return ret;
	}

	// Is there a directory with free space?
	for (size_t i = 0; i < bs->dirslen; ++i) {
		dir = &bs->dirs[i];
		if (*dir == ~(l2_bitset_entry)0) {
			continue;
		}

		bs->currdir = i;
		bs->currtable = bs->currdir * ENTSIZ + first_set(first_unset_bit(*dir)) - 1;
		expand_tables(bs);

		return ret;
	}

	// Ok, we gotta make a new dir then
	bs->currdir = bs->dirslen;
	bs->currtable = bs->currdir * ENTSIZ;
	bs->dirs = realloc(bs->dirs, bs->dirslen * 2 * sizeof(*bs->dirs));
	memset(bs->dirs + bs->dirslen, 0, sizeof(*bs->dirs) * bs->dirslen);
	bs->dirslen *= 2;
	expand_tables(bs);
	return ret;
}

void l2_bitset_unset(struct l2_bitset *bs, size_t id) {
	size_t tblidx = id / ENTSIZ;
	size_t tblbit = id % ENTSIZ;
	size_t diridx = id / (ENTSIZ * ENTSIZ);
	size_t dirbit = (id / ENTSIZ) % ENTSIZ;

	if (tblidx >= bs->tableslen) {
		return;
	}

	bs->tables[tblidx] &= ~((l2_bitset_entry)1 << tblbit);
	bs->dirs[diridx] &= ~((l2_bitset_entry)1 << dirbit);
}
