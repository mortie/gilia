#ifndef GIL_IO_H
#define GIL_IO_H

#include <stdio.h>
#include <string.h>

#define GIL_IO_BUFSIZ 1024

struct gil_io_reader {
	size_t (*read)(struct gil_io_reader *self, void *buf, size_t len);
};

struct gil_io_writer {
	void (*write)(struct gil_io_writer *self, const void *buf, size_t len);
};

struct gil_bufio_reader {
	struct gil_io_reader *r;
	size_t len;
	size_t idx;
	char buf[GIL_IO_BUFSIZ];
};

int gil_io_printf(struct gil_io_writer *w, const char *fmt, ...);

void gil_bufio_reader_init(struct gil_bufio_reader *b, struct gil_io_reader *r);
void gil_bufio_shift(struct gil_bufio_reader *b);
int gil_bufio_shift_peek(struct gil_bufio_reader *b, size_t count);
int gil_bufio_shift_get(struct gil_bufio_reader *b);
static inline int gil_bufio_peek(struct gil_bufio_reader *b, size_t count);
static inline int gil_bufio_get(struct gil_bufio_reader *b);

struct gil_bufio_writer {
	struct gil_io_writer *w;
	size_t idx;
	char buf[GIL_IO_BUFSIZ];
};

void gil_bufio_writer_init(struct gil_bufio_writer *b, struct gil_io_writer *w);
void gil_bufio_flush(struct gil_bufio_writer *b);
static inline void gil_bufio_put(struct gil_bufio_writer *b, char ch);
static inline void gil_bufio_put_n(struct gil_bufio_writer *b, const void *ptr, size_t len);

/*
 * Useful readers and writers
 */

struct gil_io_mem_reader {
	struct gil_io_reader r;
	size_t idx;
	size_t len;
	const void *mem;
};
size_t gil_io_mem_read(struct gil_io_reader *self, void *buf, size_t len);

struct gil_io_file_reader {
	struct gil_io_reader r;
	FILE *f;
};
size_t gil_io_file_read(struct gil_io_reader *self, void *buf, size_t len);

struct gil_io_mem_writer {
	struct gil_io_writer w;
	size_t len;
	size_t size;
	void *mem;
};
void gil_io_mem_write(struct gil_io_writer *self, const void *buf, size_t len);

struct gil_io_file_writer {
	struct gil_io_writer w;
	FILE *f;
};
void gil_io_file_write(struct gil_io_writer *self, const void *buf, size_t len);

/*
 * Defined in the header to let the compiler inline
 */

static inline int gil_bufio_peek(struct gil_bufio_reader *b, size_t count) {\
	size_t offset = count - 1;
	if (b->idx + offset < b->len) {
		return b->buf[b->idx + offset];
	} else {
		return gil_bufio_shift_peek(b, count);
	}
}

static inline int gil_bufio_get(struct gil_bufio_reader *b) {
	if (b->idx < b->len) {
		return b->buf[b->idx++];
	} else {
		return gil_bufio_shift_get(b);
	}
}

static inline void gil_bufio_put(struct gil_bufio_writer *b, char ch) {
	if (b->idx >= sizeof(b->buf)) {
		gil_bufio_flush(b);
	}
	b->buf[b->idx++] = ch;
}

static inline void gil_bufio_put_n(struct gil_bufio_writer *b, const void *ptr, size_t len) {
	size_t freespace = sizeof(b->buf) - b->idx;
	if (len < freespace) {
		memcpy(b->buf + b->idx, ptr, len);
		b->idx += len;
	} else {
		gil_bufio_flush(b);
		b->w->write(b->w, ptr, len);
	}
}

#endif
