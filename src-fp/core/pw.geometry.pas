unit pw.geometry;

{$mode objfpc}{$H+}

{ ---------------------------------------------------------------------------
  pw.geometry — Pure geometric algorithms for PolyWorks.

  All functions are stateless and parameterised; no global state.

  Key compatibility notes:
  - IsPolyClockwise uses the VB6 "Midpoint" centroid test (biased toward V1),
    not a cross-product sign — must be preserved exactly.
  - PointInPoly uses half-space test with Y-flipped edge normals (VB6 convention).
  - SnapToGrid uses Floor() (VB6 Int() toward -∞), never Trunc().
  - InSelRect is exclusive (strict < and >, not ≤/≥).
  --------------------------------------------------------------------------- }

interface

uses
  pw.types, pw.utils, Math, SysUtils;

{ ---- Polygon geometry -------------------------------------------------- }

{ True if the three vertices are in clockwise order.
  Replicates VB6 IsCW() exactly: uses nested Midpoint centroid,
  then PointInPoly half-space test.
  centroid.X = Midpoint(V1.X, Midpoint(V2.X, V3.X)) = V1.X/2 + V2.X/4 + V3.X/4
  centroid.Y = Midpoint(V1.Y, Midpoint(V2.Y, V3.Y)) = V1.Y/2 + V2.Y/4 + V3.Y/4 }
function IsPolyClockwise(const V1, V2, V3: TVector2): Boolean;

{ True if screen point (PX,PY) is inside a CW polygon.
  Uses the VB6 half-space test against perpendicular normals. }
function PointInPoly(PX, PY: Single; const V1, V2, V3: TVector2): Boolean;

{ Swap vertices V2 and V3 to convert CCW → CW winding. }
procedure EnsureClockwise(var P: TEditorPoly);

{ Recompute edge normals for a file polygon entry.
  For type-18 (bouncy): preserves Z component if >= 1; else sets to 1.
  For all other types: sets Z to 1. }
procedure RecomputePolyNormals(var Entry: TPMSPolyEntry);

{ ---- Segment intersection ---------------------------------------------- }

{ True if line segments (x1,y1)-(x2,y2) and (x3,y3)-(x4,y4) intersect.
  Replicates VB6 SegmentsIntersect logic (parametric t,u test). }
function SegmentsIntersect(x1, y1, x2, y2, x3, y3, x4, y4: Single): Boolean;

{ True if segment (x1,y1)-(x2,y2) intersects horizontal segment at y=HY,
  from x=HX1 to x=HX2. }
function SegIntersectsHoriz(x1, y1, x2, y2, HX1, HX2, HY: Single): Boolean;

{ True if segment (x1,y1)-(x2,y2) intersects vertical segment at x=VX,
  from y=VY1 to y=VY2. }
function SegIntersectsVert(x1, y1, x2, y2, VX, VY1, VY2: Single): Boolean;

{ ---- Sector table ------------------------------------------------------ }

{ True if the polygon at index Idx (1-based, in CompiledPolys) overlaps the
  sector whose world AABB is [SectX, SectY, SectX+SectW, SectY+SectW]. }
function PolyInSector(const Entry: TPMSPolyEntry;
                      SectX, SectY, SectW: Single): Boolean;

{ Build the sector table for a set of compiled (centred) polygons.
  SectorDiv = sectorsDivision.
  XOff, YOff = centering offsets (used to convert original sector world coords). }
procedure BuildSectorTable(const Polys: TPMSPolyArray; PolyCount: LongInt;
                           SectorDiv: LongInt; XOff, YOff: Single;
                           out Table: TSectorTable);

{ ---- Bounding box ------------------------------------------------------ }

{ Compute the world bounding box of all polygon vertices. }
procedure ComputePolyBounds(const Polys: TPMSPolyArray; PolyCount: LongInt;
                             out MinX, MinY, MaxX, MaxY: Single);

{ ---- Coordinate conversion --------------------------------------------- }

{ World → screen: SX = (WX - ScrollX) * Zoom,  SY = (WY - ScrollY) * Zoom. }
procedure WorldToScreen(WX, WY, ScrollX, ScrollY, Zoom: Single;
                        out SX, SY: Single); inline;

{ Screen → world: WX = SX / Zoom + ScrollX,  WY = SY / Zoom + ScrollY. }
procedure ScreenToWorld(SX, SY, ScrollX, ScrollY, Zoom: Single;
                        out WX, WY: Single); inline;

