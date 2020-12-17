#ifndef L2_GEN_H
#define L2_GEN_H

#include "../io.h"
#include "../strset.h"

struct l2_generator {
	struct l2_strset atoms;
	struct l2_strset strings;
	struct l2_bufio_writer writer;
};

void l2_gen_init(struct l2_generator *gen, struct l2_io_writer *w);
void l2_gen_flush(struct l2_generator *gen);
void l2_gen_free(struct l2_generator *gen);

void l2_gen_halt(struct l2_generator *gen);
void l2_gen_stack_frame(struct l2_generator *gen);
void l2_gen_assignment(struct l2_generator *gen, char **ident);
void l2_gen_number(struct l2_generator *gen, double num);
void l2_gen_namespace_lookup(struct l2_generator *gen, char **ident);

#endif
