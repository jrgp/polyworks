unit test_prefab;

{$mode objfpc}{$H+}

interface

uses
  fpcunit, testregistry, SysUtils,
  pw.types, pw.utils, pw.map, pw.prefab;

type
  TPrefabTest = class(TTestCase)
  private
    FTempDir: string;
    function TempFile(const Name: string): string;
    function MakeVert(X, Y: Single; Color: LongWord = $FFFFFFFF): TEditorVertex;
    procedure AddSelectedPoly(Doc: TMapDocument; X1, Y1, X2, Y2, X3, Y3: Single);
  protected
    procedure SetUp; override;
    procedure TearDown; override;
  published
    procedure TestSaveLoadRoundTrip;
    procedure TestLoadAppendsNotReplaces;
    procedure TestSpawnRoundTrip;
    procedure TestWaypointConnectionRemap;
    procedure TestLightsNotInPrefab;
  end;

implementation

const
  EPS = 1e-4;

function TPrefabTest.TempFile(const Name: string): string;
begin
  Result := IncludeTrailingPathDelimiter(FTempDir) + Name;
end;

function TPrefabTest.MakeVert(X, Y: Single; Color: LongWord): TEditorVertex;
begin
  FillChar(Result, SizeOf(Result), 0);
  Result.World.X := X;
  Result.World.Y := Y;
  Result.Color := ARGBToColor3(Color);
  Result.Alpha := GetAlpha(Color);
end;

procedure TPrefabTest.AddSelectedPoly(Doc: TMapDocument;
  X1, Y1, X2, Y2, X3, Y3: Single);
var
  Verts: array[0..2] of TEditorVertex;
  Idx: Integer;
begin
  Verts[0] := MakeVert(X1, Y1);
  Verts[1] := MakeVert(X2, Y2);
  Verts[2] := MakeVert(X3, Y3);
  Doc.AddPoly(Verts, POLY_TYPE_NORMAL);
  Idx := Doc.PolyCount - 1;
  Doc.Polys[Idx].Selected[1] := True;
  Doc.Polys[Idx].Selected[2] := True;
  Doc.Polys[Idx].Selected[3] := True;
end;

procedure TPrefabTest.SetUp;
begin
  FTempDir := IncludeTrailingPathDelimiter(ExtractFilePath(ParamStr(0))) +
    'pw_prefab_test_' + IntToStr(GetProcessID);
  ForceDirectories(FTempDir);
end;

procedure TPrefabTest.TearDown;
var
  SR: TSearchRec;
begin
  if FindFirst(IncludeTrailingPathDelimiter(FTempDir) + '*', faAnyFile, SR) = 0 then
  begin
    repeat
      if (SR.Name <> '.') and (SR.Name <> '..') then
        DeleteFile(IncludeTrailingPathDelimiter(FTempDir) + SR.Name);
    until FindNext(SR) <> 0;
    FindClose(SR);
  end;
  if DirectoryExists(FTempDir) then
    RmDir(FTempDir);
end;

procedure TPrefabTest.TestSaveLoadRoundTrip;
var
  Src, Dst: TMapDocument;
  Err, Fn: string;
begin
  Src := TMapDocument.Create;
  Dst := TMapDocument.Create;
  try
    AddSelectedPoly(Src, 0, 0, 100, 0, 100, 100);
    AddSelectedPoly(Src, 200, 0, 300, 0, 300, 100);

    Fn := TempFile('roundtrip.pfb');
    AssertTrue('SavePrefab succeeds', SavePrefab(Fn, Src, Err));
    AssertTrue('LoadPrefab succeeds', LoadPrefab(Fn, Dst, Err));
    AssertEquals('PolyCount=2', 2, Dst.PolyCount);
  finally
    Src.Free;
    Dst.Free;
  end;
end;

procedure TPrefabTest.TestLoadAppendsNotReplaces;
var
  Src, Dst: TMapDocument;
  Err, Fn: string;
