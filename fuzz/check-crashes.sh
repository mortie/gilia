#!/bin/sh

set -e

make -C .. OUT=fuzz/gilia CC=afl-clang -j 8

# Gilia should work with little stack space.
ulimit -s 700
ulimit -a

for f in output/*/crashes/*; do
	if ! [ -e "$f" ] || [ "$(basename "$f")" = "README.txt" ]; then
		continue
	fi

	echo
	echo "=== Running: $f"
	valgrind -q ./gilia/gilia --timeout 0.8 "$f"

	echo "OK? [Y/n]"
	read ans
	if [ "$ans" != "Y" ] && [ "$ans" != "y" ] && [ "$ans" != "" ]; then
		exit 1
	fi
done
