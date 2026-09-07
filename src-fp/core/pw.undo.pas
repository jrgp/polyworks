unit pw.undo;

{$mode objfpc}{$H+}
{$PackRecords 1}

interface

uses
  SysUtils, pw.types, pw.map;

type
  { A deep-copy snapshot of mutable document state.
    NOTE: Sketch lines are intentionally excluded (replicates VB6 bug).
    NOTE: Map options (TPMSOptions) are intentionally excluded (matches VB6). }
  TMapSnapshot = record
    Polys      : array of TEditorPoly;
    Scenery    : array of TEditorScenery;
    Spawns     : array of TEditorSpawn;
    Colliders  : array of TEditorCollider;
    Waypoints  : array of TEditorWaypoint;
    Connections: array of TEditorConnection;
    Lights     : array of TEditorLight;
    PolyCount, SceneryCount, SpawnCount,
    ColliderCount, WaypointCount, ConnCount,
    LightCount : Integer;
    { Zoom and scroll are NOT included (matches VB6) }
  end;

  TUndoStack = class
  private
    FRing    : array of TMapSnapshot;
    FRedo    : array of TMapSnapshot;
    FBase    : Integer;   { index of oldest undo snapshot }
    FCount   : Integer;   { how many undo snapshots are stored }
    FPos     : Integer;   { redo depth; 0 = at top }
    FMaxDepth: Integer;
    procedure SnapshotDoc(const Doc: TMapDocument; out Snap: TMapSnapshot);
    procedure ApplySnapshot(const Snap: TMapSnapshot; Doc: TMapDocument);
    procedure PushUndoSnapshot(const Snap: TMapSnapshot);
    function  PopUndoSnapshot(out Snap: TMapSnapshot): Boolean;
    procedure PushRedoSnapshot(const Snap: TMapSnapshot);
    function  PopRedoSnapshot(out Snap: TMapSnapshot): Boolean;
    procedure ClearRedo;
    function  GetCanUndo: Boolean;
    function  GetCanRedo: Boolean;
    function  GetUndoDepth: Integer;
    function  GetRedoDepth: Integer;
  public
    constructor Create(MaxDepth: Integer = 16);
    destructor Destroy; override;
    { Call before any destructive edit }
    procedure Push(Doc: TMapDocument);
    { Returns True and restores state if possible }
    function  Undo(Doc: TMapDocument): Boolean;
    function  Redo(Doc: TMapDocument): Boolean;
    procedure Clear;
    property  CanUndo: Boolean read GetCanUndo;
    property  CanRedo: Boolean read GetCanRedo;
    property  UndoDepth: Integer read GetUndoDepth;
    property  RedoDepth: Integer read GetRedoDepth;
  end;

implementation

procedure ClearSnapshot(var Snap: TMapSnapshot);
begin
  SetLength(Snap.Polys, 0);
  SetLength(Snap.Scenery, 0);
  SetLength(Snap.Spawns, 0);
  SetLength(Snap.Colliders, 0);
  SetLength(Snap.Waypoints, 0);
  SetLength(Snap.Connections, 0);
  SetLength(Snap.Lights, 0);
  Snap.PolyCount := 0;
  Snap.SceneryCount := 0;
  Snap.SpawnCount := 0;
  Snap.ColliderCount := 0;
  Snap.WaypointCount := 0;
  Snap.ConnCount := 0;
  Snap.LightCount := 0;
end;

constructor TUndoStack.Create(MaxDepth: Integer);
begin
  inherited Create;
  if MaxDepth < 1 then
    FMaxDepth := 1
  else
    FMaxDepth := MaxDepth;
  SetLength(FRing, FMaxDepth);
  FBase := 0;
  FCount := 0;
  FPos := 0;
  SetLength(FRedo, 0);
end;

destructor TUndoStack.Destroy;
begin
  Clear;
  SetLength(FRing, 0);
  inherited Destroy;
end;

procedure TUndoStack.SnapshotDoc(const Doc: TMapDocument; out Snap: TMapSnapshot);
begin
  ClearSnapshot(Snap);

  Snap.PolyCount := Doc.PolyCount;
  Snap.SceneryCount := Doc.SceneryCount;
  Snap.SpawnCount := Doc.SpawnCount;
  Snap.ColliderCount := Doc.ColliderCount;
  Snap.WaypointCount := Doc.WaypointCount;
  Snap.ConnCount := Doc.ConnCount;
  Snap.LightCount := Doc.LightCount;

  Snap.Polys := Copy(Doc.Polys, 0, Doc.PolyCount);
  Snap.Scenery := Copy(Doc.Scenery, 0, Doc.SceneryCount);
  Snap.Spawns := Copy(Doc.Spawns, 0, Doc.SpawnCount);
  Snap.Colliders := Copy(Doc.Colliders, 0, Doc.ColliderCount);
  Snap.Waypoints := Copy(Doc.Waypoints, 0, Doc.WaypointCount);
  Snap.Connections := Copy(Doc.Connections, 0, Doc.ConnCount);
  Snap.Lights := Copy(Doc.Lights, 0, Doc.LightCount);
