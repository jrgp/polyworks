unit pw.map;

{$mode objfpc}{$H+}
{$PackRecords 1}

interface

uses
  SysUtils,
  pw.types, pw.utils, pw.pms, pw.geometry;

type
  TMapDocument = class
  public
    { Map data arrays (0-based Pascal arrays) }
    Polys        : array of TEditorPoly;
    PolyCount    : Integer;
    Scenery      : array of TEditorScenery;
    SceneryCount : Integer;
    SceneryNames : array of string;     { 1-based indexing; [0] is unused/empty }
    ScenNameCount: Integer;
    Spawns       : array of TEditorSpawn;
    SpawnCount   : Integer;
    Colliders    : array of TEditorCollider;
    ColliderCount: Integer;
    Waypoints    : array of TEditorWaypoint;
    WaypointCount: Integer;
    Connections  : array of TEditorConnection;
    ConnCount    : Integer;
    Lights       : array of TEditorLight;
    LightCount   : Integer;
    Sketch       : array of TPMSSketchLine;
    SketchCount  : Integer;
    Options      : TPMSOptions;
    Modified     : Boolean;

    { View state }
    Zoom     : Single;
    ScrollX  : Single;
    ScrollY  : Single;

    { Lifecycle }
    constructor Create;
    procedure   NewMap;
    procedure   LoadFromPMS(const Data: TPMSData);
    procedure   SaveToPMS(out Data: TPMSData);

    { Coordinate conversion }
    procedure WorldToScreen(WX, WY: Single; out SX, SY: Single);
    procedure ScreenToWorld(SX, SY: Single; out WX, WY: Single);
    procedure RebuildScreenCache;

    { Zoom/pan }
    procedure SetZoom(NewZoom, CentreScreenX, CentreScreenY: Single);
    procedure Scroll(DX, DY: Single);

    { Polygon operations }
    procedure AddPoly(const WV: array of TEditorVertex; Kind: Byte);
    procedure DeleteSelectedPolys;

    { Entity operations }
    procedure AddScenery(NameIndex: SmallInt; WorldX, WorldY: Single;
                         ScaleX, ScaleY: Single; Rotation: Single;
                         Width, Height: LongInt);
    function  AddSceneryName(const Filename: string): Integer;  { returns 1-based index }
    procedure AddSpawn(WorldX, WorldY: Single; Team: Byte);
    procedure AddCollider(WorldX, WorldY, Radius: Single);
    procedure AddWaypoint(WorldX, WorldY: Single);
    procedure AddConnection(WP1, WP2: Integer);  { 1-based indices }
    procedure AddLight(WorldX, WorldY, WorldZ: Single; AColor: TColor3;
                       Intensity: Single; Range: SmallInt);
    procedure AddSketchLine(const SL: TPMSSketchLine);

    { Selection }
    procedure ClearSelection;
    procedure SelectPolyVertex(PolyIdx, VertIdx: Integer; Mode: TSelMode);
    procedure SelectByWorldRect(WX1, WY1, WX2, WY2: Single; Mode: TSelMode);

    { Editing }
    procedure MoveSelectedWorld(DX, DY: Single);
    procedure DeleteSelected;
    procedure SetSelectedPolyType(Kind: Byte);
    procedure SnapSelectedToGrid(GridSize: Single);
    procedure DuplicateSelected(OffsetX, OffsetY: Single);
    procedure CopySelected;
    procedure PasteClipboard;
    procedure InvertSelection;
    procedure SelectByColor;
  end;

implementation

uses
  Math;

function ClampTeam(Value: LongInt): Byte;
begin
  if Value < 0 then
    Result := 0
  else if Value > 31 then
    Result := 31
  else
    Result := Byte(Value);
end;

function PolyFullySelected(const P: TEditorPoly): Boolean;
begin
  Result := P.Selected[1] and P.Selected[2] and P.Selected[3];
end;

function PolyAnySelected(const P: TEditorPoly): Boolean;
begin
  Result := P.Selected[1] or P.Selected[2] or P.Selected[3];
end;

procedure ApplySelection(var Selected: Boolean; Inside: Boolean; Mode: TSelMode);
begin
  if not Inside then
    Exit;

  case Mode of
    smReplace, smAdd:
      Selected := True;
    smSubtract:
      Selected := False;
  end;
end;

constructor TMapDocument.Create;
begin
  inherited Create;
  NewMap;
end;

procedure TMapDocument.NewMap;
begin
  SetLength(Polys, 0);
  PolyCount := 0;

  SetLength(Scenery, 0);
  SceneryCount := 0;

  SetLength(SceneryNames, 1);
  SceneryNames[0] := '';
  ScenNameCount := 0;

  SetLength(Spawns, 0);
  SpawnCount := 0;

  SetLength(Colliders, 0);
  ColliderCount := 0;

  SetLength(Waypoints, 0);
  WaypointCount := 0;

  SetLength(Connections, 0);
  ConnCount := 0;

  SetLength(Lights, 0);
  LightCount := 0;

  SetLength(Sketch, 0);
  SketchCount := 0;

  FillChar(Options, SizeOf(Options), 0);
  Modified := False;

  Zoom := 1.0;
  ScrollX := 0.0;
  ScrollY := 0.0;
