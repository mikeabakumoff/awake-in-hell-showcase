"""Awake in Hell — level generation from a text layout (excerpt)

The playable level is not authored in the editor. This commandlet reads a plain
text file and builds the geometry, the doorways, the patrol network and the
hiding places from it.

The layout declares structural bay widths in millimetres of the original
drawing, so nothing is eyeballed:

    COLS 2400 3300 3300 3300 2400 1800 2400 ...
    ROWS 3600 5050 2100 2400 2400 2100 5050 5050

    MAP
    58884.www.zzz.ggg.48885
    99999999999999999999999
    .....0iiimmlleekk0.....

One character is one structural bay, not one metre — which is what keeps the
geometry true to a drawing whose module is 2400/3300/1500, none of which land on
a round metre. Millimetres convert to engine units at a fixed ratio.

Run headless:
    UnrealEditor-Cmd.exe <project> -run=pythonscript -script="Tools/build_from_layout.py"

Idempotent: every actor it spawns is labelled and wiped on the next run, so
reshaping a wing is a diff rather than an afternoon of dragging boxes.
"""
import unreal

MM_TO_UU = 0.1        # 1 mm of the drawing = 0.1 Unreal unit
OUTSIDE = "."

cfg = parse(LAYOUT)
COLS, ROWS, GRID = cfg["COLS"], cfg["ROWS"], cfg["MAP"]

# Fail loudly on a malformed layout rather than building half a building.
if len(GRID) != len(ROWS):
    raise RuntimeError("MAP has {} rows, ROWS declares {}".format(len(GRID), len(ROWS)))
for i, line in enumerate(GRID):
    if len(line) != len(COLS):
        raise RuntimeError(
            "MAP row {} is {} chars, COLS declares {}".format(i, len(line), len(COLS)))

# Cumulative axis positions, in engine units.
X = [0.0]
for w in COLS:
    X.append(X[-1] + w * MM_TO_UU)
Y = [0.0]
for h in ROWS:
    Y.append(Y[-1] + h * MM_TO_UU)


def wall(label, key, x0, y0, x1, y1):
    """A wall between two bays, with a doorway cut into it if one is declared."""
    horizontal = abs(y1 - y0) < 0.01
    length = abs(x1 - x0) if horizontal else abs(y1 - y0)
    cx, cy = (x0 + x1) / 2.0, (y0 + y1) / 2.0

    door = doors.get(key)
    if door is None or length <= DOOR_W + 20.0:
        box(label, cx, cy, WALL_H / 2.0,
            *( (length, WALL_T) if horizontal else (WALL_T, length) ), WALL_H)
        return

    # Two jambs plus a lintel over the opening.
    side_len = (length - DOOR_W) / 2.0
    off = (DOOR_W + side_len) / 2.0
    for sign, tag in ((-1, "a"), (1, "b")):
        if horizontal:
            box(label + tag, cx + sign * off, cy, WALL_H / 2.0, side_len, WALL_T, WALL_H)
        else:
            box(label + tag, cx, cy + sign * off, WALL_H / 2.0, WALL_T, side_len, WALL_H)

    lintel = WALL_H - DOOR_H
    if lintel > 1.0:
        if horizontal:
            box(label + "L", cx, cy, DOOR_H + lintel / 2.0, DOOR_W, WALL_T, lintel)
        else:
            box(label + "L", cx, cy, DOOR_H + lintel / 2.0, WALL_T, DOOR_W, lintel)

    if door["locked"]:
        # The door exists, it just does not open — the story gate is data too.
        if horizontal:
            box(label + "_LOCKED", cx, cy, DOOR_H / 2.0, DOOR_W, WALL_T * 0.5, DOOR_H)
        else:
            box(label + "_LOCKED", cx, cy, DOOR_H / 2.0, WALL_T * 0.5, DOOR_W, DOOR_H)


# A wall wherever two adjacent bays differ, plus the outer boundary. No wall is
# ever placed by hand; the map decides.
for r in range(len(ROWS)):
    for c in range(len(COLS) + 1):
        left, right = cell(r, c - 1), cell(r, c)
        if left == right or (left == OUTSIDE and right == OUTSIDE):
            continue
        wall("WallV_{}_{}".format(r, c), ("V", r, c), X[c], Y[r], X[c], Y[r + 1])

for r in range(len(ROWS) + 1):
    for c in range(len(COLS)):
        top, bottom = cell(r - 1, c), cell(r, c)
        if top == bottom or (top == OUTSIDE and bottom == OUTSIDE):
            continue
        wall("WallH_{}_{}".format(r, c), ("H", r, c), X[c], Y[r], X[c + 1], Y[r])

# Navigation bounds come from the layout too. Spawning them here is what stops a
# level rebuild from silently dropping navigation — a failure mode that costs an
# evening to diagnose, because the AI looks broken rather than lost.
nav = actor_editor.spawn_actor_from_class(
    unreal.NavMeshBoundsVolume,
    unreal.Vector(X[-1] / 2.0, Y[-1] / 2.0, WALL_H / 2.0),
    unreal.Rotator(0, 0, 0))

# The level is generated headlessly, so nothing bakes a static navmesh. Do NOT
# delete this actor hoping the navigation system recreates it from project
# defaults — it does not, and navigation disappears entirely.
for a in actor_editor.get_all_level_actors():
    if isinstance(a, unreal.RecastNavMesh):
        a.set_editor_property("runtime_generation", unreal.RuntimeGenerationType.DYNAMIC)
