"""Sealed room shells with different footprints, doors and windows.

Each variant is a different footprint. The interior keeps the exact footprint size;
walls, floor and ceiling are built outside it, so rooms snap together on the grid.
"""

from brushlab import *

TITLE = "Room shapes"
TAGS = ["room", "structure"]

PARAMS = dict(
    shape="rect",       # rect | l | t | u | octagon | cross
    width=384,
    depth=320,
    arm=160,            # arm width for l / t / u / cross shapes
    height=160,
    wall=16,
    door_width=64,
    door_height=112,
    arched_doors=False,
    windows=True,
)

VARIANTS = {
    "rect_hall": dict(shape="rect", width=512, depth=256),
    "l_room": dict(shape="l", width=384, depth=384),
    "t_junction": dict(shape="t", width=448, depth=384, arched_doors=True),
    "u_court": dict(shape="u", width=448, depth=384, height=192),
    "octagon_chapel": dict(shape="octagon", width=384, height=224, arched_doors=True),
    "cross_crypt": dict(shape="cross", width=448, depth=448, height=144, windows=False),
}


def footprint(p):
    w, d, a = p.width, p.depth, p.arm
    if p.shape == "rect":
        return rect(0, 0, w, d)
    if p.shape == "l":
        return l_shape(w, d, a, a)
    if p.shape == "t":
        x0 = (w - a) / 2
        return [(x0, 0), (x0 + a, 0), (x0 + a, d - a), (w, d - a), (w, d), (0, d), (0, d - a), (x0, d - a)]
    if p.shape == "u":
        return [(0, 0), (w, 0), (w, d), (w - a, d), (w - a, a), (a, a), (a, d), (0, d)]
    if p.shape == "cross":
        x0, y0 = (w - a) / 2, (d - a) / 2
        return [(x0, 0), (x0 + a, 0), (x0 + a, y0), (w, y0), (w, y0 + a), (x0 + a, y0 + a),
                (x0 + a, d), (x0, d), (x0, y0 + a), (0, y0 + a), (0, y0), (x0, y0)]
    if p.shape == "octagon":
        r = p.width / 2
        return regular_polygon((r, r), r / math.cos(math.pi / 8), 8)
    raise ValueError(f"unknown shape {p.shape}")


def build(p):
    fp = footprint(p)
    n = len(fp)
    # A door in the first edge, plus windows on every long wall that has room for one.
    openings = [Opening(0, width=p.door_width, height=p.door_height, arched=p.arched_doors)]
    if p.windows:
        for i in range(1, n):
            (ax, ay), (bx, by) = fp[i], fp[(i + 1) % n]
            if math.hypot(bx - ax, by - ay) >= 192:
                openings.append(Opening(i, width=48, height=48, sill=56, arched=p.arched_doors))

    d = Design()
    d.add(room(fp, height=p.height, wall=p.wall, openings=openings), "Room")

    # One light per convex part of the footprint, roughly in its middle.
    for piece in convex_decompose_2d(fp):
        cx = sum(x for x, _ in piece) / len(piece)
        cy = sum(y for _, y in piece) / len(piece)
        d.entity("light", (round(cx), round(cy), p.height - 32), light=250)
    d.note(f"{n}-sided footprint, {len(openings)} openings")
    return d
