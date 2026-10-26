"""Small props for dressing rooms: crates, barrels, furniture, shelves and sconces.

Everything stands on z = 0. Props are detail brushes; overlaps between parts of the
same prop are intentional (e.g. barrel hoops) and fine for func_detail / func_group.
"""

import random

from brushlab import *

TITLE = "Decor kit"
TAGS = ["decor", "props", "detail"]

PARAMS = dict(
    scene="storage",     # storage | tavern | library | sconce
    seed=7,
    crate=32,
    barrel_radius=14,
    barrel_height=44,
)

VARIANTS = {
    "storage": dict(scene="storage"),
    "tavern": dict(scene="tavern"),
    "library": dict(scene="library"),
    "sconce": dict(scene="sconce"),
}


def crate(center, size, angle=0.0):
    """A crate with a raised frame on each side face."""
    x, y, z = center
    h = size / 2
    t = 3  # frame inset
    parts = [box((x - h + t, y - h + t, z + t), (x + h - t, y + h - t, z + size - t), "wood")]
    for px in (x - h, x + h - t):
        for py in (y - h, y + h - t):  # corner posts
            parts.append(box((px, py, z + t), (px + t, py + t, z + size - t), "plank"))
    for zz in (z, z + size - t):  # top and bottom rims
        parts.append(box((x - h, y - h, zz), (x + h, y + h, zz + t), "plank"))
    if angle:
        parts = [b.rotate_z(angle, (x, y, z)) for b in parts]
    return parts


def barrel(center, r, height, hoops=True):
    x, y, z = center
    q = height / 4
    parts = [
        frustum((x, y, z), r * 0.85, r, q, 12, "wood"),
        cylinder((x, y, z + q), r, 2 * q, 12, "wood"),
        frustum((x, y, z + 3 * q), r, r * 0.85, q, 12, "wood"),
    ]
    if hoops:
        for hz in (z + q - 1, z + 3 * q - 1):
            parts.append(cylinder((x, y, hz), r + 1, 2, 12, "metal"))
    return parts


def table(center, w, d, h=36):
    x, y, z = center
    parts = [box((x - w / 2, y - d / 2, z + h - 4), (x + w / 2, y + d / 2, z + h), "plank")]
    for sx in (-1, 1):
        for sy in (-1, 1):
            lx, ly = x + sx * (w / 2 - 6), y + sy * (d / 2 - 6)
            parts.append(box((lx - 2, ly - 2, z), (lx + 2, ly + 2, z + h - 4), "wood"))
    return parts


def bench(center, length, along="x"):
    x, y, z = center
    hw, hl = 6, length / 2
    if along == "x":
        seat = box((x - hl, y - hw, z + 16), (x + hl, y + hw, z + 20), "plank")
        legs = [box((x + s * (hl - 4) - 2, y - hw, z), (x + s * (hl - 4) + 2, y + hw, z + 16), "wood") for s in (-1, 1)]
    else:
        seat = box((x - hw, y - hl, z + 16), (x + hw, y + hl, z + 20), "plank")
        legs = [box((x - hw, y + s * (hl - 4) - 2, z), (x + hw, y + s * (hl - 4) + 2, z + 16), "wood") for s in (-1, 1)]
    return [seat] + legs


def bookshelf(origin, width, height, depth=16, shelves=4, rng=None):
    x, y, z = origin  # back-left-bottom corner; the shelf faces -y
    rng = rng or random.Random(0)
    t = 2
    parts = [
        box((x, y, z), (x + t, y + depth, z + height), "wood"),
        box((x + width - t, y, z), (x + width, y + depth, z + height), "wood"),
        box((x + t, y + depth - t, z), (x + width - t, y + depth, z + height), "wood"),
    ]
    gap = (height - t) // shelves
    for i in range(shelves + 1):
        zz = z + i * gap
        parts.append(box((x + t, y, zz), (x + width - t, y + depth - t, zz + t), "plank"))
        if i == shelves:
            break
        bx = x + t + 1
        while bx < x + width - t - 4:
            bw = rng.choice([3, 4, 4, 5, 6])
            bh = round(rng.uniform(0.55, 0.85) * (gap - t))
            if bx + bw > x + width - t - 1:
                break
            if rng.random() < 0.9:
                lean = 0 if rng.random() < 0.85 else 1
                book = box((bx, y + 2, zz + t), (bx + bw, y + depth - t - 1, zz + t + bh), rng.choice(["book", "cloth", "book2"]))
                parts.append(book.rotate((0, 1, 0), -8, (bx + bw, y, zz + t)) if lean else book)
            bx += bw
    return parts


