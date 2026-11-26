# PAPERMARCH: every world's ground textures and horizon backdrop, and assets/art/worlds.txt (from tools/art/worlds.py).
#   python -I -B tools/art/make_ground.py assets/art [world numbers]
# ground_wN_road.png: 1024x1024, u across 12 units (1 curb + 10 road + 1 curb), v along the road, 12 units a repeat (wraps in v).
# ground_wN_land.png: 512x512, 16 units a repeat, wraps both ways.   backdrop_wN.png: 1024x256 RGBA, tiles sideways.
# Rows run up the road: row 0 is v = 0, z grows with the row. The sun is baked from -x, -z (ART_DIRECTION.md section 3).
import sys, os, math
import numpy as np
from scipy import ndimage
from PIL import Image
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from worlds import WORLDS, EDGES

OUT = sys.argv[1] if len(sys.argv) > 1 else 'assets/art'
ONLY = [int(a) for a in sys.argv[2:]] or sorted(WORLDS)
os.makedirs(OUT, exist_ok=True)

def rgb(h):
    return np.array([(h >> 16) & 255, (h >> 8) & 255, h & 255], float) / 255.0

def fbm(N, seed, base=4, octaves=5):
    rnd = np.random.RandomState(seed)
    out = np.zeros((N, N)); amp = 1.0; tot = 0.0
    for o in range(octaves):
        n = base * 2 ** o
        if n > N: break
        out += amp * ndimage.zoom(rnd.rand(n, n), N / n, order=3, mode='grid-wrap', grid_mode=True)
        tot += amp; amp *= 0.5
    return out / tot

def shade(height, k):
    """Relief from a height map, sun from -x and -row; wraps in rows."""
    dx = np.gradient(height, axis=1)
    dr = (np.roll(height, -1, axis=0) - np.roll(height, 1, axis=0)) * 0.5
    return 1.0 + k * (0.45 * dx + 0.55 * dr)

