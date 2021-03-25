#ifndef GIL_BC_LOADER
#define GIL_BC_LOADER

#include "bytecode.h"
#include "io.h"

#include <stdio.h>

int gil_bc_serialize(FILE *outf, unsigned char *data, size_t len);
int gil_bc_load(FILE *inf, struct gil_io_writer *w);

#endif
