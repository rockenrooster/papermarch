# PAPERMARCH: builds every 3D asset (units, barrel, gate parts, props) from primitives and renders the sprite frames.
#   blender -b --factory-startup --python-exit-code 1 -P tools/art/render_sprites.py -- --out captures/art/frames [--only soldier,foot]
# Writes <out>/<kind>.<anim>.<elev>.<frame>.png (2x supersampled) and <out>/<kind>.json (pivots, px per unit, points).
# Axes here: X = the game's x, Y = the game's z (up the road), Z = up. tools/art/pack_atlas.py turns the frames into atlases.
import bpy, bmesh, sys, os, json, math, random
from mathutils import Vector

SS = 2  # supersampling; pack_atlas.py scales down by the same factor
SUN = Vector((-0.45, -0.55, 0.70)).normalized()  # direction to the sun (docs/ART_DIRECTION.md section 3)
RIM = Vector((0.60, 0.72, 0.35)).normalized()
TAU = 2 * math.pi

args = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
def arg(name, default=None):
    return args[args.index(name) + 1] if name in args else default
OUT = os.path.abspath(arg('--out', 'captures/art/frames'))
ONLY = set(filter(None, (arg('--only', '') or '').split(',')))
os.makedirs(OUT, exist_ok=True)

# ---- materials ---------------------------------------------------------------------------------------------------------
_mats = {}
def M(h, rough=0.35, metal=0.0, coat=0.6, emit=0.0):
    key = (h, rough, metal, coat, emit)
    if key in _mats:
        return _mats[key]
    f = lambda c: c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4
    rgba = (f((h >> 16 & 255) / 255), f((h >> 8 & 255) / 255), f((h & 255) / 255), 1)
    m = bpy.data.materials.new('m%06x' % h)
    m.use_nodes = True
    b = next(n for n in m.node_tree.nodes if n.type == 'BSDF_PRINCIPLED')
    def put(name, v):
        if name in b.inputs:
            b.inputs[name].default_value = v
    put('Base Color', rgba); put('Roughness', rough); put('Metallic', metal)
    put('Coat Weight', coat); put('Coat Roughness', 0.15)
    if emit:
        put('Emission Color', rgba); put('Emission Strength', emit)
    _mats[key] = m
    return m

BLUE, BLUE_D, NAVY = 0x1F58F2, 0x1440C4, 0x16224F
WHITE, GOLD, CREAM, TAN = 0xFFFFFF, 0xFFC83A, 0xF2E9D0, 0xC9955A
SKIN, BLACK, WOOD, WOOD_D = 0xF2B98C, 0x22222A, 0x8A5A34, 0x5A3A22
RED, RED_D, OX = 0xE3262E, 0xA8141F, 0xA3131F
STEEL, STEEL_L, IVORY = 0x4A5060, 0x9AA3B4, 0xF3E6C4
BOAR, BOAR_D = 0x6B4630, 0x4A2E20
VIOLET, VIOLET_D = 0x9B4DFF, 0x6A28C8

# ---- mesh helpers ------------------------------------------------------------------------------------------------------
def link(o, parent=None, loc=(0, 0, 0), rot=(0, 0, 0), scale=(1, 1, 1)):
    bpy.context.scene.collection.objects.link(o)
    o.parent = parent
    o.location, o.rotation_euler, o.scale = loc, rot, scale if not isinstance(scale, (int, float)) else (scale,) * 3
    return o

def E(parent=None, loc=(0, 0, 0), rot=(0, 0, 0), scale=1):
    return link(bpy.data.objects.new('e', None), parent, loc, rot, scale)

def finish(bm, mat, smooth, **kw):
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    me = bpy.data.meshes.new('m')
    bm.to_mesh(me); bm.free()
    me.shade_smooth() if smooth else me.shade_flat()
    me.materials.append(mat if not isinstance(mat, int) else M(mat))
    return link(bpy.data.objects.new('o', me), **kw)

def lathe(profile, mat, seg=20, smooth=False, flat=False, **kw):
    """Revolve (radius, z) points around Z. smooth=False keeps the profile's corners hard."""
    bm = bmesh.new()
    def ring(r, z):
        if r <= 1e-6:
            return [bm.verts.new((0, 0, z))]
        return [bm.verts.new((r * math.cos(TAU * i / seg), r * math.sin(TAU * i / seg), z)) for i in range(seg)]
    prev = None
    for k in range(len(profile) - 1):
        a = prev if (smooth and prev) else ring(*profile[k])
        b = ring(*profile[k + 1])
        for i in range(seg):
            j = (i + 1) % seg
            if len(a) == 1 and len(b) == 1: continue
            if len(a) == 1: bm.faces.new((a[0], b[i], b[j]))
            elif len(b) == 1: bm.faces.new((a[i], a[j], b[0]))
            else: bm.faces.new((a[i], a[j], b[j], b[i]))
        prev = b
    return finish(bm, mat, not flat, **kw)

def ell(radii, mat, **kw):
    n = 10
    prof = [(math.sin(math.pi * i / n), -math.cos(math.pi * i / n)) for i in range(n + 1)]
    if isinstance(radii, (int, float)): radii = (radii,) * 3
    return lathe(prof, mat, smooth=True, scale=radii, **kw)

def box(dims, mat, bevel=0.02, **kw):
    bm = bmesh.new()
    bmesh.ops.create_cube(bm, size=1)
    for v in bm.verts:
        v.co = Vector((v.co.x * dims[0], v.co.y * dims[1], v.co.z * dims[2]))
    if bevel:
        bmesh.ops.bevel(bm, geom=bm.edges[:], offset=bevel, segments=2, affect='EDGES')
    return finish(bm, mat, False, **kw)

def rod(p0, p1, r0, r1, mat, parent=None, seg=12, cap=None):
    p0, p1 = Vector(p0), Vector(p1)
    d = p1 - p0
    prof = [(0, 0), (r0, 0), (r1, d.length)] + ([(0, d.length + cap)] if cap else [(0, d.length)])
    o = lathe(prof, mat, seg=seg, parent=parent, loc=p0)
    o.rotation_mode = 'QUATERNION'
    o.rotation_quaternion = d.to_track_quat('Z', 'Y')
    return o

def blob(r, mat, rough=0.18, seed=0, sub=3, smooth=True, **kw):
    rnd = random.Random(seed)
    bm = bmesh.new()
    bmesh.ops.create_icosphere(bm, subdivisions=sub, radius=1.0)
    ph = [rnd.uniform(0, TAU) for _ in range(6)]
    for v in bm.verts:
        x, y, z = v.co
        if smooth:   # lumpy, not spiky
            n = math.sin(2.3 * x + ph[0]) + math.sin(2.9 * y + ph[1]) + math.sin(2.6 * z + ph[2]) + math.sin(5.1 * x + ph[3]) * math.sin(4.7 * z + ph[4])
            v.co *= 1 + rough * n * 0.6
        else:
            v.co *= 1 + rnd.uniform(-rough, rough)
    kw.setdefault('scale', r)
    return finish(bm, mat, smooth, **kw)

def poly(pts, faces, mat, **kw):
    bm = bmesh.new()
    vs = [bm.verts.new(p) for p in pts]
    for f in faces:
        bm.faces.new([vs[i] for i in f])
    return finish(bm, mat, False, **kw)

def star(ro, ri, depth, mat, n=5, **kw):
    """A star in the XZ plane, extruded along Y."""
    pts = []
    for y in (-depth / 2, depth / 2):
        for i in range(2 * n):
            r = ro if i % 2 == 0 else ri
            a = math.pi / 2 + math.pi * i / n
            pts.append((r * math.cos(a), y, r * math.sin(a)))
    m = 2 * n
    faces = [list(range(m)), list(range(m, 2 * m))] + [(i, (i + 1) % m, m + (i + 1) % m, m + i) for i in range(m)]
    return poly(pts, faces, mat, **kw)

def pennant(w, h, mat, swallow=False, **kw):
    """A flag in the XZ plane, hoist on the Z axis, flying toward +X."""
    pts = [(0, 0, 0), (0, 0, h), (w, 0, h * 0.5)]
    faces = [(0, 1, 2)]
    if swallow:
        pts = [(0, 0, 0), (0, 0, h), (w, 0, h), (w * 0.72, 0, h * 0.5), (w, 0, 0)]
        faces = [(0, 1, 2, 3, 4)]
    o = poly(pts, faces, mat, **kw)
    sol = o.modifiers.new('s', 'SOLIDIFY'); sol.thickness = 0.02
    return o

# ---- scene -------------------------------------------------------------------------------------------------------------
scene = bpy.context.scene
def reset():
    for o in list(bpy.data.objects):
        bpy.data.objects.remove(o, do_unlink=True)
    r = scene.render
    r.engine = 'CYCLES'
    scene.cycles.device = 'CPU'
    r.threads_mode = 'FIXED'; r.threads = 4   # launch from a shell that dot-sourced scripts/env.ps1 (pins everything to the same 4 cores)
    scene.cycles.samples = 40
    scene.cycles.use_denoising = False
    scene.cycles.max_bounces = 3
    scene.cycles.filter_width = 1.5
    r.film_transparent = True
    r.image_settings.file_format = 'PNG'; r.image_settings.color_mode = 'RGBA'
    scene.view_settings.view_transform = 'Standard'
    scene.view_settings.look = 'None'
    w = scene.world or bpy.data.worlds.new('w')
    scene.world = w
    w.use_nodes = True
    bg = next(n for n in w.node_tree.nodes if n.type == 'BACKGROUND')
    bg.inputs[0].default_value = (0.52, 0.69, 1.0, 1); bg.inputs[1].default_value = 0.9
    def sun(direction, colour, strength, angle):
        d = bpy.data.lights.new('sun', 'SUN'); d.energy = strength; d.color = colour; d.angle = math.radians(angle)
        o = link(bpy.data.objects.new('sun', d))
        o.rotation_mode = 'QUATERNION'; o.rotation_quaternion = (-direction).to_track_quat('-Z', 'Y')
    sun(SUN, (1.0, 0.94, 0.82), 3.0, 6)
    sun(RIM, (1.0, 1.0, 1.0), 1.2, 10)
    # The warm ground bounces light up into the undersides; the camera does not see it.
    g = box((60, 60, 0.1), M(0xD9C7A6, 0.9, 0, 0), bevel=0, loc=(0, 0, -0.13))
    g.visible_camera = False
    cd = bpy.data.cameras.new('cam'); cd.type = 'ORTHO'; cd.clip_end = 200
    scene.camera = link(bpy.data.objects.new('cam', cd))

def render(kind, anim, frames, elevs, ppu, w, h, cz, pose=None, points=None, yc=0.0):
    """Render `frames` poses at each elevation. The view is w x h road units, centred on (0, yc, cz)."""
    s = ppu * SS
    W, H = int(round(w * s)), int(round(h * s))
    scene.render.resolution_x, scene.render.resolution_y, scene.render.resolution_percentage = W, H, 100
    cam = scene.camera
    cam.data.ortho_scale = max(w, h)
    meta = {'ppu': s, 'ss': SS, 'frames': frames, 'elevs': {}}
    for e in elevs:
        a = math.radians(e)
        cam.location = Vector((0, yc, cz)) + 60 * Vector((0, -math.cos(a), math.sin(a)))
        cam.rotation_euler = (math.pi / 2 - a, 0, 0)
        def px(p):
            return [W / 2 + s * p[0], H / 2 - s * ((p[1] - yc) * math.sin(a) + (p[2] - cz) * math.cos(a))]
        em = {'pivot': px((0, 0, 0)), 'points': {}}
        for f in range(frames):
            if pose: pose(f / frames)
            bpy.context.view_layer.update()
            if f == 0 and points:
                for name, o in points.items():
                    em['points'][name] = px(o.matrix_world.translation)
            scene.render.filepath = os.path.join(OUT, '%s.%s.%d.%d.png' % (kind, anim, e, f))
            bpy.ops.render.render(write_still=True)
        meta['elevs'][e] = em
    with open(os.path.join(OUT, '%s.%s.json' % (kind, anim)), 'w') as fh:
        json.dump(meta, fh)
    print('rendered', kind, anim, flush=True)

def want(kind, anim):
    """--only takes kinds (soldier), their first part (w3, ui) or kind:anim (boss:slam)."""
    return not ONLY or kind in ONLY or kind.split('.')[0] in ONLY or '%s:%s' % (kind, anim) in ONLY or '%s:%s' % (kind.split('.')[0], anim) in ONLY

# ---- units -------------------------------------------------------------------------------------------------------------
def leg(parent, x, trouser, boot, r=0.08, length=0.3):
    p = E(parent, (x, 0, 0))
    lathe([(0, 0.03), (r, 0), (r * 0.9, -length * 0.7)], trouser, parent=p, seg=14)
    ell((r * 1.05, r * 1.5, r * 0.9), boot, parent=p, loc=(0, r * 0.45, -length * 0.82))
    return p

def weapon_parts(kind, gun, body):
    """The gun in the soldier's hands (CONTENT_PASS 1). `gun` is the right hand's frame, +Y up the road. Returns its points."""
    if kind == 'musket':
        rod((0, -0.22, -0.03), (0, 0.2, 0), 0.05, 0.035, M(WOOD), parent=gun)
        rod((0, 0.1, 0.01), (0, 0.62, 0.01), 0.026, 0.022, M(STEEL_L, 0.25, 0.8), parent=gun)
        rod((0, 0.62, 0.01), (0, 0.8, 0.01), 0.014, 0.002, M(WHITE, 0.2, 0.9), parent=gun)
        return {'muzzle': E(gun, (0, 0.64, 0.01))}
    if kind == 'twin':          # a short cyan-steel rifle in each hand
        pts = {}
        for name, g in (('muzzle', gun), ('muzzle2', E(body, (-0.23, 0.05, 0.2), rot=(math.radians(8), 0, 0)))):
            rod((0, -0.18, -0.03), (0, 0.12, 0), 0.048, 0.036, M(WOOD_D), parent=g)
            rod((0, 0.04, 0.01), (0, 0.5, 0.01), 0.032, 0.028, M(0x5FCFF0, 0.25, 0.6), parent=g)
            rod((0, 0.4, 0.01), (0, 0.46, 0.01), 0.042, 0.042, M(GOLD, 0.25, 0.4), parent=g)
            pts[name] = E(g, (0, 0.52, 0.01))
        return pts
    if kind == 'scatter':       # a brass blunderbuss with a bell mouth
        rod((0, -0.2, -0.03), (0, 0.08, 0), 0.058, 0.046, M(WOOD), parent=gun)
        rod((0, 0.02, 0.01), (0, 0.32, 0.01), 0.046, 0.046, M(0xD9A441, 0.25, 0.6), parent=gun)
        rod((0, 0.32, 0.01), (0, 0.44, 0.01), 0.046, 0.085, M(0xD9A441, 0.25, 0.6), parent=gun)
        rod((0, 0.435, 0.01), (0, 0.45, 0.01), 0.07, 0.07, M(BLACK), parent=gun)
        return {'muzzle': E(gun, (0, 0.5, 0.01))}
    if kind == 'lance':         # a long white tube with blue rings and a glowing tip
        rod((0, -0.32, 0), (0, 0.86, 0), 0.032, 0.03, M(0xDCE6F4, 0.25, 0.5), parent=gun)
        for y in (0.0, 0.35, 0.7):
            rod((0, y, 0), (0, y + 0.07, 0), 0.048, 0.048, M(BLUE, 0.3, 0.3), parent=gun)
        ell(0.06, GL(0x6FB8FF, 8.0), parent=gun, loc=(0, 0.9, 0))
        return {'muzzle': E(gun, (0, 0.93, 0))}
    if kind == 'rocket':        # a tube on the shoulder, the rocket's red nose showing
        g = E(body, (0.2, -0.02, 0.45), rot=(math.radians(5), 0, 0))
        rod((0, -0.4, 0), (0, 0.38, 0), 0.085, 0.085, M(0x4F6240, 0.5), parent=g)
        for y in (-0.4, 0.3):
            rod((0, y, 0), (0, y + 0.08, 0), 0.1, 0.1, M(0x2E3A26, 0.5), parent=g)
        rod((0, 0.38, 0), (0, 0.58, 0), 0.07, 0.004, M(RED, 0.3), parent=g)
        box((0.03, 0.08, 0.08), M(BLACK), bevel=0, parent=g, loc=(0, 0.1, 0.1))
        return {'muzzle': E(g, (0, 0.6, 0))}
    if kind == 'flame':         # two red tanks on the pack, a hose, a steel nozzle with a pilot light
        for x in (-0.08, 0.08):
            lathe([(0, 0), (0.075, 0.02), (0.075, 0.3), (0, 0.34)], M(0xE0482A, 0.3, 0.2), seg=12, smooth=True, parent=body, loc=(x, -0.33, 0.1))
            ell((0.03, 0.03, 0.02), M(STEEL_L, 0.3, 0.7), parent=body, loc=(x, -0.33, 0.45))
        rod((0.1, -0.3, 0.14), (0.24, -0.1, 0.18), 0.022, 0.022, M(BLACK, 0.5), parent=body, seg=6)
        rod((0, -0.2, -0.02), (0, 0.34, 0.0), 0.04, 0.035, M(STEEL, 0.3, 0.7), parent=gun)
        rod((0, 0.34, 0.0), (0, 0.46, 0.0), 0.035, 0.07, M(0x2E3038, 0.4, 0.6), parent=gun)
        ell(0.04, GL(0xFFB03A, 10.0), parent=gun, loc=(0, 0.48, 0.0))
        return {'muzzle': E(gun, (0, 0.5, 0))}
    if kind == 'coil':          # a brass rod wound with copper rings, a glowing violet ball at its tip
        rod((0, -0.2, -0.02), (0, 0.5, 0.0), 0.03, 0.026, M(0xD9A441, 0.25, 0.7), parent=gun)
        for y in (0.12, 0.22, 0.32):
            rod((0, y, 0), (0, y + 0.05, 0), 0.062, 0.062, M(0xC8643C, 0.3, 0.7), parent=gun)
        ell(0.085, GL(0xB07AFF, 9.0), parent=gun, loc=(0, 0.57, 0))
        return {'muzzle': E(gun, (0, 0.6, 0))}
    if kind == 'mortar':        # a stubby tube carried at the chest, pointing up and forward, on a little base plate
        g = E(body, (0.2, 0.12, 0.16), rot=(math.radians(58), 0, 0))
        rod((0, -0.06, 0), (0, 0.36, 0), 0.085, 0.085, M(0x4F6240, 0.5), parent=g)
        rod((0, 0.3, 0), (0, 0.38, 0), 0.1, 0.1, M(0x2E3A26, 0.5), parent=g)
        rod((0, 0.375, 0), (0, 0.385, 0), 0.07, 0.07, M(BLACK), parent=g)
        box((0.22, 0.05, 0.22), M(0x2E3A26, 0.5), bevel=0.01, parent=g, loc=(0, -0.08, 0))
        return {'muzzle': E(g, (0, 0.42, 0))}
    if kind == 'frost':         # a pale blue gun with a small tank and an ice crystal for a muzzle
        rod((0, -0.2, -0.03), (0, 0.12, 0), 0.05, 0.04, M(0xDCE6F4, 0.3, 0.3), parent=gun)
        rod((0, 0.06, 0.01), (0, 0.4, 0.01), 0.04, 0.034, M(0x6FC8F0, 0.25, 0.5), parent=gun)
        ell((0.06, 0.09, 0.06), M(0xBFE8FF, 0.2, 0.3), parent=gun, loc=(0, 0.02, 0.08))
        o = lathe([(0, 0), (0.07, 0.06), (0.05, 0.2), (0, 0.28)], GL(0x9CE4FF, 5.0), seg=6, flat=True, parent=gun, loc=(0, 0.4, 0.01), rot=(math.radians(-90), 0, 0))
        return {'muzzle': E(gun, (0, 0.7, 0.01))}
    raise ValueError(kind)

