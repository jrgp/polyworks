unit pw.pms;

{$mode objfpc}{$H+}
{$PackRecords 1}

{ ---------------------------------------------------------------------------
  pw.pms — binary PMS map file load / save / compile.

  File format (all little-endian, VB6 native binary):

  Offset  Size  Description
  ------  ----  -----------
  0       4     Version (LongInt) = 11
  4       84    TOptions (TPMSOptions)
  88      4     PolyCount (LongInt)
  92      121×N Polygon entries (TPMSPolyEntry each)
  92+121N 4     SectorsDivision (LongInt)
  96+121N 4     SECTOR_NUM (LongInt) = 25 (written to file but ignored on load)
  100+..  var   Sector table: 51×51 cells; each cell = SmallInt count +
                  count×SmallInt poly-indices (1-based).
                  In PolyWorks-native (MapRandomID=-1): all zeros (51×51×2 = 5202 bytes).
  after   4     SceneryCount (LongInt)
  ..      44×S  TPMSProp entries
  ..      4     SceneryNamesCount (LongInt)
  ..      55×E  TPMSSceneryName entries
  ..      4     ColliderCount (LongInt)
  ..      16×C  TPMSCollider entries
  ..      4     SpawnCount (LongInt)
  ..      16×Sp TSaveSpawnPoint entries
  ..      4     WaypointCount (LongInt)
  ..      112×W TPMSWaypoint entries

  If MapRandomID < 0 (PolyWorks native):
  ..      2     LightCount (SmallInt)
  ..      22×L  TPMSLight entries
  ..      2     SketchCount (SmallInt)
  ..      24×K  TPMSSketchLine entries

  Else (compiled game format):
  ..      8     Four SmallInt zeros (trailing padding written by SaveAndCompile)
  --------------------------------------------------------------------------- }

interface

uses
  SysUtils, Classes,
  pw.types, pw.utils;

type
  TPMSLoadResult = (plrOK, plrFileNotFound, plrVersionMismatch,
                    plrTruncated, plrCorrupt);

  { Complete parsed PMS file contents. }
  TPMSData = record
    Version      : LongInt;
    Options      : TPMSOptions;
    Polys        : TPMSPolyArray;
    PolyCount    : LongInt;
    SectorDiv    : LongInt;
    Sectors      : TSectorTable;  { loaded sector table (may be all zeros) }
    Props        : TPMSPropArray;
    PropCount    : LongInt;
    ScenNames    : TPMSScenNameArray;
    ScenNameCount: LongInt;
    Colliders    : TPMSColliderArray;
    ColliderCount: LongInt;
    Spawns       : TPMSSpawnArray;
    SpawnCount   : LongInt;
    Waypoints    : TPMSWaypointArray;
    WaypointCount: LongInt;
    Lights       : TPMSLightArray;
    LightCount   : LongInt;
    Sketch       : TPMSSketchArray;
    SketchCount  : LongInt;
  end;

{ Load a PMS file from disk.
  Returns plrOK on success.  Err is set on failure. }
function LoadPMS(const Filename: string; out Data: TPMSData;
                 out Err: string): TPMSLoadResult;

{ Save in PolyWorks native format (MapRandomID=-1, raw world coords,
  zero sector table, lights and sketch lines preserved). }
function SavePMS(const Filename: string; const Data: TPMSData;
                 out Err: string): Boolean;

{ Save in compiled game format (MapRandomID assigned, coordinates centred,
  sector table computed, lights/sketch stripped). }
function CompilePMS(const Filename: string; const Data: TPMSData;
                    out Err: string): Boolean;

implementation

uses
  pw.geometry;

{ ---- Stream helpers ---------------------------------------------------- }

procedure StreamReadBytes(S: TStream; var Buf; Count: Integer);
begin
  if S.Read(Buf, Count) <> Count then
    raise EStreamError.Create('Unexpected end of file');
end;

procedure StreamReadLong(S: TStream; out V: LongInt);
begin
  StreamReadBytes(S, V, SizeOf(LongInt));
end;

procedure StreamReadSmall(S: TStream; out V: SmallInt);
begin
  StreamReadBytes(S, V, SizeOf(SmallInt));
end;

