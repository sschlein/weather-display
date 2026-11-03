"""A wall case for the weather display, in landscape, the USB cable leaving
at the bottom of a wing on the right that carries a sun-and-cloud emblem in
color.

Two parts and two inlays:
  front.stl       the shell: front with the display window, walls, the wing
  sun.stl, cloud.stl  the emblem, inlaid flush in the front (first layers)
  back.stl        the lid: two keyholes for the wall, posts that hold the
                  board against the front, two 3 x 10 mm sheet-metal screws
                  into the wing
  fit-test.stl    the front's first 4 mm, to try the window and the board

Printing: the STLs come lying as they print, the front face down, so the
emblem is in the first layers. In Bambu Studio, import front.stl, sun.stl
and cloud.stl in one go and answer "Yes" to loading them as one object with
several parts; give each part its filament. The lid prints on its own.

On the wall: two screws (3.5 mm, countersunk, like Spax 3.5 x 25) 110 mm
apart on one level, heads 4-5 mm off the wall; the case hangs on its
keyholes and drops 7 mm into place.

    uv run --no-project --python 3.12 --with build123d python enclosure/enclosure.py

Coordinates: x to the right, y up the wall, z out of the wall. Millimetres.
Values marked MEASURE are assumed and want checking against the board.
"""

from pathlib import Path

from build123d import (
    Align,
    Axis,
    Box,
    Circle,
    Cylinder,
    Pos,
    RectangleRounded,
    Rot,
    SlotCenterToCenter,
    Vector,
    chamfer,
    export_stl,
    extrude,
    offset,
)

OUT = Path(__file__).resolve().parent

# MARK: - The board

PCB_W, PCB_H, PCB_T = 102.0, 55.0, 1.6  # measured
# Measured: the glass reaches 8 mm past the image on the right.
GLASS_W = 11.0 + 73.44 + 8.0 + 0.5
MODULE_T = 6.0 - PCB_T  # measured: glass front to board back is 6 mm
BACK_T = 5.5  # measured: 11 mm over all, so 5 mm of parts behind the board
# The image: 73.44 x 48.96 mm for 480 x 320 at this size. Measured from the
# board's left and bottom edges (with the USB port on the right).
IMAGE_W, IMAGE_H = 73.44, 48.96
IMAGE_X, IMAGE_Y = 11.0, 2.25
# Posts on the lid press the board against the front near its corners, where
# these boards have their mounting holes and no parts.
POST_INSET = 4.0  # MEASURE: corner to the mounting holes' centers

# MARK: - The case

WALL = 2.4
FRONT = 2.0  # in front of the glass
LID = 5.0  # thick enough to hold the keyholes' pockets in itself
FIT = 0.3  # board to walls
WING = 35.0  # from the board's right edge to the case's outside: room for the plug
CORNER = 5.0  # outer corner radius, seen from the front
EDGE = 1.0  # chamfer around the front
WINDOW_MARGIN = 0.8  # window beyond the image on each side
WINDOW_BEVEL = 1.2

CABLE_D = 4.6  # MEASURE: the USB cable
# The plug points right; the cable bends down along the right wall, in a wide
# curve, and leaves at the bottom next to it.
CABLE_FROM_WALL = 1.0  # between the cable and the right wall, inside
# 3 x 10 mm sheet-metal screws hold the lid: pilot holes in the wing's
# bosses, heads sunk in the lid so it lies flat on the wall.
SCREW_PILOT = 2.5
SCREW_CLEAR = 3.4
SCREW_HEAD, SCREW_HEAD_T = 6.6, LID - 1.8
INLAY = 0.6  # emblem depth: three layers at 0.2 mm

# Derived: the cavity starts at x = y = 0 (the board's bottom left, less FIT).
CAV_W = FIT + PCB_W + WING - WALL
CAV_H = PCB_H + 2 * FIT
DEPTH_IN = MODULE_T + PCB_T + BACK_T  # inside, front to lid
W, H = CAV_W + 2 * WALL, CAV_H + 2 * WALL
D = LID + DEPTH_IN + FRONT
X0, Y0 = -WALL, -WALL  # the outside's bottom left
PCB_X, PCB_Y = FIT, FIT  # the board's bottom left
Z_LID = LID  # the lid's inside face
Z_BOARD_BACK = LID + BACK_T
Z_FRONT_IN = D - FRONT

# The wing's screws, clear of the plug and the cable.
WING_X0 = PCB_X + PCB_W  # the board's right edge
BOSS_D = 7.0
# The wing's screws: top right, and bottom left in the wing, out of the
# cable's way.
BOSSES = [
    (X0 + W - WALL - BOSS_D / 2 - 0.5, CAV_H - BOSS_D / 2 - 0.5),
    (WING_X0 + BOSS_D / 2 + 1.0, BOSS_D / 2 + 0.5),
]

