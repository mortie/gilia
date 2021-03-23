#!/bin/sh
hyperfine '../../bx-out/target while-test.l2' 'python3 while-test.py'
