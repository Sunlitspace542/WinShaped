# Recovered SNES development-hardware path

The original SNES menu was a front end for Argonaut's DOS-era downloader, not
an emulator built into Shaped. The recovered routines perform these operations:

- At startup, opening `sdemo.rom` successfully sets `SNESMode`; `CalcMenuMask`
  then exposes condition bit `0x2000`, enabling the SNES menu and its controls.

- `Send SNES` assembles a temporary renderer command stream and invokes the
  external `DL` utility.
- `Palette` reads exactly 512 bytes from `<name>.col`, copies them to
  `tmp.dat`, then runs `DL >NUL: -a5E00 tmp.dat`.
- `Pal num` writes one byte, `0xD0 + selected palette`, then runs
  `DL >NUL: -a6000 tmp.dat`.
- `Textures` runs `DL >NUL: -a8000 <name>.dat`.
- `COLTABS.DAT` is opened from the working directory and parsed as `%s %s %d`.
  `COLTAB` records populate the Colour Table selection dialog and retain their
  numeric values, `TEXMAP` records populate the Textures submenu, and `COLOUR`
  records populate the Palette submenu.
- Selecting a colour table copies `GRID.DAT` to `tmpshape.asm`, substituting the
  selected table name through the template's second line. A negative numeric
  value enables the `VNORMALS`/`VN` smooth-shading block in SNES BSP output.

The native port keeps these payload formats but removes the unavailable DOS
process dependency. It loads named SNES BGR555 `.col` files directly for the
embedded 3D-system renderer (without changing the EGA editor), exposes palette
numbers 0-F and their exact `0xD0`-`0xDF` command bytes, selects named texture
data, recreates the colour-table template source, and writes the recovered SNES
BSP assembler itself. Status text retains the original payload destinations (`0x5E00`,
`0x6000`, and `0x8000`) so the data/address contract is explicit without `DL`.

Evidence: `_SendTexFunct` at `08de:30f8`, the following palette sender at
`08de:3146`, `_SendPalNumFunct` at `08de:3273`, `_ReadColTabs` at `0d79:4112`,
and strings at DGROUP offsets `1ad4` through `1b37`.
