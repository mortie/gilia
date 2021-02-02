#include "io.h"

#include <stdlib.h>

void l2_bufio_reader_init(struct l2_bufio_reader *b, struct l2_io_reader *r) {
	b->r = r;
	b->len = 0;
	b->idx = 0;
}

void l2_bufio_shift(struct l2_bufio_reader *b) {
	if (b->idx > 0) {
		b->len -= b->idx;
		memmove(b->buf, b->buf + b->idx, b->len);
	}

	b->len += b->r->read(b->r, b->buf + b->len, sizeof(b->buf) - b->len);
	b->idx = 0;
}

int l2_bufio_shift_peek(struct l2_bufio_reader *b, size_t count) {
	size_t offset = count - 1;
	l2_bufio_shift(b);
	if (b->len <= offset) {
		return EOF;
	}

	return b->buf[offset];
}

int l2_bufio_shift_get(struct l2_bufio_reader *b) {
	l2_bufio_shift(b);
	if (b->len == 0) {
		return EOF;
	}

	return b->buf[b->idx++];
}

void l2_bufio_writer_init(struct l2_bufio_writer *b, struct l2_io_writer *w) {
	b->w = w;
	b->idx = 0;
}

void l2_bufio_flush(struct l2_bufio_writer *b) {
	b->w->write(b->w, b->buf, b->idx);
	b->idx = 0;
}

size_t l2_io_mem_read(struct l2_io_reader *self, char *buf, size_t len) {
	struct l2_io_mem_reader *r = (struct l2_io_mem_reader *)self;
	if (len >= r->len - r->idx) {
		len = r->len - r->idx;
	}

	memcpy(buf, r->mem + r->idx, len);
	r->idx += len;
	return len;
}

size_t l2_io_file_read(struct l2_io_reader *self, char *buf, size_t len) {
	struct l2_io_file_reader *r = (struct l2_io_file_reader *)self;
	return fread(buf, 1, len, r->f);
}

void l2_io_mem_write(struct l2_io_writer *self, const char *buf, size_t len) {
	struct l2_io_mem_writer *w = (struct l2_io_mem_writer *)self;
	size_t idx = w->len;
	w->len += len;
	w->mem = realloc(w->mem, w->len);
	memcpy(w->mem + idx, buf, len);
}

void l2_io_file_write(struct l2_io_writer *self, const char *buf, size_t len) {
	struct l2_io_file_writer *w = (struct l2_io_file_writer *)self;
	fwrite(buf, 1, len, w->f);
}