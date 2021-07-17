#include "gen/fs_resolver.h"

#include <libgen.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <errno.h>

#include "str.h"

static void dirnamify(char *buf) {
	char *dpath = dirname(buf);
	// dirname may or may not return a pointer to somewhere in 'bf'
	// so we have to use memmove.
	memmove(buf, dpath, strlen(dpath) + 1);
}

static char *normalize(
		struct gil_generator_resolver *resolver, const char *str, char **err) {
	struct gil_fs_resolver *rs = (struct gil_fs_resolver *)resolver;
	char path[GIL_FS_RESOLVER_PATH_MAX];
	snprintf(path, sizeof(path), "%s/%s", rs->head->dirpath, str);
	return realpath(path, NULL);
}

static struct gil_io_reader *create_reader(
		struct gil_generator_resolver *resolver, const char *path, char **err) {
	struct gil_fs_resolver *rs = (struct gil_fs_resolver *)resolver;
	struct gil_fs_resolver_component *comp = malloc(sizeof(*comp));
	if (comp == NULL) {
		*err = gil_strf("Allocation failure");
		return NULL;
	}

	comp->reader.r.read = gil_io_file_read;
	comp->reader.f = fopen(path, "r");
	if (comp->reader.f == NULL) {
		*err = gil_strf("Open: %s", strerror(errno));
		free(comp);
		return NULL;
	}

	strncpy(comp->dirpath, path, sizeof(comp->dirpath));
	comp->dirpath[sizeof(comp->dirpath) - 1] = '\0';
	dirnamify(comp->dirpath);

	comp->parent = rs->head;
	rs->head = comp;
	return &comp->reader.r;
}

static void destroy_reader(struct gil_generator_resolver *resolver, struct gil_io_reader *reader) {
	struct gil_fs_resolver *rs = (struct gil_fs_resolver *)resolver;
	struct gil_fs_resolver_component *comp = rs->head;
	assert(reader == &comp->reader.r);

	fclose(comp->reader.f);
	rs->head = comp->parent;
	free(comp);
}

void gil_fs_resolver_init(struct gil_fs_resolver *rs, const char *path) {
	rs->base.normalize = normalize;
	rs->base.create_reader = create_reader;
	rs->base.destroy_reader = destroy_reader;

	rs->end.parent = NULL;
	strncpy(rs->end.dirpath, path, sizeof(rs->end.dirpath));
	rs->end.dirpath[sizeof(rs->end.dirpath) - 1] = '\0';
	dirnamify(rs->end.dirpath);
	rs->head = &rs->end;
}

void gil_fs_resolver_destroy(struct gil_fs_resolver *rs) {
	while (rs->head != &rs->end) {
		struct gil_fs_resolver_component *parent = rs->head->parent;
		free(rs->head);
		rs->head = parent;
	}
}
