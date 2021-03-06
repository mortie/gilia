#include "io.h"

#include <stdlib.h>
#include <stdarg.h>

int gil_io_printf(struct gil_io_writer *w, const char *fmt, ...) {
	char buf[256];

	va_list va;
	va_start(va, fmt);
	int n = vsnprintf(buf, sizeof(buf), fmt, va);
	if (n < 0) {
		va_end(va);
		return n;
	} else if ((size_t)n + 1 < sizeof(buf)) {
		w->write(w, buf, n);
		va_end(va);
		return n;
	}

	// Yeah, this is slower, but it's just a fallback for when
	// the output of printf is stupidly long. That shouldn't happen much.
	char *buf2 = malloc(n + 1);
	if (buf2 == NULL) {
		va_end(va);
		return -1;
	}

	n = vsnprintf(buf2, n + 1, fmt, va);
	if (n < 0) {
		va_end(va);
		free(buf2);
		return -1;
	}

	w->write(w, buf2, n);
	va_end(va);
	free(buf2);
	return n;
}

void gil_bufio_reader_init(struct gil_bufio_reader *b, struct gil_io_reader *r) {
	b->r = r;
	b->len = 0;
	b->idx = 0;
}

void gil_bufio_shift(struct gil_bufio_reader *b) {
	if (b->idx > 0) {
		b->len -= b->idx;
		memmove(b->buf, b->buf + b->idx, b->len);
	}

	b->len += b->r->read(b->r, b->buf + b->len, sizeof(b->buf) - b->len);
	b->idx = 0;
}

int gil_bufio_shift_peek(struct gil_bufio_reader *b, size_t count) {
	size_t offset = count - 1;
	gil_bufio_shift(b);
	if (b->len <= offset) {
		return EOF;
	}

	return b->buf[offset];
}

int gil_bufio_shift_get(struct gil_bufio_reader *b) {
	gil_bufio_shift(b);
	if (b->len == 0) {
		return EOF;
	}

	return b->buf[b->idx++];
}

void gil_bufio_writer_init(struct gil_bufio_writer *b, struct gil_io_writer *w) {
	b->w = w;
	b->idx = 0;
}

void gil_bufio_flush(struct gil_bufio_writer *b) {
	if (b->idx == 0) return;
	b->w->write(b->w, b->buf, b->idx);
	b->idx = 0;
}

size_t gil_io_mem_read(struct gil_io_reader *self, void *buf, size_t len) {
	struct gil_io_mem_reader *r = (struct gil_io_mem_reader *)self;
	if (len >= r->len - r->idx) {
		len = r->len - r->idx;
	}

	memcpy(buf, (char *)r->mem + r->idx, len);
	r->idx += len;
	return len;
}

size_t gil_io_file_read(struct gil_io_reader *self, void *buf, size_t len) {
	struct gil_io_file_reader *r = (struct gil_io_file_reader *)self;
	return fread(buf, 1, len, r->f);
}

void gil_io_mem_write(struct gil_io_writer *self, const void *buf, size_t len) {
	struct gil_io_mem_writer *w = (struct gil_io_mem_writer *)self;
	size_t idx = w->len;

	if (w->len + len > w->size) {
		if (w->size == 0) w->size = 64;
		while (w->len + len > w->size) w->size *= 2;
		w->mem = realloc(w->mem, w->size);
	}

	w->len += len;
	memcpy((char *)w->mem + idx, buf, len);
}

void gil_io_file_write(struct gil_io_writer *self, const void *buf, size_t len) {
	struct gil_io_file_writer *w = (struct gil_io_file_writer *)self;
	fwrite(buf, 1, len, w->f);
}
