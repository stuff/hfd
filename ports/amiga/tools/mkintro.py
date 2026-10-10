#!/usr/bin/env python3
"""Converts the intro's data from rip/ for the Amiga port.
usage: mkintro.py <rip dir> <out dir>

Writes the screens' bitmaps (*.raw: 320 × 200, 4 interleaved bitplanes, 160 bytes per
line), the HIGH FIDELITY DREAMS rules as wide strips, and intro.i: for each screen a
Copper list template with its patch table, and the intro's tables.

The Copper list of a screen gives each frame line its palette, as the demo's per-line
palettes do (rasters). It only sets the colours a line shows: colour 0 (the border) and
the indices its pixels use. Colours that a fade level can change are patched every
frame: each patch entry holds the colour faded to the 8 levels, and the band of lines
whose level it follows (see intro.s)."""
import json, sys
from pathlib import Path
from PIL import Image

rip, out = Path(sys.argv[1]), Path(sys.argv[2])
out.mkdir(parents=True, exist_ok=True)
pictures = rip / 'graphics/pictures'

# Frame geometry: the reference's 416 × 276 frame, the ST screen at (48, 29).
FRAME_H, SCREEN_Y = 276, 29
V0 = 30                    # PAL line of the frame's first line
DIWSTRT, DIWSTOP = 0x3b79, 0x03b9  # the screen: lines 59-258, lowres pixels $79-$1b8
# Copper timing (colour clocks): a line's colours change after the screen's last pixel
# of the line before (EARLY), colour 0 in the horizontal blank between the right and
# left borders (COLOR0), and the wide lines' colour 0 in the middle of the screen (MID).
EARLY_H, COLOR0_H, MID_H = 0xde, 0x12, 0x70
COLOR0_AT = 6              # an early move that lands in the horizontal blank

BPLCON0, BPLCON1, BPLCON2, BPL1MOD, BPL2MOD = 0x100, 0x102, 0x104, 0x108, 0x10a
DIWSTRT_R, DIWSTOP_R, DDFSTRT_R, DDFSTOP_R, BPL1PTH, COLOR00 = 0x8e, 0x90, 0x92, 0x94, 0xe0, 0x180


