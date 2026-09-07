# VB6 UI port catalog

## Theme
- Main viewport/background: black (`$000000`)
- Floating forms and panel surfaces: dark slate (`$4A3C31`)
- Accent/title fallback: raised slate (`$614B3D`)
- Text: white (`$FFFFFF`)
- Fonts: Arial with 8pt-equivalent panel text and 9.75pt-equivalent main form text

## Floating windows
- All editor panels are now borderless `TForm` windows with custom 17px title bars.
- Each floating form uses the matching bitmap from `installer/skins/default/`.
- Each floating form includes a 16×16 hide button in the custom title bar.
- Windows stay on top of the main editor surface.

## Palette mapping
- `frmtools` is now a faithful 64px-wide floating palette.
- Tool button art comes from `tool_gfx.bmp` and preserves normal/hover/selected states.
- Implemented bitmap slots:
  - Transform → `TOOL_SELECT`
  - Poly Creation → `TOOL_POLY`
  - Scenery → `TOOL_SCENERY`
  - Objects → `TOOL_SPAWN`
  - Waypoints → `TOOL_WAYPOINT`
  - Sketch → `TOOL_SKETCH`
  - Lights → `TOOL_LIGHT`
- Slots for connection/collider visuals remain empty because the VB6 sheet has no matching modern equivalent.

## Floating form sizes
- Tools palette: 64×241 client pixels to preserve the full 7th bitmap row under a 17px title bar.
- Info / Properties: 208×208.
- Display settings: 208×160.
- Scenery: 208×170.
- Waypoints: 208×160.

## Main form changes
- Removed the standard Lazarus toolbar.
- Tool selection now lives in the floating palette and existing menus/shortcuts.
- Main editing area stays dark with a black viewport.
- Status text is shown on a dark status bar.

## Runtime assets
- Skin lookup tries deployed, bundle-resource, and development-relative paths.
- Linux and macOS build scripts copy `installer/skins/` into build output.
- The macOS bundle also embeds skins in `PolyWorks.app/Contents/Resources/skins/`.

## Crash handling
- `polyworks.lpr` installs a global exception handler.
- Crashes are written to stderr and `~/.polyworks-crash.log` before a user-facing error dialog is shown.
