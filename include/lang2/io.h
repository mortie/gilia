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
	struct l2_io_reader *r;
	size_t len;
	size_t idx;
	char buf[L2_IO_BUFSIZ];
};

int l2_io_printf(struct l2_io_writer *w, const char *fmt, ...);

void l2_bufio_reader_init(struct l2_bufio_reader *b, struct l2_io_reader *r);
void l2_bufio_shift(struct l2_bufio_reader *b);
int l2_bufio_shift_peek(struct l2_bufio_reader *b, size_t count);
int l2_bufio_shift_get(struct l2_bufio_reader *b);
static inline int l2_bufio_peek(struct l2_bufio_reader *b, size_t count);
static inline int l2_bufio_get(struct l2_bufio_reader *b);

struct l2_bufio_writer {
	struct l2_io_writer *w;
	size_t idx;
	char buf[L2_IO_BUFSIZ];
};

void l2_bufio_writer_init(struct l2_bufio_writer *b, struct l2_io_writer *w);
void l2_bufio_flush(struct l2_bufio_writer *b);
static inline void l2_bufio_put(struct l2_bufio_writer *b, char ch);
static inline void l2_bufio_put_n(struct l2_bufio_writer *b, const void *ptr, size_t len);

/*
 * Useful readers and writers
 */

struct l2_io_mem_reader {
	struct l2_io_reader r;
	size_t idx;
	size_t len;
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

static inline int l2_bufio_peek(struct l2_bufio_reader *b, size_t count) {\
	size_t offset = count - 1;
	if (b->idx + offset < b->len) {
		return b->buf[b->idx + offset];
	} else {
		return l2_bufio_shift_peek(b, count);
	}
}

static inline int l2_bufio_get(struct l2_bufio_reader *b) {
	if (b->idx < b->len) {
		return b->buf[b->idx++];
	} else {
		return l2_bufio_shift_get(b);
	}
}

static inline void l2_bufio_put(struct l2_bufio_writer *b, char ch) {
	if (b->idx >= sizeof(b->buf)) {
		l2_bufio_flush(b);
	}
	b->buf[b->idx++] = ch;
}

static inline void l2_bufio_put_n(struct l2_bufio_writer *b, const void *ptr, size_t len) {
	size_t freespace = sizeof(b->buf) - b->idx;
	if (len < freespace) {
		memcpy(b->buf + b->idx, ptr, len);
		b->idx += len;
	} else {
		l2_bufio_flush(b);
		b->w->write(b->w, ptr, len);
	}
}

#endif
