"""Convex brushes with per-face materials, transforms and CSG."""

from __future__ import annotations

import math
from typing import Callable, Dict, Iterable, List, Optional, Sequence, Tuple, Union

from .geometry import (
    EPS,
    DegenerateError,
    HullFace,
    Plane,
    Vec3,
    add,
    clean3,
    convex_hull,
    cross,
    dot,
    hull_volume,
    lerp,
    mul,
    normalize,
    sub,
)

# A material spec is either one material name for every face, or a dict with any of the
# keys "top", "bottom", "side" and "default". Faces whose normal points mostly up or down
# count as top or bottom; everything else is a side.
MaterialSpec = Union[str, Dict[str, str]]

DEFAULT_MATERIAL = "wall"

# Minimum thickness of fragments kept by Brush.subtract.
SLIVER = 1.0


def _pick_material(spec: MaterialSpec, normal: Vec3) -> str:
    if isinstance(spec, str):
        return spec
    default = spec.get("default", DEFAULT_MATERIAL)
    if normal[2] > 0.7:
        return spec.get("top", default)
    if normal[2] < -0.7:
        return spec.get("bottom", default)
    return spec.get("side", default)


class Face:
    __slots__ = ("plane", "polygon", "material")

    def __init__(self, plane: Plane, polygon: List[Vec3], material: str):
        self.plane = plane
        self.polygon = polygon
        self.material = material

    @property
    def normal(self) -> Vec3:
        return self.plane.normal