def soldier(cheer=False, turn=0.0, weapon='musket', overlay=False, wave=False):
    root = E(scale=0.9, rot=(0, 0, turn))
    body = E(root, (0, 0, 0.36))
    legs = [leg(body, -0.09, M(NAVY), M(BLACK)), leg(body, 0.09, M(NAVY), M(BLACK))]
    ell((0.21, 0.17, 0.22), M(BLUE), parent=body, loc=(0, 0, 0.2))
    lathe([(0.2, 0.03), (0.2, 0.09)], M(NAVY), parent=body, scale=(1.03, 0.85, 1))           # belt
    box((0.25, 0.12, 0.2), M(0x7A4E2C, 0.6, 0, 0.1), bevel=0.03, parent=body, loc=(0, -0.2, 0.2))  # pack
    rod((-0.17, -0.21, 0.37), (0.17, -0.21, 0.37), 0.06, 0.06, M(0x2A63F0, 0.7, 0, 0), parent=body)  # bedroll
    for x in (-0.11, 0.11):
        rod((x, -0.21, 0.37), (x + 0.001, -0.21, 0.37), 0.065, 0.065, M(GOLD), parent=body).scale = (1, 1, 30)
    for x in (-0.21, 0.21):
        ell((0.075, 0.075, 0.035), M(GOLD, 0.25, 0.3), parent=body, loc=(x, 0, 0.36))
    armL = E(body, (-0.23, 0, 0.32))
    ell((0.062, 0.062, 0.14), M(BLUE), parent=armL, loc=(0, 0, -0.11))
    ell(0.052, M(SKIN, 0.5, 0, 0.2), parent=armL, loc=(0, 0, -0.25))
    armR = E(body, (0.23, 0, 0.32), rot=(math.radians(55), 0, 0))
    ell((0.062, 0.062, 0.14), M(BLUE), parent=armR, loc=(0, 0, -0.11))
    ell(0.052, M(SKIN, 0.5, 0, 0.2), parent=armR, loc=(0, 0, -0.25))
    gun = E(body, (0.23, 0.05, 0.2), rot=(math.radians(8), 0, 0))
    before = set(bpy.data.objects)
    points = weapon_parts(weapon, gun, body) if weapon else {'muzzle': E(gun, (0, 0.64, 0.01))}
    weapon_objs = set(bpy.data.objects) - before
    head = E(body, (0, 0, 0.42))
    ell(0.165, M(SKIN, 0.5, 0, 0.2), parent=head, loc=(0, 0, 0.1))
    ell((0.17, 0.17, 0.1), M(0x5A3A22, 0.6), parent=head, loc=(0, -0.02, 0.13))                 # hair under the hat
    hat = E(head)
    lathe([(0, 0.17), (0.17, 0.17), (0.195, 0.42), (0, 0.42)], M(BLUE_D), parent=hat)        # shako
    lathe([(0.172, 0.17), (0.178, 0.235)], M(GOLD, 0.25, 0.3), parent=hat)
    lathe([(0.2, 0.405), (0.2, 0.43), (0, 0.43)], M(GOLD, 0.25, 0.3), parent=hat)
    lathe([(0, 0.432), (0.16, 0.432)], M(BLUE), parent=hat)
    ell((0.13, 0.1, 0.02), M(BLACK), parent=hat, loc=(0, 0.15, 0.18))                         # visor
    star(0.055, 0.025, 0.02, M(GOLD, 0.2, 0.5), parent=hat, loc=(0, 0.185, 0.3), rot=(0, 0, math.pi))
    plume = E(hat, (0, 0.12, 0.42))
    ell((0.065, 0.065, 0.13), M(WHITE, 0.6, 0, 0), parent=plume, loc=(0, 0, 0.1))
    for x in (-0.065, 0.065):                                                                 # the face: the icon, the HUD marker
        ell(0.028, M(BLACK, 0.2), parent=head, loc=(x, 0.148, 0.1))
        ell((0.04, 0.02, 0.03), M(0xF29A8C, 0.6, 0, 0), parent=head, loc=(x * 1.75, 0.13, 0.035))
    ell((0.06, 0.025, 0.02), M(0x5A3A22, 0.6), parent=head, loc=(0, 0.158, 0.03))             # moustache
    def pose(t):
        a = math.sin(TAU * t)
        legs[0].rotation_euler.x, legs[1].rotation_euler.x = 0.7 * a, -0.7 * a
        body.location.z = 0.36 + 0.035 * abs(math.cos(TAU * t))
        body.rotation_euler = (0.04, 0.05 * a, 0.10 * a)
        armL.rotation_euler.x = -0.6 * a
        plume.rotation_euler.x = -0.2 + 0.15 * math.sin(2 * TAU * t)
        gun.rotation_euler.x = math.radians(8) + 0.04 * math.cos(2 * TAU * t)
    def cheer_pose(t):      # a jump, both arms up, the hat thrown and caught
        j = max(0.0, math.sin(TAU * t))
        body.location.z = 0.36 + 0.2 * j
        legs[0].rotation_euler.x, legs[1].rotation_euler.x = 0.35 * j, -0.25 * j
        armL.rotation_euler = (math.radians(160), 0, -0.25 - 0.15 * j)
        armR.rotation_euler = (math.radians(160), 0, 0.25)
        gun.location, gun.rotation_euler = (0.3, 0.02, 0.62), (math.radians(82), 0, 0)
        k = math.sin(math.pi * t) ** 0.8
        hat.location.z = 0.75 * k
        hat.rotation_euler.x = -TAU * t
    if overlay:     # only the gun is drawn; the soldier hides what is behind him (a sprite over soldier.march)
        for o in bpy.data.objects:
            if o.type == 'MESH' and o.visible_camera and o not in weapon_objs:
                o.is_holdout = True
    if cheer:
        return root, cheer_pose, {}
    if wave:        # a prisoner calling for help: one arm high, waving; a small bounce
        def wave_pose(t):
            body.location.z = 0.36 + 0.03 * abs(math.sin(TAU * t))
            armL.rotation_euler = (math.radians(170), 0.5 * math.sin(TAU * t), 0)
            armR.rotation_euler = (0.1, 0, 0)
            head.rotation_euler.z = 0.15 * math.sin(TAU * t)
        return root, wave_pose, {}
    return root, pose, points

def raider(parent, scale=1.0, mounted=False, hood=False):
    """The red horde's body: used on foot and in the saddle. Faces +Y; the caller turns it round."""
    body = E(parent, (0, 0, 0.34), scale=scale)
    legs = []
    if not mounted:
        legs = [leg(body, -0.09, M(0x3A3340), M(WOOD_D)), leg(body, 0.09, M(0x3A3340), M(WOOD_D))]
    ell((0.21, 0.18, 0.22), M(RED), parent=body, loc=(0, 0, 0.2))
    lathe([(0.2, 0.02), (0.2, 0.08)], M(BLACK), parent=body, scale=(1.03, 0.9, 1))
    ell((0.04, 0.02, 0.04), M(GOLD, 0.25, 0.3), parent=body, loc=(0, 0.175, 0.05))
    head = E(body, (0, 0, 0.42))
    ell(0.165, M(0xE8B088, 0.5, 0, 0.2), parent=head, loc=(0, 0, 0.1))
    for x in (-0.065, 0.065):
        ell(0.03, M(BLACK, 0.2), parent=head, loc=(x, 0.145, 0.11))
        box((0.09, 0.03, 0.03), M(BLACK), bevel=0, parent=head, loc=(x, 0.15, 0.16), rot=(0, 0.45 * (1 if x > 0 else -1), 0))
    if hood:
        lathe([(0.2, 0.17), (0.19, 0.24), (0.11, 0.42), (0.02, 0.54), (0, 0.54)], M(RED_D, 0.7, 0, 0), parent=head, smooth=True)   # a pointed felt cap
        lathe([(0.19, 0.16), (0.25, 0.17), (0.19, 0.2)], M(0x7A0F18, 0.7, 0, 0), parent=head, loc=(0, 0.03, 0))
        rod((-0.14, -0.05, 0.26), (-0.3, -0.16, 0.62), 0.035, 0.004, M(CREAM, 0.6, 0, 0), parent=head, seg=5)                        # a feather
    else:
        lathe([(0.2, 0.15), (0.185, 0.2), (0.15, 0.3), (0.06, 0.44), (0, 0.56)], M(STEEL, 0.3, 0.7), parent=head, smooth=True)
        lathe([(0.185, 0.14), (0.215, 0.14), (0.215, 0.18), (0.185, 0.18)], M(STEEL_L, 0.3, 0.7), parent=head)
        box((0.04, 0.03, 0.14), M(STEEL_L, 0.3, 0.7), bevel=0, parent=head, loc=(0, 0.175, 0.12))   # nasal guard
        ell(0.04, M(RED), parent=head, loc=(0, 0, 0.56))
    arms = []
    for x in (-0.23, 0.23):
        a = E(body, (x, 0, 0.32))
        ell((0.062, 0.062, 0.14), M(RED_D), parent=a, loc=(0, 0, -0.11))
        ell(0.052, M(0xE8B088, 0.5, 0, 0.2), parent=a, loc=(0, 0, -0.25))
        arms.append(a)
    return body, legs, arms, head

def foot():
    root = E(rot=(0, 0, math.pi))
    body, legs, arms, head = raider(root)
    arms[0].rotation_euler.x = math.radians(50)
    sh = E(body, (-0.25, 0.2, 0.16), rot=(math.radians(-80), 0, math.radians(15)))            # shield, facing forward
    lathe([(0, 0.05), (0.19, 0.02), (0.2, 0), (0, 0)], M(WOOD), parent=sh, smooth=False)
    lathe([(0, 0.065), (0.15, 0.03)], M(RED_D), parent=sh)
    ell((0.06, 0.06, 0.04), M(GOLD, 0.25, 0.3), parent=sh, loc=(0, 0, 0.06))
    arms[1].rotation_euler.x = math.radians(40)
    spear = E(body, (0.27, 0.16, 0.12), rot=(math.radians(-3), 0, 0))
    rod((0, 0, -0.3), (0, 0, 0.95), 0.022, 0.022, M(WOOD), parent=spear)
    rod((0, 0, 0.95), (0, 0, 1.2), 0.05, 0.002, M(STEEL_L, 0.25, 0.8), parent=spear, seg=4)
    flag = pennant(0.3, 0.16, M(RED, 0.6, 0, 0), parent=spear, loc=(0, 0, 0.76))
    def pose(t):
        a = math.sin(TAU * t)
        legs[0].rotation_euler.x, legs[1].rotation_euler.x = 0.7 * a, -0.7 * a
        body.location.z = 0.34 + 0.035 * abs(math.cos(TAU * t))
        body.rotation_euler = (0.06, 0.06 * a, 0.10 * a)
        spear.rotation_euler = (math.radians(-3) + 0.05 * a, 0.05 * math.cos(TAU * t), 0)
        flag.rotation_euler.z = 0.5 * math.sin(TAU * t + 1)
    return root, pose, {}

def rider():
    root = E(rot=(0, 0, math.pi))
    boar = E(root, (0, 0, 0.5))
    ell((0.3, 0.52, 0.3), M(BOAR, 0.7, 0, 0), parent=boar)
    ell((0.31, 0.3, 0.3), M(BOAR, 0.7, 0, 0), parent=boar, loc=(0, 0.26, 0.08))                  # shoulder hump
    ell((0.24, 0.26, 0.23), M(BOAR, 0.7, 0, 0), parent=boar, loc=(0, 0.62, -0.02))               # head
    rod((0, 0.75, -0.06), (0, 0.98, -0.1), 0.14, 0.11, M(BOAR_D, 0.7, 0, 0), parent=boar)
    lathe([(0, 0), (0.1, 0)], M(0xE88A8A, 0.5), parent=boar, loc=(0, 0.985, -0.1), rot=(math.radians(-80), 0, 0))
    for x in (-1, 1):
        rod((0.1 * x, 0.86, -0.1), (0.24 * x, 0.97, 0.16), 0.05, 0.004, M(IVORY, 0.3), parent=boar)   # tusks
        rod((0.14 * x, 0.55, 0.16), (0.22 * x, 0.5, 0.34), 0.07, 0.01, M(BOAR_D, 0.7, 0, 0), parent=boar, seg=6)  # ears
        ell(0.03, M(0xFFE066, 0.2, 0, 0.5, 2.0), parent=boar, loc=(0.13 * x, 0.8, 0.07))               # eyes
    for i in range(7):
        y = 0.6 - i * 0.16
        rod((0, y, 0.27 + 0.1 * math.exp(-((y - 0.3) / 0.3) ** 2)), (0, y - 0.06, 0.45 + 0.1 * math.exp(-((y - 0.3) / 0.3) ** 2)),
            0.05, 0.004, M(BLACK, 0.6, 0, 0), parent=boar, seg=5)                                # bristles
    rod((0, -0.5, 0.1), (0, -0.66, 0.26), 0.02, 0.012, M(BOAR_D), parent=boar)
    ell((0.33, 0.24, 0.2), M(RED_D, 0.6, 0, 0.1), parent=boar, loc=(0, -0.08, 0.14))             # saddle cloth
    lathe([(0.325, 0), (0.325, 0.05)], M(GOLD, 0.25, 0.3), parent=boar, loc=(0, -0.08, 0.06), scale=(1.02, 0.75, 1))
    legs = []
    for x in (-0.19, 0.19):
        for y in (0.3, -0.3):
            p = E(boar, (x, y, -0.12))
            lathe([(0, 0.02), (0.1, 0), (0.06, -0.28)], M(BOAR, 0.7, 0, 0), parent=p, seg=10)
            lathe([(0.065, -0.28), (0.07, -0.38), (0, -0.38)], M(BLACK), parent=p, seg=10)
            legs.append((p, y))
    man = E(boar, (0, -0.08, 0.02))
    body, _, arms, head = raider(man, 0.92, mounted=True)
    for x in (-1, 1):
        ell((0.08, 0.1, 0.17), M(0x3A3340), parent=body, loc=(0.24 * x, 0.04, -0.05))
        ell((0.085, 0.13, 0.07), M(WOOD_D), parent=body, loc=(0.27 * x, 0.08, -0.22))
    arms[0].rotation_euler.x = math.radians(60)
    arms[1].rotation_euler.x = math.radians(50)
    lance = E(body, (0.33, 0.1, 0.12), rot=(math.radians(-6), 0, math.radians(-4)))
    rod((0, -0.55, 0), (0, 1.25, 0), 0.03, 0.022, M(CREAM), parent=lance)
    rod((0, 1.25, 0), (0, 1.5, 0), 0.05, 0.002, M(STEEL_L, 0.25, 0.8), parent=lance, seg=4)
    flag = pennant(0.5, 0.2, M(RED, 0.6, 0, 0), swallow=True, parent=lance, loc=(0, 1.2, 0.02))
    def pose(t):
        c, s = math.cos(TAU * t), math.sin(TAU * t)
        boar.location.z = 0.5 + 0.08 * max(0.0, s)
        boar.rotation_euler.x = 0.1 * c
        for p, y in legs:
            ph = TAU * t + (0.0 if y > 0 else 2.2)
            p.rotation_euler.x = 0.75 * math.sin(ph)
        body.location.z = 0.34 + 0.03 * max(0.0, -s)
        body.rotation_euler.x = 0.12 - 0.08 * c
        flag.rotation_euler.x = 0.3 * s
        lance.rotation_euler.x = math.radians(-6) - 0.05 * c
    return root, pose, {}

