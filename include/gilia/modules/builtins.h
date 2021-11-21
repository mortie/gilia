#pragma once

#include "../module.h"

struct gil_mod_builtins {
	struct gil_module base;

	gil_word
		kadd, ksub, kmul, kdiv, keq, kneq,
		klt, klteq, kgt, kgteq, kland, klor, kfirst,
		kprint, kwrite, klen,
		kif, kloop, kwhile, kfor, kguard, kmatch;
};

void gil_mod_builtins_init(struct gil_mod_builtins *mod);
