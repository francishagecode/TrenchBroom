"""Preview rendering: a 2x2 sheet (two isometric views, plan and elevation) as PNG.

Uses Pillow only. Faces are cut into small tiles and depth-sorted (painter's algorithm),
which is accurate enough for review sheets without needing a z-buffer.
"""

from __future__ import annotations

import colorsys
import hashlib
import math
from typing import Dict, List, Optional, Sequence, Tuple

from .brush import Brush
from .design import Design
from .geometry import Vec3, cross, dot, normalize, sub
from .shapes import box

KNOWN_COLORS: Dict[str, Tuple[int, int, int]] = {
    "wall": (176, 168, 150),
    "floor": (140, 112, 84),
    "ceiling": (200, 200, 205),
    "stone": (150, 150, 145),
    "brick": (165, 92, 70),
    "wood": (150, 100, 55),
    "plank": (170, 125, 75),
    "metal": (110, 125, 140),
    "trim": (205, 170, 80),
    "gold": (225, 185, 60),
    "water": (60, 120, 190),
    "lava": (230, 90, 30),
    "slime": (120, 215, 45),
    "concrete": (128, 128, 122),
    "grate": (82, 88, 96),
    "hazard": (220, 175, 35),
    "hull": (92, 104, 118),
    "grass": (90, 150, 70),
    "dirt": (120, 95, 65),
    "glass": (170, 215, 225),
    "cloth": (150, 50, 60),
    "rope": (185, 155, 105),
    "book": (120, 60, 70),
    "roof": (120, 60, 50),
    "marble": (225, 222, 215),
    "sky": (120, 170, 230),
    "clip": (200, 80, 200),
    "player": (255, 140, 0),
    "entity:light": (255, 236, 110),
    "entity": (90, 220, 130),
}

BACKGROUND = (34, 38, 46)
GRID = (48, 54, 64)
GRID_MAJOR = (62, 70, 82)
LIGHT = normalize((0.35, -0.55, 0.75))


def material_color(name: str) -> Tuple[int, int, int]:
    if name in KNOWN_COLORS:
        return KNOWN_COLORS[name]
    key = name.lower()
    for k, c in KNOWN_COLORS.items():
        if key == k or key.startswith(k):
            return c
    h = int(hashlib.md5(name.encode()).hexdigest()[:6], 16)
    r, g, b = colorsys.hls_to_rgb((h % 360) / 360.0, 0.55, 0.45)
    return (int(r * 255), int(g * 255), int(b * 255))


class View:
    def __init__(self, title: str, direction: Vec3, up: Vec3 = (0.0, 0.0, 1.0), grid: bool = False):
        self.title = title
        self.dir = normalize(direction)  # from camera into the scene
        if abs(dot(self.dir, normalize(up))) > 0.99:
            up = (0.0, 1.0, 0.0)
        self.right = normalize(cross(self.dir, up))
        self.up = cross(self.right, self.dir)
        self.grid = grid


VIEWS = [
    View("iso (from south-west)", (1.0, 1.0, -0.85)),
    View("iso (from north-east)", (-1.0, -1.0, -0.85)),
    View("plan (top)", (0.0, 0.0, -1.0), up=(0.0, 1.0, 0.0), grid=True),
    View("elevation (from south)", (0.0, 1.0, 0.0), grid=True),
]
# Views that drop ceilings and roofs when the design has any (see Design.is_cutaway).
CUTAWAY_VIEWS = (1, 2)


def _split(poly, axis_u, value):
    """Splits a polygon [(p2d, depth, is_edge_original)] by the line u = value."""
    below, above = [], []
    n = len(poly)
    for i in range(n):
        (a, da, ea), (b, db, _) = poly[i], poly[(i + 1) % n]
        sa, sb = a[axis_u] - value, b[axis_u] - value
        if sa <= 0:
            below.append((a, da, ea))
        if sa >= 0:
            above.append((a, da, ea))
        if (sa < 0 < sb) or (sb < 0 < sa):
            t = sa / (sa - sb)
            p = (a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t)
            d = da + (db - da) * t
            if sa < 0:
                below.append((p, d, False))
                above.append((p, d, ea))
            else:
                above.append((p, d, False))
                below.append((p, d, ea))
    # The edge that closes each half along the split line is not an original edge.
    return _fix_flags(below, axis_u, value), _fix_flags(above, axis_u, value)