def brute(giant=False):
    root = E(rot=(0, 0, math.pi), scale=2.0 if giant else 1.0)
    body = E(root, (0, 0, 0.38))
    legs = [leg(body, x, M(0x3A3340), M(WOOD_D), r=0.15, length=0.34) for x in (-0.22, 0.22)]
    ell((0.52, 0.44, 0.5), M(OX), parent=body, loc=(0, 0, 0.5))
    lathe([(0.49, 0.08), (0.5, 0.2)], M(BLACK), parent=body, scale=(1.04, 0.88, 1))
    ell((0.09, 0.03, 0.08), M(GOLD, 0.25, 0.3), parent=body, loc=(0, 0.45, 0.14))
    for x in (-1, 1):
        ell((0.22, 0.22, 0.17), M(STEEL, 0.3, 0.7), parent=body, loc=(0.47 * x, 0, 0.86))
        rod((0.5 * x, 0, 0.98), (0.62 * x, 0, 1.16), 0.06, 0.002, M(STEEL_L, 0.3, 0.7), parent=body, seg=6)
    head = E(body, (0, 0.06, 0.98))
    ell(0.21, M(0xD99A72, 0.5, 0, 0.2), parent=head, loc=(0, 0, 0.1))
    ell((0.19, 0.14, 0.11), M(0xD99A72, 0.5, 0, 0.2), parent=head, loc=(0, 0.1, 0.0))              # jaw
    for x in (-1, 1):
        ell(0.03, M(0xFFE066, 0.2, 0, 0.5, 3.0), parent=head, loc=(0.08 * x, 0.19, 0.12))
        rod((0.08 * x, 0.2, 0.0), (0.09 * x, 0.23, 0.1), 0.028, 0.003, M(IVORY, 0.3), parent=head, seg=6)
        rod((0.17 * x, 0, 0.26), (0.36 * x, 0.02, 0.36), 0.065, 0.045, M(IVORY, 0.3), parent=head, seg=8)   # horns
        rod((0.36 * x, 0.02, 0.36), (0.44 * x, 0.04, 0.56), 0.045, 0.003, M(IVORY, 0.3), parent=head, seg=8)
    lathe([(0.225, 0.14), (0.22, 0.2), (0.17, 0.3), (0, 0.35)], M(STEEL, 0.3, 0.7), parent=head, smooth=True)
    lathe([(0.225, 0.13), (0.245, 0.13), (0.245, 0.17), (0.225, 0.17)], M(STEEL_L, 0.3, 0.7), parent=head)
    armL = E(body, (-0.55, 0, 0.78), rot=(math.radians(10 if giant else 55), 0, 0))
    ell((0.15, 0.15, 0.3), M(OX), parent=armL, loc=(0, 0, -0.22))
    if not giant:
        sh = E(body, (-0.3, 0.5, 0.42), rot=(math.radians(-78), 0, math.radians(12)))
        lathe([(0, 0.08), (0.42, 0.03), (0.45, 0), (0, 0)], M(0x5B0E18), parent=sh)
        lathe([(0.4, 0.035), (0.455, 0.0), (0.455, -0.02), (0.4, -0.02)], M(GOLD, 0.25, 0.3), parent=sh)
        lathe([(0.13, 0.07), (0.1, 0.12), (0, 0.2)], M(GOLD, 0.25, 0.3), parent=sh, smooth=True)
        for i in range(8):
            ell(0.03, M(GOLD, 0.25, 0.3), parent=sh, loc=(0.3 * math.cos(TAU * i / 8), 0.3 * math.sin(TAU * i / 8), 0.055))
    else:
        ell(0.13, M(0xD99A72, 0.5, 0, 0.2), parent=armL, loc=(0, 0, -0.5))
        lathe([(0.5, 0.3), (0.52, 0.42)], M(WOOD_D, 0.6), parent=body, scale=(1.03, 0.88, 1), rot=(0, 0.5, 0), loc=(0, 0, 0.1))   # a strap
        rod((0, -0.4, 0.5), (0, -0.55, 2.3), 0.035, 0.03, M(WOOD, 0.6), parent=body)                                          # the banner
        ell(0.07, M(GOLD, 0.2, 0.5), parent=body, loc=(0, -0.55, 2.34))
        pennant(0.7, 0.55, M(RED, 0.6, 0, 0), swallow=True, parent=body, loc=(0.02, -0.53, 1.65))
        ell((0.1, 0.03, 0.1), M(GOLD, 0.25, 0.4), parent=body, loc=(0.24, -0.56, 1.93))
    armR = E(body, (0.55, 0, 0.78), rot=(math.radians(35), 0, 0))
    ell((0.15, 0.15, 0.3), M(OX), parent=armR, loc=(0, 0, -0.22))
    ell(0.13, M(0xD99A72, 0.5, 0, 0.2), parent=armR, loc=(0, 0, -0.5))
    mace = E(armR, (0, 0, -0.5), rot=(math.radians(-70), 0, 0))
    rod((0, 0, -0.15), (0, 0, 0.75), 0.04, 0.04, M(WOOD_D), parent=mace)
    if not giant:
        ell(0.17, M(STEEL, 0.3, 0.7), parent=mace, loc=(0, 0, 0.85))
        for i in range(10):
            v = Vector((math.cos(i * 2.4), math.sin(i * 2.4), (i - 4.5) / 5.5)).normalized()
            rod(Vector((0, 0, 0.85)) + v * 0.14, Vector((0, 0, 0.85)) + v * 0.3, 0.045, 0.002, M(STEEL_L, 0.3, 0.7), parent=mace, seg=5)
    else:
        lathe([(0.05, 0.2), (0.2, 0.95), (0.17, 1.1), (0, 1.12)], M(WOOD, 0.7, 0, 0), parent=mace, smooth=True)
        lathe([(0.16, 0.72), (0.185, 0.82)], M(STEEL, 0.35, 0.7), parent=mace)
        for i in range(5):
            v = Vector((math.cos(TAU * i / 5), math.sin(TAU * i / 5), 0))
            rod(Vector((0, 0, 0.92)) + v * 0.17, Vector((0, 0, 0.92)) + v * 0.34, 0.05, 0.003, M(IVORY, 0.3), parent=mace, seg=5)
    def pose(t):
        a = math.sin(TAU * t)
        legs[0].rotation_euler.x, legs[1].rotation_euler.x = 0.6 * a, -0.6 * a
        body.location.z = 0.38 + 0.05 * abs(math.cos(TAU * t))
        body.rotation_euler = (0.08, 0.09 * a, 0.12 * a)
        armR.rotation_euler.x = math.radians(35) + 0.2 * a
        head.rotation_euler.z = -0.1 * a
        if giant: armL.rotation_euler.x = math.radians(10) - 0.35 * a
    return root, pose, {}

def ogre(addon=None, anim='march', skin=0x6FAE3C, belly=0x9CCB5A, cloth=RED):
    root = E(rot=(0, 0, math.pi), scale=2.4)
    S, B, I = M(skin, 0.55, 0, 0.15), M(belly, 0.55, 0, 0.15), M(IVORY, 0.3)
    IRON = M(STEEL, 0.35, 0.7)
    body = E(root, (0, 0, 0.56))
    legs = []
    for x in (-0.33, 0.33):
        p = E(body, (x, 0, 0.05))
        lathe([(0, 0.05), (0.24, 0), (0.19, -0.42)], S, parent=p, smooth=True)
        ell((0.24, 0.34, 0.14), S, parent=p, loc=(0, 0.1, -0.46))
        for k in (-1, 0, 1):
            ell(0.055, I, parent=p, loc=(0.13 * k, 0.4 - 0.03 * abs(k), -0.5))
        legs.append(p)
    torso = E(body, (0, 0, 0))
    ell((0.66, 0.56, 0.56), S, parent=torso, loc=(0, 0, 0.45))
    ell((0.54, 0.46, 0.46), B, parent=torso, loc=(0, 0.16, 0.42))
    ell((0.74, 0.46, 0.4), S, parent=torso, loc=(0, 0, 0.98))
    lathe([(0.63, 0.1), (0.66, 0.24)], M(WOOD_D, 0.6, 0, 0.1), parent=torso, loc=(0, 0.03, 0), scale=(1, 0.9, 1))
    for y, tilt in ((0.57, 0.15), (-0.54, -0.15)):
        o = poly([(-0.27, 0, 0.25), (0.27, 0, 0.25), (0.27, 0, -0.22), (0, 0, -0.42), (-0.27, 0, -0.22)], [(0, 1, 2, 3, 4)],
                 M(cloth, 0.6, 0, 0.1), parent=torso, loc=(0, y, -0.02), rot=(tilt, 0, 0))
        o.modifiers.new('s', 'SOLIDIFY').thickness = 0.05
        box((0.5, 0.07, 0.06), M(GOLD, 0.25, 0.4), bevel=0, parent=torso, loc=(0, y + 0.01 * (1 if y > 0 else -1), -0.2), rot=(tilt, 0, 0))
    lathe([(0, 0.05), (0.13, 0.03), (0.15, 0), (0, 0)], M(GOLD, 0.25, 0.4), parent=torso, loc=(0, 0.6, 0.17), rot=(math.radians(-80), 0, 0))
    for i in range(9):
        a = math.radians(-60 + 15 * i)
        rod((0.42 * math.sin(a), 0.36 * math.cos(a) + 0.08, 1.12 - 0.1 * math.cos(a)),
            (0.44 * math.sin(a), 0.4 * math.cos(a) + 0.08, 0.98 - 0.1 * math.cos(a)), 0.045, 0.004, I, parent=torso, seg=6)
    head = E(torso, (0, 0.14, 1.2))
    ell(0.3, S, parent=head, loc=(0, 0, 0.12))
    ell((0.29, 0.25, 0.17), B, parent=head, loc=(0, 0.12, -0.03))
    ell((0.09, 0.09, 0.08), S, parent=head, loc=(0, 0.3, 0.1))
    box((0.5, 0.12, 0.09), M(0x4A7A28, 0.55), bevel=0.03, parent=head, loc=(0, 0.25, 0.24), rot=(0.25, 0, 0))
    for x in (-1, 1):
        ell(0.05, M(0xFFE23A, 0.2, 0, 0.5, 6.0), parent=head, loc=(0.12 * x, 0.265, 0.17))
        rod((0.14 * x, 0.3, -0.04), (0.17 * x, 0.36, 0.16), 0.05, 0.004, I, parent=head, seg=8)          # tusks
        rod((0.26 * x, 0, 0.14), (0.5 * x, 0, 0.2), 0.1, 0.02, S, parent=head, seg=6)                    # ears
        rod((0.2 * x, 0, 0.32), (0.42 * x, 0.02, 0.5), 0.1, 0.075, I, parent=head, seg=10)               # horns
        rod((0.42 * x, 0.02, 0.5), (0.52 * x, 0.06, 0.74), 0.075, 0.045, I, parent=head, seg=10)
        rod((0.52 * x, 0.06, 0.74), (0.5 * x, 0.12, 0.95), 0.045, 0.003, I, parent=head, seg=10)
    arms = []
    for x in (-1, 1):
        ell((0.3, 0.3, 0.22), IRON, parent=torso, loc=(0.74 * x, 0, 1.12))
        for k in range(3):
            a = math.radians(20 + 35 * k)
            v = Vector((x * math.cos(a), 0, math.sin(a)))
            rod(Vector((0.74 * x, 0, 1.12)) + v * 0.2, Vector((0.74 * x, 0, 1.12)) + v * 0.48, 0.07, 0.003, M(STEEL_L, 0.3, 0.7), parent=torso, seg=6)
        p = E(torso, (0.8 * x, 0, 1.0))
        ell((0.21, 0.21, 0.36), S, parent=p, loc=(0, 0, -0.28))
        fore = E(p, (0, 0, -0.56), rot=(math.radians(35), 0, 0))
        ell((0.23, 0.23, 0.34), S, parent=fore, loc=(0, 0, -0.24))
        lathe([(0.2, -0.4), (0.24, -0.4), (0.24, -0.26), (0.2, -0.26)], IRON, parent=fore)
        ell(0.24, S, parent=fore, loc=(0, 0, -0.58))
        arms.append((p, fore))
    club = E(arms[1][1], (0, 0, -0.58), rot=(math.radians(-55), math.radians(-28), 0))
    lathe([(0, -0.3), (0.08, -0.3), (0.09, 0.2), (0.24, 1.25), (0.2, 1.4), (0, 1.42)], M(WOOD, 0.7, 0, 0), parent=club, smooth=True)
    for z in (0.75, 1.15):
        r = 0.09 + 0.15 * (z - 0.2) / 1.05
        lathe([(r + 0.015, z - 0.06), (r + 0.03, z + 0.06)], IRON, parent=club)
        for i in range(6):
            v = Vector((math.cos(TAU * i / 6 + z), math.sin(TAU * i / 6 + z), 0))
            rod(Vector((0, 0, z)) + v * r, Vector((0, 0, z)) + v * (r + 0.2), 0.05, 0.003, I, parent=club, seg=5)
    def pose(t):
        s, c = math.sin(TAU * t), math.cos(TAU * t)
        for i, p in enumerate(legs):
            k = s if i == 0 else -s
            p.rotation_euler.x = 0.45 * k
            p.location.z = 0.05 + 0.1 * max(0.0, -k if i == 0 else k) * 0
        body.location.z = 0.56 + 0.07 * abs(c)
        torso.rotation_euler = (0.06 + 0.04 * abs(s), 0.07 * s, 0.1 * s)
        head.rotation_euler.z = -0.12 * s
        arms[0][0].rotation_euler.x = -0.35 * s
        arms[1][0].rotation_euler.x = 0.25 * s
        club.rotation_euler.x = math.radians(-55) + 0.15 * c
    def sm(a, b, t):
        k = min(1.0, max(0.0, (t - a) / (b - a)))
        return k * k * (3 - 2 * k)
    def slam(t):        # the club goes up (frames 0-3), comes down (it lands at frame 5), and is lifted again
        r, d, c = sm(0, 0.45, t), sm(0.5, 0.66, t), sm(0.78, 1, t)
        arms[1][0].rotation_euler.x = 0.25 + 2.6 * r - 2.55 * d - 0.05 * c       # up past the head, then down to the road
        club.rotation_euler.x = math.radians(-55) - math.radians(125) * r + math.radians(125) * c   # in line with the forearm, like a bat
        club.rotation_euler.y = math.radians(-28) * (1 - r + c)
        torso.rotation_euler = (0.06 - 0.22 * r + 0.55 * d - 0.33 * c, 0, 0.1 * r - 0.1 * d)
        body.location.z = 0.56 + 0.06 * r - 0.14 * d + 0.08 * c
        arms[0][0].rotation_euler.x = -0.4 * r + 0.6 * d - 0.2 * c
    rock = None
    if anim == 'throw':
        rock = blob(0.36, MT(0x8E97A3), seed=5, sub=1, smooth=False, rough=0.2, parent=arms[0][1], loc=(0, 0.1, -0.9))
    def throw(t):       # a rock lifted over the head with the free hand (frames 0-3) and hurled (it leaves at frame 5)
        r, d, c = sm(0, 0.45, t), sm(0.5, 0.64, t), sm(0.75, 1, t)
        arms[0][0].rotation_euler.x = 2.9 * r - 1.7 * d - 1.2 * c
        torso.rotation_euler = (0.06 - 0.2 * r + 0.45 * d - 0.25 * c, 0, -0.08 * r + 0.08 * d)
        body.location.z = 0.56 + 0.05 * r - 0.1 * d + 0.05 * c
        rock.scale = (0, 0, 0) if t >= 0.6 else (0.36, 0.36, 0.36)
    def roar(t):        # arms thrown wide, the chest back and then forward, the head up, a shake
        k = math.sin(math.pi * t)
        arms[0][0].rotation_euler = (-0.5 * k, 1.1 * k, 0)
        arms[1][0].rotation_euler = (-0.3 * k, -1.1 * k, 0)
        torso.rotation_euler = (0.06 - 0.25 * math.sin(TAU * t), 0, 0)
        head.rotation_euler.x = -0.35 * k
        body.location.z = 0.56 + 0.03 * math.sin(4 * TAU * t) * k
    pose = {'march': pose, 'slam': slam, 'throw': throw, 'roar': roar}[anim]
    points = {'release': E(root, (-0.8, 0.6, 2.35))} if anim == 'throw' else {}
    ogre.head = head
    if addon:   # the ogre hides what is behind it but is not drawn: the add-on is a sprite over the shared boss
        for o in list(bpy.data.objects):
            if o.type == 'MESH' and o.visible_camera:
                o.is_holdout = True
        addon(head, torso)
    return root, pose, points

# ---- barrel, gate parts ------------------------------------------------------------------------------------------------
def barrel():
    g = M(0xFFC228, 0.3, 0.25)
    lathe([(0, 0), (0.37, 0), (0.45, 0.2), (0.5, 0.48), (0.5, 0.68), (0.45, 0.96), (0.37, 1.16), (0.33, 1.16), (0.33, 1.12), (0, 1.12)], g, seg=28, smooth=False)
    for z in (0.16, 0.58, 1.0):
        r = 0.44 if z != 0.58 else 0.51
        lathe([(r, z - 0.05), (r + 0.02, z - 0.05), (r + 0.02, z + 0.05), (r, z + 0.05)], M(0x2456D8, 0.3, 0.4), seg=28)
        for i in range(10):
            ell(0.028, M(GOLD, 0.2, 0.5), loc=((r + 0.025) * math.cos(TAU * i / 10), (r + 0.025) * math.sin(TAU * i / 10), z))
    star(0.2, 0.09, 0.05, M(WHITE, 0.3), loc=(0, -0.5, 0.79))
    star(0.2, 0.09, 0.05, M(WHITE, 0.3), loc=(0, -0.5, 0.37))
    star(0.24, 0.11, 0.04, M(WHITE, 0.3), loc=(0, 0, 1.13), rot=(math.radians(90), 0, 0))
    rod((0.2, 0.1, 1.12), (0.26, 0.12, 1.3), 0.02, 0.015, M(BLACK))
    ell(0.05, M(0xFFB020, 0.2, 0, 0, 12.0), loc=(0.265, 0.122, 1.33))

