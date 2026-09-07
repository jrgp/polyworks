unit test_map;

{$mode objfpc}{$H+}

interface

uses
  fpcunit, testregistry, SysUtils, Math,
  pw.types, pw.utils, pw.pms, pw.map;

type
  TMapTest = class(TTestCase)
  private
    FDoc: TMapDocument;
    function MakeVert(X, Y: Single; Color: LongWord = $FFFFFFFF): TEditorVertex;
    function MakeCWVerts(out V1, V2, V3: TEditorVertex): Boolean;
    function MakeCCWVerts(out V1, V2, V3: TEditorVertex): Boolean;
  protected
    procedure SetUp; override;
    procedure TearDown; override;
  published
    { NewMap }
    procedure TestNewMapEmptyArrays;
    procedure TestNewMapDefaultZoom;

    { LoadFromPMS / SaveToPMS }
    procedure TestLoadEmptyData;
    procedure TestLoadPolyData;
    procedure TestLoadSceneryAlphaZeroRemapped;
    procedure TestLoadSavePMSRoundTrip;

    { AddPoly }
    procedure TestAddPolyCWPreserved;
    procedure TestAddPolyCCWSwapped;
    procedure TestAddPolyCount;

    { RebuildScreenCache }
    procedure TestRebuildScreenCacheAtDefaultZoom;

    { SetZoom }
    procedure TestSetZoomKeepsCenterPoint;

    { MoveSelectedWorld }
    procedure TestMoveSelectedPoly;
    procedure TestMoveUnselectedPolyNotMoved;

    { DeleteSelected }
    procedure TestDeleteSelectedPoly;
    procedure TestDeletePartiallySelectedPolyNotDeleted;

    { AddScenery / AddSceneryName }
    procedure TestAddSceneryName;
    procedure TestAddSceneryIncreasesCount;

    { AddSpawn / AddCollider / AddWaypoint }
    procedure TestAddSpawn;
    procedure TestAddCollider;
    procedure TestAddWaypoint;

    { Waypoint connections }
    procedure TestAddConnection;
    procedure TestDeleteWaypointRemapsConnections;

    { AddLight }
    procedure TestAddLight;

    { SelectByWorldRect }
    procedure TestSelectByWorldRect;
    procedure TestClearSelection;

    { SnapSelectedToGrid }
    procedure TestSnapSelectedToGrid;

    { Integration: load real map }
    procedure TestLoadRealMap_Arena;
  end;

implementation

const
  EPS = 1e-4;

function TMapTest.MakeVert(X, Y: Single; Color: LongWord): TEditorVertex;
begin
  FillChar(Result, SizeOf(Result), 0);
  Result.World.X := X;
  Result.World.Y := Y;
  Result.Color := ARGBToColor3(Color);
  Result.Alpha := GetAlpha(Color);
end;

function TMapTest.MakeCWVerts(out V1, V2, V3: TEditorVertex): Boolean;
begin
  { CW triangle: (0,0), (100,0), (100,100) }
  V1 := MakeVert(0, 0);
  V2 := MakeVert(100, 0);
  V3 := MakeVert(100, 100);
  Result := True;
end;

function TMapTest.MakeCCWVerts(out V1, V2, V3: TEditorVertex): Boolean;
begin
  { CCW triangle: (0,0), (100,100), (100,0) }
  V1 := MakeVert(0, 0);
  V2 := MakeVert(100, 100);
  V3 := MakeVert(100, 0);
  Result := True;
end;

procedure TMapTest.SetUp;
begin
  FDoc := TMapDocument.Create;
end;

procedure TMapTest.TearDown;
begin
  FDoc.Free;
end;

{ ---- NewMap --------------------------------------------------------------- }

procedure TMapTest.TestNewMapEmptyArrays;
begin
  AssertEquals('PolyCount=0', 0, FDoc.PolyCount);
  AssertEquals('SceneryCount=0', 0, FDoc.SceneryCount);
  AssertEquals('SpawnCount=0', 0, FDoc.SpawnCount);
  AssertEquals('ColliderCount=0', 0, FDoc.ColliderCount);
  AssertEquals('WaypointCount=0', 0, FDoc.WaypointCount);
  AssertEquals('ConnCount=0', 0, FDoc.ConnCount);
  AssertEquals('LightCount=0', 0, FDoc.LightCount);
  AssertEquals('SketchCount=0', 0, FDoc.SketchCount);
  AssertFalse('Modified=False', FDoc.Modified);
