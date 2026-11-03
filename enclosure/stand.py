"""A desk stand for the case: it stands in a cradle, leaning back, and the
USB cable runs from the case's bottom right through a channel under it to
the middle and out at the back.

    stand.stl   prints as it stands, in one piece, without supports

The case drops into the cradle from above: a low lip in front (below the
image), a rest behind, and walls left and right that hold it from sliding
sideways. The cable leaves the case at the bottom right and
lies in a channel in the cradle's floor, hidden under the case; in the
middle it turns toward the back in a wide curve and leaves under the foot.

    uv run --no-project --python 3.12 --with build123d python enclosure/stand.py

Writes stand.json too: where the case sits, for enclosure/render.py.
"""

import json
from math import cos, radians, sin
from pathlib import Path

from build123d import Align, Box, FilletPolyline, Plane, Pos, RectangleRounded, export_stl, extrude, trace

import enclosure as case

OUT = Path(__file__).resolve().parent

TILT = 15.0  # degrees back from upright
FIT = 0.4  # case to cradle, front and back
FLOOR = 16.0  # under the case's front edge, along the case: room for the channel
LIP_H, LIP_T = 3.5, 3.0  # in front of the case: below the image's edge
REST_H, REST_T = 32.0, 5.0  # behind the case
SIDE_H, SIDE_T = 15.0, 3.0  # walls left and right: the case can't slide out sideways
FOOT_DEPTH, FOOT_T = 62.0, 5.0  # from the front, so tapping doesn't tip it over

CHANNEL_W = case.CABLE_D + 0.8
CHANNEL_DEPTH = 9.0  # the cable bends from down into the channel: room for it
TURN = 12.0  # the channel's turn toward the back

T = radians(TILT)
UP = (0.0, sin(T), cos(T))  # along the case, toward its top
OUT_DIR = (0.0, -cos(T), sin(T))  # out of the case's front
# The cradle's own frame: x along the case's width, y up the case, z out of
# its front; origin at the case's bottom back left edge. Placed so the front
# edge of the floor sits on the table at y = 0.
s0, t0 = -FLOOR, case.D + LIP_T
ORIGIN = tuple(-(s0 * UP[i] + t0 * OUT_DIR[i]) for i in range(3))
CRADLE = Plane(origin=ORIGIN, x_dir=(1, 0, 0), z_dir=OUT_DIR)


def local_box(x0, x1, y0, y1, z0, z1):
    return CRADLE * Pos(x0, y0, z0) * Box(x1 - x0, y1 - y0, z1 - z0, align=(Align.MIN, Align.MIN, Align.MIN))


def stand():
    w, d = case.W, case.D
    x0, x1 = -FIT - SIDE_T, w + FIT + SIDE_T
    body = local_box(x0, x1, -FLOOR, SIDE_H, -REST_T, d + LIP_T)
    body += local_box(x0, x1, SIDE_H - 1, REST_H, -REST_T, 0)
    body += Pos(x0, 0, 0) * Box(x1 - x0, FOOT_DEPTH, FOOT_T, align=(Align.MIN, Align.MIN, Align.MIN))
    # Flat on the table.
    body &= Pos(x0, 0, 0) * Box(x1 - x0, 500, 500, align=(Align.MIN, Align.CENTER, Align.MIN))

    # The case, with its rounded corners and a little play all round; above
    # the lip, its front.
    outline = RectangleRounded(w + 2 * FIT, 400, case.CORNER + FIT, align=(Align.MIN, Align.MIN))
    body -= CRADLE * Pos(-FIT, 0, -FIT) * extrude(outline, d + 2 * FIT)
    body -= local_box(-FIT, w + FIT, LIP_H, 200, d, d + LIP_T + 1)

    # The channel: from under the case's cable exit along the floor to the
    # middle, then toward the back, down the floor's slope and out under the
    # foot. The case's cable leaves at x (from its left edge) and t (from
    # its back), going down.
    cable_x = case.CAV_W - case.CABLE_FROM_WALL - case.CABLE_D / 2 - case.X0
    cable_t = case.LID + case.CABLE_D / 2 + 1.0
    mid = w / 2
    path = FilletPolyline((cable_x + 0.6, cable_t), (mid, cable_t), (mid, -200), radius=TURN)
    channel = extrude(trace(path, CHANNEL_W), CHANNEL_DEPTH + 1)
    # Drawn on the floor, looking down the case: x across, y along t; cut
    # from just above the floor down into it.
    floor = Plane(origin=CRADLE.origin + CRADLE.y_dir * 1.0, x_dir=(1, 0, 0), z_dir=-CRADLE.y_dir)
    body -= floor * channel
    # Out under the foot, from where the sloping channel reaches the table.
    reach = CRADLE.from_local_coords((mid, -CHANNEL_DEPTH, cable_t - TURN)).Y
    body -= Pos(mid - CHANNEL_W / 2, reach, -1) * Box(
        CHANNEL_W, FOOT_DEPTH - reach + 1, CHANNEL_W + 1, align=(Align.MIN, Align.MIN, Align.MIN)
    )
    return body


def main():
    part = stand()
    export_stl(part, str(OUT / "stand.stl"))
    bb = part.bounding_box()
    print(f"stand.stl  {bb.size.X:.1f} x {bb.size.Y:.1f} x {bb.size.Z:.1f} mm, valid {part.is_valid}")
    # Where the case (as it hangs on the wall: x right, y up, z out) sits.
    x, u, n = (1, 0, 0), UP, OUT_DIR
    o = [ORIGIN[i] - case.X0 * x[i] - case.Y0 * u[i] for i in range(3)]
    matrix = [[x[i], u[i], n[i], o[i]] for i in range(3)] + [[0, 0, 0, 1]]
    (OUT / "stand.json").write_text(json.dumps({"case": matrix}, indent=1))


if __name__ == "__main__":
    main()
