"""Shape primitives and architectural building blocks.

Conventions: Quake units, Z is up, a player is 32 x 32 x 56 units, and doors are
usually 64-128 wide and 96-128 tall. All functions return a Brush or a list of Brushes.
Angles are in degrees, counter-clockwise when viewed from above.
"""

from __future__ import annotations

import math
from typing import Dict, List, Optional, Sequence, Tuple

from .brush import Brush, MaterialSpec, rotate_point_z, subtract_all
from .geometry import (
    Vec2,
    Vec3,
    convex_decompose_2d,
    ensure_ccw,
    offset_polygon_2d,
)

# ---------------------------------------------------------------------------------------
# Primitives
# ---------------------------------------------------------------------------------------


def box(mins: Vec3, maxs: Vec3, material: MaterialSpec = "wall") -> Brush:
    """Axis-aligned box from two opposite corners (in any order)."""
    lo = tuple(min(a, b) for a, b in zip(mins, maxs))
    hi = tuple(max(a, b) for a, b in zip(mins, maxs))
    pts = [(x, y, z) for x in (lo[0], hi[0]) for y in (lo[1], hi[1]) for z in (lo[2], hi[2])]
    return Brush(pts, material)


def block(center: Vec3, size: Vec3, material: MaterialSpec = "wall") -> Brush:
    """Axis-aligned box from its center and size."""
    h = [s / 2.0 for s in size]
    return box(
        (center[0] - h[0], center[1] - h[1], center[2] - h[2]),
        (center[0] + h[0], center[1] + h[1], center[2] + h[2]),
        material,
    )


def hull(points: Sequence[Vec3], material: MaterialSpec = "wall") -> Brush:
    """Convex hull of arbitrary points: the most general way to make a brush."""
    return Brush(points, material)


def prism(polygon: Sequence[Vec2], z0: float, z1: float, material: MaterialSpec = "wall") -> Brush:
    """Extrudes a convex 2D polygon between two heights."""
    pts = [(x, y, z0) for x, y in polygon] + [(x, y, z1) for x, y in polygon]
    return Brush(pts, material)


def slab(polygon: Sequence[Vec2], z0: float, z1: float, material: MaterialSpec = "wall") -> List[Brush]:
    """Extrudes any simple 2D polygon (concave is fine) as a few convex brushes."""
    return [prism(piece, z0, z1, material) for piece in convex_decompose_2d(polygon)]


def wedge(mins: Vec3, maxs: Vec3, high_side: str = "+x", material: MaterialSpec = "wall") -> Brush:
    """A ramp filling the box: full height at `high_side` (+x, -x, +y, -y), zero height
    on the opposite side."""
    (x0, y0, z0), (x1, y1, z1) = mins, maxs
    base = [(x0, y0, z0), (x1, y0, z0), (x0, y1, z0), (x1, y1, z0)]
    top = {
        "+x": [(x1, y0, z1), (x1, y1, z1)],
        "-x": [(x0, y0, z1), (x0, y1, z1)],
        "+y": [(x0, y1, z1), (x1, y1, z1)],
        "-y": [(x0, y0, z1), (x1, y0, z1)],
    }[high_side]
    return Brush(base + top, material)


def circle_points(center: Vec2, radius: float, sides: int, start_deg: float = 0.0,
                  radius_y: Optional[float] = None) -> List[Vec2]:
    ry = radius if radius_y is None else radius_y
    out = []
    for i in range(sides):
        a = math.radians(start_deg + 360.0 * i / sides)
        out.append((center[0] + radius * math.cos(a), center[1] + ry * math.sin(a)))
    return out


def cylinder(center: Vec3, radius: float, height: float, sides: int = 12,
             material: MaterialSpec = "wall", start_deg: Optional[float] = None) -> Brush:
    """Vertical cylinder standing on `center` (center is the middle of the bottom cap).
    By default it is rotated so that flat sides face the axes."""
    if start_deg is None:
        start_deg = 180.0 / sides
    poly = circle_points((center[0], center[1]), radius, sides, start_deg)
    return prism(poly, center[2], center[2] + height, material)