def gate_post(kind, wheels=False):
    if wheels:      # a little cart that runs on gate.rail
        box((0.62, 0.5, 0.16), MT(0x8A5A34), bevel=0.03, loc=(0, 0, 0.24))
        for x in (-0.2, 0.2):
            for y in (-0.3, 0.3):
                lathe([(0, -0.05), (0.17, -0.05), (0.17, 0.05), (0, 0.05)], M(STEEL, 0.35, 0.7), seg=14, loc=(x, y, 0.17), rot=(math.radians(90), 0, 0))
                lathe([(0, -0.06), (0.06, -0.06), (0.06, 0.06), (0, 0.06)], M(GOLD, 0.25, 0.5), seg=10, loc=(x, y, 0.17), rot=(math.radians(90), 0, 0))
    else:
        lathe([(0, 0), (0.3, 0), (0.3, 0.12), (0.22, 0.2), (0.22, 0.3), (0, 0.3)], M(0xCFC5B4, 0.8, 0, 0), seg=16)
    if kind == 'neg':
        lathe([(0.17, 0.3), (0.12, 2.3), (0, 2.65)], M(WOOD_D, 0.8, 0, 0), seg=10)
        for z in (0.8, 1.7):
            lathe([(0.17, z - 0.06), (0.17, z + 0.06)], M(STEEL, 0.3, 0.7), seg=10)
            for i in range(4):
                v = Vector((math.cos(TAU * i / 4 + 0.8), math.sin(TAU * i / 4 + 0.8), 0))
                rod(Vector((0, 0, z)) + v * 0.14, Vector((0, 0, z)) + v * 0.42, 0.05, 0.003, M(STEEL_L, 0.3, 0.7), seg=5)
        pennant(0.55, 0.36, M(RED, 0.7, 0, 0), swallow=True, loc=(0, -0.02, 1.95), rot=(0, 0.25, 0))
        return
    c, d = {'add': (BLUE, WHITE), 'mul': (VIOLET, GOLD), 'wpn': (0xFF8A1E, 0xE9DFCE)}[kind]
    for i in range(7):
        lathe([(0.12, 0.3 + 0.3 * i), (0.12, 0.6 + 0.3 * i)], M(c if i % 2 == 0 else d, 0.3, 0.3 if (i % 2 and kind == 'mul') else 0), seg=16)
    lathe([(0.17, 2.4), (0.17, 2.46), (0, 2.46)], M(GOLD, 0.25, 0.4), seg=16)
    if kind != 'mul':
        ell(0.17, M(GOLD, 0.2, 0.5), loc=(0, 0, 2.6))
    else:
        star(0.26, 0.12, 0.08, M(GOLD, 0.2, 0.5), loc=(0, 0, 2.68))
    pennant(0.6, 0.34, M(c, 0.6, 0, 0), loc=(0, -0.02, 1.9), rot=(0, 0.2, 0))

def gate_spikes():
    rod((-0.5, 0, 0.14), (0.5, 0, 0.14), 0.09, 0.09, M(WOOD_D, 0.8, 0, 0), seg=8)
    for x in (-0.3, 0.0, 0.3):
        for sgn in (-1, 1):
            a, b = Vector((x - 0.2 * sgn, 0.05 * sgn, 0.0)), Vector((x + 0.22 * sgn, 0.05 * sgn, 0.78))
            rod(a, a + (b - a) * 0.8, 0.05, 0.05, M(WOOD, 0.8, 0, 0), seg=6)
            rod(a + (b - a) * 0.8, b, 0.05, 0.003, M(RED, 0.4), seg=6)

# ---- props: shared builders, coloured per world ------------------------------------------------------------------------
MT = lambda h: M(h, 0.85, 0, 0)          # matt paint
GL = lambda h, e: M(h, 0.3, 0, 0.3, e * 0.3)   # glowing (stronger clips to flat yellow-white)
STONE = 0xCFC5B4

def tree_round(leaf, trunk=WOOD, fruit=None, fruit_r=0.09, fruit_emit=0.0, hang=None):
    lathe([(0.3, 0), (0.2, 0.5), (0.16, 1.6)], MT(trunk), seg=10)
    rnd = random.Random(3)
    blob(1.15, MT(leaf[0]), seed=1, loc=(0, 0, 2.5))
    for i in range(7):
        a = TAU * i / 7
        blob(0.72, MT(leaf[i % 2]), seed=10 + i, loc=(0.85 * math.cos(a), 0.85 * math.sin(a), 2.2 + 0.5 * rnd.random()))
    blob(0.7, MT(leaf[2]), seed=30, loc=(-0.4, -0.45, 3.1))
    blob(0.45, MT(leaf[2]), seed=31, loc=(0.45, -0.3, 3.35))
    for i in range(12 if fruit else 0):
        v = Vector((rnd.uniform(-1, 1), rnd.uniform(-1, 0.2), rnd.uniform(-0.2, 1))).normalized()
        c = fruit[i % len(fruit)]
        ell(fruit_r, GL(c, fruit_emit) if fruit_emit else M(c, 0.3), loc=Vector((0, 0, 2.5)) + v * 1.45)
    for i in range(9 if hang else 0):
        a = TAU * i / 9
        ell((0.16, 0.16, 0.5), MT(hang[i % len(hang)]), loc=(1.25 * math.cos(a), 1.25 * math.sin(a) * 0.9, 1.75 + 0.2 * rnd.random()))

def poplar(leaf=(0x2F9E58, 0x257F4A, 0x5CC978)):
    lathe([(0.2, 0), (0.13, 1.0)], MT(WOOD_D), seg=10)
    blob((0.8, 0.8, 1.9), MT(leaf[0]), seed=4, rough=0.1, loc=(0, 0, 2.7))
    blob((0.6, 0.6, 1.0), MT(leaf[1]), seed=5, loc=(0.25, 0.2, 1.6))
    blob((0.5, 0.5, 1.1), MT(leaf[2]), seed=6, loc=(-0.25, -0.3, 3.2))

def blossom(P=(0xFF8FB8, 0xFFB7D0, 0xFFE3EC), trunk=0x5A3A40):
    rod((0, 0, 0), (0.15, 0, 1.2), 0.2, 0.13, MT(trunk))
    rod((0.15, 0, 1.2), (-0.4, 0, 1.9), 0.12, 0.07, MT(trunk))
    rod((0.15, 0, 1.2), (0.6, 0.1, 1.9), 0.12, 0.07, MT(trunk))
    for i, (x, y, z, r) in enumerate([(-0.55, 0, 2.3, 0.85), (0.65, 0.1, 2.35, 0.8), (0, 0.2, 2.9, 0.8), (0.1, -0.4, 2.2, 0.6),
                                      (-0.3, -0.35, 2.95, 0.5), (0.6, -0.3, 2.9, 0.45), (-0.9, -0.2, 1.9, 0.4)]):
        blob(r, MT(P[0 if i < 3 else (1 if i < 5 else 2)]), seed=40 + i, loc=(x, y, z))

def bush(G=(0x4FB04A, 0x3C9440, 0x7FD060), dots=(0xFFE04A, WHITE, 0xFF6FA0)):
    blob(0.55, MT(G[0]), seed=1, loc=(0, 0, 0.4)); blob(0.42, MT(G[1]), seed=2, loc=(0.5, 0.1, 0.32)); blob(0.36, MT(G[2]), seed=3, loc=(-0.42, -0.15, 0.36))
    rnd = random.Random(5)
    for i in range(9 if dots else 0):
        v = Vector((rnd.uniform(-1, 1), rnd.uniform(-1, 0), rnd.uniform(0.2, 1))).normalized()
        ell(0.06, M(dots[i % len(dots)], 0.4), loc=Vector((rnd.uniform(-0.3, 0.3), 0, 0.38)) + v * 0.55)

def rock(a=0xA6AEB8, b=0x8E97A3, top=0x6FBF4A):
    blob((0.75, 0.6, 0.5), MT(a), seed=7, sub=1, smooth=False, rough=0.25, loc=(0, 0, 0.3))
    blob((0.4, 0.35, 0.3), MT(b), seed=9, sub=1, smooth=False, rough=0.25, loc=(0.65, -0.2, 0.18))
    if top: blob((0.45, 0.35, 0.12), MT(top), seed=11, loc=(-0.1, -0.05, 0.72))

def flowers():
    rnd = random.Random(2)
    for i in range(9):
        x, y = rnd.uniform(-0.4, 0.4), rnd.uniform(-0.25, 0.25)
        hgt = rnd.uniform(0.22, 0.4)
        rod((x, y, 0), (x, y, hgt), 0.02, 0.012, MT(0x3C9440), seg=5)
        ell((0.09, 0.09, 0.06), M([0xFFE04A, WHITE, 0xFF6FA0, 0xFF4A4A][i % 4], 0.4), loc=(x, y, hgt))
        ell(0.035, M(0xFFB020, 0.4), loc=(x, y, hgt + 0.04))
    blob((0.45, 0.3, 0.12), MT(0x4FB04A), seed=3, loc=(0, 0, 0.06))

def haystack(c=0xEDBA40):
    lathe([(0, 0), (0.85, 0), (0.9, 0.5), (0.65, 1.0), (0.28, 1.35), (0, 1.48)], MT(c), seg=14, smooth=True)
    lathe([(0.88, 0.42), (0.92, 0.5), (0.88, 0.58)], MT(0xB07A2A), seg=14)
    rod((0.3, -0.5, 0), (0.75, -0.75, 1.5), 0.025, 0.025, M(WOOD_D))

def cottage(wall=0xE6DAC2, roof=0xD63A26, door=0x2F6BFF, stilts=0.0, snow=False):
    r = E(loc=(0, 0, stilts), rot=(0, 0, math.radians(-18)))
    for x in (-1.1, 1.1) if stilts else ():
        for y in (-0.8, 0.8):
            rod((x, y, -stilts), (x, y, 0.1), 0.09, 0.09, MT(WOOD_D), parent=r)
    box((2.6, 2.0, 1.5), MT(wall), bevel=0.04, parent=r, loc=(0, 0, 0.75))
    w, d, h0, h1 = 1.5, 1.2, 1.45, 2.6
    poly([(-w, -d, h0), (w, -d, h0), (w, d, h0), (-w, d, h0), (-w, 0, h1), (w, 0, h1)],
         [(0, 1, 5, 4), (2, 3, 4, 5), (0, 4, 3), (1, 2, 5), (0, 3, 2, 1)], MT(roof), parent=r)
    if snow:   # a white blanket on the upper two thirds of the roof
        poly([(-w, -d * 0.7, h0 + 0.4), (w, -d * 0.7, h0 + 0.4), (w, d * 0.7, h0 + 0.4), (-w, d * 0.7, h0 + 0.4), (-w, 0, h1 + 0.08), (w, 0, h1 + 0.08)],
             [(0, 1, 5, 4), (2, 3, 4, 5)], MT(0xD4DEEA), parent=r)
    box((0.5, 0.08, 0.9), M(door, 0.5), bevel=0.02, parent=r, loc=(-0.5, -1.0, 0.45))
    box((0.5, 0.08, 0.5), M(WHITE, 0.5), bevel=0.02, parent=r, loc=(0.7, -1.0, 0.85))
    box((0.38, 0.1, 0.38), M(0x6FC8FF, 0.2), bevel=0.01, parent=r, loc=(0.7, -1.0, 0.85))
    box((0.5, 0.08, 0.5), M(WHITE, 0.5), bevel=0.02, parent=r, loc=(1.3, -0.3, 0.85), rot=(0, 0, math.radians(90)))
    box((0.4, 0.4, 0.9), MT(0xB8A090), bevel=0.03, parent=r, loc=(0.8, 0.5, 2.3))

def windmill():
    lathe([(0, 0), (1.15, 0), (0.75, 3.7), (0, 3.7)], MT(0xE6DAC2), seg=10)
    lathe([(0.95, 3.6), (0, 4.9)], MT(0xD63A26), seg=10)
    box((0.6, 0.1, 1.1), M(0x2F6BFF, 0.5), bevel=0.02, loc=(0, -1.12, 0.55), rot=(math.radians(-6), 0, 0))
    box((0.4, 0.1, 0.4), M(0x6FC8FF, 0.2), bevel=0.01, loc=(0, -0.95, 2.2), rot=(math.radians(-6), 0, 0))
    hub = E(loc=(0, -1.0, 3.75))
    rod((0, 0.3, 0), (0, -0.15, 0), 0.12, 0.12, M(WOOD_D), parent=hub)
    for i in range(4):
        b = E(hub, rot=(0, TAU * i / 4, 0))
        box((0.1, 0.06, 2.3), MT(WOOD_D), bevel=0.01, parent=b, loc=(0, -0.1, 1.15))
        box((0.55, 0.03, 1.6), MT(0xE9DFC8), bevel=0.01, parent=b, loc=(0.3, -0.1, 1.45))
        for z in (0.75, 1.15, 1.55, 1.95, 2.2):
            box((0.6, 0.05, 0.05), MT(WOOD_D), bevel=0, parent=b, loc=(0.3, -0.12, z))
    def pose(t):
        hub.rotation_euler.y = t * TAU / 4
    return pose

def post(c1=BLUE, c2=WHITE, top='ball', base=STONE):
    """The curb's edge post, 1 unit tall."""
    lathe([(0, 0), (0.2, 0), (0.2, 0.1), (0.13, 0.16)], MT(base), seg=12)
    for i in range(4):
        lathe([(0.12, 0.16 + 0.16 * i), (0.12, 0.32 + 0.16 * i)], M(c2 if i % 2 else c1, 0.35), seg=12)
    if top == 'ball': ell(0.15, M(GOLD, 0.2, 0.5), loc=(0, 0, 0.9))
    elif top == 'lantern':
        box((0.26, 0.26, 0.3), GL(0xFFD46A, 4.0), bevel=0.03, loc=(0, 0, 0.98))
        lathe([(0.24, 1.12), (0, 1.32)], M(c1, 0.35), seg=4)
    elif top == 'flame':
        lathe([(0.1, 0.8), (0.2, 0.95), (0.17, 1.0)], M(STEEL, 0.3, 0.7), seg=10)
        blob((0.15, 0.15, 0.3), GL(0xFF8A1E, 6.0), seed=2, loc=(0, 0, 1.22)); blob((0.08, 0.08, 0.18), GL(0xFFE46A, 9.0), seed=3, loc=(0, -0.05, 1.16))
    elif top == 'flower':
        box((0.5, 0.3, 0.2), MT(WOOD), bevel=0.02, loc=(0, 0, 0.9))
        for i in range(5):
            ell(0.1, M([0xFF6FA0, 0xFFE04A, WHITE][i % 3], 0.4), loc=(-0.2 + 0.1 * i, -0.02, 1.06 + 0.04 * (i % 2)))
    elif top == 'skull':
        ell((0.17, 0.17, 0.16), M(IVORY, 0.4), loc=(0, 0, 0.95))
        for x in (-0.07, 0.07): ell(0.045, M(BLACK), loc=(x, -0.14, 0.97))
    elif top == 'buoy':
        ell((0.2, 0.2, 0.16), M(0xE8413A, 0.3), loc=(0, 0, 0.94)); lathe([(0.205, 0.9), (0.205, 0.98)], M(WHITE, 0.3), seg=14)
    elif top == 'point': lathe([(0.14, 0.8), (0, 1.1)], M(GOLD, 0.2, 0.5), seg=4)

def banner(col=BLUE, mark=WHITE, trim=GOLD):
    lathe([(0, 0), (0.22, 0), (0.22, 0.1), (0.1, 0.2)], MT(STONE), seg=12)
    lathe([(0.07, 0.2), (0.05, 3.3)], M(WOOD, 0.6), seg=10)
    ell(0.12, M(GOLD, 0.2, 0.5), loc=(0, 0, 3.4))
    rod((-0.05, -0.04, 3.1), (0.85, -0.04, 3.1), 0.035, 0.035, M(WOOD, 0.6), seg=8)
    f = E(loc=(0.08, -0.06, 3.05), rot=(0, math.radians(90), 0))
    pennant(1.5, 0.7, M(col, 0.6, 0, 0), swallow=True, parent=f)
    box((0.05, 0.03, 0.72), M(trim, 0.3, 0.3), bevel=0, loc=(0.43, -0.09, 2.75), rot=(0, math.radians(90), 0))
    star(0.2, 0.09, 0.03, M(mark, 0.4), loc=(0.43, -0.09, 2.35))

def fir(snow=False, g=(0x2E8558, 0x3C9E68)):
    lathe([(0.22, 0), (0.15, 0.9)], MT(WOOD_D), seg=10)
    for i in range(4):
        r, z = 1.25 - 0.26 * i, 0.6 + 0.8 * i
        lathe([(0, z), (r, z), (r * 0.22, z + 1.05), (0, z + 1.15)], MT(g[i % 2]), seg=12)
        if snow:
            lathe([(r * 0.72, z + 0.36), (r * 0.2, z + 1.08), (0, z + 1.19)], MT(0xD4DEEA), seg=12, scale=(1.05, 1.05, 1))

def palm(g=(0x2FA88A, 0x49C05E)):
    pts = [(0, 0, 0), (0.15, 0, 0.8), (0.34, 0, 1.6), (0.48, 0, 2.4), (0.55, 0, 3.2)]
    for i in range(4):
        rod(pts[i], pts[i + 1], 0.2 - 0.03 * i, 0.17 - 0.03 * i, MT(0xA8774A if i % 2 else 0x8E6038), seg=10)
    for i in range(9):
        f = E(loc=pts[-1], rot=(0, 0, TAU * i / 9))
        ell((0.85, 0.24, 0.06), MT(g[i % 2]), parent=f, loc=(0.8, 0, -0.12 - 0.1 * (i % 2)), rot=(0, math.radians(22 + 14 * (i % 2)), 0))
    for a in (0.5, 2.6, 4.4):
        ell(0.13, MT(0x6B4630), loc=(0.55 + 0.16 * math.cos(a), 0.16 * math.sin(a), 3.05))

def cactus():
    g = MT(0x4FA86A)
    lathe([(0, 0), (0.3, 0), (0.32, 1.5), (0.2, 1.85), (0, 1.95)], g, seg=12, smooth=True)
    for sx, z, hgt in ((1, 0.8, 0.7), (-1, 1.1, 0.5)):
        rod((0, 0, z), (0.6 * sx, 0, z), 0.15, 0.15, g, seg=10)
        lathe([(0.15, 0), (0.16, hgt), (0, hgt + 0.15)], g, seg=10, smooth=True, loc=(0.6 * sx, 0, z))
    ell((0.14, 0.14, 0.09), M(0xFF6FA0, 0.4), loc=(0, 0, 1.97))

def tent(c1=0xE0338F, c2=0xFFF0D0, flag=GOLD):
    lathe([(1.25, 0), (1.25, 0.9)], MT(c2), seg=16)
    for i in range(5):
        r0, r1 = 1.4 * (1 - i / 5), 1.4 * (1 - (i + 1) / 5)
        lathe([(r0, 0.9 + 0.32 * i), (max(r1, 0.0), 0.9 + 0.32 * (i + 1))], MT(c1 if i % 2 == 0 else c2), seg=16)
    box((0.5, 0.06, 0.75), MT(0x5A3A22), bevel=0.02, loc=(0, -1.24, 0.38))
    rod((0, 0, 2.5), (0, 0, 3.1), 0.03, 0.03, M(WOOD))
    pennant(0.5, 0.26, M(flag, 0.5), loc=(0, 0, 2.8))