procedure StreamWriteBytes(S: TStream; const Buf; Count: Integer);
begin
  S.Write(Buf, Count);
end;

procedure StreamWriteLong(S: TStream; V: LongInt);
begin
  S.Write(V, SizeOf(LongInt));
end;

procedure StreamWriteSmall(S: TStream; V: SmallInt);
begin
  S.Write(V, SizeOf(SmallInt));
end;

{ ---- LoadPMS ----------------------------------------------------------- }

function LoadPMS(const Filename: string; out Data: TPMSData;
                 out Err: string): TPMSLoadResult;
var
  S: TFileStream;
  I, J, K: Integer;
  PolyCount, ScenCount, ElemCount, ColCount, SpawnCnt, WPCount: LongInt;
  LightCnt, SketchCnt: SmallInt;
  SectorNumFile: LongInt;
  PolysInSector: SmallInt;
  PolyIdx: SmallInt;
  Poly: TPMSPolyEntry;
  Prop: TPMSProp;
  ScenName: TPMSSceneryName;
  Col: TPMSCollider;
  Spawn: TPMSSpawnPoint;
  WP: TPMSWaypoint;
  Lt: TPMSLight;
  Sk: TPMSSketchLine;
begin
  Result := plrOK;
  Err := '';
  FillChar(Data, SizeOf(Data), 0);

  if not FileExists(Filename) then
  begin
    Result := plrFileNotFound;
    Err := 'File not found: ' + Filename;
    Exit;
  end;

  try
    S := TFileStream.Create(Filename, fmOpenRead or fmShareDenyNone);
    try
      { Version }
      StreamReadLong(S, Data.Version);
      if Data.Version <> PMS_VERSION then
      begin
        Result := plrVersionMismatch;
        Err := Format('Unsupported PMS version: %d (expected %d)',
                      [Data.Version, PMS_VERSION]);
        Exit;
      end;

      { Options }
      StreamReadBytes(S, Data.Options, SizeOf(TPMSOptions));

      { Polygons }
      StreamReadLong(S, PolyCount);
      Data.PolyCount := PolyCount;
      SetLength(Data.Polys, PolyCount);
      for I := 0 to PolyCount - 1 do
      begin
        StreamReadBytes(S, Data.Polys[I], SizeOf(TPMSPolyEntry));
        { Recompute Z component of Perp (stores length, used for bounce).
          VB6 does this on load: Perp.vertex(j).Z = Sqrt(X^2 + Y^2).
          We store whatever is in the file and recompute later. }
      end;

      { Sector table header }
      StreamReadLong(S, Data.SectorDiv);
      StreamReadLong(S, SectorNumFile);  { always 25, written but ignored }

      { Sector table: 51×51 cells, variable length }
      FillChar(Data.Sectors, SizeOf(TSectorTable), 0);
      for I := 0 to SECTOR_CELLS - 1 do
        for J := 0 to SECTOR_CELLS - 1 do
        begin
          StreamReadSmall(S, PolysInSector);
          if PolysInSector < 0 then PolysInSector := 0;
          if PolysInSector > 256 then PolysInSector := 256;
          Data.Sectors[I][J].PolyCount := PolysInSector;
          SetLength(Data.Sectors[I][J].PolyIndex, PolysInSector);
          for K := 0 to PolysInSector - 1 do
          begin
            StreamReadSmall(S, PolyIdx);
            Data.Sectors[I][J].PolyIndex[K] := PolyIdx;
          end;
        end;

      { Scenery props }
      StreamReadLong(S, ScenCount);
      Data.PropCount := ScenCount;
      SetLength(Data.Props, ScenCount);
      for I := 0 to ScenCount - 1 do
        StreamReadBytes(S, Data.Props[I], SizeOf(TPMSProp));

      { Scenery texture names }
      StreamReadLong(S, ElemCount);
      Data.ScenNameCount := ElemCount;
      SetLength(Data.ScenNames, ElemCount);
      for I := 0 to ElemCount - 1 do
        StreamReadBytes(S, Data.ScenNames[I], SizeOf(TPMSSceneryName));

      { Colliders }
      StreamReadLong(S, ColCount);
      Data.ColliderCount := ColCount;
      SetLength(Data.Colliders, ColCount);
      for I := 0 to ColCount - 1 do
        StreamReadBytes(S, Data.Colliders[I], SizeOf(TPMSCollider));

      { Spawn points }
      StreamReadLong(S, SpawnCnt);
      Data.SpawnCount := SpawnCnt;
      SetLength(Data.Spawns, SpawnCnt);
      for I := 0 to SpawnCnt - 1 do
        StreamReadBytes(S, Data.Spawns[I], SizeOf(TPMSSpawnPoint));

      { Waypoints }
      StreamReadLong(S, WPCount);
      Data.WaypointCount := WPCount;
      SetLength(Data.Waypoints, WPCount);
      for I := 0 to WPCount - 1 do
        StreamReadBytes(S, Data.Waypoints[I], SizeOf(TPMSWaypoint));

      { PolyWorks extension: lights + sketch (only if MapRandomID < 0) }
      if Data.Options.MapRandomID < 0 then
      begin
        StreamReadSmall(S, LightCnt);
        Data.LightCount := LightCnt;
        SetLength(Data.Lights, LightCnt);
        for I := 0 to LightCnt - 1 do
          StreamReadBytes(S, Data.Lights[I], SizeOf(TPMSLight));

        StreamReadSmall(S, SketchCnt);
        Data.SketchCount := SketchCnt;
        SetLength(Data.Sketch, SketchCnt);
        for I := 0 to SketchCnt - 1 do
          StreamReadBytes(S, Data.Sketch[I], SizeOf(TPMSSketchLine));
      end;
      { else: trailing bytes (8 zero bytes from SaveAndCompile) are ignored }

    finally
      S.Free;
    end;
  except
    on E: EStreamError do
    begin
      Result := plrTruncated;
      Err := 'File truncated or corrupt: ' + E.Message;
    end;
    on E: Exception do
    begin
      Result := plrCorrupt;
      Err := 'Error loading PMS: ' + E.Message;
    end;
  end;