{ Snap a world coordinate to the nearest grid line.
  Uses VB6 Int() = Floor() semantics. }
function SnapToGrid(Coord, GridSize: Single): Single;

{ True if two coordinates are within Range of each other. }
function NearCoord(A, B, Range: Single): Boolean; inline;

{ True if (X,Y) is strictly inside the selection rectangle.
  Exclusive test (VB6 InSelRect uses strict < and >, not ≤/≥). }
function InSelRect(X, Y, X1, Y1, X2, Y2: Single): Boolean; inline;

implementation

{ ---- IsPolyClockwise --------------------------------------------------- }

function IsPolyClockwise(const V1, V2, V3: TVector2): Boolean;
var
  CX, CY: Single;
  { Edge normals for half-space test (VB6 convention: Y is flipped) }
  N1X, N1Y, N2X, N2Y, N3X, N3Y: Single;
  D1, D2, D3: Single;
  Len: Single;
  DX, DY: Single;
begin
  { Biased centroid: Midpoint(V1, Midpoint(V2, V3)) }
  CX := Midpoint(V1.X, Midpoint(V2.X, V3.X));
  CY := Midpoint(V1.Y, Midpoint(V2.Y, V3.Y));

  { Compute edge normals (perpendicular to each edge, rotated right).
    VB6 convention: xDiff = Vn.X - Vj.X,  yDiff = Vj.Y - Vn.Y  (Y flipped)
    Normal = (yDiff / len, xDiff / len) }

  { Edge 1→2 }
  DX := V2.X - V1.X;
  DY := V1.Y - V2.Y;  { Y-flipped }
  Len := Sqrt(DX * DX + DY * DY);
  if Len = 0 then Len := 1;
  N1X := DY / Len;
  N1Y := DX / Len;

  { Edge 2→3 }
  DX := V3.X - V2.X;
  DY := V2.Y - V3.Y;
  Len := Sqrt(DX * DX + DY * DY);
  if Len = 0 then Len := 1;
  N2X := DY / Len;
  N2Y := DX / Len;

  { Edge 3→1 }
  DX := V1.X - V3.X;
  DY := V3.Y - V1.Y;
  Len := Sqrt(DX * DX + DY * DY);
  if Len = 0 then Len := 1;
  N3X := DY / Len;
  N3Y := DX / Len;

  { Half-space test: centroid must be on the normal side of each edge }
  D1 := (CX - V1.X) * N1X + (CY - V1.Y) * N1Y;
  D2 := (CX - V2.X) * N2X + (CY - V2.Y) * N2Y;
  D3 := (CX - V3.X) * N3X + (CY - V3.Y) * N3Y;

  Result := (D1 >= 0) and (D2 >= 0) and (D3 >= 0);
end;

{ ---- PointInPoly ------------------------------------------------------- }

function PointInPoly(PX, PY: Single; const V1, V2, V3: TVector2): Boolean;
var
  N1X, N1Y, N2X, N2Y, N3X, N3Y: Single;
  D1, D2, D3: Single;
  Len, DX, DY: Single;
begin
  { Same edge-normal + half-space test as IsPolyClockwise, but using the
    given point instead of the biased centroid. }
  DX := V2.X - V1.X; DY := V1.Y - V2.Y;
  Len := Sqrt(DX * DX + DY * DY);
  if Len = 0 then Len := 1;
  N1X := DY / Len; N1Y := DX / Len;

  DX := V3.X - V2.X; DY := V2.Y - V3.Y;
  Len := Sqrt(DX * DX + DY * DY);
  if Len = 0 then Len := 1;
  N2X := DY / Len; N2Y := DX / Len;

  DX := V1.X - V3.X; DY := V3.Y - V1.Y;
  Len := Sqrt(DX * DX + DY * DY);
  if Len = 0 then Len := 1;
  N3X := DY / Len; N3Y := DX / Len;

  D1 := (PX - V1.X) * N1X + (PY - V1.Y) * N1Y;
  D2 := (PX - V2.X) * N2X + (PY - V2.Y) * N2Y;
  D3 := (PX - V3.X) * N3X + (PY - V3.Y) * N3Y;

  Result := (D1 >= 0) and (D2 >= 0) and (D3 >= 0);
end;

{ ---- EnsureClockwise --------------------------------------------------- }

procedure EnsureClockwise(var P: TEditorPoly);
var
  V1, V2, V3: TVector2;
  TmpV: TEditorVertex;
