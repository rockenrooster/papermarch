# PAPERMARCH: composes a battle picture from the runtime files in assets/art at the game's projection, the way the
# engine should draw it (ground textures, haze, cast shadows, sorted sprites, gate panels, additive effects, post).
#   python -I -B tools/art/compose.py target captures/art/target_battle.png
#   python -I -B tools/art/compose.py preview captures/art/preview_w1.png
import sys, os, math, random
import numpy as np
from scipy import ndimage
from PIL import Image, ImageDraw, ImageFont, ImageEnhance

SCENE, OUTP = sys.argv[1], sys.argv[2]
ART = 'assets/art'
W, H = 1080, 2400
PITCH, CAMH, BACK = 30.0, 15.0, 8.5
SHADOW = (0.64, 0.79, 0.35)                 # dx, dz per unit of height; opacity
HAZE, HAZE0, HAZE1 = (0xBF, 0xE0, 0xF2), 60.0, 150.0
SKY_TOP, SKY_HOR = (0x4F, 0xA8, 0xF0), (0xBF, 0xE0, 0xF2)
FONT = 'assets/font/Roboto-Black.ttf'
rnd = random.Random(7)

class Cam:
    def __init__(s, zsquad):
        th = math.radians(PITCH)
        s.sn, s.cs, s.h = math.sin(th), math.cos(th), CAMH
        ds = s.h * s.sn + BACK * s.cs
        s.f = min(0.92 * W, 0.44 * H) * ds / 10.0
        s.cx, s.cy = W / 2, 0.8 * H - s.f * (s.h * s.cs - BACK * s.sn) / ds
        s.z = zsquad - BACK
        s.horizon = s.cy - s.f * math.tan(th)
    def project(s, x, zw, y=0.0):
        t = zw - s.z
        depth = (s.h - y) * s.sn + t * s.cs
        if depth < 0.5: return None
        return s.cx + x * s.f / depth, s.cy - ((y - s.h) * s.cs + t * s.sn) * s.f / depth, s.f / depth
cam = Cam(0.0)

# ---- the manifest ------------------------------------------------------------------------------------------------------
atlases, sprites, anims, points = {}, {}, {}, {}
for line in open(os.path.join(ART, 'sprites.txt')):
    p = line.split('#')[0].split()
    if not p: continue
    if p[0] == 'atlas': atlases[p[1]] = Image.open(os.path.join(ART, p[2])).convert('RGBA')
    elif p[0] == 'anim': anims[p[1]] = (int(p[2]), p[3], p[4])
    elif p[0] == 'sprite':
        ka, elev, frame = p[1].rsplit('.', 2)
        x, y, w, h = map(int, p[3:7])
        sprites.setdefault(ka, {}).setdefault(int(elev), {})[int(frame)] = dict(img=atlases[p[2]].crop((x, y, x + w, y + h)), w=w, h=h, px=float(p[7]), py=float(p[8]), height=float(p[9]))
    elif p[0] == 'point':
        points[(p[1], p[2])] = (float(p[3]), float(p[4]))

def pick(ka, zw, frame):
    ev = math.degrees(math.atan2(CAMH, max(zw - cam.z, 0.1)))
    elevs = sprites[ka]
    e = min(elevs, key=lambda k: abs(k - ev))
    return elevs[e][frame % len(elevs[e])], e

def comp(dst, im, x, y):
    x, y = int(round(x)), int(round(y))
    x0, y0, x1, y1 = max(x, 0), max(y, 0), min(x + im.width, dst.width), min(y + im.height, dst.height)
    if x1 <= x0 or y1 <= y0: return
    dst.alpha_composite(im.crop((x0 - x, y0 - y, x1 - x, y1 - y)), (x0, y0))

canvas = None
addl = np.zeros((H, W, 3), np.float32)

