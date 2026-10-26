"""Brush Lab: write brush prefabs as small Python scripts, preview them, review them,
and paste the approved ones into TrenchBroom.

    from brushlab import *

Everything a design script normally needs is exported here.
"""

from .brush import Brush, Face, rotate_point, rotate_point_z, subtract_all
from .design import Design, Entity
from .geometry import Plane, convex_decompose_2d, offset_polygon_2d
from .shapes import *  # noqa: F401,F403
from .shapes import __all__ as _shape_names

__all__ = [
    "Brush",
    "Design",
    "Entity",
    "Face",
    "Plane",
    "convex_decompose_2d",
    "offset_polygon_2d",
    "rotate_point",
    "rotate_point_z",
    "subtract_all",
    "math",
] + list(_shape_names)

import math  # noqa: E402  (handy in design scripts)