# Keyholes, 110 mm apart on one level: one behind the board's left end,
# one in the wing's outer half, clear of the USB plug.
KEYHOLES = [(PCB_X + 14.0, CAV_H / 2 + 6), (PCB_X + 124.0, CAV_H / 2 + 6)]
# For 3.5 mm wood screws with 7 mm countersunk heads.
KEY_HEAD, KEY_SHANK, KEY_SLOT = 8.0, 4.2, 7.0
KEY_POCKET_W, KEY_POCKET_T = 8.4, 3.0  # in the lid, behind 2 mm, for the head


def outline(w, h, r):
    return RectangleRounded(w, h, r, align=(Align.MIN, Align.MIN))


def front_shell():
    shell = Pos(X0, Y0, 0) * extrude(outline(W, H, CORNER), D)
    shell = chamfer(shell.edges().group_by(Axis.Z)[-1], EDGE)
    shell -= Pos(0, 0, -1) * extrude(outline(CAV_W, CAV_H, max(0.5, CORNER - WALL)), Z_FRONT_IN + 1)

    # The window, beveled toward the front.
    wx, wy = PCB_X + IMAGE_X - WINDOW_MARGIN, PCB_Y + IMAGE_Y - WINDOW_MARGIN
    ww, wh = IMAGE_W + 2 * WINDOW_MARGIN, IMAGE_H + 2 * WINDOW_MARGIN
    shell -= Pos(wx, wy, Z_FRONT_IN - 1) * extrude(outline(ww, wh, 1.0), FRONT + 2)
    shell -= Pos(wx, wy, D - WINDOW_BEVEL) * extrude(outline(ww, wh, 1.0), WINDOW_BEVEL + 0.01, taper=-45)

    # Bosses in the wing for the lid's screws.
    for x, y in BOSSES:
        boss = Pos(x, y, Z_LID) * Cylinder(BOSS_D / 2, Z_FRONT_IN - Z_LID, align=(Align.CENTER, Align.CENTER, Align.MIN))
        shell += boss
        shell -= Pos(x, y, Z_LID - 1) * Cylinder(SCREW_PILOT / 2, Z_FRONT_IN - Z_LID - 1.5 + 1, align=(Align.CENTER, Align.CENTER, Align.MIN))

    # Slots in the left wall that the lid's tongues hook into.
    for y in (CAV_H * 0.25, CAV_H * 0.75):
        shell -= Pos(-1.2, y - 6, LID / 2 - 0.7) * Box(1.2 + 0.01, 12, 1.4, align=(Align.MIN, Align.MIN, Align.MIN))

    # The cable leaves through the bottom: a slot open to the back, so the
    # cable drops in before the lid goes on.
    cx = CAV_W - CABLE_FROM_WALL - CABLE_D / 2
    cable = Pos(cx, Y0 - 1, 0) * Rot(-90, 0, 0) * Cylinder(CABLE_D / 2, WALL + 2, align=(Align.CENTER, Align.CENTER, Align.MIN))
    cable = Pos(0, 0, LID + CABLE_D / 2 + 1.0) * cable
    shell -= cable
    shell -= Pos(cx - CABLE_D / 2, Y0 - 1, -1) * Box(CABLE_D, WALL + 2, LID + CABLE_D / 2 + 2.0, align=(Align.MIN, Align.MIN, Align.MIN))

    # Vents along the top and bottom of the wing.
    # Between the bottom left boss and the cable, and clear of the top right boss.
    for i in range(3):
        x = WING_X0 + 12 + i * 4.5
        for y in (Y0 - 1, CAV_H - 1):
            if abs(x - cx) < CABLE_D:
                continue
            shell -= Pos(x, y, Z_LID + 3) * Box(1.6, WALL + 2, DEPTH_IN - 7, align=(Align.CENTER, Align.MIN, Align.MIN))
    return shell


