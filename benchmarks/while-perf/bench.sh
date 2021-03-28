#!/bin/sh
if [ -e "../../build.old/gilia" ]; then
	hyperfine \
		'../../build.old/gilia while-test.g' \
		'../../build/gilia while-test.g' \
		'python3 while-test.py'
else
	hyperfine \
		'../../build/gilia while-test.g' \
		'python3 while-test.py'
fi
