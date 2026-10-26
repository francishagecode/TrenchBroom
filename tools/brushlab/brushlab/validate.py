"""Sanity checks that catch the usual problems before a design reaches the reviewer."""

from __future__ import annotations

import math
from typing import Dict, List

from .design import Design

# Quake's default world is +-4096; TrenchBroom's default is larger but compilers are not.
WORLD_LIMIT = 4096.0
MIN_THICKNESS = 1.0
MAX_BRUSHES = 2000


class Issue:
    def __init__(self, level: str, message: str):
        self.level = level  # "error", "warning" or "info"
        self.message = message

    def to_json(self) -> Dict[str, str]:
        return {"level": self.level, "message": self.message}

    def __repr__(self) -> str:
        return f"{self.level}: {self.message}"


def validate(design: Design, check_overlaps: bool = True, grid: float = 1.0) -> List[Issue]:
    issues: List[Issue] = []
    brushes = design.brushes
    if not brushes:
        issues.append(Issue("error", "design contains no brushes"))
        return issues
    if len(brushes) > MAX_BRUSHES:
        issues.append(Issue("warning", f"{len(brushes)} brushes is a lot for one prefab"))

    off_grid = 0
    for group, bs in design.groups.items():
        for i, b in enumerate(bs):
            where = f"{group}[{i}]"
            for v in b.vertices:
                if not all(math.isfinite(c) for c in v):
                    issues.append(Issue("error", f"{where}: non-finite vertex"))
                    break
                if any(abs(c) > WORLD_LIMIT for c in v):
                    issues.append(Issue("error", f"{where}: vertex {v} outside +-{WORLD_LIMIT:g}"))
                    break
            t = b.min_thickness()
            if t < MIN_THICKNESS:
                issues.append(Issue("error", f"{where}: sliver brush ({t:.3f} units thick)"))
            if any(any(abs(c / grid - round(c / grid)) > 1e-3 for c in v) for v in b.vertices):
                off_grid += 1
    if off_grid:
        issues.append(Issue(
            "info",
            f"{off_grid} brush(es) have off-grid vertices (expected for curves; "
            f"call .snapped() if they should be on the grid)"))

    if check_overlaps and len(brushes) <= 600:
        labels = [f"{g}[{i}]" for g, bs in design.groups.items() for i in range(len(bs))]
        overlaps = []
        for i in range(len(brushes)):
            for j in range(i + 1, len(brushes)):
                if brushes[i].overlaps(brushes[j], eps=0.5):
                    overlaps.append((labels[i], labels[j]))
        if overlaps:
            examples = ", ".join(f"{a} & {b}" for a, b in overlaps[:3])
            issues.append(Issue(
                "warning",
                f"{len(overlaps)} overlapping brush pair(s); fine for detail, wasteful for "
                f"structure (e.g. {examples})"))

    for e in design.entities:
        if e.origin is None and e.classname != "worldspawn":
            issues.append(Issue("warning", f"entity {e.classname} has no origin"))
        elif e.origin is not None and any(b.contains(e.origin, eps=-1.0) for b in brushes):
            issues.append(Issue("warning", f"entity {e.classname} at {e.origin} is inside a brush"))
    return issues