class Brush:
    """A convex solid. Construct it from points; the convex hull is computed for you.

    Brushes are immutable: every operation returns new brushes.
    """

    __slots__ = ("faces", "vertices", "tags", "_bounds")

    def __init__(
        self,
        points: Iterable[Vec3],
        material: MaterialSpec = DEFAULT_MATERIAL,
        tags: Optional[Dict[str, str]] = None,
        _face_materials: Optional[Sequence[Tuple[Plane, str]]] = None,
    ):
        hull = convex_hull(list(points))
        faces = []
        for hf in hull:
            mat = None
            if _face_materials:
                for pl, m in _face_materials:
                    if pl.same_as(hf.plane):
                        mat = m
                        break
            if mat is None:
                mat = _pick_material(material, hf.plane.normal)
            faces.append(Face(hf.plane, hf.polygon, mat))
        self.faces: List[Face] = faces
        verts = {}
        for f in faces:
            for p in f.polygon:
                verts[p] = True
        self.vertices: List[Vec3] = list(verts.keys())
        self.tags: Dict[str, str] = dict(tags or {})
        self._bounds = None

    # -- queries -------------------------------------------------------------------------

    @property
    def bounds(self) -> Tuple[Vec3, Vec3]:
        if self._bounds is None:
            xs = [v[0] for v in self.vertices]
            ys = [v[1] for v in self.vertices]
            zs = [v[2] for v in self.vertices]
            self._bounds = ((min(xs), min(ys), min(zs)), (max(xs), max(ys), max(zs)))
        return self._bounds

    @property
    def volume(self) -> float:
        return hull_volume([HullFace(f.plane, f.polygon) for f in self.faces])

    @property
    def center(self) -> Vec3:
        lo, hi = self.bounds
        return lerp(lo, hi, 0.5)

    def contains(self, p: Vec3, eps: float = EPS) -> bool:
        return all(f.plane.distance(p) <= eps for f in self.faces)

    def min_thickness(self) -> float:
        """Smallest extent of the brush measured along any of its face normals."""
        best = math.inf
        for f in self.faces:
            n = f.plane.normal
            ds = [dot(n, v) for v in self.vertices]
            best = min(best, max(ds) - min(ds))
        return best

    def _face_materials(self) -> List[Tuple[Plane, str]]:
        return [(f.plane, f.material) for f in self.faces]

    def _rebuild(self, points: Iterable[Vec3], cap_material: Optional[str] = None,
                 face_materials: Optional[List[Tuple[Plane, str]]] = None) -> "Brush":
        fm = face_materials if face_materials is not None else self._face_materials()
        default = cap_material or (self.faces[0].material if self.faces else DEFAULT_MATERIAL)
        return Brush(points, default, self.tags, _face_materials=fm)

    # -- materials -----------------------------------------------------------------------

    def with_material(self, spec: MaterialSpec) -> "Brush":
        b = Brush(self.vertices, spec, self.tags)
        return b

    def with_face_material(self, predicate: Callable[[Face], bool], material: str) -> "Brush":
        fm = [(f.plane, material if predicate(f) else f.material) for f in self.faces]
        return self._rebuild(self.vertices, face_materials=fm)

    def with_top(self, material: str) -> "Brush":
        return self.with_face_material(lambda f: f.normal[2] > 0.7, material)

    def tagged(self, **tags: str) -> "Brush":
        b = self._rebuild(self.vertices)
        b.tags.update(tags)
        return b

    # -- transforms ----------------------------------------------------------------------

    def map_points(self, fn: Callable[[Vec3], Vec3]) -> "Brush":
        """Applies an affine point transform. Face materials follow their faces."""
        fm = []
        for f in self.faces:
            a, b, c = (fn(p) for p in _three_spread(f.polygon))
            try:
                fm.append((Plane.from_points(a, b, c), f.material))
            except ValueError:
                pass
        # Mirroring flips the winding; normalize planes to face away from the new center.
        new_points = [fn(p) for p in self.vertices]
        from .geometry import centroid

        c = centroid(new_points)
        fm = [(pl if pl.distance(c) < 0 else pl.flipped(), m) for pl, m in fm]
        return self._rebuild(new_points, face_materials=fm)

    def translate(self, x: float = 0.0, y: float = 0.0, z: float = 0.0) -> "Brush":
        return self.map_points(lambda p: (p[0] + x, p[1] + y, p[2] + z))

    def move(self, offset: Vec3) -> "Brush":
        return self.translate(*offset)

    def rotate_z(self, degrees: float, origin: Vec3 = (0.0, 0.0, 0.0)) -> "Brush":
        return self.map_points(lambda p: rotate_point_z(p, degrees, origin))

    def rotate(self, axis: Vec3, degrees: float, origin: Vec3 = (0.0, 0.0, 0.0)) -> "Brush":
        return self.map_points(lambda p: rotate_point(p, axis, degrees, origin))

    def scale(self, sx: float, sy: Optional[float] = None, sz: Optional[float] = None,
              origin: Vec3 = (0.0, 0.0, 0.0)) -> "Brush":
        sy = sx if sy is None else sy
        sz = sx if sz is None else sz
        return self.map_points(lambda p: (
            origin[0] + (p[0] - origin[0]) * sx,
            origin[1] + (p[1] - origin[1]) * sy,
            origin[2] + (p[2] - origin[2]) * sz,
        ))

    def mirror_x(self, x: float = 0.0) -> "Brush":
        return self.map_points(lambda p: (2 * x - p[0], p[1], p[2]))

    def mirror_y(self, y: float = 0.0) -> "Brush":
        return self.map_points(lambda p: (p[0], 2 * y - p[1], p[2]))

    def snapped(self, grid: float = 1.0) -> "Brush":
        """Rounds every vertex to the grid. Curved brushes may change slightly."""
        return self.map_points(lambda p: tuple(round(c / grid) * grid for c in p))

    # -- CSG -----------------------------------------------------------------------------

    def clip(self, plane: Plane, cap_material: Optional[str] = None) -> Optional["Brush"]:
        """Keeps the part behind the plane (where plane.distance(p) <= 0)."""
        pts = []
        for v in self.vertices:
            if plane.distance(v) <= EPS:
                pts.append(v)
        for f in self.faces:
            poly = f.polygon
            for i in range(len(poly)):
                a, b = poly[i], poly[(i + 1) % len(poly)]
                da, db = plane.distance(a), plane.distance(b)
                if (da < -EPS and db > EPS) or (da > EPS and db < -EPS):
                    pts.append(lerp(a, b, da / (da - db)))
        if len(pts) < 4:
            return None
        fm = self._face_materials() + [(plane, cap_material or self.faces[0].material)]
        try:
            b = self._rebuild(pts, face_materials=fm)
        except DegenerateError:
            return None
        if b.volume < 1e-3:
            return None
        return b

    def split(self, plane: Plane, cap_material: Optional[str] = None):
        """Returns (back, front) halves; either may be None."""
        return self.clip(plane, cap_material), self.clip(plane.flipped(), cap_material)

    def intersect(self, other: "Brush") -> Optional["Brush"]:
        b: Optional[Brush] = self
        for f in other.faces:
            if b is None:
                return None
            b = b.clip(f.plane, f.material)
        return b

    def overlaps(self, other: "Brush", eps: float = 0.01) -> bool:
        """True if the brushes share interior volume (touching does not count)."""
        (alo, ahi), (blo, bhi) = self.bounds, other.bounds
        for i in range(3):
            if ahi[i] <= blo[i] + eps or bhi[i] <= alo[i] + eps:
                return False
        # Separating axis over face normals.
        for brush_a, brush_b in ((self, other), (other, self)):
            for f in brush_a.faces:
                if min(f.plane.distance(v) for v in brush_b.vertices) >= -eps:
                    return False
        inter = self.intersect(other)
        return inter is not None and inter.volume > eps

    def subtract(self, cutter: "Brush", cap_material: Optional[str] = None) -> List["Brush"]:
        """Returns self minus cutter as a list of convex fragments."""
        if not _bounds_overlap(self.bounds, cutter.bounds):
            return [self]
        result = []
        remaining: Optional[Brush] = self
        for f in cutter.faces:
            if remaining is None:
                break
            outside = remaining.clip(f.plane.flipped(), cap_material)
            # Fragments thinner than a unit only ever border the cut, so dropping
            # them just widens the hole a hair instead of leaving unusable slivers.
            if outside is not None and outside.min_thickness() >= SLIVER:
                result.append(outside)
            remaining = remaining.clip(f.plane, cap_material)
        if remaining is None:
            # The cutter did not actually intersect.
            return [self]
        return result

    def __repr__(self) -> str:
        lo, hi = self.bounds
        return f"Brush({len(self.faces)} faces, bounds={clean3(lo)}..{clean3(hi)})"


