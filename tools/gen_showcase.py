"""Generate what the kaa art lacks for the showcase room (deterministic, random.seed(1)).

Usage: python tools/gen_showcase.py

Outputs:
  assets/props/crystals.png, crystals_e.png, crystals_n.png, crystals.json
      4 crystals (32x32 cells) + a soft "halo 0" cell for the additive glow.
  assets/kaa/tilesets/grass_m.png
      mask for grass.png: R = rim on top edges, G = AO (darker towards inner corners).
  assets/levels/showcase.ldtk
      LDtk 1.5 project, one 96x30 level (16 px grid): Collision IntGrid, Ground tiles
      auto-picked from the IntGrid, kaa parallax layers, props, critters, lights, crystals.
Never touches the kaa images or their _n maps (tools/import_kaa.py owns those).
"""
import json
import math
import random
import uuid
from pathlib import Path

from PIL import Image, ImageDraw

from import_kaa import make_normal_map  # same normal-map recipe as the kaa import

ROOT = Path(__file__).resolve().parent.parent
ASSETS = ROOT / "assets"
U = 32.0  # art px per world unit (LevelLoader)


# ---------------------------------------------------------------- crystals
CRYSTALS = [  # (albedo base, emissive) per crystal
    ((120, 200, 215), (0, 255, 255)),
    ((205, 130, 210), (255, 0, 255)),
    ((110, 185, 225), (0, 230, 255)),
    ((215, 120, 190), (255, 40, 220)),
]


def crystal_shards(rng, cx):
    """List of shard polygons (bottom at y=31) for one 32x32 cell."""
    shards = []
    for k, (dx, h, w, lean) in enumerate([(0, 26, 9, 0), (-7, 16, 7, -3), (7, 19, 7, 3), (-11, 9, 5, -2), (11, 11, 5, 2)]):
        h += rng.randint(-2, 2)
        x = cx + dx
        shards.append([(x - w / 2, 31), (x - w / 2 + 1, 31 - h * 0.7), (x + lean, 31 - h), (x + w / 2 - 1, 31 - h * 0.7), (x + w / 2, 31)])
    return shards


def gen_crystals():
    rng = random.Random(1)
    W, H = 160, 32
    alb = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    emi = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    da, de = ImageDraw.Draw(alb), ImageDraw.Draw(emi)
    frames = {}
    for i, (base, glow) in enumerate(CRYSTALS):
        cx = i * 32 + 16
        dark = tuple(int(c * 0.55) for c in base)
        light = tuple(min(255, int(c * 1.25 + 30)) for c in base)
        for poly in crystal_shards(rng, cx):
            da.polygon(poly, fill=base + (255,))
            # 3-4 tones: dark right facet, light left facet, highlight edge
            tip = poly[2]
            da.polygon([poly[0], poly[1], tip, (tip[0], 31)], fill=light + (255,))
            da.polygon([(tip[0], 31), tip, poly[3], poly[4]], fill=dark + (255,))
            da.line([poly[1], tip], fill=(255, 255, 255, 255))
            # emissive: same shapes, strong colour, a bit dimmer on the dark facet
            de.polygon(poly, fill=glow + (255,))
            de.polygon([(tip[0], 31), tip, poly[3], poly[4]], fill=tuple(int(c * 0.6) for c in glow) + (255,))
        frames[f"crystal {i}"] = (i * 32, 0)
    # halo: white, alpha = smooth radial falloff (additive sprite, LINEAR sampling)
    hp = alb.load()
    for y in range(32):
        for x in range(32):
            r = math.hypot(x + 0.5 - 16, y + 0.5 - 16) / 16
            a = max(0.0, 1 - r) ** 2
            hp[128 + x, y] = (255, 255, 255, int(round(a * 255)))
    frames["halo 0"] = (128, 0)
    # emissive alpha copied from albedo so the map is a clean RGBA sibling
    ep = emi.load()
    ap = alb.load()
    for y in range(H):
        for x in range(W):
            r, g, b, _ = ep[x, y]
            ep[x, y] = (r, g, b, ap[x, y][3] if x < 128 else 0)
    out = ASSETS / "props"
    out.mkdir(parents=True, exist_ok=True)
    alb.save(out / "crystals.png")
    emi.save(out / "crystals_e.png")
    make_normal_map(alb).save(out / "crystals_n.png")
    frames_json = {
        name: {"frame": {"x": x, "y": y, "w": 32, "h": 32}, "rotated": False, "trimmed": False,
               "spriteSourceSize": {"x": 0, "y": 0, "w": 32, "h": 32}, "sourceSize": {"w": 32, "h": 32},
               "duration": 100, "pivot": {"x": 16, "y": 32 if name.startswith("crystal") else 16}}
        for name, (x, y) in frames.items()
    }
    meta = {"app": "tools/gen_showcase.py", "image": "crystals.png", "format": "RGBA8888",
            "size": {"w": W, "h": H}, "scale": "1", "frameTags": []}
    (out / "crystals.json").write_text(json.dumps({"frames": frames_json, "meta": meta}, indent=1), encoding="utf-8")


