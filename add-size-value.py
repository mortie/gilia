#!/usr/bin/env python3

import sys
import subprocess
import struct
import io

if len(sys.argv) != 2:
    print("Usage: add-size-value.py <file>")
    exit(1)

out = subprocess.check_output(["objdump", "-t", sys.argv[1]])
addr = None
for line in out.split(b'\n'):
    if b"gil_binary_size" in line:
        addr = int(line.split(b' ')[0], 16)

if addr is None:
    print("Couldn't find gil_binary_size")
    exit(1)

with open(sys.argv[1], "r+b") as f:
    f.seek(0, io.SEEK_END)
    size = f.tell()
    print(f"Write {size} to addr {hex(addr)}")
    f.seek(addr)
    f.write(struct.pack("i", size))
