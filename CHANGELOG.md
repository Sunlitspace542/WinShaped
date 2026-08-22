# Changelog

## Unreleased

- Added a complete illustrated user guide with 59 native-build screenshots,
  covering every editor menu, modeling workflow, transform, animation feature,
  Preview control, format, exporter, SNES data path, keyboard shortcut, CLI
  option, limit, and troubleshooting case.

## 1.0.0 - 2026-08-22

- First complete native 64-bit Windows release.
- Reconstructed the original 640x480 editor layout, menus, dialogs, keyboard
  controls, editing operations, animation workflow, and modal Preview.
- Added native readers for `3DG1`, `3DCG`, `3DAN`, and `3DA1` models.
- Added recovered GZS, BSP, PC, and internal-format exporters.
- Added an embedded 3D-system replacement for the unavailable Argonaut target
  renderer and preserved the recovered SNES data contracts.
- Added `--bsp [input [output]]` for interactive or automatic BSP ASM export.
- Added byte-exact, behavioral, rendering-edge, DOS-oracle, and pixel-layout
  regression coverage.
