"""Toxic waste containment bay aboard an alien ship: brutalist concrete and metal.

A heavy chamfered hall with a sunken containment pit of sludge. You enter through blast
doors in the south (and north) walls and cross the pit on a grated catwalk with hazard
railings. Leaning concrete buttresses line the walls, deep beams cross the ceiling, and
feed pipes drop from the ceiling into the sludge. Sludge lights glow green from the pit.

Parameters:
  size / length     hall interior (X / Y), chamfer cuts the corners
  height            floor to ceiling
  pit               "octagon" (central vat with a column platform) or "trench" (a
                    sludge channel running east-west, crossed by bridges)
  pit_radius        octagon pit apothem; trench_width for the trench
  pit_depth         how far the pit drops below the floor; sludge fills `sludge` of it
  catwalks          "ns" (north-south only) or "cross" (adds east-west walks)
"""

from brushlab import *

TITLE = "Toxic vat bay"
TAGS = ["sci-fi", "brutalist", "industrial", "room", "hazard"]

PARAMS = dict(
    size=768,
    length=768,
    chamfer=128,
    height=320,
    wall=32,
    pit="octagon",
    pit_radius=208,
    trench_width=192,
    pit_depth=128,
    sludge=72,
    catwalks="ns",
    catwalk_width=64,
    rail_height=40,
    buttress_spacing=128,
    beam_spacing=128,
    pipes=4,
    door_width=128,
    door_height=128,
)

VARIANTS = {
    "vat_bay": {},
    "cross_walks": dict(size=896, length=896, pit_radius=256, pit_depth=192, sludge=96,
                        catwalks="cross", pipes=8, height=384),
    "trench": dict(size=640, length=1024, pit="trench", pipes=5, height=288),
}


def inward(a, b):
    """Unit direction along edge a->b and the inward normal of a CCW footprint."""
    ln = math.hypot(b[0] - a[0], b[1] - a[1])
    ux, uy = (b[0] - a[0]) / ln, (b[1] - a[1]) / ln
    return (ux, uy), (-uy, ux), ln


def buttress(a, b, t, width, base_depth, top_depth, height):
    """A concrete buttress leaning against the inner face of wall a->b, centered t units
    along it: deep at the floor, shallow at the top."""
    (ux, uy), (nx, ny), _ = inward(a, b)
    cx, cy = a[0] + ux * t, a[1] + uy * t
    pts = []
    for s in (-width / 2, width / 2):
        x, y = cx + ux * s, cy + uy * s
        pts += [(x, y, 0), (x, y, height),
                (x + nx * base_depth, y + ny * base_depth, 0),
                (x + nx * top_depth, y + ny * top_depth, height)]
    return hull(pts, "concrete")


