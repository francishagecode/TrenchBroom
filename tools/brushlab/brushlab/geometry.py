"""Low-level vector math, convex hulls and convex polygon helpers.

Everything here works on plain tuples so the toolkit has no dependencies. Brushes are
small (rarely more than a few dozen vertices), so simple O(n^2) algorithms are fine.
"""

from __future__ import annotations

import math
from typing import Iterable, List, Sequence, Tuple

Vec3 = Tuple[float, float, float]
Vec2 = Tuple[float, float]

# Distance tolerance in map units. Map coordinates are usually integers in the range of
# a few thousand units, so this is far below anything visible but well above float noise.
EPS = 1e-4


def add(a: Vec3, b: Vec3) -> Vec3:
    return (a[0] + b[0], a[1] + b[1], a[2] + b[2])


def sub(a: Vec3, b: Vec3) -> Vec3:
    return (a[0] - b[0], a[1] - b[1], a[2] - b[2])


def mul(a: Vec3, s: float) -> Vec3:
    return (a[0] * s, a[1] * s, a[2] * s)


def dot(a: Vec3, b: Vec3) -> float:
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]


def cross(a: Vec3, b: Vec3) -> Vec3:
    return (
        a[1] * b[2] - a[2] * b[1],
        a[2] * b[0] - a[0] * b[2],
        a[0] * b[1] - a[1] * b[0],
    )


def length(a: Vec3) -> float:
    return math.sqrt(dot(a, a))


def normalize(a: Vec3) -> Vec3:
    n = length(a)
    if n == 0.0:
        raise ValueError("cannot normalize a zero vector")
    return (a[0] / n, a[1] / n, a[2] / n)


def lerp(a: Vec3, b: Vec3, t: float) -> Vec3:
    return (a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t, a[2] + (b[2] - a[2]) * t)


def centroid(points: Sequence[Vec3]) -> Vec3:
    n = float(len(points))
    return (
        sum(p[0] for p in points) / n,
        sum(p[1] for p in points) / n,
        sum(p[2] for p in points) / n,
    )


def clean(v: float, digits: int = 6) -> float:
    """Rounds away float noise so that 63.99999999 becomes 64."""
    r = round(v, digits)
    return 0.0 if r == 0.0 else r


def clean3(p: Vec3) -> Vec3:
    return (clean(p[0]), clean(p[1]), clean(p[2]))


def dedupe(points: Iterable[Vec3]) -> List[Vec3]:
    seen = set()
    result = []
    for p in points:
        p = clean3(p)
        key = (round(p[0] / EPS), round(p[1] / EPS), round(p[2] / EPS))
        if key not in seen:
            seen.add(key)
            result.append(p)
    return result


class Plane:
    """The plane n.x = d with unit normal n. The brush interior is on the negative side."""

    __slots__ = ("normal", "dist")

    def __init__(self, normal: Vec3, dist: float):
        self.normal = normal
        self.dist = dist

    @staticmethod
    def from_points(a: Vec3, b: Vec3, c: Vec3) -> "Plane":
        n = normalize(cross(sub(b, a), sub(c, a)))
        return Plane(n, dot(n, a))

    def distance(self, p: Vec3) -> float:
        return dot(self.normal, p) - self.dist

    def flipped(self) -> "Plane":
        return Plane(mul(self.normal, -1.0), -self.dist)

    def same_as(self, other: "Plane", eps: float = EPS) -> bool:
        return dot(self.normal, other.normal) > 1.0 - 1e-7 and abs(self.dist - other.dist) < eps * 10

    def __repr__(self) -> str:
        return f"Plane({self.normal}, {self.dist})"


class HullFace:
    __slots__ = ("plane", "polygon")

    def __init__(self, plane: Plane, polygon: List[Vec3]):
        self.plane = plane
        self.polygon = polygon  # counter-clockwise when viewed from outside


class DegenerateError(ValueError):
    pass


