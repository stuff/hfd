#!/usr/bin/env python3
"""Decrypts the HFD boot sector payload (stage 1).

usage: decrypt_boot.py hifidrea.st out.bin

The boot code (offset $1E) XOR-decrypts $132 bytes taken from boot sector
offset $6A, with a key that counts down from -$132, and writes them to
absolute address $8 (over the exception vectors), then jumps to $10.
The output file is a RAM image starting at address 0 (8 zero bytes + payload).
"""
import sys
disk = open(sys.argv[1], 'rb').read()
src = disk[0x6a:]
out = bytearray()
d3 = 0x132
while True:
    t = (-d3) & 0xffff          # neg.w d3 ; beq done
    if t == 0:
        break
    out.append(src[len(out)] ^ (t & 0xff))   # eor.b d3,d1
    d3 = (~t) & 0xffff          # not.w d3
open(sys.argv[2], 'wb').write(bytes(8) + out)
print('%d bytes decrypted -> loaded at $8, entry $10' % len(out))
