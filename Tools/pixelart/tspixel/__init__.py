"""
tspixel: small helpers for drawing pixel art in code (Python + numpy, no PIL).

Images are numpy uint8 arrays of shape (height, width, 4), RGBA. A game keeps its own art (what characters,
props and textures look like) and builds on these:

  png      write_png(path, rgba), upscale(rgba, k)
  color    hex colours, mixing
  canvas   blank images, masks (ellipse, polygon), outline, disc, line, gradient, speckle, jagged band
  sheet    the character sprite-sheet layout Tessera's UTSSpriteComponent plays (TSSpriteSheet in TSSprite.h)

Use from a game's script:

    import os, sys
    sys.path.insert(0, os.path.join(<repo>, "unreal", "Plugins", "Tessera", "Tools", "pixelart"))
    from tspixel import png, color, canvas, sheet
"""
