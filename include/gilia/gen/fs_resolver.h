#ifndef GIL_FS_RESOLVER_H
#define GIL_FS_RESOLVER_H

#include "gen.h"
#include "../io.h"

#define GIL_FS_RESOLVER_PATH_MAX 4096

struct gil_fs_resolver_component {
	struct gil_fs_resolver_component *parent;
	struct gil_io_file_reader reader;
	char dirpath[GIL_FS_RESOLVER_PATH_MAX];
};

struct gil_fs_resolver {
	struct gil_generator_resolver base;
	struct gil_fs_resolver_component *head;

	struct gil_fs_resolver_component end;
};

void gil_fs_resolver_init(struct gil_fs_resolver *rs, const char *path);
void gil_fs_resolver_destroy(struct gil_fs_resolver *rs);

#endif
