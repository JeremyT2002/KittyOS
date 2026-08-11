"""Convert the Braille cat into a colour-keyed half-cell sprite + preview PNG.

Pipeline:  Braille -> dot bitmap -> region fill -> 2x downscale -> colour keys
"""
import sys, os, zlib, struct
from collections import deque

BITS = [(0, 0), (0, 1), (0, 2), (1, 0), (1, 1), (1, 2), (0, 3), (1, 3)]

VGA_RGB = [(0,0,0),(0,0,170),(0,170,0),(0,170,170),(170,0,0),(170,0,170),
           (170,85,0),(170,170,170),(85,85,85),(85,85,255),(85,255,85),
           (85,255,255),(255,85,85),(255,85,255),(255,255,85),(255,255,255)]

# colour keys -> VGA colour index
KEY_COLOR = {'k': 0, 'p': 5, 'g': 7, 's': 4, 'w': 15}

# ---------------------------------------------------------------- decode ----
ART = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'cat.txt')
lines = [l.rstrip('\n') for l in open(ART, encoding='utf-8') if l.strip('\n')]
w = max(len(l) for l in lines)
H, W = len(lines) * 4, w * 2
bmp = [[0] * W for _ in range(H)]
for cy, line in enumerate(lines):
    for cx, ch in enumerate(line):
        v = ord(ch) - 0x2800
        if 0 <= v <= 0xFF:
            for bit, (dx, dy) in enumerate(BITS):
                if v & (1 << bit):
                    bmp[cy * 4 + dy][cx * 2 + dx] = 1
ys = [y for y in range(H) if any(bmp[y])]
xs = [x for x in range(W) if any(bmp[y][x] for y in range(H))]
bmp = [r[xs[0]:xs[-1] + 1] for r in bmp[ys[0]:ys[-1] + 1]]
H, W = len(bmp), len(bmp[0])

# ------------------------------------------------------------- label fill ---
def label(pred):
    lab = [[-1] * W for _ in range(H)]
    regs = []
    for sy in range(H):
        for sx in range(W):
            if not pred(sy, sx) or lab[sy][sx] >= 0:
                continue
            rid = len(regs); q = deque([(sy, sx)]); lab[sy][sx] = rid; cells = []
            while q:
                y, x = q.popleft(); cells.append((y, x))
                for dy, dx in ((1,0),(-1,0),(0,1),(0,-1)):
                    ny, nx = y+dy, x+dx
                    if 0 <= ny < H and 0 <= nx < W and pred(ny, nx) and lab[ny][nx] < 0:
                        lab[ny][nx] = rid; q.append((ny, nx))
            regs.append(cells)
    return lab, regs

hole, holes = label(lambda y, x: not bmp[y][x])
ink,  inks  = label(lambda y, x: bmp[y][x] == 1)

# region ids identified from regions.py output
TART, HEAD, TAIL = 2, 3, 4
LEGS = (7, 8, 9, 10)

key = [[' '] * W for _ in range(H)]
for y in range(H):
    for x in range(W):
        if bmp[y][x]:
            key[y][x] = 'k'
        else:
            r = hole[y][x]
            if r == TART:                 key[y][x] = 'p'
            elif r in (HEAD, TAIL) or r in LEGS: key[y][x] = 'g'

# small ink blobs floating inside the tart become sprinkles, not outline
for cells in inks:
    if len(cells) > 14:
        continue
    touching = set()
    for (y, x) in cells:
        for dy, dx in ((1,0),(-1,0),(0,1),(0,-1)):
            ny, nx = y+dy, x+dx
            if 0 <= ny < H and 0 <= nx < W and not bmp[ny][nx]:
                touching.add(hole[ny][nx])
    if touching == {TART}:
        for (y, x) in cells:
            key[y][x] = 's'