def _fix_flags(poly, axis_u, value):
    if len(poly) < 3:
        return []
    out = []
    n = len(poly)
    for i in range(n):
        (a, da, ea), (b, _, _) = poly[i], poly[(i + 1) % n]
        on_line = abs(a[axis_u] - value) < 1e-6 and abs(b[axis_u] - value) < 1e-6
        out.append((a, da, ea and not on_line))
    return out


def _tile(poly, size):
    """Cuts a projected polygon into screen-space tiles of roughly `size` pixels."""
    pieces = [poly]
    for axis in (0, 1):
        lo = min(p[0][axis] for p in poly)
        hi = max(p[0][axis] for p in poly)
        k = math.floor(lo / size) + 1
        cuts = []
        while k * size < hi:
            cuts.append(k * size)
            k += 1
        for c in cuts:
            nxt = []
            for pc in pieces:
                if min(p[0][axis] for p in pc) >= c or max(p[0][axis] for p in pc) <= c:
                    nxt.append(pc)
                    continue
                a, b = _split(pc, axis, c)
                if a:
                    nxt.append(a)
                if b:
                    nxt.append(b)
            pieces = nxt
    return pieces


def render_view(img, rect, scene: List[Brush], view: View, highlight: Optional[List[Brush]] = None,
                label: str = "", fit: Optional[List[Brush]] = None, title: Optional[str] = None):
    from PIL import ImageDraw

    x0, y0, w, h = rect
    draw = ImageDraw.Draw(img)
    draw.rectangle([x0, y0, x0 + w - 1, y0 + h - 1], fill=BACKGROUND)

    all_pts = [v for b in (fit or scene) for v in b.vertices]
    us = [dot(p, view.right) for p in all_pts]
    vs = [dot(p, view.up) for p in all_pts]
    umin, umax, vmin, vmax = min(us), max(us), min(vs), max(vs)
    margin = 28
    scale = min((w - 2 * margin) / max(umax - umin, 1e-6), (h - 2 * margin - 18) / max(vmax - vmin, 1e-6))
    cu, cv = (umin + umax) / 2, (vmin + vmax) / 2

    def project(p: Vec3):
        return (
            x0 + w / 2 + (dot(p, view.right) - cu) * scale,
            y0 + h / 2 + 9 - (dot(p, view.up) - cv) * scale,
        )

    if view.grid:
        step = 16.0
        while step * scale < 10:
            step *= 2
        u_start = math.floor((umin - (w / 2) / scale) / step) * step
        v_start = math.floor((vmin - (h / 2) / scale) / step) * step
        u = u_start
        while u < umax + (w / 2) / scale:
            sx = x0 + w / 2 + (u - cu) * scale
            if x0 <= sx < x0 + w:
                major = abs(u / (step * 4) - round(u / (step * 4))) < 1e-6
                draw.line([(sx, y0), (sx, y0 + h - 1)], fill=GRID_MAJOR if major else GRID)
            u += step
        v = v_start
        while v < vmax + (h / 2) / scale:
            sy = y0 + h / 2 + 9 - (v - cv) * scale
            if y0 <= sy < y0 + h:
                major = abs(v / (step * 4) - round(v / (step * 4))) < 1e-6
                draw.line([(x0, sy), (x0 + w - 1, sy)], fill=GRID_MAJOR if major else GRID)
            v += step

    tile_px = 14.0
    tiles = []
    hl = set(id(b) for b in (highlight or []))
    for b in scene:
        for f in b.faces:
            n = f.normal
            if dot(n, view.dir) >= -1e-6:
                continue
            base = KNOWN_COLORS["player"] if id(b) in hl else material_color(f.material)
            shade = 0.42 + 0.58 * max(0.0, dot(n, LIGHT))
            # Slight per-axis tint so perpendicular faces never look identical.
            shade *= 0.92 + 0.08 * abs(n[0])
            color = tuple(min(255, int(c * shade)) for c in base)
            edge = tuple(int(c * 0.45) for c in base)
            poly = [(project(p), dot(p, view.dir), True) for p in f.polygon]
            for t in _tile(poly, tile_px):
                depth = sum(p[1] for p in t) / len(t)
                tiles.append((depth, t, color, edge))

    tiles.sort(key=lambda t: -t[0])
    for _, t, color, edge in tiles:
        pts = [p[0] for p in t]
        draw.polygon(pts, fill=color, outline=color)
        for i in range(len(t)):
            if t[i][2]:
                draw.line([t[i][0], t[(i + 1) % len(t)][0]], fill=edge, width=1)

    font = _font(14)
    draw.text((x0 + 8, y0 + 6), title or view.title, fill=(210, 215, 225), font=font)
    if label:
        draw.text((x0 + 8, y0 + h - 22), label, fill=(150, 160, 175), font=font)
    return scale


