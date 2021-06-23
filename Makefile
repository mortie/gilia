OUT ?= build
CFLAGS += -Iinclude/gilia -fPIC -g -O2 \
	-Wall -Wextra -Wno-unused-parameter -Wpedantic
LDLIBS += -lreadline

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

CFLAGS += -DGIL_MAX_STACK_DEPTH=$(MAX_STACK_DEPTH)

OBJCOPY ?= objcopy
STRIP ?= strip
IWYU ?= include-what-you-use -Xiwyu --no_comments

TRACE ?= 0
IGNORE_DEPS ?= 0

all: $(OUT)/gilia

include files.mk
include rules.mk

$(OUT)/gilia: $(call objify,$(LIB_SRCS) $(CMD_SRCS))
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)
	./add-size-value.py $@

$(OUT)/gilia.so: $(call objify,$(LIB_SRCS))
	$(CC) $(LDFLAGS) -shared -o $@ $^

ifneq ($(IGNORE_DEPS),1)
include $(call depify,$(LIB_SRCS) $(CMD_SRCS))
endif

ifeq ($(TRACE),1)
CFLAGS += -DGIL_ENABLE_TRACE
endif

.PHONY: strip
strip: $(OUT)/gilia
	$(STRIP) --keep-symbol=gil_binary_size $(OUT)/gilia
	./add-size-value.py --strip --strip-cmd "$(STRIP)" $(OUT)/gilia

.PHONY: clean
clean:
	rm -rf $(OUT)

.PHONY: iwyu
iwyu:
	$(MAKE) -k -B CC="$(IWYU)" IGNORE_DEPS=1 $(call objify,$(LIB_SRCS) $(CMD_SRCS))
