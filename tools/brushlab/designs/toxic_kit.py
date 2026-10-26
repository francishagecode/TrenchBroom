"""Toxic facility kit: snap-together brutalist/metal pieces for an alien ship.

Lego rules, so pieces line up on TrenchBroom's grid without fiddling:
  - Module grid is 128 x 128 (half modules of 64 for small parts). Snap to 32+.
  - The walkable floor is z = 0; the catwalk deck is flush with it (z = -8..0),
    everything else stands on it. One storey is 128 tall.
  - Every piece's footprint starts at (0, 0). Wall pieces are 32 deep with their face
    toward -y. Rotate in 90 degree steps.

Each variant is one piece; `snap_test` puts them together to prove they tile.
"""

from brushlab import *

TITLE = "Toxic facility kit"
TAGS = ["sci-fi", "brutalist", "kit", "modular", "hazard"]

PARAMS = dict(
    piece="wall_panel",
    module=128,      # T: floor/wall module
    storey=128,      # H: wall height
    wall=32,         # wall depth
)


# ---------------------------------------------------------------------------------------
# Pieces. Each returns (groups, entities, footprint) in local coordinates.
# ---------------------------------------------------------------------------------------


def chamfer_square(cx, cy, size, ch):
    h = size / 2
    return [(cx - h + ch, cy - h), (cx + h - ch, cy - h), (cx + h, cy - h + ch), (cx + h, cy + h - ch),
            (cx + h - ch, cy + h), (cx - h + ch, cy + h), (cx - h, cy + h - ch), (cx - h, cy - h + ch)]


def top_band(x0, x1, p):
    """Chamfered concrete cap along the top of a wall face."""
    H = p.storey
    pts = []
    for x in (x0, x1):
        pts += [(x, 8, H - 24), (x, 0, H - 24), (x, 0, H - 12), (x, 8, H), (x, p.wall, H), (x, p.wall, H - 24)]
    return hull(pts, "concrete")


def wall_frame(p, recess_material="concrete"):
    """Core slab with a plinth, side stiles and a chamfered top band; the middle is an
    8-unit recess. Stiles of neighbouring panels pair up into pilasters."""
    T, H, W = p.module, p.storey, p.wall
    core = box((0, 8, 0), (T, W, H - 24), "concrete")
    if recess_material != "concrete":
        core = core.with_face_material(lambda f: f.normal[1] < -0.9, recess_material)
    return {
        "Core": [core],
        "Frame": [box((0, 0, 0), (T, 8, 16), "hull"),
                  box((0, 0, 16), (12, 8, H - 24), "concrete"),
                  box((T - 12, 0, 16), (T, 8, H - 24), "concrete"),
                  top_band(0, T, p)],
    }


def wall_panel(p):
    g = wall_frame(p)
    g["Frame"].append(box((12, 4, 60), (p.module - 12, 8, 68), "hull"))  # metal seam band
    return g, [], (p.module, p.wall)


def wall_vent(p):
    g = wall_frame(p, recess_material="grate")
    louvres = []
    for z0 in range(24, int(p.storey) - 28, 10):
        pts = []
        for x in (12, p.module - 12):
            pts += [(x, 8, z0), (x, 8, z0 + 3), (x, 2, z0 - 3), (x, 2, z0)]
        louvres.append(hull(pts, "hull"))
    g["Louvres"] = louvres
    return g, [], (p.module, p.wall)


def column(p):
    H = p.storey
    c = 32
    parts = [
        prism(chamfer_square(c, c, 56, 10), 0, 16, "concrete"),
        prism(chamfer_square(c, c, 40, 8), 16, 40, "concrete"),
        prism(chamfer_square(c, c, 44, 8), 40, 48, "hull"),
        prism(chamfer_square(c, c, 40, 8), 48, H - 24, "concrete"),
        hull([(x, y, H - 24) for x, y in chamfer_square(c, c, 40, 8)]
             + [(x, y, H) for x, y in chamfer_square(c, c, 64, 12)], "concrete"),
    ]
    return {"Column": parts}, [], (64, 64)


def catwalk(p):
    """Grated catwalk segment, flush with the floor, railings on both long sides."""
    T = p.module
    parts = {"Deck": [box((0, 0, -8), (T, 64, 0), "grate"),
                      box((0, 4, -20), (T, 12, -8), "hull"), box((0, 52, -20), (T, 60, -8), "hull")],
             "Railings": []}
    for y in (4, 60):
        for x in (8, T / 2, T - 8):
            parts["Railings"].append(block((x, y, 20), (3, 3, 40), "hazard"))
        parts["Railings"] += [segment((0, y, 38.5), (T, y, 38.5), 3, 3, "hazard"),
                              segment((0, y, 20), (T, y, 20), 2, 2, "hazard")]
    return parts, [], (T, 64)


PIECES = {f.__name__: f for f in (wall_panel, wall_vent, column, catwalk)}

VARIANTS = {name: dict(piece=name) for name in PIECES}
VARIANTS["snap_test"] = dict(piece="snap_test")


# ---------------------------------------------------------------------------------------
# Assembly
# ---------------------------------------------------------------------------------------


def place(d, p, name, dx, dy, rot=0, label=None):
    """Adds a piece rotated by `rot` (multiple of 90) with its footprint's min corner at
    (dx, dy). Each placed piece becomes its own group, like a Lego brick."""
    groups, entities, (w, h) = PIECES[name](p)
    rot %= 360
    rw, rh = (w, h) if rot in (0, 180) else (h, w)
    pivot = (w / 2, h / 2, 0)
    tx, ty = dx + rw / 2 - w / 2, dy + rh / 2 - h / 2
    label = label or name
    for part, brushes in groups.items():
        d.add([b.rotate_z(rot, pivot).translate(tx, ty) for b in brushes], f"{label}/{part}")
    for classname, origin, props in entities:
        x, y, z = rotate_point_z(origin, rot, pivot)
        d.entity(classname, (round(x + tx, 3), round(y + ty, 3), z), group=label, **props)


def snap_test(d, p):
    """A wall run of panels and a vent with columns standing on the panel joints, and a
    catwalk of three segments running alongside it."""
    T = p.module
    n = 0

    def put(name, x, y, rot=0):
        nonlocal n
        n += 1
        place(d, p, name, x, y, rot, label=f"{name} {n}")

    for i, name in enumerate(("wall_panel", "wall_vent", "wall_panel", "wall_panel")):
        put(name, i * T, 0)
    for joint in (T, 3 * T):
        put("column", joint - 32, -96)
    for i in range(3):
        put("catwalk", T / 2 + i * T, -224)
    d.entity("info_player_start", (T, -192, 24), angle=90)
    d.note("panel stiles pair into pilasters at the joints; catwalk rails run continuously")


def build(p):
    d = Design()
    if p.piece == "snap_test":
        snap_test(d, p)
    else:
        place(d, p, p.piece, 0, 0)
    d.note("railings, flanges and drum bands overlap their parts on purpose: func_detail them")
    return d