def frustum(center: Vec3, r_bottom: float, r_top: float, height: float, sides: int = 12,
            material: MaterialSpec = "wall", start_deg: Optional[float] = None) -> Brush:
    """Tapered cylinder (or a cone if r_top is 0)."""
    if start_deg is None:
        start_deg = 180.0 / sides
    c2 = (center[0], center[1])
    pts = [(x, y, center[2]) for x, y in circle_points(c2, r_bottom, sides, start_deg)]
    if r_top <= 0:
        pts.append((center[0], center[1], center[2] + height))
    else:
        pts += [(x, y, center[2] + height) for x, y in circle_points(c2, r_top, sides, start_deg)]
    return Brush(pts, material)


def sphere(center: Vec3, radius: float, rings: int = 6, sides: int = 12,
           material: MaterialSpec = "wall") -> Brush:
    """A single-brush sphere approximation."""
    pts = [(center[0], center[1], center[2] - radius), (center[0], center[1], center[2] + radius)]
    for r in range(1, rings):
        phi = math.pi * r / rings - math.pi / 2
        z = center[2] + radius * math.sin(phi)
        rr = radius * math.cos(phi)
        pts += [(x, y, z) for x, y in circle_points((center[0], center[1]), rr, sides)]
    return Brush(pts, material)


def segment(a: Vec3, b: Vec3, width: float, height: float, material: MaterialSpec = "wall") -> Brush:
    """A beam of rectangular cross-section from a to b. The cross-section stays upright
    (its height is measured along Z where possible), so it works for rails and rafters."""
    from .geometry import cross, normalize, sub

    d = sub(b, a)
    fwd = normalize(d)
    up = (0.0, 0.0, 1.0)
    side = cross(fwd, up)
    if sum(c * c for c in side) < 1e-9:
        side = (1.0, 0.0, 0.0)
    side = normalize(side)
    up = normalize(cross(side, fwd))
    hw, hh = width / 2.0, height / 2.0
    pts = []
    for base in (a, b):
        for sx in (-hw, hw):
            for sz in (-hh, hh):
                pts.append(tuple(base[i] + side[i] * sx + up[i] * sz for i in range(3)))
    return Brush(pts, material)


# ---------------------------------------------------------------------------------------
# Sweeps and curves
# ---------------------------------------------------------------------------------------


def ring(center: Vec3, r_inner: float, r_outer: float, z0: float, z1: float,
         start_deg: float = 0.0, end_deg: Optional[float] = None, segments: int = 16,
         material: MaterialSpec = "wall") -> List[Brush]:
    """A curved wall / ring / tower segment between two radii, one brush per segment.
    `z0` and `z1` are absolute heights. Without `end_deg` the ring is closed."""
    if end_deg is None:
        end_deg = start_deg + 360.0
    out = []
    for i in range(segments):
        a0 = start_deg + (end_deg - start_deg) * i / segments
        a1 = start_deg + (end_deg - start_deg) * (i + 1) / segments
        pts = []
        for a in (a0, a1):
            r = math.radians(a)
            for rad in (r_inner, r_outer):
                x, y = center[0] + rad * math.cos(r), center[1] + rad * math.sin(r)
                pts += [(x, y, z0), (x, y, z1)]
        out.append(Brush(pts, material))
    return out


def sweep(profile: Sequence[Vec2], path: Sequence[Vec3], material: MaterialSpec = "wall") -> List[Brush]:
    """Sweeps a convex 2D profile (x = sideways, y = up) along a polyline path.

    Each path segment becomes one brush. Joints are mitered so neighbouring pieces meet
    without gaps. Good for railings, pipes, trims, curbs and winding walls.
    """
    from .geometry import cross, normalize, sub

    def frame(d: Vec3):
        fwd = normalize(d)
        side = cross(fwd, (0.0, 0.0, 1.0))
        if sum(c * c for c in side) < 1e-9:
            side = (1.0, 0.0, 0.0)
        side = normalize(side)
        up = normalize(cross(side, fwd))
        return side, up

    n = len(path)
    dirs = [sub(path[i + 1], path[i]) for i in range(n - 1)]
    joint_frames = []
    for i in range(n):
        if i == 0:
            s, u = frame(dirs[0])
            scale = 1.0
        elif i == n - 1:
            s, u = frame(dirs[-1])
            scale = 1.0
        else:
            s0, _ = frame(dirs[i - 1])
            s1, _ = frame(dirs[i])
            sm = normalize(tuple(s0[k] + s1[k] for k in range(3)))
            cosv = sum(sm[k] * s1[k] for k in range(3))
            s = sm
            u = normalize(cross(s, normalize(tuple(dirs[i - 1][k] + dirs[i][k] for k in range(3)))))
            scale = 1.0 / max(cosv, 0.2)
            if u[2] < 0:
                u = tuple(-c for c in u)
        joint_frames.append((s, u, scale))

    def ring_at(i: int):
        s, u, scale = joint_frames[i]
        return [
            tuple(path[i][k] + s[k] * px * scale + u[k] * py for k in range(3))
            for px, py in profile
        ]

    rings = [ring_at(i) for i in range(n)]
    return [Brush(rings[i] + rings[i + 1], material) for i in range(n - 1)]


