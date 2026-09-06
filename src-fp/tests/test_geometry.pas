unit test_geometry;

{$mode objfpc}{$H+}

interface

uses
  fpcunit, testregistry, SysUtils, Math,
  pw.types, pw.utils, pw.geometry;

type
  TGeometryTest = class(TTestCase)
  published
    { IsPolyClockwise }
    procedure TestCWTriangle;
    procedure TestCCWTriangle;
    procedure TestDegenerateTriangle;

    { PointInPoly }
    procedure TestPointInsideCW;
    procedure TestPointOutsideCW;
    procedure TestCentroidInsideCW;

    { EnsureClockwise }
    procedure TestEnsureClockwiseCW;
    procedure TestEnsureClockwiseCCW;

    { RecomputePolyNormals }
    procedure TestNormalsRightTriangle;
    procedure TestNormalsBouncyPreservesZ;
    procedure TestNormalsNonBouncySetsZ;

    { SegmentsIntersect }
    procedure TestSegmentsCrossing;
    procedure TestSegmentsParallel;
    procedure TestSegmentsNonIntersecting;
    procedure TestSegmentsTouching;

    { SnapToGrid }
    procedure TestSnapToGridPositive;
    procedure TestSnapToGridNegative;
    procedure TestSnapToGridExact;

    { InSelRect }
    procedure TestInSelRectInside;
    procedure TestInSelRectOnEdge;
    procedure TestInSelRectOutside;
    procedure TestInSelRectReversed;

    { NearCoord }
    procedure TestNearCoord;

    { WorldToScreen / ScreenToWorld }
    procedure TestWorldToScreenRoundTrip;
    procedure TestWorldToScreenZoom;

    { ComputePolyBounds }
    procedure TestPolyBoundsEmpty;
    procedure TestPolyBoundsSingle;
  end;

function MakeV2(X, Y: Single): TVector2;

implementation

function MakeV2(X, Y: Single): TVector2;
begin
  Result.X := X;
  Result.Y := Y;
end;

{ ---- IsPolyClockwise --------------------------------------------------- }

procedure TGeometryTest.TestCWTriangle;
begin
  { CW triangle in screen space (Y increases downward): }
  { V1=(0,0), V2=(100,0), V3=(50,100) — standard CW }
  AssertTrue('CW triangle is CW',
    IsPolyClockwise(MakeV2(0, 0), MakeV2(100, 0), MakeV2(50, 100)));
end;

procedure TGeometryTest.TestCCWTriangle;
begin
  { CCW triangle: same verts in CCW order }
  AssertFalse('CCW triangle is not CW',
    IsPolyClockwise(MakeV2(0, 0), MakeV2(50, 100), MakeV2(100, 0)));
end;

procedure TGeometryTest.TestDegenerateTriangle;
begin
  { Degenerate (zero-area) triangle — should not crash }
  try
    IsPolyClockwise(MakeV2(0, 0), MakeV2(0, 0), MakeV2(0, 0));
  except
    Fail('Degenerate triangle should not raise exception');
  end;
end;

{ ---- PointInPoly -------------------------------------------------------- }

procedure TGeometryTest.TestPointInsideCW;
var
  V1, V2, V3: TVector2;
begin
  V1 := MakeV2(0, 0); V2 := MakeV2(100, 0); V3 := MakeV2(50, 100);
  AssertTrue('Centroid inside CW triangle',
    PointInPoly(50, 30, V1, V2, V3));
end;

procedure TGeometryTest.TestPointOutsideCW;
var
  V1, V2, V3: TVector2;
begin
  V1 := MakeV2(0, 0); V2 := MakeV2(100, 0); V3 := MakeV2(50, 100);
  AssertFalse('Point far outside CW triangle',
    PointInPoly(-100, -100, V1, V2, V3));
end;

procedure TGeometryTest.TestCentroidInsideCW;
var
  V1, V2, V3: TVector2;
  CX, CY: Single;
