unit pw.types;

{$mode objfpc}{$H+}
{$PackRecords 1}  { All records are explicitly packed; no hidden padding }

interface

{ ---------------------------------------------------------------------------
  pw.types — all persistent and editor-only record types for PolyWorks.

  Naming convention:
    TPW*   = types used in the binary PMS file (packed, layout matches VB6).
    TEditor* = editor-only types (not written to file).

  VB6 type mapping:
    VB6 Long     = LongInt  (4 bytes, signed)
    VB6 Integer  = SmallInt (2 bytes, signed)
    VB6 Boolean  = WordBool (2 bytes; True = -1 = $FFFF, False = 0)
    VB6 Single   = Single   (4 bytes, IEEE 754)
    VB6 Byte     = Byte     (1 byte)
  --------------------------------------------------------------------------- }

uses
  SysUtils;

const
  MAX_POLYS      = 4000;
  MAX_SCENERY    = 500;
  MAX_WAYPOINTS  = 500;
  MAX_SPAWNS     = 256;
  MAX_COLLIDERS  = 128;
  MAX_LIGHTS     = 255;
  MAX_CONNECTIONS_PER_WP = 20;
  SECTOR_NUM     = 25;   { sector range is -25..25 = 51 cells per axis }
  SECTOR_CELLS   = 51;   { SECTOR_NUM*2+1 }
  PMS_VERSION    = 11;

  POLY_TYPE_NORMAL          = 0;
  POLY_TYPE_ONLY_BULLETS    = 1;
  POLY_TYPE_ONLY_PLAYERS    = 2;
  POLY_TYPE_NO_COLLIDE      = 3;
  POLY_TYPE_ICE             = 4;
  POLY_TYPE_DEADLY          = 5;
  POLY_TYPE_BLOODY_DEADLY   = 6;
  POLY_TYPE_HURTS           = 7;
  POLY_TYPE_REGENERATES     = 8;
  POLY_TYPE_LAVA            = 9;
  POLY_TYPE_ALPHA_BULLETS   = 10;
  POLY_TYPE_ALPHA_PLAYERS   = 11;
  POLY_TYPE_BRAVO_BULLETS   = 12;
  POLY_TYPE_BRAVO_PLAYERS   = 13;
  POLY_TYPE_CHARLIE_BULLETS = 14;
  POLY_TYPE_CHARLIE_PLAYERS = 15;
  POLY_TYPE_DELTA_BULLETS   = 16;
  POLY_TYPE_DELTA_PLAYERS   = 17;
  POLY_TYPE_BOUNCY          = 18;
  POLY_TYPE_EXPLOSIVE       = 19;
  POLY_TYPE_HIT_MULTIPLY    = 20;
  POLY_TYPE_COLLIDER        = 21;
  POLY_TYPE_NO_PASS         = 22;
  POLY_TYPE_SHIFT           = 23;
  POLY_TYPE_WEATHER         = 24;
  POLY_TYPE_NO_FOOTSTEPS    = 25;
  POLY_TYPE_MAX             = 25;