def convex_hull(points: Sequence[Vec3]) -> List[HullFace]:
    """Returns the faces of the convex hull of the given points.

    Coplanar triangles are merged into single polygonal faces, and vertices that lie in the
    middle of an edge are removed. Raises DegenerateError if the points do not span a volume.
    """
    pts = dedupe(points)
    if len(pts) < 4:
        raise DegenerateError("a brush needs at least four distinct points")

    # Pick an initial non-degenerate tetrahedron from extreme points.
    i0 = 0
    i1 = max(range(len(pts)), key=lambda i: length(sub(pts[i], pts[i0])))
    if length(sub(pts[i1], pts[i0])) < EPS:
        raise DegenerateError("points are coincident")
    d01 = normalize(sub(pts[i1], pts[i0]))

    def line_dist(i: int) -> float:
        v = sub(pts[i], pts[i0])
        return length(cross(v, d01))

    i2 = max(range(len(pts)), key=line_dist)
    if line_dist(i2) < EPS:
        raise DegenerateError("points are collinear")
    base = Plane.from_points(pts[i0], pts[i1], pts[i2])
    i3 = max(range(len(pts)), key=lambda i: abs(base.distance(pts[i])))
    if abs(base.distance(pts[i3])) < EPS:
        raise DegenerateError("points are coplanar; a brush needs volume")

    interior = centroid([pts[i0], pts[i1], pts[i2], pts[i3]])

    faces: List[List[int]] = []

    def make_face(a: int, b: int, c: int) -> List[int]:
        plane = Plane.from_points(pts[a], pts[b], pts[c])
        if plane.distance(interior) > 0:
            return [a, c, b]
        return [a, b, c]

    for tri in ((i0, i1, i2), (i0, i1, i3), (i0, i2, i3), (i1, i2, i3)):
        faces.append(make_face(*tri))

    planes = {}

    def plane_of(f: List[int]) -> Plane:
        key = tuple(f)
        pl = planes.get(key)
        if pl is None:
            pl = Plane.from_points(pts[f[0]], pts[f[1]], pts[f[2]])
            planes[key] = pl
        return pl

    used = {i0, i1, i2, i3}
    for pi in range(len(pts)):
        if pi in used:
            continue
        p = pts[pi]
        visible = [f for f in faces if plane_of(f).distance(p) > EPS]
        if not visible:
            continue
        edges = set()
        for f in visible:
            for k in range(3):
                edges.add((f[k], f[(k + 1) % 3]))
        horizon = [(u, v) for (u, v) in edges if (v, u) not in edges]
        visible_ids = set(id(f) for f in visible)
        faces = [f for f in faces if id(f) not in visible_ids]
        for (u, v) in horizon:
            faces.append([u, v, pi])

    # Merge coplanar triangles.
    groups: List[Tuple[Plane, set]] = []
    for f in faces:
        try:
            pl = plane_of(f)
        except ValueError:
            continue  # sliver triangle with zero area
        for gpl, verts in groups:
            if gpl.same_as(pl):
                verts.update(f)
                break
        else:
            groups.append((pl, set(f)))

    result = []
    for pl, verts in groups:
        poly = order_polygon([pts[i] for i in verts], pl.normal)
        poly = remove_collinear(poly)
        if len(poly) >= 3:
            result.append(HullFace(pl, poly))

    if len(result) < 4:
        raise DegenerateError("hull has fewer than four faces")
    return result


def order_polygon(points: List[Vec3], normal: Vec3) -> List[Vec3]:
    """Sorts coplanar points counter-clockwise around the normal."""
    c = centroid(points)
    # Build a basis in the plane.
    ref = (1.0, 0.0, 0.0) if abs(normal[0]) < 0.9 else (0.0, 1.0, 0.0)
    u = normalize(cross(normal, ref))
    v = cross(normal, u)

    def angle(p: Vec3) -> float:
        d = sub(p, c)
        return math.atan2(dot(d, v), dot(d, u))

    return sorted(points, key=angle)


def remove_collinear(poly: List[Vec3]) -> List[Vec3]:
    changed = True
    while changed and len(poly) > 3:
        changed = False
        for i in range(len(poly)):
            a, b, c = poly[i - 1], poly[i], poly[(i + 1) % len(poly)]
            ab = sub(b, a)
            bc = sub(c, b)
            if length(ab) < EPS or length(cross(ab, bc)) < EPS * max(1.0, length(ab) + length(bc)):
                del poly[i]
                changed = True
                break
    return poly


def polygon_area(poly: Sequence[Vec3]) -> float:
    total = (0.0, 0.0, 0.0)
    for i in range(len(poly)):
        total = add(total, cross(poly[i], poly[(i + 1) % len(poly)]))
    return length(total) / 2.0


def hull_volume(faces: Sequence[HullFace]) -> float:
    origin = faces[0].polygon[0]
    vol = 0.0
    for f in faces:
        poly = f.polygon
        for i in range(1, len(poly) - 1):
            vol += dot(sub(poly[0], origin), cross(sub(poly[i], origin), sub(poly[i + 1], origin)))
    return abs(vol) / 6.0


# ---------------------------------------------------------------------------------------
# 2D polygon helpers (footprints)
# ---------------------------------------------------------------------------------------


def signed_area_2d(poly: Sequence[Vec2]) -> float:
    a = 0.0
    for i in range(len(poly)):
        x0, y0 = poly[i]
        x1, y1 = poly[(i + 1) % len(poly)]
        a += x0 * y1 - x1 * y0
    return a / 2.0


def ensure_ccw(poly: Sequence[Vec2]) -> List[Vec2]:
    poly = [tuple(map(float, p)) for p in poly]
    return poly if signed_area_2d(poly) > 0 else list(reversed(poly))


