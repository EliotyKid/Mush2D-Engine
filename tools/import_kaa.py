"""Import selected GameMaker sprites from the kaa project into Mush2D assets.

Usage: python tools/import_kaa.py <path-to-kaa>/sprites

Outputs (under assets/kaa/):
  characters/kaa_player.{png,json}, characters/kaa_animals.{png,json}
  props/kaa_props.{png,json}
  tilesets/grass.png, tilesets/dirt.png
  parallax/bg1..bg5.png, parallax/water.png
Atlases use the Aseprite "JSON hash" format that AtlasLoader reads, plus
frameTags (one per GameMaker sprite) and a per-frame "pivot" (GameMaker origin).
Every image also gets a generated <name>_n.png normal map (see make_normal_map).
"""
import json
import math
import re
import shutil
import sys
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "assets" / "kaa"

ATLASES = {
    "characters/kaa_player": {
        "idle": "sPlayer", "run": "sPlayerRun", "jump": "sPlayerJump", "fall": "sPlayerFall",
        "push": "sPlayerPush", "attack_prepare": "sPlayerPreparingAtack",
        "attack_stay": "sPlayerStayAtack", "attack_leave": "sPlayerLeaveAtack",
        "balance_front": "sPlayerFrontBalance", "balance_back": "sPlayerBackBalance",
    },
    "characters/kaa_animals": {
        "fox_idle": "sFoxIdle", "fox_walk": "sFoxWalk",
        "wolf_idle": "sWolfIdle", "wolf_walk": "sWolfWalk", "wolf_howl": "sWolfHowl",
        "bunny_idle": "sBunnyIdle", "bunny_walk": "sBunnyWalk",
        "bird_idle": "sBirdIdle", "bird_fly": "sBirdFly",
    },
    "props/kaa_props": {
        "tree": "sTree", "tree_trunk": "sTreeTronco", "tree_leaves": "sTreeFolhas",
        "tree_dead": "stree1", "tree_canopy": "Sprite123", "bush": "sArbusto",
        "vine": "svine", "dust": "sDust", "sun": "sSun", "stars": "sStars",
    },
}
SINGLES = {
    "tilesets/grass": "Sprite63", "tilesets/dirt": "Sprite91",
    "parallax/bg1": "sBg1", "parallax/bg2": "sBg2", "parallax/bg3": "sBg3",
    "parallax/bg4": "sBg4", "parallax/bg5": "sBg5", "parallax/water": "sBgWater",
}


def read_gm_sprite(sprites_dir: Path, name: str):
    """Return (frames as RGBA images in order, origin (x, y), fps)."""
    d = sprites_dir / name
    yy = (d / f"{name}.yy").read_text(encoding="utf-8")
    frames_block = yy[yy.index('"frames"'):]
    frames_block = frames_block[:frames_block.index("]")]
    ids = re.findall(r'"name":"([0-9a-f-]{36})"', frames_block)
    num = lambda key, default: float(m.group(1)) if (m := re.search(rf'"{key}":\s*(-?[\d.]+)', yy)) else default
    origin = (int(num("xorigin", 0)), int(num("yorigin", 0)))
    fps = num("playbackSpeed", 15.0)
    if int(num("playbackSpeedType", 0)) == 1:  # frames per game frame -> assume 60 game fps
        fps *= 60.0
    frames = [Image.open(d / f"{i}.png").convert("RGBA") for i in ids]
    return frames, origin, fps


