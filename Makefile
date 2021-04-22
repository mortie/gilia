OUT ?= build
CFLAGS += -Iinclude/gilia -fPIC -g -O2 \
	-Wall -Wextra -Wno-unused-parameter
LDLIBS += -lreadline

OBJCOPY ?= objcopy
STRIP ?= strip
IWYU ?= include-what-you-use -Xiwyu --no_comments

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
