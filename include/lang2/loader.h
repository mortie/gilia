#ifndef L2_BC_LOADER
#define L2_BC_LOADER

#include "bytecode.h"
#include "io.h"

#include <stdio.h>

int l2_bc_serialize(FILE *outf, unsigned char *data, size_t len);
int l2_bc_load(FILE *inf, struct l2_io_writer *w);

#endif
