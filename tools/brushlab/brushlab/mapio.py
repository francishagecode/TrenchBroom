"""Writes designs as Quake .map text that TrenchBroom can open or paste."""

from __future__ import annotations

import json
import os
from typing import Dict, List, Optional

from .brush import Brush, Face, _three_spread
from .design import Design
from .geometry import Vec3, cross, normalize


def fmt(v: float) -> str:
    r = round(v, 6)
    if abs(r - round(r)) < 1e-9:
        return str(int(round(r)))
    s = f"{r:.6f}".rstrip("0").rstrip(".")
    return "0" if s == "-0" else s


def load_materials(path: Optional[str]) -> Dict[str, str]:
    """Loads the semantic-material -> texture-name table, if there is one."""
    if path and os.path.exists(path):
        with open(path) as f:
            data = json.load(f)
        return {k: v for k, v in data.items() if not k.startswith("_")}
    return {}


def _valve_axes(normal: Vec3):
    """Paraxial texture axes like TrenchBroom's default for new faces."""
    ax = [abs(c) for c in normal]
    if ax[2] >= ax[0] and ax[2] >= ax[1]:
        u, v = (1.0, 0.0, 0.0), (0.0, -1.0, 0.0)
    elif ax[0] >= ax[1]:
        u, v = (0.0, 1.0, 0.0), (0.0, 0.0, -1.0)
    else:
        u, v = (1.0, 0.0, 0.0), (0.0, 0.0, -1.0)
    return u, v


def face_line(face: Face, materials: Dict[str, str], valve: bool) -> str:
    # Quake winding: normal = (p2 - p0) x (p1 - p0) points out of the brush. Our polygons
    # are counter-clockwise seen from outside, so we reverse the order.
    a, b, c = _three_spread(face.polygon)
    p0, p1, p2 = a, c, b
    pts = " ".join(f"( {fmt(p[0])} {fmt(p[1])} {fmt(p[2])} )" for p in (p0, p1, p2))
    tex = materials.get(face.material, face.material)
    if valve:
        u, v = _valve_axes(face.normal)
        return (
            f"{pts} {tex} [ {fmt(u[0])} {fmt(u[1])} {fmt(u[2])} 0 ] "
            f"[ {fmt(v[0])} {fmt(v[1])} {fmt(v[2])} 0 ] 0 1 1"
        )
    return f"{pts} {tex} 0 0 0 1 1"


def brush_text(brush: Brush, index: int, materials: Dict[str, str], valve: bool) -> List[str]:
    lines = [f"// brush {index}", "{"]
    lines += [face_line(f, materials, valve) for f in brush.faces]
    lines.append("}")
    return lines


def design_to_map(design: Design, materials: Optional[Dict[str, str]] = None,
                  valve: bool = False, game: str = "Quake", worldspawn: bool = True,
                  worldspawn_props: Optional[Dict[str, str]] = None) -> str:
    """Serializes the design. Every design group becomes a TrenchBroom group; nested group
    names ("A/B") become nested groups. With worldspawn=False the result is a snippet
    suitable for pasting into an open map."""
    materials = materials or {}
    out: List[str] = []
    entity_index = 0
    if worldspawn:
        out += [f"// Game: {game}", f"// Format: {'Valve' if valve else 'Standard'}"]
        out += [f"// entity {entity_index}", "{", '"classname" "worldspawn"']
        if valve:
            out.append('"mapversion" "220"')
        for k, v in (worldspawn_props or {}).items():
            out.append(f'"{k}" "{v}"')
        out.append("}")
        entity_index += 1

    # Assign group ids, creating implicit parents.
    group_ids: Dict[str, int] = {}

    def ensure(path: str) -> int:
        if path in group_ids:
            return group_ids[path]
        if "/" in path:
            ensure(path.rsplit("/", 1)[0])
        group_ids[path] = len(group_ids) + 1
        return group_ids[path]

    for name in design.groups:
        ensure(name)
    for e in design.entities:
        if e.group:
            ensure(e.group)

    for path, gid in group_ids.items():
        out += [f"// entity {entity_index}", "{", '"classname" "func_group"',
                '"_tb_type" "_tb_group"', f'"_tb_name" "{path.rsplit("/", 1)[-1]}"',
                f'"_tb_id" "{gid}"']
        if "/" in path:
            out.append(f'"_tb_group" "{group_ids[path.rsplit("/", 1)[0]]}"')
        for i, b in enumerate(design.groups.get(path, [])):
            out += brush_text(b, i, materials, valve)
        out.append("}")
        entity_index += 1

    for e in design.entities:
        out += [f"// entity {entity_index}", "{", f'"classname" "{e.classname}"']
        if e.origin is not None:
            out.append(f'"origin" "{fmt(e.origin[0])} {fmt(e.origin[1])} {fmt(e.origin[2])}"')
        for k, v in e.properties.items():
            out.append(f'"{k}" "{v}"')
        if e.group:
            out.append(f'"_tb_group" "{group_ids[e.group]}"')
        out.append("}")
        entity_index += 1

    return "\n".join(out) + "\n"
