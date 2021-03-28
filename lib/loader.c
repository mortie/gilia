#include "loader.h"

#include <stdint.h>

int gil_bc_serialize(FILE *outf, unsigned char *data, size_t len) {
	char header[4] = { 0x1b, 0x67, 0x6c, 0x63 };
	if (fwrite(header, 1, 4, outf) < 4) {
		fprintf(stderr, "Write error\n");
		return -1;
	}

	uint32_t version = gil_bytecode_version;

	unsigned char version_buf[4] = {
		(version >> 0) & 0xff,
		(version >> 8) & 0xff,
		(version >> 16) & 0xff,
		(version >> 24) & 0xff,
	};
	if (fwrite(version_buf, 1, 4, outf) < 4) {
		fprintf(stderr, "Write error\n");
		return -1;
	}

	if (fwrite(data, 1, len, outf) < len) {
		fprintf(stderr, "Write error\n");
		return -1;
	}

	return 0;
}

int gil_bc_load(FILE *inf, struct gil_io_writer *w) {
	// Header is already read by main

	unsigned char version_buf[4];
	if (fread(version_buf, 1, 4, inf) < 4) {
		fprintf(stderr, "Read error\n");
		return -1;
	}

	uint32_t version = 0 |
		((uint32_t)version_buf[0]) << 0 |
		((uint32_t)version_buf[1]) << 8 |
		((uint32_t)version_buf[2]) << 16 |
		((uint32_t)version_buf[3]) << 24;
	if (version != gil_bytecode_version) {
		fprintf(stderr,
				"Version mismatch! Bytecode file uses bytecode version %i"
				", but your build of Gilia uses bytecode version %i\n",
				version, gil_bytecode_version);
		return -1;
	}

	unsigned char buffer[4096];

	while (1) {
		size_t n = fread(buffer, 1, sizeof(buffer), inf);
		if (n == 0) {
			return 0;
		}

		w->write(w, buffer, n);
	}
}