def _cross2(o: Vec2, a: Vec2, b: Vec2) -> float:
    return (a[0] - o[0]) * (b[1] - o[1]) - (a[1] - o[1]) * (b[0] - o[0])


def is_convex_2d(poly: Sequence[Vec2]) -> bool:
    n = len(poly)
    for i in range(n):
        if _cross2(poly[i - 1], poly[i], poly[(i + 1) % n]) < -1e-9:
            return False
    return True


def _point_in_triangle(p: Vec2, a: Vec2, b: Vec2, c: Vec2) -> bool:
    return _cross2(a, b, p) >= -1e-9 and _cross2(b, c, p) >= -1e-9 and _cross2(c, a, p) >= -1e-9


def convex_decompose_2d(poly: Sequence[Vec2]) -> List[List[Vec2]]:
    """Splits a simple polygon into convex pieces (ear clipping + Hertel-Mehlhorn merge).

    The result usually has few pieces for architectural footprints such as L, T or U
    shapes. Each piece is counter-clockwise.
    """
    poly = ensure_ccw(poly)
    # Drop collinear points first; they produce needless splits.
    pts = []
    for i in range(len(poly)):
        if abs(_cross2(poly[i - 1], poly[i], poly[(i + 1) % len(poly)])) > 1e-9:
            pts.append(poly[i])
    if is_convex_2d(pts):
        return [pts]

    pieces = _reflex_split(pts, depth=0)
    if pieces is not None:
        return _merge_convex(pieces)
    return _ear_decompose(pts)


def _merge_convex(pieces: List[List[Vec2]]) -> List[List[Vec2]]:
    """Merges neighbouring pieces whose union is still convex."""
    def key(p: Vec2):
        return (round(p[0], 5), round(p[1], 5))

    pieces = [list(p) for p in pieces]
    merged = True
    while merged:
        merged = False
        for a in range(len(pieces)):
            for b in range(a + 1, len(pieces)):
                ka = [key(p) for p in pieces[a]]
                kb = [key(p) for p in pieces[b]]
                shared = _shared_edge(ka, kb)
                if shared is None:
                    continue
                lookup = {key(p): p for p in pieces[a] + pieces[b]}
                cand = _drop_collinear([lookup[k] for k in _merge(ka, kb, shared)])
                if len(cand) >= 3 and is_convex_2d(cand):
                    pieces[a] = cand
                    del pieces[b]
                    merged = True
                    break
            if merged:
                break
    return pieces


def _ray_hit(poly: List[Vec2], origin: Vec2, d: Vec2, skip: Tuple[int, ...]):
    """First intersection of the ray origin + t*d (t > 0) with the polygon boundary.
    Returns (t, edge_index, point) or None."""
    best = None
    n = len(poly)
    for i in range(n):
        if i in skip:
            continue
        a, b = poly[i], poly[(i + 1) % n]
        e = (b[0] - a[0], b[1] - a[1])
        den = d[0] * e[1] - d[1] * e[0]
        if abs(den) < 1e-12:
            continue
        w = (a[0] - origin[0], a[1] - origin[1])
        t = (w[0] * e[1] - w[1] * e[0]) / den
        s = (w[0] * d[1] - w[1] * d[0]) / den
        if t > 1e-7 and -1e-9 <= s <= 1 + 1e-9 and (best is None or t < best[0]):
            best = (t, i, (origin[0] + d[0] * t, origin[1] + d[1] * t))
    return best


def _reflex_split(pts: List[Vec2], depth: int):
    """Recursively cuts at reflex vertices by extending one of their edges, which gives
    axis-aligned cuts for rectilinear footprints (L, T, U shapes...)."""
    if is_convex_2d(pts):
        return [pts]
    if depth > 64:
        return None
    n = len(pts)
    best = None
    for i in range(n):
        prev, cur, nxt = pts[i - 1], pts[i], pts[(i + 1) % n]
        if _cross2(prev, cur, nxt) >= -1e-9:
            continue
        # Extend the incoming edge forward, or the outgoing edge backward.
        for d, skip in (
            ((cur[0] - prev[0], cur[1] - prev[1]), ((i - 1) % n, i)),
            ((cur[0] - nxt[0], cur[1] - nxt[1]), ((i - 1) % n, i)),
        ):
            hit = _ray_hit(pts, cur, d, skip)
            if hit is None:
                continue
            t = hit[0] * math.hypot(*d)
            if best is None or t < best[0]:
                best = (t, i, hit[1], hit[2])
    if best is None:
        return None
    _, i, edge, p = best
    # Insert the hit point into the edge (unless it is an existing vertex) and split.
    ring = list(pts)
    j = (edge + 1) % n
    if math.hypot(p[0] - ring[edge][0], p[1] - ring[edge][1]) < 1e-7:
        k = edge
    elif math.hypot(p[0] - ring[j][0], p[1] - ring[j][1]) < 1e-7:
        k = j
    else:
        ring.insert(edge + 1, p)
        k = edge + 1
        if k <= i:
            i += 1
    a = _walk(ring, i, k)
    b = _walk(ring, k, i)
    if len(a) < 3 or len(b) < 3 or signed_area_2d(a) <= 1e-9 or signed_area_2d(b) <= 1e-9:
        return None
    ra = _reflex_split(_drop_collinear(a), depth + 1)
    rb = _reflex_split(_drop_collinear(b), depth + 1)
    if ra is None or rb is None:
        return None
    return ra + rb


