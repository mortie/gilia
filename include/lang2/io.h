#ifndef L2_IO_H
#define L2_IO_H

#include <stdio.h>
#include <string.h>

#define L2_IO_BUFSIZ 1024

struct l2_io_reader {
	size_t (*read)(struct l2_io_reader *self, char *buf, size_t len);
};

struct l2_io_writer {
	void (*write)(struct l2_io_writer *self, const char *buf, size_t len);
};

struct l2_bufio_reader {
	size_t bufsiz;
	size_t len;
	size_t idx;
	char buf[];
};

void l2_bufio_shift(struct l2_bufio_reader *b, struct l2_io_reader *r);
int l2_bufio_shift_peek(struct l2_bufio_reader *b, struct l2_io_reader *r, size_t count);
int l2_bufio_shift_get(struct l2_bufio_reader *b, struct l2_io_reader *r);
int l2_bufio_peek(struct l2_bufio_reader *b, struct l2_io_reader *r, size_t count);
int l2_bufio_get(struct l2_bufio_reader *b, struct l2_io_reader *r);

struct l2_bufio_writer {
	size_t bufsiz;
	size_t idx;
	char buf[];
};

void l2_bufio_flush(struct l2_bufio_writer *b, struct l2_io_writer *w);
void l2_bufio_put(struct l2_bufio_writer *b, struct l2_io_writer *w, char ch);
void l2_bufio_put_n(struct l2_bufio_writer *b, struct l2_io_writer *w, const char *ptr, size_t len);

/*
 * Useful readers and writers
 */

struct l2_io_mem_reader {
	struct l2_io_reader r;
	size_t len;
	size_t idx;
	const char *mem;
};
size_t l2_io_mem_read(struct l2_io_reader *self, char *buf, size_t len);

struct l2_io_file_reader {
	struct l2_io_reader r;
	FILE *f;
};
size_t l2_io_file_read(struct l2_io_reader *self, char *buf, size_t len);

struct l2_io_mem_writer {
	struct l2_io_writer w;
	size_t len;
	char *mem;
};
void l2_io_mem_write(struct l2_io_writer *self, const char *buf, size_t len);

struct l2_io_file_writer {
	struct l2_io_writer w;
	FILE *f;
};
void l2_io_file_write(struct l2_io_writer *self, const char *buf, size_t len);

/*
 * Defined in the header to let the compiler inline
 */

inline int l2_bufio_peek(struct l2_bufio_reader *b, struct l2_io_reader *r, size_t count) {\
	size_t offset = count - 1;
	if (b->idx + offset < b->len) {
		return b->buf[b->idx + offset];
	} else {
		return l2_bufio_shift_peek(b, r, count);
	}
}

inline int l2_bufio_get(struct l2_bufio_reader *b, struct l2_io_reader *r) {
	if (b->idx < b->bufsiz) {
		return b->buf[b->idx++];
	} else {
		return l2_bufio_shift_get(b, r);
	}
}

inline void l2_bufio_put(struct l2_bufio_writer *b, struct l2_io_writer *w, char ch) {
	if (b->idx >= b->bufsiz) {
		l2_bufio_flush(b, w);
	}
	b->buf[b->idx++] = ch;
}

inline void l2_bufio_put_n(struct l2_bufio_writer *b, struct l2_io_writer *w, const char *ptr, size_t len) {
	size_t freespace = b->bufsiz - b->idx;
	if (len < freespace) {
		memcpy(b->buf + b->idx, ptr, len);
		b->idx += len;
	} else {
		l2_bufio_flush(b, w);
		w->write(w, ptr, len);
	}
}

#endif
