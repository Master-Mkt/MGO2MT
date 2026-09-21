# Software-authored raster fixtures

These small images contain generated color patterns, not original game artwork.
`manifest.json` records their original byte sizes and SHA-256 values.

- `alpha.png`: 16×16 RGBA. Pixel `(x,y)` is `(17*x,17*y,17*(x XOR y),[0,64,128,255][x%4])`.
- `pages.tiff`: two uncompressed pages. The first matches `alpha.png`; the second is 24×20 opaque magenta.
- `sizes.ico`: two PNG-backed icon entries. The first matches `alpha.png`; the second is 32×32 opaque magenta. The runtime deliberately reads entry zero.
- `animated.gif`: two indexed frames. The first is 16×16 palette index `(x+y)%4`, with palette RGBA `(0,0,0,0)`, `(255,23,7,255)`, `(11,241,53,255)`, `(19,47,239,255)`. The second is solid palette index 3. The runtime deliberately displays frame zero without animation.

The C++ test compares actual Windows decoding against these authored patterns and checks that optional DDS conversion does not modify input files.