end;

procedure TMapTest.TestNewMapDefaultZoom;
begin
  AssertEquals('Zoom=1.0', 1.0, FDoc.Zoom, EPS);
  AssertEquals('ScrollX=0', 0.0, FDoc.ScrollX, EPS);
  AssertEquals('ScrollY=0', 0.0, FDoc.ScrollY, EPS);
end;

{ ---- LoadFromPMS / SaveToPMS -------------------------------------------- }

procedure TMapTest.TestLoadEmptyData;
var
  Data: TPMSData;
begin
  FillChar(Data, SizeOf(Data), 0);
  Data.Version := 11;
  FDoc.LoadFromPMS(Data);
  AssertEquals('PolyCount=0', 0, FDoc.PolyCount);
  AssertEquals('SceneryCount=0', 0, FDoc.SceneryCount);
end;

procedure TMapTest.TestLoadPolyData;
var
  Data: TPMSData;
  Entry: TPMSPolyEntry;
begin
  FillChar(Data, SizeOf(Data), 0);
  Data.Version := 11;
  Data.PolyCount := 1;
  SetLength(Data.Polys, 1);
  FillChar(Entry, SizeOf(Entry), 0);
  Entry.Poly.V[1].X := 10.0;  Entry.Poly.V[1].Y := 20.0;
  Entry.Poly.V[2].X := 110.0; Entry.Poly.V[2].Y := 20.0;
  Entry.Poly.V[3].X := 110.0; Entry.Poly.V[3].Y := 120.0;
  Entry.Poly.V[1].Color := $FFFFFFFF;
  Entry.Poly.V[2].Color := $FFFFFFFF;
  Entry.Poly.V[3].Color := $FFFFFFFF;
  Entry.PolyType := 0;
  Data.Polys[0] := Entry;
  FDoc.LoadFromPMS(Data);
  AssertEquals('PolyCount=1', 1, FDoc.PolyCount);
  AssertEquals('V1.X=10', 10.0, FDoc.Polys[0].V[1].World.X, EPS);
  AssertEquals('V1.Y=20', 20.0, FDoc.Polys[0].V[1].World.Y, EPS);
end;

procedure TMapTest.TestLoadSceneryAlphaZeroRemapped;
var
  Data: TPMSData;
  Prop: TPMSProp;
  SN: TPMSSceneryName;
begin
  FillChar(Data, SizeOf(Data), 0);
  Data.Version := 11;
  Data.ScenNameCount := 1;
  SetLength(Data.ScenNames, 1);
  FillChar(SN, SizeOf(SN), 0);
  WritePascalStr('test.bmp', SN.Name, 50);
  Data.ScenNames[0] := SN;

  Data.PropCount := 1;
  SetLength(Data.Props, 1);
  FillChar(Prop, SizeOf(Prop), 0);
  Prop.Active := WordBool(-1);
  Prop.Style := 1;
  Prop.X := 100.0; Prop.Y := 100.0;
  Prop.Width := 64; Prop.Height := 64;
  Prop.ScaleX := 1.0; Prop.ScaleY := 1.0;
  Prop.Alpha := 0;  { 0 should become 255 }
  Data.Props[0] := Prop;

  FDoc.LoadFromPMS(Data);
  AssertEquals('SceneryCount=1', 1, FDoc.SceneryCount);
  AssertEquals('Alpha 0 → 255', 255, Integer(FDoc.Scenery[0].Alpha));
end;

procedure TMapTest.TestLoadSavePMSRoundTrip;
var
  Data, Data2: TPMSData;
  Err: string;
  Fn: string;
  Res: TPMSLoadResult;
