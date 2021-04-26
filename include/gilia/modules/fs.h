#pragma once

#include "../module.h"

struct gil_mod_fs {
	struct gil_module base;

	gil_word kopen, kclose, kread;
	gil_word nsfile;
	gil_word tfile;
};

void gil_mod_fs_init(struct gil_mod_fs *mod);
