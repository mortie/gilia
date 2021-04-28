#!/bin/sh

set -e

workers=8

if ! [ -e "examples" ]; then
	mkdir examples
	cp ../test/examples/*.g examples
fi

make -C .. OUT=fuzz/gilia CC=afl-clang -j $workers

(afl-fuzz -i examples -o output -M fuzz1 -- gilia/gilia @@ | cat) &
for i in $(seq 2 $workers); do
	(afl-fuzz -i examples -o output -S fuzz$i -- gilia/gilia @@ | cat) &
done

wait
