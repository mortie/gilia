# Gilia Architecture

Gilia is a programming language. Its implementation is split into two major parts:
the compiler and the virtual machine. There are also shared components.

## I/O

The gilia implementation uses a simple but effective I/O model defined in
[include/gilia/io.h](https://github.com/mortie/gilia/blob/main/include/gilia/io.h).
There are two basic "interface" types: the reader `gil_io_reader`
and the writer `gil_io_writer`.

A reader is something which data can be read from. A reader must implement
the `size_t read(struct gil_io_reader *self, void *buf, size_t len)` function,
which fills `buf` with up to `len` bytes, and returns the number of bytes written.
The reader should block until something is read into `buf`. A return value of
0 should be interpreted as EOF.

A writer is something which data can be written to. A writer must impleemnt
the `void write(struct gil_io_writer *self, void *buf, size_t len)` function,
which writes `len` bytes from `buf`. The writer should block until
all bytes are written.

For example writers, look at `gil_io_mem_writer` (with its `write` function
`gil_io_mem_write`) and `gil_io_file_writer` (with its `write` function `gil_io_file_write`).
For example readers, look at `gil_io_mem_reader` (with its `read` function
`gil_io_mem_read`) and `gil_io_file_reader` (with `read` function `gil_io_mem_read`).

Readers and writers aren't necessarily meant to be fast. Instead, if you're doing
a lot of reading or writing (especially short reads and writes), you should be
using the types `gil_bufio_reader` and `gil_bufio_writer`. These implement buffered
reading/writing, only calling the underlying `read` or `write` function when
the buffer gets full. The `gil_bufio_reader` also has the ability to peek
some number (less than `gil_IO_BUFSIZ`) forwards.

## Atoms

In gilia, every identifier is an "atom" (name borrowed from Erlang). An atom is just
a number which represents the name. Every identifier gets its own ID, and every
occurrence of a particular identifier name will be associated with the same ID.
You can think of it like a hash, except that there are no collisions.

Every namespace (be that the namespace of local variables in a function, the
namespace of global functions, or a namespace variable) is just a map from
an integer ID (the atom) to a variable reference.

Atom literals such as `'foo` will evaluate to an atom variable which has the
same numeric ID as the identifier `foo`. This makes it possible to pass
names around at runtime.

## Compiler

The compiler consists of the lexer, the parser and the code generator.

The lexer reads from a `reader` and produces tokens as it reads.
A token carries information about what kind of token it is (identifier, number,
open paren, etc). In addition, the token might carry extra information;
number tokens contain a number, dot-number tokens contain an integer,
and identifiers and strings contain a string.

The parser reads tokens from the lexer and uses the code generator to generate
bytecode. Unlike other languages, the parser doesn't produce a syntax tree;
it just emits bytecode as it goes. This is possible thanks to careful syntax
design combined with careful bytecode design.

The code generator contains, for the most part, thin wrappers around bytecode
instructions. For example, the `gil_gen_halt` function just contains one line
to write an `gil_OP_HALT` instruction. However, it also has the responsibility
of keeping track of the atoms (so that the name `foo` always gets the same ID
everywhere), and of keeping track of the string literals (so that multiple
string literals with the same content only show up once in the bytecode).

## Virtual Machine

The virtual machine executes bytecode.
The central piece of the VM is the `gil_vm_step` function, which looks at the
code at the instruction pointer and executes it with a giant switch statement.
