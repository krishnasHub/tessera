"""
The character sprite-sheet layout Tessera plays (TSSpriteSheet in TesseraGameplay/Public/TSSprite.h; keep in step):

    4 columns x 13 rows of square frames
    rows 0-3   facing the camera: idle(2)  walk(4)  attack(4)  hurt(1)
    rows 4-7   facing away:       idle     walk     attack     hurt
    rows 8-11  facing right:      idle     walk     attack     hurt     (left = mirrored at runtime)
    row 12     dead (col 0), guard (shield raised) facing the camera / away / right (cols 1-3)
"""
import numpy as np

COLS, ROWS = 4, 13
DIRS = ("down", "up", "side")
ACTIONS = (("idle", 2), ("walk", 4), ("attack", 4), ("hurt", 1))
EXTRA_ROW = 12      # dead + guard frames


def row_of(direction, action):
    return DIRS.index(direction) * 4 + [a for a, _ in ACTIONS].index(action)


def build(draw, frame, dead, guard):
    """A whole sheet. draw(direction, action, frame_index) -> frame image; dead -> image; guard(direction) -> image."""
    out = np.zeros((frame * ROWS, frame * COLS, 4), np.uint8)
    for d in DIRS:
        for action, n in ACTIONS:
            for f in range(n):
                put(out, frame, row_of(d, action), f, draw(d, action, f))
    put(out, frame, EXTRA_ROW, 0, dead)
    for i, d in enumerate(DIRS):
        put(out, frame, EXTRA_ROW, 1 + i, guard(d))
    return out


def put(out, frame, row, col, img):
    out[row * frame:(row + 1) * frame, col * frame:(col + 1) * frame] = img
