#!/bin/sh
hyperfine '../../bx-out/target while-test.g' 'python3 while-test.py'
