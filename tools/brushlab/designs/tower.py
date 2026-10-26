"""A round tower with a spiral staircase around a central post.

The wall is a ring of segments with a door at the bottom and arrow-slit windows that
follow the stairs. The top is either crenellated, a conical roof, or open.
"""

from brushlab import *

TITLE = "Spiral tower"
TAGS = ["tower", "stairs", "structure"]

PARAMS = dict(
    radius=160,          # interior radius
    wall=24,
    height=448,
    segments=16,
    stair_width=96,
    step_height=16,
    degrees_per_step=20,
    top="crenellated",   # crenellated | cone | open
    windows=True,
)

VARIANTS = {
    "watchtower": dict(),
    "wizard": dict(top="cone", height=576, radius=128, stair_width=80, segments=12),
    "stair_core": dict(top="open", height=320, windows=False),
}


def build(p):
    d = Design()
    r_in, r_out = p.radius, p.radius + p.wall
    seg_deg = 360.0 / p.segments

    walls = ring((0, 0, 0), r_in, r_out, 0, p.height, segments=p.segments, material="stone",
                 start_deg=-90 - seg_deg / 2)  # one segment centered on -y for the door
    # Door facing -y (south).
    walls = subtract_all(walls, box((-32, -r_out - 8, 0), (32, -r_in + 8, 112)), "stone")

    # Stairs wind counter-clockwise from the door.
    steps = int((p.height - 64) // p.step_height)
    post_r = max(16, r_in - p.stair_width)
    stair_start = -90 + 30
    # The post's corners must stay inside the straight inner edge of each step.
    post_vertex_r = post_r * math.cos(math.radians(p.degrees_per_step / 2)) - 1
    d.add(cylinder((0, 0, 0), post_vertex_r, p.height, 12, "stone"), "Tower/Post")
    # Likewise the steps' outer corners must stay inside the wall's flat inner faces.
    stair_outer_r = r_in * math.cos(math.radians(seg_deg / 2)) - 1
    d.add(spiral_stairs((0, 0, 0), post_r, stair_outer_r, steps, p.step_height, p.degrees_per_step,
                        start_deg=stair_start, thickness=p.step_height * 2, material="stone"), "Tower/Stairs")

    if p.windows:
        # Arrow slits every quarter turn, at head height above the step below them.
        for k in range(1, steps, max(1, int(90 // p.degrees_per_step))):
            a = math.radians(stair_start + (k + 0.5) * p.degrees_per_step)
            z = (k + 1) * p.step_height + 48
            if z + 40 > p.height - 16:
                break
            cx, cy = (r_in + p.wall / 2) * math.cos(a), (r_in + p.wall / 2) * math.sin(a)
            slit = block((cx, cy, z + 20), (p.wall * 3, 8, 40)).rotate_z(math.degrees(a) + 90, (cx, cy, z))
            walls = subtract_all(walls, slit, "stone")
    d.add(walls, "Tower/Wall")

    # Top floor and floor slab.
    d.add(cylinder((0, 0, -16), r_out, 16, p.segments, "stone", start_deg=-90 - seg_deg / 2),
          "Tower/Floor")
    top_floor = ring((0, 0, 0), post_r, stair_outer_r, p.height - 16, p.height, segments=p.segments,
                     start_deg=stair_start + steps * p.degrees_per_step - 360 + 10,
                     end_deg=stair_start + steps * p.degrees_per_step - 10, material="wood")
    d.add(top_floor, "Tower/Top floor")

    if p.top == "crenellated":
        merlons = p.segments
        for i in range(merlons):
            if i % 2 == 0:
                d.add(ring((0, 0, 0), r_in, r_out, p.height, p.height + 40, segments=1,
                           start_deg=-90 - seg_deg / 2 + i * seg_deg, end_deg=-90 + seg_deg / 2 + i * seg_deg,
                           material="stone"), "Tower/Battlements")
        d.entity("light", (0, 0, p.height + 48), group="Tower/Battlements", light=300)
    elif p.top == "cone":
        eave = -90 - seg_deg / 2
        d.add(cylinder((0, 0, p.height), r_out + 24, 4, p.segments, "wood", start_deg=eave), "Tower/Roof")
        d.add(frustum((0, 0, p.height + 4), r_out + 24, 0, r_out * 1.6, p.segments, "roof",
                      start_deg=eave), "Tower/Roof")

    for k in range(0, steps, max(1, int(180 // p.degrees_per_step))):
        a = math.radians(stair_start + k * p.degrees_per_step + 10)
        z = k * p.step_height + 96
        d.entity("light", (round((r_in - 12) * math.cos(a)), round((r_in - 12) * math.sin(a)), z),
                 group="Tower/Wall", light=150)
    # Show the staircase in the cutaway views.
    d.hide_in_cutaway("Tower/Wall", "Tower/Top floor", "Tower/Battlements")
    d.note(f"{steps} steps of {p.step_height} units, {steps * p.degrees_per_step / 360:.1f} turns")
    return d
