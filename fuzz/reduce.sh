#!/bin/sh

if [ -z "$1" ]; then
	echo "Usage: reduce <file> [out]"
fi

infile="$1"

if [ -z "$2" ]; then
	outfile="reduced.g"
else
	outfile="$2"
fi

afl-tmin -i "$infile" -o "$outfile" -- ./gilia/gilia -
