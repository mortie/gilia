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

(afl-fuzz -i examples -o output -M fuzz1 -- gilia/gilia @@ | cat) &
main=$!

sleep 1
if ! kill -0 $main 2>/dev/null; then
	echo "Main process stopped, exiting."
	exit 1
fi

for i in $(seq 2 $workers); do
	(afl-fuzz -i examples -o output -S fuzz$i -- gilia/gilia - | cat) &
done

wait
sleep 1