def _bounds_overlap(a, b, eps: float = EPS) -> bool:
    (alo, ahi), (blo, bhi) = a, b
    return all(ahi[i] > blo[i] + eps and bhi[i] > alo[i] + eps for i in range(3))


def _three_spread(poly: Sequence[Vec3]) -> Tuple[Vec3, Vec3, Vec3]:
    """Picks three polygon vertices that span a large triangle, keeping the winding."""
    n = len(poly)
    if n == 3:
        return poly[0], poly[1], poly[2]
    best = None
    best_area = -1.0
    step = max(1, n // 3)
    for i in range(n):
        j, k = (i + step) % n, (i + 2 * step) % n
        if len({i, j, k}) < 3:
            continue
        a = cross(sub(poly[j], poly[i]), sub(poly[k], poly[i]))
        area = dot(a, a)
        if area > best_area:
            best_area = area
            best = (poly[i], poly[j], poly[k])
    return best  # type: ignore[return-value]


def rotate_point_z(p: Vec3, degrees: float, origin: Vec3 = (0.0, 0.0, 0.0)) -> Vec3:
    r = math.radians(degrees)
    c, s = math.cos(r), math.sin(r)
    x, y = p[0] - origin[0], p[1] - origin[1]
    return (origin[0] + x * c - y * s, origin[1] + x * s + y * c, p[2])


def rotate_point(p: Vec3, axis: Vec3, degrees: float, origin: Vec3 = (0.0, 0.0, 0.0)) -> Vec3:
    k = normalize(axis)
    r = math.radians(degrees)
    v = sub(p, origin)
    c, s = math.cos(r), math.sin(r)
    # Rodrigues' rotation formula.
    rotated = add(add(mul(v, c), mul(cross(k, v), s)), mul(k, dot(k, v) * (1 - c)))
    return add(origin, rotated)


def subtract_all(brushes: Iterable[Brush], cutter: Brush,
                 cap_material: Optional[str] = None) -> List[Brush]:
    out = []
    for b in brushes:
        out.extend(b.subtract(cutter, cap_material))
    return out