_FONTS = {}


def _font(size: int):
    from PIL import ImageFont

    if size not in _FONTS:
        try:
            _FONTS[size] = ImageFont.load_default(size=size)
        except TypeError:
            _FONTS[size] = ImageFont.load_default()
    return _FONTS[size]


def entity_markers(design: Design) -> List[Brush]:
    """Small boxes standing in for point entities (lights are yellow, others green)."""
    from .shapes import block

    out = []
    for e in design.entities:
        if e.origin is None:
            continue
        mat = "entity:light" if e.classname.startswith("light") else "entity"
        size = (12, 12, 12) if mat == "entity:light" else (16, 16, 32)
        out.append(block(e.origin, size, mat))
    return out


def player_reference(design: Design) -> Brush:
    """A 32x32x56 box standing just off the design's south-east corner. That corner
    lands beside the design (never in front of it) in all four preview views."""
    lo, hi = design.bounds
    x = hi[0] + 48
    y = lo[1] - 48
    # Stand on the floor (z = 0) when the design is near it, e.g. wall-mounted parts and
    # pieces that drop below the floor; otherwise stand at the design's lowest point.
    z = 0.0 if lo[2] <= 256 and hi[2] >= -256 else lo[2]
    return box((x - 16, y - 16, z), (x + 16, y + 16, z + 56), "player")


def render_sheet(design: Design, path: str, title: str = "", subtitle: str = "", size: int = 1200,
                 show_player: bool = True) -> None:
    from PIL import Image, ImageDraw

    header = 54
    img = Image.new("RGB", (size, size + header), (24, 27, 33))
    full = list(design.brushes)
    if not full:
        img.save(path)
        return
    cut = design.cutaway_brushes()
    has_cutaway = len(cut) < len(full)
    markers = entity_markers(design)
    ref = [player_reference(design)] if show_player else []
    half = size // 2
    for i, view in enumerate(VIEWS):
        use_cut = has_cutaway and i in CUTAWAY_VIEWS
        scene = (cut if use_cut else full) + markers + ref
        rx, ry = (i % 2) * half, header + (i // 2) * half
        render_view(img, (rx + 1, ry + 1, half - 2, half - 2), scene, view, highlight=ref,
                    fit=full + markers + ref, title=view.title + (" - cutaway" if use_cut else ""))
    draw = ImageDraw.Draw(img)
    lo, hi = design.bounds
    dims = f"{hi[0]-lo[0]:g} x {hi[1]-lo[1]:g} x {hi[2]-lo[2]:g} units"
    draw.text((12, 8), title or design.name, fill=(240, 240, 245), font=_font(20))
    info = f"{len(design.brushes)} brushes  |  {dims}  |  orange = player (32x32x56)"
    if any(e.classname.startswith("light") for e in design.entities):
        info += "  |  yellow = light"
    if any(not e.classname.startswith("light") and e.origin for e in design.entities):
        info += "  |  green = other entity"
    if subtitle:
        info = subtitle + "  |  " + info
    draw.text((12, 32), info, fill=(160, 170, 185), font=_font(14))
    img.save(path)


def render_thumbnail(design: Design, path: str, size: int = 360) -> None:
    from PIL import Image

    img = Image.new("RGB", (size, size), BACKGROUND)
    scene = list(design.brushes)
    if scene:
        ref = [player_reference(design)]
        render_view(img, (0, 0, size, size), scene + ref, VIEWS[0], highlight=ref, title="")
    img.save(path)