begin
  V1 := MakeV2(0, 0); V2 := MakeV2(200, 0); V3 := MakeV2(100, 200);
  CX := (V1.X + V2.X + V3.X) / 3;
  CY := (V1.Y + V2.Y + V3.Y) / 3;
  AssertTrue('True centroid inside CW triangle', PointInPoly(CX, CY, V1, V2, V3));
end;

{ ---- EnsureClockwise --------------------------------------------------- }

procedure TGeometryTest.TestEnsureClockwiseCW;
var
  P: TEditorPoly;
begin
  FillChar(P, SizeOf(P), 0);
  P.V[1].World := MakeV2(0, 0);
  P.V[2].World := MakeV2(100, 0);
  P.V[3].World := MakeV2(50, 100);
  EnsureClockwise(P);
  { CW triangle should not be modified }
  AssertEquals('V2.X unchanged', 100.0, P.V[2].World.X);
  AssertEquals('V3.X unchanged', 50.0, P.V[3].World.X);
end;

procedure TGeometryTest.TestEnsureClockwiseCCW;
var
  P: TEditorPoly;
begin
  FillChar(P, SizeOf(P), 0);
  P.V[1].World := MakeV2(0, 0);
  P.V[2].World := MakeV2(50, 100);   { CCW order }
  P.V[3].World := MakeV2(100, 0);
  EnsureClockwise(P);
  { After fix, V2 and V3 should be swapped }
  AssertEquals('V2.X after CW enforce', 100.0, P.V[2].World.X);
  AssertEquals('V3.X after CW enforce', 50.0, P.V[3].World.X);
  { Verify result is actually CW }
  AssertTrue('Result is CW after enforce',
    IsPolyClockwise(P.V[1].World, P.V[2].World, P.V[3].World));
end;

{ ---- RecomputePolyNormals ----------------------------------------------- }

procedure TGeometryTest.TestNormalsRightTriangle;
var
  E: TPMSPolyEntry;
begin
  FillChar(E, SizeOf(E), 0);
  { Horizontal edge from (0,0) to (100,0): normal should be (0,1) or (-1,0)
    depending on convention. }
  E.Poly.V[1].X := 0;   E.Poly.V[1].Y := 0;
  E.Poly.V[2].X := 100; E.Poly.V[2].Y := 0;
  E.Poly.V[3].X := 50;  E.Poly.V[3].Y := 100;
  E.PolyType := POLY_TYPE_NORMAL;
  E.Poly.Perp.N[1].Z := 1.0;
  E.Poly.Perp.N[2].Z := 1.0;
  E.Poly.Perp.N[3].Z := 1.0;
  RecomputePolyNormals(E);
  { Z must be 1.0 for non-bouncy }
  AssertEquals('Z=1 for normal poly edge 1', 1.0, E.Poly.Perp.N[1].Z);
  { Magnitude of (X,Y) should be 1 (normalized) }
  AssertTrue('Normal is unit length',
    Abs(Sqrt(E.Poly.Perp.N[1].X*E.Poly.Perp.N[1].X +
             E.Poly.Perp.N[1].Y*E.Poly.Perp.N[1].Y) - 1.0) < 0.001);
end;

procedure TGeometryTest.TestNormalsBouncyPreservesZ;
var
  E: TPMSPolyEntry;
begin
  FillChar(E, SizeOf(E), 0);
  E.Poly.V[1].X := 0;   E.Poly.V[1].Y := 0;
  E.Poly.V[2].X := 100; E.Poly.V[2].Y := 0;
  E.Poly.V[3].X := 50;  E.Poly.V[3].Y := 100;
  E.PolyType := POLY_TYPE_BOUNCY;
  { Set bounce value > 1 for edge 1 }
  E.Poly.Perp.N[1].Z := 2.5;
  E.Poly.Perp.N[2].Z := 1.0;
  E.Poly.Perp.N[3].Z := 1.0;
  RecomputePolyNormals(E);
  { Bouncy polygons preserve Z >= 1 }
  AssertEquals('Bouncy Z preserved on edge 1', 1.0, E.Poly.Perp.N[1].Z);
  { Normal components should be scaled by bounce }
  AssertTrue('Bouncy normal magnitude = bounce',
    Abs(Sqrt(E.Poly.Perp.N[1].X*E.Poly.Perp.N[1].X +
             E.Poly.Perp.N[1].Y*E.Poly.Perp.N[1].Y) - 2.5) < 0.01);