def make_normal_map(img: Image.Image, strength: float = 2.0) -> Image.Image:
    """Height = distance to nearest transparent pixel (saturated at 3 px), normalized,
    plus 0.3 * luminance; normal from a Sobel filter. Alpha is copied."""
    w, h = img.size
    px = img.load()
    alpha = [[px[x, y][3] > 0 for x in range(w)] for y in range(h)]
    sat = 3
    dist = [[0.0] * w for _ in range(h)]
    for y in range(h):
        for x in range(w):
            if not alpha[y][x]:
                continue
            best = sat
            for dy in range(-sat, sat + 1):
                for dx in range(-sat, sat + 1):
                    xx, yy = x + dx, y + dy
                    outside = not (0 <= xx < w and 0 <= yy < h)
                    if outside or not alpha[yy][xx]:
                        best = min(best, math.hypot(dx, dy))
            dist[y][x] = best / sat
    height = [[0.0] * w for _ in range(h)]
    for y in range(h):
        for x in range(w):
            r, g, b, _ = px[x, y]
            lum = (0.2126 * r + 0.7152 * g + 0.0722 * b) / 255.0
            height[y][x] = dist[y][x] + 0.3 * lum if alpha[y][x] else 0.0

    def hgt(x, y):
        return height[min(max(y, 0), h - 1)][min(max(x, 0), w - 1)]

    out = Image.new("RGBA", (w, h))
    op = out.load()
    for y in range(h):
        for x in range(w):
            gx = (hgt(x + 1, y - 1) + 2 * hgt(x + 1, y) + hgt(x + 1, y + 1)
                  - hgt(x - 1, y - 1) - 2 * hgt(x - 1, y) - hgt(x - 1, y + 1))
            gy = (hgt(x - 1, y + 1) + 2 * hgt(x, y + 1) + hgt(x + 1, y + 1)
                  - hgt(x - 1, y - 1) - 2 * hgt(x, y - 1) - hgt(x + 1, y - 1))
            # Image Y points down; normal map Y points up (green = up).
            nx, ny, nz = -gx * strength, gy * strength, 1.0
            ln = math.sqrt(nx * nx + ny * ny + nz * nz)
            enc = lambda v: int(round((v / ln * 0.5 + 0.5) * 255))
            op[x, y] = (enc(nx), enc(ny), enc(nz), px[x, y][3])
    return out


def save_with_normal(img: Image.Image, stem: Path):
    stem.parent.mkdir(parents=True, exist_ok=True)
    img.save(stem.with_suffix(".png"))
    make_normal_map(img).save(stem.parent / f"{stem.name}_n.png")


def build_atlas(sprites_dir: Path, stem: Path, tags: dict, pad: int = 1, max_w: int = 1024):
    entries = []  # (tag, index, frame image, origin, fps)
    for tag, gm_name in tags.items():
        frames, origin, fps = read_gm_sprite(sprites_dir, gm_name)
        entries += [(tag, i, f, origin, fps) for i, f in enumerate(frames)]
    # Shelf packing, tallest first.
    order = sorted(range(len(entries)), key=lambda k: -entries[k][2].height)
    pos, x, y, shelf_h, width = {}, pad, pad, 0, pad
    for k in order:
        fw, fh = entries[k][2].size
        if x + fw + pad > max_w:
            x, y, shelf_h = pad, y + shelf_h + pad, 0
        pos[k] = (x, y)
        x += fw + pad
        shelf_h = max(shelf_h, fh)
        width = max(width, x)
    height = y + shelf_h + pad
    atlas = Image.new("RGBA", (width, height), (0, 0, 0, 0))
    frames_json, tags_json, cursor = {}, [], 0
    for tag in tags:
        idx = [k for k, e in enumerate(entries) if e[0] == tag]
        for k in idx:
            _, i, img, origin, fps = entries[k]
            fx, fy = pos[k]
            atlas.paste(img, (fx, fy))
            frames_json[f"{tag} {i}"] = {
                "frame": {"x": fx, "y": fy, "w": img.width, "h": img.height},
                "rotated": False, "trimmed": False,
                "spriteSourceSize": {"x": 0, "y": 0, "w": img.width, "h": img.height},
                "sourceSize": {"w": img.width, "h": img.height},
                "duration": int(round(1000.0 / fps)) if fps > 0 else 100,
                "pivot": {"x": origin[0], "y": origin[1]},
            }
        tags_json.append({"name": tag, "from": cursor, "to": cursor + len(idx) - 1, "direction": "forward"})
        cursor += len(idx)
    save_with_normal(atlas, stem)
    meta = {"app": "tools/import_kaa.py", "image": f"{stem.name}.png", "format": "RGBA8888",
            "size": {"w": width, "h": height}, "scale": "1", "frameTags": tags_json}
    stem.with_suffix(".json").write_text(json.dumps({"frames": frames_json, "meta": meta}, indent=1), encoding="utf-8")
    print(f"{stem.relative_to(ROOT)}: {len(entries)} frames, {width}x{height}")


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    sprites_dir = Path(sys.argv[1])
    if OUT.exists():
        shutil.rmtree(OUT)
    for stem, tags in ATLASES.items():
        build_atlas(sprites_dir, OUT / stem, tags)
    for stem, gm_name in SINGLES.items():
        frames, _, _ = read_gm_sprite(sprites_dir, gm_name)
        save_with_normal(frames[0], OUT / stem)
        print(f"assets/kaa/{stem}.png: {frames[0].width}x{frames[0].height}")


if __name__ == "__main__":
    main()
