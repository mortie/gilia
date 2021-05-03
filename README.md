# Gilia

Gilia is a programming language I'm working on.
It consists of a compiler which generates bytecode, and a VM which executes
that bytecode.

To get a feel for the language, you can look at the examples in the
[test/examples](https://github.com/mortie/gilia/tree/main/test/examples)
directory. The `.g` files contain Gilia source code, and the
`.expected` files contain the text which the Gilia programs are expected to output.

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

## TODO

### Function parameters

Example:

	add := {$first $second
		first + second
	}

With validation:

	is-real := {$x (typeof x) == 'real}

	add := {$first: is-real $second: is-real
		first + second
	}

### Module system

* Make it possible to implement modules in gilia
  (MVP implementation just adds modules implemented in C)
* Deduplicate imports; multiple `import "fs"` expressions should return the same object

### 'With' blocks

	with fs.open("lol") {$f
		print f.read()
	}

`with` would be something like this:

	with := {$val $block
		block val
		val.@destroy()
	}

### Line continuation syntax

* It's useful to be able to make lines not terminated by newline.

```
	print "Hello world"
	-> "the answer is:" 42
```

Should work as if there's no line break between the lines.

### Pattern matching

	match whatever
	-> is-error {$err
		print "Oh noes it failed" err
	}
	-> is-number {$num
		print "seems like we got a number"
	}
	-> is-string {$str
		print "we got a string" str
	}
	-> is-any {$val
		print "we got something else" val
	}

### At-methods (like Python's dunder methods)

Implementing custom functionality should be a thing. Some namespace property
names should be special. Here are some ideas:

* `@init`: The presence of this function should indicate that the namespace
  is a "class". Calling the namespace as if it was a function should result in:
	* A new namespace should be created, with the other namespace as its parent
	* The `@init` function should be called on the new namespace
* `@destroy`: Destructor method, possibly used by things like `with`-blocks.
  Maybe called by the GC?
* `@add`, `@sub`, `@mul`, `@div`: Overload the math operators

### Error handling

* Error handling is hard. Needs rework.

### Generational GC

* The current GC just does a global mark+sweep every time it has to clean up.
* Generational GC: Keep around a list of young objects. When there are many
  (say >=256) young objects, do a mark+sweep on only the young objects.
  Clean the list of young objects (so all remaining objects are considered old).
* Do a full GC every once in a while, but much less frequently than the young GC.
* Any time a young object gains a reference from an old object, it should
  no longer be considered young.
* Or maybe the mark should happen across the whole object graph, and the sweep
  is the only part which needs to be limited to the young objects?
  Needs experimentation.