end;

procedure TGeometryTest.TestNormalsNonBouncySetsZ;
var
  E: TPMSPolyEntry;
begin
  FillChar(E, SizeOf(E), 0);
  E.Poly.V[1].X := 0;   E.Poly.V[1].Y := 0;
  E.Poly.V[2].X := 100; E.Poly.V[2].Y := 0;
  E.Poly.V[3].X := 50;  E.Poly.V[3].Y := 100;
  E.PolyType := POLY_TYPE_ICE;
  E.Poly.Perp.N[1].Z := 99;  { should be overwritten }
  RecomputePolyNormals(E);
  AssertEquals('Non-bouncy Z forced to 1', 1.0, E.Poly.Perp.N[1].Z);
end;

{ ---- SegmentsIntersect ------------------------------------------------- }

procedure TGeometryTest.TestSegmentsCrossing;
begin
  AssertTrue('Crossing X-segments',
    SegmentsIntersect(0, 0, 10, 10, 0, 10, 10, 0));
end;

procedure TGeometryTest.TestSegmentsParallel;
begin
  AssertFalse('Parallel segments',
    SegmentsIntersect(0, 0, 10, 0, 0, 5, 10, 5));
end;

procedure TGeometryTest.TestSegmentsNonIntersecting;
begin
  AssertFalse('Non-intersecting segments',
    SegmentsIntersect(0, 0, 5, 0, 10, 0, 20, 0));
end;

procedure TGeometryTest.TestSegmentsTouching;
begin
  { T-intersection: endpoint of second on first }
  AssertTrue('T-intersection',
    SegmentsIntersect(0, 0, 10, 0, 5, 0, 5, 10));
end;

{ ---- SnapToGrid --------------------------------------------------------- }

procedure TGeometryTest.TestSnapToGridPositive;
begin
  { Grid size 10, coord 14 → snap to 10 (floor toward -inf) }
  AssertEquals('Snap 14 to grid 10', 10.0, SnapToGrid(14.0, 10.0));
  AssertEquals('Snap 16 to grid 10', 10.0, SnapToGrid(16.0, 10.0));
  AssertEquals('Snap 20 to grid 10', 20.0, SnapToGrid(20.0, 10.0));
end;

procedure TGeometryTest.TestSnapToGridNegative;
begin
  { VB6 Int() floors toward -∞: Int(-0.5) = -1, not 0 }
  AssertEquals('Snap -1 to grid 10', -10.0, SnapToGrid(-1.0, 10.0));
  AssertEquals('Snap -9 to grid 10', -10.0, SnapToGrid(-9.0, 10.0));
  AssertEquals('Snap -10 to grid 10', -10.0, SnapToGrid(-10.0, 10.0));
  AssertEquals('Snap -11 to grid 10', -20.0, SnapToGrid(-11.0, 10.0));
end;

procedure TGeometryTest.TestSnapToGridExact;
begin
  AssertEquals('Snap exact multiple', 30.0, SnapToGrid(30.0, 10.0));
  AssertEquals('Snap zero', 0.0, SnapToGrid(0.0, 10.0));
end;

{ ---- InSelRect ---------------------------------------------------------- }

procedure TGeometryTest.TestInSelRectInside;
begin
  AssertTrue('Point strictly inside rect',
    InSelRect(50, 50, 0, 0, 100, 100));
