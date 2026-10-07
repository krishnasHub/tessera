"""Drawing on RGBA numpy images (height x width x 4, uint8). Coordinates are pixels, x right, y down."""
import numpy as np

from .color import hex_color, mix


def rng(seed):
    return np.random.default_rng(seed)


def blank(w, h):
    return np.zeros((h, w, 4), np.uint8)


def solid(col, w, h=None):
    a = np.zeros((h or w, w, 4), np.uint8)
    a[...] = col
    return a


def ellipse_mask(w, h, cx, cy, rx, ry):
    Y, X = np.mgrid[0:h, 0:w]
    return ((X - cx) / rx) ** 2 + ((Y - cy) / ry) ** 2 <= 1


def poly_mask(pts, w, h=None):
    """Inside a polygon [(x, y), ...] (even-odd rule), sampled at pixel centres."""
    h = h or w
    Y, X = np.mgrid[0:h, 0:w] + 0.5
    inside = np.zeros((h, w), bool)
    m = len(pts)
    for i in range(m):
        x0, y0 = pts[i]; x1, y1 = pts[(i + 1) % m]
        cond = ((y0 > Y) != (y1 > Y)) & (X < (x1 - x0) * (Y - y0) / (y1 - y0 + 1e-9) + x0)
        inside ^= cond
    return inside


def outline(a, col=(34, 26, 44, 255)):
    """A one-pixel outline around everything opaque."""
    m = a[..., 3] > 0
    d = m | np.roll(m, 1, 0) | np.roll(m, -1, 0) | np.roll(m, 1, 1) | np.roll(m, -1, 1)
    a[d & ~m] = col
    return a


def disc(a, cx, cy, r, col):
    h, w, _ = a.shape
    a[ellipse_mask(w, h, cx, cy, r, r)] = col


def line(a, x0, y0, x1, y1, col, width=1):
    n = int(max(abs(x1 - x0), abs(y1 - y0)) * 2) + 1
    h, w, _ = a.shape
    for i in range(n + 1):
        t = i / n
        x, y = x0 + (x1 - x0) * t, y0 + (y1 - y0) * t
        for dx in range(-(width // 2), width - width // 2):
            for dy in range(-(width // 2), width - width // 2):
                xi, yi = int(round(x + dx)), int(round(y + dy))
                if 0 <= xi < w and 0 <= yi < h:
                    a[yi, xi] = col


def speckle(a, r, cols, p):
    """Scatter colours from cols over a fraction p of the pixels (r: a numpy Generator)."""
    m = r.random((a.shape[0], a.shape[1])) < p
    idx = r.integers(0, len(cols), size=m.sum())
    a[m] = np.array(cols, np.uint8)[idx]
    return a


def vgrad(a, y0, y1, c0, c1):
    """Rows y0..y1 shade from hex colour c0 to c1."""
    for y in range(y0, y1):
        t = (y - y0) / max(1, y1 - y0 - 1)
        a[y, :] = mix(hex_color(c0), hex_color(c1), t)


def noise_band(a, y0, y1, base, seed, jag=6, step=3):
    """A band from a jagged top edge near y0 down to y1 in hex colour base (hills, rooftops, treelines)."""
    r = np.random.default_rng(seed)
    h = 0.0
    for x in range(0, a.shape[1]):
        if x % step == 0:
            h = np.clip(h + r.uniform(-jag, jag) * 0.4, -jag, jag)
        top = int(y0 + h)
        a[max(0, top):y1, x] = hex_color(base)