begin
  FillChar(Data, SizeOf(Data), 0);
  Data.Version := 11;
  WritePascalStr('RoundTripMap', Data.Options.MapName, 38);
  WritePascalStr('grass.bmp', Data.Options.TextureName, 24);
  Data.Options.StartJet := 200;

  FDoc.LoadFromPMS(Data);
  FDoc.SaveToPMS(Data2);
  AssertEquals('PolyCount preserved', Data.PolyCount, Data2.PolyCount);
  AssertEquals('StartJet preserved',
    Integer(Data.Options.StartJet), Integer(Data2.Options.StartJet));

  Fn := GetTempDir + 'pw_maptest_roundtrip.pms';
  SavePMS(Fn, Data2, Err);
  Res := LoadPMS(Fn, Data2, Err);
  AssertEquals('Reload OK', Ord(plrOK), Ord(Res));
  AssertEquals('MapName preserved',
    ReadPascalStr(Data.Options.MapName),
    ReadPascalStr(Data2.Options.MapName));
  DeleteFile(Fn);
end;

{ ---- AddPoly -------------------------------------------------------------- }

procedure TMapTest.TestAddPolyCWPreserved;
var
  V1, V2, V3: TEditorVertex;
  Verts: array[0..2] of TEditorVertex;
begin
  MakeCWVerts(V1, V2, V3);
  Verts[0] := V1; Verts[1] := V2; Verts[2] := V3;
  FDoc.AddPoly(Verts, 0);
  AssertEquals('PolyCount=1', 1, FDoc.PolyCount);
  { CW vertices should NOT be swapped }
  AssertEquals('V1.X unchanged', 0.0, FDoc.Polys[0].V[1].World.X, EPS);
  AssertEquals('V2.X unchanged', 100.0, FDoc.Polys[0].V[2].World.X, EPS);
  AssertEquals('V3.X unchanged', 100.0, FDoc.Polys[0].V[3].World.X, EPS);
end;

procedure TMapTest.TestAddPolyCCWSwapped;
var
  V1, V2, V3: TEditorVertex;
  Verts: array[0..2] of TEditorVertex;
begin
  { CCW: (0,0), (100,100), (100,0) → after swap V2↔V3: (0,0), (100,0), (100,100) = CW }
  MakeCCWVerts(V1, V2, V3);
  Verts[0] := V1; Verts[1] := V2; Verts[2] := V3;
  FDoc.AddPoly(Verts, 0);
  AssertEquals('PolyCount=1', 1, FDoc.PolyCount);
  { After CW enforcement: V2 and V3 should be swapped compared to input }
  AssertEquals('V1 unchanged', 0.0, FDoc.Polys[0].V[1].World.X, EPS);
  { Original V2=(100,100) becomes V3; original V3=(100,0) becomes V2 }
  AssertEquals('V2.Y swapped to 0', 0.0, FDoc.Polys[0].V[2].World.Y, EPS);
  AssertEquals('V3.Y swapped to 100', 100.0, FDoc.Polys[0].V[3].World.Y, EPS);
end;

procedure TMapTest.TestAddPolyCount;
var
  V1, V2, V3: TEditorVertex;
  Verts: array[0..2] of TEditorVertex;
  I: Integer;
begin
  MakeCWVerts(V1, V2, V3);
  Verts[0] := V1; Verts[1] := V2; Verts[2] := V3;
  for I := 1 to 5 do
    FDoc.AddPoly(Verts, 0);
  AssertEquals('PolyCount=5', 5, FDoc.PolyCount);
end;

{ ---- RebuildScreenCache --------------------------------------------------- }

procedure TMapTest.TestRebuildScreenCacheAtDefaultZoom;
var
  V1, V2, V3: TEditorVertex;
  Verts: array[0..2] of TEditorVertex;
begin
  { At zoom=1, scroll=(0,0): ScreenX = WorldX, ScreenY = WorldY }
  MakeCWVerts(V1, V2, V3);
  Verts[0] := V1; Verts[1] := V2; Verts[2] := V3;
  FDoc.AddPoly(Verts, 0);
  AssertEquals('Screen.X = World.X at zoom=1', 0.0, FDoc.Polys[0].V[1].Screen.X, EPS);
  AssertEquals('Screen.Y = World.Y at zoom=1', 0.0, FDoc.Polys[0].V[1].Screen.Y, EPS);
  AssertEquals('V2 Screen.X', 100.0, FDoc.Polys[0].V[2].Screen.X, EPS);
