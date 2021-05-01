OUT ?= build

CFLAGS += -g -I../include/gilia -Isnow -DSNOW_ENABLED \
	-Wall -Wextra -Wno-unsed-parameter
LDFLAGS +=
LDLIBS +=

GILCFLAGS +=
GILLDFLAGS +=

SANITIZE ?= address,undefined
ifneq ($(SANITIZE),)
	CFLAGS += -fsanitize=$(SANITIZE)
	LDFLAGS += -fsanitize=$(SANITIZE)
	GILCFLAGS += -fsanitize=$(SANITIZE)
endif

COVERAGE ?= 1
ifeq ($(COVERAGE),1)
	CFLAGS += -fprofile-arcs -ftest-coverage
	LDFLAGS += -fprofile-arcs -ftest-coverage --coverage
	GILCFLAGS += -fprofile-arcs -ftest-coverage
	GILLDFLAGS += -fprofile-arcs -ftest-coverage --coverage
	LDLIBS += -lgcov
endif

all: $(OUT)/test

include files.mk
include ../files.mk
include ../rules.mk

$(OUT)/test: $(call objify,$(TEST_SRCS)) $(OUT)/gilia.so
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

.PHONY: $(OUT)/gilia.so
$(OUT)/gilia.so:
	CFLAGS="$(GILCFLAGS)" LDFLAGS="$(GILLDFLAGS)" OUT="test/$(OUT)" \
		$(MAKE) -C .. test/$(OUT)/gilia.so

.PHONY: check
check: $(OUT)/test
	./$(OUT)/test $(ARGS)

.PHONY: generate-coverage
generate-coverage: check
	mkdir -p coverage
	gcov $$(find $(OUT) -type f -name '*.o')
	mv *.gcov coverage
	cp $$(find build -type f -name '*.gcda' -or -name '*.gcno') coverage
	cd coverage && lcov --capture --directory . --output-file coverage.info
	cd coverage && genhtml coverage.info --output-directory report

.PHONY: clean
clean:
	rm -rf $(OUT) coverage
