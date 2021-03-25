#ifndef GIL_GEN_H
#define GIL_GEN_H

#include "../io.h"
#include "../strset.h"
#include "../bytecode.h"

struct gil_generator_string {
	gil_word length;
	gil_word pos;
};

struct gil_generator {
	struct gil_strset atomset;
	struct gil_strset stringset;
	struct gil_generator_string *strings;
	gil_word pos;
	struct gil_bufio_writer writer;
};

void gil_gen_init(struct gil_generator *gen, struct gil_io_writer *w);
void gil_gen_flush(struct gil_generator *gen);
void gil_gen_free(struct gil_generator *gen);

void gil_gen_halt(struct gil_generator *gen);
void gil_gen_rjmp(struct gil_generator *gen, gil_word len);
void gil_gen_rjmp_placeholder(struct gil_generator *gen);
void gil_gen_discard(struct gil_generator *gen);
void gil_gen_swap_discard(struct gil_generator *gen);
void gil_gen_ret(struct gil_generator *gen);
void gil_gen_none(struct gil_generator *gen);
void gil_gen_number(struct gil_generator *gen, double num);
void gil_gen_string(struct gil_generator *gen, char **str);
void gil_gen_string_copy(struct gil_generator *gen, char *str);
void gil_gen_atom(struct gil_generator *gen, char **ident);
void gil_gen_atom_copy(struct gil_generator *gen, char *ident);
void gil_gen_function(struct gil_generator *gen, gil_word pos);
void gil_gen_array(struct gil_generator *gen, gil_word count);
void gil_gen_namespace(struct gil_generator *gen);
void gil_gen_namespace_set(struct gil_generator *gen, char **ident);
void gil_gen_namespace_set_copy(struct gil_generator *gen, char *ident);
void gil_gen_namespace_lookup(struct gil_generator *gen, char **ident);
void gil_gen_namespace_lookup_copy(struct gil_generator *gen, char *ident);
void gil_gen_array_lookup(struct gil_generator *gen, int number);
void gil_gen_array_set(struct gil_generator *gen, int number);
void gil_gen_dynamic_lookup(struct gil_generator *gen);
void gil_gen_dynamic_set(struct gil_generator *gen);
void gil_gen_stack_frame_get_args(struct gil_generator *gen);
void gil_gen_stack_frame_lookup(struct gil_generator *gen, char **ident);
void gil_gen_stack_frame_lookup_copy(struct gil_generator *gen, char *ident);
void gil_gen_stack_frame_set(struct gil_generator *gen, char **ident);
void gil_gen_stack_frame_set_copy(struct gil_generator *gen, char *ident);
void gil_gen_stack_frame_replace(struct gil_generator *gen, char **ident);
void gil_gen_stack_frame_replace_copy(struct gil_generator *gen, char *ident);
void gil_gen_func_call(struct gil_generator *gen, gil_word argc);
void gil_gen_func_call_infix(struct gil_generator *gen);

#endif
