"""Mesh export for the interactive WebGL viewer in the gallery."""

from __future__ import annotations

from typing import Any, Dict

from .design import Design
from .render import KNOWN_COLORS, entity_markers, material_color, player_reference


def _mesh(brushes, ref=()) -> Dict[str, Any]:
    pos, nrm, col, edges = [], [], [], []
    for b in brushes:
        for f in b.faces:
            c = KNOWN_COLORS["player"] if b in ref else material_color(f.material)
            c = [round(x / 255.0, 3) for x in c]
            n = [round(x, 4) for x in f.normal]
            poly = f.polygon
            for i in range(1, len(poly) - 1):
                for p in (poly[0], poly[i], poly[i + 1]):
                    pos += [round(p[0], 2), round(p[1], 2), round(p[2], 2)]
                    nrm += n
                    col += c
            for i in range(len(poly)):
                a, b2 = poly[i], poly[(i + 1) % len(poly)]
                edges += [round(a[0], 2), round(a[1], 2), round(a[2], 2),
                          round(b2[0], 2), round(b2[1], 2), round(b2[2], 2)]
    return {"pos": pos, "nrm": nrm, "col": col, "edges": edges}


def mesh_json(design: Design, player: bool = True) -> Dict[str, Any]:
    """Two meshes: `base` is always drawn, `roof` holds ceilings/roofs that the viewer
    can hide (see Design.is_cutaway)."""
    base, roof = [], []
    for g, bs in design.groups.items():
        (roof if design.is_cutaway(g) else base).extend(bs)
    ref = [player_reference(design)] if player and design.brushes else []
    lo, hi = design.bounds
    return {
        "base": _mesh(base + entity_markers(design) + ref, ref),
        "roof": _mesh(roof),
        "bounds": [list(lo), list(hi)],
    }