def arch(c=0xD9A860):
    for x in (-1.1, 1.1):
        box((0.7, 0.7, 2.6), MT(c), bevel=0.05, loc=(x, 0, 1.3))
        box((0.9, 0.9, 0.25), MT(0xC08E48), bevel=0.04, loc=(x, 0, 0.12))
    box((3.3, 0.8, 0.6), MT(c), bevel=0.05, loc=(0, 0, 2.9))
    box((3.6, 0.9, 0.2), MT(0xC08E48), bevel=0.04, loc=(0, 0, 3.3))
    box((0.5, 0.82, 0.3), MT(0x2EC4C0), bevel=0.02, loc=(0, 0, 2.9))

def pots():
    for x, y, s, c in ((-0.45, 0, 1.0, 0xC8643C), (0.4, 0.15, 0.75, 0xD9824E), (0.1, -0.4, 0.55, 0xB8562E)):
        lathe([(0, 0), (0.25, 0), (0.42, 0.4), (0.3, 0.8), (0.2, 0.9), (0.26, 1.0), (0.2, 1.0)], M(c, 0.5), seg=14, smooth=True, loc=(x, y, 0), scale=s)
        lathe([(0.425, 0.36), (0.425, 0.46)], M(0x2EC4C0, 0.4), seg=14, loc=(x, y, 0), scale=s)

def snowman():
    w = MT(0xD4DEEA)
    ell(0.6, w, loc=(0, 0, 0.55)); ell(0.44, w, loc=(0, 0, 1.3)); ell(0.32, w, loc=(0, 0, 1.9))
    rod((0, -0.3, 1.9), (0, -0.62, 1.86), 0.06, 0.005, M(0xFF8A1E, 0.4), seg=8)
    for x in (-0.12, 0.12): ell(0.045, M(BLACK), loc=(x, -0.28, 2.0))
    lathe([(0.33, 1.58), (0.36, 1.7)], M(RED, 0.6), seg=14)
    lathe([(0.4, 2.12), (0.4, 2.16), (0.24, 2.16), (0.24, 2.5), (0, 2.5)], M(BLACK, 0.5), seg=14)
    for sx in (-1, 1): rod((0.4 * sx, 0, 1.4), (0.95 * sx, 0, 1.75), 0.03, 0.02, MT(WOOD_D), seg=6)

def crystal(cols, emit=0.0, n=6, seed=1, hgt=1.8):
    rnd = random.Random(seed)
    for i in range(n):
        h, r = hgt * rnd.uniform(0.45, 1.0), rnd.uniform(0.16, 0.3)
        c = cols[i % len(cols)]
        o = lathe([(0, 0), (r, 0), (r, h * 0.72), (0, h)], GL(c, emit) if emit else M(c, 0.15, 0.0, 1.0), seg=6, flat=True,
                  loc=(rnd.uniform(-0.5, 0.5), rnd.uniform(-0.3, 0.3), 0))
        o.rotation_euler = (rnd.uniform(-0.35, 0.35), rnd.uniform(-0.45, 0.45), rnd.uniform(0, 1))

def pumpkins():
    for x, y, s in ((-0.35, 0, 1.0), (0.45, 0.1, 0.7), (0.1, -0.4, 0.5)):
        ell((0.45 * s, 0.45 * s, 0.34 * s), M(0xF08A1E, 0.45), loc=(x, y, 0.33 * s))
        rod((x, y, 0.62 * s), (x + 0.05, y, 0.8 * s), 0.05 * s, 0.03 * s, MT(0x4F8A3A), seg=6)

def scarecrow():
    rod((0, 0, 0), (0, 0, 2.2), 0.05, 0.05, MT(WOOD_D))
    rod((-0.9, 0, 1.6), (0.9, 0, 1.6), 0.04, 0.04, MT(WOOD_D))
    box((0.9, 0.3, 0.8), MT(0x3A6FD8), bevel=0.06, loc=(0, 0, 1.35))
    box((1.7, 0.22, 0.24), MT(0x3A6FD8), bevel=0.05, loc=(0, 0, 1.6))
    ell(0.3, MT(0xE8D29A), loc=(0, 0, 2.1))
    for x in (-0.1, 0.1): ell(0.04, M(BLACK), loc=(x, -0.27, 2.15))
    lathe([(0.55, 2.3), (0.3, 2.36), (0.2, 2.7), (0, 2.75)], MT(0xD9A441), seg=14)
    for x in (-0.9, 0.9): blob(0.14, MT(0xEDBA40), seed=3, loc=(x, 0, 1.6))

def umbrella(c1=0xE8413A, c2=WHITE):
    rod((0, 0, 0), (0.25, 0, 2.3), 0.04, 0.04, M(WHITE, 0.4))
    cap = E(loc=(0.25, 0, 2.25), rot=(0, math.radians(10), 0))
    for i in range(4):
        lathe([(1.3 * (1 - i / 4), 0.12 * i), (1.3 * (1 - (i + 1) / 4), 0.12 * (i + 1))], M(c1 if i % 2 == 0 else c2, 0.6), seg=16, parent=cap)
    box((0.7, 1.5, 0.06), MT(0x2EC4C0), bevel=0.02, loc=(-0.6, -0.2, 0.04))
    ell((0.3, 0.3, 0.28), M(0xFFD23A, 0.3), loc=(0.8, -0.6, 0.28))

def lighthouse():
    for i in range(5):
        r0, r1 = 1.0 - 0.09 * i, 1.0 - 0.09 * (i + 1)
        lathe([(r0, 0.9 * i), (r1, 0.9 * (i + 1))], MT(0xE8413A if i % 2 == 0 else 0xF2ECE0), seg=16)
    lathe([(0.75, 4.5), (0.75, 4.62), (0, 4.62)], MT(0x3A3F4C), seg=16)
    lathe([(0.42, 4.62), (0.42, 5.2)], GL(0xFFE46A, 5.0), seg=12)
    lathe([(0.6, 5.2), (0, 5.85)], MT(0xE8413A), seg=16)
    box((0.5, 0.1, 0.9), M(0x2456D8, 0.5), bevel=0.02, loc=(0, -1.0, 0.45))

def boat():
    r = E(rot=(0, math.radians(8), math.radians(25)))
    ell((0.7, 1.6, 0.45), M(0x2456D8, 0.4), parent=r, loc=(0, 0, 0.3))
    ell((0.6, 1.45, 0.12), MT(0xE8D8B0), parent=r, loc=(0, 0, 0.62))
    rod((0, 0.1, 0.6), (0, 0.1, 3.0), 0.05, 0.04, MT(WOOD), parent=r)
    o = poly([(0, 0.12, 0.9), (0, 0.12, 2.9), (0, -1.3, 0.9)], [(0, 1, 2)], MT(0xFFF8EC), parent=r)
    o.modifiers.new('s', 'SOLIDIFY').thickness = 0.03

def tulip(c=0xFF4A5A):
    rod((0, 0, 0), (0.05, 0, 1.5), 0.06, 0.05, MT(0x3C9440), seg=8)
    for sx in (-1, 1):
        ell((0.14, 0.05, 0.55), MT(0x4FB04A), loc=(0.2 * sx, 0, 0.5), rot=(0, 0.45 * sx, 0))
    lathe([(0, 1.45), (0.3, 1.55), (0.4, 1.9), (0.3, 2.25), (0.12, 2.3)], M(c, 0.4), seg=12, smooth=True, loc=(0.05, 0, 0))
    for a in range(3):
        ell((0.2, 0.2, 0.42), M(c, 0.4), loc=(0.05 + 0.2 * math.cos(a * 2.1), 0.2 * math.sin(a * 2.1), 1.95))

def beehive():
    box((1.0, 0.8, 0.12), MT(WOOD), bevel=0.02, loc=(0, 0, 0.5))
    for x in (-0.4, 0.4): rod((x, 0, 0), (x, 0, 0.5), 0.05, 0.05, MT(WOOD_D))
    for i in range(5):
        r = 0.48 - 0.02 * i * i
        ell((r, r, 0.14), M(0xF2C14A, 0.5), loc=(0, 0, 0.68 + 0.2 * i))
    ell((0.12, 0.05, 0.1), M(BLACK), loc=(0, -0.47, 0.72))

def mushroom(cap=0xE8413A, spot=WHITE, emit=0.0, h=1.6):
    lathe([(0, 0), (0.3, 0), (0.2, h * 0.5), (0.24, h), (0, h)], MT(0xF2E9D0), seg=12, smooth=True)
    capm = GL(cap, emit) if emit else M(cap, 0.4)
    lathe([(0.24, h - 0.05), (1.0, h), (0.98, h + 0.15), (0.6, h + 0.55), (0, h + 0.72)], capm, seg=18, smooth=True)
    rnd = random.Random(4)
    for i in range(8):
        a, k = rnd.uniform(0, TAU), rnd.uniform(0.25, 0.85)
        ell((0.12, 0.12, 0.05), GL(spot, emit * 1.5) if emit else M(spot, 0.4), loc=(k * 0.95 * math.cos(a), k * 0.95 * math.sin(a), h + 0.7 - 0.55 * k * k))

def mangrove():
    for i in range(6):
        a = TAU * i / 6
        rod((1.0 * math.cos(a), 0.8 * math.sin(a), 0), (0.15 * math.cos(a), 0.15 * math.sin(a), 1.3), 0.07, 0.1, MT(0x5E5240), seg=8)
    lathe([(0.2, 1.2), (0.15, 2.3)], MT(0x5E5240), seg=10)
    for i, (x, y, z, r) in enumerate(((0, 0, 2.9, 1.2), (-0.9, 0, 2.5, 0.8), (0.9, 0.1, 2.6, 0.85), (0.2, -0.5, 3.3, 0.7))):
        blob((r, r, r * 0.6), MT([0x2F7F5A, 0x3F9A66, 0x256A4C, 0x58B070][i]), seed=60 + i, loc=(x, y, z))
    for i in range(5):
        rod((-0.8 + 0.4 * i, -0.3, 2.3), (-0.8 + 0.4 * i, -0.3, 1.5 + 0.15 * (i % 2)), 0.02, 0.015, MT(0x4E8A5E), seg=5)

def reeds(g=0x5A9A4E, tip=0x6B4630):
    rnd = random.Random(6)
    for i in range(11):
        x, y, h = rnd.uniform(-0.5, 0.5), rnd.uniform(-0.25, 0.25), rnd.uniform(1.0, 1.9)
        rod((x, y, 0), (x + rnd.uniform(-0.15, 0.15), y, h), 0.03, 0.02, MT(g), seg=5)
        if i % 2 == 0: ell((0.06, 0.06, 0.18), MT(tip), loc=(x, y, h))

def column(broken=False):
    m = M(0xBCC4D0, 0.5)
    box((1.0, 1.0, 0.25), m, bevel=0.03, loc=(0, 0, 0.12))
    h = 1.6 if broken else 3.2
    lathe([(0.38, 0.25), (0.33, 0.25 + h)] + ([(0, 0.4 + h)] if broken else []), m, seg=12, flat=True)
    if not broken:
        box((0.95, 0.95, 0.22), m, bevel=0.03, loc=(0, 0, 3.56))
        lathe([(0.5, 3.67), (0.5, 3.75)], M(GOLD, 0.2, 0.5), seg=12)
    else:
        box((0.6, 0.5, 0.4), m, bevel=0.05, loc=(0.8, -0.3, 0.2), rot=(0.2, 0.1, 0.6))

def statue():
    box((1.2, 1.2, 0.8), M(0xBCC4D0, 0.5), bevel=0.05, loc=(0, 0, 0.4))
    g = M(GOLD, 0.2, 0.7)
    lathe([(0.3, 0.8), (0.12, 2.4), (0, 2.5)], g, seg=4, flat=True)
    star(0.55, 0.25, 0.14, g, loc=(0, 0, 2.95))

def cloud_tree():
    lathe([(0.22, 0), (0.14, 1.5)], M(GOLD, 0.25, 0.5), seg=10)
    for i, (x, y, z, r) in enumerate(((0, 0, 2.3, 1.0), (-0.8, 0, 2.0, 0.7), (0.8, 0.1, 2.05, 0.75), (0.2, -0.3, 2.8, 0.7), (-0.4, -0.3, 2.7, 0.5))):
        blob(r, MT([0xCCD4E0, 0xB8C6DC, 0xC2CEE0, 0xCCD4E0, 0xAEBEDA][i]), seed=70 + i, rough=0.1, loc=(x, y, z))

def airship():
    r = E(loc=(0, 0, 4.2), rot=(0, 0, math.radians(20)))
    ell((1.7, 0.85, 0.85), M(0xE8413A, 0.4), parent=r)
    lathe([(0.86, -0.25), (0.86, 0.25)], M(GOLD, 0.3, 0.3), seg=20, parent=r, rot=(0, math.radians(90), 0))
    box((0.9, 0.45, 0.35), MT(WOOD), bevel=0.04, parent=r, loc=(0, 0, -1.2))
    for x in (-0.4, 0.4): rod((x, 0, -1.05), (x * 1.6, 0, -0.75), 0.015, 0.015, MT(WOOD_D), parent=r, seg=5)
    o = poly([(-1.5, 0, 0), (-2.2, 0, 0.6), (-2.2, 0, -0.6)], [(0, 1, 2)], M(GOLD, 0.4), parent=r)
    o.modifiers.new('s', 'SOLIDIFY').thickness = 0.05

def charred():
    c = MT(0x2E2830)
    rod((0, 0, 0), (0.1, 0, 1.8), 0.22, 0.13, c)
    for x, z, x2, z2 in ((0.1, 1.8, -0.7, 2.9), (0.1, 1.8, 0.8, 2.8), (0.05, 1.2, 0.9, 1.9), (-0.3, 2.3, -1.0, 2.5), (0.5, 2.4, 0.6, 3.4)):
        rod((x, 0, z), (x2, 0.1, z2), 0.1, 0.03, c, seg=8)
    for p in ((-0.7, 0, 2.9), (0.8, 0, 2.8), (0.6, 0, 3.4), (0.1, -0.15, 1.0)):
        ell(0.07, GL(0xFF7A1A, 10.0), loc=p)

def vent():
    lathe([(0, 0), (1.3, 0), (0.9, 0.5), (0.5, 1.1), (0.36, 1.15)], MT(0x3A2E34), seg=12, smooth=True)
    lathe([(0, 1.1), (0.4, 1.12)], GL(0xFF7A1A, 10.0), seg=12)
    for a in (0.6, 2.9, 4.6):
        rod((0.4 * math.cos(a), 0.4 * math.sin(a), 1.1), (1.0 * math.cos(a), 1.0 * math.sin(a), 0.25), 0.07, 0.05, GL(0xFF5A10, 7.0), seg=6)
    blob((0.25, 0.25, 0.4), GL(0xFFD23A, 10.0), seed=3, loc=(0, 0, 1.35))

def brazier():
    lathe([(0, 0), (0.4, 0), (0.1, 0.25), (0.08, 1.0), (0.45, 1.3), (0.5, 1.5), (0.4, 1.5), (0, 1.38)], M(STEEL, 0.4, 0.7), seg=12)
    blob((0.32, 0.32, 0.55), GL(0xFF7A1A, 8.0), seed=2, loc=(0, 0, 1.85)); blob((0.18, 0.18, 0.35), GL(0xFFE46A, 12.0), seed=3, loc=(0, -0.08, 1.75))

def stall(c1=0xE8413A, c2=WHITE):
    box((2.4, 1.0, 0.9), MT(WOOD), bevel=0.03, loc=(0, 0, 0.45))
    for x in (-1.15, 1.15):
        for y in (-0.45, 0.45): rod((x, y, 0.9), (x, y, 2.2 if y > 0 else 1.9), 0.04, 0.04, MT(WOOD_D), seg=6)
    for i in range(6):
        box((0.42, 1.25, 0.06), M(c1 if i % 2 == 0 else c2, 0.6), bevel=0.01, loc=(-1.05 + 0.42 * i, -0.05, 2.1), rot=(math.radians(15), 0, 0))
    for i in range(6):
        ell(0.14, M([0xFFD23A, 0xFF6FA0, 0x4FC878, 0x6FC8FF][i % 4], 0.3), loc=(-0.9 + 0.36 * i, -0.2, 1.04))

def balloons():
    rod((0, 0, 0), (0, 0, 1.0), 0.05, 0.05, MT(WOOD))
    for i, c in enumerate((0xE8413A, 0xFFD23A, 0x2F6BFF, 0x4FC878, 0x9B4DFF, 0xFF6FA0)):
        p = Vector((0.55 * math.cos(i * 1.05) * (1 + 0.2 * (i % 2)), 0.3 * math.sin(i * 1.05), 2.2 + 0.35 * (i % 3)))
        rod((0, 0, 1.0), p, 0.008, 0.008, M(WHITE), seg=4)
        ell((0.3, 0.3, 0.36), M(c, 0.2, 0, 1.0), loc=p + Vector((0, 0, 0.3)))

def ferris():
    hubp = Vector((0, 0, 3.6))
    for sx in (-1, 1):
        for y in (-0.5, 0.5): rod((1.6 * sx, y, 0), (0, y, 3.6), 0.07, 0.07, M(0xE8413A, 0.4), seg=8)
    rod((0, -0.6, 3.6), (0, 0.6, 3.6), 0.14, 0.14, M(GOLD, 0.3, 0.4))
    hub = E(loc=hubp)
    lathe([(2.9, -0.04), (3.0, -0.04), (3.0, 0.04), (2.9, 0.04), (2.9, -0.04)], M(WHITE, 0.4), seg=32, parent=hub, rot=(math.radians(90), 0, 0))
    cabins = []
    for i in range(8):
        a = TAU * i / 8
        rod((0, 0, 0), (2.95 * math.cos(a), 0, 2.95 * math.sin(a)), 0.03, 0.03, M(WHITE, 0.4), parent=hub, seg=5)
        c = E(loc=hubp)
        box((0.6, 0.5, 0.5), M([0xE8413A, 0xFFD23A, 0x2F6BFF, 0x4FC878, 0x9B4DFF, 0xFF6FA0, 0xFF8A1E, 0x2EC4C0][i], 0.35), bevel=0.06, parent=c, loc=(0, 0, -0.4))
        cabins.append((c, a))
    def pose(t):
        hub.rotation_euler.y = -t * TAU / 8
        for c, a in cabins:
            c.location = hubp + 2.95 * Vector((math.cos(a + t * TAU / 8), 0, math.sin(a + t * TAU / 8)))
    return pose

