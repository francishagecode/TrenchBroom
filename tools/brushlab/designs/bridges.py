"""Bridges spanning a gap along X; you walk across in the X direction.

Stone styles are built from abutments, piers with cutwaters, voussoir arches with
spandrel fill, a deck, and parapets or a balustrade. The rope style is a sagging
plank deck with rope handrails. The ground (top of the abutments) is at z = deck.
"""

from brushlab import *

TITLE = "Bridges"
TAGS = ["bridge", "outdoor", "structure"]

PARAMS = dict(
    style="stone",        # stone | rope
    span=512,             # total length including abutments
    width=128,            # deck width (Y)
    deck=192,             # deck top height above the riverbed (z = 0)
    arches=2,
    abutment=64,
    pier=48,
    ring=16,              # voussoir ring thickness
    rise_ratio=0.5,       # arch rise / opening width (0.5 = semicircle)
    deck_thickness=16,
    parapet_height=40,
    parapet_thickness=12,
    balustrade=False,
    lamps=False,
    sag=32,               # rope bridge sag at mid-span
)

VARIANTS = {
    "single_arch": dict(span=384, arches=1, deck=224),
    "triple_arch": dict(span=768, arches=3, rise_ratio=0.38),
    "balustrade_lamps": dict(span=640, arches=2, balustrade=True, lamps=True, width=160),
    "rope": dict(style="rope", span=512, width=64, deck=192),
}