end;

{ ---- SavePMS (PolyWorks native format) --------------------------------- }

function SavePMS(const Filename: string; const Data: TPMSData;
                 out Err: string): Boolean;
var
  S: TFileStream;
  Opt: TPMSOptions;
  I, J: Integer;
  SectorDiv: LongInt;
  SectorNumConst: LongInt;
  ZeroSmall: SmallInt;
  LightCnt, SketchCnt: SmallInt;
begin
  Result := False;
  Err := '';
  try
    S := TFileStream.Create(Filename, fmCreate);
    try
      { Version }
      StreamWriteLong(S, PMS_VERSION);

      { Options: force MapRandomID = -1 for PolyWorks native }
      Opt := Data.Options;
      Opt.MapRandomID := -1;
      StreamWriteBytes(S, Opt, SizeOf(TPMSOptions));

      { Polygon count + polygons }
      StreamWriteLong(S, Data.PolyCount);
      for I := 0 to Data.PolyCount - 1 do
        StreamWriteBytes(S, Data.Polys[I], SizeOf(TPMSPolyEntry));

      { Sector table header }
      SectorDiv := Data.SectorDiv;
      if SectorDiv = 0 then SectorDiv := 1;  { avoid div-by-zero in game }
      SectorNumConst := SECTOR_NUM;
      StreamWriteLong(S, SectorDiv);
      StreamWriteLong(S, SectorNumConst);

      { Zero sector table: 51×51 × SmallInt(0) = 5202 bytes }
      ZeroSmall := 0;
      for I := 0 to SECTOR_CELLS - 1 do
        for J := 0 to SECTOR_CELLS - 1 do
          StreamWriteSmall(S, ZeroSmall);

      { Scenery props }
      StreamWriteLong(S, Data.PropCount);
      for I := 0 to Data.PropCount - 1 do
        StreamWriteBytes(S, Data.Props[I], SizeOf(TPMSProp));

      { Scenery names }
      StreamWriteLong(S, Data.ScenNameCount);
      for I := 0 to Data.ScenNameCount - 1 do
        StreamWriteBytes(S, Data.ScenNames[I], SizeOf(TPMSSceneryName));

      { Colliders }
      StreamWriteLong(S, Data.ColliderCount);
      for I := 0 to Data.ColliderCount - 1 do
        StreamWriteBytes(S, Data.Colliders[I], SizeOf(TPMSCollider));

      { Spawns }
      StreamWriteLong(S, Data.SpawnCount);
      for I := 0 to Data.SpawnCount - 1 do
        StreamWriteBytes(S, Data.Spawns[I], SizeOf(TPMSSpawnPoint));

      { Waypoints }
      StreamWriteLong(S, Data.WaypointCount);
      for I := 0 to Data.WaypointCount - 1 do
        StreamWriteBytes(S, Data.Waypoints[I], SizeOf(TPMSWaypoint));

      { PolyWorks extension: lights + sketch }
      LightCnt := SmallInt(Data.LightCount);
      StreamWriteSmall(S, LightCnt);
      for I := 0 to Data.LightCount - 1 do
        StreamWriteBytes(S, Data.Lights[I], SizeOf(TPMSLight));

      SketchCnt := SmallInt(Data.SketchCount);
      StreamWriteSmall(S, SketchCnt);
      for I := 0 to Data.SketchCount - 1 do
        StreamWriteBytes(S, Data.Sketch[I], SizeOf(TPMSSketchLine));

    finally
      S.Free;
    end;
    Result := True;
  except
    on E: Exception do
    begin
      Err := 'Error saving PMS: ' + E.Message;
    end;
  end;