# ---------------------------------------------------------------- grass mask
# grass.png 16 px grid: 9-slice at cols 1..3 / rows 0..2, inner corners at cols 1..2 / rows 3..4.
INNER = {(1, 3): (15, 15), (2, 3): (0, 15), (1, 4): (15, 0), (2, 4): (0, 0)}  # tile -> concave corner px


def gen_grass_mask():
    src = Image.open(ASSETS / "kaa" / "tilesets" / "grass.png").convert("RGBA")
    w, h = src.size
    sp = src.load()
    m = Image.new("RGBA", (w, h), (0, 255, 0, 255))
    mp = m.load()
    for y in range(h):
        for x in range(w):
            a = sp[x, y][3]
            tx, ty = x // 16, y // 16
            lx, ly = x % 16, y % 16
            r, g = 0, 255
            if ty == 0 and 1 <= tx <= 3 and a:
                # rim: first 3 opaque px from the top of each column of a top-edge tile
                first = next((yy for yy in range(16) if sp[x, yy][3]), 16)
                if ly - first < 3:
                    r = 255 - (ly - first) * 60
            if (tx, ty) in INNER:
                # inner corner tiles: AO collects in the crease of the concave corner
                cx, cy = INNER[(tx, ty)]
                d = math.hypot(lx - cx, ly - cy) / 22.6
                g = int(255 - 120 * (1.0 - min(1.0, d)))
            mp[x, y] = (r, g, 0, a)
    m.save(ASSETS / "kaa" / "tilesets" / "grass_m.png")


# ---------------------------------------------------------------- level
GRID = 16
CW, CH = 96, 30
PXW, PXH = CW * GRID, CH * GRID
WW, WH = PXW / U, PXH / U  # world size 48 x 15
CENTRE = (WW / 2, WH / 2)
HALF_VIEW = (1280 / 3 / U / 2, 720 / 3 / U / 2)  # 1280x720 at pixel scale 3
CAM_LOW_Y = HALF_VIEW[1]
FLOOR_ROW = 27


def build_collision():
    g = [[0] * CW for _ in range(CH)]

    def fill(x0, x1, y0, y1, v=1):
        for y in range(y0, y1 + 1):
            for x in range(x0, x1 + 1):
                g[y][x] = v

    fill(0, 1, 0, CH - 1)            # left wall
    fill(CW - 2, CW - 1, 0, CH - 1)  # right wall
    fill(2, CW - 3, FLOOR_ROW, CH - 1)  # jungle floor
    fill(24, 37, 24, 26)             # mound (3 tiles up)
    fill(40, 46, 21, 22)             # floating platform B (3 above the mound)
    fill(50, 55, 18, 19)             # floating platform C (3 above B)
    fill(62, 69, FLOOR_ROW, 28, 0)   # hollow (2 down)
    fill(76, 79, 25, 26)             # step
    fill(80, CW - 3, 22, 26)         # high ledge
    return g


def ground_tile(g, x, y):
    """grass.png tile (col, row) for solid cell (x, y). Out of bounds counts as solid."""
    s = lambda xx, yy: not (0 <= xx < CW and 0 <= yy < CH) or g[yy][xx] == 1
    n, so, w, e = s(x, y - 1), s(x, y + 1), s(x - 1, y), s(x + 1, y)
    if not n:
        return (1, 0) if not w else (3, 0) if not e else (2, 0)
    if not so:
        return (1, 2) if not w else (3, 2) if not e else (2, 2)
    if not w:
        return (1, 1)
    if not e:
        return (3, 1)
    if not s(x - 1, y - 1):
        return (2, 4)
    if not s(x + 1, y - 1):
        return (1, 4)
    if not s(x - 1, y + 1):
        return (2, 3)
    if not s(x + 1, y + 1):
        return (1, 3)
    return (2, 1)


def world_to_px(x, y):
    return [int(round(x * U)), int(round(PXH - y * U))]


