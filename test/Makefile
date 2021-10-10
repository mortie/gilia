SRCS = $(wildcard src/*.c)

WARNINGS := -Wall -Wextra -Wpedantic -Wno-unused-parameter
INCLUDES := -I../include/gilia -Isnow
DEFINES := -DSNOW_ENABLED

FLAGS := $(WARNINGS) $(INCLUDES) $(DEFINES)
CFLAGS += $(FLAGS)
LDFLAGS +=
LDLIBS += -lreadline

OUT ?= build
CC ?= cc
PKG_CONFIG ?= pkg-config

SANITIZE ?= address,undefined
ifneq ($(SANITIZE),)
LDFLAGS += -fsanitize=$(SANITIZE)
FLAGS += -fsanitize=$(SANITIZE)
endif

ifeq ($(VERBOSE),1)
define exec
	@echo '$(1):' $(2)
	@$(2)
endef
else
define exec
	@echo '$(1)' $@
	@$(2) || (echo '$(1) $@:' $(2) >&2 && false)
endef
endif

$(OUT)/test: $(patsubst %,$(OUT)/%.o,$(SRCS)) $(OUT)/libgilia.so
	@mkdir -p $(@D)
	$(call exec,LD ,$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS))

.PHONY: $(OUT)/libgilia.so
$(OUT)/libgilia.so:
	$(MAKE) -C .. SANITIZE=$(SANITIZE) OUT=$(abspath $(OUT)) $(abspath $(OUT))/libgilia.so

$(OUT)/%.c.o: %.c $(OUT)/%.c.d
	@mkdir -p $(@D)
	$(call exec,CC ,$(CC) $(CFLAGS) -MMD -o $@ -c $<)
$(OUT)/%.c.d: %.c;

ifneq ($(filter clean,$(MAKECMDGOALS)),clean)
-include $(patsubst %,$(OUT)/%.d,$(SRCS))
endif

.PHONY: check
check: $(OUT)/test
	$(OUT)/test

.PHONY: clean
clean:
	rm -rf $(OUT) coverage

.PHONY: generate-coverage
generate-coverage: check
	mkdir -p coverage
	gcov $$(find $(OUT) -type f -name '*.o')
	mv *.gcov coverage
	cp $$(find build -type f -name '*.gcda' -or -name '*.gcno') coverage
	cd coverage && lcov --capture --directory . --output-file coverage.info
	cd coverage && genhtml coverage.info --output-directory report