end;

procedure TMapDocument.LoadFromPMS(const Data: TPMSData);
var
  I, J, OutIdx: Integer;
  Prop: TPMSProp;
  P: TEditorPoly;
  WP: TPMSWaypoint;
  C: TEditorCollider;
  S: TEditorSpawn;
  L: TEditorLight;
  ES: TEditorScenery;
  EW: TEditorWaypoint;
begin
  NewMap;
  Options := Data.Options;

  PolyCount := Data.PolyCount;
  SetLength(Polys, PolyCount);
  for I := 0 to PolyCount - 1 do
  begin
    FillChar(P, SizeOf(P), 0);
    P.PolyType := Data.Polys[I].PolyType;
    P.Perp := Data.Polys[I].Poly.Perp;
    for J := 1 to 3 do
    begin
      P.V[J].World.X := Data.Polys[I].Poly.V[J].X;
      P.V[J].World.Y := Data.Polys[I].Poly.V[J].Y;
      P.V[J].Color := ARGBToColor3(Data.Polys[I].Poly.V[J].Color);
      P.V[J].Alpha := GetAlpha(Data.Polys[I].Poly.V[J].Color);
      P.V[J].Tu := Data.Polys[I].Poly.V[J].Tu;
      P.V[J].Tv := Data.Polys[I].Poly.V[J].Tv;
      P.BaseColor[J] := P.V[J].Color;
      P.ScreenV[J].Color := Data.Polys[I].Poly.V[J].Color;
    end;
    Polys[I] := P;
  end;

  ScenNameCount := Data.ScenNameCount;
  SetLength(SceneryNames, ScenNameCount + 1);
  SceneryNames[0] := '';
  for I := 0 to ScenNameCount - 1 do
    SceneryNames[I + 1] := ReadPascalStr(Data.ScenNames[I].Name);

  SetLength(Scenery, Data.PropCount);
  OutIdx := 0;
  for I := 0 to Data.PropCount - 1 do
  begin
    Prop := Data.Props[I];
    if (Prop.X > 32766) or (Prop.X < -32766) or
       (Prop.Y > 32766) or (Prop.Y < -32766) then
      Continue;
    if (Prop.Width < 0) or (Prop.Height < 0) or
       (Trunc(Prop.ScaleX * 1000) = 0) or (Trunc(Prop.ScaleY * 1000) = 0) then
      Continue;
    if (Prop.ScaleX < -10000) or (Prop.ScaleX > 10000) or
       (Prop.ScaleY < -10000) or (Prop.ScaleY > 10000) then
      Continue;
    if Prop.Style < 1 then
      Continue;

    FillChar(ES, SizeOf(ES), 0);
    ES.Style := Prop.Style;
    ES.X := Prop.X;
    ES.Y := Prop.Y;
    ES.Rotation := Prop.Rotation;
    ES.ScaleX := Prop.ScaleX;
    ES.ScaleY := Prop.ScaleY;
    ES.Width := Prop.Width;
    ES.Height := Prop.Height;
    if Prop.Alpha < 1 then
      ES.Alpha := 255
    else if Prop.Alpha <= 255 then
      ES.Alpha := Byte(Prop.Alpha)
    else
      ES.Alpha := 255;
    ES.Color := LongInt(ARGBWithAlpha(ES.Alpha, LongWord(Prop.Color)));
    if (Prop.Level >= 0) and (Prop.Level <= 255) then
      ES.Level := Byte(Prop.Level)
    else
      ES.Level := 0;
    Scenery[OutIdx] := ES;
    Inc(OutIdx);
  end;
  SceneryCount := OutIdx;
  SetLength(Scenery, SceneryCount);

  ColliderCount := Data.ColliderCount;
  SetLength(Colliders, ColliderCount);
  for I := 0 to ColliderCount - 1 do
  begin
    FillChar(C, SizeOf(C), 0);
    C.Active := Data.Colliders[I].Active <> 0;
    C.X := Data.Colliders[I].X;
    C.Y := Data.Colliders[I].Y;
    C.Radius := Data.Colliders[I].Radius;
    Colliders[I] := C;
  end;

  SpawnCount := Data.SpawnCount;
  SetLength(Spawns, SpawnCount);
  for I := 0 to SpawnCount - 1 do
  begin
    FillChar(S, SizeOf(S), 0);
    S.Active := Data.Spawns[I].Active <> 0;
    S.X := Data.Spawns[I].X;
    S.Y := Data.Spawns[I].Y;
    S.Team := ClampTeam(Data.Spawns[I].Team);
    Spawns[I] := S;
  end;

  WaypointCount := Data.WaypointCount;
  SetLength(Waypoints, WaypointCount);
  SetLength(Connections, 0);
  ConnCount := 0;
  for I := 0 to WaypointCount - 1 do
  begin
    WP := Data.Waypoints[I];
    FillChar(EW, SizeOf(EW), 0);
    EW.X := WP.X;
    EW.Y := WP.Y;
    if WP.ID <> 0 then
      EW.ID := WP.ID
    else
      EW.ID := I + 1;
    EW.Left := WP.Left <> 0;
    EW.Right := WP.Right <> 0;
    EW.Up := WP.Up <> 0;
    EW.Down := WP.Down <> 0;
    EW.M2 := WP.M2 <> 0;
    EW.PathNum := WP.PathNum;
    EW.Special := WP.Special;
    if WP.ConnectionsNum > 0 then
      EW.NumConns := Min(WP.ConnectionsNum, MAX_CONNECTIONS_PER_WP)
    else
      EW.NumConns := 0;
    EW.TempIndex := I + 1;
    Waypoints[I] := EW;

    if WP.ConnectionsNum > 0 then
      for J := 1 to Min(WP.ConnectionsNum, MAX_CONNECTIONS_PER_WP) do
      begin
        SetLength(Connections, ConnCount + 1);
        Connections[ConnCount].Point1 := I + 1;
        Connections[ConnCount].Point2 := WP.Connections[J];
        Inc(ConnCount);
      end;
  end;

  LightCount := Data.LightCount;
  SetLength(Lights, LightCount);
  for I := 0 to LightCount - 1 do
  begin
    FillChar(L, SizeOf(L), 0);
    L.X := Data.Lights[I].X;
    L.Y := Data.Lights[I].Y;
    L.Z := Data.Lights[I].Z;
    L.Color := Data.Lights[I].Color;
    L.Intensity := Data.Lights[I].Intensity;
    L.Range := Data.Lights[I].Range;
    L.Selected := Data.Lights[I].Selected <> 0;
    Lights[I] := L;
  end;

  SketchCount := Data.SketchCount;
  SetLength(Sketch, SketchCount);
  for I := 0 to SketchCount - 1 do
    Sketch[I] := Data.Sketch[I];

  Modified := False;
  RebuildScreenCache;
