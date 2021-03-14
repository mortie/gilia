#include "loader.h"

int l2_bc_serialize(FILE *outf, l2_word *data, size_t len) {
	char header[4] = { 0x1b, 0x6c, 0x32, 0x63 };
	if (fwrite(header, 1, 4, outf) < 4) {
		fprintf(stderr, "Write error\n");
		return -1;
	}

	char version[4] = {
		(l2_bytecode_version & 0xff000000ul) >> 24,
		(l2_bytecode_version & 0x00ff0000ul) >> 16,
		(l2_bytecode_version & 0x0000ff00ul) >> 8,
		(l2_bytecode_version & 0x000000fful) >> 0,
	};
	if (fwrite(version, 1, 4, outf) < 4) {
		fprintf(stderr, "Write error\n");
		return -1;
	}

	for (size_t i = 0; i < len; ++i) {
		l2_word word = data[i];
		char *dest = (char *)&data[i];
		dest[0] = (word & 0xff000000ul) >> 24;
		dest[1] = (word & 0x00ff0000ul) >> 16;
		dest[2] = (word & 0x0000ff00ul) >> 8;
		dest[3] = (word & 0x000000fful) >> 0;
	}

	if (fwrite(data, 4, len, outf) < len) {
		fprintf(stderr, "Write error\n");
		return -1;
	}

	return 0;
}

int l2_bc_load(FILE *inf, struct l2_io_writer *w) {
	// Header is already read by main

	char version_buf[4];
	if (fread(version_buf, 1, 4, inf) < 4) {
		fprintf(stderr, "Read error\n");
		return -1;
	}

	int version = 0 |
		((unsigned int)version_buf[0]) << 24 |
		((unsigned int)version_buf[1]) << 16 |
		((unsigned int)version_buf[2]) << 8 |
		((unsigned int)version_buf[3]) << 0;
	if (version != l2_bytecode_version) {
		fprintf(
				stderr, "Version mismatch! Bytecode file uses bytecode version %i"
				", but your build of lang2 uses bytecode version %i\n",
				version, l2_bytecode_version);
		return -1;
	}

	struct l2_bufio_writer writer;
	l2_bufio_writer_init(&writer, w);

	char buffer[4096];

	while (1) {
		size_t n = fread(buffer, 1, sizeof(buffer), inf);
		if (n < 4) {
			l2_bufio_flush(&writer);
			return 0;
		}

		for (size_t i = 0; i < n; i += 4) {
			l2_word word = 0 |
				((unsigned int)buffer[i + 0]) << 24 |
				((unsigned int)buffer[i + 1]) << 16 |
				((unsigned int)buffer[i + 2]) << 8 |
				((unsigned int)buffer[i + 3]) << 0;
			l2_bufio_put_n(&writer, &word, 4);
		}
	}
}