def sconce(origin):
    """Wall torch: bracket out of a wall at y = origin.y, light above the cup."""
    x, y, z = origin
    parts = [
        box((x - 6, y - 3, z - 10), (x + 6, y, z + 10), "metal"),            # wall plate
        segment((x, y - 3, z - 4), (x, y - 14, z + 4), 3, 3, "metal"),      # arm
        frustum((x, y - 16, z + 2), 3, 7, 10, 8, "metal"),                  # cup
    ]
    return parts, (x, y - 16, z + 22)


def build(p):
    d = Design()
    rng = random.Random(p.seed)
    s = p.crate
    if p.scene == "storage":
        d.add(crate((0, 0, 0), s), "Storage/Crates")
        d.add(crate((s + 2, 0, 0), s), "Storage/Crates")
        d.add(crate((s / 2 + 1, 2, s), s, angle=rng.uniform(8, 20)), "Storage/Crates")
        d.add(crate((0, s + 6, 0), s * 1.5, angle=-6), "Storage/Crates")
        r = p.barrel_radius
        for bx, by in [(80, -6), (110, 4), (92, 32)]:
            d.add(barrel((bx, by, 0), r, p.barrel_height), "Storage/Barrels")
        # One barrel lying on its side along +y, resting on the floor.
        lying = barrel((132, 40, 0), r, p.barrel_height)
        d.add([b.rotate((1, 0, 0), -90, (132, 40, 0)).translate(0, 0, r + 1) for b in lying],
              "Storage/Barrels")
    elif p.scene == "tavern":
        d.add(table((0, 0, 0), 96, 48), "Tavern/Table")
        d.add(bench((0, -40, 0), 88), "Tavern/Benches")
        d.add(bench((0, 40, 0), 88), "Tavern/Benches")
        d.add(table((140, 0, 0), 48, 48), "Tavern/Table")
        for ang in (0, 120, 240):
            cx = 140 + 34 * math.cos(math.radians(ang))
            cy = 34 * math.sin(math.radians(ang))
            d.add(cylinder((cx, cy, 0), 8, 18, 8, "wood"), "Tavern/Stools")
        for k in range(3):  # mugs and a candle on the big table
            d.add(cylinder((-30 + k * 22, rng.uniform(-10, 10), 36), 3, 6, 8, "metal"), "Tavern/Props")
        d.add(cylinder((10, 0, 36), 2, 8, 6, "marble"), "Tavern/Props")
        d.entity("light", (10, 0, 52), group="Tavern/Props", light=120, _color="1 0.8 0.5")
    elif p.scene == "library":
        for i in range(3):
            d.add(bookshelf((i * 66, 0, 0), 64, 112, rng=rng), f"Library/Shelf {i + 1}")
        d.add(table((99, -64, 0), 80, 40, 32), "Library/Desk")
        d.add(hull([(80, -72, 32), (96, -72, 32), (80, -60, 32), (96, -60, 32), (82, -70, 34), (94, -70, 34)], "cloth"),
              "Library/Desk")  # an open book
    elif p.scene == "sconce":
        d.add(box((-64, 0, 0), (64, 16, 128), "brick"), "Wall")
        for x in (-32, 32):
            parts, light = sconce((x, 0, 72))
            d.add(parts, "Sconces")
            d.entity("light", light, group="Sconces", light=180, _color="1 0.7 0.4")
    else:
        raise ValueError(f"unknown scene {p.scene}")
    d.note("overlaps between parts of the same prop are intentional; make props func_detail")
    return d