# ---- boss add-ons (drawn over the shared ogre; head and torso are its pivots) ------------------------------------------
def a_turban(head, torso):
    ell((0.37, 0.37, 0.2), MT(0xFFF0D0), parent=head, loc=(0, -0.02, 0.36))
    for z in (0.3, 0.4): lathe([(0.36, z), (0.38, z + 0.05), (0.36, z + 0.1)], MT(0xE8D8B0), parent=head, rot=(0.15, 0.1, 0))
    ell((0.1, 0.06, 0.12), M(0x2EC4C0, 0.15, 0, 1.0), parent=head, loc=(0, 0.33, 0.42))
    rod((0, 0.3, 0.5), (0.05, 0.28, 0.9), 0.06, 0.01, M(0xE0338F, 0.5), parent=head, seg=6)
def a_fur(head, torso):
    for i in range(12):
        a = TAU * i / 12
        blob(0.26, MT(0xD4DEEA), seed=80 + i, rough=0.12, parent=torso, loc=(0.62 * math.sin(a), 0.42 * math.cos(a) + 0.04, 1.2 - 0.06 * math.cos(a)))
    for x in (-1, 1): blob(0.3, MT(0xC4D2E2), seed=95, rough=0.12, parent=torso, loc=(0.78 * x, 0.1, 1.32))
def a_embers(head, torso):
    for i in range(8):
        a = TAU * i / 8
        rod((0.26 * math.cos(a), 0.26 * math.sin(a), 0.34), (0.3 * math.cos(a), 0.3 * math.sin(a), 0.62 + 0.12 * (i % 2)), 0.08, 0.004, GL(0xFF7A1A if i % 2 else 0xFFD23A, 9.0), parent=head, seg=6)
    for x in (-1, 1): ell(0.07, GL(0xFF5A10, 14.0), parent=head, loc=(0.12 * x, 0.27, 0.17))
def a_crystals(head, torso):
    rnd = random.Random(5)
    for x in (-1, 1):
        for i in range(5):
            v = Vector((x * rnd.uniform(0.1, 0.8), rnd.uniform(-0.4, 0.3), 1.0)).normalized()
            p = Vector((0.74 * x, 0, 1.2))
            o = lathe([(0, 0), (0.1, 0), (0.1, 0.4), (0, 0.6)], M([0xFFD6E4, 0xFFFFFF, 0xF2B8D0][i % 3], 0.1, 0, 1.0), seg=6, flat=True, parent=torso, loc=p)
            o.rotation_mode = 'QUATERNION'; o.rotation_quaternion = v.to_track_quat('Z', 'Y')
def a_flowers(head, torso):
    lathe([(0.3, 0.26), (0.31, 0.32)], MT(0x3C9440), parent=head)
    for i in range(11):
        a = TAU * i / 11
        ell((0.09, 0.09, 0.07), M([0xFF6FA0, 0xFFE04A, WHITE][i % 3], 0.4), parent=head, loc=(0.32 * math.cos(a), 0.32 * math.sin(a), 0.32))
def a_mushrooms(head, torso):
    for x in (-1, 1):
        for i, (dx, dy, s) in enumerate(((0, 0, 1.0), (0.2, 0.15, 0.6), (-0.18, -0.1, 0.7))):
            p = (0.74 * x + dx, dy, 1.3)
            lathe([(0.05, 0), (0.05, 0.22)], MT(0xF2E9D0), seg=8, parent=torso, loc=p, scale=s)
            lathe([(0.05, 0.2), (0.24, 0.2), (0.15, 0.34), (0, 0.4)], M(0xE8413A if i != 1 else 0xF2C14A, 0.4), seg=10, smooth=True, parent=torso, loc=p, scale=s)
def a_crown(head, torso):
    g = M(GOLD, 0.2, 0.7)
    lathe([(0.27, 0.3), (0.3, 0.45)], g, parent=head)
    for i in range(7):
        a = TAU * i / 7
        rod((0.29 * math.cos(a), 0.29 * math.sin(a), 0.44), (0.31 * math.cos(a), 0.31 * math.sin(a), 0.66), 0.06, 0.004, g, parent=head, seg=4)
        ell(0.035, M([0x2F6BFF, 0xE8413A][i % 2], 0.1, 0, 1.0), parent=head, loc=(0.3 * math.cos(a), 0.3 * math.sin(a), 0.38))
def a_flames(head, torso):
    for x in (-1, 1):
        blob((0.1, 0.1, 0.22), GL(0xFF5A10, 10.0), seed=2, parent=head, loc=(0.5 * x, 0.12, 1.05))
        blob((0.06, 0.06, 0.13), GL(0xFFD23A, 14.0), seed=3, parent=head, loc=(0.5 * x, 0.14, 1.0))
        ell(0.075, GL(0xFF2A10, 14.0), parent=head, loc=(0.12 * x, 0.27, 0.17))
def a_lanterns(head, torso):
    for x, c in ((-1, 0xFF4A4A), (1, 0xFFC83A)):
        rod((0.5 * x, 0.12, 0.92), (0.56 * x, 0.2, 0.62), 0.008, 0.008, M(BLACK), parent=head, seg=4)
        ell((0.13, 0.13, 0.16), GL(c, 5.0), parent=head, loc=(0.56 * x, 0.2, 0.48))
        for z in (0.32, 0.64): lathe([(0.06, z - 0.015), (0.06, z + 0.015)], M(BLACK), seg=8, parent=head, loc=(0.56 * x, 0.2, 0))

# ---- the squad's banner, HUD sprites, the icon's soldier ----------------------------------------------------------------
def squad_banner():
    pole = E()
    rod((0, 0, 0), (0, 0, 2.75), 0.045, 0.035, M(WOOD, 0.5), parent=pole)
    ell(0.1, M(GOLD, 0.2, 0.5), parent=pole, loc=(0, 0, 2.82))
    cloth = E(pole, (0, -0.06, 1.95), rot=(math.radians(-30), 0, 0))     # leaning back, to face the high camera
    box((2.0, 0.03, 1.1), M(GOLD, 0.3, 0.3), bevel=0.01, parent=cloth, loc=(0, 0.015, 0))
    box((1.86, 0.03, 0.96), M(BLUE, 0.6, 0, 0), bevel=0.01, parent=cloth, loc=(0, -0.005, 0))
    rod((-1.06, 0, 0.56), (1.06, 0, 0.56), 0.035, 0.035, M(WOOD, 0.5), parent=cloth, seg=8)
    for x in (-1.06, 1.06):
        ell(0.07, M(GOLD, 0.2, 0.5), parent=cloth, loc=(x, 0, 0.56))
        rod((x * 0.9, 0, -0.52), (x * 0.9, 0, -0.78), 0.05, 0.01, M(GOLD, 0.3, 0.3), parent=cloth, seg=6)
    text = E(cloth, (0, -0.03, 0))
    def pose(t):
        pole.rotation_euler.y = 0.05 * math.sin(TAU * t)
        cloth.rotation_euler = (math.radians(-30) + 0.05 * math.cos(TAU * t), 0, 0.03 * math.sin(TAU * t + 1))
    return pole, pose, {'text': text}

def ui_star(full):
    c = M(GOLD, 0.2, 0.4) if full else MT(0x5E5248)
    star(0.5, 0.23, 0.16, c, loc=(0, 0, 0.5))
    star(0.34, 0.16, 0.06, M(0xFFE07A, 0.2, 0.3) if full else MT(0x4A4038), loc=(0, -0.09, 0.5))

def ui_boss_head():
    ogre()
    def under_head(o):
        while o is not None:
            if o == ogre.head: return True
            o = o.parent
        return False
    for o in bpy.data.objects:
        if o.type == 'MESH' and o.visible_camera and not under_head(o):
            o.hide_render = True

def ui_skull():
    iv = M(IVORY, 0.4)
    for sgn in (-1, 1):
        rod((-0.5 * sgn, 0.1, 0.1), (0.5 * sgn, 0.1, 0.95), 0.07, 0.07, iv, seg=8)
        for q in ((-0.5 * sgn, 0.1, 0.1), (0.5 * sgn, 0.1, 0.95)): ell(0.11, iv, loc=q)
    ell((0.4, 0.36, 0.36), iv, loc=(0, 0, 0.62))
    box((0.42, 0.3, 0.26), iv, bevel=0.06, loc=(0, -0.05, 0.3))
    for x in (-0.15, 0.15): ell((0.1, 0.06, 0.11), M(BLACK), loc=(x, -0.31, 0.6))
    ell((0.04, 0.04, 0.06), M(BLACK), loc=(0, -0.36, 0.44))
    for x in (-0.12, 0, 0.12): box((0.02, 0.05, 0.14), M(BLACK), bevel=0, loc=(x, -0.2, 0.26))

def ui_hand():
    r = E(rot=(0, math.radians(-18), 0))
    w = M(WHITE, 0.5, 0, 0.2)
    lathe([(0.2, 0), (0.2, 0.2)], M(BLUE, 0.4), parent=r, scale=(1, 0.6, 1))
    lathe([(0.21, 0.16), (0.21, 0.22)], M(GOLD, 0.3, 0.3), parent=r, scale=(1, 0.6, 1))
    ell((0.26, 0.13, 0.3), w, parent=r, loc=(0, 0, 0.48))
    rod((-0.14, -0.02, 0.68), (-0.16, -0.02, 1.2), 0.08, 0.07, w, parent=r, seg=10, cap=0.07)
    for i, x in enumerate((0.0, 0.13, 0.24)):
        ell((0.075, 0.1, 0.11), w, parent=r, loc=(x, -0.06, 0.74 - 0.04 * i))
    rod((-0.2, -0.02, 0.45), (-0.42, -0.04, 0.66), 0.085, 0.07, w, parent=r, seg=10, cap=0.06)
    tip = E(r, (-0.16, -0.02, 1.27))
    return r, None, {'tip': tip}

def ui_trophy():
    g = M(GOLD, 0.18, 0.7)
    box((1.0, 0.7, 0.3), MT(0x5A3A22), bevel=0.04, loc=(0, 0, 0.15))
    box((0.6, 0.05, 0.16), M(0xFFE07A, 0.2, 0.4), bevel=0.01, loc=(0, -0.36, 0.15))
    lathe([(0.3, 0.3), (0.1, 0.42), (0.08, 0.75), (0.2, 0.85), (0.52, 1.25), (0.6, 1.8), (0.55, 1.8), (0.48, 1.3), (0, 1.0)], g, seg=24, smooth=True)
    lathe([(0.6, 1.76), (0.63, 1.8), (0.6, 1.84)], g, seg=24)
    for sx in (-1, 1):
        pts = [(0.55 * sx, 0, 1.7), (0.85 * sx, 0, 1.6), (0.9 * sx, 0, 1.3), (0.5 * sx, 0, 1.12)]
        for a_, b_ in zip(pts, pts[1:]):
            rod(a_, b_, 0.05, 0.05, g, seg=8); ell(0.05, g, loc=b_)
    star(0.24, 0.11, 0.06, M(WHITE, 0.3), loc=(0, -0.57, 1.5))

# ---- content pass: crates, the medallion ---------------------------------------------------------------------------------
def crate(kind):
    """A wooden crate: orange iron bands and a cream plate for the weapon's icon, or gold bands and stars (supplies)."""
    band = M(0xFF8A1E, 0.3, 0.3) if kind == 'wpn' else M(GOLD, 0.2, 0.5)
    box((1.0, 1.0, 0.9), MT(0xB8823E), bevel=0.03, loc=(0, 0, 0.45))
    for x in (-0.47, 0.47):
        for y in (-0.47, 0.47):
            box((0.14, 0.14, 0.96), MT(0x8A5A34), bevel=0.02, loc=(x, y, 0.48))
    for z in (0.12, 0.78):
        box((1.08, 1.08, 0.12), band, bevel=0.015, loc=(0, 0, z))
    box((1.04, 1.04, 0.06), MT(0x8A5A34), bevel=0.02, loc=(0, 0, 0.93))
    if kind == 'wpn':
        box((0.5, 0.04, 0.42), M(0xDDD3BC, 0.5), bevel=0.01, loc=(0, -0.52, 0.45))
        box((0.6, 0.6, 0.04), M(0xDDD3BC, 0.5), bevel=0.01, loc=(0, 0, 0.97))
    else:
        star(0.22, 0.1, 0.05, M(GOLD, 0.2, 0.5), loc=(0, -0.53, 0.45))
        star(0.26, 0.12, 0.04, M(GOLD, 0.2, 0.5), loc=(0, 0, 0.97), rot=(math.radians(90), 0, 0))

def medal():
    """A gold medallion standing on edge, turning; the engine draws the power-up's icon on it."""
    spin = E(loc=(0, 0, 1.0))
    g, g2 = M(GOLD, 0.18, 0.7), M(0xFFE07A, 0.25, 0.5)
    disc = E(spin, rot=(math.radians(90), 0, 0))
    lathe([(0, -0.06), (0.6, -0.06), (0.6, 0.06), (0, 0.06)], g, seg=32, parent=disc)
    lathe([(0.5, -0.075), (0.6, -0.075), (0.6, 0.075), (0.5, 0.075), (0.5, -0.075)], M(0xD98A00, 0.2, 0.7), seg=32, parent=disc)
    lathe([(0, -0.068), (0.46, -0.068)], g2, seg=32, parent=disc)
    lathe([(0, 0.068), (0.46, 0.068)], g2, seg=32, parent=disc)
    def pose(t):
        spin.rotation_euler.z = math.pi * t        # half a turn: both faces are alike
        spin.location.z = 1.0 + 0.06 * math.sin(TAU * t)
    return spin, pose, {}

# ---- content pass, steps 3 and 4: archer, catapult, mine, rail, cage, boulder -------------------------------------------
def stretch(o, p0, p1):
    """Lay a unit rod (built from the origin to z = 1) between two points."""
    p0, p1 = Vector(p0), Vector(p1)
    o.location = p0
    o.rotation_mode = 'QUATERNION'
    o.rotation_quaternion = (p1 - p0).to_track_quat('Z', 'Y')
    o.scale = (1, 1, (p1 - p0).length)

def archer(anim='idle'):
    root = E(rot=(0, 0, math.pi))
    turn = E(root, rot=(0, 0, math.radians(-65)))                 # a sideways stance, so the bow shows
    body, legs, arms, head = raider(turn, hood=True)
    head.rotation_euler.z = math.radians(65)                      # the face still looks down the road
    rod((0.1, -0.2, 0.08), (-0.04, -0.22, 0.5), 0.06, 0.065, M(WOOD_D, 0.6), parent=body)      # quiver
    for dx in (-0.03, 0.02):
        ell((0.03, 0.03, 0.06), M(CREAM, 0.6), parent=body, loc=(-0.05 + dx, -0.22, 0.56))
    arms[0].rotation_euler.x = math.radians(82)                   # the bow arm, straight out
    bow = E(body, (-0.23, 0.3, 0.3))
    n, hw = 8, 0.72
    pts = [(0, 0.22 * (1 - ((-hw + 2 * hw * k / n) / hw) ** 2), -hw + 2 * hw * k / n) for k in range(n + 1)]
    for p0, p1 in zip(pts, pts[1:]):
        rod(p0, p1, 0.036, 0.036, M(0xE0B070, 0.5), parent=bow, seg=8)
    strings = [rod((0, 0, 0), (0, 0, 1), 0.012, 0.012, M(CREAM), parent=bow, seg=4) for _ in range(2)]
    arrow = E(bow)
    rod((0, 0, 0), (0, 0.66, 0), 0.02, 0.02, M(WOOD, 0.6), parent=arrow, seg=5)
    rod((0, 0.66, 0), (0, 0.76, 0), 0.03, 0.002, M(STEEL_L, 0.25, 0.8), parent=arrow, seg=4)
    ell((0.035, 0.07, 0.035), M(RED, 0.6), parent=arrow, loc=(0, 0.05, 0))
    release = E(bow, (0, 0.45, 0))
    def pose(t):
        if anim == 'idle':
            pull, up, vis = 0.02, -0.25 + 0.05 * math.sin(TAU * t), 0
            body.rotation_euler.z = 0.05 * math.sin(TAU * t)
        else:               # nock and draw (frames 0-4), loose at frame 5, lower
            d = min(1.0, t / 0.5) if t < 0.62 else 0.0
            pull, up, vis = 0.02 + 0.34 * d, 0.5 * min(1.0, t / 0.3) * (1 - max(0.0, (t - 0.75) / 0.25)), 1 if t < 0.62 else 0
            body.rotation_euler.x = -0.1 * d
            arms[1].rotation_euler.x = math.radians(78)
            arms[1].location.y = -0.3 * d
        bow.rotation_euler.x = up
        nock = (0, -pull, 0)
        stretch(strings[0], pts[0], nock); stretch(strings[1], pts[-1], nock)
        arrow.location, arrow.scale = nock, (vis, vis, vis)
    return root, pose, {'release': release}