def emblem():
    """Sun and cloud as 2D faces on the front, centered on the panel right
    of the display window."""
    cx, cy = 0.0, 0.0
    s = 1.3
    sun_c = Vector(cx + 4 * s, cy + 6 * s)
    sun = Pos(sun_c.X, sun_c.Y) * Circle(6.2 * s)
    for i in range(8):
        sun += Pos(sun_c.X, sun_c.Y) * Rot(0, 0, i * 45) * Pos(10.4 * s, 0) * RectangleRounded(3.6 * s, 1.8 * s, 0.85 * s)
    cloud = (
        Pos(cx - 6.5 * s, cy - 6 * s) * Circle(4.6 * s)
        + Pos(cx - 0.5 * s, cy - 2.5 * s) * Circle(6.6 * s)
        + Pos(cx + 6.5 * s, cy - 5.5 * s) * Circle(5.0 * s)
        + Pos(cx - 0.5 * s, cy - 8.6 * s) * RectangleRounded(25 * s, 6.4 * s, 3.1 * s)
    )
    # A ring of the case's color between sun and cloud.
    sun -= offset(cloud, 1.0)
    # Drawn around the origin; centered as a whole on the panel between the
    # window and the case's right edge, and on the case's height.
    window_right = PCB_X + IMAGE_X + IMAGE_W + WINDOW_MARGIN
    box = (sun + cloud).bounding_box()
    dx = (window_right + X0 + W) / 2 - (box.min.X + box.max.X) / 2
    dy = (Y0 + Y0 + H) / 2 - (box.min.Y + box.max.Y) / 2
    return Pos(dx, dy) * sun, Pos(dx, dy) * cloud


def inlay(face):
    return Pos(0, 0, D - INLAY) * extrude(face, INLAY)


def back_lid():
    gap = 0.2
    lid = Pos(gap, gap, 0) * extrude(outline(CAV_W - 2 * gap, CAV_H - 2 * gap, max(0.5, CORNER - WALL - gap)), LID)
    # Tongues on the left edge, into the shell's slots.
    for y in (CAV_H * 0.25, CAV_H * 0.75):
        lid += Pos(-1.0, y - 5.5, LID / 2 - 0.6) * Box(1.0 + gap + 0.01, 11, 1.2, align=(Align.MIN, Align.MIN, Align.MIN))
    # Screws into the wing, heads sunk.
    for x, y in BOSSES:
        lid -= Pos(x, y, -1) * Cylinder(SCREW_CLEAR / 2, LID + 2, align=(Align.CENTER, Align.CENTER, Align.MIN))
        lid -= Pos(x, y, -0.01) * Cylinder(SCREW_HEAD / 2, SCREW_HEAD_T, align=(Align.CENTER, Align.CENTER, Align.MIN))
    # Posts that hold the board against the front.
    for px in (PCB_X + POST_INSET, PCB_X + PCB_W - POST_INSET):
        for py in (PCB_Y + POST_INSET, PCB_Y + PCB_H - POST_INSET):
            lid += Pos(px, py, LID) * Cylinder(2.5, BACK_T, align=(Align.CENTER, Align.CENTER, Align.MIN))
    # Keyholes: the head goes in at the round end, the case drops so the
    # shank slides up the slot, and the head sits in a pocket behind it.
    plate = LID - KEY_POCKET_T
    for kx, ky in KEYHOLES:
        lid -= Pos(kx, ky, -1) * Cylinder(KEY_HEAD / 2, LID + 2, align=(Align.CENTER, Align.CENTER, Align.MIN))
        lid -= Pos(kx, ky + KEY_SLOT / 2, -1) * extrude(SlotCenterToCenter(KEY_SLOT, KEY_SHANK, rotation=90), LID + 2)
        lid -= Pos(kx, ky + KEY_SLOT / 2, plate) * extrude(
            SlotCenterToCenter(KEY_SLOT, KEY_POCKET_W, rotation=90), KEY_POCKET_T + 0.01
        )
    return lid


def main():
    sun_face, cloud_face = emblem()
    front = front_shell()
    sun, cloud = inlay(sun_face), inlay(cloud_face)
    front -= sun
    front -= cloud
    back = back_lid()
    # The front's first 4 mm: a quick print to check the window over the
    # image and the board's fit before printing the whole case.
    fit_test = front & Pos(X0 - 1, Y0 - 1, D - 4) * Box(W + 2, H + 2, 5, align=(Align.MIN, Align.MIN, Align.MIN))
    # As they print: the front face down, flipped about x; the lid already
    # lies on its outside. Flipped the same way, the inlays stay in their
    # pockets.
    flip = Pos(0, H + 2 * Y0, D) * Rot(180, 0, 0)
    front, sun, cloud, fit_test = (flip * p for p in (front, sun, cloud, fit_test))
    fit_test = Pos(0, 0, -fit_test.bounding_box().min.Z) * fit_test
    parts = {"front": front, "sun": sun, "cloud": cloud, "back": back, "fit-test": fit_test}
    for name, part in parts.items():
        export_stl(part, str(OUT / f"{name}.stl"))
        bb = part.bounding_box()
        print(f"{name}.stl  {bb.size.X:.1f} x {bb.size.Y:.1f} x {bb.size.Z:.1f} mm, valid {part.is_valid}")
    print(f"case: {W:.1f} x {H:.1f} x {D:.1f} mm; keyholes {KEYHOLES[1][0] - KEYHOLES[0][0]:.0f} mm apart")


if __name__ == "__main__":
    main()
