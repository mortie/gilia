#include "bitset.h"

#ifdef __GLIBC__
#include <features.h>
#endif 
#include <limits.h>
#include <string.h>
#include <strings.h>

#define ENTSIZ (sizeof(gil_bitset_entry) * CHAR_BIT)

static gil_bitset_entry first_unset_bit(gil_bitset_entry n) {
	return ~n & (n + 1);
}

#if defined(__GLIBC__) && ( \
		(__GLIBC__>= 2 && __GLIBC_MINOR__ >= 27) || \
		_GNU_SOURCE)
#define first_set ffsll
#else
static int first_set(gil_bitset_entry n) {
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

static void expand_tables(struct gil_bitset *bs) {
	while (bs->currtable >= bs->tableslen) {
		bs->tables = realloc(bs->tables, bs->tableslen * 2 * sizeof(*bs->tables));
		memset(bs->tables + bs->tableslen, 0, sizeof(*bs->tables) * bs->tableslen);
		bs->tableslen *= 2;
	}
}

void gil_bitset_init(struct gil_bitset *bs) {
	bs->tableslen = 4;
	bs->tables = calloc(bs->tableslen, sizeof(*bs->tables));
	bs->currtable = 0;
	bs->dirslen = 1;
	bs->dirs = calloc(bs->dirslen, sizeof(*bs->dirs));
	bs->currdir = 0;
}

void gil_bitset_free(struct gil_bitset *bs) {
	free(bs->tables);
	free(bs->dirs);
}

int gil_bitset_get(struct gil_bitset *bs, size_t id) {
	size_t tblidx = id / ENTSIZ;
	size_t tblbit = id % ENTSIZ;
	if (tblidx >= bs->tableslen) {
		return 0;
	}

	return !!(bs->tables[tblidx] & (gil_bitset_entry)1 << tblbit);
}

size_t gil_bitset_set_next(struct gil_bitset *bs) {
	gil_bitset_entry *table = &bs->tables[bs->currtable];
	gil_bitset_entry bit = first_unset_bit(*table);
	*table |= bit;
	size_t ret = bs->currtable * ENTSIZ + first_set(bit) - 1;

	// Still free space?
	if (*table != ~(gil_bitset_entry)0) {
		return ret;
	}

	// Ok, this table is full then...
	gil_bitset_entry *dir = &bs->dirs[bs->currdir];
	*dir |= (gil_bitset_entry)1 << (bs->currtable % ENTSIZ);

	// Is there still space in this directory?
	if (*dir != ~(gil_bitset_entry)0) {
		bs->currtable = bs->currdir * ENTSIZ + first_set(first_unset_bit(*dir)) - 1;
		expand_tables(bs);
		return ret;
	}

	// Is there a directory with free space?
	for (size_t i = 0; i < bs->dirslen; ++i) {
		dir = &bs->dirs[i];
		if (*dir == ~(gil_bitset_entry)0) {
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

void gil_bitset_unset(struct gil_bitset *bs, size_t id) {
	size_t tblidx = id / ENTSIZ;
	size_t tblbit = id % ENTSIZ;
	size_t diridx = id / (ENTSIZ * ENTSIZ);
	size_t dirbit = (id / ENTSIZ) % ENTSIZ;

	if (tblidx >= bs->tableslen) {
		return;
	}

	bs->tables[tblidx] &= ~((gil_bitset_entry)1 << tblbit);
	bs->dirs[diridx] &= ~((gil_bitset_entry)1 << dirbit);
}

void gil_bitset_iterator_init(
		struct gil_bitset_iterator *it, struct gil_bitset *bs) {
	it->tableidx = 0;
	if (bs->tableslen > 0) {
		it->table = bs->tables[0];
	} else {
		it->table = 0;
	}
}

void gil_bitset_iterator_init_from(
		struct gil_bitset_iterator *it, struct gil_bitset *bs, size_t start) {
	it->tableidx = start / ENTSIZ;
	if (bs->tableslen > it->tableidx) {
		it->table = bs->tables[it->tableidx];
		for (size_t i = 0; i < start - (it->tableidx * ENTSIZ); ++i) {
			it->table &= ~((gil_bitset_entry)1 << i);
		}
	} else {
		it->table = 0;
	}
}

int gil_bitset_iterator_next(
		struct gil_bitset_iterator *it, struct gil_bitset *bs, size_t *val) {
start:
	if (it->table != 0) {
		int fs = first_set(it->table) - 1;
		*val = it->tableidx * ENTSIZ + fs;
		it->table &= ~((gil_bitset_entry)1 << fs);
		return 1;
	}

	while (1) {
		it->tableidx += 1;
		if (it->tableidx >= bs->tableslen) {
			return 0;
		}

		it->table = bs->tables[it->tableidx];
		if (it->table != 0) {
			goto start; // tail recurse
		}
	}
}
