# Brush Lab

Design brush prefabs (rooms, bridges, towers, decor and so on) as small Python scripts,
render review sheets, approve or send back variants in a browser, and paste approved
designs straight into TrenchBroom.

It is built so coding agents can iterate on their own. An agent writes a design, builds
it, looks at the PNG sheet, fixes what the validator or the picture shows, and leaves the
result for you to review. Your feedback is recorded where the agent can read it.

Requirements: Python 3.9+ and Pillow (`pip install pillow`) for previews. Nothing else.

## Quick start

```sh
cd tools/brushlab
./bl build              # build every design: validate, export .map, render previews
./bl serve              # review gallery at http://127.0.0.1:8765 with approve buttons
./bl status --pending   # what still needs review, with reviewer notes
```

## The review loop

1. **An agent proposes.** It writes `designs/<name>.py` with one or more `VARIANTS`, runs
   `./bl build <name>`, and checks `out/<name>/<variant>/sheet.png`.
2. **You review.** In `./bl serve`, each variant shows a sheet with two isometric views,
   a plan and an elevation. Ceilings and roofs are hidden in the cutaway views, and an
   orange 32x32x56 box (off the south-east corner) marks player scale. You also get an orbitable 3D view, the
   parameters (values that differ from the defaults are highlighted), validation results
   and the agent's notes. Then:
   - **Approve**: the `.map` is copied to `approved/<design>--<variant>.map`.
   - **Request changes** with a note, e.g. "railings too low, add a lamp at mid-span".
   - **Reject**.
3. **The agent iterates.** `./bl status --pending` shows your notes. When the agent changes
   a design, every variant whose geometry changed is marked *revised* and returns to the
   "Needs review" filter.

Review state lives in `review.json`, so it can be committed and shared.

## Getting designs into TrenchBroom

- In the gallery, click **Copy for TrenchBroom**, then press Cmd/Ctrl+V in TrenchBroom.
- From the shell: `./bl copy bridges@rope`, then paste.
- Or open or merge a file: `approved/*.map`, `out/<design>/<variant>/<design>--<variant>.map`,
  or `./bl export tower@wizard -o tower.map` (add `--valve` for Valve 220).

Each design group becomes a TrenchBroom group (nested with `/`, e.g. `Bridge/Railings`),
so a pasted prefab stays one selectable unit. Point entities such as lights are included.
Geometry is placed at its design coordinates; move it after pasting.

## Materials

Designs use semantic material names (`wall`, `floor`, `stone`, `wood`, `metal`, `trim`,
`roof`, ...). These pick the preview colors. `materials.json` maps them to the textures of
your game or texture set when exporting. Unmapped names are written as they are, so you
can also use real texture names directly.

## Writing a design

```python
"""What this is and why. Shown to the reviewer."""

from brushlab import *

TITLE = "Guard house"
TAGS = ["room", "outdoor"]

PARAMS = dict(width=256, depth=192, height=128)   # defaults
VARIANTS = {                                       # each one is reviewed separately
    "small": {},
    "long": dict(depth=384),
}

def build(p):                       # p.width etc.; p is a dict too
    d = Design()
    d.add(room(rect(0, 0, p.width, p.depth), height=p.height,
               openings=[Opening(edge=0, width=64, height=96, arched=True)]), "Guard house")
    d.add(stairs((p.width / 2, -80, -64), "+y", 64, 4), "Guard house/Steps")  # up to the door
    d.entity("light", (p.width / 2, p.depth / 2, p.height - 24), light=250)
    d.note("door faces south")
    return d
```

`./bl new <name>` scaffolds this. The four seed designs (`room_shapes`, `bridges`,
`decor_kit`, `tower`) are working examples of most of the API.

### Conventions

Quake units, Z up. The player is 32 x 32 x 56, steps are 16 high and 16 or more deep,
doors are 64-128 wide and 96-128 tall, and walls are 8-32 thick. Keep things on an 8 or
16 unit grid unless they are curved.

### API cheat sheet

| Area | Functions |
| --- | --- |
| Primitives | `box(mins, maxs)`, `block(center, size)`, `hull(points)`, `prism(poly2d, z0, z1)`, `slab(any_poly2d, z0, z1)` (concave is fine), `wedge`, `cylinder`, `frustum` (also cones), `sphere`, `segment(a, b, w, h)` (beam between points) |
| Curves | `ring(center, r_in, r_out, z0, z1, start, end, segments)`, `arch(center, span, thickness, depth, rise, axis, fill_to)`, `sweep(profile2d, path3d)`, `arc_path(...)` |
| Stairs | `stairs(start, "+x", width, steps)`, `spiral_stairs(center, r_in, r_out, steps)` |
| Rooms | `room(footprint, height, wall, openings=[Opening(edge, width, height, offset, sill, arched)])`, footprints `rect`, `l_shape`, `regular_polygon`, or any list of points |
| Brush ops | `.translate`, `.rotate_z(deg, origin)`, `.rotate(axis, deg, origin)`, `.scale`, `.mirror_x/y`, `.snapped(grid)`, `.subtract(cutter)`, `.intersect`, `.clip(plane)`, `.split(plane)`, `.with_material`, `.with_top` |
| Helpers | `subtract_all(brushes, cutter)`, `repeat(fn, count, step)`, `rotate_all`, `translate_all`, `convex_decompose_2d` |
| Design | `d.add(brushes, "Group/Sub")`, `d.entity(classname, origin, **props)`, `d.carve(cutter, groups)`, `d.note(text)`, `d.hide_in_cutaway(group)` |

Materials can be one name, or a dict such as `{"top": "grass", "default": "dirt"}`
(the keys are `top`, `bottom`, `side` and `default`).

### What the validator checks

Errors: sliver brushes (under 1 unit thick), non-finite or out-of-world vertices, and
designs that fail to build. Warnings: overlapping brushes (fine for detail, wasteful for
structure), entities buried inside brushes, and very large brush counts. Info: off-grid
vertices, which are expected for curves.

`subtract` already drops fragments thinner than 1 unit, which only ever widens a cut.

## Commands

| Command | Purpose |
| --- | --- |
| `./bl build [design[@variant] ...] [--force] [-v]` | build, validate and render; exits non-zero on errors |
| `./bl new <name>` | scaffold a design file |
| `./bl serve [--port N]` | review gallery with approve, request changes and reject |
| `./bl gallery [--inline]` | static gallery file (`--inline` embeds the images) |
| `./bl status [--pending]` | review status and reviewer notes |
| `./bl review <design@variant> approved\|changes\|rejected [-m note]` | record a review from the shell |
| `./bl copy <design@variant>` | copy to the clipboard for pasting into TrenchBroom |
| `./bl export <design@variant> [-o file] [--valve] [--snippet]` | write a `.map` |

Build outputs are cached on the design file and library sources. Everything under `out/`
is generated and ignored by git.