end;

{ ---- SetZoom -------------------------------------------------------------- }

procedure TMapTest.TestSetZoomKeepsCenterPoint;
var
  V1, V2, V3: TEditorVertex;
  Verts: array[0..2] of TEditorVertex;
  WX1, WY1, WX2, WY2: Single;
begin
  { With zoom=1, center at screen (50,50) → world (50,50)
    After zoom to 2, center should still be world (50,50) }
  MakeCWVerts(V1, V2, V3);
  Verts[0] := V1; Verts[1] := V2; Verts[2] := V3;
  FDoc.AddPoly(Verts, 0);

  FDoc.ScreenToWorld(50, 50, WX1, WY1);
  FDoc.SetZoom(2.0, 50, 50);
  FDoc.ScreenToWorld(50, 50, WX2, WY2);

  AssertEquals('World X same after zoom', WX1, WX2, EPS);
  AssertEquals('World Y same after zoom', WY1, WY2, EPS);
end;

{ ---- MoveSelectedWorld ---------------------------------------------------- }

procedure TMapTest.TestMoveSelectedPoly;
var
  V1, V2, V3: TEditorVertex;
  Verts: array[0..2] of TEditorVertex;
begin
  MakeCWVerts(V1, V2, V3);
  Verts[0] := V1; Verts[1] := V2; Verts[2] := V3;
  FDoc.AddPoly(Verts, 0);
  { Select all vertices }
  FDoc.Polys[0].Selected[1] := True;
  FDoc.Polys[0].Selected[2] := True;
  FDoc.Polys[0].Selected[3] := True;
  FDoc.MoveSelectedWorld(10, 5);
  AssertEquals('V1.X moved', 10.0, FDoc.Polys[0].V[1].World.X, EPS);
  AssertEquals('V1.Y moved', 5.0,  FDoc.Polys[0].V[1].World.Y, EPS);
  AssertEquals('V2.X moved', 110.0, FDoc.Polys[0].V[2].World.X, EPS);
end;

procedure TMapTest.TestMoveUnselectedPolyNotMoved;
var
  V1, V2, V3: TEditorVertex;
  Verts: array[0..2] of TEditorVertex;
begin
  MakeCWVerts(V1, V2, V3);
  Verts[0] := V1; Verts[1] := V2; Verts[2] := V3;
  FDoc.AddPoly(Verts, 0);
  { No vertices selected }
  FDoc.MoveSelectedWorld(10, 5);
  AssertEquals('V1.X NOT moved', 0.0, FDoc.Polys[0].V[1].World.X, EPS);
end;

{ ---- DeleteSelected ------------------------------------------------------- }

procedure TMapTest.TestDeleteSelectedPoly;
var
  V1, V2, V3: TEditorVertex;
  Verts: array[0..2] of TEditorVertex;
begin
  MakeCWVerts(V1, V2, V3);
  Verts[0] := V1; Verts[1] := V2; Verts[2] := V3;
  FDoc.AddPoly(Verts, 0);
  AssertEquals('Before delete: PolyCount=1', 1, FDoc.PolyCount);
  FDoc.Polys[0].Selected[1] := True;
  FDoc.Polys[0].Selected[2] := True;
  FDoc.Polys[0].Selected[3] := True;
  FDoc.DeleteSelected;
  AssertEquals('After delete: PolyCount=0', 0, FDoc.PolyCount);
end;

procedure TMapTest.TestDeletePartiallySelectedPolyNotDeleted;
var
  V1, V2, V3: TEditorVertex;
  Verts: array[0..2] of TEditorVertex;
begin
  MakeCWVerts(V1, V2, V3);
  Verts[0] := V1; Verts[1] := V2; Verts[2] := V3;
  FDoc.AddPoly(Verts, 0);
  { Select only 2 of 3 vertices → poly should NOT be deleted }
  FDoc.Polys[0].Selected[1] := True;
  FDoc.Polys[0].Selected[2] := True;
  FDoc.DeleteSelected;
  AssertEquals('Partial select: PolyCount=1', 1, FDoc.PolyCount);