end;

procedure TMapDocument.SaveToPMS(out Data: TPMSData);
var
  I, J, Count: Integer;
  Entry: TPMSPolyEntry;
  Prop: TPMSProp;
  SP: TPMSSpawnPoint;
  WP: TPMSWaypoint;
  L: TPMSLight;
  SN: TPMSSceneryName;
  MinX, MinY, MaxX, MaxY: Single;
begin
  FillChar(Data, SizeOf(Data), 0);
  Data.Version := PMS_VERSION;
  Data.Options := Options;

  Data.PolyCount := PolyCount;
  SetLength(Data.Polys, PolyCount);
  for I := 0 to PolyCount - 1 do
  begin
    FillChar(Entry, SizeOf(Entry), 0);
    Entry.PolyType := Polys[I].PolyType;
    Entry.Poly.Perp := Polys[I].Perp;
    for J := 1 to 3 do
    begin
      Entry.Poly.V[J].X := Polys[I].V[J].World.X;
      Entry.Poly.V[J].Y := Polys[I].V[J].World.Y;
      Entry.Poly.V[J].Z := 1.0;
      Entry.Poly.V[J].Rhw := 1.0;
      Entry.Poly.V[J].Color := Color3ToARGB(Polys[I].BaseColor[J], Polys[I].V[J].Alpha);
      Entry.Poly.V[J].Tu := Polys[I].V[J].Tu;
      Entry.Poly.V[J].Tv := Polys[I].V[J].Tv;
    end;
    RecomputePolyNormals(Entry);
    Data.Polys[I] := Entry;
  end;

  if PolyCount > 0 then
  begin
    ComputePolyBounds(Data.Polys, Data.PolyCount, MinX, MinY, MaxX, MaxY);
    if (MaxX - MinX) > (MaxY - MinY) then
      Data.SectorDiv := VBInt(((MaxX - MinX) + 100) / 25)
    else
      Data.SectorDiv := VBInt(((MaxY - MinY) + 100) / 25);
    if Data.SectorDiv < 1 then
      Data.SectorDiv := 1;
  end
  else
    Data.SectorDiv := 1;

  Data.PropCount := SceneryCount;
  SetLength(Data.Props, SceneryCount);
  for I := 0 to SceneryCount - 1 do
  begin
    FillChar(Prop, SizeOf(Prop), 0);
    Prop.Active := WordBool(-1);
    Prop.Style := Scenery[I].Style;
    Prop.Width := Scenery[I].Width;
    Prop.Height := Scenery[I].Height;
    Prop.X := Scenery[I].X;
    Prop.Y := Scenery[I].Y;
    Prop.Rotation := Scenery[I].Rotation;
    Prop.ScaleX := Scenery[I].ScaleX;
    Prop.ScaleY := Scenery[I].ScaleY;
    Prop.Alpha := Scenery[I].Alpha;
    Prop.Color := Scenery[I].Color;
    Prop.Level := Scenery[I].Level;
    Data.Props[I] := Prop;
  end;

  Data.ScenNameCount := ScenNameCount;
  SetLength(Data.ScenNames, ScenNameCount);
  for I := 0 to ScenNameCount - 1 do
  begin
    FillChar(SN, SizeOf(SN), 0);
    WritePascalStr(SceneryNames[I + 1], SN.Name, 50);
    SN.Date := 0;
    Data.ScenNames[I] := SN;
  end;

  Data.ColliderCount := ColliderCount;
  SetLength(Data.Colliders, ColliderCount);
  for I := 0 to ColliderCount - 1 do
  begin
    FillChar(Data.Colliders[I], SizeOf(TPMSCollider), 0);
    if Colliders[I].Active then
      Data.Colliders[I].Active := 1
    else
      Data.Colliders[I].Active := 0;
    Data.Colliders[I].X := Colliders[I].X;
    Data.Colliders[I].Y := Colliders[I].Y;
    Data.Colliders[I].Radius := Colliders[I].Radius;
  end;

  Data.SpawnCount := SpawnCount;
  SetLength(Data.Spawns, SpawnCount);
  for I := 0 to SpawnCount - 1 do
  begin
    FillChar(SP, SizeOf(SP), 0);
    if Spawns[I].Active then
      SP.Active := 1
    else
      SP.Active := 0;
    SP.X := Round(Spawns[I].X);
    SP.Y := Round(Spawns[I].Y);
    SP.Team := Spawns[I].Team;
    Data.Spawns[I] := SP;
  end;

  Data.WaypointCount := WaypointCount;
  SetLength(Data.Waypoints, WaypointCount);
  for I := 0 to WaypointCount - 1 do
  begin
    FillChar(WP, SizeOf(WP), 0);
    WP.Active := 1;
    WP.ID := I + 1;
    WP.X := Round(Waypoints[I].X);
    WP.Y := Round(Waypoints[I].Y);
    if Waypoints[I].Left then WP.Left := 1;
    if Waypoints[I].Right then WP.Right := 1;
    if Waypoints[I].Up then WP.Up := 1;
    if Waypoints[I].Down then WP.Down := 1;
    if Waypoints[I].M2 then WP.M2 := 1;
    WP.PathNum := Waypoints[I].PathNum;
    WP.Special := Waypoints[I].Special;
    Count := 0;
    for J := 0 to ConnCount - 1 do
      if (Connections[J].Point1 = I + 1) and
         (Connections[J].Point2 >= 1) and (Connections[J].Point2 <= WaypointCount) and
         (Count < MAX_CONNECTIONS_PER_WP) then
      begin
        Inc(Count);
        WP.Connections[Count] := Connections[J].Point2;
      end;
    WP.ConnectionsNum := Count;
    Data.Waypoints[I] := WP;
  end;

  Data.LightCount := LightCount;
  SetLength(Data.Lights, LightCount);
  for I := 0 to LightCount - 1 do
  begin
    FillChar(L, SizeOf(L), 0);
    if Lights[I].Selected then
      L.Selected := 1
    else
      L.Selected := 0;
    L.Color := Lights[I].Color;
    L.Intensity := Lights[I].Intensity;
    L.Range := Round(Lights[I].Range);
    L.X := Lights[I].X;
    L.Y := Lights[I].Y;
    L.Z := Lights[I].Z;
    Data.Lights[I] := L;
  end;

  Data.SketchCount := SketchCount;
  SetLength(Data.Sketch, SketchCount);
  for I := 0 to SketchCount - 1 do
    Data.Sketch[I] := Sketch[I];
