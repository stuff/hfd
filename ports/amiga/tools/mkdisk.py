#!/usr/bin/env python3
"""Makes the bootable disk.
usage: mkdisk.py <zx0> <boot.bin> <main.bin> <modules dir> <cache dir> <out.adf>

The disk: the bootblock (with its checksum), main.bin's first part (up to `later`, as
is: the bootblock reads it), then its second part and the 8 modules, each compressed
with ZX0. (Shrinkler compresses the modules 10 % smaller, but decompresses at 4 KB/s on
a 7 MHz 68000: Just Spank It would take 25 s, the demo took 11.) Samples often compress
better as differences between successive bytes: each module is stored either way,
whichever is smaller.

The files' table goes into main.bin's first part, at FILE_TABLE (main.s): for each file,
its offset on the disk, the bytes to read (whole sectors), where to read them in the
destination, where the compressed data starts there, its size, and where its samples
start if they are stored as differences (0 if not). The compressed data ends its
sectors, which end at the end of the destination plus the compressor's margin: the
file decompresses in place."""
import hashlib, struct, subprocess, sys
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

zx0, boot_path, main_path, modules_dir, cache, out = sys.argv[1:7]
cache = Path(cache)
cache.mkdir(parents=True, exist_ok=True)

FILE_TABLE, ENTRY = 8, 24
SECTOR, DISK = 512, 901120
PART2_MARGIN, SONG_RESERVE = 4096, 204 * 1024   # as in memory.i
MODULES = sorted(Path(modules_dir).glob('*.mod'))
assert len(MODULES) == 8


def zx0_pack(data: bytes) -> tuple[bytes, int]:
    """ZX0's data and its margin for decompressing in place (its "delta")."""
    key = hashlib.sha1(data).hexdigest()
    packed, margin_file = cache / f'{key}.zx0', cache / f'{key}.zx0delta'
    if not margin_file.exists():   # (written last: the entry is complete)
        raw, tmp = cache / f'{key}.bin', cache / f'{key}.tmp'
        raw.write_bytes(data)
        log = subprocess.run([zx0, '-f', str(raw), str(tmp)], check=True, capture_output=True, text=True).stdout
        tmp.rename(packed)
        margin_file.write_text(log.split('delta ')[1].split(')')[0])
        raw.unlink()
    return packed.read_bytes(), int(margin_file.read_text())


def deltas(module: bytes) -> tuple[bytes, int]:
    """The module with its samples as differences, and where they start."""
    start = 1084 + (max(module[952:952 + 128]) + 1) * 1024
    d = bytearray(module)
    prev = 0
    for k in range(start, len(d)):
        d[k], prev = (module[k] - prev) & 255, module[k]
    return bytes(d), start


main = bytearray(Path(main_path).read_bytes())
later = struct.unpack('>I', main[4:8])[0]
assert later % SECTOR == 0 and len(main) % SECTOR == 0
candidates = [[(bytes(main[later:]), 0)]]
for m in MODULES:
    raw = m.read_bytes()
    candidates.append([(raw, 0), deltas(raw)])
with ThreadPoolExecutor() as pool:   # ZX0 takes a minute or two per module: all at once
    list(pool.map(lambda c: zx0_pack(c[0]), [c for cs in candidates for c in cs]))
files = [(*candidates[0][0], PART2_MARGIN)]
for cs in candidates[1:]:
    files.append(min(cs, key=lambda f: len(zx0_pack(f[0])[0])) + (SONG_RESERVE,))

disk = bytearray(DISK)
offset = 1024 + later
table = bytearray()
for k, (data, delta_from, reserve) in enumerate(files):
    packed, margin = zx0_pack(data)
    end = len(data) + margin
    end += end % 2
    sectors = -(-len(packed) // SECTOR) * SECTOR
    load_at = end - sectors
    data_at = end - len(packed)
    assert load_at >= 0 and end <= len(data) + (PART2_MARGIN if k == 0 else reserve - len(data)), \
        f'not enough room to decompress in place: {len(data)} bytes, margin {margin}, {sectors} to read'
    assert offset + sectors <= DISK, 'the disk is full'
    disk[offset + data_at - load_at:offset + data_at - load_at + len(packed)] = packed
    table += struct.pack('>IIIIII', offset, sectors, load_at, data_at, len(data), delta_from)
    offset += sectors
main[FILE_TABLE:FILE_TABLE + len(table)] = table

boot = bytearray(Path(boot_path).read_bytes().ljust(1024, b'\0'))
assert len(boot) == 1024, 'the bootblock is larger than 1024 bytes'
total = 0
for (v,) in struct.iter_unpack('>I', boot):
    total += v
    if total > 0xffffffff:
        total = (total & 0xffffffff) + 1
struct.pack_into('>I', boot, 4, ~total & 0xffffffff)
disk[0:1024] = boot
disk[1024:1024 + later] = main[:later]
Path(out).write_bytes(disk)
print(f'{out}: {offset // 1024} KB used of 880 (program {later // 1024} KB + '
      f'{(offset - 1024 - later) // 1024} KB compressed)')
