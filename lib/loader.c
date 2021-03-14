#include "loader.h"

#include <stdint.h>

int l2_bc_serialize(FILE *outf, l2_word *data, size_t len) {
	char header[4] = { 0x1b, 0x6c, 0x32, 0x63 };
	if (fwrite(header, 1, 4, outf) < 4) {
		fprintf(stderr, "Write error\n");
		return -1;
	}

	uint32_t version = l2_bytecode_version;

	unsigned char version_buf[4] = {
		(version & 0xff000000ull) >> 24,
		(version & 0x00ff0000ull) >> 16,
		(version & 0x0000ff00ull) >> 8,
		(version & 0x000000ffull) >> 0,
	};
	if (fwrite(version_buf, 1, 4, outf) < 4) {
		fprintf(stderr, "Write error\n");
		return -1;
	}

	struct l2_io_file_writer w = {
		.w.write = l2_io_file_write,
		.f = outf,
	};

	struct l2_bufio_writer writer;
	l2_bufio_writer_init(&writer, &w.w);

	unsigned char word_buf[4];
	for (size_t i = 0; i < len; ++i) {
		uint32_t word = data[i];
		word_buf[0] = (word & 0xff000000ull) >> 24;
		word_buf[1] = (word & 0x00ff0000ull) >> 16;
		word_buf[2] = (word & 0x0000ff00ull) >> 8;
		word_buf[3] = (word & 0x000000ffull) >> 0;
		l2_bufio_put_n(&writer, word_buf, 4);
	}

	l2_bufio_flush(&writer);
	return 0;
}

int l2_bc_load(FILE *inf, struct l2_io_writer *w) {
	// Header is already read by main

	unsigned char version_buf[4];
	if (fread(version_buf, 1, 4, inf) < 4) {
		fprintf(stderr, "Read error\n");
		return -1;
	}

	uint32_t version = 0 |
		((uint32_t)version_buf[0]) << 24 |
		((uint32_t)version_buf[1]) << 16 |
		((uint32_t)version_buf[2]) << 8 |
		((uint32_t)version_buf[3]) << 0;
	if (version != l2_bytecode_version) {
		fprintf(
				stderr, "Version mismatch! Bytecode file uses bytecode version %i"
				", but your build of lang2 uses bytecode version %i\n",
				version, l2_bytecode_version);
		return -1;
	}

	struct l2_bufio_writer writer;
	l2_bufio_writer_init(&writer, w);

	// Must be divisible by 4
	unsigned char buffer[4096];

	while (1) {
		size_t n = fread(buffer, 1, sizeof(buffer), inf);
		if (n < 4) {
			l2_bufio_flush(&writer);
			return 0;
		}

		for (size_t i = 0; i < n; i += 4) {
			l2_word word = 0 |
				((uint32_t)buffer[i + 0]) << 24 |
				((uint32_t)buffer[i + 1]) << 16 |
				((uint32_t)buffer[i + 2]) << 8 |
				((uint32_t)buffer[i + 3]) << 0;
			l2_bufio_put_n(&writer, &word, 4);
		}
	}
}