end;

procedure TMapDocument.WorldToScreen(WX, WY: Single; out SX, SY: Single);
begin
  pw.geometry.WorldToScreen(WX, WY, ScrollX, ScrollY, Zoom, SX, SY);
end;

procedure TMapDocument.ScreenToWorld(SX, SY: Single; out WX, WY: Single);
begin
  pw.geometry.ScreenToWorld(SX, SY, ScrollX, ScrollY, Zoom, WX, WY);
end;

procedure TMapDocument.RebuildScreenCache;
var
  I, J: Integer;
begin
  for I := 0 to PolyCount - 1 do
    for J := 1 to 3 do
    begin
      pw.geometry.WorldToScreen(Polys[I].V[J].World.X, Polys[I].V[J].World.Y,
        ScrollX, ScrollY, Zoom, Polys[I].V[J].Screen.X, Polys[I].V[J].Screen.Y);
      Polys[I].ScreenV[J].Screen := Polys[I].V[J].Screen;
      if Polys[I].ScreenV[J].Color = 0 then
        Polys[I].ScreenV[J].Color := Color3ToARGB(Polys[I].BaseColor[J], Polys[I].V[J].Alpha);
    end;

  for I := 0 to SceneryCount - 1 do
    pw.geometry.WorldToScreen(Scenery[I].X, Scenery[I].Y,
      ScrollX, ScrollY, Zoom, Scenery[I].ScreenX, Scenery[I].ScreenY);
end;

procedure TMapDocument.SetZoom(NewZoom, CentreScreenX, CentreScreenY: Single);
var
  OldZoom: Single;
  WX, WY: Single;
begin
  NewZoom := Clamp(NewZoom, 0.0625, 16.0);
  OldZoom := Zoom;
  if OldZoom = 0 then
    OldZoom := 1;

  WX := CentreScreenX / OldZoom + ScrollX;
  WY := CentreScreenY / OldZoom + ScrollY;
  Zoom := NewZoom;
  ScrollX := WX - CentreScreenX / NewZoom;
  ScrollY := WY - CentreScreenY / NewZoom;
  RebuildScreenCache;
