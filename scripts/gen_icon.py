#!/usr/bin/env python3
"""Generate resources/appicon.ico: rounded-square accent gradient tile with a
white isometric cube outline (the product mark)."""

from PIL import Image, ImageDraw

BASE = 256.0


def rounded_rect_mask(size, radius):
    mask = Image.new("L", (size, size), 0)
    d = ImageDraw.Draw(mask)
    d.rounded_rectangle([0, 0, size - 1, size - 1], radius=radius, fill=255)
    return mask


def vertical_gradient(size, top, bottom):
    grad = Image.new("RGB", (1, size))
    for y in range(size):
        t = y / max(1, size - 1)
        color = tuple(int(top[i] + (bottom[i] - top[i]) * t) for i in range(3))
    # draw pixel by pixel via putpixel on (1, size)
    px = grad.load()
    for y in range(size):
        t = y / max(1, size - 1)
        color = tuple(int(top[i] + (bottom[i] - top[i]) * t) for i in range(3))
        px[0, y] = color
    return grad.resize((size, size))


def draw_cube(layer, size, stroke_scale):
    """Isometric cube: hexagon outline + inner Y edges, drawn with round caps."""
    d = ImageDraw.Draw(layer)
    s = size / BASE

    cx, cy = 128.0 * s, 134.0 * s
    r = 74.0 * s
    # hexagon vertices (top, upper-right, lower-right, bottom, lower-left, upper-left)
    pts = [
        (cx, cy - r),
        (cx + r * 0.866, cy - r * 0.5),
        (cx + r * 0.866, cy + r * 0.5),
        (cx, cy + r),
        (cx - r * 0.866, cy + r * 0.5),
        (cx - r * 0.866, cy - r * 0.5),
    ]
    center = (cx, cy)
    width = max(2, int(round(13.0 * s)))

    def seg(a, b):
        d.line([a, b], fill=(255, 255, 255, 255), width=width)
        # round caps
        rad = width / 2.0
        for p in (a, b):
            d.ellipse([p[0] - rad, p[1] - rad, p[0] + rad, p[1] + rad], fill=(255, 255, 255, 255))

    for i in range(6):
        seg(pts[i], pts[(i + 1) % 6])
    seg(center, pts[1])  # to upper-right
    seg(center, pts[5])  # to upper-left
    seg(center, pts[3])  # to bottom


def make_icon(size):
    radius = int(size * 0.225)
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))

    # gradient tile: #2F9BFF -> #0871D6
    grad = vertical_gradient(size, (47, 155, 255), (8, 113, 214)).convert("RGBA")
    tile = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    tile.paste(grad, (0, 0))
    tile.putalpha(rounded_rect_mask(size, radius))

    # subtle top highlight
    highlight = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    hd = ImageDraw.Draw(highlight)
    hd.rounded_rectangle([0, 0, size - 1, size - 1], radius=radius, fill=(255, 255, 255, 26))
    tile = Image.alpha_composite(tile, highlight)

    img = Image.alpha_composite(img, tile)

    cube_layer = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    draw_cube(cube_layer, size, size / BASE)
    img = Image.alpha_composite(img, cube_layer)
    return img


def main():
    sizes = [256, 128, 64, 48, 32, 24, 16]
    images = [make_icon(s) for s in sizes]
    images[0].save("resources/appicon.ico", format="ICO", sizes=[(s, s) for s in sizes])
    images[0].save("resources/appicon_preview.png", format="PNG")
    print("icon written")


if __name__ == "__main__":
    main()
