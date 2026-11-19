"""ALREADY RUN - DO NOT RUN AGAIN. The file layout has since changed to LDtk's own format, so this script
no longer matches assets/levels/showcase.ldtk and would corrupt it.

One-off (continuous world, task 5): appends 2 test rooms to assets/levels/showcase.ldtk.

- Forest2: right of Showcase, region forest. Cave1: up and to the right of Forest2, region cave; its floor
  continues Forest2's high right ledge, so the player walks from one into the other.
- Each room: ground and platforms (Collision IntGrid + grass.png tiles picked from it), 1 Slime with respawn,
  1 Slime without, 1 Item "permanent", 1 Item "respawn", a Lever wired to a Door (F_EntityRef "door"),
  a Checkpoint and a few Lights.
- Adds the entity defs (Slime, Item, Door, Lever, Checkpoint) and fields that are missing.
- Never touches the Showcase level (layers, entities); everything else is written back byte for byte.

Run once from the repo root: python tools/add_test_rooms.py
"""
import copy
import json
import random
import sys
import uuid
from pathlib import Path

from gen_showcase import field_def, ground_tile
from migrate_showcase_parallax import dump_like

ROOT = Path(__file__).resolve().parent.parent
LDTK = ROOT / "assets" / "levels" / "showcase.ldtk"
GRID = 16
CW, CH = 64, 30  # cells per test room (1024 x 480 px)


def fill(g, x0, x1, y0, y1):
    for y in range(y0, y1 + 1):
        for x in range(x0, x1 + 1):
            g[y][x] = 1


def forest2():
    g = [[0] * CW for _ in range(CH)]
    fill(g, 0, CW - 1, 27, CH - 1)  # floor (top row 27, as the Showcase floor)
    fill(g, 0, 7, 22, 26)           # continues the Showcase's high right ledge
    fill(g, 20, 27, 24, 25)         # platforms, 3 cells apart (jump apex ~3.8 cells)
    fill(g, 31, 38, 21, 22)
    fill(g, 42, 49, 18, 19)
    fill(g, 52, CW - 1, 15, 16)     # right ledge (top row 15): Cave1's floor continues it
    fill(g, CW - 2, CW - 1, 17, 26) # wall under the ledge (no room beyond)
    # (identifier, pivot px, fields); pivot (0.5, 1) except Light (0.5, 0.5)
    ents = [
        ("Checkpoint", (176, 432), {}),
        ("Lever", (240, 432), {}),
        ("Door", (304, 432), {}),
        ("Slime", (456, 432), {"respawn": True}),
        ("Slime", (648, 432), {"respawn": False}),
        ("Item", (560, 320), {"kind": "coin", "persistence": "permanent"}),
        ("Item", (736, 272), {"kind": "coin", "persistence": "respawn"}),
        ("Light", (400, 300), {}),
    ]
    return g, ents


def cave1():
    g = [[0] * CW for _ in range(CH)]
    fill(g, 0, CW - 1, 28, CH - 1)  # floor (top row 28 = Forest2's ledge top in world space)
    fill(g, 0, CW - 1, 0, 1)        # ceiling
    fill(g, 0, 1, 2, 12)            # left wall above the opening into Forest2
    fill(g, CW - 2, CW - 1, 2, 27)  # right wall
    fill(g, 20, 27, 25, 26)         # platforms
    fill(g, 31, 38, 22, 23)
    fill(g, 42, 49, 25, 26)
    ents = [
        ("Checkpoint", (96, 448), {}),
        ("Lever", (176, 448), {}),
        ("Door", (256, 448), {}),
        ("Slime", (488, 448), {"respawn": True}),
        ("Slime", (880, 448), {"respawn": False}),
        ("Item", (560, 336), {"kind": "coin", "persistence": "permanent"}),
        ("Item", (736, 384), {"kind": "coin", "persistence": "respawn"}),
        ("Light", (320, 256), {"color": "#8FB8FF", "intensity": 2.5, "radius": 7.0}),
        ("Light", (768, 256), {"color": "#8FB8FF", "intensity": 2.5, "radius": 7.0}),
    ]
    return g, ents


# identifier -> (colour, width, height, resizable, [(field, type, default)])
ENTITY_DEFS = {
    "Slime": ("#7CD35A", 32, 32, False, [("respawn", "Bool", True), ("respawnDistance", "Float", 12.0)]),
    "Item": ("#FFD84A", 16, 16, False, [("kind", "String", "coin"), ("persistence", "String", "permanent"),
                                        ("respawnDistance", "Float", 12.0)]),
    "Door": ("#8A5A3A", 32, 64, True, []),
    "Lever": ("#C0C0C0", 32, 32, False, [("door", "EntityRef", None)]),
    "Checkpoint": ("#5AA0FF", 32, 32, False, []),
}


