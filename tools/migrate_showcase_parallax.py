"""One-off migration (continuous world, task 3): the showcase's 5 parallax tile layers become the layers of
assets/regions/forest.region.json, together with the current look (showcase.look.json).

- Each layer keeps its image, depth and parallax (the look's panel overrides win over the LDtk values).
- offset = world top-left of the image's first copy minus (1 - parallax) * start-room centre, so that the
  layer sits where the old room-centre anchor put it (the look does not change). repeatX = true.
- showcase.ldtk loses only those layer instances, their layer defs and their now-unused tilesets, and its
  level "Showcase" gets the level field region = "forest" (the field def is added if missing).
  Everything else is written back byte for byte (same JSON layout as the file already has).

Run once from the repo root: python tools/migrate_showcase_parallax.py
"""
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
LDTK = ROOT / "assets" / "levels" / "showcase.ldtk"
LOOK = ROOT / "assets" / "levels" / "showcase.look.json"
REGION = ROOT / "assets" / "regions" / "forest.region.json"
LEVEL = "Showcase"
LAYERS = ["Reeds_d2", "Shore_d4", "Lake_d7", "Trees_d11", "Forest_d18"]  # front -> back
PX_PER_UNIT = 32.0


def r6(v):
    """Same 6-significant-digit rounding the engine writes."""
    return float(f"{v:.6g}")


def dump_like(obj, original_text):
    """json.dumps with the original file's layout; asserts an unchanged load/dump is byte-identical."""
    newline = "\r\n" if "\r\n" in original_text else "\n"
    text = json.dumps(obj, indent=1)
    if original_text.endswith(("\n", "\r\n")):
        text += "\n"
    return text.replace("\n", newline)


def tileset_refs(node, uids, found):
    """Collects tileset uids referenced anywhere (keys such as tilesetDefUid, tilesetUid, iconTilesetUid)."""
    if isinstance(node, dict):
        for k, v in node.items():
            if "ileset" in k and isinstance(v, int) and v in uids:
                found.add(v)
            tileset_refs(v, uids, found)
    elif isinstance(node, list):
        for v in node:
            tileset_refs(v, uids, found)


def main():
    ldtk_text = LDTK.read_bytes().decode("utf-8")
    root = json.loads(ldtk_text)
    if dump_like(root, ldtk_text) != ldtk_text:
        sys.exit("showcase.ldtk does not round-trip byte for byte; refusing to rewrite it")
    look = json.loads(LOOK.read_text(encoding="utf-8"))

    defs = root["defs"]
    layer_defs = {d["identifier"]: d for d in defs["layers"]}
    tilesets = {t["uid"]: t for t in defs["tilesets"]}
    missing = [n for n in LAYERS if n not in layer_defs]
    if missing:
        sys.exit(f"layer defs not found (already migrated?): {missing}")
    level = next(l for l in root["levels"] if l["identifier"] == LEVEL)
    wx, wy = level["worldX"], level["worldY"]
    anchor = ((wx + level["pxWid"] / 2) / PX_PER_UNIT, -(wy + level["pxHei"] / 2) / PX_PER_UNIT)

    layers = []
    for name in LAYERS:
        d = layer_defs[name]
        ts = tilesets[d["tilesetDefUid"]]
        inst = next(li for li in level["layerInstances"] if li["__identifier"] == name)
        tiles = inst["gridTiles"] + inst["autoLayerTiles"]
        ox, oy = inst["__pxTotalOffsetX"], inst["__pxTotalOffsetY"]
        # Every tile must belong to an unflipped copy of the whole image, copies side by side every image width.
        origins = {(t["px"][0] - t["src"][0] + ox, t["px"][1] - t["src"][1] + oy) for t in tiles}
        xs = sorted(o[0] for o in origins)
        assert all(t["f"] == 0 for t in tiles), name
        assert len({o[1] for o in origins}) == 1, (name, origins)
        assert all(b - a == ts["pxWid"] for a, b in zip(xs, xs[1:])), (name, xs)
        top_left_px = (xs[0], next(iter(origins))[1])

        override = look.get("layers", {}).get(name, {})
        parallax = override.get("parallax", [1 - d["parallaxFactorX"], 1 - d["parallaxFactorY"]])
        depth = override.get("depth", float(re.search(r"_d(-?[\d.]+)$", name).group(1)))
        top_left = ((wx + top_left_px[0]) / PX_PER_UNIT, -(wy + top_left_px[1]) / PX_PER_UNIT)
        offset = [r6(top_left[i] - (1 - parallax[i]) * anchor[i]) for i in range(2)]
        image = (LDTK.parent / ts["relPath"]).resolve().relative_to(ROOT).as_posix()
        layers.append({"depth": depth, "image": image, "offset": offset, "parallax": parallax, "repeatX": True})
        print(f"{name}: {image} depth {depth} parallax {parallax} offset {offset}")

    # Region file: the look minus the overrides of the layers that moved here.
    for name in LAYERS:
        look.get("layers", {}).pop(name, None)
    REGION.parent.mkdir(parents=True, exist_ok=True)
    REGION.write_text(json.dumps({"layers": layers, "look": look}, indent=2, sort_keys=True) + "\n", encoding="utf-8",
                      newline="\n")

    # LDtk: drop the instances, the layer defs, then tilesets nobody references any more.
    removed_uids = {layer_defs[n]["uid"] for n in LAYERS}
    candidate_ts = {layer_defs[n]["tilesetDefUid"] for n in LAYERS}
    for lv in root["levels"]:
        lv["layerInstances"] = [li for li in lv["layerInstances"] if li["layerDefUid"] not in removed_uids]
    defs["layers"] = [d for d in defs["layers"] if d["uid"] not in removed_uids]
    still_used = set()
    tileset_refs({k: v for k, v in root.items() if k != "defs"}, candidate_ts, still_used)
    tileset_refs({k: v for k, v in defs.items() if k != "tilesets"}, candidate_ts, still_used)
    defs["tilesets"] = [t for t in defs["tilesets"] if t["uid"] not in candidate_ts - still_used]
    print("removed tilesets:", sorted(candidate_ts - still_used))

    # Level field "region": String def (shaped like an existing String field def), value "forest".
    field = next((f for f in defs["levelFields"] if f["identifier"] == "region"), None)
    if field is None:
        template = next(f for e in defs["entities"] for f in e["fieldDefs"] if f["__type"] == "String")
        field = json.loads(json.dumps(template))
        field.update({"identifier": "region", "uid": root["nextUid"], "doc": "Region file: assets/regions/<region>.region.json",
                      "defaultOverride": {"id": "V_String", "params": ["forest"]}})
        root["nextUid"] += 1
        defs["levelFields"].append(field)
    for lv in root["levels"]:
        fis = [f for f in lv["fieldInstances"] if f["defUid"] != field["uid"]]
        value = "forest" if lv["identifier"] == LEVEL else next(
            (f["__value"] for f in lv["fieldInstances"] if f["defUid"] == field["uid"]), "forest")
        fis.append({"__identifier": "region", "__type": "String", "__value": value, "__tile": None,
                    "defUid": field["uid"], "realEditorValues": [{"id": "V_String", "params": [value]}]})
        lv["fieldInstances"] = fis

    LDTK.write_bytes(dump_like(root, ldtk_text).encode("utf-8"))
    print(f"wrote {REGION.relative_to(ROOT)} and {LDTK.relative_to(ROOT)}")


if __name__ == "__main__":
    main()
