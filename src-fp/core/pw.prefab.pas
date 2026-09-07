unit pw.prefab;

{$mode objfpc}{$H+}
{$PackRecords 1}

interface

uses
  SysUtils, Classes, pw.types, pw.utils, pw.map;

function SavePrefab(const Filename: string; Doc: TMapDocument;
                    out Err: string): Boolean;

function LoadPrefab(const Filename: string; Doc: TMapDocument;
                    out Err: string): Boolean;

implementation

uses
  pw.geometry;

type
  TPrefabConnection = packed record
    Point1: SmallInt;
    Point2: SmallInt;
  end;

function PolyFullySelected(const P: TEditorPoly): Boolean;
begin
  Result := P.Selected[1] and P.Selected[2] and P.Selected[3];
end;

procedure StreamReadBytes(S: TStream; var Buf; Count: Integer);
begin
  if Count <= 0 then
    Exit;
  if S.Read(Buf, Count) <> Count then
    raise EStreamError.Create('Unexpected end of file');
end;

procedure StreamReadLong(S: TStream; out V: LongInt);
begin
  StreamReadBytes(S, V, SizeOf(V));
end;

procedure StreamReadSmall(S: TStream; out V: SmallInt);
begin
  StreamReadBytes(S, V, SizeOf(V));
end;

procedure StreamWriteBytes(S: TStream; const Buf; Count: Integer);
begin
  if Count > 0 then
    S.WriteBuffer(Buf, Count);
end;

procedure StreamWriteLong(S: TStream; V: LongInt);
begin
  S.WriteBuffer(V, SizeOf(V));
end;

procedure StreamWriteSmall(S: TStream; V: SmallInt);
begin
  S.WriteBuffer(V, SizeOf(V));
end;

function SceneryNameForStyle(const Doc: TMapDocument; Style: SmallInt): string;
begin
  if (Style >= 1) and (Style <= Doc.ScenNameCount) and
     (Style < Length(Doc.SceneryNames)) then
    Result := Doc.SceneryNames[Style]
  else
    Result := '';
end;

function SavePrefab(const Filename: string; Doc: TMapDocument;
                    out Err: string): Boolean;
var
  S: TFileStream;
  I, J, Count: Integer;
  PolyCount, SceneryCount, ColliderCount, SpawnCount, WaypointCount, ConnCount: LongInt;
  Entry: TPMSPolyEntry;
  Prop: TPMSProp;
  Col: TPMSCollider;
  Spawn: TPMSSpawnPoint;
  WP: TPMSWaypoint;
  Conn: TPrefabConnection;
  WPMap: array of Integer;
  NameBytes: RawByteString;
  NameLen: SmallInt;