# Which movable part each pixel belongs to: 0 body, 1 tail, 2..5 the four legs
# left to right.  The body's bottom outline sits on row 32, so the legs start at
# row 33 -- taking row 32 with them would tear a hole in the body.
LEG_PART = {r: 2 + i for i, r in enumerate(sorted(LEGS, key=lambda r: min(c[1] for c in holes[r])))}
part = [[0] * W for _ in range(H)]
for y in range(H):
    for x in range(W):
        r = hole[y][x] if not bmp[y][x] else -1
        if r == TAIL:    part[y][x] = 1
        elif r in LEGS:  part[y][x] = LEG_PART[r]
for y in range(H):
    for x in range(W):
        if not bmp[y][x]:
            continue
        if y >= 33:                       # leg outlines, body outline excluded
            near = [part[y][xx] for xx in range(max(0, x-3), min(W, x+4))
                    if part[y][xx] >= 2]
            part[y][x] = near[0] if near else 0
        elif 13 <= y <= 25 and x <= 12:   # tail outline
            part[y][x] = 1

# -------------------------------------------------------------- downscale ---
OW, OH = W // 2, (H + 1) // 2
def shrink(src_key, src_part):
    ok = [[' '] * OW for _ in range(OH)]
    op = [[0] * OW for _ in range(OH)]
    for y in range(OH):
        for x in range(OW):
            votes = {}
            parts = {}
            for dy in (0, 1):
                for dx in (0, 1):
                    sy, sx = y*2+dy, x*2+dx
                    if sy >= H or sx >= W:
                        continue
                    k = src_key[sy][sx]
                    votes[k] = votes.get(k, 0) + 1
                    parts[src_part[sy][sx]] = parts.get(src_part[sy][sx], 0) + 1
            # outline wins if it covers at least half the block, so lines survive
            if votes.get('k', 0) >= 2:   ok[y][x] = 'k'
            elif votes.get('s', 0) >= 1: ok[y][x] = 's'
            elif votes.get('k', 0) == 1 and votes.get(' ', 0) >= 2: ok[y][x] = 'k'
            else:
                ok[y][x] = max((v, k) for k, v in votes.items())[1]
            op[y][x] = max((v, p) for p, v in parts.items())[1]
    return ok, op

skey, spart = shrink(key, part)

# ------------------------------------------------------------- animation ----
def shift(base_key, base_part, deltas):
    """deltas: {part_id: dy}.  A part moved DOWN is stretched (its old pixels
    stay) so no gap opens where it joins the body; a part moved UP is really
    moved, which just tucks it into the body above it."""
    out = [[' '] * OW for _ in range(OH)]
    for y in range(OH):
        for x in range(OW):
            k = base_key[y][x]
            if k == ' ':
                continue
            d = deltas.get(base_part[y][x], 0)
            if d >= 0:
                out[y][x] = k                   # keep, i.e. stretch
    for y in range(OH):
        for x in range(OW):
            k = base_key[y][x]
            if k == ' ':
                continue
            d = deltas.get(base_part[y][x], 0)
            if d and 0 <= y + d < OH:
                out[y + d][x] = k
    return out

DOWN = {2: 1, 3: -1, 4: 1, 5: -1}
UP   = {2: -1, 3: 1, 4: -1, 5: 1}
FRAMES = [shift(skey, spart, {}),
          shift(skey, spart, {**DOWN, 1: -1}),
          shift(skey, spart, {1: 1}),
          shift(skey, spart, {**UP, 1: 0})]
# Vertical bob, in HALF-rows: a whole cell of bob is far too violent at this
# sprite size.
FRAME_DY = [0, 1, 0, 1]

# ------------------------------------------------------------ preview PNG ---
CW, CH = 9, 16
SW, SH = 80, 25
CAT_X, CAT_Y = 50, 7
TRAIL_COLORS = [4, 6, 14, 2, 3, 5]
TRAIL_TOP_HALF = CAT_Y * 2 + 2
BG = 1