def hexcolor(c: str) -> int:
    """#rrggbb (each component 17 × its 4-bit value) to $rgb."""
    v = int(c[1:], 16)
    return ((v >> 16) // 17) << 8 | ((v >> 8 & 255) // 17) << 4 | (v & 255) // 17


def fade(c: int, j: int, keep_lsb: bool) -> int:
    """The demo's fade (SPEC.md 1), as fadeColor() in the web port's display.ts."""
    if j <= 0:
        return c
    result = 0
    for shift in (8, 4, 0):
        n = c >> shift & 15
        v3 = (n >> 1) - j
        if v3 >= 0:
            result |= ((v3 << 1) | (n & 1 if keep_lsb else 0)) << shift
    return result


def indexed(name: str) -> list[list[int]]:
    im = Image.open(pictures / name)
    w, h = im.size
    data = list(im.getdata())
    return [data[y * w:(y + 1) * w] for y in range(h)]


def planar(rows: list[list[int]], width_words: int) -> bytes:
    """Rows of indices to interleaved bitplanes: for each row, plane 0 to 3."""
    data = bytearray()
    for row in rows:
        row = row + [0] * (width_words * 16 - len(row))
        for plane in range(4):
            for x in range(0, width_words * 16, 8):
                byte = 0
                for i in range(8):
                    byte = byte << 1 | (row[x + i] >> plane & 1)
                data.append(byte)
    return bytes(data)


class Copper:
    def __init__(self):
        self.words: list[int] = []
        self.patches: list[tuple[int, int, list[int]]] = []
        self.crossed = False  # past line 255: waits compare the low 8 bits

    def wait(self, v: int, h: int):
        if v >= 256 and not self.crossed:
            self.words += [0xffdf, 0xfffe]
            self.crossed = True
        self.words += [(v & 255) << 8 | h | 1, 0xfffe]
        if v == 255 and h >= 0xdf - 1:
            self.crossed = True

    def move(self, reg: int, value: int):
        self.words += [reg, value]

    def color(self, i: int, spec):
        """spec: $rgb (never faded), or (band, the colour at the 8 fade levels)."""
        if isinstance(spec, tuple):
            band, values = spec
            self.patches.append((len(self.words) * 2 + 2, band, list(values)))
            spec = values[0]
        self.move(COLOR00 + 2 * i, spec)


def build(name: str, rows, colors, height=200, wide_rows=()):
    """A screen's Copper list. rows: its rows of indices (416 wide for wide rows): the
    colours each line needs. colors(y, i): colour i on screen line y (in the borders too),
    a $rgb never faded or (band, values at the 8 fade levels), the band being the one
    whose level it follows. height: the screen's lines (200, more if the lower border is
    open). wide_rows: the rows whose borders show the row's first and last pixels (the
    side borders opened)."""
    cop = Copper()
    cop.move(DIWSTRT_R, DIWSTRT)
    cop.move(DIWSTOP_R, ((DIWSTRT >> 8) + height & 255) << 8 | DIWSTOP & 255)
    cop.move(DDFSTRT_R, 0x30)
    cop.move(DDFSTOP_R, 0xc8)
    cop.move(BPLCON0, 0x4200 if rows else 0x0200)
    cop.move(BPLCON1, 0x88)   # the screen's pixels 8 lowres pixels after the fetch
    cop.move(BPLCON2, 0)
    cop.move(BPL1MOD, 120)
    cop.move(BPL2MOD, 120)
    planes = len(cop.words) * 2 + 2
    for p in range(4):
        cop.move(BPL1PTH + 4 * p, 0)
        cop.move(BPL1PTH + 4 * p + 2, 0)

    key = lambda spec: spec if isinstance(spec, int) else (spec[0], tuple(spec[1]))
    state = [None] * 16
    for line in range(FRAME_H):
        y = line - SCREEN_Y
        v = V0 + line
        needed = {0}
        if rows and 0 <= y < height:
            needed |= set(rows[y])
        want = {i: colors(y, i) for i in needed}
        right = None
        if y in wide_rows:
            want[0] = colors(y, rows[y][0])
            right = colors(y, rows[y][-1])
        changes = [i for i in sorted(want) if state[i] != key(want[i])]
        for i in changes:
            state[i] = key(want[i])
        emit = lambda i: cop.color(i, want[i])
        early = [i for i in changes if i != 0]
        if line == 0:
            for i in changes:
                emit(i)
        else:
            if early:
                cop.wait(v - 1, EARLY_H)
                for i in early[:COLOR0_AT]:
                    emit(i)
            if 0 in changes:
                if len(early) < COLOR0_AT:
                    cop.wait(v, COLOR0_H)
                emit(0)
            for i in early[COLOR0_AT:]:
                emit(i)
            assert len(changes) <= 16
        if right:
            cop.wait(v, MID_H)
            cop.color(0, right)
            state[0] = key(right)
    cop.words += [0xffff, 0xfffe]

    asm = [f'{name}_copper:']
    for k in range(0, len(cop.words), 8):
        asm.append('\tdc.w\t' + ','.join(f'${w:04x}' for w in cop.words[k:k + 8]))
    asm.append(f'{name}_copper_end:')
    asm.append(f'{name}_patches:')
    for offset, band, values in cop.patches:
        asm.append(f'\tdc.w\t{offset},{band},' + ','.join(f'${c:03x}' for c in values))
    asm.append('\tdc.w\t-1')
    asm.append(f'{name.upper()}_PLANES\tequ\t{planes}')
    return '\n'.join(asm) + '\n', len(cop.words) * 2


def meta(name: str):
    d = json.loads((pictures / name).read_text())
    return [hexcolor(c) for c in d['palette']], d.get('per_line_palette', [])


def bands(*ranges):
    """band_of for screens whose bands are ranges of lines [y0, y1), one band each."""
    return lambda y, i: next((b for b, (y0, y1) in enumerate(ranges) if y0 <= y < y1), None)


def picture(palette, per_line, band_of, keep=()):
    """colors() for a picture: its palette and per-line palette (rip/graphics' JSON), each
    colour following band_of(y, i) (None: never faded), keeping the STE's low bit for the
    bands in keep (the loader's fade)."""
    def colors(y, i):
        c = palette[i]
        if 0 <= y < 200 and y < len(per_line) and str(i) in per_line[y]:
            c = hexcolor(per_line[y][str(i)])
        band = band_of(y, i)
        return c if band is None else (band, [fade(c, j, band in keep) for j in range(8)])
    return colors


sources = []
sizes = []

# The ear: band 0 is the whole screen (the loader's fade), bands 1 and 2 are colours 14
# and 15 on the words' lines (DECRUNCHING and LOADING, set directly every frame); there,
# the other colours aren't faded (SPEC.md 3.1).
ear = indexed('loading_screen_indexed.png')
(out / 'ear.raw').write_bytes(planar(ear, 20))
WORD_LINES = range(189, 200)


def ear_band(y, i):
    if y in WORD_LINES:
        return {0: 0, 14: 1, 15: 2}.get(i)
    return 0


src, size = build('ear', ear, picture(*meta('loading_screen.json'), ear_band, keep=(0,)))
sources.append(src), sizes.append(size)

aura = indexed('fullscreen/screen_an_aura_production_indexed.png')
(out / 'aura.raw').write_bytes(planar(aura, 20))
src, size = build('aura', aura, picture(*meta('fullscreen/screen_an_aura_production.json'),
                                        bands((53, 73), (84, 116), (127, 147))))
sources.append(src), sizes.append(size)

aeon = indexed('fullscreen/screen_code_and_design_aeon_indexed.png')
(out / 'aeon.raw').write_bytes(planar(aeon, 20))
src, size = build('aeon', aeon, picture(*meta('fullscreen/screen_code_and_design_aeon.json'),
                                        bands((7, 39), (42, 62), (65, 97), (128, 192))))
sources.append(src), sizes.append(size)

# HIGH FIDELITY DREAMS: 416 wide, x = 0 at the frame's left edge. The screen shows
# x = 48 to 367; the rules' lines are redrawn every frame from the strips (intro.s), with
# index 8 (black there) as their background. Bands: 0 the rules, 1 to 3 the words.
hfd = indexed('fullscreen/screen_high_fidelity_dreams_wide_indexed.png')
TOP_RULE, BOTTOM_RULE = range(14, 22), range(178, 186)
rules = set(TOP_RULE) | set(BOTTOM_RULE)
screen = [row[48:368] if y not in rules else [8] * 320 for y, row in enumerate(hfd)]
for y, row in enumerate(hfd):
    if y not in rules:
        assert set(row[:48]) | set(row[368:]) <= {0}, f'HFD line {y} has pixels in the borders'
(out / 'hfd.raw').write_bytes(planar(screen, 20))
# The rules' strips: column c of the top strip is the image's x = c - 240, of the bottom
# strip x = c + 48, index 8 outside the rule. 41 words: one more for the shifts.
top = [[hfd[y][c - 240] if 66 <= c - 240 <= 415 else 8 for c in range(656)] for y in TOP_RULE]
bottom = [[hfd[y][c + 48] if c + 48 <= 349 else 8 for c in range(656)] for y in BOTTOM_RULE]
(out / 'rules.raw').write_bytes(planar(top, 41) + planar(bottom, 41))
hfd_rows = [row if y in rules else row[48:368] for y, row in enumerate(hfd)]
words_band = bands((37, 69), (84, 116), (131, 163))
src, size = build('hfd', hfd_rows, picture(
    *meta('fullscreen/screen_high_fidelity_dreams.json'),
    lambda y, i: 0 if y in rules else (None if words_band(y, i) is None else words_band(y, i) + 1)),
    wide_rows=rules)
sources.append(src), sizes.append(size)

src, size = build('black', None, lambda y, i: 0)
sources.append(src), sizes.append(size)

# The menu (SPEC.md 5), as the web port's scenes/menu.ts: one template per cursor
# position, every colour following band 0 (the menu's fade), with the measured
# exceptions in the values at each level.
MENU_ROWS = 218                       # the menu opens the lower border
ENTRY_Y, ENTRY_STEP, ENTRY_H = 37, 23, 20
BASE = [0x000, 0x004, 0x226, 0x448, 0x66a, 0xccc, 0x000, 0x000,
        0x000, 0x626, 0x848, 0xa6a, 0xc8c, 0xfff, 0x000, 0x000]
ORANGE = [0x840, 0xa62, 0xc84, 0xea6, 0xfff]   # SELECT SONG, LOADING, DECRUNCHING
PINK = [0x626, 0x848, 0xa6a, 0xc8c, 0xfff]
CURSOR = [0x226, 0x448, 0x66a, 0x88c, 0xfff]   # colours 1-5 of the entry under the cursor
CURSOR_BAR = [0xfff, 0x226, 0x448, 0x66a, 0x88c, 0xfff]  # colour 0 on its lines 7-12
# The 3-line rules (colour 0) don't fade like the palette: the measured table, per level.
RULE_FADE = [[0xfff, 0xaaa, 0x666], [0xccc, 0x888, 0x666], [0xaaa, 0x888, 0x444], [0x888, 0x666, 0x444],
             [0x666, 0x444, 0x222], [0x444, 0x444, 0x222], [0x222, 0x222, 0x000], [0x000, 0x000, 0x000]]
faded = lambda c, band=0: (band, [fade(c, j, False) for j in range(8)])
rule = lambda line: (0, [RULE_FADE[j][line] for j in range(8)])

menu = [[p & 7 for p in row] for row in indexed('../menu/menu_select_song_indexed.png')]
(out / 'menu.raw').write_bytes(planar(menu, 20))
# Any entry may be the one playing (drawn with bitplane 3: index + 8).
entry_rows = {y for k in range(8) for y in range(ENTRY_Y + k * ENTRY_STEP, ENTRY_Y + k * ENTRY_STEP + ENTRY_H)}
menu_needed = [row + [p | 8 for p in row if p] if y in entry_rows else row for y, row in enumerate(menu)]


def menu_colors(cursor):
    entry = ENTRY_Y + cursor * ENTRY_STEP

    def colors(y, i):
        if i == 0 and (2 <= y <= 4 or 30 <= y <= 32):
            return rule(y - 2 if y <= 4 else y - 30)
        if entry + 7 <= y <= entry + 12:
            if i == 0:
                return faded(CURSOR_BAR[y - entry - 7])
            if i in (7, 15):
                return faded(0xfff)
        if 7 <= y <= 26 and 1 <= i <= 5:
            return faded(ORANGE[i - 1])
        if entry <= y < entry + ENTRY_H and 1 <= i <= 5:
            return faded(CURSOR[i - 1])
        if i == 5 and 0 <= y < MENU_ROWS:
            # The entries' outline: fff one fade level down, not ccc faded.
            return (0, [0xccc] + [fade(0xfff, j, False) for j in range(1, 8)])
        return faded(BASE[i])
    return colors


for k in range(8):
    src, size = build(f'menu{k}', menu_needed, menu_colors(k), height=MENU_ROWS)
    sources.append(src), sizes.append(size)

# The loading screen (SPEC.md 6): band 0 the screen's fade, band 1 the word's lines
# (LOADING then DECRUNCHING, which also fade on their own).
LOADING_WORD_Y, LOADING_NAME_Y = 78, 106
words = [indexed('../menu/words/word_loading_indexed.png'), indexed('../menu/words/word_decrunching_indexed.png')]
for word, name in zip(words, ('loading', 'decrunching')):
    x = (320 - len(word[0])) >> 1
    (out / f'word_{name}.raw').write_bytes(planar([[0] * x + row + [0] * (320 - x - len(row)) for row in word], 20))
loading_needed = [[] for _ in range(200)]
for y in range(20):
    loading_needed[LOADING_WORD_Y + y] = sorted(set(words[0][y]) | set(words[1][y]))
    loading_needed[LOADING_NAME_Y + y] = [9, 10, 11, 12, 13]
LOADING_PAL = [0, *ORANGE, 0, 0, 0, *PINK, 0, 0]


def loading_colors(y, i):
    if i == 0 and (67 <= y <= 69 or 135 <= y <= 137):
        return rule(y - 67 if y <= 69 else y - 135)
    if LOADING_WORD_Y <= y < LOADING_WORD_Y + 20 and 1 <= i <= 5:
        return faded(LOADING_PAL[i], 1)
    return faded(LOADING_PAL[i])


src, size = build('loading', loading_needed, loading_colors)
sources.append(src), sizes.append(size)


def pulse(k: int) -> int:
    """LOADING's colour at VBL k of the ear screen (k from 1), as in the web port's intro.ts."""
    m = (k - 1) % 56
    n, sub = m >> 2, m & 3
    up = n < 7
    level = n if up else 7 - (n - 7)
    r, g, b = (level + ((1 if up else -1) if sub >= i else 0) for i in range(3))
    return (r * 2) << 8 | (g * 2) << 4 | b * 2


# The text pages' colours: an STE palette word (odd nibble order) to $rgb, for all 4096.
ste = lambda n: (n & 7) << 1 | n >> 3
(out / 'ste_colors.raw').write_bytes(b''.join(
    (ste(w >> 8 & 15) << 8 | ste(w >> 4 & 15) << 4 | ste(w & 15)).to_bytes(2, 'big') for w in range(4096)))


def words(label: str, values) -> str:
    rows = [values[i:i + 8] for i in range(0, len(values), 8)]
    return label + ':\n' + ''.join('\tdc.w\t' + ','.join(f'${v:03x}' for v in row) + '\n' for row in rows)


(out / 'intro.i').write_text(
    '; Generated by tools/mkintro.py.\n'
    f'COPPER_SIZE\tequ\t{max(sizes)}\n'
    + ''.join(sources)
    + words('pulse_table', [pulse(k) for k in range(1, 57)])
    + words('ear_background', [fade(0x004, j, True) for j in range(8)]))
