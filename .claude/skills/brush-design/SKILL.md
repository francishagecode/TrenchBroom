---
name: brush-design
description: Design new brush geometry (rooms, room shapes, corridors, bridges, towers, stairs, arches, decor and props, or any prefab) with Brush Lab and hand it to the user for review. Use this when asked to design, iterate on, or brainstorm level geometry or brushes, to propose variations, or to act on review feedback for Brush Lab designs.
---

# Designing brushes with Brush Lab

Brush Lab (`tools/brushlab/`) turns small Python design scripts into TrenchBroom-ready
`.map` prefabs, review sheets and a review gallery. You propose designs; the user
approves them. Read `tools/brushlab/README.md` for the full API. The seed designs in
`tools/brushlab/designs/` are working examples to copy from.

All commands run from `tools/brushlab/`.

## The loop

1. **Check for feedback first.** Run `./bl status --pending`. Reviewer notes on designs
   marked `changes` are your top priority. Address every point in the note.
2. **Write or edit** `designs/<snake_case_name>.py`, using `./bl new <name>` for a new
   one. Put every dimension the reviewer might want to tweak in `PARAMS`, and offer 2-4
   meaningfully different `VARIANTS` (shape, scale, style), not near-duplicates.
3. **Build:** `./bl build <name>`. It must report no `ERR` or `FAIL` lines. Fix warnings
   unless they are intentional (overlaps inside detail props are fine); say so in a
   `d.note(...)` if you keep one.
4. **Look at every sheet** (`out/<name>/<variant>/sheet.png`) with your image-reading
   tool. Check against the player box (orange, 32x32x56): Is the doorway at least 64 wide
   and 96 tall? Are steps 16 high? Is there headroom? Do pieces meet without gaps or
   floating parts? Does the plan match the intent? The cutaway views hide groups named
   `*ceiling` / `*roof` and anything passed to `d.hide_in_cutaway(...)`, so make sure the
   interesting interior is visible there.
5. **Iterate** until the sheet looks right. Expect to need several passes.
6. **Hand off.** Tell the user what you made, list the variants, and point them to
   `./bl serve` (or the sheet paths). Mention trade-offs and anything you were unsure about.

## Rules

- Never approve or reject your own designs, and never edit `review.json` by hand. Review
  decisions belong to the user.
- Do not change a variant the user approved unless asked. Add a new variant instead. Any
  geometry change to an approved variant marks it "revised" and sends it back to review.
- Keep designs sealed where they are meant to be playable spaces (rooms have a floor,
  walls and ceiling), and keep everything within +-4096 units.
- Use semantic materials (`wall`, `floor`, `ceiling`, `stone`, `wood`, `plank`, `metal`,
  `trim`, `roof`, `glass`, `grass`, `dirt`, ...). They drive preview colors and are
  mapped to real textures through `materials.json`.
- Group sensibly (`"Bridge/Deck"`, `"Bridge/Railings"`). Groups become TrenchBroom
  groups, so the user can select, hide and reuse parts.
- Write the module docstring for the reviewer: what it is, where you walk, and what the
  parameters do.
- If the library is missing a primitive you need, add it to `brushlab/shapes.py` (with a
  docstring) rather than hacking around it in a design, and rebuild all designs
  (`./bl build`) to make sure nothing else broke.

## Geometry tips

- Brushes must be convex. Build concave things from several brushes: `slab()` and
  `room()` decompose footprints for you, and `subtract()` carves holes.
- To cut a doorway, prefer `Opening(...)` in `room()`. For other carving use
  `d.carve(cutter, groups=[...])` or `subtract_all`.
- Use `arch(..., fill_to=z)` for arches set into walls and bridges, `sweep()` for rails,
  trims and pipes, and `ring()` for round walls and towers.
- Keep structural pieces on an 8 or 16 unit grid. Curved pieces will be off-grid, which
  is fine.
