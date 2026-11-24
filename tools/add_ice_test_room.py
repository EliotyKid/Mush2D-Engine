"""D2 (Iki moves, task 7): the IceTest room in assets/levels/showcase.ldtk. Idempotent: a second run changes nothing.

Additions only, plus one approved exception (author, D2):
- defs: IntGrid value 9 "IceClimb" (#7fd4ff) on the Collision layer: collides like 1, its faces are climbable ice.
- level IceTest (1024 x 640 px), left of Showcase (worldX -1024), its floor top level with Showcase's (world y 432).
  Entered from Showcase through a doorway, off the main path (the player starts at Showcase's left and goes right).
  Layout (cells of 16 px, 1 u = 32 px = 2 cells; row 37 = floor top):
  - the Item "ability.ice.stakes" on the floor, between the doorway and the wall;
  - an ice wall of 8 u: the right face of a pillar (cols 30-33, rows 21-36; col 33 is value 9);
  - an ice ceiling of 6 u: a slab of 9 (cols 30-45, rows 19-20) whose underside runs 12 cells right of the wall,
    over a floorless pit (cols 36-43, rows 37-38);
  - a high platform (cols 44-55, top row 25, 6 u above the floor) with a Checkpoint, under the slab's end:
    hang from the ceiling, move right, drop onto it. Without the ability nothing reaches it (jump ~1.9 u).
- the exception: Showcase's left wall gets a doorway, cols 0-1 rows 23-26 (8 Collision cells set to 0, their
  Ground_d0 tiles removed), 2 u high for the Iki's 1.6 u collider. Nothing else in Showcase changes.

The script proves it: the normalized dump of everything that existed (minus the doorway cells and tiles, nextUid)
is the same before and after. Writes LDtk's own layout (tools/ldtk_json.py).

Run from the repo root: py tools/add_ice_test_room.py
"""
import copy
import json
import random
import sys
import uuid
from pathlib import Path

from gen_showcase import ground_tile
from ldtk_json import dumps

ROOT = Path(__file__).resolve().parent.parent
LDTK = ROOT / "assets" / "levels" / "showcase.ldtk"
GRID = 16
CW, CH = 64, 40
NAME = "IceTest"
DOOR_COLS, DOOR_ROWS = (0, 1), range(23, 27)  # Showcase cells of the doorway
ICE_COLOR = "#7fd4ff"


def grid():
    g = [[0] * CW for _ in range(CH)]

    def fill(x0, x1, y0, y1, v=1):
        for y in range(y0, y1 + 1):
            for x in range(x0, x1 + 1):
                g[y][x] = v
    fill(0, CW - 1, 0, 1)           # ceiling
    fill(0, 1, 2, 36)               # left wall
    fill(CW - 2, CW - 1, 2, 32)     # right wall above the doorway (rows 33-36 open, world y 368..432)
    fill(0, CW - 1, 37, CH - 1)     # floor (top row 37, world y 432)
    fill(36, 43, 37, 38, 0)         # floorless pit under the ice ceiling
    fill(30, 32, 21, 36)            # pillar...
    fill(33, 33, 21, 36, 9)         # ...its right face: ice wall, rows 21-36 = 8 u
    fill(30, 45, 19, 20, 9)         # ice slab: underside cols 34-45 = 6 u of ceiling
    fill(44, 55, 25, 26)            # high platform, top row 25 (6 u above the floor)
    return g


# (identifier, pivot px, fields)
ENTITIES = [
    ("Item", (872, 592), {"kind": "ability.ice.stakes", "persistence": "permanent"}),
    ("Checkpoint", (816, 400), {}),
    ("Light", (560, 448), {"color": "#8FD8FF", "intensity": 2.2, "radius": 7.0}),
    ("Light", (816, 336), {"color": "#8FD8FF", "intensity": 1.8, "radius": 6.0}),
    ("Light", (240, 480), {"color": "#8FB8FF", "intensity": 1.5, "radius": 8.0}),
]


def normalized(root):
    """Everything that existed before this script, as sorted JSON: no IceTest, no value 9, no nextUid, and the
    doorway cells/tiles of Showcase masked (the approved exception)."""
    r = copy.deepcopy(root)
    r.pop("nextUid")
    r["levels"] = [lv for lv in r["levels"] if lv["identifier"] != NAME]
    for d in r["defs"]["layers"]:
        if d["identifier"] == "Collision":
            d["intGridValues"] = [v for v in d["intGridValues"] if v["value"] != 9]
    show = next(lv for lv in r["levels"] if lv["identifier"] == "Showcase")
    for li in show["layerInstances"]:
        w = li["__cWid"]
        door = {y * w + x for y in DOOR_ROWS for x in DOOR_COLS}
        if li["__identifier"] == "Collision":
            for i in door:
                li["intGridCsv"][i] = "door"
        if li["__identifier"] == "Ground_d0":
            li["gridTiles"] = [t for t in li["gridTiles"] if t["d"][0] not in door]
    return json.dumps(r, sort_keys=True, ensure_ascii=False)