begin
  Result := False;
  Err := '';

  if Doc = nil then
  begin
    Err := 'No document supplied';
    Exit;
  end;

  PolyCount := 0;
  for I := 0 to Doc.PolyCount - 1 do
    if PolyFullySelected(Doc.Polys[I]) then
      Inc(PolyCount);

  SceneryCount := 0;
  for I := 0 to Doc.SceneryCount - 1 do
    if Doc.Scenery[I].Selected then
      Inc(SceneryCount);

  ColliderCount := 0;
  for I := 0 to Doc.ColliderCount - 1 do
    if Doc.Colliders[I].Selected then
      Inc(ColliderCount);

  SpawnCount := 0;
  for I := 0 to Doc.SpawnCount - 1 do
    if Doc.Spawns[I].Selected then
      Inc(SpawnCount);

  SetLength(WPMap, Doc.WaypointCount + 1);
  WaypointCount := 0;
  for I := 0 to Doc.WaypointCount - 1 do
    if Doc.Waypoints[I].Selected then
    begin
      Inc(WaypointCount);
      WPMap[I + 1] := WaypointCount;
    end;

  ConnCount := 0;
  for I := 0 to Doc.ConnCount - 1 do
    if (Doc.Connections[I].Point1 >= 1) and (Doc.Connections[I].Point1 <= Doc.WaypointCount) and
       (Doc.Connections[I].Point2 >= 1) and (Doc.Connections[I].Point2 <= Doc.WaypointCount) and
       (WPMap[Doc.Connections[I].Point1] > 0) and (WPMap[Doc.Connections[I].Point2] > 0) then
      Inc(ConnCount);

  try
    S := TFileStream.Create(Filename, fmCreate);
    try
      StreamWriteLong(S, PolyCount);
      for I := 0 to Doc.PolyCount - 1 do
        if PolyFullySelected(Doc.Polys[I]) then
        begin
          FillChar(Entry, SizeOf(Entry), 0);
          Entry.PolyType := Doc.Polys[I].PolyType;
          Entry.Poly.Perp := Doc.Polys[I].Perp;
          for J := 1 to 3 do
          begin
            Entry.Poly.V[J].X := Doc.Polys[I].V[J].World.X;
            Entry.Poly.V[J].Y := Doc.Polys[I].V[J].World.Y;
            Entry.Poly.V[J].Z := 1.0;
            Entry.Poly.V[J].Rhw := 1.0;
            Entry.Poly.V[J].Color := Color3ToARGB(Doc.Polys[I].BaseColor[J], Doc.Polys[I].V[J].Alpha);
            Entry.Poly.V[J].Tu := Doc.Polys[I].V[J].Tu;
            Entry.Poly.V[J].Tv := Doc.Polys[I].V[J].Tv;
          end;
          RecomputePolyNormals(Entry);
          StreamWriteBytes(S, Entry, SizeOf(Entry));
        end;

      StreamWriteLong(S, SceneryCount);
      for I := 0 to Doc.SceneryCount - 1 do
        if Doc.Scenery[I].Selected then
        begin
          FillChar(Prop, SizeOf(Prop), 0);
          Prop.Active := WordBool(-1);
          Prop.Style := Doc.Scenery[I].Style;
          Prop.Width := Doc.Scenery[I].Width;
          Prop.Height := Doc.Scenery[I].Height;
          Prop.X := Doc.Scenery[I].X;
          Prop.Y := Doc.Scenery[I].Y;
          Prop.Rotation := Doc.Scenery[I].Rotation;
          Prop.ScaleX := Doc.Scenery[I].ScaleX;
          Prop.ScaleY := Doc.Scenery[I].ScaleY;
          Prop.Alpha := Doc.Scenery[I].Alpha;
          Prop.Color := Doc.Scenery[I].Color;
          Prop.Level := Doc.Scenery[I].Level;
          StreamWriteBytes(S, Prop, SizeOf(Prop));

          NameBytes := RawByteString(SceneryNameForStyle(Doc, Doc.Scenery[I].Style));
          if Length(NameBytes) > High(SmallInt) then
            SetLength(NameBytes, High(SmallInt));
          NameLen := Length(NameBytes);
          StreamWriteSmall(S, NameLen);
          if NameLen > 0 then
            StreamWriteBytes(S, NameBytes[1], NameLen);
        end;

      StreamWriteLong(S, ColliderCount);
      for I := 0 to Doc.ColliderCount - 1 do
        if Doc.Colliders[I].Selected then
        begin
          FillChar(Col, SizeOf(Col), 0);
          if Doc.Colliders[I].Active then
            Col.Active := 1;
          Col.X := Doc.Colliders[I].X;
          Col.Y := Doc.Colliders[I].Y;
          Col.Radius := Doc.Colliders[I].Radius;
          StreamWriteBytes(S, Col, SizeOf(Col));
        end;

      StreamWriteLong(S, SpawnCount);
      for I := 0 to Doc.SpawnCount - 1 do
        if Doc.Spawns[I].Selected then
        begin
          FillChar(Spawn, SizeOf(Spawn), 0);
          if Doc.Spawns[I].Active then
            Spawn.Active := 1;
          Spawn.X := Round(Doc.Spawns[I].X);
          Spawn.Y := Round(Doc.Spawns[I].Y);
          Spawn.Team := Doc.Spawns[I].Team;
          StreamWriteBytes(S, Spawn, SizeOf(Spawn));
        end;

      StreamWriteLong(S, WaypointCount);
      for I := 0 to Doc.WaypointCount - 1 do
        if Doc.Waypoints[I].Selected then
        begin
          FillChar(WP, SizeOf(WP), 0);
          WP.Active := 1;
          WP.ID := WPMap[I + 1];
          WP.X := Round(Doc.Waypoints[I].X);
          WP.Y := Round(Doc.Waypoints[I].Y);
          if Doc.Waypoints[I].Left then WP.Left := 1;
          if Doc.Waypoints[I].Right then WP.Right := 1;
          if Doc.Waypoints[I].Up then WP.Up := 1;
          if Doc.Waypoints[I].Down then WP.Down := 1;
          if Doc.Waypoints[I].M2 then WP.M2 := 1;
          WP.PathNum := Doc.Waypoints[I].PathNum;
          WP.Special := Doc.Waypoints[I].Special;
          Count := 0;
          for J := 0 to Doc.ConnCount - 1 do
            if (Doc.Connections[J].Point1 = I + 1) and
               (Doc.Connections[J].Point2 >= 1) and (Doc.Connections[J].Point2 <= Doc.WaypointCount) and
               (WPMap[Doc.Connections[J].Point2] > 0) and
               (Count < MAX_CONNECTIONS_PER_WP) then
            begin
              Inc(Count);
              WP.Connections[Count] := WPMap[Doc.Connections[J].Point2];
            end;
          WP.ConnectionsNum := Count;
          StreamWriteBytes(S, WP, SizeOf(WP));
        end;

      StreamWriteLong(S, ConnCount);
      for I := 0 to Doc.ConnCount - 1 do
        if (Doc.Connections[I].Point1 >= 1) and (Doc.Connections[I].Point1 <= Doc.WaypointCount) and
           (Doc.Connections[I].Point2 >= 1) and (Doc.Connections[I].Point2 <= Doc.WaypointCount) and
           (WPMap[Doc.Connections[I].Point1] > 0) and (WPMap[Doc.Connections[I].Point2] > 0) then
        begin
          Conn.Point1 := WPMap[Doc.Connections[I].Point1];
          Conn.Point2 := WPMap[Doc.Connections[I].Point2];
          StreamWriteBytes(S, Conn, SizeOf(Conn));
        end;
    finally
      S.Free;
    end;
    Result := True;
  except
    on E: Exception do
      Err := 'Error saving prefab: ' + E.Message;
  end;