def displayed_to_authored(x, y, fx, fy, cam):
    """Where to author an element of a layer (LDtk factors fx, fy) so it shows at (x, y) for camera `cam`.
    LDtk / engine (with the anchor): shown = authored + f * (cam - centre)."""
    return x - fx * (cam[0] - CENTRE[0]), y - fy * (cam[1] - CENTRE[1])


def cam_for(x):
    return (min(max(x, HALF_VIEW[0]), WW - HALF_VIEW[0]), CAM_LOW_Y)


class Uid:
    def __init__(self):
        self.n = 100

    def __call__(self):
        self.n += 1
        return self.n


def iid(rng):
    return str(uuid.UUID(int=rng.getrandbits(128), version=4))


FIELD_TYPES = {"Float": "F_Float", "String": "F_String", "Bool": "F_Bool", "Int": "F_Int", "Color": "F_Color"}


def field_def(uid, name, typ, default):
    return {
        "identifier": name, "doc": None, "__type": typ, "uid": uid, "type": FIELD_TYPES[typ], "isArray": False,
        "canBeNull": False, "arrayMinLength": None, "arrayMaxLength": None, "editorDisplayMode": "ValueOnly",
        "editorDisplayScale": 1, "editorDisplayPos": "Above", "editorLinkStyle": "StraightArrow",
        "editorDisplayColor": None, "editorAlwaysShow": False, "editorShowInWorld": True, "editorCutLongValues": True,
        "editorTextSuffix": None, "editorTextPrefix": None, "useForSmartColor": typ == "Color", "exportToToc": False,
        "searchable": False, "min": None, "max": None, "regex": None, "acceptFileTypes": None,
        "defaultOverride": {"id": "V_" + ("Int" if typ == "Color" else typ), "params": [default]},
        "textLanguageMode": None, "symmetricalRef": False, "autoChainRef": True, "allowOutOfLevelRef": True,
        "allowedRefs": "OnlySame", "allowedRefsEntityUid": None, "allowedRefTags": [], "tilesetUid": None,
    }


def color_int(hex_):
    return int(hex_[1:], 16)


