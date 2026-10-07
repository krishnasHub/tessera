"""Colours as RGBA tuples of 0..255."""


def hex_color(hexstr, a=255):
    """'#rrggbb' -> (r, g, b, a)."""
    h = hexstr.lstrip("#")
    return (int(h[0:2], 16), int(h[2:4], 16), int(h[4:6], 16), a)


def mix(c, d, t):
    """Blend c toward d by t (0..1); keeps c's alpha."""
    return tuple(int(round(c[i] * (1 - t) + d[i] * t)) for i in range(3)) + (c[3],)
