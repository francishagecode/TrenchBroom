"""The Design container: named groups of brushes plus point entities."""

from __future__ import annotations

from collections import OrderedDict
from typing import Dict, Iterable, List, Optional, Sequence, Union

from .brush import Brush, subtract_all
from .geometry import Vec3

BrushOrList = Union[Brush, Iterable[Brush], Dict[str, List[Brush]]]


class Entity:
    def __init__(self, classname: str, origin: Optional[Vec3] = None, group: Optional[str] = None,
                 **properties):
        self.classname = classname
        self.origin = origin
        self.group = group
        self.properties = {k: str(v) for k, v in properties.items()}


class Design:
    """Collects brushes into named groups. Group names may be nested with "/", for
    example "Bridge/Railings". Every group becomes a TrenchBroom group when exported."""

    def __init__(self, name: str = "Design"):
        self.name = name
        self.groups: "OrderedDict[str, List[Brush]]" = OrderedDict()
        self.entities: List[Entity] = []
        self.notes: List[str] = []
        # Groups hidden in the cutaway preview views. By default any group whose name
        # ends in "ceiling" or "roof" is hidden, so you can see into rooms.
        self.cutaway: List[str] = []

    def add(self, brushes: BrushOrList, group: Optional[str] = None) -> "Design":
        """Adds a brush, a list of brushes, or the dict returned by room()."""
        if isinstance(brushes, dict):
            for key, value in brushes.items():
                self.add(value, f"{group}/{key}" if group else key)
            return self
        group = group or self.name
        items = [brushes] if isinstance(brushes, Brush) else list(brushes)
        self.groups.setdefault(group, []).extend(items)
        return self

    def entity(self, classname: str, origin: Optional[Vec3] = None, group: Optional[str] = None,
               **properties) -> "Design":
        """Adds a point entity, e.g. d.entity("light", (0, 0, 96), light=200)."""
        self.entities.append(Entity(classname, origin, group, **properties))
        return self

    def carve(self, cutter: Brush, groups: Optional[Sequence[str]] = None,
              cap_material: Optional[str] = None) -> "Design":
        """Subtracts the cutter from every brush (or only from the named groups)."""
        for name in list(self.groups):
            if groups is None or name in groups or any(name.startswith(g + "/") for g in groups):
                self.groups[name] = subtract_all(self.groups[name], cutter, cap_material)
        return self

    def note(self, text: str) -> "Design":
        """Free-form notes shown to the reviewer next to the previews."""
        self.notes.append(text)
        return self

    def hide_in_cutaway(self, *groups: str) -> "Design":
        """Hides these groups (and their subgroups) in the cutaway preview views."""
        self.cutaway.extend(groups)
        return self

    def is_cutaway(self, group: str) -> bool:
        leaf = group.rsplit("/", 1)[-1].lower()
        if leaf.endswith("ceiling") or leaf.endswith("roof"):
            return True
        return any(group == g or group.startswith(g + "/") for g in self.cutaway)

    def cutaway_brushes(self) -> List[Brush]:
        return [b for g, bs in self.groups.items() if not self.is_cutaway(g) for b in bs]

    @property
    def brushes(self) -> List[Brush]:
        return [b for bs in self.groups.values() for b in bs]

    @property
    def bounds(self):
        bs = self.brushes
        if not bs:
            return ((0.0, 0.0, 0.0), (0.0, 0.0, 0.0))
        los = [b.bounds[0] for b in bs]
        his = [b.bounds[1] for b in bs]
        return (
            tuple(min(p[i] for p in los) for i in range(3)),
            tuple(max(p[i] for p in his) for i in range(3)),
        )
