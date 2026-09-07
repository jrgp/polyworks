# PolyWorks Rewrite — Implementation Progress

## Status: M1–M9 complete, M10–M12 in progress

### Completed milestones

| Milestone | Files | Tests | Status |
|-----------|-------|-------|--------|
| M1 — Types & utilities | pw.types.pas, pw.utils.pas | 25 | ✅ done |
| M2 — PMS file format | pw.pms.pas | 12 | ✅ done |
| M3 — Geometry | pw.geometry.pas | 27 | ✅ done |
| M4 — Map document | pw.map.pas | 27 | ✅ done |
| M5 — Lights | pw.lights.pas | 6 | ✅ done |
| M6 — Undo/redo | pw.undo.pas | 9 | ✅ done |
| M7 — Prefab format | pw.prefab.pas | 5 | ✅ done |
| M8 — Config | pw.config.pas | 6 | ✅ done |
| M9 — Renderer | renderer.pas | smoke | ✅ done |

**Total automated tests: 116 passing, 0 failures**

### In progress

| Milestone | Files | Status |
|-----------|-------|--------|
| M10 — Viewport | viewport.pas | 🔄 in progress |
| M11 — Tools | tools.pas | 🔄 in progress |
| M12 — Main form | frmmain.pas, polyworks.lpr, polyworks.lpi | 🔄 in progress |

### Not started

- M13 — Floating panels (frmscenery, frmwaypoints, frmdisplay, frmtools, frminfo)
- M14 — Dialogs (frmmap, frmcolor, frmpreferences, frmtexture)

## Real map validation

All 97 real .pms maps from `maps/` directory load successfully.
Round-trip (load → save → reload) verified to be byte-accurate.

## Build/test

```sh
./build.sh   # builds headless test suite
./test.sh    # runs all 116 tests
lazbuild polyworks.lpi  # builds GUI application (once M12 complete)
```

## Commits

- `ec13c7b` — arch.md: VB6 archaeology
- `ef3f220` — plan.md: 62-step implementation roadmap
- `12aab7a` — M1-M3: types, PMS format, geometry (64 tests)
- `11fd747` — M7-M9: prefab, config, OpenGL renderer (116 tests)