end;

procedure TMapDocument.Scroll(DX, DY: Single);
begin
  ScrollX := ScrollX + DX;
  ScrollY := ScrollY + DY;
  RebuildScreenCache;
end;

procedure TMapDocument.AddPoly(const WV: array of TEditorVertex; Kind: Byte);
var
  P: TEditorPoly;
  V1, V2, V3: TVector2;
begin
  if Length(WV) <> 3 then
    Exit;

  FillChar(P, SizeOf(P), 0);
  P.V[1] := WV[0];
  P.V[2] := WV[1];
  P.V[3] := WV[2];
  P.PolyType := Kind;

  V1 := P.V[1].World;
  V2 := P.V[2].World;
  V3 := P.V[3].World;
  if not IsPolyClockwise(V1, V2, V3) then
  begin
    P.V[2] := WV[2];
    P.V[3] := WV[1];
  end;

  P.BaseColor[1] := P.V[1].Color;
  P.BaseColor[2] := P.V[2].Color;
  P.BaseColor[3] := P.V[3].Color;
  P.ScreenV[1].Color := Color3ToARGB(P.BaseColor[1], P.V[1].Alpha);
  P.ScreenV[2].Color := Color3ToARGB(P.BaseColor[2], P.V[2].Alpha);
  P.ScreenV[3].Color := Color3ToARGB(P.BaseColor[3], P.V[3].Alpha);

  SetLength(Polys, PolyCount + 1);
  Polys[PolyCount] := P;
  Inc(PolyCount);
  Modified := True;
  RebuildScreenCache;
end;

procedure TMapDocument.DeleteSelectedPolys;
var
  I, NewCount: Integer;
  NewPolys: array of TEditorPoly;
begin
  SetLength(NewPolys, PolyCount);
  NewCount := 0;
  for I := 0 to PolyCount - 1 do
    if not PolyFullySelected(Polys[I]) then
    begin
      NewPolys[NewCount] := Polys[I];
      Inc(NewCount);
    end;

  if NewCount <> PolyCount then
  begin
    SetLength(NewPolys, NewCount);
    Polys := NewPolys;
    PolyCount := NewCount;
    Modified := True;
    RebuildScreenCache;
  end;
end;

procedure TMapDocument.AddScenery(NameIndex: SmallInt; WorldX, WorldY: Single;
  ScaleX, ScaleY: Single; Rotation: Single; Width, Height: LongInt);
var
  S: TEditorScenery;
begin
  FillChar(S, SizeOf(S), 0);
  S.Style := NameIndex;
  S.X := WorldX;
  S.Y := WorldY;
  S.ScaleX := ScaleX;
  S.ScaleY := ScaleY;
  S.Rotation := Rotation;
  S.Width := Width;
  S.Height := Height;
  S.Alpha := 255;
  S.Color := LongInt(MakeARGB(255, 255, 255, 255));
  S.Level := 0;
  SetLength(Scenery, SceneryCount + 1);
  Scenery[SceneryCount] := S;
  Inc(SceneryCount);
  Modified := True;
  RebuildScreenCache;
end;

function TMapDocument.AddSceneryName(const Filename: string): Integer;
var
  I: Integer;
begin
  for I := 1 to ScenNameCount do
    if SameText(SceneryNames[I], Filename) then
      Exit(I);

  SetLength(SceneryNames, ScenNameCount + 2);
  Inc(ScenNameCount);
  SceneryNames[ScenNameCount] := Filename;
  Result := ScenNameCount;
  Modified := True;
end;

procedure TMapDocument.AddSpawn(WorldX, WorldY: Single; Team: Byte);
var
  S: TEditorSpawn;
begin
  FillChar(S, SizeOf(S), 0);
  S.X := WorldX;
  S.Y := WorldY;
  S.Team := Team;
  S.Active := True;
  SetLength(Spawns, SpawnCount + 1);
  Spawns[SpawnCount] := S;
  Inc(SpawnCount);
  Modified := True;
end;

procedure TMapDocument.AddCollider(WorldX, WorldY, Radius: Single);
var
  C: TEditorCollider;
begin
  FillChar(C, SizeOf(C), 0);
  C.X := WorldX;
  C.Y := WorldY;
  C.Radius := Radius;
  C.Active := True;
  SetLength(Colliders, ColliderCount + 1);
  Colliders[ColliderCount] := C;
  Inc(ColliderCount);
  Modified := True;
end;

procedure TMapDocument.AddWaypoint(WorldX, WorldY: Single);
var
  W: TEditorWaypoint;
begin
  FillChar(W, SizeOf(W), 0);
  W.X := WorldX;
  W.Y := WorldY;
  W.ID := WaypointCount + 1;
  W.TempIndex := WaypointCount + 1;
  SetLength(Waypoints, WaypointCount + 1);
  Waypoints[WaypointCount] := W;
  Inc(WaypointCount);
  Modified := True;
end;