def main():
    text = LDTK.read_bytes().decode("utf-8")
    root = json.loads(text)
    if dumps(root, text) != text:
        sys.exit("showcase.ldtk is not in LDtk's layout (tools/ldtk_json.py); refusing to rewrite it")
    before = normalized(root)
    rng = random.Random(9)
    new_iid = lambda: str(uuid.UUID(int=rng.getrandbits(128), version=4))
    defs = root["defs"]
    levels = {lv["identifier"]: lv for lv in root["levels"]}
    show = levels["Showcase"]

    # 1. IntGrid value 9 on Collision.
    col_def = next(d for d in defs["layers"] if d["identifier"] == "Collision")
    if not any(v["value"] == 9 for v in col_def["intGridValues"]):
        v = copy.deepcopy(col_def["intGridValues"][0])
        v.update({"value": 9, "identifier": "IceClimb", "color": ICE_COLOR})
        col_def["intGridValues"].append(v)

    # 2. The doorway in Showcase's left wall (setting 0 and dropping tiles is idempotent).
    for li in show["layerInstances"]:
        w = li["__cWid"]
        door = {y * w + x for y in DOOR_ROWS for x in DOOR_COLS}
        if li["__identifier"] == "Collision":
            for i in door:
                li["intGridCsv"][i] = 0
        if li["__identifier"] == "Ground_d0":
            li["gridTiles"] = [t for t in li["gridTiles"] if t["d"][0] not in door]

    # 3. The room, its layers like Forest2's (same defs order, Terrain_d0 included), and its entities.
    if NAME not in levels:
        ent_defs = {e["identifier"]: e for e in defs["entities"]}
        src = levels["Forest2"]
        lv = copy.deepcopy({k: v for k, v in src.items() if k != "layerInstances"})
        root["nextUid"] += 1
        lv.update({"identifier": NAME, "iid": new_iid(), "uid": root["nextUid"] - 1,
                   "worldX": show["worldX"] - CW * GRID, "worldY": show["worldY"] + 27 * GRID - 37 * GRID,
                   "pxWid": CW * GRID, "pxHei": CH * GRID, "__neighbours": []})
        region = lv["fieldInstances"][0]
        assert region["__identifier"] == "region"
        region.update({"__value": "cave", "realEditorValues": [{"id": "V_String", "params": ["cave"]}]})
        g = grid()
        solid = [[1 if c in (1, 9) else 0 for c in row] for row in g]
        layers = []
        for s in src["layerInstances"]:
            li = copy.deepcopy(s)
            gs = li["__gridSize"]
            li.update({"__cWid": CW * GRID // gs, "__cHei": CH * GRID // gs, "iid": new_iid(), "levelId": lv["uid"],
                       "seed": rng.randrange(9999999), "intGridCsv": [], "autoLayerTiles": [], "gridTiles": [],
                       "entityInstances": []})
            layers.append(li)
        by_name = {li["__identifier"]: li for li in layers}
        by_name["Collision"]["intGridCsv"] = [c for row in g for c in row]
        by_name["Ground_d0"]["gridTiles"] = [
            {"px": [x * GRID, y * GRID], "src": [c * GRID, r * GRID], "f": 0, "t": r * 5 + c, "d": [y * CW + x], "a": 1}
            for y in range(CH) for x in range(CW) if solid[y][x] for c, r in [ground_tile(solid, x, y)]]
        instances = []
        for ident, px, vals in ENTITIES:
            d = ent_defs[ident]
            fis = []
            for fd in d["fieldDefs"]:
                v = vals.get(fd["identifier"], fd["defaultOverride"]["params"][0] if fd["defaultOverride"] else None)
                if fd["__type"] == "Color":
                    real = {"id": "V_Int", "params": [int(v[1:], 16)]}
                else:
                    real = {"id": "V_" + fd["__type"], "params": [v]}
                fis.append({"__identifier": fd["identifier"], "__type": fd["__type"], "__value": v, "__tile": None,
                            "defUid": fd["uid"], "realEditorValues": [real]})
            instances.append({"__identifier": ident, "__grid": [px[0] // GRID, px[1] // GRID],
                              "__pivot": [d["pivotX"], d["pivotY"]], "__tags": [], "__tile": None,
                              "__smartColor": d["color"], "__worldX": lv["worldX"] + px[0],
                              "__worldY": lv["worldY"] + px[1], "iid": new_iid(), "width": d["width"],
                              "height": d["height"], "defUid": d["uid"], "px": list(px), "fieldInstances": fis})
        by_name["Entities"]["entityInstances"] = instances
        lv["layerInstances"] = layers
        root["levels"].append(lv)

    assert normalized(root) == before, "an existing part of showcase.ldtk changed"
    out = dumps(root, text)
    if out == text:
        print("showcase.ldtk already has IceTest, value 9 and the doorway: nothing to do")
        return
    LDTK.write_bytes(out.encode("utf-8"))
    print(f"added IntGrid 9 IceClimb, the Showcase doorway and {NAME}; nextUid = {root['nextUid']}")


if __name__ == "__main__":
    main()