def arc_path(center: Vec3, radius: float, start_deg: float, end_deg: float, segments: int,
             z_start: Optional[float] = None, z_end: Optional[float] = None) -> List[Vec3]:
    """Points along a horizontal arc, optionally rising from z_start to z_end (helix)."""
    zs = center[2] if z_start is None else z_start
    ze = zs if z_end is None else z_end
    pts = []
    for i in range(segments + 1):
        t = i / segments
        a = math.radians(start_deg + (end_deg - start_deg) * t)
        pts.append((center[0] + radius * math.cos(a), center[1] + radius * math.sin(a), zs + (ze - zs) * t))
    return pts


def arch(center: Vec3, span: float, thickness: float, depth: float, rise: Optional[float] = None,
         segments: int = 8, axis: str = "y", fill_to: Optional[float] = None,
         material: MaterialSpec = "stone") -> List[Brush]:
    """An arch made of voussoirs (wedge stones).

    `center` is the middle of the springing line (where the curve starts) at floor of the
    opening; `span` is the clear opening width; `thickness` is the band thickness; `depth`
    is how deep the arch is along `axis` ("x" or "y" is the direction you walk through).
    `rise` defaults to span / 2 (a semicircle); smaller values give a segmental/elliptic
    arch. If `fill_to` is given, spandrels are added so the arch fills a rectangle up to
    that absolute Z, which makes it easy to embed in a wall.
    """
    rise = span / 2.0 if rise is None else rise
    a_in, b_in = span / 2.0, rise
    a_out, b_out = a_in + thickness, rise + thickness

    def pt(s: float, v: float, w: float) -> Vec3:
        if axis == "y":
            return (center[0] + s, center[1] + w, center[2] + v)
        return (center[0] + w, center[1] + s, center[2] + v)

    out = []
    hd = depth / 2.0
    for i in range(segments):
        t0 = math.pi * i / segments
        t1 = math.pi * (i + 1) / segments
        pts = []
        for t in (t0, t1):
            for a, b in ((a_in, b_in), (a_out, b_out)):
                s, v = -a * math.cos(t), b * math.sin(t)
                pts += [pt(s, v, -hd), pt(s, v, hd)]
        out.append(Brush(pts, material))

    if fill_to is not None:
        # Spandrels: vertical strips between the outer curve and the top of the rectangle.
        top = fill_to - center[2]
        for i in range(segments):
            t0 = math.pi * i / segments
            t1 = math.pi * (i + 1) / segments
            s0, v0 = -a_out * math.cos(t0), b_out * math.sin(t0)
            s1, v1 = -a_out * math.cos(t1), b_out * math.sin(t1)
            if top - min(v0, v1) <= 0.5:
                continue
            pts = []
            for s, v in ((s0, v0), (s1, v1)):
                pts += [pt(s, v, -hd), pt(s, v, hd), pt(s, top, -hd), pt(s, top, hd)]
            out.append(Brush(pts, material))
    return out


# ---------------------------------------------------------------------------------------
# Stairs
# ---------------------------------------------------------------------------------------


