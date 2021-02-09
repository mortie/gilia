#!/bin/sh
set -ex

box --run -q
mkdir -p coverage
gcov $(find bx-out -type f -name '*.o')
mv *.gcov coverage
cp $(find bx-out -type f -name '*.gcda' -or -name '*.gcno') coverage

cd coverage
lcov --capture --directory . --output-file coverage.info
genhtml coverage.info --output-directory report