def render(frame_i):
    px = [[BG] * (SW * CW) for _ in range(SH * CH)]
    def half(cx, cy_half, color):
        if color is None: return
        for yy in range(CH // 2):
            gy = cy_half * (CH // 2) + yy
            if gy >= SH * CH: return
            for xx in range(CW):
                gx = cx * CW + xx
                if gx < SW * CW:
                    px[gy][gx] = color
    art = FRAMES[frame_i % 4]
    dy_half = FRAME_DY[frame_i % 4]
    saw = (0, 0, 1, 1)
    for x in range(CAT_X + 1):
        off = saw[(x + frame_i) & 3]
        for b, col in enumerate(TRAIL_COLORS):
            half(x, TRAIL_TOP_HALF + b * 2 + off, col)
            half(x, TRAIL_TOP_HALF + b * 2 + 1 + off, col)
    for y in range(OH):
        for x in range(OW):
            k = art[y][x]
            if k != ' ':
                half(CAT_X + x, CAT_Y * 2 + y + dy_half, KEY_COLOR[k])
    return px

def write_png(px, path):
    h = len(px); w = len(px[0])
    raw = b''.join(b'\x00' + bytes(b for x in row for b in VGA_RGB[x]) for row in px)
    def chunk(t, d):
        return struct.pack('>I', len(d)) + t + d + struct.pack('>I', zlib.crc32(t + d) & 0xffffffff)
    open(path, 'wb').write(b'\x89PNG\r\n\x1a\n'
        + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0))
        + chunk(b'IDAT', zlib.compress(raw, 9)) + chunk(b'IEND', b''))

if '--preview' in sys.argv:
    write_png(render(0), 'preview0.png')
    write_png(render(1), 'preview1.png')
    print('sprite %dx%d half-rows (%d cells tall)' % (OW, OH, (OH + 1) // 2))
    print('wrote preview0.png preview1.png')

if '--header' in sys.argv:
    path = sys.argv[sys.argv.index('--header') + 1]
    out = []
    out.append('/* nyan_art.h -- GENERATED, do not edit by hand.')
    out.append(' *')
    out.append(' * Produced by tools/braille2sprite.py from tools/cat.txt:')
    out.append(' *   Braille art -> dot bitmap -> region fill -> 2x downscale -> colour keys')
    out.append(' *')
    out.append(' * Each string is ONE HALF-ROW of pixels, not a text row: two consecutive')
    out.append(' * strings share a character cell and are drawn with a half block.  Keys:')
    out.append(' *')
    out.append(" *   ' '  transparent      'k'  outline        'p'  pop-tart filling")
    out.append(" *   'g'  fur              's'  sprinkle")
    out.append(' *')
    out.append(' * Regenerate with:')
    out.append(' *   python3 tools/braille2sprite.py --header nyan_art.h')
    out.append(' */')
    out.append('#ifndef KITTYOS_NYAN_ART_H')
    out.append('#define KITTYOS_NYAN_ART_H')
    out.append('')
    out.append('#define CAT_W          %d   /* pixels across, one per cell column */' % OW)
    out.append('#define CAT_HALF_ROWS  %d   /* pixels down, two per cell row      */' % OH)
    out.append('#define CAT_FRAMES     %d' % len(FRAMES))
    out.append('')
    for i, f in enumerate(FRAMES):
        out.append('static const char *const cat_frame%d[CAT_HALF_ROWS] = {' % i)
        for row in f:
            out.append('    "%s",' % ''.join(row))
        out.append('};')
        out.append('')
    out.append('static const char *const *const cat_frames[CAT_FRAMES] = {')
    out.append('    ' + ', '.join('cat_frame%d' % i for i in range(len(FRAMES))) + ',')
    out.append('};')
    out.append('')
    out.append('/* Vertical bob per frame, in half-rows. */')
    out.append('static const int cat_bob[CAT_FRAMES] = { %s };' %
               ', '.join(str(d) for d in FRAME_DY))
    out.append('')
    out.append('#endif /* KITTYOS_NYAN_ART_H */')
    open(path, 'w', newline='\n').write('\n'.join(out) + '\n')
    print('wrote %s (%d half-rows x %d px, %d frames)' % (path, OH, OW, len(FRAMES)))