end;

procedure TUndoStack.ApplySnapshot(const Snap: TMapSnapshot; Doc: TMapDocument);
begin
  Doc.PolyCount := Snap.PolyCount;
  Doc.SceneryCount := Snap.SceneryCount;
  Doc.SpawnCount := Snap.SpawnCount;
  Doc.ColliderCount := Snap.ColliderCount;
  Doc.WaypointCount := Snap.WaypointCount;
  Doc.ConnCount := Snap.ConnCount;
  Doc.LightCount := Snap.LightCount;

  Doc.Polys := Copy(Snap.Polys, 0, Snap.PolyCount);
  Doc.Scenery := Copy(Snap.Scenery, 0, Snap.SceneryCount);
  Doc.Spawns := Copy(Snap.Spawns, 0, Snap.SpawnCount);
  Doc.Colliders := Copy(Snap.Colliders, 0, Snap.ColliderCount);
  Doc.Waypoints := Copy(Snap.Waypoints, 0, Snap.WaypointCount);
  Doc.Connections := Copy(Snap.Connections, 0, Snap.ConnCount);
  Doc.Lights := Copy(Snap.Lights, 0, Snap.LightCount);

  Doc.RebuildScreenCache;
  Doc.Modified := True;
end;

procedure TUndoStack.PushUndoSnapshot(const Snap: TMapSnapshot);
var
  Idx: Integer;
begin
  if FMaxDepth < 1 then
    Exit;

  if FCount < FMaxDepth then
  begin
    Idx := (FBase + FCount) mod FMaxDepth;
    Inc(FCount);
  end
  else
  begin
    Idx := FBase;
    FBase := (FBase + 1) mod FMaxDepth;
  end;

  FRing[Idx] := Snap;
end;

function TUndoStack.PopUndoSnapshot(out Snap: TMapSnapshot): Boolean;
var
  Idx: Integer;
begin
  Result := FCount > 0;
  if not Result then
    Exit;

  Idx := (FBase + FCount - 1) mod FMaxDepth;
  Snap := FRing[Idx];
  ClearSnapshot(FRing[Idx]);
  Dec(FCount);
  if FCount = 0 then
    FBase := 0;
end;

procedure TUndoStack.PushRedoSnapshot(const Snap: TMapSnapshot);
var
  N: Integer;
begin
  N := Length(FRedo);
  SetLength(FRedo, N + 1);
  FRedo[N] := Snap;
  FPos := Length(FRedo);
end;

function TUndoStack.PopRedoSnapshot(out Snap: TMapSnapshot): Boolean;
var
  N: Integer;
begin
  N := Length(FRedo);
  Result := N > 0;
  if not Result then
    Exit;

  Snap := FRedo[N - 1];
  ClearSnapshot(FRedo[N - 1]);
  SetLength(FRedo, N - 1);
  FPos := Length(FRedo);
end;

procedure TUndoStack.ClearRedo;
begin
  SetLength(FRedo, 0);
  FPos := 0;
end;

function TUndoStack.GetCanUndo: Boolean;
begin
  Result := FCount > 0;
end;

function TUndoStack.GetCanRedo: Boolean;
begin
  Result := FPos > 0;
end;

function TUndoStack.GetUndoDepth: Integer;
begin
  Result := FCount;
end;

function TUndoStack.GetRedoDepth: Integer;
begin
  Result := FPos;
end;

procedure TUndoStack.Push(Doc: TMapDocument);
var
  Snap: TMapSnapshot;
begin
  if Doc = nil then
    Exit;

  ClearRedo;
  SnapshotDoc(Doc, Snap);
  PushUndoSnapshot(Snap);
end;

function TUndoStack.Undo(Doc: TMapDocument): Boolean;
var
  CurrentSnap, UndoSnap: TMapSnapshot;
begin
  Result := False;
  if (Doc = nil) or not PopUndoSnapshot(UndoSnap) then
    Exit;

  SnapshotDoc(Doc, CurrentSnap);
  PushRedoSnapshot(CurrentSnap);
  ApplySnapshot(UndoSnap, Doc);
  Result := True;
end;

function TUndoStack.Redo(Doc: TMapDocument): Boolean;
var
  CurrentSnap, RedoSnap: TMapSnapshot;
begin
  Result := False;
  if (Doc = nil) or not PopRedoSnapshot(RedoSnap) then
    Exit;

  SnapshotDoc(Doc, CurrentSnap);
  PushUndoSnapshot(CurrentSnap);
  ApplySnapshot(RedoSnap, Doc);
  Result := True;
end;

procedure TUndoStack.Clear;
var
  I: Integer;
begin
  for I := 0 to Length(FRing) - 1 do
    ClearSnapshot(FRing[I]);
  FBase := 0;
  FCount := 0;
  ClearRedo;
end;

end.