def _walk(ring: List[Vec2], start: int, end: int) -> List[Vec2]:
    out = []
    i = start
    while True:
        out.append(ring[i])
        if i == end:
            break
        i = (i + 1) % len(ring)
    return out


def _drop_collinear(poly: List[Vec2]) -> List[Vec2]:
    return [
        poly[i] for i in range(len(poly))
        if abs(_cross2(poly[i - 1], poly[i], poly[(i + 1) % len(poly)])) > 1e-9
    ]


def _ear_decompose(pts: List[Vec2]) -> List[List[Vec2]]:
    # Ear clipping into triangles of vertex indices.
    idx = list(range(len(pts)))
    tris = []
    guard = 0
    while len(idx) > 3 and guard < 10000:
        guard += 1
        for k in range(len(idx)):
            i0, i1, i2 = idx[k - 1], idx[k], idx[(k + 1) % len(idx)]
            a, b, c = pts[i0], pts[i1], pts[i2]
            if _cross2(a, b, c) <= 1e-9:
                continue
            if any(
                _point_in_triangle(pts[j], a, b, c)
                for j in idx
                if j not in (i0, i1, i2)
            ):
                continue
            tris.append([i0, i1, i2])
            del idx[k]
            break
        else:
            raise ValueError("footprint polygon is not simple")
    tris.append(idx[:])

    # Hertel-Mehlhorn: greedily merge pieces across diagonals while the result is convex.
    pieces = [t[:] for t in tris]
    merged = True
    while merged:
        merged = False
        for a in range(len(pieces)):
            for b in range(a + 1, len(pieces)):
                pa, pb = pieces[a], pieces[b]
                shared = _shared_edge(pa, pb)
                if shared is None:
                    continue
                cand = _merge(pa, pb, shared)
                if is_convex_2d([pts[i] for i in cand]):
                    pieces[a] = cand
                    del pieces[b]
                    merged = True
                    break
            if merged:
                break

    return [[pts[i] for i in piece] for piece in pieces]


def _shared_edge(pa: List[int], pb: List[int]):
    for i in range(len(pa)):
        u, v = pa[i], pa[(i + 1) % len(pa)]
        for j in range(len(pb)):
            if pb[j] == v and pb[(j + 1) % len(pb)] == u:
                return (i, j)
    return None


def _merge(pa: List[int], pb: List[int], shared) -> List[int]:
    i, j = shared
    # pa edge i: pa[i] -> pa[i+1]; pb edge j: pb[j] -> pb[j+1] (reversed)
    ra = pa[i + 1:] + pa[: i + 1]  # starts at pa[i+1] (=pb[j]), ends at pa[i]
    rb = pb[j + 1:] + pb[: j + 1]  # starts at pb[j+1] (=pa[i]), ends at pb[j]
    return ra[:-1] + rb[:-1]


def offset_polygon_2d(poly: Sequence[Vec2], distance: float) -> List[Vec2]:
    """Offsets a CCW polygon outward (positive) or inward (negative) with mitered corners."""
    poly = ensure_ccw(poly)
    n = len(poly)
    lines = []
    for i in range(n):
        a, b = poly[i], poly[(i + 1) % n]
        dx, dy = b[0] - a[0], b[1] - a[1]
        ln = math.hypot(dx, dy)
        nx, ny = dy / ln, -dx / ln  # outward normal for CCW
        lines.append(((a[0] + nx * distance, a[1] + ny * distance), (dx, dy)))
    result = []
    for i in range(n):
        (p0, d0), (p1, d1) = lines[i - 1], lines[i]
        result.append(_intersect_lines(p0, d0, p1, d1))
    return result


def _intersect_lines(p0: Vec2, d0: Vec2, p1: Vec2, d1: Vec2) -> Vec2:
    den = d0[0] * d1[1] - d0[1] * d1[0]
    if abs(den) < 1e-12:
        return p1
    t = ((p1[0] - p0[0]) * d1[1] - (p1[1] - p0[1]) * d1[0]) / den
    return (p0[0] + d0[0] * t, p0[1] + d0[1] * t)