procedure TMapDocument.AddConnection(WP1, WP2: Integer);
begin
  if (WP1 < 1) or (WP1 > WaypointCount) or (WP2 < 1) or (WP2 > WaypointCount) then
    Exit;

  SetLength(Connections, ConnCount + 1);
  Connections[ConnCount].Point1 := WP1;
  Connections[ConnCount].Point2 := WP2;
  Inc(ConnCount);
  Inc(Waypoints[WP1 - 1].NumConns);
  Modified := True;
end;

procedure TMapDocument.AddLight(WorldX, WorldY, WorldZ: Single;
  AColor: TColor3; Intensity: Single; Range: SmallInt);
var
  L: TEditorLight;
begin
  FillChar(L, SizeOf(L), 0);
  L.X := WorldX;
  L.Y := WorldY;
  L.Z := WorldZ;
  L.Color := AColor;
  L.Intensity := Intensity;
  L.Range := Range;
  SetLength(Lights, LightCount + 1);
  Lights[LightCount] := L;
  Inc(LightCount);
  Modified := True;
end;

procedure TMapDocument.AddSketchLine(const SL: TPMSSketchLine);
begin
  SetLength(Sketch, SketchCount + 1);
  Sketch[SketchCount] := SL;
  Inc(SketchCount);
  Modified := True;
end;

procedure TMapDocument.ClearSelection;
var
  I, J: Integer;
begin
  for I := 0 to PolyCount - 1 do
    for J := 1 to 3 do
      Polys[I].Selected[J] := False;

  for I := 0 to SceneryCount - 1 do
    Scenery[I].Selected := False;
  for I := 0 to SpawnCount - 1 do
    Spawns[I].Selected := False;
  for I := 0 to ColliderCount - 1 do
    Colliders[I].Selected := False;
  for I := 0 to WaypointCount - 1 do
    Waypoints[I].Selected := False;
  for I := 0 to LightCount - 1 do
    Lights[I].Selected := False;
end;

procedure TMapDocument.SelectPolyVertex(PolyIdx, VertIdx: Integer; Mode: TSelMode);
begin
  if Mode = smReplace then
    ClearSelection;

  if (PolyIdx < 0) or (PolyIdx >= PolyCount) or (VertIdx < 1) or (VertIdx > 3) then
    Exit;

  case Mode of
    smReplace, smAdd:
      Polys[PolyIdx].Selected[VertIdx] := True;
    smSubtract:
      Polys[PolyIdx].Selected[VertIdx] := False;
  end;
end;

procedure TMapDocument.SelectByWorldRect(WX1, WY1, WX2, WY2: Single; Mode: TSelMode);
var
  I, J: Integer;
begin
  if Mode = smReplace then
    ClearSelection;

  for I := 0 to PolyCount - 1 do
    for J := 1 to 3 do
      ApplySelection(Polys[I].Selected[J],
        InSelRect(Polys[I].V[J].World.X, Polys[I].V[J].World.Y, WX1, WY1, WX2, WY2),
        Mode);

  for I := 0 to SceneryCount - 1 do
    ApplySelection(Scenery[I].Selected,
      InSelRect(Scenery[I].X, Scenery[I].Y, WX1, WY1, WX2, WY2), Mode);

  for I := 0 to SpawnCount - 1 do
    ApplySelection(Spawns[I].Selected,
      InSelRect(Spawns[I].X, Spawns[I].Y, WX1, WY1, WX2, WY2), Mode);

  for I := 0 to ColliderCount - 1 do
    ApplySelection(Colliders[I].Selected,
      InSelRect(Colliders[I].X, Colliders[I].Y, WX1, WY1, WX2, WY2), Mode);

  for I := 0 to WaypointCount - 1 do
    ApplySelection(Waypoints[I].Selected,
      InSelRect(Waypoints[I].X, Waypoints[I].Y, WX1, WY1, WX2, WY2), Mode);

  for I := 0 to LightCount - 1 do
    ApplySelection(Lights[I].Selected,
      InSelRect(Lights[I].X, Lights[I].Y, WX1, WY1, WX2, WY2), Mode);
end;

procedure TMapDocument.MoveSelectedWorld(DX, DY: Single);
var
  I, J: Integer;
  Changed: Boolean;
begin
  Changed := False;

  for I := 0 to PolyCount - 1 do
    for J := 1 to 3 do
      if Polys[I].Selected[J] then
      begin
        Polys[I].V[J].World.X := Polys[I].V[J].World.X + DX;
        Polys[I].V[J].World.Y := Polys[I].V[J].World.Y + DY;
        Changed := True;
      end;

  for I := 0 to SceneryCount - 1 do
    if Scenery[I].Selected then
    begin
      Scenery[I].X := Scenery[I].X + DX;
      Scenery[I].Y := Scenery[I].Y + DY;
      Changed := True;
    end;

  for I := 0 to SpawnCount - 1 do
    if Spawns[I].Selected then
    begin
      Spawns[I].X := Spawns[I].X + DX;
      Spawns[I].Y := Spawns[I].Y + DY;
      Changed := True;
    end;

  for I := 0 to ColliderCount - 1 do
    if Colliders[I].Selected then
    begin
      Colliders[I].X := Colliders[I].X + DX;
      Colliders[I].Y := Colliders[I].Y + DY;
      Changed := True;
    end;

  for I := 0 to WaypointCount - 1 do
    if Waypoints[I].Selected then
    begin
      Waypoints[I].X := Waypoints[I].X + DX;
      Waypoints[I].Y := Waypoints[I].Y + DY;
      Changed := True;
    end;

  for I := 0 to LightCount - 1 do
    if Lights[I].Selected then
    begin
      Lights[I].X := Lights[I].X + DX;
      Lights[I].Y := Lights[I].Y + DY;
      Changed := True;
    end;

  if Changed then
  begin
    Modified := True;
    RebuildScreenCache;
  end;
