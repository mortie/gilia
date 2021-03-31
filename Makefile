OUT ?= build
CFLAGS += -Iinclude/gilia -fPIC -g -O2 \
	-Wall -Wextra -Wno-unused-parameter
LDLIBS += -lreadline

OBJCOPY ?= objcopy
STRIP ?= strip

all: $(OUT)/gilia

include files.mk
include rules.mk

$(OUT)/gilia: $(call objify,$(LIB_SRCS) $(CMD_SRCS))
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)
	./add-size-value.py $@

$(OUT)/gilia.so: $(call objify,$(LIB_SRCS))
	$(CC) $(LDFLAGS) -shared -o $@ $^

include $(call depify,$(LIB_SRCS) $(CMD_SRCS))

.PHONY: strip
strip: $(OUT)/gilia
	$(STRIP) --keep-symbol=gil_binary_size $(OUT)/gilia
	./add-size-value.py $(OUT)/gilia

.PHONY: clean
clean:
	rm -rf $(OUT)