end;

function LoadPrefab(const Filename: string; Doc: TMapDocument;
                    out Err: string): Boolean;
var
  S: TFileStream;
  I, J: Integer;
  NewPolyCount, NewSceneryCount, NewColliderCount, NewSpawnCount,
  NewWaypointCount, NewConnCount: LongInt;
  Polys: TPMSPolyArray;
  Props: TPMSPropArray;
  PropNames: array of string;
  Colliders: TPMSColliderArray;
  Spawns: TPMSSpawnArray;
  Waypoints: TPMSWaypointArray;
  Conns: array of TPrefabConnection;
  NameLen: SmallInt;
  NameBytes: RawByteString;
  P: TEditorPoly;
  ES: TEditorScenery;
  EC: TEditorCollider;
  ESpawn: TEditorSpawn;
  EW: TEditorWaypoint;
  BaseWaypointCount: Integer;
  StyleIdx: Integer;
  Abs1, Abs2: Integer;
begin
  Result := False;
  Err := '';

  if Doc = nil then
  begin
    Err := 'No document supplied';
    Exit;
  end;

  if not FileExists(Filename) then
  begin
    Err := 'File not found: ' + Filename;
    Exit;
  end;

  try
    S := TFileStream.Create(Filename, fmOpenRead or fmShareDenyNone);
    try
      StreamReadLong(S, NewPolyCount);
      SetLength(Polys, NewPolyCount);
      for I := 0 to NewPolyCount - 1 do
        StreamReadBytes(S, Polys[I], SizeOf(TPMSPolyEntry));

      StreamReadLong(S, NewSceneryCount);
      SetLength(Props, NewSceneryCount);
      SetLength(PropNames, NewSceneryCount);
      for I := 0 to NewSceneryCount - 1 do
      begin
        StreamReadBytes(S, Props[I], SizeOf(TPMSProp));
        StreamReadSmall(S, NameLen);
        if NameLen < 0 then
          NameLen := 0;
        SetLength(NameBytes, NameLen);
        if NameLen > 0 then
          StreamReadBytes(S, NameBytes[1], NameLen);
        PropNames[I] := string(NameBytes);
      end;

      StreamReadLong(S, NewColliderCount);
      SetLength(Colliders, NewColliderCount);
      for I := 0 to NewColliderCount - 1 do
        StreamReadBytes(S, Colliders[I], SizeOf(TPMSCollider));

      StreamReadLong(S, NewSpawnCount);
      SetLength(Spawns, NewSpawnCount);
      for I := 0 to NewSpawnCount - 1 do
        StreamReadBytes(S, Spawns[I], SizeOf(TPMSSpawnPoint));

      StreamReadLong(S, NewWaypointCount);
      SetLength(Waypoints, NewWaypointCount);
      for I := 0 to NewWaypointCount - 1 do
        StreamReadBytes(S, Waypoints[I], SizeOf(TPMSWaypoint));

      StreamReadLong(S, NewConnCount);
      SetLength(Conns, NewConnCount);
      for I := 0 to NewConnCount - 1 do
        StreamReadBytes(S, Conns[I], SizeOf(TPrefabConnection));
    finally
      S.Free;
    end;

    Doc.ClearSelection;

    for I := 0 to NewPolyCount - 1 do
    begin
      FillChar(P, SizeOf(P), 0);
      P.PolyType := Polys[I].PolyType;
      P.Perp := Polys[I].Poly.Perp;
      for J := 1 to 3 do
      begin
        P.V[J].World.X := Polys[I].Poly.V[J].X;
        P.V[J].World.Y := Polys[I].Poly.V[J].Y;
        P.V[J].Color := ARGBToColor3(Polys[I].Poly.V[J].Color);
        P.V[J].Alpha := GetAlpha(Polys[I].Poly.V[J].Color);
        P.V[J].Tu := Polys[I].Poly.V[J].Tu;
        P.V[J].Tv := Polys[I].Poly.V[J].Tv;
        P.BaseColor[J] := P.V[J].Color;
        P.ScreenV[J].Color := Polys[I].Poly.V[J].Color;
        P.Selected[J] := True;
      end;
      SetLength(Doc.Polys, Doc.PolyCount + 1);
      Doc.Polys[Doc.PolyCount] := P;
      Inc(Doc.PolyCount);
    end;

    for I := 0 to NewSceneryCount - 1 do
    begin
      if (Props[I].X > 32766) or (Props[I].X < -32766) or
         (Props[I].Y > 32766) or (Props[I].Y < -32766) then
        Continue;
      if (Props[I].Width < 0) or (Props[I].Height < 0) or
         (Trunc(Props[I].ScaleX * 1000) = 0) or (Trunc(Props[I].ScaleY * 1000) = 0) then
        Continue;
      if (Props[I].ScaleX < -10000) or (Props[I].ScaleX > 10000) or
         (Props[I].ScaleY < -10000) or (Props[I].ScaleY > 10000) then
        Continue;

      FillChar(ES, SizeOf(ES), 0);
      StyleIdx := Doc.AddSceneryName(PropNames[I]);
      ES.Style := StyleIdx;
      ES.X := Props[I].X;
      ES.Y := Props[I].Y;
      ES.Rotation := Props[I].Rotation;
      ES.ScaleX := Props[I].ScaleX;
      ES.ScaleY := Props[I].ScaleY;
      ES.Width := Props[I].Width;
      ES.Height := Props[I].Height;
      if Props[I].Alpha < 1 then
        ES.Alpha := 255
      else if Props[I].Alpha <= 255 then
        ES.Alpha := Byte(Props[I].Alpha)
      else
        ES.Alpha := 255;
      ES.Color := LongInt(ARGBWithAlpha(ES.Alpha, LongWord(Props[I].Color)));
      if (Props[I].Level >= 0) and (Props[I].Level <= 255) then
        ES.Level := Byte(Props[I].Level)
      else
        ES.Level := 0;
      ES.Selected := True;
      SetLength(Doc.Scenery, Doc.SceneryCount + 1);
      Doc.Scenery[Doc.SceneryCount] := ES;
      Inc(Doc.SceneryCount);
    end;

    for I := 0 to NewColliderCount - 1 do
    begin
      FillChar(EC, SizeOf(EC), 0);
      EC.Active := Colliders[I].Active <> 0;
      EC.X := Colliders[I].X;
      EC.Y := Colliders[I].Y;
      EC.Radius := Colliders[I].Radius;
      EC.Selected := True;
      SetLength(Doc.Colliders, Doc.ColliderCount + 1);
      Doc.Colliders[Doc.ColliderCount] := EC;
      Inc(Doc.ColliderCount);
    end;

    for I := 0 to NewSpawnCount - 1 do
    begin
      FillChar(ESpawn, SizeOf(ESpawn), 0);
      ESpawn.Active := Spawns[I].Active <> 0;
      ESpawn.X := Spawns[I].X;
      ESpawn.Y := Spawns[I].Y;
      ESpawn.Team := Byte(Spawns[I].Team and $FF);
      ESpawn.Selected := True;
      SetLength(Doc.Spawns, Doc.SpawnCount + 1);
      Doc.Spawns[Doc.SpawnCount] := ESpawn;
      Inc(Doc.SpawnCount);
    end;

    BaseWaypointCount := Doc.WaypointCount;
    for I := 0 to NewWaypointCount - 1 do
    begin
      FillChar(EW, SizeOf(EW), 0);
      EW.X := Waypoints[I].X;
      EW.Y := Waypoints[I].Y;
      EW.ID := BaseWaypointCount + I + 1;
      EW.Left := Waypoints[I].Left <> 0;
      EW.Right := Waypoints[I].Right <> 0;
      EW.Up := Waypoints[I].Up <> 0;
      EW.Down := Waypoints[I].Down <> 0;
      EW.M2 := Waypoints[I].M2 <> 0;
      EW.PathNum := Waypoints[I].PathNum;
      EW.Special := Waypoints[I].Special;
      EW.NumConns := 0;
      EW.Selected := True;
      EW.TempIndex := BaseWaypointCount + I + 1;
      SetLength(Doc.Waypoints, Doc.WaypointCount + 1);
      Doc.Waypoints[Doc.WaypointCount] := EW;
      Inc(Doc.WaypointCount);
    end;

    for I := 0 to NewConnCount - 1 do
    begin
      if (Conns[I].Point1 < 1) or (Conns[I].Point1 > NewWaypointCount) or
         (Conns[I].Point2 < 1) or (Conns[I].Point2 > NewWaypointCount) then
        Continue;
      Abs1 := BaseWaypointCount + Conns[I].Point1;
      Abs2 := BaseWaypointCount + Conns[I].Point2;
      SetLength(Doc.Connections, Doc.ConnCount + 1);
      Doc.Connections[Doc.ConnCount].Point1 := Abs1;
      Doc.Connections[Doc.ConnCount].Point2 := Abs2;
      Inc(Doc.ConnCount);
      Inc(Doc.Waypoints[Abs1 - 1].NumConns);
    end;

    Doc.Modified := True;
    Doc.RebuildScreenCache;
    Result := True;
  except
    on E: EStreamError do
      Err := 'File truncated or corrupt: ' + E.Message;
    on E: Exception do
      Err := 'Error loading prefab: ' + E.Message;
  end;
end;

end.