end;

{ ---- CompilePMS (game-ready format) ------------------------------------ }

function CompilePMS(const Filename: string; const Data: TPMSData;
                    out Err: string): Boolean;
var
  S: TFileStream;
  Opt: TPMSOptions;
  CompiledPolys: TPMSPolyArray;
  CompiledProps: TPMSPropArray;
  CompiledColliders: TPMSColliderArray;
  CompiledSpawns: TPMSSpawnArray;
  CompiledWPs: TPMSWaypointArray;
  Sectors: TSectorTable;
  SectorDiv: LongInt;
  SectorNumConst: LongInt;
  I, J, K: Integer;
  ZeroSmall: SmallInt;
  MapID: LongInt;
  MapW, MapH: Single;
  XOff, YOff: Single;
  MinX, MinY, MaxX, MaxY: Single;
  CellCount: SmallInt;
begin
  Result := False;
  Err := '';

  { Deep-copy polygon data so we can centre coordinates }
  SetLength(CompiledPolys, Data.PolyCount);
  for I := 0 to Data.PolyCount - 1 do
    CompiledPolys[I] := Data.Polys[I];

  { Compute bounding box and centering offset }
  ComputePolyBounds(CompiledPolys, Data.PolyCount, MinX, MinY, MaxX, MaxY);
  XOff := (MinX + MaxX) * 0.5;
  YOff := (MinY + MaxY) * 0.5;
  MapW := MaxX - MinX;
  MapH := MaxY - MinY;

  { Apply centering offset to all polygon vertices }
  for I := 0 to Data.PolyCount - 1 do
    for J := 1 to 3 do
    begin
      CompiledPolys[I].Poly.V[J].X := CompiledPolys[I].Poly.V[J].X - XOff;
      CompiledPolys[I].Poly.V[J].Y := CompiledPolys[I].Poly.V[J].Y - YOff;
    end;

  { Recompute normals for all polygons }
  for I := 0 to Data.PolyCount - 1 do
    RecomputePolyNormals(CompiledPolys[I]);

  { Compute sector division }
  if MapW > MapH then
    SectorDiv := VBInt((MapW + 100) / 25)
  else
    SectorDiv := VBInt((MapH + 100) / 25);
  if SectorDiv < 1 then SectorDiv := 1;

  { Build sector table }
  BuildSectorTable(CompiledPolys, Data.PolyCount, SectorDiv, XOff, YOff, Sectors);

  { Centre scenery props }
  SetLength(CompiledProps, Data.PropCount);
  for I := 0 to Data.PropCount - 1 do
  begin
    CompiledProps[I] := Data.Props[I];
    CompiledProps[I].X := Data.Props[I].X - XOff;
    CompiledProps[I].Y := Data.Props[I].Y - YOff;
  end;

  { Centre colliders }
  SetLength(CompiledColliders, Data.ColliderCount);
  for I := 0 to Data.ColliderCount - 1 do
  begin
    CompiledColliders[I] := Data.Colliders[I];
    CompiledColliders[I].X := Data.Colliders[I].X - XOff;
    CompiledColliders[I].Y := Data.Colliders[I].Y - YOff;
  end;

  { Centre spawn points (note: file stores as LongInt) }
  SetLength(CompiledSpawns, Data.SpawnCount);
  for I := 0 to Data.SpawnCount - 1 do
  begin
    CompiledSpawns[I] := Data.Spawns[I];
    CompiledSpawns[I].X := Round(Data.Spawns[I].X - XOff);
    CompiledSpawns[I].Y := Round(Data.Spawns[I].Y - YOff);
  end;

  { Centre waypoints }
  SetLength(CompiledWPs, Data.WaypointCount);
  for I := 0 to Data.WaypointCount - 1 do
  begin
    CompiledWPs[I] := Data.Waypoints[I];
    CompiledWPs[I].X := Round(Data.Waypoints[I].X - XOff);
    CompiledWPs[I].Y := Round(Data.Waypoints[I].Y - YOff);
  end;

  { Assign a new positive MapRandomID (use a simple hash of the map name) }
  MapID := Abs(LongInt(PtrUInt(@Data.Options.MapName[0]) xor $12345678));
  if MapID <= 0 then MapID := 1;

  try
    S := TFileStream.Create(Filename, fmCreate);
    try
      StreamWriteLong(S, PMS_VERSION);

      Opt := Data.Options;
      Opt.MapRandomID := MapID;
      StreamWriteBytes(S, Opt, SizeOf(TPMSOptions));

      StreamWriteLong(S, Data.PolyCount);
      for I := 0 to Data.PolyCount - 1 do
        StreamWriteBytes(S, CompiledPolys[I], SizeOf(TPMSPolyEntry));

      SectorNumConst := SECTOR_NUM;
      StreamWriteLong(S, SectorDiv);
      StreamWriteLong(S, SectorNumConst);

      { Write computed sector table }
      for I := 0 to SECTOR_CELLS - 1 do
        for J := 0 to SECTOR_CELLS - 1 do
        begin
          CellCount := Sectors[I][J].PolyCount;
          StreamWriteSmall(S, CellCount);
          for K := 0 to CellCount - 1 do
          begin
            ZeroSmall := Sectors[I][J].PolyIndex[K];
            StreamWriteSmall(S, ZeroSmall);
          end;
        end;

      StreamWriteLong(S, Data.PropCount);
      for I := 0 to Data.PropCount - 1 do
        StreamWriteBytes(S, CompiledProps[I], SizeOf(TPMSProp));

      StreamWriteLong(S, Data.ScenNameCount);
      for I := 0 to Data.ScenNameCount - 1 do
        StreamWriteBytes(S, Data.ScenNames[I], SizeOf(TPMSSceneryName));

      StreamWriteLong(S, Data.ColliderCount);
      for I := 0 to Data.ColliderCount - 1 do
        StreamWriteBytes(S, CompiledColliders[I], SizeOf(TPMSCollider));

      StreamWriteLong(S, Data.SpawnCount);
      for I := 0 to Data.SpawnCount - 1 do
        StreamWriteBytes(S, CompiledSpawns[I], SizeOf(TPMSSpawnPoint));

      StreamWriteLong(S, Data.WaypointCount);
      for I := 0 to Data.WaypointCount - 1 do
        StreamWriteBytes(S, CompiledWPs[I], SizeOf(TPMSWaypoint));

      { Trailing 4 × SmallInt zeros (matching VB6 SaveAndCompile behaviour) }
      ZeroSmall := 0;
      StreamWriteSmall(S, ZeroSmall);
      StreamWriteSmall(S, ZeroSmall);
      StreamWriteSmall(S, ZeroSmall);
      StreamWriteSmall(S, ZeroSmall);

    finally
      S.Free;
    end;
    Result := True;
  except
    on E: Exception do
      Err := 'Error compiling PMS: ' + E.Message;
  end;
end;

end.