def main():
    rng = random.Random(1)
    gen_crystals()
    gen_grass_mask()

    uid = Uid()
    g = build_collision()

    # ---- tilesets
    def tileset(ident, rel, w, h, grid):
        return {"__cWid": w // grid, "__cHei": h // grid, "identifier": ident, "uid": uid(), "relPath": rel,
                "embedAtlas": None, "pxWid": w, "pxHei": h, "tileGridSize": grid, "spacing": 0, "padding": 0,
                "tags": [], "tagsSourceEnumUid": None, "enumTags": [], "customData": [], "savedSelections": [],
                "cachedPixelData": None}

    ts_grass = tileset("Grass", "../kaa/tilesets/grass.png", 80, 80, 16)
    # (layer, image, LDtk parallax X, Y, displayed bottom (world y) at the starting camera; floor top = 1.5)
    PARALLAX = [
        ("Reeds_d2", "bg5", 0.15, 0.08, 0.4),
        ("Shore_d4", "bg4", 0.30, 0.15, 0.7),
        ("Lake_d7", "bg3", 0.45, 0.25, 0.6),
        ("Trees_d11", "bg2", 0.60, 0.35, 0.9),
        ("Forest_d18", "bg1", 0.78, 0.45, 1.2),
    ]
    ts_bg = {img: tileset(img.capitalize(), f"../kaa/parallax/{img}.png", 576, 324, 36) for _, img, *_ in PARALLAX}

    # ---- entity defs
    ent_defs = {}

    def entity_def(name, color, w, h, pivot, fields):
        d = {"identifier": name, "uid": uid(), "tags": [], "exportToToc": False, "allowOutOfBounds": True, "doc": None,
             "width": w, "height": h, "resizableX": False, "resizableY": False, "minWidth": None, "maxWidth": None,
             "minHeight": None, "maxHeight": None, "keepAspectRatio": False, "tileOpacity": 1, "fillOpacity": 0.08,
             "lineOpacity": 1, "hollow": False, "color": color, "renderMode": "Rectangle", "showName": True,
             "tilesetId": None, "tileRenderMode": "FitInside", "tileRect": None, "uiTileRect": None,
             "nineSliceBorders": [], "maxCount": 1 if name == "PlayerStart" else 0, "limitScope": "PerLevel",
             "limitBehavior": "MoveLastOne", "pivotX": pivot[0], "pivotY": pivot[1],
             "fieldDefs": [field_def(uid(), fn, ft, fd) for fn, ft, fd in fields]}
        ent_defs[name] = d

    entity_def("PlayerStart", "#FFFFFF", 20, 52, (0.5, 1), [])
    entity_def("Light", "#FFC37A", 16, 16, (0.5, 0.5), [
        ("color", "Color", color_int("#FFC37A")), ("intensity", "Float", 2.0), ("radius", "Float", 6.0),
        ("height", "Float", 1.5), ("depth", "Float", 0.0), ("flicker", "Float", 0.0)])
    entity_def("Crystal", "#00FFFF", 16, 16, (0.5, 1), [("variant", "Int", 0)])
    entity_def("Prop", "#6BBF59", 16, 16, (0.5, 1), [("frame", "String", "bush 0"), ("depth", "Float", 0.0), ("flipX", "Bool", False)])
    entity_def("Critter", "#E0A060", 16, 16, (0.5, 1), [("anim", "String", "fox_idle"), ("flipX", "Bool", False)])

    def ent(name, x, y, **vals):
        """Entity whose pivot sits at world (x, y)."""
        d = ent_defs[name]
        px = world_to_px(x, y)
        fis = []
        for fd in d["fieldDefs"]:
            v = vals.get(fd["identifier"], fd["defaultOverride"]["params"][0])
            if fd["__type"] == "Color":
                shown, real = (v if isinstance(v, str) else "#%06X" % v), {"id": "V_Int", "params": [color_int(v) if isinstance(v, str) else v]}
            else:
                shown, real = v, {"id": "V_" + fd["__type"], "params": [v]}
            fis.append({"__identifier": fd["identifier"], "__type": fd["__type"], "__value": shown, "__tile": None,
                        "defUid": fd["uid"], "realEditorValues": [real]})
        return {"__identifier": name, "__grid": [px[0] // GRID, px[1] // GRID], "__pivot": [d["pivotX"], d["pivotY"]],
                "__tags": [], "__tile": None, "__smartColor": d["color"], "__worldX": px[0], "__worldY": px[1],
                "iid": iid(rng), "width": d["width"], "height": d["height"], "defUid": d["uid"], "px": px,
                "fieldInstances": fis}

    floor_y = (PXH - FLOOR_ROW * GRID) / U  # 1.5
    mound_y = (PXH - 24 * GRID) / U         # 3.0
    platC_y = (PXH - 18 * GRID) / U         # 6.0
    hollow_y = (PXH - 29 * GRID) / U        # 0.5
    ledge_y = (PXH - 22 * GRID) / U         # 4.0
    under_b_y = (PXH - 23 * GRID) / U       # 3.5

    entities = [
        ent("PlayerStart", 4.5, floor_y),
        # Lights: warm sun shafts, fireflies, one on the far tree layer.
        ent("Light", 7.0, floor_y + 4.7, color="#FFD59A", intensity=1.6, radius=6.5, height=2.5, depth=0.0, flicker=0.0),
        ent("Light", 11.8, floor_y + 0.9, color="#FFE27A", intensity=1.8, radius=3.0, height=0.6, depth=0.0, flicker=0.3),
        ent("Light", 12.0, floor_y + 4.5, color="#FFB070", intensity=1.0, radius=10.0, height=1.0, depth=11.0, flicker=0.0),
        ent("Light", 24.0, floor_y + 6.0, color="#FFCF8A", intensity=2.2, radius=7.0, height=2.0, depth=0.0, flicker=0.05),
        ent("Light", 42.0, floor_y + 4.0, color="#FF9E5E", intensity=2.2, radius=6.5, height=1.5, depth=0.0, flicker=0.12),
        # Crystals
        ent("Crystal", 10.5, floor_y, variant=0),
        ent("Crystal", 26.5, platC_y, variant=1),
        ent("Crystal", 33.0, hollow_y, variant=2),
        ent("Crystal", 45.0, ledge_y, variant=3),
        # Props on the gameplay plane (depth > 0 = behind the player, fogged a little)
        ent("Prop", 8.6, floor_y, frame="tree 0", depth=1.2, flipX=False),
        ent("Prop", 14.3, mound_y, frame="tree_dead 0", depth=2.5, flipX=True),
        ent("Prop", 27.5, floor_y, frame="tree 0", depth=1.6, flipX=True),
        ent("Prop", 43.5, ledge_y, frame="tree_dead 0", depth=0.8, flipX=False),
        ent("Prop", 2.6, floor_y, frame="bush 0", depth=0.4, flipX=False),
        ent("Prop", 11.2, floor_y, frame="bush 0", depth=0.3, flipX=True),
        ent("Prop", 21.5, floor_y, frame="bush 0", depth=0.4, flipX=False),
        ent("Prop", 35.5, floor_y, frame="bush 0", depth=0.3, flipX=True),
        ent("Prop", 40.5, ledge_y, frame="bush 0", depth=0.4, flipX=False),
        # vines hang from platform B's underside (their pivot is near the top-left)
        ent("Prop", 20.5, under_b_y, frame="vine 0", depth=0.2, flipX=False),
        ent("Prop", 22.8, under_b_y, frame="vine 0", depth=0.2, flipX=True),
        # Critters
        ent("Critter", 7.4, floor_y, anim="fox_idle", flipX=True),
        ent("Critter", 15.9, mound_y, anim="bunny_idle", flipX=False),
    ]
    # Foreground props: authored so they show at the intended place for the camera near them.
    FG = (-0.3, -0.15)
    fg_entities = []
    for (frame, dx, dy, flip) in [
        ("tree_leaves 0", 1.6, 8.2, True), ("tree_canopy 0", 12.6, -0.8, False),
        ("tree_leaves 0", 20.0, 8.4, False), ("tree_canopy 0", 27.0, -0.4, True),
        ("tree_leaves 0", 34.5, 8.2, True), ("tree_canopy 0", 46.5, -0.2, False),
    ]:
        ax, ay = displayed_to_authored(dx, dy, FG[0], FG[1], cam_for(dx))
        fg_entities.append(ent("Prop", ax, ay, frame=frame, depth=-2.0, flipX=flip))

    # ---- layer defs (front -> back)
    def layer_def(ident, typ, grid, fx=0.0, fy=0.0, tileset_uid=None, int_values=None):
        return {"__type": typ, "identifier": ident, "type": typ, "uid": uid(), "doc": None, "uiColor": None,
                "gridSize": grid, "guideGridWid": 0, "guideGridHei": 0, "displayOpacity": 1, "inactiveOpacity": 0.6,
                "hideInList": False, "hideFieldsWhenInactive": True, "canSelectWhenInactive": True,
                "renderInWorldView": True, "pxOffsetX": 0, "pxOffsetY": 0, "parallaxFactorX": fx,
                "parallaxFactorY": fy, "parallaxScaling": False, "requiredTags": [], "excludedTags": [],
                "autoTilesKilledByOtherLayerUid": None, "uiFilterTags": [], "useAsyncRender": False,
                "intGridValues": int_values or [], "intGridValuesGroups": [], "autoRuleGroups": [],
                "autoSourceLayerDefUid": None, "tilesetDefUid": tileset_uid, "tilePivotX": 0, "tilePivotY": 0,
                "biomeFieldUid": None}

    ldefs = [
        layer_def("Entities", "Entities", GRID),
        layer_def("Foreground_d-2", "Entities", GRID, *FG),
        layer_def("Collision", "IntGrid", GRID, int_values=[
            {"value": 1, "identifier": "solid", "color": "#2F5A50", "tile": None, "groupUid": 0}]),
        layer_def("Ground_d0", "Tiles", GRID, tileset_uid=ts_grass["uid"]),
    ] + [layer_def(n, "Tiles", 36, fx, fy, ts_bg[img]["uid"]) for n, img, fx, fy, _ in PARALLAX]
    ldef = {d["identifier"]: d for d in ldefs}

    level_iid = iid(rng)

    def layer_inst(ident, cw, ch, grid, offset_y=0, ts=None, int_csv=None, tiles=None, ents=None):
        d = ldef[ident]
        return {"__identifier": ident, "__type": d["type"], "__cWid": cw, "__cHei": ch, "__gridSize": grid,
                "__opacity": 1, "__pxTotalOffsetX": 0, "__pxTotalOffsetY": offset_y,
                "__tilesetDefUid": ts["uid"] if ts else None, "__tilesetRelPath": ts["relPath"] if ts else None,
                "iid": iid(rng), "levelId": 0, "layerDefUid": d["uid"], "pxOffsetX": 0, "pxOffsetY": offset_y,
                "visible": True, "optionalRules": [], "intGridCsv": int_csv or [], "autoLayerTiles": [],
                "seed": rng.randrange(9999999), "overrideTilesetUid": None, "gridTiles": tiles or [],
                "entityInstances": ents or []}

    def grid_tile(px, src, ts_w, grid):
        t = (src[1] // grid) * (ts_w // grid) + src[0] // grid
        return {"px": px, "src": src, "f": 0, "t": t, "d": [(px[1] // grid) * CW + px[0] // grid], "a": 1}

    ground_tiles = [grid_tile([x * GRID, y * GRID], [c * GRID, r * GRID], 80, GRID)
                    for y in range(CH) for x in range(CW) if g[y][x] for c, r in [ground_tile(g, x, y)]]

    instances = [
        layer_inst("Entities", CW, CH, GRID, ents=entities),
        layer_inst("Foreground_d-2", CW, CH, GRID, ents=fg_entities),
        layer_inst("Collision", CW, CH, GRID, int_csv=[v for row in g for v in row]),
        layer_inst("Ground_d0", CW, CH, GRID, ts=ts_grass, tiles=ground_tiles),
    ]
    bw, bh, bg = 576, 324, 36
    pcw, pch = math.ceil(PXW / bg), math.ceil(PXH / bg)
    for name, img, fx, fy, bottom in PARALLAX:
        # Displayed bottom at the starting camera (low) -> authored bottom -> layer pixel offset.
        _, authored_bottom = displayed_to_authored(0, bottom, fx, fy, (0, CAM_LOW_Y))
        top_px = int(round(PXH - authored_bottom * U - bh))
        tiles = []
        for copy_x in range(0, PXW, bw):
            for ty in range(bh // bg):
                for tx in range(bw // bg):
                    cx = copy_x // bg + tx
                    if cx >= pcw:
                        continue
                    t = grid_tile([cx * bg, ty * bg], [tx * bg, ty * bg], bw, bg)
                    t["d"] = [ty * pcw + cx]
                    tiles.append(t)
        instances.append(layer_inst(name, pcw, pch, bg, offset_y=top_px, ts=ts_bg[img], tiles=tiles))

    level = {
        "identifier": "Showcase", "iid": level_iid, "uid": uid(), "worldX": 0, "worldY": 0, "worldDepth": 0,
        "pxWid": PXW, "pxHei": PXH, "__bgColor": "#8CA9AD", "bgColor": None, "useAutoIdentifier": False,
        "bgRelPath": None, "bgPos": None, "bgPivotX": 0.5, "bgPivotY": 0.5, "__smartColor": "#C5D4D6",
        "__bgPos": None, "externalRelPath": None, "fieldInstances": [], "layerInstances": instances,
        "__neighbours": [],
    }

    project = {
        "__header__": {"fileType": "LDtk Project JSON", "app": "LDtk", "doc": "https://ldtk.io/json",
                       "schema": "https://ldtk.io/files/JSON_SCHEMA.json", "appAuthor": "Sebastien 'deepnight' Benard",
                       "appVersion": "1.5.3", "url": "https://ldtk.io"},
        "iid": iid(rng), "jsonVersion": "1.5.3", "appBuildId": 473703, "nextUid": uid() + 1,
        "identifierStyle": "Capitalize", "toc": [], "worldLayout": "Free", "worldGridWidth": PXW,
        "worldGridHeight": PXH, "defaultLevelWidth": PXW, "defaultLevelHeight": PXH, "defaultPivotX": 0.5,
        "defaultPivotY": 1, "defaultGridSize": GRID, "defaultEntityWidth": 16, "defaultEntityHeight": 16,
        "bgColor": "#40465B", "defaultLevelBgColor": "#8CA9AD", "minifyJson": False, "externalLevels": False,
        "exportTiled": False, "simplifiedExport": False, "imageExportMode": "None", "exportLevelBg": True,
        "pngFilePattern": None, "backupOnSave": False, "backupLimit": 10, "backupRelPath": None,
        "levelNamePattern": "Level_%idx", "tutorialDesc": None, "customCommands": [], "flags": [],
        "defs": {"layers": ldefs, "entities": list(ent_defs.values()),
                 "tilesets": [ts_grass] + list(ts_bg.values()), "enums": [], "externalEnums": [], "levelFields": []},
        "levels": [level], "worlds": [], "dummyWorldIid": iid(rng),
    }
    out = ASSETS / "levels"
    out.mkdir(parents=True, exist_ok=True)
    (out / "showcase.ldtk").write_text(json.dumps(project, indent=1), encoding="utf-8")
    print(f"showcase.ldtk: {CW}x{CH} cells, {len(ground_tiles)} ground tiles, "
          f"{len(entities) + len(fg_entities)} entities")


if __name__ == "__main__":
    main()