end;

procedure TMapDocument.DeleteSelected;
var
  I, J, NewCount: Integer;
  PolyNew: array of TEditorPoly;
  SceneryNew: array of TEditorScenery;
  SpawnNew: array of TEditorSpawn;
  ColliderNew: array of TEditorCollider;
  WaypointNew: array of TEditorWaypoint;
  LightNew: array of TEditorLight;
  ConnNew: array of TEditorConnection;
  Mapping: array of Integer;
  Changed: Boolean;
begin
  Changed := False;

  SetLength(PolyNew, PolyCount);
  NewCount := 0;
  for I := 0 to PolyCount - 1 do
    if not PolyFullySelected(Polys[I]) then
    begin
      PolyNew[NewCount] := Polys[I];
      Inc(NewCount);
    end
    else
      Changed := True;
  SetLength(PolyNew, NewCount);
  Polys := PolyNew;
  PolyCount := NewCount;

  SetLength(SceneryNew, SceneryCount);
  NewCount := 0;
  for I := 0 to SceneryCount - 1 do
    if not Scenery[I].Selected then
    begin
      SceneryNew[NewCount] := Scenery[I];
      Inc(NewCount);
    end
    else
      Changed := True;
  SetLength(SceneryNew, NewCount);
  Scenery := SceneryNew;
  SceneryCount := NewCount;

  SetLength(SpawnNew, SpawnCount);
  NewCount := 0;
  for I := 0 to SpawnCount - 1 do
    if not Spawns[I].Selected then
    begin
      SpawnNew[NewCount] := Spawns[I];
      Inc(NewCount);
    end
    else
      Changed := True;
  SetLength(SpawnNew, NewCount);
  Spawns := SpawnNew;
  SpawnCount := NewCount;

  SetLength(ColliderNew, ColliderCount);
  NewCount := 0;
  for I := 0 to ColliderCount - 1 do
    if not Colliders[I].Selected then
    begin
      ColliderNew[NewCount] := Colliders[I];
      Inc(NewCount);
    end
    else
      Changed := True;
  SetLength(ColliderNew, NewCount);
  Colliders := ColliderNew;
  ColliderCount := NewCount;

  SetLength(Mapping, WaypointCount + 1);
  SetLength(WaypointNew, WaypointCount);
  NewCount := 0;
  for I := 0 to WaypointCount - 1 do
    if not Waypoints[I].Selected then
    begin
      Mapping[I + 1] := NewCount + 1;
      WaypointNew[NewCount] := Waypoints[I];
      WaypointNew[NewCount].TempIndex := NewCount + 1;
      WaypointNew[NewCount].NumConns := 0;
      Inc(NewCount);
    end
    else
    begin
      Mapping[I + 1] := 0;
      Changed := True;
    end;
  SetLength(WaypointNew, NewCount);

  SetLength(ConnNew, ConnCount);
  J := 0;
  for I := 0 to ConnCount - 1 do
    if (Connections[I].Point1 >= 1) and (Connections[I].Point1 <= WaypointCount) and
       (Connections[I].Point2 >= 1) and (Connections[I].Point2 <= WaypointCount) and
       (Mapping[Connections[I].Point1] > 0) and (Mapping[Connections[I].Point2] > 0) then
    begin
      ConnNew[J].Point1 := Mapping[Connections[I].Point1];
      ConnNew[J].Point2 := Mapping[Connections[I].Point2];
      Inc(J);
    end
    else
      Changed := True;
  SetLength(ConnNew, J);

  for I := 0 to NewCount - 1 do
  begin
    WaypointNew[I].NumConns := 0;
    WaypointNew[I].ID := I + 1;
  end;
  for I := 0 to J - 1 do
    if (ConnNew[I].Point1 >= 1) and (ConnNew[I].Point1 <= NewCount) then
      Inc(WaypointNew[ConnNew[I].Point1 - 1].NumConns);

  Waypoints := WaypointNew;
  WaypointCount := NewCount;
  Connections := ConnNew;
  ConnCount := J;

  SetLength(LightNew, LightCount);
  NewCount := 0;
  for I := 0 to LightCount - 1 do
    if not Lights[I].Selected then
    begin
      LightNew[NewCount] := Lights[I];
      Inc(NewCount);
    end
    else
      Changed := True;
  SetLength(LightNew, NewCount);
  Lights := LightNew;
  LightCount := NewCount;

  if Changed then
  begin
    Modified := True;
    RebuildScreenCache;
  end;
end;

procedure TMapDocument.SetSelectedPolyType(Kind: Byte);
var
  I: Integer;
  Changed: Boolean;