end;

{ ---- Scenery -------------------------------------------------------------- }

procedure TMapTest.TestAddSceneryName;
var
  Idx: Integer;
begin
  Idx := FDoc.AddSceneryName('grass.png');
  AssertEquals('First name index=1', 1, Idx);
  AssertEquals('ScenNameCount=1', 1, FDoc.ScenNameCount);
  AssertEquals('Name stored', 'grass.png', FDoc.SceneryNames[1]);
  { Adding same name returns same index }
  Idx := FDoc.AddSceneryName('grass.png');
  AssertEquals('Duplicate returns same idx', 1, Idx);
  AssertEquals('ScenNameCount still 1', 1, FDoc.ScenNameCount);
end;

procedure TMapTest.TestAddSceneryIncreasesCount;
var
  Idx: Integer;
begin
  Idx := FDoc.AddSceneryName('rock.bmp');
  FDoc.AddScenery(Idx, 50.0, 75.0, 1.0, 1.0, 0.0, 64, 64);
  AssertEquals('SceneryCount=1', 1, FDoc.SceneryCount);
  AssertEquals('X=50', 50.0, FDoc.Scenery[0].X, EPS);
  AssertEquals('Y=75', 75.0, FDoc.Scenery[0].Y, EPS);
end;

{ ---- Spawns / Colliders / Waypoints --------------------------------------- }

procedure TMapTest.TestAddSpawn;
begin
  FDoc.AddSpawn(100.0, 200.0, 1);
  AssertEquals('SpawnCount=1', 1, FDoc.SpawnCount);
  AssertEquals('X=100', 100.0, FDoc.Spawns[0].X, EPS);
  AssertEquals('Y=200', 200.0, FDoc.Spawns[0].Y, EPS);
  AssertEquals('Team=1', 1, Integer(FDoc.Spawns[0].Team));
end;

procedure TMapTest.TestAddCollider;
begin
  FDoc.AddCollider(50.0, 60.0, 25.0);
  AssertEquals('ColliderCount=1', 1, FDoc.ColliderCount);
  AssertEquals('Radius=25', 25.0, FDoc.Colliders[0].Radius, EPS);
end;

procedure TMapTest.TestAddWaypoint;
begin
  FDoc.AddWaypoint(10.0, 20.0);
  AssertEquals('WaypointCount=1', 1, FDoc.WaypointCount);
  AssertEquals('X=10', 10.0, FDoc.Waypoints[0].X, EPS);
  AssertEquals('Y=20', 20.0, FDoc.Waypoints[0].Y, EPS);
end;

procedure TMapTest.TestAddConnection;
begin
  FDoc.AddWaypoint(0.0, 0.0);
  FDoc.AddWaypoint(100.0, 0.0);
  FDoc.AddConnection(1, 2);
  AssertEquals('ConnCount=1', 1, FDoc.ConnCount);
  AssertEquals('Point1=1', 1, Integer(FDoc.Connections[0].Point1));
  AssertEquals('Point2=2', 2, Integer(FDoc.Connections[0].Point2));
end;

procedure TMapTest.TestDeleteWaypointRemapsConnections;
var
  V1, V2, V3: TEditorVertex;
  Verts: array[0..2] of TEditorVertex;
begin
  { Add 3 waypoints and connect 1→2, 2→3 }
  FDoc.AddWaypoint(0.0, 0.0);
  FDoc.AddWaypoint(100.0, 0.0);
  FDoc.AddWaypoint(200.0, 0.0);
  FDoc.AddConnection(1, 2);
  FDoc.AddConnection(2, 3);
  AssertEquals('Initial ConnCount=2', 2, FDoc.ConnCount);
  { Delete waypoint 1 (mark WP[0] as selected) }
  FDoc.Waypoints[0].Selected := True;
  FDoc.DeleteSelected;
  AssertEquals('WaypointCount=2', 2, FDoc.WaypointCount);
  { Connection 1→2 should be removed; 2→3 should become 1→2 }
  AssertEquals('ConnCount=1 after delete', 1, FDoc.ConnCount);
