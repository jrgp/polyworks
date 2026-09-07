# PolyWorks Rewrite — Implementation Progress

## Status: ALL MILESTONES COMPLETE ✅

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
| M9 — Renderer | renderer.pas | compile | ✅ done |
| M10 — Viewport | viewport.pas | compile | ✅ done |
| M11 — Tools | tools.pas | compile | ✅ done |
| M12 — Main form | frmmain.pas, polyworks.lpr, polyworks.lpi | compile | ✅ done |
| M13 — Panels | frminfo, frmdisplay, frmscenery, frmwaypoints, frmtools | compile | ✅ done |
| M14 — Dialogs | frmmap, frmpreferences, frmcolor, frmtexture | compile | ✅ done |

**Total automated tests: 116 passing, 0 failures**
**Application binary: build/polyworks (27 MB ELF64)**

### Real map validation

All 97 real .pms maps from `maps/` directory load successfully.
Round-trip (load → save → reload) verified to be byte-accurate.

## Build/test

```sh
./build.sh   # builds Lazarus application → build/polyworks
./test.sh    # runs all 116 headless tests (no display required)
```

## What works

- Open/save/compile Soldat .pms maps (all 97 real maps load)
- OpenGL viewport rendering (polygons, scenery, objects, lights, sketch)
- All 9 editing tools (select, polygon, spawn, waypoint, collider, light, sketch)
- Undo/redo (ring buffer, configurable depth)
- Map Properties dialog (edit name, texture, colors, options)
- Preferences dialog (Soldat path, undo depth, grid, snap)
- View toggles (polygons, wireframe, points, grid, objects, etc.)
- Pan (middle-button drag) and zoom (scroll wheel)
- Keyboard shortcuts (Ctrl+Z/Y, Delete, arrow keys)
- Config persistence (INI file)
- Texture loading via stb_image (color-key transparency)
- Prefab save/load (.pwf format)

## Commits

- `ec13c7b` — arch.md: VB6 archaeology
- `ef3f220` — plan.md: 62-step implementation roadmap
- `12aab7a` — M1-M3: types, PMS format, geometry (64 tests)
- `eec0209` — M4-M6: map model, lights, undo (106 tests)
- `11fd747` — M7-M9: prefab, config, OpenGL renderer (116 tests)
- `b997de4` — M10-M12: viewport, tools, main form (lazbuild ✅)
- `4702fb1` — M13-M14: panels, dialogs
- `1206c0a` — Dialog integration into main form