def railing(x0, y0, x1, y1, z, h):
    """Posts and two rails along a straight line at deck height z."""
    parts = []
    ln = math.hypot(x1 - x0, y1 - y0)
    n = max(1, int(ln // 48))
    for i in range(n + 1):
        t = i / n
        px, py = x0 + (x1 - x0) * t, y0 + (y1 - y0) * t
        parts.append(block((px, py, z + h / 2), (3, 3, h), "hazard"))
    for rz, th in ((z + h - 3, 3), (z + h / 2 - 1, 2)):
        parts.append(segment((x0, y0, rz), (x1, y1, rz), th, th, "hazard"))
    return parts


def walkway(d, x0, y0, x1, y1, p, name):
    """A grated deck flush with the floor (z = -8..0) with railings on its long sides."""
    d.add(box((x0, y0, -8), (x1, y1, 0), "grate"), f"{name}/Deck")
    if x1 - x0 >= y1 - y0:
        for y in (y0 + 2, y1 - 2):
            d.add(railing(x0 + 2, y, x1 - 2, y, 0, p.rail_height), f"{name}/Railings")
    else:
        for x in (x0 + 2, x1 - 2):
            d.add(railing(x, y0 + 2, x, y1 - 2, 0, p.rail_height), f"{name}/Railings")


def build(p):
    W, L, c, H = p.size, p.length, p.chamfer, p.height
    cx, cy = W / 2, L / 2
    fp = [(c, 0), (W - c, 0), (W, c), (W, L - c), (W - c, L), (c, L), (0, L - c), (0, c)]
    doors = [0, 4] + ([2, 6] if p.catwalks == "cross" else [])
    d = Design()

    # The shell: a thick floor slab that the pit is carved into, plain walls and ceiling.
    shell = room(fp, height=H, wall=p.wall, floor=p.pit_depth + 16, ceiling=32,
                 openings=[Opening(e, width=p.door_width, height=p.door_height) for e in doors],
                 materials={"floor": "metal", "wall": "concrete", "ceiling": "concrete"})
    d.add(shell, "Bay")

    # The pit and the walkways across it.
    hw = p.catwalk_width / 2
    if p.pit == "octagon":
        R = p.pit_radius
        pit = regular_polygon((cx, cy), R / math.cos(math.pi / 8), 8)
        plat = 64
        spans = [(cx - hw, cy - R, cx + hw, cy - plat), (cx - hw, cy + plat, cx + hw, cy + R)]
        if p.catwalks == "cross":
            spans += [(cx - R, cy - hw, cx - plat, cy + hw), (cx + plat, cy - hw, cx + R, cy + hw)]
        # Pipes sit between the walkways (offset half a step from the axes); the sludge
        # lights go between the pipes.
        def ring_spots(n, offset, r):
            return [(cx + r * math.cos(math.radians(offset + k * 360 / n)),
                     cy + r * math.sin(math.radians(offset + k * 360 / n))) for k in range(n)]
        pipe_spots = ring_spots(p.pipes, 180 / p.pipes, R * 0.72)
        glow_spots = ring_spots(p.pipes, 0, R * 0.55)
    else:
        tw = p.trench_width
        pit = rect(0, cy - tw / 2, W, cy + tw / 2)
        spans = [(x - hw, cy - tw / 2, x + hw, cy + tw / 2) for x in (W / 3, 2 * W / 3)]
        pipe_spots = [(W * (k + 0.5) / p.pipes, cy) for k in range(p.pipes)]
        pipe_spots = [s for s in pipe_spots if all(abs(s[0] - x) > hw + 24 for x in (W / 3, 2 * W / 3))]
        glow_spots = [(W * k / 6, cy + tw / 4 * (1 if k % 2 else -1)) for k in range(1, 6)]

    bottom = -p.pit_depth
    pit_cutter = prism(pit, bottom, 1)
    d.carve(pit_cutter, groups=["Bay/floor"], cap_material="concrete")
    sludge_top = bottom + p.sludge
    sludge = slab(pit, bottom, sludge_top, "slime")

    for i, (x0, y0, x1, y1) in enumerate(spans):
        walkway(d, x0, y0, x1, y1, p, f"Vat/Catwalk {i + 1}")

    if p.pit == "octagon":
        # Central platform on a flared brutalist column standing in the sludge.
        d.add(prism(regular_polygon((cx, cy), plat / math.cos(math.pi / 8), 8), -16, 0,
                    {"top": "grate", "default": "hazard"}), "Vat/Platform")
        column = [cylinder((cx, cy, bottom), 40, -48 - bottom, 8, "concrete"),
                  frustum((cx, cy, -48), 40, plat, 32, 8, "concrete")]
        d.add(column, "Vat/Platform")
        for b in column:
            sludge = subtract_all(sludge, b, "slime")
        for k in range(4):  # railing around the platform, open where walkways land
            a0 = math.radians(45 + 90 * k + 18)
            a1 = math.radians(45 + 90 * k + 72)
            r = plat - 4
            d.add(railing(cx + r * math.cos(a0), cy + r * math.sin(a0),
                          cx + r * math.cos(a1), cy + r * math.sin(a1), 0, p.rail_height),
                  "Vat/Platform")
        d.entity("light", (cx, cy, 72), group="Vat/Platform", light=200, _color="1 0.9 0.7")
    d.add(sludge, "Vat/Sludge")

    # Hazard curb around the pit edge, open where walkways start.
    curb = []
    for i in range(len(pit)):
        a, b = pit[i], pit[(i + 1) % len(pit)]
        if p.pit == "trench" and a[0] == b[0]:
            continue  # the trench's short ends run into the walls
        (ux, uy), (nx, ny), ln = inward(a, b)
        o = 12  # the curb sits outside the pit edge
        q = [a, b, (b[0] - nx * o, b[1] - ny * o), (a[0] - nx * o, a[1] - ny * o)]
        curb.append(prism(q, 0, 8, "hazard"))
    for x0, y0, x1, y1 in spans:
        curb = subtract_all(curb, box((x0, y0 - 16, -1), (x1, y1 + 16, 9)), "hazard")
        curb = subtract_all(curb, box((x0 - 16, y0, -1), (x1 + 16, y1, 9)), "hazard")
    d.add(curb, "Vat/Curb")

    # Feed pipes from the ceiling into the sludge, with flanges.
    for i, (px, py) in enumerate(pipe_spots):
        d.add(cylinder((px, py, sludge_top), 16, H - sludge_top, 12, "metal"), "Vat/Pipes")
        z = 32.0
        while z < H - 32:
            d.add(cylinder((px, py, z), 20, 6, 12, "hull"), "Vat/Pipes")
            z += 96
    for gx, gy in glow_spots:
        d.entity("light", (round(gx), round(gy), round(sludge_top + 24)), group="Vat/Sludge",
                 light=150, _color="0.45 1 0.3")

    # Leaning buttresses along the long walls, kept clear of doors and corners.
    for e in range(0, 8, 2):
        a, b = fp[e], fp[e + 1]
        _, _, ln = inward(a, b)
        t = p.buttress_spacing / 2
        while t < ln - p.buttress_spacing / 4:
            near_door = e in doors and abs(t - ln / 2) < p.door_width / 2 + 40
            if not near_door:
                piece = buttress(a, b, t, 32, 56, 12, H - 48)
                if not piece.overlaps(pit_cutter):  # nothing to stand on over the pit
                    d.add(piece, "Bay/Buttresses")
            t += p.buttress_spacing

    # Deep beams under the ceiling, trimmed to the chamfered footprint.
    clip = prism(fp, H - 48, H)
    y = p.beam_spacing / 2
    while y < L:
        beam = box((0, y - 16, H - 48), (W, y + 16, H), "concrete").intersect(clip)
        if beam is not None:
            d.add(beam, "Bay/Beams")
        y += p.beam_spacing
    d.hide_in_cutaway("Bay/Beams")

    # Door frames: hazard-striped jambs and a heavy lintel on the inside of each door.
    for e in doors:
        a, b = fp[e], fp[e + 1]
        (ux, uy), (nx, ny), ln = inward(a, b)
        mx, my = (a[0] + b[0]) / 2, (a[1] + b[1]) / 2
        half = p.door_width / 2
        for s0, s1, z0, z1 in ((-half - 24, -half, 0, p.door_height + 24),
                               (half, half + 24, 0, p.door_height + 24),
                               (-half - 24, half + 24, p.door_height, p.door_height + 24)):
            pts = []
            for s in (s0, s1):
                for dd in (0, 12):
                    x, y2 = mx + ux * s + nx * dd, my + uy * s + ny * dd
                    pts += [(x, y2, z0), (x, y2, z1)]
            d.add(hull(pts, "hazard"), "Bay/Door frames")

    for lx, ly in ((W / 4, L / 4), (3 * W / 4, L / 4), (W / 4, 3 * L / 4), (3 * W / 4, 3 * L / 4)):
        d.entity("light", (round(lx), round(ly), H - 72), group="Bay", light=180, _color="0.8 0.9 1")
    d.entity("info_player_start", (cx, 40, 24), angle=90)

    d.note("map 'slime' to your liquid texture (e.g. *slime0) in materials.json")
    d.note("railings and flanges overlap their decks/pipes on purpose: make them func_detail")
    return d