def catapult(anim='idle'):
    root = E(rot=(0, 0, math.pi))
    T, TD, IR = MT(0x8A5A34), MT(0x5A3A22), M(STEEL, 0.35, 0.7)
    for x in (-0.7, 0.7):
        box((0.22, 2.7, 0.22), T, bevel=0.03, parent=root, loc=(x, 0, 0.5))
        box((0.2, 0.2, 1.45), T, bevel=0.03, parent=root, loc=(x, 0.25, 1.2))
        rod((x, -0.95, 0.55), (x, 0.25, 1.75), 0.07, 0.07, TD, parent=root, seg=6)
        for y in (-0.9, 0.9):
            w = E(root, (x * 1.3, y, 0.42), rot=(0, math.radians(90), 0))
            lathe([(0, -0.08), (0.42, -0.08), (0.42, 0.08), (0, 0.08)], TD, seg=16, parent=w)
            lathe([(0, -0.1), (0.12, -0.1), (0.12, 0.1), (0, 0.1)], IR, seg=10, parent=w)
            lathe([(0.38, -0.09), (0.43, -0.09), (0.43, 0.09), (0.38, 0.09), (0.38, -0.09)], IR, seg=16, parent=w)
    for y in (-1.15, 1.15):
        box((1.62, 0.2, 0.2), TD, bevel=0.03, parent=root, loc=(0, y, 0.5))
    rod((-0.85, 0.25, 1.85), (0.85, 0.25, 1.85), 0.07, 0.07, IR, parent=root, seg=8)
    rod((0.7, 0.25, 1.9), (0.7, 0.25, 2.9), 0.03, 0.03, TD, parent=root, seg=6)
    pennant(0.6, 0.36, M(RED, 0.6, 0, 0), swallow=True, parent=root, loc=(0.7, 0.25, 2.5))
    arm = E(root, (0, 0.25, 1.85))
    box((0.17, 3.1, 0.17), T, bevel=0.03, parent=arm, loc=(0, -0.95, 0))
    lathe([(0, 0), (0.24, 0.02), (0.34, 0.16), (0.36, 0.3), (0.31, 0.3), (0.28, 0.16), (0, 0.08)], IR, seg=14, parent=arm, loc=(0, -2.4, 0.02))
    stone = blob(0.3, MT(0x8E97A3), seed=5, sub=1, smooth=False, rough=0.2, parent=arm, loc=(0, -2.4, 0.38))
    box((0.6, 0.5, 0.6), IR, bevel=0.05, parent=arm, loc=(0, 0.55, -0.32))
    release = E(root, (0, 0.7, 4.2))
    def pose(t):
        if anim == 'idle':
            a = 25
        else:               # strain (frames 0-1), the arm swings over (2-4), the stone leaves at frame 4, the arm rocks
            k = min(1.0, max(0.0, (t - 0.2) / 0.3)); k = k * k * (3 - 2 * k)
            a = 25 + 3 * math.sin(TAU * t * 4) * (t < 0.2) - 125 * k
            if t > 0.5: a += 14 * math.exp(-(t - 0.5) * 7) * math.sin((t - 0.5) * 26)
            stone.scale = (0, 0, 0) if t >= 0.5 else (0.3, 0.3, 0.3)
        arm.rotation_euler.x = math.radians(a)
    return root, pose, {'release': release}

def mine():
    lathe([(0, 0), (0.5, 0), (0.52, 0.08), (0.44, 0.2), (0.26, 0.33), (0, 0.38)], M(0x2E3038, 0.35, 0.6), seg=20, smooth=True)
    lathe([(0.5, 0.03), (0.54, 0.06), (0.5, 0.1)], M(STEEL_L, 0.3, 0.7), seg=20)
    for i in range(8):
        a = TAU * i / 8
        ell(0.055, M(RED, 0.3), loc=(0.4 * math.cos(a), 0.4 * math.sin(a), 0.24))
    rod((0, 0, 0.36), (0, 0, 0.46), 0.05, 0.05, M(STEEL_L, 0.3, 0.7), seg=8)
    off = ell(0.085, M(0x7A1418, 0.3), loc=(0, 0, 0.5))
    on = ell(0.095, GL(0xFF3020, 30.0), loc=(0, 0, 0.5))
    def pose(t):
        on.scale, off.scale = ((0.095,) * 3, (0, 0, 0)) if t >= 0.5 else ((0, 0, 0), (0.085,) * 3)
    return pose

def gate_rail():
    """One unit of the rail a moving gate runs on, across the road: tile it along x."""
    for x in (-0.25, 0.25):
        box((0.18, 0.9, 0.07), MT(0x6A4A30), bevel=0.015, loc=(x, 0, 0.035))
    for y in (-0.3, 0.3):
        rod((-0.5, y, 0.1), (0.5, y, 0.1), 0.04, 0.04, M(STEEL_L, 0.3, 0.8), seg=8)

def cage(part):
    """A green iron cage, 1.7 square and 2.5 tall with its finial, in two sprites: the back (floor, far bars) and the front (near bars, roof).
    Draw back, the prisoners, front."""
    G, GD = M(0x2FA85A, 0.35, 0.5), M(0x1F7A40, 0.4, 0.5)
    back, front = [], []
    back.append(box((1.75, 1.75, 0.12), GD, bevel=0.02, loc=(0, 0, 0.06)))
    xs = [-0.8 + 1.6 * k / 5 for k in range(6)]
    for x in xs:
        back.append(rod((x, 0.8, 0.1), (x, 0.8, 1.9), 0.04, 0.04, G, seg=8))
        front.append(rod((x, -0.8, 0.1), (x, -0.8, 1.9), 0.04, 0.04, G, seg=8))
    for x in (-0.8, 0.8):
        for y in (0.27, -0.27):
            (back if y > 0 else front).append(rod((x, y, 0.1), (x, y, 1.9), 0.04, 0.04, G, seg=8))
    front.append(box((1.78, 0.12, 0.14), GD, bevel=0.02, loc=(0, -0.82, 0.14)))
    for y in (-0.82, 0.82):                                             # an open roof: a frame and two cross bars, so the prisoners show from above
        front.append(box((1.8, 0.14, 0.14), GD, bevel=0.02, loc=(0, y, 1.95)))
        front.append(box((0.14, 1.8, 0.14), GD, bevel=0.02, loc=(y, 0, 1.95)))
    front.append(box((1.7, 0.1, 0.1), G, bevel=0.02, loc=(0, 0, 1.95)))
    front.append(box((0.1, 1.7, 0.1), G, bevel=0.02, loc=(0, 0, 1.95)))
    front.append(rod((0, 0, 1.95), (0, 0, 2.3), 0.05, 0.04, G, seg=8))
    front.append(ell(0.14, M(GOLD, 0.2, 0.5), loc=(0, 0, 2.4)))
    front.append(box((0.3, 0.06, 0.36), M(GOLD, 0.25, 0.5), bevel=0.02, loc=(0, -0.86, 1.0)))       # the lock
    for o in (front if part == 'back' else back):
        o.is_holdout = True

def boulder():
    r = E(loc=(0, 0, 1.0))
    blob(0.55, MT(0x8E97A3), seed=5, sub=1, smooth=False, rough=0.22, parent=r)
    blob(0.3, MT(0x767F8C), seed=8, sub=1, smooth=False, rough=0.2, parent=r, loc=(0.3, 0.1, 0.25))
    def pose(t):
        r.rotation_euler = (TAU * t, 0.4, 0)
    return pose

# ---- content pass, step 5: shield bearer, bomber, drummer, the field cannon, small 3D effects ----------------------------
def shieldman():
    """Marches behind a tower shield; the shield itself is the overlay shield.front (three stages of damage)."""
    root = E(rot=(0, 0, math.pi))
    body, legs, arms, head = raider(root)
    lathe([(0.03, 0.3), (0.05, 0.56), (0, 0.6)], M(RED, 0.5), parent=head, seg=4, scale=(0.5, 3.4, 1))    # a red crest on the helmet
    for a in arms: a.rotation_euler.x = math.radians(62)
    sword = E(body, (0.27, 0.2, 0.14), rot=(math.radians(-18), 0, 0))
    rod((0, 0, -0.08), (0, 0, 0.06), 0.03, 0.03, M(WOOD_D), parent=sword, seg=6)
    box((0.16, 0.04, 0.03), M(GOLD, 0.25, 0.4), bevel=0, parent=sword, loc=(0, 0, 0.07))
    lathe([(0.035, 0.08), (0.03, 0.55), (0, 0.66)], M(STEEL_L, 0.2, 0.8), parent=sword, seg=4, scale=(1, 0.35, 1))
    def pose(t):
        a = math.sin(TAU * t)
        legs[0].rotation_euler.x, legs[1].rotation_euler.x = 0.6 * a, -0.6 * a
        body.location.z = 0.34 + 0.03 * abs(math.cos(TAU * t))
        body.rotation_euler = (0.1, 0.04 * a, 0.06 * a)
        sword.rotation_euler.x = math.radians(-18) + 0.08 * a
    return root, pose, {}

def shield_front():
    """The tower shield, 0.8 wide and 1.1 tall, standing in front of the shieldman: frame 0 whole, 1 cracked, 2 broken."""
    root = E(rot=(0, 0, math.pi))
    sh = E(root, (0, 0.34, 0.08), rot=(math.radians(-8), 0, 0))
    IR, RD = M(STEEL, 0.3, 0.7), M(0xC81E28, 0.35)
    whole = [box((0.8, 0.07, 1.1), IR, bevel=0.035, parent=sh, loc=(0, 0, 0.55)),
             box((0.68, 0.03, 0.98), RD, bevel=0.02, parent=sh, loc=(0, 0.04, 0.55)),
             lathe([(0, 0.06), (0.12, 0.03), (0.15, 0)], M(GOLD, 0.2, 0.5), seg=14, smooth=True, parent=sh, loc=(0, 0.055, 0.58), rot=(math.radians(-90), 0, 0)),
             box((0.68, 0.035, 0.07), M(GOLD, 0.25, 0.4), bevel=0, parent=sh, loc=(0, 0.045, 0.2)),
             box((0.68, 0.035, 0.07), M(GOLD, 0.25, 0.4), bevel=0, parent=sh, loc=(0, 0.045, 0.92))]
    K = M(0x1A1A22, 0.6)
    crack1 = [box((0.03, 0.02, 0.42), K, bevel=0, parent=sh, loc=(0.16, 0.062, 0.78), rot=(0, 0.5, 0)),
              box((0.03, 0.02, 0.26), K, bevel=0, parent=sh, loc=(0.03, 0.062, 0.52), rot=(0, -0.6, 0)),
              box((0.03, 0.02, 0.2), K, bevel=0, parent=sh, loc=(-0.2, 0.062, 0.34), rot=(0, 0.9, 0))]
    crack2 = [box((0.035, 0.02, 0.5), K, bevel=0, parent=sh, loc=(-0.14, 0.062, 0.7), rot=(0, -0.35, 0)),
              box((0.035, 0.02, 0.36), K, bevel=0, parent=sh, loc=(0.2, 0.062, 0.32), rot=(0, 0.7, 0)),
              box((0.05, 0.02, 0.3), K, bevel=0, parent=sh, loc=(0.0, 0.062, 0.3), rot=(0, 0.1, 0))]
    bite = box((0.5, 0.3, 0.5), K, bevel=0, parent=sh, loc=(0.34, 0, 1.02), rot=(0, 0.7, 0))      # cuts the top corner away (a hold-out)
    bite.is_holdout = True
    def pose(t):
        stage = int(round(t * 3))
        for o in crack1: o.scale = (1, 1, 1) if stage >= 1 else (0, 0, 0)
        for o in crack2: o.scale = (1, 1, 1) if stage >= 2 else (0, 0, 0)
        bite.scale = (1, 1, 1) if stage >= 2 else (0, 0, 0)
    return root, pose, {}

def bomber():
    root = E(rot=(0, 0, math.pi))
    body, legs, arms, head = raider(root, scale=0.85)
    for a in arms: a.rotation_euler.x = math.radians(172)
    keg = E(body, (0, 0.02, 0.98))
    lathe([(0, 0), (0.17, 0), (0.22, 0.12), (0.22, 0.24), (0.17, 0.36), (0, 0.36)], M(0x26262E, 0.35, 0.3), seg=16, smooth=True, parent=keg)
    for z in (0.07, 0.29):
        lathe([(0.2, z - 0.025), (0.215, z), (0.2, z + 0.025)], M(STEEL_L, 0.3, 0.7), seg=16, parent=keg)
    star(0.09, 0.04, 0.02, M(RED, 0.4), parent=keg, loc=(0, 0.225, 0.18), rot=(0, 0, math.pi))
    rod((0, 0, 0.36), (0.07, 0, 0.5), 0.02, 0.015, M(CREAM, 0.7), parent=keg, seg=5)
    spark = blob(0.06, GL(0xFFB03A, 30.0), seed=4, sub=1, parent=keg, loc=(0.075, 0, 0.53))
    point = E(keg, (0, 0, 0.18))
    def pose(t):
        a = math.sin(TAU * t)
        legs[0].rotation_euler.x, legs[1].rotation_euler.x = 0.95 * a, -0.95 * a
        body.location.z = 0.34 + 0.05 * abs(math.cos(TAU * t))
        body.rotation_euler = (0.2, 0.05 * a, 0.1 * a)
        keg.rotation_euler.y = 0.08 * a
        k = 0.06 * (0.7 + 0.5 * abs(math.sin(TAU * t * 2)))
        spark.scale = (k, k, k)
    return root, pose, {'keg': point}

def drummer():
    root = E(rot=(0, 0, math.pi))
    body, legs, arms, head = raider(root)
    drum = E(body, (0, 0.3, 0.02))
    lathe([(0, 0), (0.24, 0), (0.25, 0.14), (0.24, 0.28), (0, 0.28)], M(RED, 0.35), seg=18, smooth=False, parent=drum)
    for z in (0.0, 0.26):
        lathe([(0.24, z), (0.265, z), (0.265, z + 0.04), (0.24, z + 0.04)], M(GOLD, 0.25, 0.5), seg=18, parent=drum)
    lathe([(0, 0.3), (0.235, 0.3)], M(CREAM, 0.6, 0, 0), seg=18, parent=drum)
    for i in range(8):
        a0, a1 = TAU * i / 8, TAU * (i + 0.5) / 8
        rod((0.255 * math.cos(a0), 0.255 * math.sin(a0), 0.03), (0.255 * math.cos(a1), 0.255 * math.sin(a1), 0.27), 0.012, 0.012, M(CREAM), parent=drum, seg=4)
    sticks = []
    for i, a in enumerate(arms):
        a.rotation_euler.x = math.radians(55)
        st = E(a, (0, 0, -0.25))
        rod((0, 0, 0), (0, 0.26, 0.02), 0.016, 0.014, M(0xE0B070, 0.5), parent=st, seg=5)
        ell(0.03, M(CREAM, 0.5), parent=st, loc=(0, 0.27, 0.02))
        sticks.append(a)
    point = E(drum, (0, 0, 0.3))
    def pose(t):
        a = math.sin(TAU * t)
        legs[0].rotation_euler.x, legs[1].rotation_euler.x = 0.7 * a, -0.7 * a
        body.location.z = 0.34 + 0.035 * abs(math.cos(TAU * t))
        body.rotation_euler = (0.03, 0.04 * a, 0.08 * a)
        sticks[0].rotation_euler.x = math.radians(55) + 0.45 * max(0.0, math.sin(TAU * t * 2))
        sticks[1].rotation_euler.x = math.radians(55) + 0.45 * max(0.0, -math.sin(TAU * t * 2))
    return root, pose, {'drum': point}

def field_cannon():
    """The squad's brass field gun, rolling up the road (+Y): 1.9 long, 1.3 wide."""
    root = E()
    BR, IR, WD = M(0xD9A441, 0.2, 0.75), M(STEEL, 0.35, 0.7), MT(0x7A4E2C)
    carriage = E(root, (0, 0, 0.5))
    box((0.34, 1.5, 0.16), WD, bevel=0.03, parent=carriage, loc=(0, -0.45, -0.12), rot=(math.radians(-14), 0, 0))     # the trail
    box((0.9, 0.3, 0.2), WD, bevel=0.03, parent=carriage, loc=(0, 0.05, 0.0))
    barrel = E(carriage, (0, 0.05, 0.2), rot=(math.radians(6), 0, 0))
    lathe([(0, -0.5), (0.2, -0.48), (0.22, -0.3), (0.17, 0.3), (0.15, 0.85), (0.19, 0.87), (0.19, 0.98), (0.11, 0.98), (0.11, 0.9), (0, 0.9)], BR, seg=18, parent=barrel, rot=(math.radians(-90), 0, 0))
    ell(0.09, BR, parent=barrel, loc=(0, -0.56, 0))
    for y in (-0.2, 0.25):
        lathe([(0.2, y - 0.03), (0.225, y), (0.2, y + 0.03)], M(GOLD, 0.2, 0.6), seg=18, parent=barrel, rot=(math.radians(-90), 0, 0))
    muzzle = E(barrel, (0, 1.0, 0))
    wheels = []
    for x in (-0.58, 0.58):
        w = E(carriage, (x, 0.05, 0.0), rot=(0, math.radians(90), 0))
        lathe([(0.42, -0.05), (0.5, -0.05), (0.5, 0.05), (0.42, 0.05), (0.42, -0.05)], IR, seg=20, parent=w)
        lathe([(0, -0.07), (0.1, -0.07), (0.1, 0.07), (0, 0.07)], BR, seg=10, parent=w)
        for i in range(8):
            a = TAU * i / 8
            rod((0.08 * math.cos(a), 0.08 * math.sin(a), 0), (0.44 * math.cos(a), 0.44 * math.sin(a), 0), 0.03, 0.025, WD, parent=w, seg=5)
        wheels.append(w)
    def pose(t):
        for w in wheels: w.rotation_euler.z = -TAU * t / 8        # one spoke's turn a cycle: the loop is seamless
        carriage.location.z = 0.5 + 0.012 * math.sin(TAU * t * 2)
        carriage.rotation_euler.x = 0.012 * math.sin(TAU * t * 2 + 1)
    return root, pose, {'muzzle': muzzle}

def cannonball():
    r = E(loc=(0, 0, 0.6))
    ell(0.3, M(0x22242C, 0.3, 0.6), parent=r)
    lathe([(0.3, -0.03), (0.31, 0), (0.3, 0.03)], M(STEEL_L, 0.3, 0.7), seg=18, parent=r)
    for v in ((0, 0.29, 0.1), (0.2, -0.2, 0.1), (-0.2, 0.05, -0.22)):
        ell(0.045, M(STEEL_L, 0.3, 0.7), parent=r, loc=v)
    def pose(t):
        r.rotation_euler = (TAU * t, 0, 0.3)
    return pose

def recruit_fly():
    spin = E(loc=(0, 0, 1.2))
    root, _, _ = soldier(weapon=None)
    root.parent = spin
    root.location = (0, 0, -0.55)
    def pose(t):
        spin.rotation_euler = (TAU * t, 0.3, 0.2)
    return pose

