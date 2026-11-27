"""Generate the placeholder terrain textures (deterministic, random.seed(1)). The author replaces them later.

Usage: python tools/gen_terrain_textures.py   (needs assets/kaa/tilesets/grass.png and dirt.png: tools/import_kaa.py)

Outputs assets/terrain/{forest,cave}/{edge,side,fill}.png (+ _n normal maps), read by LevelLoader:
  edge  32x16  grass strip, hangs under the surface line (top row = the line). Opaque for 12 rows, ragged below.
  side  32x16  earth/stone strip for walls and undersides, opaque over all 16 rows.
  fill  32x32  seamless earth, repeats every 1 x 1 u.
Also assets/kaa/tilesets/grass_slopes{,_n,_m}.png: grass.png plus a row of 6 grid slope tiles (IntGrid 3..8).
forest palette comes from the kaa grass (edge) and dirt (side, fill) tilesets; cave is the same, darker and bluer.
"""
import random
from collections import Counter
from pathlib import Path

from PIL import Image

from import_kaa import make_normal_map

ROOT = Path(__file__).resolve().parent.parent / "examples" / "iki_demo"  # the demo game's folder (assets/ lives there)
TILESETS = ROOT / "assets" / "kaa" / "tilesets"
OUT = ROOT / "assets" / "terrain"
W, H, CAP, SOLID = 32, 16, 6, 12


def palette(path: Path, n: int = 4):
    """The n most frequent opaque colours, darkest first."""
    img = Image.open(path).convert("RGBA")
    px = [img.getpixel((x, y)) for y in range(img.height) for x in range(img.width)]
    top = [c for c, _ in Counter(p[:3] for p in px if p[3] == 255).most_common(n)]
    return sorted(top, key=lambda c: 0.2126 * c[0] + 0.7152 * c[1] + 0.0722 * c[2])


def strip(rng, ramp, cap_src=None, solid=SOLID):
    """edge/side: CAP rows lit, then a dithered fall-off to the darkest tone, then ragged drips (when solid < H)."""
    img = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    px = img.load()
    dark, mid, light = ramp[0], ramp[len(ramp) // 2], ramp[-1]
    for x in range(W):
        for y in range(solid):
            if y < CAP:
                s = cap_src.getpixel((16 + x, y)) if cap_src else (0, 0, 0, 0)
                c = s[:3] if s[3] == 255 else (light if rng.random() < 0.8 else mid)
            else:
                t = (y - CAP) / (SOLID - CAP)  # 0 -> 1 down the fall-off (dark beyond SOLID)
                c = mid if rng.random() > t else dark
            px[x, y] = c + (255,)
        run = rng.randint(0, 3)
        for y in range(solid, min(solid + run, H)):
            px[x, y] = dark + (255,)
    return img


def seamless_fill(rng, ramp):
    """32x32 earth: mid base with wrapped dark / light blobs."""
    base = ramp[len(ramp) // 2]
    img = Image.new("RGBA", (32, 32), base + (255,))
    px = img.load()
    for tone, count in ((ramp[0], 26), (ramp[-1], 14)):
        for _ in range(count):
            x, y, w, h = rng.randrange(32), rng.randrange(32), rng.randint(1, 3), rng.randint(1, 2)
            for dy in range(h):
                for dx in range(w):
                    px[(x + dx) % 32, (y + dy) % 32] = tone + (255,)
    return img


def cave(img: Image.Image) -> Image.Image:
    out = img.copy()
    px = out.load()
    for y in range(out.height):
        for x in range(out.width):
            r, g, b, a = px[x, y]
            px[x, y] = (int(r * 0.45), int(g * 0.5), int(b * 0.65), a)
    return out


def normal_for(img: Image.Image, wrap_x: bool, wrap_y: bool) -> Image.Image:
    """Normal map of a repeating texture: computed on a tiled copy so tile borders get no false rim."""
    nx, ny = (3 if wrap_x else 1), (3 if wrap_y else 1)
    big = Image.new("RGBA", (img.width * nx, img.height * ny))
    for j in range(ny):
        for i in range(nx):
            big.paste(img, (i * img.width, j * img.height))
    n = make_normal_map(big)
    ox, oy = (img.width if wrap_x else 0), (img.height if wrap_y else 0)
    return n.crop((ox, oy, ox + img.width, oy + img.height))


def slope_height(value, x):
    """Surface height in px above the cell bottom at pixel column x (centre) for IntGrid slope values 3-8."""
    u = x + 0.5 if value % 2 else 16 - (x + 0.5)  # odd values rise to the right
    return {3: u, 4: u, 5: u / 2, 6: u / 2, 7: 8 + u / 2, 8: 8 + u / 2}[value]


def slope_sheet(src: Image.Image) -> Image.Image:
    """grass.png (5x5 tiles, kept at the same src coords) + row 5: slope tiles for IntGrid values 3..8 (cols 0..5).
    Each is cut along its diagonal: the top tile (2,0) follows the slope, the inner tile (2,1) fills below it."""
    out = Image.new(src.mode, (6 * 16, 6 * 16), (0, 0, 0, 0))
    out.paste(src, (0, 0))
    top, inner = src.crop((32, 0, 48, 16)), src.crop((32, 16, 48, 32))
    for value in range(3, 9):
        tile = Image.new(src.mode, (16, 16), (0, 0, 0, 0))
        for x in range(16):
            surface = 16 - slope_height(value, x)  # px from the cell top
            for y in range(16):
                d = int(y - surface)  # px under the surface
                if y + 0.5 >= surface:
                    tile.putpixel((x, y), top.getpixel((x, d)) if d < 16 else inner.getpixel((x, y)))
        out.paste(tile, ((value - 3) * 16, 5 * 16))
    return out


def main():
    rng = random.Random(1)
    grass = Image.open(TILESETS / "grass.png").convert("RGBA")
    # Grid slope tiles: grass_slopes.png (+ the same cut of grass_n / grass_m), used by Cave1's Ground_d0.
    for suffix in ("", "_n", "_m"):
        src = TILESETS / f"grass{suffix}.png"
        if src.exists():
            slope_sheet(Image.open(src).convert("RGBA")).save(TILESETS / f"grass_slopes{suffix}.png")
            print(f"assets/kaa/tilesets/grass_slopes{suffix}.png: 96x96")
    earth = palette(TILESETS / "dirt.png")
    greens = palette(TILESETS / "grass.png")
    textures = {
        "edge": (strip(rng, greens, grass), True, False),
        "side": (strip(rng, earth, solid=H), True, False),  # fully opaque: no fill gaps on faces steeper than ~49 deg
        "fill": (seamless_fill(rng, earth), True, True),
    }
    for style, tint in (("forest", lambda i: i), ("cave", cave)):
        d = OUT / style
        d.mkdir(parents=True, exist_ok=True)
        for name, (img, wx, wy) in textures.items():
            img = tint(img)
            img.save(d / f"{name}.png")
            normal_for(img, wx, wy).save(d / f"{name}_n.png")
            print(f"assets/terrain/{style}/{name}.png: {img.width}x{img.height}")


if __name__ == "__main__":
    main()