def stairs(start: Vec3, direction: str, width: float, steps: int, step_height: float = 16.0,
           step_depth: float = 16.0, solid: bool = True, material: MaterialSpec = "stone") -> List[Brush]:
    """A straight flight. `start` is the bottom-front-center of the first step, and
    `direction` is one of +x, -x, +y, -y. Solid steps reach down to start Z; otherwise each
    step is a slab of one step height."""
    dx, dy = {"+x": (1, 0), "-x": (-1, 0), "+y": (0, 1), "-y": (0, -1)}[direction]
    out = []
    hw = width / 2.0
    for i in range(steps):
        d0, d1 = i * step_depth, (i + 1) * step_depth
        z1 = start[2] + (i + 1) * step_height
        z0 = start[2] if solid else z1 - step_height
        if dx:
            x0, x1 = start[0] + dx * d0, start[0] + dx * d1
            out.append(box((x0, start[1] - hw, z0), (x1, start[1] + hw, z1), material))
        else:
            y0, y1 = start[1] + dy * d0, start[1] + dy * d1
            out.append(box((start[0] - hw, y0, z0), (start[0] + hw, y1, z1), material))
    return out


def spiral_stairs(center: Vec3, r_inner: float, r_outer: float, steps: int,
                  step_height: float = 16.0, degrees_per_step: float = 22.5,
                  start_deg: float = 0.0, thickness: Optional[float] = None,
                  material: MaterialSpec = "stone") -> List[Brush]:
    """Wedge-shaped steps winding counter-clockwise around a center post. If `thickness`
    is None, each step reaches down to the floor."""
    out = []
    for i in range(steps):
        a0 = start_deg + i * degrees_per_step
        a1 = a0 + degrees_per_step
        z1 = center[2] + (i + 1) * step_height
        z0 = center[2] if thickness is None else max(center[2], z1 - thickness)
        pts = []
        for a in (a0, a1):
            r = math.radians(a)
            for rad in (r_inner, r_outer):
                x, y = center[0] + rad * math.cos(r), center[1] + rad * math.sin(r)
                pts += [(x, y, z0), (x, y, z1)]
        out.append(Brush(pts, material))
    return out


# ---------------------------------------------------------------------------------------
# Rooms
# ---------------------------------------------------------------------------------------


class Opening:
    """A hole to cut into the walls of a room.

    `edge` is the index of the footprint edge (edge i runs from point i to point i+1),
    `offset` is the distance of the opening's center from the start of that edge
    (None = the middle), `width` and `height` are the clear size, and `sill` is the
    height of the bottom above the floor (0 for doors).
    """

    def __init__(self, edge: int, width: float = 64.0, height: float = 112.0,
                 offset: Optional[float] = None, sill: float = 0.0, arched: bool = False):
        self.edge = edge
        self.width = width
        self.height = height
        self.offset = offset
        self.sill = sill
        self.arched = arched