def stone(p, d):
    L, W, H = p.span, p.width, p.deck
    deck_bottom = H - p.deck_thickness
    n = p.arches
    opening = (L - 2 * p.abutment - (n - 1) * p.pier) / n
    rise = opening * p.rise_ratio
    spring = deck_bottom - rise - p.ring - 8  # keep a little masonry over the crown
    if spring <= 16:
        raise ValueError(f"deck too low for {n} arch(es) of {opening:.0f} units")

    # Abutments: full width below the springing line, minus the arch ring above it.
    for x0, x1, ring_side in ((0, p.abutment, "right"), (L - p.abutment, L, "left")):
        d.add(box((x0, 0, 0), (x1, W, spring), "stone"), "Bridge/Supports")
        ax0, ax1 = (x0, x1 - p.ring) if ring_side == "right" else (x0 + p.ring, x1)
        d.add(box((ax0, 0, spring), (ax1, W, deck_bottom), "stone"), "Bridge/Supports")

    # Piers with pointed cutwaters on both faces.
    for i in range(1, n):
        x0 = p.abutment + i * opening + (i - 1) * p.pier
        x1 = x0 + p.pier
        xm = (x0 + x1) / 2
        d.add(box((x0, 0, 0), (x1, W, spring), "stone"), "Bridge/Supports")
        if p.pier > 2 * p.ring:
            d.add(box((x0 + p.ring, 0, spring), (x1 - p.ring, W, deck_bottom), "stone"),
                  "Bridge/Supports")
        tip = p.pier * 0.6
        for y_face, y_tip in ((0, -tip), (W, W + tip)):
            d.add(hull([(x0, y_face, 0), (x1, y_face, 0), (xm, y_tip, 0),
                        (x0, y_face, spring - 24), (x1, y_face, spring - 24), (xm, y_tip, spring - 48)],
                       "stone"), "Bridge/Supports")

    # Arches with spandrels filling up to the deck.
    for i in range(n):
        xc = p.abutment + i * (opening + p.pier) + opening / 2
        d.add(arch((xc, W / 2, spring), opening, p.ring, W, rise=rise, segments=max(6, int(opening // 32)),
                   axis="y", fill_to=deck_bottom, material="stone"), "Bridge/Arches")

    d.add(box((0, 0, deck_bottom), (L, W, H), {"top": "dirt", "default": "stone"}), "Bridge/Deck")

    # Parapets or a balustrade on both edges.
    t = p.parapet_thickness
    for y0 in (0, W - t):
        if p.balustrade:
            d.add(box((0, y0, H), (L, y0 + t, H + 6), "marble"), "Bridge/Balustrade")
            count = int(L // 24)
            for k in range(count):
                x = (k + 0.5) * L / count
                d.add(frustum((x, y0 + t / 2, H + 6), 5, 3, p.parapet_height - 14, 8, "marble"),
                      "Bridge/Balustrade")
            d.add(box((0, y0 - 2, H + p.parapet_height - 8), (L, y0 + t + 2, H + p.parapet_height),
                      "marble"), "Bridge/Balustrade")
        else:
            d.add(box((0, y0, H), (L, y0 + t, H + p.parapet_height - 6), "stone"), "Bridge/Parapets")
            d.add(box((0, y0 - 2, H + p.parapet_height - 6), (L, y0 + t + 2, H + p.parapet_height),
                      "trim"), "Bridge/Parapets")

    if p.lamps:
        posts = [p.abutment / 2, L - p.abutment / 2]
        posts += [p.abutment + i * opening + (i - 0.5) * p.pier for i in range(1, n)]
        for x in posts:
            for y in (t / 2, W - t / 2):
                top = H + p.parapet_height + 64
                d.add(box((x - 6, y - 6, H + p.parapet_height), (x + 6, y + 6, top), "metal"), "Bridge/Lamps")
                # An open lantern cage so the light entity sits in free space.
                for sx in (-7, 5):
                    for sy in (-7, 5):
                        d.add(box((x + sx, y + sy, top), (x + sx + 2, y + sy + 2, top + 16), "metal"),
                              "Bridge/Lamps")
                d.add(frustum((x, y, top + 16), 11, 0, 10, 4, "metal", start_deg=45), "Bridge/Lamps")
                d.entity("light", (x, y, top + 8), group="Bridge/Lamps", light=200)
    d.note(f"{n} arch(es), clear opening {opening:.0f} units, headroom {spring + rise:.0f} units")


def rope(p, d):
    L, W, H = p.span, p.width, p.deck

    def z_at(x):  # parabolic sag between the two anchors
        t = x / L
        return H - p.sag * 4 * t * (1 - t)

    # Anchor platforms on both banks.
    for x0, x1 in ((-96, 0), (L, L + 96)):
        d.add(box((x0, -16, 0), (x1, W + 16, H), {"top": "grass", "default": "dirt"}), "Bridge/Banks")

    # Planks with small gaps, each tilted to follow the curve.
    plank, gap = 14, 3
    x = 2.0
    while x + plank < L - 2:
        x0, x1 = x, x + plank
        z0, z1 = z_at(x0), z_at(x1)
        d.add(hull([(x0, 0, z0 - 4), (x1, 0, z1 - 4), (x0, W, z0 - 4), (x1, W, z1 - 4),
                    (x0, 0, z0), (x1, 0, z1), (x0, W, z0), (x1, W, z1)], "plank"), "Bridge/Planks")
        x += plank + gap

    # Posts at both ends, handrail ropes and hangers.
    rail_h = 44
    for y in (-4, W + 4):
        for xp in (-8, L + 8):
            d.add(block((xp, y, H + 32), (8, 8, 64), "wood"), "Bridge/Posts")
        # The rope runs from post face to post face, following the deck's sag.
        path = [(-4, y, H + rail_h)] + [(L * k / 12, y, z_at(L * k / 12) + rail_h) for k in range(1, 12)]
        path.append((L + 4, y, H + rail_h))
        d.add(sweep([(-1.5, -1.5), (1.5, -1.5), (1.5, 1.5), (-1.5, 1.5)], path, "rope"), "Bridge/Ropes")
        for k in range(2, 11, 2):
            px, py, pz = path[k]
            d.add(segment((px, py, z_at(px)), (px, py, pz - 1.5), 2, 2, "rope"), "Bridge/Ropes")
    d.note("planks follow a parabolic sag; ropes are thin detail brushes")


def build(p):
    d = Design()
    (rope if p.style == "rope" else stone)(p, d)
    return d
