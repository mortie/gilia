LIB_SRCS := \
	lib/gen/gen.c \
	lib/parse/lex.c \
	lib/parse/error.c \
	lib/parse/parse.c \
	lib/vm/vm.c \
	lib/vm/print.c \
	lib/vm/builtins.c \
	lib/vm/namespace.c \
	lib/bitset.c \
	lib/io.c \
	lib/loader.c \
	lib/strset.c \
	lib/trace.c

CMD_SRCS := \
	cmd/main.c \
	cmd/size.s