def save(arr, name):
    # 256 colours, dithered: a third of the bytes of a 24-bit PNG, and no visible banding on stone or grass
    im = Image.fromarray((np.clip(arr, 0, 1) * 255 + 0.5).astype(np.uint8), 'RGB')
    im.quantize(256, method=Image.MEDIANCUT, dither=Image.FLOYDSTEINBERG).save(os.path.join(OUT, name), optimize=True)
    print('wrote', name, os.path.getsize(os.path.join(OUT, name)) // 1024, 'KB')

def dabber(col, N):
    def dab(cx, cy, rx, ry, c, alpha):
        xi = np.arange(int(cx - rx) - 1, int(cx + rx) + 2); yi = np.arange(int(cy - ry) - 1, int(cy + ry) + 2)
        d = ((xi[None, :] + 0.5 - cx) / rx) ** 2 + ((yi[:, None] + 0.5 - cy) / ry) ** 2
        m = (np.clip(1.5 - d * 1.5, 0, 1) * alpha)[..., None]
        ix = np.ix_(yi % N, xi % N)
        col[ix] = col[ix] * (1 - m) + c * m
    return dab

#          rows, width min, max (units), corner radius, bevel, running bond
STYLES = {'flag': (11, 1.0, 2.3, 7.0, 5.0, False), 'sett': (20, 0.55, 0.95, 6.0, 4.0, False), 'brick': (24, 1.0, 1.0, 2.5, 3.0, True),
          'plank': (14, 3.5, 8.0, 2.0, 2.5, False), 'marble': (6, 2.0, 2.0, 1.5, 2.0, False)}

def road(n, w):
    style, A, B, joint_c, curb_c, ex = w['road']
    rows, wmin, wmax, radius, bevel, bond = STYLES[style]
    N = 1024
    rnd = np.random.RandomState(12 + n)
    ppu = N / 12.0
    x0, x1 = int(round(ppu)), int(round(N - ppu))
    col = np.zeros((N, N, 3)); height = np.zeros((N, N))
    ys, xs = np.mgrid[0:N, 0:N]
    def stone(cx, cy, hw, hh, r, c, top=1.0, bev=bevel):
        ax0, ax1 = max(0, int(cx - hw) - 1), min(N, int(cx + hw) + 2)
        rws = np.arange(int(cy - hh) - 1, int(cy + hh) + 2)
        X = xs[0, ax0:ax1][None, :] + 0.5; Y = rws[:, None] + 0.5
        qx, qy = np.abs(X - cx) - (hw - r), np.abs(Y - cy) - (hh - r)
        sd = np.hypot(np.maximum(qx, 0), np.maximum(qy, 0)) + np.minimum(np.maximum(qx, qy), 0) - r
        m = np.clip(-sd / bev, 0, 1); m = m * m * (3 - 2 * m)
        ri = rws % N
        height[ri, ax0:ax1] = np.maximum(height[ri, ax0:ax1], m * top)
        col[ri, ax0:ax1] = np.where((sd < 0)[..., None], c, col[ri, ax0:ax1])
    A, B = rgb(A), rgb(B)
    hs = rnd.uniform(0.8, 1.35, rows) if style == 'flag' else np.ones(rows) * rnd.uniform(0.97, 1.03, rows) ** (style == 'sett')
    hs = hs / hs.sum() * N
    y = 0.0
    for i, hrow in enumerate(hs):
        x = x0 - (0.5 * ppu * (i % 2) if bond else (0 if style == 'marble' else rnd.uniform(0, wmax) * ppu))
        while x < x1:
            wd = rnd.uniform(wmin, wmax) * ppu
            a, b = max(x, x0), min(x + wd, x1)
            if b - a > 0.3 * wmin * ppu:
                c = A + (B - A) * rnd.rand() + rnd.uniform(-0.035, 0.035) + np.array([0.012, 0, -0.012]) * rnd.uniform(-1, 1)
                if style == 'marble': c = A if (i + int(round((x - x0) / wd))) % 2 == 0 else B
                stone((a + b) / 2, y + hrow / 2, (b - a) / 2 - 1.6, hrow / 2 - 1.6, radius, c, top=rnd.uniform(0.9, 1.0))
            x += wd
        y += hrow
    C = rgb(curb_c)
    for side in (0, 1):
        cx = x0 / 2 if side == 0 else N - x0 / 2
        for i in range(6):
            stone(cx, (i + 0.5 + 0.5 * side) * N / 6, x0 / 2 - 1.5, N / 12 - 1.5, 6.0, C + rnd.uniform(-0.03, 0.02), top=2.2, bev=7.0)
    joint = height < 0.02
    u = np.abs(xs / N - 0.5) * 2
    jcol = np.broadcast_to(rgb(joint_c), col.shape).copy()
    if 'moss' in ex:
        moss = np.clip((fbm(N, 3, 6) - 0.45) * 5, 0, 1) * (1.0 if ex.get('moss_all') else np.clip((u - 0.45) * 2.5, 0, 1))
        jcol = jcol * (1 - moss[..., None]) + rgb(ex['moss']) * moss[..., None]
        if ex.get('moss_all'):     # moss creeping over the boards too
            m2 = (np.clip((fbm(N, 8, 5) - 0.55) * 4, 0, 1) * 0.55)[..., None]
            col = col * (1 - m2) + rgb(ex['moss']) * m2
    if not ex.get('joint_after'):
        col = np.where(joint[..., None], jcol, col)
    col *= (0.93 + 0.14 * fbm(N, 5, 8))[..., None]
    if style == 'plank':     # grain along the boards
        g = ndimage.zoom(np.random.RandomState(4).rand(256, 6), (N / 256, N / 6), order=1, mode='grid-wrap', grid_mode=True)
        col *= (0.94 + 0.12 * g)[..., None]
    if ex.get('veins'):
        v = np.abs(fbm(N, 9, 3, 4) - 0.5)
        col *= (1 - 0.12 * np.clip(1 - v / 0.012, 0, 1) * (~joint))[..., None]
    col *= (1.0 + 0.07 * np.exp(-(u / 0.33) ** 2) * (~joint))[..., None]
    col *= (1 + 0.03 * (np.random.RandomState(1).rand(N, N) - 0.5))[..., None]
    hb = ndimage.gaussian_filter(height, 1.2, mode=['wrap', 'nearest'])
    col *= np.clip(shade(hb, 2.6), 0.55, 1.35)[..., None]
    ao = ndimage.gaussian_filter(np.minimum(height, 1.0), 5, mode=['wrap', 'nearest'])
    col *= (0.72 + 0.28 * np.clip(ao * 1.25, 0, 1))[..., None]
    if ex.get('joint_after'):   # snow or lava in the joints: lit, not shaded
        k = np.clip((0.35 - height) / 0.35, 0, 1)
        if ex.get('lava'):
            heat = np.clip((fbm(N, 6, 5) - 0.35) * 3.5, 0, 1)
            jcol = rgb(0x2A2026) * (1 - heat[..., None]) + (rgb(joint_c) * 0.6 + rgb(0xFFD23A) * 0.4 * heat[..., None]) * heat[..., None]
            k = np.clip((0.12 - height) / 0.12, 0, 1)
            col += (rgb(joint_c) * 0.25) * (ndimage.gaussian_filter(k * heat, 6, mode=['wrap', 'nearest']))[..., None]   # the glow on the stone
        col = col * (1 - k[..., None]) + jcol * k[..., None]
    sh = np.clip(1 - (xs - x0) / (0.28 * ppu), 0, 1) * (xs >= x0)
    col *= (1 - 0.3 * sh)[..., None] * np.array([1, 1.01, 1.06])[None, None, :] ** sh[..., None]
    dab = dabber(col, N)
    for c, count, r in ex.get('dots', []):      # petals or confetti lying on the road
        for i in range(count):
            dab(rnd.uniform(x0, x1), rnd.uniform(0, N), r * rnd.uniform(0.7, 1.3), r * rnd.uniform(0.5, 1.0), rgb(c), 0.95)
    save(col, 'ground_w%d_road.png' % n)

def land(n, w):
    (c1, c2, c3), (td, tl, tufts), dots, ex = w['land']
    N = 512
    k = N / 1024.0
    rnd = np.random.RandomState(21 + n)
    G1, G2, G3 = rgb(c1), rgb(c2), rgb(c3)
    f1, f2 = fbm(N, 31 + n, 3, 4), fbm(N, 32 + n, 8, 4)
    col = G2 + (G1 - G2) * np.clip((f1 - 0.35) * 2.2, 0, 1)[..., None]
    col = col + (G3 - col) * np.clip((f2 - 0.55) * 3, 0, 1)[..., None] * 0.6
    if 'clover' in ex:
        col = col + (rgb(ex['clover']) - col) * np.clip((fbm(N, 33, 5, 3) - 0.62) * 8, 0, 1)[..., None] * 0.7
    if ex.get('ripples'):     # wind ripples in sand
        ys = np.mgrid[0:N, 0:N][0]
        col *= (1 + 0.05 * np.sin((ys + 60 * f1) * 2 * math.pi / (N / 20)))[..., None]
    if ex.get('relief'):      # drifts
        col *= np.clip(shade(ndimage.gaussian_filter(f2, 2, mode='wrap') * 40, 1.0), 0.8, 1.15)[..., None]
    dab = dabber(col, N)
    if 'puffs' in ex:         # cloud tops
        a, b, count = ex['puffs']
        for i in range(count):
            cx, cy, s = rnd.uniform(0, N), rnd.uniform(0, N), rnd.uniform(14, 34)
            dab(cx + 3, cy + 4, s, s * 0.7, rgb(b), 0.8); dab(cx, cy, s * 0.9, s * 0.62, rgb(a), 0.9)
    if 'pads' in ex:          # lily pads on dark water
        a, b, count = ex['pads']
        for i in range(count):
            cx, cy, s = rnd.uniform(0, N), rnd.uniform(0, N), rnd.uniform(4, 8)
            dab(cx + 1.5, cy + 2, s, s * 0.8, rgb(0x163C3A), 0.6); dab(cx, cy, s, s * 0.8, rgb(a), 1.0); dab(cx - 1, cy - 1, s * 0.5, s * 0.4, rgb(b), 0.8)
    for i in range(tufts):
        cx, cy, s = rnd.uniform(0, N), rnd.uniform(0, N), rnd.uniform(2.0, 4.5) * k * 1.3
        dab(cx + 0.8, cy + 1.0, s, s * 1.6, rgb(td), 0.5); dab(cx, cy, s * 0.7, s * 1.5, rgb(tl), 0.4)
    for c, count, r in dots:
        for i in range(count):
            cx, cy, s = rnd.uniform(0, N), rnd.uniform(0, N), r * k * 1.3 * rnd.uniform(0.8, 1.2)
            dab(cx + 1, cy + 1.3, s, s, col[int(cy) % N, int(cx) % N] * 0.6, 0.6); dab(cx, cy, s, s, rgb(c), 1.0)
    col *= (1 + 0.04 * (rnd.rand(N, N) - 0.5))[..., None]
    save(col, 'ground_w%d_land.png' % n)

def backdrop(n, w):
    ridges, feats = w['backdrop']
    W, H = 1024, 256
    out = np.zeros((H, W, 4))
    xs = np.arange(W) / W * 2 * math.pi
    X = np.arange(W)[None, :]
    ys = np.arange(H)[:, None]
    def over(mask, c, a=1.0):
        m = np.clip(mask, 0, 1) * a
        out[..., :3] = out[..., :3] * (1 - m[..., None]) + c * m[..., None]
        out[..., 3] = out[..., 3] * (1 - m) + m
    def ridge(base, amp, terms, seed):
        r = np.random.RandomState(seed + n * 7)
        y = np.zeros(W)
        for t in terms:
            y += r.uniform(0.4, 1.0) / t * np.sin(t * xs + r.uniform(0, 6.28))
        return base - amp * y
    r5 = np.random.RandomState(5 + n)
    cloud = rgb(0xFFFFFF) if n != 9 else rgb(0x8A6A70)
    for i in range(9 if n != 7 else 4):
        cx, cy, s = r5.uniform(0, W), r5.uniform(30, 120), r5.uniform(22, 44)
        for j in range(6):
            px, py, r = cx + r5.uniform(-1.6, 1.6) * s, cy + r5.uniform(-0.25, 0.25) * s, s * r5.uniform(0.5, 1.0)
            dx = (X - px + W / 2) % W - W / 2
            over((r - np.hypot(dx, (ys - py) * 1.7)) / 2.0 + 0.5, cloud, 0.95)
    for i, (base, amp, terms, seed) in enumerate(((150, 34, (1, 2, 3, 5), 1), (185, 26, (2, 3, 5, 8), 2), (218, 18, (3, 4, 7, 11), 3))):
        c = rgb(ridges[i])
        r = ridge(base, amp, terms, seed)
        if i == 0:
            for f in feats:
                if f[0] == 'tri':     # peaks, pyramids, sails, tents: (x, base y, height, colour, cap colour)
                    _, cx, by, hgt, fc = f[:5]
                    m = np.minimum((ys - (by - hgt)) * 0.75 - np.abs(X - cx) + 0.5, by + 40 - ys + 0.5)
                    over(m, rgb(fc))
                    over(np.minimum(m, (cx - X) * 0.6 + 0.5), rgb(fc) * 0.82)            # the shaded side
                    if len(f) > 5: over(np.minimum(m, (by - hgt * 0.62) - ys + np.sin(X * 0.3) * 4), rgb(f[5]))
                elif f[0] == 'band':
                    over((ys - f[1]) + 0.5, rgb(f[2]))
        over((ys - r[None, :]) / 1.5, c)
        over((ys - r[None, :] - 10) / 30.0, c * 0.9, 0.5)
        if i == 1:
            for f in feats:
                if f[0] == 'castle':
                    cx0 = f[1]
                    wall, roofc = (rgb(0xD8D2E4), rgb(0xD85A5A)) if f[2] == 1 else (rgb(0xC8A888), rgb(0x7A4A3A))
                    for dx, wd, h in ((-20, 16, 58), (20, 22, 80), (65, 16, 52), (0, 60, 34)):
                        cx = cx0 + dx
                        top = r[cx] - h
                        over(np.minimum(np.minimum(X - (cx - wd / 2), (cx + wd / 2) - X), np.minimum(ys - top, r[cx] + 8 - ys)) + 0.5, wall)
                        over(np.minimum(np.minimum(X - cx, (cx + wd / 2) - X), np.minimum(ys - top, r[cx] + 8 - ys)) + 0.5, wall * 0.86)
                        if h > 40:
                            over(np.minimum((ys - (top - wd * 1.2)) * 0.5 - np.abs(X - cx) + 0.5, top - ys + 0.5), roofc)
    out[..., :3] /= np.maximum(out[..., 3:], 1e-6)
    out[out[..., 3] < 1e-4, :3] = rgb(ridges[0])
    Image.fromarray((np.clip(out, 0, 1) * 255 + 0.5).astype(np.uint8), 'RGBA').save(os.path.join(OUT, 'backdrop_w%d.png' % n), optimize=True)

def worlds_txt():
    L = ['# PAPERMARCH worlds: how each world looks (docs/ART_DIRECTION.md). Written by tools/art/make_ground.py from tools/art/worlds.py.',
         '# world <n> <name>',
         '# road <file> <width> <tile>      u = (x + width/2) / width, v = z / tile; the image\'s first row is v = 0',
         '# land <file> <tile>              u = x / tile, v = z / tile',
         '# haze <0xRRGGBB> <start> <end>   distance fog, units ahead of the camera\'s foot (smoothstep)',
         '# sky <top> <horizon>',
         '# backdrop <file> <height>        bottom edge on the horizon, as wide as the screen; height = share of the screen\'s width',
         '# light <0xRRGGBB> <strength>     grade: colour *= mix(1, light, strength)',
         '# grade <contrast> <saturation> | bloom <threshold> <strength> | vignette <strength>',
         '# shadow <dx> <dz> <alpha>        cast shadows: ground offset per unit of height; opacity',
         '# boss <hueDegrees> <0xRRGGBB>    the shared ogre: rotate hue, then multiply by the tint',
         '# bossaddon <kind>                a sprite drawn over the boss at the same ground point, scale and frame',
         '# prop <kind> <weight> <scaleMin> <scaleMax> <xMin> <xMax> <spacing>',
         '# edge <kind> <spacing> <x>       both sides at |x|, every <spacing> units',
         '# ambient <kind> <perSecond>']
    for n in sorted(WORLDS):
        w = WORLDS[n]
        L += ['world %d %s' % (n, w['name']), 'road ground_w%d_road.png 12 12' % n, 'land ground_w%d_land.png 16' % n,
              'haze 0x%06X %g %g' % w['haze'], 'sky 0x%06X 0x%06X' % w['sky'], 'backdrop backdrop_w%d.png 0.25' % n,
              'light 0x%06X %g' % w['light'], 'grade %g %g' % w['grade'], 'bloom %g %g' % w['bloom'], 'vignette %g' % w['vignette'],
              'shadow 0.64 0.79 %g' % w['shadow'], 'boss %d 0x%06X' % w['boss'][:2]]
        if w['boss'][2]: L.append('bossaddon w%d.boss_addon' % n)
        L += ['prop w%d.%s %g %g %g %g %g %g' % ((n,) + p) for p in w['props']]
        L += ['edge w%d.%s %g %g' % ((n,) + e) for e in EDGES]
        L += ['ambient w%d.%s %g' % (n, a[0], a[3]) for a in w['ambient']]
    open(os.path.join(OUT, 'worlds.txt'), 'w', newline='\n').write('\n'.join(L) + '\n')
    print('wrote worlds.txt')

for n in ONLY:
    road(n, WORLDS[n]); land(n, WORLDS[n]); backdrop(n, WORLDS[n])
worlds_txt()
