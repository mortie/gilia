#ifndef L2_PARSE_H
#define L2_PARSE_H

#include "lex.h"
#include "gen/gen.h"

int l2_parse_program(struct l2_lexer *lexer, struct l2_generator *gen);

#endif