def room(footprint: Sequence[Vec2], floor_z: float = 0.0, height: float = 128.0,
         wall: float = 16.0, floor: float = 16.0, ceiling: Optional[float] = 16.0,
         openings: Sequence[Opening] = (), materials: Optional[Dict[str, str]] = None) -> Dict[str, List[Brush]]:
    """A sealed room shell around a 2D footprint (any simple polygon, given as the
    *interior* outline). Returns a dict with "floor", "walls" and "ceiling" brush lists.

    Walls are built outside the footprint so the interior keeps the exact footprint size.
    Use `openings` to cut doors and windows. Set `ceiling=None` for an open-top room.
    """
    m = {"floor": "floor", "wall": "wall", "ceiling": "ceiling"}
    m.update(materials or {})
    fp = ensure_ccw(footprint)
    outer = offset_polygon_2d(fp, wall)
    top = floor_z + height

    floor_brushes = slab(outer, floor_z - floor, floor_z, {"top": m["floor"], "default": m["wall"]})
    ceiling_brushes = (
        slab(outer, top, top + ceiling, {"bottom": m["ceiling"], "default": m["wall"]})
        if ceiling
        else []
    )

    walls = []
    n = len(fp)
    for i in range(n):
        j = (i + 1) % n
        quad = [fp[i], fp[j], outer[j], outer[i]]
        walls.append(prism(quad, floor_z, top, m["wall"]))

    for op in openings:
        i = op.edge % n
        j = (i + 1) % n
        (ax, ay), (bx, by) = fp[i], fp[j]
        ln = math.hypot(bx - ax, by - ay)
        ux, uy = (bx - ax) / ln, (by - ay) / ln
        nx, ny = uy, -ux  # outward normal
        off = ln / 2.0 if op.offset is None else op.offset
        cx, cy = ax + ux * off, ay + uy * off
        hw = op.width / 2.0
        z0 = floor_z + op.sill
        z1 = z0 + op.height
        reach = wall * 2 + 8

        def cutter_pts(zb: float, zt: float, half: float):
            pts = []
            for s in (-half, half):
                for d in (-8.0, reach):
                    x, y = cx + ux * s + nx * d, cy + uy * s + ny * d
                    pts += [(x, y, zb), (x, y, zt)]
            return pts

        walls = subtract_all(walls, Brush(cutter_pts(z0, z1, hw)), m["wall"])
        if op.arched:
            # Fill the top of the rectangular hole with spandrel strips so the opening
            # gets a round head (a semicircle of radius hw) without fan-shaped slivers.
            spring = z1 - hw
            segs = max(4, int(op.width // 16))
            for k in range(segs):
                t0, t1 = math.pi * k / segs, math.pi * (k + 1) / segs
                pts = []
                for t in (t0, t1):
                    s_, v = -hw * math.cos(t), hw * math.sin(t)
                    for dd in (0.0, wall):
                        x, y = cx + ux * s_ + nx * dd, cy + uy * s_ + ny * dd
                        pts += [(x, y, spring + v), (x, y, z1)]
                if z1 - (spring + min(math.sin(t0), math.sin(t1)) * hw) > 0.5:
                    walls.append(Brush(pts, m["wall"]))

    return {"floor": floor_brushes, "walls": walls, "ceiling": ceiling_brushes}


def rect(x0: float, y0: float, x1: float, y1: float) -> List[Vec2]:
    """A rectangular footprint."""
    return [(x0, y0), (x1, y0), (x1, y1), (x0, y1)]


def l_shape(width: float, depth: float, arm_width: float, arm_depth: float,
            origin: Vec2 = (0.0, 0.0)) -> List[Vec2]:
    """An L footprint: a width x arm_depth bar along X plus an arm_width x depth bar
    along Y, joined at the origin corner."""
    ox, oy = origin
    return [
        (ox, oy), (ox + width, oy), (ox + width, oy + arm_depth),
        (ox + arm_width, oy + arm_depth), (ox + arm_width, oy + depth), (ox, oy + depth),
    ]


def regular_polygon(center: Vec2, radius: float, sides: int, start_deg: Optional[float] = None) -> List[Vec2]:
    """Octagonal, hexagonal etc. footprints with flat sides facing the axes."""
    if start_deg is None:
        start_deg = 180.0 / sides
    return circle_points(center, radius, sides, start_deg)


def rotate_all(brushes: Sequence[Brush], degrees: float, origin: Vec3 = (0.0, 0.0, 0.0)) -> List[Brush]:
    return [b.rotate_z(degrees, origin) for b in brushes]


def translate_all(brushes: Sequence[Brush], x: float = 0.0, y: float = 0.0, z: float = 0.0) -> List[Brush]:
    return [b.translate(x, y, z) for b in brushes]


def repeat(make, count: int, step: Vec3) -> List[Brush]:
    """Calls make(i) for i in range(count) and offsets each result by i * step.
    `make` may return a Brush or a list of Brushes."""
    out = []
    for i in range(count):
        res = make(i)
        res = res if isinstance(res, list) else [res]
        out += [b.translate(step[0] * i, step[1] * i, step[2] * i) for b in res]
    return out


__all__ = [
    "Opening",
    "arc_path",
    "arch",
    "block",
    "box",
    "circle_points",
    "cylinder",
    "frustum",
    "hull",
    "l_shape",
    "prism",
    "rect",
    "regular_polygon",
    "repeat",
    "ring",
    "room",
    "rotate_all",
    "rotate_point_z",
    "segment",
    "slab",
    "sphere",
    "spiral_stairs",
    "stairs",
    "sweep",
    "translate_all",
    "wedge",
]
