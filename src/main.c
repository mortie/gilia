#include "io.h"

int main() {
	struct l2_io_mem_reader r = { l2_io_mem_read };
	r.mem = "Hello World";
	r.len = strlen(r.mem);

	struct l2_bufio_reader rb;
	l2_bufio_reader_init(&rb, &r.r);

	while (1) {
		int ch = l2_bufio_get(&rb);
		if (ch == EOF) break;
		printf("%c", (char)ch);
	}
	printf("\n");
}
