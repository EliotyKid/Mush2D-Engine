"""One-off (terrain and slopes, task 5): terrain and grid-slope test content in assets/levels/showcase.ldtk.

- defs: entity layer "Terrain_d0" (4 px grid, right after Collision; the _d0 suffix keeps the terrain at the
  gameplay depth), entity "Terrain" (points, closed, thickness, oneWay, style), enum TerrainStyle (forest, cave),
  IntGrid values 2..8 on Collision, tileset GrassSlopes (assets/kaa/tilesets/grass_slopes.png, from
  tools/gen_terrain_textures.py).
- Forest2: the two low step platforms go; an organic Terrain ground with 2 hills and a ~30 deg descent, and a
  one-way branch over the descent. The slimes move to the floor past the descent.
- Cave1: the old platforms go; a mesa with 45 deg ramps (3/4), a hill with 22.5 deg ramps (5+7 / 8+6) and a
  one-way platform (2) with a slime on it; the other slime moves past the hill. Its Ground_d0 uses the GrassSlopes tileset (grass.png + slope tiles), slope tiles placed
  directly as gridTiles (auto-rules are left to the author, see README).
- Never touches the Showcase level; LDtk adds its empty Terrain_d0 instance itself on the next save.
- Writes LDtk's own layout back (tools/ldtk_json.py: an unchanged load/dump is byte-identical). Refuses to run twice (Forest2 already has the Terrain layer).

Run once from the repo root: python tools/add_terrain_content.py
"""
import copy
import json
import random
import sys
import uuid
from pathlib import Path

from gen_showcase import field_def, ground_tile
from ldtk_json import dumps

ROOT = Path(__file__).resolve().parent.parent
LDTK = ROOT / "assets" / "levels" / "showcase.ldtk"
GRID, TGRID = 16, 4
LAYER = "Terrain_d0"
SLOPE_ROW = 5  # grass_slopes.png: row 5, col value-3

# Forest2 terrain, level px (multiples of 4). Floor top is y 432, room bottom 480.
FOREST_GROUND = [(336, 432), (368, 420), (400, 404), (424, 394), (448, 390), (472, 394), (496, 404), (520, 412),
                 (540, 408), (560, 392), (576, 380), (592, 372), (604, 372), (700, 428), (712, 432)]
FOREST_BRANCH = [(608, 340), (628, 336), (648, 340)]


def forest2_grid(g, w):
    for y in (24, 25):
        g[y * w + 20:y * w + 28] = [0] * 8  # platform cols 20..27 (under hill 1)
    for y in (21, 22):
        g[y * w + 31:y * w + 39] = [0] * 8  # platform cols 31..38 (in hill 2)


def cave1_grid(g, w):
    def put(y, x0, x1, v):
        g[y * w + x0:y * w + x1 + 1] = [v] * (x1 - x0 + 1)
    for y0, x0 in ((25, 20), (22, 31), (25, 42)):  # old 8x2 platforms
        put(y0, x0, x0 + 7, 0)
        put(y0 + 1, x0, x0 + 7, 0)
    # mesa, 45 deg both sides, top row 25 (y 400) on cols 31..38
    put(27, 28, 28, 3); put(27, 29, 40, 1); put(27, 41, 41, 4)
    put(26, 29, 29, 3); put(26, 30, 39, 1); put(26, 40, 40, 4)
    put(25, 30, 30, 3); put(25, 31, 38, 1); put(25, 39, 39, 4)
    # hill, 22.5 deg both sides (low 5 then high 7 up, high 8 then low 6 down), top row 26 (y 416) on cols 48..53
    put(27, 44, 44, 5); put(27, 45, 45, 7); put(27, 46, 55, 1); put(27, 56, 56, 8); put(27, 57, 57, 6)
    put(26, 46, 46, 5); put(26, 47, 47, 7); put(26, 48, 53, 1); put(26, 54, 54, 8); put(26, 55, 55, 6)
    # one-way platform (2), top y 352: reached from the mesa top, walked under from the floor
    put(22, 21, 27, 2)


def tiles_for(g, w, h, slopes):
    """Ground_d0 gridTiles: grass 9-slice for 1 (slopes count as solid), top row for 2, slope tiles for 3..8."""
    solid = [[1 if g[y * w + x] in (1, 3, 4, 5, 6, 7, 8) else 0 for x in range(w)] for y in range(h)]
    oneway = [[1 if g[y * w + x] == 2 else 0 for x in range(w)] for y in range(h)]
    cols = 6 if slopes else 5
    out = []
    for y in range(h):
        for x in range(w):
            v = g[y * w + x]
            if v == 0:
                continue
            if v >= 3:
                c, r = v - 3, SLOPE_ROW
            else:
                c, r = ground_tile(solid if v == 1 else oneway, x, y)
                if v == 2:
                    r = 0  # a thin top: never the closed bottom row of the 9-slice
            out.append({"px": [x * GRID, y * GRID], "src": [c * GRID, r * GRID], "f": 0, "t": r * cols + c,
                        "d": [y * w + x], "a": 1})
    return out