begin
  Src := TMapDocument.Create;
  Dst := TMapDocument.Create;
  try
    AddSelectedPoly(Src, 0, 0, 100, 0, 100, 100);
    AddSelectedPoly(Src, 200, 0, 300, 0, 300, 100);
    AddSelectedPoly(Dst, 400, 0, 500, 0, 500, 100);
    Dst.ClearSelection;

    Fn := TempFile('append.pfb');
    AssertTrue('SavePrefab succeeds', SavePrefab(Fn, Src, Err));
    AssertTrue('LoadPrefab succeeds', LoadPrefab(Fn, Dst, Err));
    AssertEquals('PolyCount=3', 3, Dst.PolyCount);
  finally
    Src.Free;
    Dst.Free;
  end;
end;

procedure TPrefabTest.TestSpawnRoundTrip;
var
  Src, Dst: TMapDocument;
  Err, Fn: string;
begin
  Src := TMapDocument.Create;
  Dst := TMapDocument.Create;
  try
    Src.AddSpawn(123, 456, 3);
    Src.Spawns[0].Selected := True;

    Fn := TempFile('spawn.pfb');
    AssertTrue('SavePrefab succeeds', SavePrefab(Fn, Src, Err));
    AssertTrue('LoadPrefab succeeds', LoadPrefab(Fn, Dst, Err));
    AssertEquals('SpawnCount=1', 1, Dst.SpawnCount);
    AssertEquals('Spawn X', 123.0, Dst.Spawns[0].X, EPS);
    AssertEquals('Spawn Y', 456.0, Dst.Spawns[0].Y, EPS);
  finally
    Src.Free;
    Dst.Free;
  end;
end;

procedure TPrefabTest.TestWaypointConnectionRemap;
var
  Src, Dst: TMapDocument;
  Err, Fn: string;
begin
  Src := TMapDocument.Create;
  Dst := TMapDocument.Create;
  try
    Src.AddWaypoint(10, 20);
    Src.AddWaypoint(30, 40);
    Src.Waypoints[0].Selected := True;
    Src.Waypoints[1].Selected := True;
    Src.AddConnection(1, 2);

    Dst.AddWaypoint(100, 200);

    Fn := TempFile('waypoints.pfb');
    AssertTrue('SavePrefab succeeds', SavePrefab(Fn, Src, Err));
    AssertTrue('LoadPrefab succeeds', LoadPrefab(Fn, Dst, Err));
    AssertEquals('WaypointCount=3', 3, Dst.WaypointCount);
    AssertEquals('ConnCount=1', 1, Dst.ConnCount);
    AssertEquals('Connection point1 remapped', 2, Dst.Connections[0].Point1);
    AssertEquals('Connection point2 remapped', 3, Dst.Connections[0].Point2);
    AssertTrue('Connection point1 valid',
      (Dst.Connections[0].Point1 >= 1) and (Dst.Connections[0].Point1 <= Dst.WaypointCount));
    AssertTrue('Connection point2 valid',
      (Dst.Connections[0].Point2 >= 1) and (Dst.Connections[0].Point2 <= Dst.WaypointCount));
    AssertEquals('Loaded source waypoint has one outgoing connection',
      1, Dst.Waypoints[1].NumConns);
  finally
    Src.Free;
    Dst.Free;
  end;
end;

procedure TPrefabTest.TestLightsNotInPrefab;
var
  Src, Dst: TMapDocument;
  Err, Fn: string;
  C: TColor3;
begin
  Src := TMapDocument.Create;
  Dst := TMapDocument.Create;
  try
    C.R := 255;
    C.G := 200;
    C.B := 100;
    Src.AddLight(10, 20, 5, C, 1.0, 64);
    Src.Lights[0].Selected := True;

    Fn := TempFile('nolights.pfb');
    AssertTrue('SavePrefab succeeds', SavePrefab(Fn, Src, Err));
    AssertTrue('LoadPrefab succeeds', LoadPrefab(Fn, Dst, Err));
    AssertEquals('LightCount=0', 0, Dst.LightCount);
  finally
    Src.Free;
    Dst.Free;
  end;
end;

initialization
  RegisterTest(TPrefabTest);

end.