def draw(ka, x, zw, frame=0, scale=1.0, tint=1.0, y=0.0, alpha=1.0):
    pr = cam.project(x, zw, y)
    if not pr: return None
    sx, sy, s = pr
    rec, e = pick(ka, zw, frame)
    k = s * scale * rec['height'] / rec['h']
    w, h = int(round(rec['w'] * k)), int(round(rec['h'] * k))
    if w < 1 or h < 1 or sx + w < 0 or sx - w > W or sy - h > H or sy + h < 0: return None
    im = rec['img'].resize((w, h), Image.LANCZOS)
    ox, oy = sx - rec['px'] * k, sy - rec['py'] * k
    if anims.get(ka, (0, '', 'alpha'))[2] == 'add':
        a = np.asarray(im, np.float32) / 255.0
        x0, y0 = int(round(ox)), int(round(oy))
        xa, ya, xb, yb = max(x0, 0), max(y0, 0), min(x0 + w, W), min(y0 + h, H)
        if xb > xa and yb > ya:
            sub = a[ya - y0:yb - y0, xa - x0:xb - x0]
            addl[ya:yb, xa:xb] += sub[..., :3] * sub[..., 3:] * alpha
    else:
        if tint != 1.0: im = ImageEnhance.Brightness(im).enhance(tint)
        if alpha != 1.0: im.putalpha(im.getchannel('A').point(lambda v: int(v * alpha)))
        comp(canvas, im, ox, oy)
    return sx, sy, s, k, rec, e

# ---- the ground --------------------------------------------------------------------------------------------------------
def pyramid(path):
    im = Image.open(os.path.join(ART, path)).convert('RGB')
    out = [np.asarray(im, np.float32) / 255.0]
    while im.width > 4:
        im = im.reduce(2); out.append(np.asarray(im, np.float32) / 255.0)
    return out

def ground(casters):
    ys, xs = np.mgrid[0:H, 0:W].astype(np.float32) + 0.5
    k = (ys - cam.cy) / cam.f
    den = k * cam.cs + cam.sn
    ok = den > 0.004
    t = np.where(ok, cam.h * (cam.cs - k * cam.sn) / np.maximum(den, 0.004), 400.0)
    depth = cam.h * cam.sn + t * cam.cs
    x = (xs - cam.cx) * depth / cam.f
    zw = cam.z + t
    out = np.zeros((H, W, 3), np.float32)
    road, land = pyramid('ground_w1_road.png'), pyramid('ground_w1_land.png')
    dtdy = np.abs(np.gradient(t, axis=0))
    for tex, width, tile, mask, u in ((road, 12.0, 12.0, np.abs(x) <= 6.0, (x + 6.0) / 12.0), (land, 16.0, 16.0, np.abs(x) > 6.0, (x / 16.0) % 1.0)):
        n = tex[0].shape[0]
        fp = np.sqrt(np.maximum(dtdy * n / tile, 1e-3) * np.maximum(depth / cam.f * n / width, 1e-3))   # texels per pixel
        lvl = np.clip(np.round(np.log2(np.maximum(fp, 1.0))), 0, len(tex) - 1).astype(int)
        v = (zw / tile) % 1.0
        for L in range(len(tex)):
            m = mask & ok & (lvl == L)
            if not m.any(): continue
            nl = tex[L].shape[0]
            out[m] = tex[L][np.minimum((v[m] * nl).astype(int), nl - 1), np.minimum((np.clip(u[m], 0, 0.9999) * nl).astype(int), nl - 1)]
    # cast shadows: one union layer
    sh = Image.new('L', (W, H), 0)
    d = ImageDraw.Draw(sh)
    L2 = math.hypot(SHADOW[0], SHADOW[1])
    ux, uz = SHADOW[0] / L2, SHADOW[1] / L2
    for cx, cz, hgt, rad in casters:
        a = hgt * L2 * 0.5 + rad
        ox, oz = cx + ux * (a - rad), cz + uz * (a - rad)
        pts = []
        for i in range(14):
            c, s_ = math.cos(i * math.tau / 14) * a, math.sin(i * math.tau / 14) * rad
            pr = cam.project(ox + ux * c - uz * s_, oz + uz * c + ux * s_)
            if pr: pts.append((pr[0], pr[1]))
        if len(pts) > 2: d.polygon(pts, fill=255)
    m = ndimage.gaussian_filter(np.asarray(sh, np.float32) / 255.0, 2.5)[..., None] * SHADOW[2]
    out = out * (1 - m) + out * np.array([0.25, 0.32, 0.6], np.float32) * m
    # haze, sky, backdrop
    hz = np.clip((t - HAZE0) / (HAZE1 - HAZE0), 0, 1); hz = (hz * hz * (3 - 2 * hz))[..., None]
    out = out * (1 - hz) + np.array(HAZE, np.float32) / 255.0 * hz
    g = np.clip(ys / max(cam.horizon, 1.0), 0, 1)[..., None]
    sky = np.array(SKY_TOP, np.float32) / 255.0 * (1 - g) + np.array(SKY_HOR, np.float32) / 255.0 * g
    hy = int(round(cam.horizon))
    bd = Image.open(os.path.join(ART, 'backdrop_w1.png')).convert('RGBA')
    bd = bd.resize((W, int(W * 0.25)), Image.LANCZOS)
    skyim = Image.fromarray((sky * 255).astype(np.uint8), 'RGB').convert('RGBA')
    comp(skyim, bd, 0, hy - bd.height + 2)
    skyf = np.asarray(skyim.convert('RGB'), np.float32) / 255.0
    skyf = skyf * 0.65 + sky * 0.35                      # the backdrop sits 35% into the haze
    out[:max(hy, 0)] = skyf[:max(hy, 0)]
    return Image.fromarray((np.clip(out, 0, 1) * 255 + 0.5).astype(np.uint8), 'RGB').convert('RGBA')

