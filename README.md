# Gilia

Gilia is a programming language I'm working on.
It consists of a compiler which generates bytecode, and a VM which executes
that bytecode.

To get a feel for the language, you can look at the examples in the
[test/examples](https://github.com/mortie/gilia/tree/main/test/examples)
directory. The `.g` files contain Gilia source code, and the
`.expected` files contain the text which the Lang2 programs are expected to output.

## Build

Clone this repo:

	git clone https://github.com/mortie/gilia.git
	cd gilia

Build:

	make -j

Run the REPL:

	./build/gilia

Run tests:

	git submodule update --init
	make -j -C test check

## About

Gilia is an expression-based language with a really neat syntax.

Hello World is exactly like in Python 2:

	print "Hello World"

However, unlike in Python 2, this isn't a hackish print statement,
it's just a normal function call.

Semicolons are unnecessary:

	print "Hello World
	print "Goodbye World"
	print "See You"

Control flow _looks_ as you would expect:

	i := 10
	if i < 20 {
		print "i is less than 20"
	}

However, it probably doesn't _work_ as you would expect.
`if i < 20 { ... }` is a function call to the function `if`, where the first argument
is the result of calling `<` on `i` and `20`, and the second argument is a lambda.

While loops are also cool:

	i := 0
	while {i < 10} {
		print i
		i = i + 1
	}

Here, the first argument is a condition function, and the second argument is
the body function.