def main():
    text = LDTK.read_bytes().decode("utf-8")
    root = json.loads(text)
    if dump_like(root, text) != text:
        sys.exit("showcase.ldtk does not round-trip byte for byte; refusing to rewrite it")
    if any(lv["identifier"] in ("Forest2", "Cave1") for lv in root["levels"]):
        sys.exit("Forest2/Cave1 already present: nothing to do")
    rng = random.Random(5)
    new_iid = lambda: str(uuid.UUID(int=rng.getrandbits(128), version=4))

    def uid():
        root["nextUid"] += 1
        return root["nextUid"] - 1

    defs = root["defs"]
    template = next(e for e in defs["entities"] if e["identifier"] == "Crystal")
    ent_defs = {e["identifier"]: e for e in defs["entities"]}
    for name, (color, w, h, resizable, fields) in ENTITY_DEFS.items():
        d = ent_defs.get(name)
        if d is None:
            d = copy.deepcopy(template)
            d.update({"identifier": name, "uid": uid(), "color": color, "width": w, "height": h,
                      "resizableX": resizable, "resizableY": resizable, "pivotX": 0.5, "pivotY": 1, "fieldDefs": []})
            defs["entities"].append(d)
            ent_defs[name] = d
        have = {f["identifier"] for f in d["fieldDefs"]}
        for fname, ftype, default in fields:
            if fname in have:
                continue
            if ftype == "EntityRef":  # -> the Door's iid
                f = field_def(uid(), fname, "String", "")
                f.update({"__type": "EntityRef", "type": "F_EntityRef", "canBeNull": True, "defaultOverride": None,
                          "editorDisplayMode": "RefLinkBetweenCenters", "allowedRefs": "OnlySpecificEntity",
                          "allowedRefsEntityUid": ent_defs["Door"]["uid"]})
            else:
                f = field_def(uid(), fname, ftype, default)
            d["fieldDefs"].append(f)

    region_def = next(f for f in defs["levelFields"] if f["identifier"] == "region")
    showcase = next(lv for lv in root["levels"] if lv["identifier"] == "Showcase")
    layer_defs = {d["uid"]: d for d in defs["layers"]}

    def add_level(name, world_px, region, g, ents):
        lv = copy.deepcopy({k: v for k, v in showcase.items() if k != "layerInstances"})
        lv.update({"identifier": name, "iid": new_iid(), "uid": uid(), "worldX": world_px[0], "worldY": world_px[1],
                   "pxWid": CW * GRID, "pxHei": CH * GRID, "__neighbours": [],
                   "fieldInstances": [{"__identifier": "region", "__type": "String", "__value": region, "__tile": None,
                                       "defUid": region_def["uid"],
                                       "realEditorValues": [{"id": "V_String", "params": [region]}]}]})
        layers = []
        for src in showcase["layerInstances"]:  # same layers, same order
            li = copy.deepcopy(src)
            li.update({"__cWid": CW, "__cHei": CH, "iid": new_iid(), "levelId": lv["uid"], "seed": rng.randrange(9999999),
                       "intGridCsv": [], "autoLayerTiles": [], "gridTiles": [], "entityInstances": []})
            layers.append(li)
        by_name = {li["__identifier"]: li for li in layers}
        by_name["Collision"]["intGridCsv"] = [c for row in g for c in row]
        by_name["Ground_d0"]["gridTiles"] = [
            {"px": [x * GRID, y * GRID], "src": [c * GRID, r * GRID], "f": 0, "t": r * 5 + c, "d": [y * CW + x], "a": 1}
            for y in range(CH) for x in range(CW) if g[y][x] for c, r in [ground_tile(g, x, y)]]
        assert layer_defs[by_name["Ground_d0"]["layerDefUid"]]["gridSize"] == GRID

        entities_layer = by_name["Entities"]
        instances = []
        for ident, px, vals in ents:
            d = ent_defs[ident]
            fis = []
            for fd in d["fieldDefs"]:
                v = vals.get(fd["identifier"], fd["defaultOverride"]["params"][0] if fd["defaultOverride"] else None)
                if fd["__type"] == "Color":
                    v = v if isinstance(v, str) else "#%06X" % v
                    real = {"id": "V_Int", "params": [int(v[1:], 16)]}
                elif fd["__type"] == "EntityRef":
                    real = None  # set below, once the Door exists
                else:
                    real = {"id": "V_" + fd["__type"], "params": [v]}
                fis.append({"__identifier": fd["identifier"], "__type": fd["__type"], "__value": v, "__tile": None,
                            "defUid": fd["uid"], "realEditorValues": [real] if real else []})
            instances.append({"__identifier": ident, "__grid": [px[0] // GRID, px[1] // GRID],
                              "__pivot": [d["pivotX"], d["pivotY"]], "__tags": [], "__tile": None,
                              "__smartColor": d["color"], "__worldX": world_px[0] + px[0], "__worldY": world_px[1] + px[1],
                              "iid": new_iid(), "width": d["width"], "height": d["height"], "defUid": d["uid"],
                              "px": list(px), "fieldInstances": fis})
        door = next(e for e in instances if e["__identifier"] == "Door")
        for e in instances:
            for fi in e["fieldInstances"]:
                if fi["__type"] == "EntityRef":
                    fi["__value"] = {"entityIid": door["iid"], "layerIid": entities_layer["iid"], "levelIid": lv["iid"],
                                     "worldIid": root["dummyWorldIid"]}
                    fi["realEditorValues"] = [{"id": "V_String", "params": [door["iid"]]}]
        entities_layer["entityInstances"] = instances
        lv["layerInstances"] = layers
        root["levels"].append(lv)
        return lv

    sx, sy = showcase["worldX"], showcase["worldY"]
    f2 = add_level("Forest2", (sx + showcase["pxWid"], sy), "forest", *forest2())
    # Cave1's floor top (row 28) meets Forest2's right ledge top (row 15) in world space.
    add_level("Cave1", (f2["worldX"] + f2["pxWid"], f2["worldY"] + 15 * GRID - 28 * GRID), "cave", *cave1())

    LDTK.write_bytes(dump_like(root, text).encode("utf-8"))
    print(f"added Forest2 and Cave1 to {LDTK.relative_to(ROOT)}; nextUid = {root['nextUid']}")


if __name__ == "__main__":
    main()