# ---- gates (what the panel shader should draw) -------------------------------------------------------------------------
GATE = {'add': ((0x2E, 0x8B, 0xFF), (0xBF, 0xE4, 0xFF)), 'mul': ((0x9B, 0x4D, 0xFF), (0xE6, 0xCC, 0xFF)), 'neg': ((0xF0, 0x32, 0x3C), (0xFF, 0xC2, 0xB8))}
def gate(kind, x0, x1, zw, text, tsec=0.7):
    a, b = cam.project(x0, zw, 0.25), cam.project(x1, zw, 2.2)
    if not a or not b: return
    px0, py1, px1, py0 = int(a[0]), int(a[1]), int(b[0]), int(b[1])
    w, h = px1 - px0, py1 - py0
    if w < 4 or h < 4: return
    C, Hc = (np.array(c, np.float32) / 255.0 for c in GATE[kind])
    v, u = np.mgrid[0:h, 0:w].astype(np.float32)
    u, v = (u + 0.5) / w, 1 - (v + 0.5) / h
    wu, hu = x1 - x0, 1.95
    rgb = (C * 0.75)[None, None, :] * (1 - v[..., None]) + C[None, None, :] * v[..., None]
    al = 0.62 + 0.24 * v
    if kind == 'mul':
        pat = ((u * wu + v * hu) / 0.4 + tsec * 0.5 / 0.4) % 1.0 < 0.5
    else:
        sgn = 1.0 if kind == 'add' else -1.0
        pat = ((v * hu * sgn - np.abs(u - 0.5) * wu * 0.8) / 0.45 - tsec * 0.6 / 0.45) % 1.0 < 0.5
    rgb = np.where(pat[..., None], np.minimum(rgb * 1.25, 1.0), rgb); al = al + 0.12 * pat
    if kind == 'neg':
        dk = np.clip(1 - v / 0.25, 0, 1)[..., None]
        rgb = rgb * (1 - 0.5 * dk)
    sheen = np.exp(-(((u - 0.35 - 0.36 * (v - 0.5) * hu / wu) / 0.09) ** 2))[..., None]
    rgb = np.minimum(rgb + 0.25 * sheen, 1.0)
    edge = (np.minimum(v, 1 - v) * hu < 0.06)[..., None]
    rgb = np.where(edge, Hc[None, None, :], rgb); al = np.where(edge[..., 0], 0.95, al)
    inner = (np.abs(np.minimum(v, 1 - v) * hu - 0.1) < 0.012)[..., None]
    rgb = np.where(inner, rgb * 0.5 + 0.5, rgb)
    im = Image.fromarray((np.dstack([rgb, np.clip(al, 0, 1)]) * 255).astype(np.uint8), 'RGBA')
    d = ImageDraw.Draw(im)
    size = int(h * 1.25 / 1.95 * 0.92)
    font = ImageFont.truetype(FONT, size)
    while size > 6 and d.textlength(text, font=font) > 0.8 * w:
        size -= 2; font = ImageFont.truetype(FONT, size)
    dark = tuple(int(c * 255 * 0.35) for c in C)
    d.text((w / 2, h / 2), text, font=font, fill=(255, 255, 255, 255), anchor='mm', stroke_width=max(2, size // 12), stroke_fill=dark + (255,))
    comp(canvas, im, px0, py0)
    # the top edge glows a little (the bloom takes it from there)
    addl[max(py0, 0):max(py0 + max(2, h // 30), 0), max(px0, 0):min(px1, W)] += Hc * 0.35
    if kind == 'neg':
        n = int(round(x1 - x0))
        for i in range(n):
            draw('gate.spikes.still', x0 + (i + 0.5) * (x1 - x0) / n, zw - 0.02)
    for x in (x0, x1):
        draw('gate.post_%s.still' % kind, x, zw - 0.03)

# ---- the scenes --------------------------------------------------------------------------------------------------------
items, casters, adds, late = [], [], [], []       # items: (z, fn)
def unit(ka, x, z, hgt, rad, scale=None, **kw):
    scale = scale or rnd.uniform(0.94, 1.06)
    items.append((z, lambda: draw(ka, x, z, rnd.randrange(8), scale, rnd.uniform(0.92, 1.06), **kw)))
    casters.append((x, z, hgt * scale, rad * scale))

def squad(n, cx, cz):
    R = min(4.4, 0.55 + 0.30 * math.sqrt(n))
    shown = min(n, 150)
    out = []
    for i in range(shown):
        r, a = R * math.sqrt((i + 0.5) / shown), i * 2.39996
        x, z = max(-4.7, min(4.7, cx + r * math.cos(a))), cz + r * math.sin(a) * 0.9
        unit('soldier.march', x, z, 1.1, 0.24)
        out.append((x, z))
    return out, R

def horde(ka, n, x0, x1, z0, z1, hgt, rad, gap):
    cols = max(1, int((x1 - x0) / gap))
    for i in range(n):
        r, c = divmod(i, cols)
        z = z0 + r * gap * 1.05 + rnd.uniform(-0.2, 0.2)
        if z > z1: break
        unit(ka, x0 + (c + 0.5 + 0.5 * (r % 2)) * (x1 - x0) / (cols + 0.5) + rnd.uniform(-0.18, 0.18), z, hgt, rad)

def props(zlo, zhi):
    spec = [('w1.tree_a', 0.8, 0.85, 1.25, 8, 16, 5), ('w1.tree_b', 0.6, 0.8, 1.2, 9, 20, 7), ('w1.tree_c', 0.5, 0.8, 1.1, 7.5, 14, 11),
            ('w1.bush', 0.7, 0.8, 1.3, 6.8, 12, 4), ('w1.rock', 0.4, 0.7, 1.2, 6.8, 15, 9), ('w1.flowers', 0.8, 0.8, 1.4, 6.5, 11, 2.5),
            ('w1.haystack', 0.35, 0.9, 1.1, 9, 15, 17), ('w1.cottage', 0.5, 1.0, 1.15, 13, 20, 37), ('w1.windmill', 0.5, 1.0, 1.1, 15, 22, 61)]
    sizes = {'w1.tree_a': (3.8, 1.2), 'w1.tree_b': (4.6, 0.8), 'w1.tree_c': (3.6, 1.1), 'w1.bush': (0.9, 0.6), 'w1.rock': (0.8, 0.7), 'w1.flowers': (0.3, 0.3),
             'w1.haystack': (1.4, 0.9), 'w1.cottage': (2.4, 1.5), 'w1.windmill': (4.6, 1.1)}
    pr = random.Random(3)
    for kind, wgt, s0, s1, xa, xb, sp in spec:
        for side in (-1, 1):
            z = zlo
            while z < zhi:
                zz, x, sc, keep = z + pr.uniform(-sp / 2, sp / 2), side * pr.uniform(xa, xb), pr.uniform(s0, s1), pr.random() < wgt
                z += sp
                if not keep: continue
                items.append((zz, lambda kind=kind, x=x, zz=zz, sc=sc: draw(kind + '.still', x, zz, 0, sc)))
                casters.append((x, zz, sizes[kind][0] * sc, sizes[kind][1] * sc))
    for side in (-1, 1):
        z = math.ceil(zlo / 3) * 3
        while z < zhi:
            items.append((z, lambda x=side * 5.5, z=z: draw('w1.post.still', x, z)))
            casters.append((side * 5.5, z, 1.0, 0.2))
            if z % 12 == 0:
                items.append((z + 0.01, lambda x=side * 6.6, z=z: draw('w1.banner.still', x, z)))
                casters.append((side * 6.6, z, 3.4, 0.25))
            z += 3

def fx(ka, x, z, frame, scale=1.0, y=0.0, alpha=1.0, front=True):
    (late if front else items).append((z, lambda: draw(ka + '.play', x, z, frame, scale, y=y, alpha=alpha)))

def barrel(x, z):
    items.append((z, lambda: draw('barrel.still', x, z)))
    casters.append((x, z, 1.3, 0.5))

count = 0
if SCENE == 'target':
    count = 148
    men, R = squad(count, -0.7, 0.0)
    props(-8, 150)
    items.append((13.0, lambda: gate('add', -5, 0, 13.0, '+45')))
    items.append((13.0, lambda: gate('mul', 0, 5, 13.0, '×2')))
    items.append((43.0, lambda: gate('neg', -5, 0, 43.0, '-69')))
    items.append((43.0, lambda: gate('neg', 0, 5, 43.0, '-25')))
    barrel(3.5, 7.2)
    horde('rider.march', 22, -4.4, 4.4, 16.0, 19.5, 1.6, 0.5, 1.5)
    horde('foot.march', 520, -4.7, 4.7, 21.0, 37.0, 1.1, 0.25, 0.62)
    for i in range(12):
        unit('brute.march', rnd.uniform(-4.2, 4.2), rnd.uniform(23, 36), 1.8, 0.7)
    unit('boss.march', 0.4, 52.0, 5.8, 2.0, scale=1.0)
    horde('foot.march', 160, -4.6, 4.6, 56.0, 64.0, 1.1, 0.25, 0.62)
    # the fight: tracers, muzzle flashes, hits, a barrel going up
    for i in range(46):
        fx('fx.tracer', -0.7 + rnd.uniform(-R, R), rnd.uniform(2.5, 21.0), 0, y=0.55)
    front = sorted(men, key=lambda m: -m[1])[:40]
    for x, z in rnd.sample(front, 12):
        fx('fx.muzzle', x + 0.2, z + 0.62, rnd.randrange(3), y=0.52)
    for i in range(10):
        fx('fx.hit', rnd.uniform(-4, 4), rnd.uniform(15.5, 21.5), rnd.randrange(4), 1.2, y=0.6)
    for i in range(7):
        fx('fx.poof_red', rnd.uniform(-4, 4), rnd.uniform(15.5, 21.0), rnd.randrange(1, 6))
    for i in range(3):
        fx('fx.hit', rnd.uniform(-2.2, -0.2), 12.95, i, 1.3, y=rnd.uniform(0.6, 1.8))
    fx('fx.ring', -3.4, 8.6, 3, 1.0, front=False)
    fx('fx.explosion_glow', -3.4, 8.6, 3, y=0.8)
    fx('fx.explosion', -3.4, 8.6, 4, y=0.8)
    for i in range(6):
        fx('fx.debris', -3.4 + rnd.uniform(-2.2, 2.2), 8.6, rnd.randrange(8), 1.3, y=rnd.uniform(0.8, 3.6))
    for i in range(5):
        fx('fx.sparkle', -3.4 + rnd.uniform(-1.5, 1.5), 8.2, rnd.randrange(1, 5), y=rnd.uniform(0.3, 1.6))
    for i in range(10):
        fx('fx.dust', -0.7 + rnd.uniform(-R, R), -R * 0.8 + rnd.uniform(-0.5, 0.3), rnd.randrange(5), 1.2, front=False)
else:
    count = 26
    men, R = squad(count, 1.2, 0.0)
    props(-8, 150)
    for i in range(7):
        items.append((9.0 + i * 2.6, lambda i=i: gate('add', 2.2, 5.0, 9.0 + i * 2.6, '+%d' % (3 if i < 3 else (2 if i < 5 else 1)))))
    items.append((33.0, lambda: gate('add', -5, 0, 33.0, '+8')))
    items.append((33.0, lambda: gate('mul', 0, 5, 33.0, '×2')))
    barrel(-3.0, 15.0); barrel(-1.2, 19.0)
    horde('foot.march', 90, -4.0, 4.0, 44.0, 50.0, 1.1, 0.25, 0.62)
    for i in range(8):
        fx('fx.tracer', 1.2 + rnd.uniform(-R, R), rnd.uniform(2.5, 14.0), 0, y=0.55)
    for x, z in rnd.sample(sorted(men, key=lambda m: -m[1])[:10], 4):
        fx('fx.muzzle', x + 0.2, z + 0.62, rnd.randrange(3), y=0.52)

canvas = ground(casters)
for z, fn in sorted(items, key=lambda it: -it[0]):
    fn()
for z, fn in sorted(late, key=lambda it: -it[0]):
    fn()
for i in range(14):                                     # ambient leaves and petals, in screen space
    ka = 'w1.leaf.play' if i % 3 else 'w1.petal.play'
    rec = sprites[ka][0][rnd.randrange(8)]
    k = rnd.uniform(0.6, 1.4)
    comp(canvas, rec['img'].resize((int(rec['w'] * k), int(rec['h'] * k)), Image.LANCZOS), rnd.uniform(0, W), rnd.uniform(250, H))

# ---- post: additive layer, bloom, grade, vignette ----------------------------------------------------------------------
img = np.asarray(canvas.convert('RGB'), np.float32) / 255.0 + addl
luma = img @ np.array([0.299, 0.587, 0.114], np.float32)
br = img * np.clip((luma - 0.82) / 0.2, 0, 1)[..., None]
bloom = sum(np.dstack([ndimage.gaussian_filter(br[..., c], s) for c in range(3)]) for s in (5, 14, 34)) / 3.0
img = np.minimum(img, 1.0) + 0.55 * bloom
img = img * (1 - 0.06 + 0.06 * np.array([1.0, 0.96, 0.86], np.float32))     # light 0xFFF5DB at 0.06
img = (img - 0.5) * 1.06 + 0.5
g = (img @ np.array([0.299, 0.587, 0.114], np.float32))[..., None]
img = g + (img - g) * 1.12
ys, xs = np.mgrid[0:H, 0:W].astype(np.float32)
r = np.hypot((xs - W / 2) / (W / 2), (ys - H / 2) / (H / 2) * 1.0) / 1.414
vg = np.clip((r - 0.55) / 0.5, 0, 1); vg = vg * vg * (3 - 2 * vg)
img *= (1 - 0.16 * vg)[..., None]
final = Image.fromarray((np.clip(img, 0, 1) * 255 + 0.5).astype(np.uint8), 'RGB').convert('RGBA')

# ---- a plain HUD so the picture reads as the game (the real HUD is stage 3) ---------------------------------------------
d = ImageDraw.Draw(final)
ink, cream = (58, 42, 30, 255), (255, 246, 224, 255)
def chip(box, text, size, fill=cream, fg=ink, r=36):
    d.rounded_rectangle((box[0], box[1] + 8, box[2], box[3] + 8), r, fill=(40, 30, 20, 120))
    d.rounded_rectangle(box, r, fill=fill, outline=ink, width=4)
    d.text(((box[0] + box[2]) / 2, (box[1] + box[3]) / 2), text, font=ImageFont.truetype(FONT, size), fill=fg, anchor='mm')
chip((40, 70, 330, 180), 'LEVEL 12', 46)
chip((930, 70, 1040, 180), 'II', 50, r=55)
d.rounded_rectangle((60, 232, 1020, 262), 15, fill=(60, 45, 35, 200))
d.rounded_rectangle((60, 232, 500, 262), 15, fill=(47, 107, 255, 255))
d.ellipse((478, 222, 522, 272), fill=(255, 255, 255, 255), outline=(29, 73, 200, 255), width=6)
d.ellipse((990, 220, 1044, 274), fill=(227, 38, 46, 255), outline=ink, width=4)
sq = cam.project(men[0][0] - 0.0, 0.0, 0.0)
bx, by = sq[0], sq[1] - 2.9 * sq[2]
chip((bx - 110, by - 50, bx + 110, by + 50), str(count), 64, fill=(47, 107, 255, 255), fg=(255, 255, 255, 255), r=40)
final.convert('RGB').save(OUTP)
print('wrote', OUTP)
