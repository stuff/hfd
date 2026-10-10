#!/usr/bin/env python3
"""Makes the bootable disk: the bootblock (with its checksum), then main.bin from sector 2.
usage: mkadf.py <boot.bin> <main.bin> <out.adf>"""
import struct, sys

boot = bytearray(open(sys.argv[1], 'rb').read().ljust(1024, b'\0'))
main = open(sys.argv[2], 'rb').read()
assert len(boot) == 1024, 'the bootblock is larger than 1024 bytes'

# The checksum makes the sum of the bootblock's longwords, with carry wraparound, $ffffffff.
total = 0
for (v,) in struct.iter_unpack('>I', boot):
    total += v
    if total > 0xffffffff:
        total = (total & 0xffffffff) + 1
struct.pack_into('>I', boot, 4, ~total & 0xffffffff)

disk = bytes(boot) + main
assert len(disk) <= 901120, 'main.bin does not fit on the disk'
open(sys.argv[3], 'wb').write(disk.ljust(901120, b'\0'))
