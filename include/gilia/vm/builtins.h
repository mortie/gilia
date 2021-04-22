#include "bytecode.h"
// IWYU pragma: no_include "builtins.x.h"

struct gil_vm;

#define XFUNCTION(name, f) \
	gil_word f( \
			struct gil_vm *vm, gil_word mid, gil_word self, \
			gil_word argc, gil_word *argv);
#include "../builtins.x.h" // IWYU pragma: export
#undef XFUNCTION