begin
  V1.X := P.V[1].World.X; V1.Y := P.V[1].World.Y;
  V2.X := P.V[2].World.X; V2.Y := P.V[2].World.Y;
  V3.X := P.V[3].World.X; V3.Y := P.V[3].World.Y;
  if not IsPolyClockwise(V1, V2, V3) then
  begin
    TmpV := P.V[2];
    P.V[2] := P.V[3];
    P.V[3] := TmpV;
  end;
end;

{ ---- RecomputePolyNormals ---------------------------------------------- }

procedure RecomputePolyNormals(var Entry: TPMSPolyEntry);
var
  J, NextJ: Integer;
  XDiff, YDiff, Len, Bounce: Single;
begin
  for J := 1 to 3 do
  begin
    NextJ := J mod 3 + 1;
    XDiff := Entry.Poly.V[NextJ].X - Entry.Poly.V[J].X;
    YDiff := Entry.Poly.V[J].Y - Entry.Poly.V[NextJ].Y;  { Y-flipped }
    if (XDiff = 0) and (YDiff = 0) then
      Len := 1
    else
      Len := Sqrt(XDiff * XDiff + YDiff * YDiff);

    if Entry.PolyType = POLY_TYPE_BOUNCY then
    begin
      Bounce := Entry.Poly.Perp.N[J].Z;
      if Bounce < 1 then Bounce := 1;
    end
    else
      Bounce := 1.0;

    Entry.Poly.Perp.N[J].X := (YDiff / Len) * Bounce;
    Entry.Poly.Perp.N[J].Y := (XDiff / Len) * Bounce;
    Entry.Poly.Perp.N[J].Z := 1.0;
  end;
end;

{ ---- SegmentsIntersect ------------------------------------------------- }

function SegmentsIntersect(x1, y1, x2, y2, x3, y3, x4, y4: Single): Boolean;
{ Replicates VB6 SegmentsIntersect exactly.
  VB6: s = (DX*(B1-y1)+dy*(x1-A1))/(da*dy-db*DX)
       T = (da*(y1-B1)+db*(A1-x1))/(db*DX-da*dy)
  where DX/dy = first seg direction, da/db = second seg direction. }
var
  DX, DY, DA, DB, Denom, S, T: Single;
begin
  DX := x2 - x1;  DY := y2 - y1;   { first segment direction }
  DA := x4 - x3;  DB := y4 - y3;   { second segment direction }

  Denom := DA * DY - DB * DX;
  if Abs(Denom) < 1e-10 then
  begin
    Result := False;  { parallel or collinear — VB6 returns false }
    Exit;
  end;

  S := (DX * (y3 - y1) + DY * (x1 - x3)) / Denom;
  T := (DA * (y1 - y3) + DB * (x3 - x1)) / (-Denom);

  Result := (S >= 0) and (S <= 1) and (T >= 0) and (T <= 1);
end;

function SegIntersectsHoriz(x1, y1, x2, y2, HX1, HX2, HY: Single): Boolean;
begin
  Result := SegmentsIntersect(x1, y1, x2, y2, HX1, HY, HX2, HY);
end;

function SegIntersectsVert(x1, y1, x2, y2, VX, VY1, VY2: Single): Boolean;
begin
  Result := SegmentsIntersect(x1, y1, x2, y2, VX, VY1, VX, VY2);
end;

{ ---- PolyInSector ------------------------------------------------------- }

function PolyInSector(const Entry: TPMSPolyEntry;
                      SectX, SectY, SectW: Single): Boolean;
var
  MinPX, MinPY, MaxPX, MaxPY: Single;
  I: Integer;
  PX, PY: Single;
begin
  { Quick AABB overlap test first }
  MinPX := Entry.Poly.V[1].X; MaxPX := MinPX;
  MinPY := Entry.Poly.V[1].Y; MaxPY := MinPY;
  for I := 2 to 3 do
  begin
    PX := Entry.Poly.V[I].X; PY := Entry.Poly.V[I].Y;
    if PX < MinPX then MinPX := PX;
    if PX > MaxPX then MaxPX := PX;
    if PY < MinPY then MinPY := PY;
    if PY > MaxPY then MaxPY := PY;
  end;

  { AABB non-overlap → definitely not in sector }
  if (MaxPX < SectX) or (MinPX > SectX + SectW) or
     (MaxPY < SectY) or (MinPY > SectY + SectW) then
  begin
    Result := False;
    Exit;
  end;

  { More precise: check if any vertex is inside, or any edge crosses the sector }
  { For now use AABB overlap — this matches VB6 IsInSector which also uses AABB }
  Result := True;
