MAJOR = 0
MINOR = 1

SRCS = \
	lib/gen/fs_resolver.c \
	lib/gen/gen.c \
	lib/modules/builtins.c \
	lib/modules/fs.c \
	lib/parse/error.c \
	lib/parse/lex.c \
	lib/parse/parse.c \
	lib/vm/namespace.c \
	lib/vm/print.c \
	lib/vm/vm.c \
	lib/bitset.c \
	lib/io.c \
	lib/loader.c \
	lib/str.c \
	lib/strset.c \
	lib/trace.c \
	cmd/main.c \
#

# This is the max depth for all recursive algorithms in Gilia.
# The assumptions which led to the number are as follows:
# * There is at least 1MiB of stack memory available. (This is the stack size on Windows.)
# * Gilia should use at most 1/4 of the total stack available in recursive 
#   algorithms.
# * Each stack frame will definitely use <=512 bytes of stack memory.
# This gives (1MiB / 4) / 512b = 512.
# If you compile Gilia for a system with a small amount of stack, you may want
# to do a more careful analysis to ensure you're within bounds.
MAX_STACK_DEPTH ?= 512

WARNINGS := -Wall -Wextra -Wpedantic -Wno-unused-parameter
INCLUDES := -Iinclude/gilia
DEFINES := -DGIL_MAX_STACK_DEPTH=$(MAX_STACK_DEPTH) -DGIL_MAJOR=$(MAJOR) -DGIL_MINOR=$(MINOR)

FLAGS := $(WARNINGS) $(INCLUDES) $(DEFINES) -g
CFLAGS += $(FLAGS)
LDFLAGS +=
LDLIBS += -lreadline

HASH :=
BUILDDIR ?= build
OUT ?= $(BUILDDIR)/$(HASH)
CC ?= cc
PKG_CONFIG ?= pkg-config

ifeq ($(RELEASE),1)
HASH := release
FLAGS += -O2 -DNDEBUG
else
HASH := debug
endif

ifneq ($(SANITIZE),)
HASH := $(HASH)-sanitize-$(SANITIZE)
LDFLAGS += -fsanitize=$(SANITIZE)
FLAGS += -fsanitize=$(SANITIZE)
endif

ifeq ($(TRACE),1)
	HASH := $(HASH)-trace
	FLAGS += -DGIL_ENABLE_TRACE
endif

ifeq ($(VERBOSE),1)
define exec
	@echo '$(1): $(2)'
	@$(2)
endef
else
define exec
	@echo '$(1)' $@
	@$(2) || (echo '$(1) $@: $(2)' >&2 && false)
endef
endif

$(BUILDDIR)/gilia: $(OUT)/gilia
	ln -sf $(HASH)/gilia $@

$(OUT)/gilia: $(patsubst %,$(OUT)/%.o,$(SRCS))
	@mkdir -p $(@D)
	$(call exec,LD ,$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS))

$(OUT)/libgilia.so: $(patsubst %,$(OUT)/%.o,$(SRCS))
	$(call exec,LD ,$(CC) $(LDFLAGS) -shared -o $@ $^ $(LDLIBS))

$(OUT)/%.c.o: %.c $(OUT)/%.c.d
	@mkdir -p $(@D)
	$(call exec,CC ,$(CC) $(CFLAGS) -MMD -o $@ -c $<)
$(OUT)/%.c.d: %.c;

ifneq ($(filter clean,$(MAKECMDGOALS)),clean)
-include $(patsubst %,$(OUT)/%.d,$(SRCS))
endif

.PHONY: clean
clean:
	rm -rf $(OUT)

.PHONY: cleanall
cleanall:
	rm -rf $(BUILDDIR)
