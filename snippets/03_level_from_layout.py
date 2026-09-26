import unreal

MM_TO_UU = 0.1
OUTSIDE = "."

cfg = parse(LAYOUT)
COLS, ROWS, GRID = cfg["COLS"], cfg["ROWS"], cfg["MAP"]


if len(GRID) != len(ROWS):
    raise RuntimeError("MAP has {} rows, ROWS declares {}".format(len(GRID), len(ROWS)))
for i, line in enumerate(GRID):
    if len(line) != len(COLS):
        raise RuntimeError(
            "MAP row {} is {} chars, COLS declares {}".format(i, len(line), len(COLS)))


X = [0.0]
for w in COLS:
    X.append(X[-1] + w * MM_TO_UU)
Y = [0.0]
for h in ROWS:
    Y.append(Y[-1] + h * MM_TO_UU)


def wall(label, key, x0, y0, x1, y1):
    horizontal = abs(y1 - y0) < 0.01
    length = abs(x1 - x0) if horizontal else abs(y1 - y0)
    cx, cy = (x0 + x1) / 2.0, (y0 + y1) / 2.0

    door = doors.get(key)
    if door is None or length <= DOOR_W + 20.0:
        box(label, cx, cy, WALL_H / 2.0,
            *( (length, WALL_T) if horizontal else (WALL_T, length) ), WALL_H)
        return


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

        if horizontal:
            box(label + "_LOCKED", cx, cy, DOOR_H / 2.0, DOOR_W, WALL_T * 0.5, DOOR_H)
        else:
            box(label + "_LOCKED", cx, cy, DOOR_H / 2.0, WALL_T * 0.5, DOOR_W, DOOR_H)


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


nav = actor_editor.spawn_actor_from_class(
    unreal.NavMeshBoundsVolume,
    unreal.Vector(X[-1] / 2.0, Y[-1] / 2.0, WALL_H / 2.0),
    unreal.Rotator(0, 0, 0))


for a in actor_editor.get_all_level_actors():
    if isinstance(a, unreal.RecastNavMesh):
        a.set_editor_property("runtime_generation", unreal.RuntimeGenerationType.DYNAMIC)
