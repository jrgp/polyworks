PolyWorks — Soldat map editor
=============================

A portable Windows build.  Extract the PolyWorks folder anywhere and run
PolyWorks.exe.  There is nothing to install: no setup program, no registry
entries, no environment variables and no changes to PATH.  Deleting the folder
removes the application completely.

Settings are written to polyworks.ini next to PolyWorks.exe, so they travel
with the folder — put it on a USB stick and your preferences come along.  If
the folder is read-only (for example under Program Files), settings fall back
to your Windows user profile instead.


Soldat's artwork
----------------

Maps reference textures and scenery that ship with Soldat itself, which cannot
be redistributed here.  There are two ways to make them available:

  * Copy Soldat's Textures and Scenery-gfx folders into this folder, replacing
    the empty ones.  They are then found automatically.

  * Or point PolyWorks at your Soldat installation under
    File > Preferences > Soldat directory.

Maps stored inside a Soldat installation also resolve their artwork from that
installation directly, with no configuration at all.

Where a scenery image cannot be found, PolyWorks substitutes a placeholder and
prints the paths it searched, so the map still opens and stays editable.  A
missing polygon texture is not replaced: the polygons are drawn in their own
vertex colours instead, which is what the original editor does.


What is in this folder
----------------------

  PolyWorks.exe            The editor.  Everything it needs is compiled in;
                           it uses only Windows' own DLLs.
  polyworks.ini            Your settings (created on first exit).
  PolyWorks Help.html      The original manual.  Start here.
  Help\                    Images used by the manual.
  skins\default\           Interface bitmaps, mouse cursors and colours.
  palettes\                Colour palettes.  current.txt is saved on exit.
  lists\                   Named scenery lists.
  Workspace\               Saved positions of the floating tool windows.
  Maps\                    Somewhere to keep your maps (optional).
  Prefabs\                 Saved prefabs (.pfb).
  Textures\                Drop Soldat's textures here.
  Scenery-gfx\             Drop Soldat's scenery here.

Maps can be opened from anywhere on disk; none of these folders restrict where
you may keep your work.


Requirements
------------

64-bit Windows and a graphics driver providing OpenGL 1.2 or later, which
covers essentially any machine that can run Soldat.