def main():
    text = LDTK.read_bytes().decode("utf-8")
    root = json.loads(text)
    if dumps(root, text) != text:
        sys.exit("showcase.ldtk is not in LDtk's layout (tools/ldtk_json.py); refusing to rewrite it")
    levels = {lv["identifier"]: lv for lv in root["levels"]}
    if "Forest2" not in levels or "Cave1" not in levels:
        sys.exit("Forest2/Cave1 missing: run tools/add_test_rooms.py first")
    if any(li["__identifier"] == LAYER for li in levels["Forest2"]["layerInstances"]):
        sys.exit(f"Forest2 already has the {LAYER} layer: nothing to do")
    if not (ROOT / "assets" / "kaa" / "tilesets" / "grass_slopes.png").exists():
        sys.exit("grass_slopes.png missing: run tools/gen_terrain_textures.py first")
    showcase_before = json.dumps(levels["Showcase"], sort_keys=True)
    rng = random.Random(7)
    new_iid = lambda: str(uuid.UUID(int=rng.getrandbits(128), version=4))

    def uid():
        root["nextUid"] += 1
        return root["nextUid"] - 1

    defs = root["defs"]
    layer_defs = {d["identifier"]: d for d in defs["layers"]}

    # IntGrid values 2..8 on Collision (value 1 = solid already there).
    col_def = layer_defs["Collision"]
    have = {v["value"] for v in col_def["intGridValues"]}
    template_v = col_def["intGridValues"][0]
    for value, name, color in ((2, "oneway", "#4FA3FF"), (3, "slope45_r", "#E0A030"), (4, "slope45_l", "#E07A30"),
                               (5, "slope22_low_r", "#B0D040"), (6, "slope22_low_l", "#80C040"),
                               (7, "slope22_high_r", "#C0E080"), (8, "slope22_high_l", "#90D080")):
        if value not in have:
            v = copy.deepcopy(template_v)
            v.update({"value": value, "identifier": name, "color": color})
            col_def["intGridValues"].append(v)

    # Tileset GrassSlopes: grass.png's 5x5 tiles at the same src + slope row.
    grass_ts = next(t for t in defs["tilesets"] if t["relPath"].endswith("grass.png"))
    slopes_ts = copy.deepcopy(grass_ts)
    slopes_ts.update({"identifier": "GrassSlopes", "uid": uid(), "relPath": "../kaa/tilesets/grass_slopes.png",
                      "pxWid": 96, "pxHei": 96, "__cWid": 6, "__cHei": 6, "savedSelections": []})
    slopes_ts.pop("cachedPixelData", None)  # LDtk recomputes it
    defs["tilesets"].append(slopes_ts)

    # Enum TerrainStyle.
    style_enum = {"identifier": "TerrainStyle", "uid": uid(),
                  "values": [{"id": "forest", "tileRect": None, "color": 0x4C9A3B},
                             {"id": "cave", "tileRect": None, "color": 0x4A5E8A}],
                  "iconTilesetUid": None, "externalRelPath": None, "externalFileChecksum": None, "tags": []}
    defs["enums"].append(style_enum)

    # Entity Terrain: a 4x4 marker centred on the first point's cell (pivot 0.5, 0.5), like the F_Point dots.
    template = next(e for e in defs["entities"] if e["identifier"] == "Crystal")
    terrain = copy.deepcopy(template)
    terrain.update({"identifier": "Terrain", "uid": uid(), "color": "#C08040", "width": TGRID, "height": TGRID,
                    "pivotX": 0.5, "pivotY": 0.5, "fieldDefs": [], "doc": "Free terrain line: points (4 px cells)"})
    points = field_def(uid(), "points", "String", "")
    points.update({"__type": "Array<Point>", "type": "F_Point", "isArray": True, "canBeNull": False,
                   "editorDisplayMode": "PointPath", "defaultOverride": None})
    style = field_def(uid(), "style", "String", "forest")
    style.update({"__type": "LocalEnum.TerrainStyle", "type": f"F_Enum({style_enum['uid']})"})
    terrain["fieldDefs"] = [points, field_def(uid(), "closed", "Bool", False),
                            field_def(uid(), "thickness", "Float", 3.0), field_def(uid(), "oneWay", "Bool", False),
                            style]
    defs["entities"].append(terrain)

    # Layer Terrain_d0, right after Collision.
    terrain_layer = copy.deepcopy(layer_defs["Entities"])
    terrain_layer.update({"identifier": LAYER, "uid": uid(), "gridSize": TGRID})
    defs["layers"].insert(defs["layers"].index(col_def) + 1, terrain_layer)
    order = [d["uid"] for d in defs["layers"]]
    def_names = {d["uid"]: d["identifier"] for d in defs["layers"]}

    def terrain_entity(level, pts, closed=False, thickness=3.0, one_way=False, style_id="forest"):
        (x0, y0), rest = pts[0], pts[1:]
        cx, cy = x0 // TGRID, y0 // TGRID
        values = {"points": [{"cx": x // TGRID, "cy": y // TGRID} for x, y in rest], "closed": closed,
                  "thickness": thickness, "oneWay": one_way, "style": style_id}
        fis = []
        for fd in terrain["fieldDefs"]:
            v = values[fd["identifier"]]
            if fd["identifier"] == "points":
                real = [{"id": "V_String", "params": [f"{p['cx']},{p['cy']}"]} for p in v]
            elif fd["identifier"] == "style":
                real = [{"id": "V_String", "params": [v]}]
            else:
                real = [{"id": "V_" + fd["__type"], "params": [v]}]
            fis.append({"__identifier": fd["identifier"], "__type": fd["__type"], "__value": v, "__tile": None,
                        "defUid": fd["uid"], "realEditorValues": real})
        px = [cx * TGRID + TGRID // 2, cy * TGRID + TGRID // 2]
        return {"__identifier": "Terrain", "__grid": [cx, cy], "__pivot": [0.5, 0.5], "__tags": [], "__tile": None,
                "__smartColor": terrain["color"], "__worldX": level["worldX"] + px[0],
                "__worldY": level["worldY"] + px[1], "iid": new_iid(), "width": TGRID, "height": TGRID,
                "defUid": terrain["uid"], "px": px, "fieldInstances": fis}

    def add_layer(level, entities):
        layers = level["layerInstances"]
        for li in layers:  # derived names follow the defs (LDtk sanitised Foreground_d-2 -> Foreground_d_2)
            li["__identifier"] = def_names[li["layerDefUid"]]
        src = next(li for li in layers if li["layerDefUid"] == col_def["uid"])
        li = copy.deepcopy(src)
        li.update({"__identifier": LAYER, "__type": "Entities", "__cWid": level["pxWid"] // TGRID,
                   "__cHei": level["pxHei"] // TGRID, "__gridSize": TGRID, "iid": new_iid(),
                   "layerDefUid": terrain_layer["uid"], "seed": rng.randrange(9999999), "intGridCsv": [],
                   "autoLayerTiles": [], "gridTiles": [], "entityInstances": entities})
        layers.append(li)
        layers.sort(key=lambda l: order.index(l["layerDefUid"]))

    def layer(level, name):
        return next(li for li in level["layerInstances"] if li["__identifier"] == name)

    def move_entity(level, ident, old_px, new_px):
        e = next(e for e in layer(level, "Entities")["entityInstances"]
                 if e["__identifier"] == ident and e["px"] == list(old_px))
        e["px"] = list(new_px)
        e["__grid"] = [new_px[0] // GRID, new_px[1] // GRID]
        e["__worldX"], e["__worldY"] = level["worldX"] + new_px[0], level["worldY"] + new_px[1]

    # Forest2
    f2 = levels["Forest2"]
    w, h = f2["pxWid"] // GRID, f2["pxHei"] // GRID
    col = layer(f2, "Collision")
    forest2_grid(col["intGridCsv"], w)
    layer(f2, "Ground_d0")["gridTiles"] = tiles_for(col["intGridCsv"], w, h, slopes=False)
    move_entity(f2, "Slime", (456, 432), (880, 432))  # off the hills: they are left for the player
    move_entity(f2, "Slime", (648, 432), (776, 432))  # past the descent, on the floor
    add_layer(f2, [terrain_entity(f2, FOREST_GROUND, thickness=1.5),
                   terrain_entity(f2, FOREST_BRANCH, thickness=0.25, one_way=True)])

    # Cave1
    c1 = levels["Cave1"]
    w, h = c1["pxWid"] // GRID, c1["pxHei"] // GRID
    col = layer(c1, "Collision")
    cave1_grid(col["intGridCsv"], w)
    ground = layer(c1, "Ground_d0")
    ground.update({"overrideTilesetUid": slopes_ts["uid"], "__tilesetDefUid": slopes_ts["uid"],
                   "__tilesetRelPath": slopes_ts["relPath"],
                   "gridTiles": tiles_for(col["intGridCsv"], w, h, slopes=True)})
    move_entity(c1, "Slime", (488, 448), (392, 352))  # on the one-way platform
    move_entity(c1, "Slime", (880, 448), (960, 448))  # floor past the 22.5 deg hill
    add_layer(c1, [])

    assert json.dumps(levels["Showcase"], sort_keys=True) == showcase_before, "Showcase changed"
    LDTK.write_bytes(dumps(root, text).encode("utf-8"))
    print(f"added {LAYER}, Terrain, TerrainStyle, IntGrid 2..8, GrassSlopes; Forest2 + Cave1 content; "
          f"nextUid = {root['nextUid']}")


if __name__ == "__main__":
    main()