begin
  Changed := False;
  for I := 0 to PolyCount - 1 do
    if PolyAnySelected(Polys[I]) then
    begin
      Polys[I].PolyType := Kind;
      Changed := True;
    end;

  if Changed then
    Modified := True;
end;

procedure TMapDocument.SnapSelectedToGrid(GridSize: Single);
var
  I, J: Integer;
  Changed: Boolean;
begin
  Changed := False;

  for I := 0 to PolyCount - 1 do
    for J := 1 to 3 do
      if Polys[I].Selected[J] then
      begin
        Polys[I].V[J].World.X := SnapToGrid(Polys[I].V[J].World.X, GridSize);
        Polys[I].V[J].World.Y := SnapToGrid(Polys[I].V[J].World.Y, GridSize);
        Changed := True;
      end;

  for I := 0 to SceneryCount - 1 do
    if Scenery[I].Selected then
    begin
      Scenery[I].X := SnapToGrid(Scenery[I].X, GridSize);
      Scenery[I].Y := SnapToGrid(Scenery[I].Y, GridSize);
      Changed := True;
    end;

  for I := 0 to SpawnCount - 1 do
    if Spawns[I].Selected then
    begin
      Spawns[I].X := SnapToGrid(Spawns[I].X, GridSize);
      Spawns[I].Y := SnapToGrid(Spawns[I].Y, GridSize);
      Changed := True;
    end;

  for I := 0 to ColliderCount - 1 do
    if Colliders[I].Selected then
    begin
      Colliders[I].X := SnapToGrid(Colliders[I].X, GridSize);
      Colliders[I].Y := SnapToGrid(Colliders[I].Y, GridSize);
      Changed := True;
    end;

  for I := 0 to WaypointCount - 1 do
    if Waypoints[I].Selected then
    begin
      Waypoints[I].X := SnapToGrid(Waypoints[I].X, GridSize);
      Waypoints[I].Y := SnapToGrid(Waypoints[I].Y, GridSize);
      Changed := True;
    end;

  for I := 0 to LightCount - 1 do
    if Lights[I].Selected then
    begin
      Lights[I].X := SnapToGrid(Lights[I].X, GridSize);
      Lights[I].Y := SnapToGrid(Lights[I].Y, GridSize);
      Changed := True;
    end;

  if Changed then
  begin
    Modified := True;
    RebuildScreenCache;
  end;
end;

procedure TMapDocument.DuplicateSelected(OffsetX, OffsetY: Single);
var
  I, J, OldPolyCount, OldSceneryCount: Integer;
  NewPoly: TEditorPoly;
  NewScen: TEditorScenery;
begin
  OldPolyCount    := PolyCount;
  OldSceneryCount := SceneryCount;

  for I := 0 to OldPolyCount - 1 do
    if PolyAnySelected(Polys[I]) then
    begin
      NewPoly := Polys[I];
      for J := 1 to 3 do
      begin
        NewPoly.V[J].World.X := NewPoly.V[J].World.X + OffsetX;
        NewPoly.V[J].World.Y := NewPoly.V[J].World.Y + OffsetY;
      end;
      if PolyCount >= Length(Polys) then
        SetLength(Polys, Max(8, PolyCount * 2));
      Polys[PolyCount] := NewPoly;
      Inc(PolyCount);
    end;

  for I := 0 to OldSceneryCount - 1 do
    if Scenery[I].Selected then
    begin
      NewScen := Scenery[I];
      NewScen.X := NewScen.X + OffsetX;
      NewScen.Y := NewScen.Y + OffsetY;
      if SceneryCount >= Length(Scenery) then
        SetLength(Scenery, Max(8, SceneryCount * 2));
      Scenery[SceneryCount] := NewScen;
      Inc(SceneryCount);
    end;

  { Clear original selection; select only the new duplicates }
  for I := 0 to OldPolyCount - 1 do
    for J := 1 to 3 do
      Polys[I].Selected[J] := False;
  for I := 0 to OldSceneryCount - 1 do
    Scenery[I].Selected := False;

  Modified := True;
  RebuildScreenCache;
end;

procedure TMapDocument.CopySelected;
begin
  { Clipboard copy — not yet implemented }
end;

procedure TMapDocument.PasteClipboard;
begin
  { Clipboard paste — not yet implemented }
end;

procedure TMapDocument.InvertSelection;
var
  I, J: Integer;
begin
  for I := 0 to PolyCount - 1 do
    for J := 1 to 3 do
      Polys[I].Selected[J] := not Polys[I].Selected[J];
  for I := 0 to SceneryCount - 1 do
    Scenery[I].Selected := not Scenery[I].Selected;
  for I := 0 to SpawnCount - 1 do
    Spawns[I].Selected := not Spawns[I].Selected;
  for I := 0 to ColliderCount - 1 do
    Colliders[I].Selected := not Colliders[I].Selected;
  for I := 0 to WaypointCount - 1 do
    Waypoints[I].Selected := not Waypoints[I].Selected;
  for I := 0 to LightCount - 1 do
    Lights[I].Selected := not Lights[I].Selected;
end;

procedure TMapDocument.SelectByColor;
begin
  { Select all vertices matching the current painting color — not yet implemented }
end;

end.
