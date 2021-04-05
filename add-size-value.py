#!/usr/bin/env python3

import sys
import subprocess
import struct
import io

filename = None
strip = False
stripcmd = "strip"
dashes = False

def usage():
    print("Usage: add-size-value.py [options] <file>")
    print()
    print("Options:")
    print("  --strip:               Strip the file")
    print("  --strip-cmd <command>: The program to strip a file (default: 'strip')")

args = iter(sys.argv[1:])
for arg in args:
    if not dashes and arg == "--strip":
        strip = True
    elif not dashes and arg == "--strip-cmd":
        stripcmd = next(args)
    elif not dashes and arg == "--":
        dashes = True
    elif filename == None:
        filename = arg
    else:
        usage()
        exit(1)

if filename == None:
    usage()
    exit(1)

out = subprocess.check_output(["objdump", "-t", "--", filename])
addr = None
for line in out.split(b'\n'):
    if b"gil_binary_size" in line:
        addr = int(line.split(b' ')[0], 16)

if addr is None:
    print("Couldn't find gil_binary_size")
    exit(1)

if strip:
    subprocess.check_output([stripcmd, "--", filename])

with open(filename, "r+b") as f:
    f.seek(0, io.SEEK_END)
    size = f.tell()
    print(f"Write {size} to addr {hex(addr)}")
    f.seek(addr)
    f.write(struct.pack("i", size))