def parachute():
    swing = E(loc=(0, 0, 2.5))
    for i, (z0, z1, c) in enumerate(((0.0, 0.16, BLUE), (0.16, 0.3, 0xE9DFCE), (0.3, 0.4, BLUE), (0.4, 0.45, 0xE9DFCE))):
        r0, r1 = 0.8 * math.cos(z0 / 0.45 * 1.45), 0.8 * math.cos(z1 / 0.45 * 1.45)
        lathe([(r0, z0), (max(r1, 0.0), z1)], M(c, 0.7, 0, 0), seg=16, parent=swing)
    root, _, _ = soldier(weapon=None)
    root.parent = swing
    root.location = (0, 0, -1.55)
    for i in range(6):
        a = TAU * i / 6
        rod((0.78 * math.cos(a), 0.78 * math.sin(a), 0.0), (0.12 * math.cos(a), 0.1 * math.sin(a), -0.85), 0.008, 0.008, M(CREAM), parent=swing, seg=4)
    def pose(t):
        swing.rotation_euler = (0, 0.16 * math.sin(TAU * t), 0)
    return pose

# ---- the list ----------------------------------------------------------------------------------------------------------
UE, PE = [60, 38, 20], [45, 20]
S, Mv, T, L, XL = (2.6, 2.4, 0.5), (4.6, 5.6, 2.0), (3.4, 6.4, 2.6), (5.6, 8.4, 3.2), (8.0, 10.0, 3.6)    # view w, h, centre z
JOBS = [  # kind, builder, anim, frames, elevations, px/unit, (view w, view h, centre z), centre y, scale
    ('soldier', lambda: soldier(weapon=None), 'march', 8, UE, 100, (1.3, 1.9, 0.55), 0.15, 1),
    ('foot', foot, 'march', 8, UE, 84, (1.5, 2.3, 0.75), 0.0, 1),
    ('rider', rider, 'march', 8, UE, 84, (2.0, 3.6, 0.8), -0.3, 1),
    ('brute', brute, 'march', 8, UE, 84, (2.4, 3.2, 0.9), 0.0, 1),
    ('boss', ogre, 'march', 8, [38, 20], 72, (7.2, 8.4, 2.9), -0.4, 1),
    ('barrel', barrel, 'still', 1, UE, 90, (1.8, 2.5, 0.8), 0.0, 1.25),
    ('gate.post_add', lambda: gate_post('add'), 'still', 1, UE, 84, (1.8, 3.6, 1.4), 0.0, 1),
    ('gate.post_mul', lambda: gate_post('mul'), 'still', 1, UE, 84, (1.8, 3.6, 1.4), 0.0, 1),
    ('gate.post_neg', lambda: gate_post('neg'), 'still', 1, UE, 84, (1.8, 3.6, 1.4), 0.0, 1),
    ('gate.spikes', gate_spikes, 'still', 1, UE, 84, (1.3, 1.4, 0.35), 0.0, 1),
]
def P(kind, build, view=Mv, frames=1, scale=1):
    JOBS.append((kind, build, 'still', frames, PE, 64, view, 0.0, scale))
def EDGE(w, post_args, banner_args):
    JOBS.append(('w%d.post' % w, lambda: post(*post_args), 'still', 1, UE, 84, (0.9, 1.9, 0.6), 0.0, 1))
    P('w%d.banner' % w, lambda: banner(*banner_args), (2.6, 4.6, 1.8))
BOSS_ATTACKS = ('slam', 'throw', 'roar')
ATTACK_VIEW = (9.6, 12.0, 4.6)      # the raised club reaches 10 units; these anims are rendered at 52 px a unit
ADDON_VIEW = (6.0, 6.8, 4.8)        # add-ons sit on the head and shoulders: a tighter view (the pivot is computed, so any view lines up)
def ADDON(w, fn):
    JOBS.append(('w%d.boss_addon' % w, lambda: ogre(addon=fn), 'march', 8, [38, 20], 72, (7.2, 8.4, 2.9), -0.4, 1))
    for a in BOSS_ATTACKS:
        JOBS.append(('w%d.boss_addon' % w, lambda a=a: ogre(addon=fn, anim=a), a, 8, [38, 20], 52, ADDON_VIEW, -0.4, 1))

JOBS += [
    ('soldier', lambda: soldier(cheer=True, weapon=None), 'cheer', 8, UE, 100, (1.5, 2.9, 0.95), 0.15, 1),
    ('gate.post_wpn', lambda: gate_post('wpn'), 'still', 1, UE, 84, (1.8, 3.6, 1.4), 0.0, 1),
    ('crate.wpn', lambda: crate('wpn'), 'still', 1, UE, 90, (2.2, 2.6, 0.7), 0.0, 1.3),
    ('crate.supply', lambda: crate('supply'), 'still', 1, UE, 90, (2.2, 2.6, 0.7), 0.0, 1.3),
    ('pickup.medal', medal, 'spin', 8, UE, 90, (1.6, 2.6, 0.95), 0.0, 1),
    ('squad.banner', squad_banner, 'march', 8, UE, 100, (2.8, 4.2, 1.5), 0.2, 1),
    # HUD sprites: seen from the front; pack_atlas.py turns units into dp (UI_DP there)
    ('ui.star_full', lambda: ui_star(True), 'still', 1, [10], 132, (1.2, 1.2, 0.5), 0.0, 1),
    ('ui.star_empty', lambda: ui_star(False), 'still', 1, [10], 132, (1.2, 1.2, 0.5), 0.0, 1),
    ('ui.shako', lambda: soldier(turn=math.pi - 0.45), 'still', 1, [14], 96, (1.2, 1.6, 0.62), 0.0, 1),
    ('ui.boss_head', ui_boss_head, 'still', 1, [10], 42, (3.6, 3.4, 4.75), -0.3, 1),
    ('ui.skull', ui_skull, 'still', 1, [10], 96, (1.4, 1.3, 0.52), 0.0, 1),
    ('ui.hand', ui_hand, 'still', 1, [10], 150, (1.4, 1.6, 0.65), 0.0, 1),
    ('ui.trophy', ui_trophy, 'still', 1, [12], 180, (2.2, 2.2, 0.95), 0.0, 1),
    ('icon.bust', lambda: soldier(turn=math.pi - 0.5), 'still', 1, [8], 420, (1.3, 1.3, 0.78), 0.0, 1),
]
for _w in ('musket', 'twin', 'scatter', 'lance', 'rocket', 'flame', 'coil', 'mortar', 'frost'):     # the gun alone, over soldier.march
    JOBS.append(('wpn.' + _w, lambda _w=_w: soldier(weapon=_w, overlay=True), 'march', 8, UE, 100, (1.3, 1.9, 0.55), 0.15, 1))
for _a in BOSS_ATTACKS:
    JOBS.append(('boss', lambda _a=_a: ogre(anim=_a), _a, 8, [38, 20], 52, ATTACK_VIEW, -0.4, 1))
JOBS += [
    ('giant', lambda: brute(giant=True), 'march', 8, UE, 84, (5.0, 7.6, 2.2), 0.0, 1),
    ('archer', lambda: archer('idle'), 'idle', 8, UE, 84, (2.2, 3.0, 0.85), 0.0, 1),
    ('archer', lambda: archer('shoot'), 'shoot', 8, UE, 84, (2.2, 3.0, 0.85), 0.0, 1),
    ('catapult', lambda: catapult('idle'), 'idle', 1, UE, 64, (3.6, 7.6, 2.3), 0.0, 1),
    ('catapult', lambda: catapult('fire'), 'fire', 8, UE, 64, (3.6, 7.6, 2.3), 0.0, 1),
    ('haz.mine', mine, 'still', 2, UE, 90, (1.4, 1.2, 0.25), 0.0, 1),
    ('gate.rail', gate_rail, 'still', 1, UE, 84, (1.3, 1.5, 0.1), 0.0, 1),
    ('cage.back', lambda: cage('back'), 'still', 1, UE, 84, (2.6, 4.4, 1.3), 0.0, 1),
    ('cage', lambda: cage('front'), 'still', 1, UE, 84, (2.6, 4.4, 1.3), 0.0, 1),
    ('cage.prisoner', lambda: soldier(turn=math.pi, weapon=None, wave=True), 'wave', 8, UE, 100, (1.3, 2.2, 0.7), 0.0, 1),
    ('fx.boulder', boulder, 'play', 8, [35], 64, (1.8, 1.8, 1.0), 0.0, 1),
]
JOBS += [
    ('shieldman', shieldman, 'march', 8, UE, 84, (1.5, 2.3, 0.75), 0.0, 1),
    ('shield.front', shield_front, 'still', 3, UE, 84, (1.5, 2.3, 0.75), 0.0, 1),
    ('bomber', bomber, 'march', 8, UE, 84, (1.5, 2.6, 0.85), 0.0, 1),
    ('drummer', drummer, 'march', 8, UE, 84, (1.6, 2.3, 0.75), 0.0, 1),
    ('squad.cannon', field_cannon, 'march', 8, UE, 100, (1.8, 3.0, 0.6), 0.1, 1),
    ('fx.cannonball', cannonball, 'play', 8, [35], 80, (1.0, 1.0, 0.6), 0.0, 1),
    ('fx.recruit_fly', recruit_fly, 'play', 8, [35], 64, (1.9, 1.9, 1.2), 0.0, 1),
    ('fx.parachute', parachute, 'play', 8, [30], 64, (2.4, 3.4, 1.75), 0.0, 1),
]
for _k in ('add', 'mul', 'neg', 'wpn'):
    JOBS.append(('gate.post_%s_w' % _k, lambda _k=_k: gate_post(_k, wheels=True), 'still', 1, UE, 84, (1.8, 3.6, 1.4), 0.0, 1))
# 1 GREENMEADOW
P('w1.tree_a', lambda: tree_round((0x4FB04A, 0x3C9440, 0x7FD060), fruit=(RED,))); P('w1.tree_b', poplar, T); P('w1.tree_c', blossom)
P('w1.bush', bush, S); P('w1.rock', rock, S); P('w1.flowers', flowers, S); P('w1.haystack', haystack, (2.4, 2.8, 0.8))
P('w1.cottage', cottage, (4.4, 4.6, 1.3)); P('w1.windmill', windmill, L, 8)
EDGE(1, (BLUE, WHITE, 'ball'), (BLUE, WHITE))
# 2 SUNBAKED
P('w2.palm', palm, (4.6, 6.0, 2.2)); P('w2.cactus', cactus, (2.6, 3.4, 1.0)); P('w2.tent', tent, (4.0, 5.0, 1.6)); P('w2.arch', arch, (4.6, 5.6, 1.8))
P('w2.pots', pots, S); P('w2.rock', lambda: rock(0xD9A860, 0xC08E48, None), S)
EDGE(2, (0xB8803E, 0xFFEFC4, 'point', 0xE0C088), (0xE0338F, 0xFFEFC4)); ADDON(2, a_turban)
# 3 FROSTVALE
P('w3.fir', lambda: fir(True), (3.6, 6.4, 2.4)); P('w3.snowman', snowman, (2.8, 3.8, 1.3)); P('w3.crystal', lambda: crystal((0xBFE8FF, 0x8FD0F8, 0xE6FAFF), 0.6), (2.6, 3.2, 0.9))
P('w3.cabin', lambda: cottage(0x9A6A44, 0x6E4630, 0xD63A26, snow=True), (4.4, 4.6, 1.3)); P('w3.rock', lambda: rock(0x9EAEC0, 0x8698AE, 0xD4DEEA), S)
P('w3.bush', lambda: bush((0xD4DEEA, 0xBCCCDE, 0xDCE6F0), None), S)
EDGE(3, (0x5FA8D8, WHITE, 'lantern'), (0x5FC8EA, WHITE)); ADDON(3, a_fur)
# 4 EMBERFALL
P('w4.maple_a', lambda: tree_round((0xD0402A, 0xB02A1E, 0xEE7A34))); P('w4.maple_b', lambda: tree_round((0xE0962A, 0xCC7420, 0xF0B84A)))
P('w4.pumpkins', pumpkins, S); P('w4.scarecrow', scarecrow, (2.8, 4.0, 1.4)); P('w4.haystack', haystack, (2.4, 2.8, 0.8))
P('w4.barn', lambda: cottage(0xB8402E, 0x4A3A3A, 0xF2E9D0), (4.4, 4.6, 1.3)); P('w4.bush', lambda: bush((0xCC7420, 0xB84A1E, 0xE0962A), (0xFFE04A,)), S)
EDGE(4, (WOOD, WOOD_D, 'point', 0xB8A090), (0xFF7A2A, 0xFFF0D0)); ADDON(4, a_embers)
# 5 TIDEREACH
P('w5.palm', lambda: palm((0x3CB86A, 0x58CC78)), (4.6, 6.0, 2.2)); P('w5.umbrella', umbrella, (3.6, 4.0, 1.3)); P('w5.lighthouse', lighthouse, (3.4, 8.4, 3.0))
P('w5.boat', boat, (4.4, 5.0, 1.5)); P('w5.rock', lambda: rock(0xA6AEB8, 0x8E97A3, None), S); P('w5.grass', lambda: reeds(0x8FBF5A, 0xE8D29A), (2.2, 3.0, 0.9))
EDGE(5, (WHITE, 0x2EC4C0, 'buoy', 0xE8DCC0), (0x1EA8C8, WHITE)); ADDON(5, a_crystals)
# 6 BLOOMGROVE
P('w6.cherry', blossom); P('w6.wisteria', lambda: tree_round((0x9A6ADC, 0x7E4EC8, 0xC8A6F2), hang=(0xB48AE8, 0xD8C0F8)))
P('w6.tulip_a', tulip, (2.2, 3.6, 1.2)); P('w6.tulip_b', lambda: tulip(0xFFD23A), (2.2, 3.6, 1.2)); P('w6.beehive', beehive, (2.2, 2.8, 0.9))
P('w6.bush', lambda: bush((0x5CB85A, 0x46A04E, 0x8AD870), (0xFF8FB8, WHITE, 0xFF4A8A)), S); P('w6.flowers', flowers, S)
EDGE(6, (0xFF8FB8, WHITE, 'flower'), (0xFF6FA0, WHITE)); ADDON(6, a_flowers)
# 7 MIREFEN
P('w7.mangrove', mangrove, (4.8, 5.6, 2.0)); P('w7.mushroom_a', mushroom, (3.2, 3.8, 1.2)); P('w7.mushroom_b', lambda: mushroom(0x3CE0C0, 0xC8FFF0, 2.5, 0.9), (3.2, 3.0, 0.9))
P('w7.reeds', reeds, (2.2, 3.0, 0.9)); P('w7.hut', lambda: cottage(0x7E7458, 0x4E6A4A, 0x3A2E22, stilts=1.1), (4.4, 5.6, 1.8)); P('w7.rock', lambda: rock(0x7E8478, 0x6A7066, 0x4E8A5E), S)
EDGE(7, (0x5E5240, 0x4E4A38, 'flame', 0x6A7066), (0x3F8A5A, 0xD8E8B0)); ADDON(7, a_mushrooms)
# 8 SKYHOLD
P('w8.column', column, (2.4, 5.4, 2.0)); P('w8.ruin', lambda: column(True), (3.0, 3.4, 1.0)); P('w8.statue', statue, (2.6, 4.8, 1.7)); P('w8.cloud_tree', cloud_tree, (3.8, 5.0, 1.8))
P('w8.airship', airship, (5.6, 8.0, 3.0)); P('w8.puff', lambda: bush((0xCCD4E0, 0xB8C6DC, 0xD4DCE8), None), S)
EDGE(8, (0xC8D0DC, GOLD, 'ball', 0xBCC4D0), (0xD0D8E4, GOLD)); ADDON(8, a_crown)
# 9 CINDERPEAK
P('w9.charred', charred, (3.4, 5.0, 1.8)); P('w9.vent', vent, (3.6, 3.2, 0.9)); P('w9.obsidian', lambda: crystal((0x2A2238, 0x3A2E4E, 0x1E1828), 0, 6, 3, 2.2), (2.6, 3.6, 1.1))
P('w9.lava_crystal', lambda: crystal((0xFF7A1A, 0xFFB03A, 0xFF4A10), 5.0, 5, 8, 1.3), (2.6, 2.8, 0.8)); P('w9.brazier', brazier, (2.0, 3.4, 1.2)); P('w9.rock', lambda: rock(0x4A434E, 0x3A343E, None), S)
EDGE(9, (0x3A343E, 0x8E1A1A, 'skull', 0x2E2830), (0x2A2228, 0xFF5A1E, 0xA8141F)); ADDON(9, a_flames)
# 10 LANTERNFAIR
P('w10.lantern_tree', lambda: tree_round((0x3E9E7A, 0x2E8466, 0x62C298), fruit=(0xFF4A4A, 0xFFC83A, 0xFF8FB8), fruit_r=0.15, fruit_emit=4.0))
P('w10.stall', stall, (4.0, 4.0, 1.3)); P('w10.tent_a', lambda: tent(0xE8413A, WHITE), (4.0, 5.0, 1.6)); P('w10.tent_b', lambda: tent(0x2F6BFF, 0xFFD23A, 0xE8413A), (4.0, 5.0, 1.6))
P('w10.balloons', balloons, (2.6, 4.6, 1.6)); P('w10.ferris', ferris, XL, 8); P('w10.bush', lambda: bush((0x3E9E7A, 0x2E8466, 0x62C298), (0xFF4A4A, 0xFFC83A)), S)
EDGE(10, (0xE8413A, 0xFFD23A, 'lantern'), (0x9B4DFF, 0xFFD23A)); ADDON(10, a_lanterns)

for kind, build, anim, frames, elevs, ppu, (w, h, cz), yc, scale in JOBS:
    if not want(kind, anim):
        continue
    reset()
    res = build()
    pose, points = None, None
    if isinstance(res, tuple):
        _, pose, points = res
    elif callable(res):
        pose = res
    if scale != 1:
        top = E(scale=scale)
        for o in list(bpy.data.objects):
            if o.parent is None and o is not top and o.type in ('MESH', 'EMPTY'):
                o.parent = top
    render(kind, anim, frames, elevs, ppu, w, h, cz, pose, points, yc)
print('done')
