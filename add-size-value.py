#!/usr/bin/env python3

import os
import argparse
import sys
import struct
from elftools.elf.elffile import ELFFile

symbol_name = "gil_binary_size"
binary_size = 0

parser = argparse.ArgumentParser(description="Add 'gil_binary_size' to an ELF binary")
parser.add_argument("file")
args = parser.parse_args()

if __name__ == "__main__":
    binary_size = os.stat(args.file).st_size

    with open(args.file, "rb+") as f:
        elf = ELFFile(f)
        symtab = None
        for section in elf.iter_sections():
            if section.name == ".symtab":
                symtab = section
                break
        if symtab == None:
            raise Exception("Couldn't find symbol table ('.symtab' section)")

        symbol = None
        for sym in section.iter_symbols():
            if sym.name == symbol_name:
                symbol = sym
                break
        if symbol == None:
            raise Exception("Couldn't find symbol '" + symbol_name + "'")

        symbol_offset = None
        for i, section in enumerate(elf.iter_sections()):
            if i == symbol.entry.st_shndx:
                symbol_offset = section.header.sh_offset + (
                        symbol.entry.st_value - section.header.sh_addr)
        if symbol_offset == None:
            raise Exception("Couldn't find section with index " + str(symbol.entry.st_shndx))

        print("Writing", hex(binary_size), "to address", hex(symbol_offset), file=sys.stderr)
        f.seek(symbol_offset)
        f.write(struct.pack("I", binary_size))
