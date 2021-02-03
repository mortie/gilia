#ifndef L2_GEN_H
#define L2_GEN_H

#include "../io.h"
#include "../strset.h"
#include "../bytecode.h"

struct l2_generator_string {
	l2_word length;
	l2_word pos;
};

struct l2_generator {
	struct l2_strset atomset;
	struct l2_strset stringset;
	struct l2_generator_string *strings;
	l2_word pos;
	struct l2_bufio_writer writer;
};

void l2_gen_init(struct l2_generator *gen, struct l2_io_writer *w);
void l2_gen_flush(struct l2_generator *gen);
void l2_gen_free(struct l2_generator *gen);

void l2_gen_halt(struct l2_generator *gen);
void l2_gen_rjmp(struct l2_generator *gen, l2_word len);
void l2_gen_pop(struct l2_generator *gen);
void l2_gen_push(struct l2_generator *gen, l2_word word);
void l2_gen_ret(struct l2_generator *gen);
void l2_gen_assignment(struct l2_generator *gen, char **ident);
void l2_gen_number(struct l2_generator *gen, double num);
void l2_gen_string(struct l2_generator *gen, char **str);
void l2_gen_atom(struct l2_generator *gen, char **ident);
void l2_gen_function(struct l2_generator *gen, l2_word pos);
void l2_gen_namespace_lookup(struct l2_generator *gen, char **ident);
void l2_gen_func_call(struct l2_generator *gen);

#endif
