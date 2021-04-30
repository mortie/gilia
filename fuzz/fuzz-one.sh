#!/bin/sh

set -e

workers=8

if ! [ -e "examples" ]; then
	mkdir examples
	cp ../test/examples/*.g examples
fi

make -C .. OUT=fuzz/gilia CC=afl-clang -j $workers

# Gilia should work with little stack space.
ulimit -s 700
ulimit -a

if [ -e "output/fuzz1/is_main_node" ]; then
	count="$(ls "output/" | wc -l)"
	name="fuzz$((count + 1))"
	opt="-S"
else
	name="fuzz1"
	opt="-M"
fi

echo afl-fuzz -i examples -o output "$opt" "$name" -- gilia/gilia --timeout 0.8 -
afl-fuzz -i examples -o output "$opt" "$name" -- gilia/gilia --timeout 0.8 -