type
  { ---- File-compatible (packed) types ------------------------------------ }

  { A single vertex as stored in the PMS file.
    TCustomVertex in VB6 modOpenSoldatMap.bas.
    Size: 7 × 4 = 28 bytes. }
  TPMSVertex = packed record
    X    : Single;   { world X coordinate }
    Y    : Single;   { world Y coordinate }
    Z    : Single;   { always 1.0 on disk }
    Rhw  : Single;   { always 1.0 on disk (RHW = 1/W for D3D pre-transformed) }
    Color: LongWord; { ARGB colour, 0xAARRGGBB }
    Tu   : Single;   { texture U coordinate }
    Tv   : Single;   { texture V coordinate }
  end;

  { Edge normal / perp vertex as stored in the PMS file.
    TVertexHit in VB6.  Size = 3 × 4 = 12 bytes. }
  TPMSNormal = packed record
    X: Single;   { sin(edge angle) × bounciness }
    Y: Single;   { cos(edge angle) × bounciness }
    Z: Single;   { always 1.0 on disk (bounciness for type-18, else 1) }
  end;

  { Three edge normals.  TPolyHit in VB6.  Size = 3 × 12 = 36 bytes. }
  TPMSPolyNormals = packed record
    N: array[1..3] of TPMSNormal;
  end;

  { Main polygon body.  TPolygon in VB6.
    Size = 3 × 28 + 36 = 84 + 36 = 120 bytes. }
  TPMSPolygon = packed record
    V   : array[1..3] of TPMSVertex;
    Perp: TPMSPolyNormals;
  end;

  { Complete on-disk polygon entry.  TMapFile_Polygon in VB6.
    Size = 120 + 1 = 121 bytes. }
  TPMSPolyEntry = packed record
    Poly    : TPMSPolygon;
    PolyType: Byte;
  end;

  { Scenery (prop) instance as stored in the PMS file.  TProp in VB6.
    NOTE: active is VB6 Boolean = WordBool (2 bytes; -1 = True, 0 = False).
    Size = 2+2+4+4+4+4+4+4+4+4+4+4 = 44 bytes.
    (2 Byte fields + 10 Long/Single fields = 2×2 + 10×4 = 44) }
  TPMSProp = packed record
    Active  : WordBool;  { VB6 Boolean in UDT = 2 bytes }
    Style   : SmallInt;  { scenery style index, 1-based }
    Width   : LongInt;   { original texture width in pixels }
    Height  : LongInt;   { original texture height in pixels }
    X       : Single;    { world X position }
    Y       : Single;    { world Y position }
    Rotation: Single;    { rotation in radians }
    ScaleX  : Single;
    ScaleY  : Single;
    Alpha   : LongInt;   { opacity 0..255 (stored as Long in VB6) }
    Color   : LongInt;   { ARGB tint }
    Level   : LongInt;   { render layer 0..2 }
  end;

  { Scenery texture filename entry.  TMapFile_Scenery in VB6.
    sceneryName(0..50) = 51-byte array; byte 0 = length.
    Size = 51 + 4 = 55 bytes. }
  TPMSSceneryName = packed record
    Name: array[0..50] of Byte;  { byte 0 = string length }
    Date: LongInt;               { file date/time stamp }
  end;

  { Circle collider.  TCollider in VB6.
    Size = 4+4+4+4 = 16 bytes. }
  TPMSCollider = packed record
    Active: LongInt;  { non-zero = active }
    X     : Single;
    Y     : Single;
    Radius: Single;
  end;

  { Spawn point as saved to file.  TSaveSpawnPoint in VB6.
    Note: X and Y are Long (integer world coords), not Single.
    Size = 4+4+4+4 = 16 bytes. }
  TPMSSpawnPoint = packed record
    Active: LongInt;
    X     : LongInt;  { integer world X }
    Y     : LongInt;  { integer world Y }
    Team  : LongInt;  { 0=general, 1=alpha, 2=bravo, 3=charlie, 4=delta,
                        5=frogger, 6=yellow, 7=red, etc. }
  end;

  { Waypoint as saved to file.  TNewWaypoint in VB6.
    Connections array always has MAX_CONNECTIONS_PER_WP slots (20 Longs).
    Size = 4+4+4+4+7+5+4+80 = 112 bytes. }
  TPMSWaypoint = packed record
    Active       : LongInt;
    ID           : LongInt;
    X            : LongInt;
    Y            : LongInt;
    Left         : Byte;
    Right        : Byte;
    Up           : Byte;
    Down         : Byte;
    M2           : Byte;
    PathNum      : Byte;
    Special      : Byte;
    Crap         : array[1..5] of Byte;  { VB6: crap(1 To 5) }
    ConnectionsNum: LongInt;
    Connections  : array[1..20] of LongInt;
  end;

  { Three-channel colour.  TColor in VB6.  Size = 3 bytes. }
  TColor3 = packed record
    R, G, B: Byte;
  end;

  { Light source.  TLightSource in VB6.
    Note: range is VB6 Integer = SmallInt (2 bytes).
    Size = 1+3+4+2+4+4+4 = 22 bytes. }
  TPMSLight = packed record
    Selected : Byte;
    Color    : TColor3;
    Intensity: Single;
    Range    : SmallInt;  { VB6 Integer = 2 bytes }
    X        : Single;
    Y        : Single;
    Z        : Single;
  end;

  { Sketch vertex.  TSketchVertex in VB6.  Size = 12 bytes. }
  TPMSSketchVertex = packed record
    X, Y, Z: Single;
  end;

  { Sketch line segment.  TSketchLine in VB6.  Size = 24 bytes. }
  TPMSSketchLine = packed record
    V: array[1..2] of TPMSSketchVertex;
  end;

  { Map options header.  TOptions in VB6.
    Size = 39+25+4+4+4+1+1+1+1+4 = 84 bytes.
    NOTE: arch.md incorrectly stated 75 bytes; the VB6 source is authoritative. }
  TPMSOptions = packed record
    MapName       : array[0..38] of Byte;  { byte 0 = length, max 38 chars }
    TextureName   : array[0..24] of Byte;  { byte 0 = length, max 24 chars }
    BgColor1      : LongWord;   { ARGB background colour top }
    BgColor2      : LongWord;   { ARGB background colour bottom }
    StartJet      : LongInt;    { jet amount; VB6 Long = 4 bytes }
    GrenadePacks  : Byte;
    Medikits      : Byte;
    Weather       : Byte;
    Steps         : Byte;
    MapRandomID   : LongInt;    { -1=PolyWorks native, 0=new/blank, >0=compiled }
  end;

  { ---- Editor-only types (not written to file) --------------------------- }

  { 2D float vector used throughout the editor. }
  TVector2 = record
    X, Y: Single;
  end;

  { Editor representation of a polygon vertex with both world and screen coords. }
  TEditorVertex = record
    World : TVector2;   { source of truth: world coordinates }
    Screen: TVector2;   { cache: (World - Scroll) * Zoom }
    Color : TColor3;    { unlit base colour (R,G,B) }
    Alpha : Byte;       { vertex alpha }
    Tu, Tv: Single;     { texture coordinates }
  end;

  { Editor polygon — owns world coords; screen coords are a rebuild cache. }
  TEditorPoly = record
    V        : array[1..3] of TEditorVertex;
    Perp     : TPMSPolyNormals;  { precomputed normals }
    PolyType : Byte;
    Selected : array[1..3] of Boolean;  { per-vertex selection }
  end;

  { Editor scenery instance. }
  TEditorScenery = record
    Style   : SmallInt;   { 1-based index into SceneryNames }
    X, Y    : Single;     { world position }
    ScreenX, ScreenY: Single;
    Rotation: Single;
    ScaleX, ScaleY: Single;
    Width, Height: LongInt;  { from texture }
    Alpha   : Byte;
    Color   : LongInt;
    Level   : Byte;   { render layer: 0=back, 1=middle, 2=front }
    Selected: Boolean;
  end;

  { Editor spawn point. }
  TEditorSpawn = record
    X, Y    : Single;
    Team    : Byte;
    Selected: Boolean;
    Active  : Boolean;
  end;

  { Editor collider. }
  TEditorCollider = record
    X, Y, Radius: Single;
    Selected    : Boolean;
    Active      : Boolean;
  end;

  { Editor waypoint. }
  TEditorWaypoint = record
    X, Y       : Single;
    ID         : LongInt;
    Left, Right, Up, Down, M2: Boolean;
    PathNum    : Byte;
    Special    : Byte;
    NumConns   : Integer;  { number of valid connections from this WP }
    Selected   : Boolean;
    TempIndex  : Integer;  { used during delete/remap operations }
  end;

  { Flat connection pair — stored in TMapDocument.Connections[]. }
  TEditorConnection = record
    Point1, Point2: Integer;  { 1-based waypoint indices }
  end;

  { Editor light source. }
  TEditorLight = record
    X, Y, Z   : Single;
    Color     : TColor3;
    Intensity : Single;
    Range     : Single;
    Selected  : Boolean;
  end;

  { Selection rectangle (world coordinates). }
  TSelRect = record
    X1, Y1, X2, Y2: Single;
  end;

  { Selection mode for region/click selection. }
  TSelMode = (smReplace, smAdd, smSubtract);

  { ---- Array types used by TPMSData ------------------------------------- }

  TPMSPolyArray      = array of TPMSPolyEntry;
  TPMSPropArray      = array of TPMSProp;
  TPMSScenNameArray  = array of TPMSSceneryName;
  TPMSColliderArray  = array of TPMSCollider;
  TPMSSpawnArray     = array of TPMSSpawnPoint;
  TPMSWaypointArray  = array of TPMSWaypoint;
  TPMSLightArray     = array of TPMSLight;
  TPMSSketchArray    = array of TPMSSketchLine;

  { ---- Sector table type ------------------------------------------------ }

  { Variable-length sector cell: contains poly count + up to 256 indices. }
  TSectorCell = record
    PolyCount: SmallInt;              { number of polygons in this cell }
    PolyIndex: array of SmallInt;     { 1-based polygon indices }
  end;

  { Full 51×51 sector table.
    Indexed [X][Y] where X, Y in range 0..SECTOR_CELLS-1
    (corresponding to world sectors -SECTOR_NUM..SECTOR_NUM). }
  TSectorTable = array[0..SECTOR_CELLS-1, 0..SECTOR_CELLS-1] of TSectorCell;

implementation

end.