end;

{ ---- Lights --------------------------------------------------------------- }

procedure TMapTest.TestAddLight;
var
  LColor: TColor3;
begin
  LColor.R := 255; LColor.G := 200; LColor.B := 100;
  FDoc.AddLight(50.0, 60.0, 0.0, LColor, 0.8, 300);
  AssertEquals('LightCount=1', 1, FDoc.LightCount);
  AssertEquals('Light.X=50', 50.0, FDoc.Lights[0].X, EPS);
  AssertEquals('Light.Intensity=0.8', 0.8, FDoc.Lights[0].Intensity, EPS);
end;

{ ---- Selection ------------------------------------------------------------ }

procedure TMapTest.TestSelectByWorldRect;
var
  V1, V2, V3: TEditorVertex;
  Verts: array[0..2] of TEditorVertex;
begin
  MakeCWVerts(V1, V2, V3);
  Verts[0] := V1; Verts[1] := V2; Verts[2] := V3;
  FDoc.AddPoly(Verts, 0);
  { Select rect that covers the polygon vertices }
  FDoc.SelectByWorldRect(-10, -10, 200, 200, smReplace);
  AssertTrue('V1 selected', FDoc.Polys[0].Selected[1]);
  AssertTrue('V2 selected', FDoc.Polys[0].Selected[2]);
  AssertTrue('V3 selected', FDoc.Polys[0].Selected[3]);
end;

procedure TMapTest.TestClearSelection;
var
  V1, V2, V3: TEditorVertex;
  Verts: array[0..2] of TEditorVertex;
begin
  MakeCWVerts(V1, V2, V3);
  Verts[0] := V1; Verts[1] := V2; Verts[2] := V3;
  FDoc.AddPoly(Verts, 0);
  FDoc.Polys[0].Selected[1] := True;
  FDoc.ClearSelection;
  AssertFalse('V1 deselected', FDoc.Polys[0].Selected[1]);
end;

{ ---- SnapSelectedToGrid --------------------------------------------------- }

procedure TMapTest.TestSnapSelectedToGrid;
var
  V1, V2, V3: TEditorVertex;
  Verts: array[0..2] of TEditorVertex;
begin
  V1 := MakeVert(7.0, 13.0);
  V2 := MakeVert(107.0, 3.0);
  V3 := MakeVert(107.0, 103.0);
  Verts[0] := V1; Verts[1] := V2; Verts[2] := V3;
  FDoc.AddPoly(Verts, 0);
  FDoc.Polys[0].Selected[1] := True;
  FDoc.Polys[0].Selected[2] := True;
  FDoc.Polys[0].Selected[3] := True;
  FDoc.SnapSelectedToGrid(10.0);
  { Floor(7/10)*10 = 0, Floor(13/10)*10 = 10 }
  AssertEquals('V1.X snapped', 0.0, FDoc.Polys[0].V[1].World.X, EPS);
  AssertEquals('V1.Y snapped', 10.0, FDoc.Polys[0].V[1].World.Y, EPS);
end;

{ ---- Integration ---------------------------------------------------------- }

procedure TMapTest.TestLoadRealMap_Arena;
var
  Data: TPMSData;
  Err: string;
  MapPath: string;
begin
  MapPath := '../../maps/Arena.pms';
  if not FileExists(MapPath) then
    Exit;  { skip if not found from test runner context }

  if LoadPMS(MapPath, Data, Err) <> plrOK then
  begin
    Fail('Could not load Arena.pms: ' + Err);
    Exit;
  end;

  FDoc.LoadFromPMS(Data);
  AssertTrue('Arena poly count > 0', FDoc.PolyCount > 0);
  { Verify screen cache built: ScreenX should equal WorldX at zoom=1, scroll=0 }
  AssertEquals('ScreenX=WorldX at default zoom/scroll',
    FDoc.Polys[0].V[1].World.X,
    FDoc.Polys[0].V[1].Screen.X, EPS);
end;

initialization
  RegisterTest(TMapTest);

end.