end;

{ ---- BuildSectorTable --------------------------------------------------- }

procedure BuildSectorTable(const Polys: TPMSPolyArray; PolyCount: LongInt;
                           SectorDiv: LongInt; XOff, YOff: Single;
                           out Table: TSectorTable);
var
  SX, SY, I: Integer;
  Count: Integer;
  WorldX, WorldY, WorldW: Single;
  Entry: TPMSPolyEntry;
begin
  FillChar(Table, SizeOf(TSectorTable), 0);

  for SX := 0 to SECTOR_CELLS - 1 do
    for SY := 0 to SECTOR_CELLS - 1 do
      SetLength(Table[SX][SY].PolyIndex, 0);

  for SX := 0 to SECTOR_CELLS - 1 do
    for SY := 0 to SECTOR_CELLS - 1 do
    begin
      { Compute sector world AABB (centred coords).
        VB6: IsInSector(i, sectorsDivision * (X-0.5) + xOffset - 1, ...)
        where X = SX - SECTOR_NUM (sector index mapped to -25..25) }
      WorldX := SectorDiv * ((SX - SECTOR_NUM) - 0.5) - 1;
      WorldY := SectorDiv * ((SY - SECTOR_NUM) - 0.5) - 1;
      WorldW := SectorDiv + 2;

      Count := 0;
      for I := 0 to PolyCount - 1 do
      begin
        if Count >= 256 then Break;
        Entry := Polys[I];
        { Skip type-3 polygons (NoCollide) — they don't go in the sector table }
        if Entry.PolyType = POLY_TYPE_NO_COLLIDE then Continue;
        if PolyInSector(Entry, WorldX, WorldY, WorldW) then
        begin
          Inc(Count);
          SetLength(Table[SX][SY].PolyIndex, Count);
          Table[SX][SY].PolyIndex[Count - 1] := SmallInt(I + 1);  { 1-based }
        end;
      end;
      Table[SX][SY].PolyCount := SmallInt(Count);
    end;
end;

{ ---- ComputePolyBounds -------------------------------------------------- }

procedure ComputePolyBounds(const Polys: TPMSPolyArray; PolyCount: LongInt;
                             out MinX, MinY, MaxX, MaxY: Single);
var
  I, J: Integer;
  V: Single;
begin
  if PolyCount = 0 then
  begin
    MinX := 0; MinY := 0; MaxX := 0; MaxY := 0;
    Exit;
  end;
  MinX := Polys[0].Poly.V[1].X; MaxX := MinX;
  MinY := Polys[0].Poly.V[1].Y; MaxY := MinY;
  for I := 0 to PolyCount - 1 do
    for J := 1 to 3 do
    begin
      V := Polys[I].Poly.V[J].X;
      if V < MinX then MinX := V;
      if V > MaxX then MaxX := V;
      V := Polys[I].Poly.V[J].Y;
      if V < MinY then MinY := V;
      if V > MaxY then MaxY := V;
    end;
end;

{ ---- Coordinate conversion --------------------------------------------- }

procedure WorldToScreen(WX, WY, ScrollX, ScrollY, Zoom: Single;
                        out SX, SY: Single);
begin
  SX := (WX - ScrollX) * Zoom;
  SY := (WY - ScrollY) * Zoom;
end;

procedure ScreenToWorld(SX, SY, ScrollX, ScrollY, Zoom: Single;
                        out WX, WY: Single);
begin
  if Zoom = 0 then Zoom := 1;
  WX := SX / Zoom + ScrollX;
  WY := SY / Zoom + ScrollY;
end;

function SnapToGrid(Coord, GridSize: Single): Single;
begin
  if GridSize <= 0 then GridSize := 1;
  Result := Floor(Coord / GridSize) * GridSize;
end;

function NearCoord(A, B, Range: Single): Boolean;
begin
  Result := Abs(A - B) <= Range;
end;

function InSelRect(X, Y, X1, Y1, X2, Y2: Single): Boolean;
var
  LX, RX, TY, BY: Single;
begin
  { Normalise so that X1 < X2 and Y1 < Y2 }
  if X1 < X2 then begin LX := X1; RX := X2; end
  else begin LX := X2; RX := X1; end;
  if Y1 < Y2 then begin TY := Y1; BY := Y2; end
  else begin TY := Y2; BY := Y1; end;
  { Exclusive test: strictly inside the rectangle }
  Result := (X > LX) and (X < RX) and (Y > TY) and (Y < BY);
end;

end.
