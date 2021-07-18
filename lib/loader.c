#include "loader.h"

#include <stdint.h>
#include "bytecode.h"

#include "io.h"

#if !defined(GIL_MAJOR) || !defined(GIL_MINOR)
#error "Need to define GIL_MAJOR and GIL_MINOR macros"
#endif

int gil_bc_serialize(FILE *outf, unsigned char *data, size_t len) {
	char header[4] = { 0x1b, 0x67, 0x6c, 0x63 };
	if (fwrite(header, 1, 4, outf) < 4) {
		fprintf(stderr, "Write error\n");
		return -1;
	}

	unsigned char version_buf[4] = {
		((GIL_MAJOR) >> 8) & 0xff,
		((GIL_MAJOR) >> 0) & 0xff,
		((GIL_MINOR) >> 8) & 0xff,
		((GIL_MINOR) >> 0) & 0xff,
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

	uint16_t major = (uint32_t)version_buf[0] << 8 | version_buf[1];
	uint16_t minor = (uint32_t)version_buf[2] << 8 | version_buf[3];

	if (major != (GIL_MAJOR) || minor > (GIL_MINOR)) {
		fprintf(stderr,
				"Version mismatch! Bytecode is from version %i.%i"
				", but your build of Gilia is version %i.%i\n",
				major, minor, (GIL_MAJOR), (GIL_MINOR));
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