end;

procedure TGeometryTest.TestInSelRectOnEdge;
begin
  { Exclusive: points on the edge are NOT selected }
  AssertFalse('Point on left edge not selected',
    InSelRect(0, 50, 0, 0, 100, 100));
  AssertFalse('Point on top edge not selected',
    InSelRect(50, 0, 0, 0, 100, 100));
  AssertFalse('Point on right edge not selected',
    InSelRect(100, 50, 0, 0, 100, 100));
  AssertFalse('Point on bottom edge not selected',
    InSelRect(50, 100, 0, 0, 100, 100));
end;

procedure TGeometryTest.TestInSelRectOutside;
begin
  AssertFalse('Point outside rect', InSelRect(150, 50, 0, 0, 100, 100));
end;

procedure TGeometryTest.TestInSelRectReversed;
begin
  { Rect specified with inverted corners — should still work }
  AssertTrue('Reversed rect inside', InSelRect(50, 50, 100, 100, 0, 0));
end;

{ ---- NearCoord ---------------------------------------------------------- }

procedure TGeometryTest.TestNearCoord;
begin
  AssertTrue('Same coord', NearCoord(5.0, 5.0, 8.0));
  AssertTrue('Within range', NearCoord(5.0, 12.0, 8.0));
  AssertTrue('At boundary', NearCoord(5.0, 13.0, 8.0));
  AssertFalse('Just outside range', NearCoord(5.0, 13.1, 8.0));
end;

{ ---- WorldToScreen / ScreenToWorld -------------------------------------- }

procedure TGeometryTest.TestWorldToScreenRoundTrip;
var
  SX, SY, WX, WY: Single;
begin
  WorldToScreen(100, 200, 50, 75, 2.0, SX, SY);
  ScreenToWorld(SX, SY, 50, 75, 2.0, WX, WY);
  AssertTrue('WX round-trip', Abs(WX - 100) < 0.001);
  AssertTrue('WY round-trip', Abs(WY - 200) < 0.001);
end;

procedure TGeometryTest.TestWorldToScreenZoom;
var
  SX, SY: Single;
begin
  { At zoom=2, scroll=0: screen = world * 2 }
  WorldToScreen(50, 75, 0, 0, 2.0, SX, SY);
  AssertEquals('SX at zoom 2', 100.0, SX);
  AssertEquals('SY at zoom 2', 150.0, SY);
end;

{ ---- ComputePolyBounds -------------------------------------------------- }

procedure TGeometryTest.TestPolyBoundsEmpty;
var
  Polys: TPMSPolyArray;
  MinX, MinY, MaxX, MaxY: Single;
begin
  SetLength(Polys, 0);
  ComputePolyBounds(Polys, 0, MinX, MinY, MaxX, MaxY);
  AssertEquals('Empty bounds MinX', 0.0, MinX);
  AssertEquals('Empty bounds MaxX', 0.0, MaxX);
end;

procedure TGeometryTest.TestPolyBoundsSingle;
var
  Polys: TPMSPolyArray;
  MinX, MinY, MaxX, MaxY: Single;
begin
  SetLength(Polys, 1);
  FillChar(Polys[0], SizeOf(Polys[0]), 0);
  Polys[0].Poly.V[1].X := -10; Polys[0].Poly.V[1].Y := -20;
  Polys[0].Poly.V[2].X :=  50; Polys[0].Poly.V[2].Y :=   0;
  Polys[0].Poly.V[3].X :=   0; Polys[0].Poly.V[3].Y :=  30;
  ComputePolyBounds(Polys, 1, MinX, MinY, MaxX, MaxY);
  AssertEquals('MinX', -10.0, MinX);
  AssertEquals('MaxX', 50.0, MaxX);
  AssertEquals('MinY', -20.0, MinY);
  AssertEquals('MaxY', 30.0, MaxY);
end;

initialization
  RegisterTest(TGeometryTest);

end.
