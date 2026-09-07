unit tools;

{$mode objfpc}{$H+}

interface

uses
  Classes, SysUtils, Math, Controls, GL,
  pw.types, pw.map, pw.undo, pw.geometry;

const
  TOOL_SELECT     = 0;
  TOOL_POLY       = 1;
  TOOL_SCENERY    = 2;
  TOOL_SPAWN      = 3;
  TOOL_WAYPOINT   = 4;
  TOOL_CONNECTION = 5;
  TOOL_COLLIDER   = 6;
  TOOL_LIGHT      = 7;
  TOOL_SKETCH     = 8;

  DEFAULT_HIT_PIXELS = 8.0;
  DEFAULT_LIGHT_RANGE = 300;

type
  ITool = interface
    ['{F1B5E74E-9C40-4A0D-92C1-A7893E51D67F}']
    procedure MouseDown(Doc: TMapDocument; UndoStack: TUndoStack;
                        WX, WY: Single; Button: TMouseButton;
                        Shift: TShiftState; Repaint: TNotifyEvent);
    procedure MouseMove(Doc: TMapDocument; WX, WY: Single;
                        Shift: TShiftState; Repaint: TNotifyEvent);
    procedure MouseUp(Doc: TMapDocument; WX, WY: Single;
                      Button: TMouseButton; Repaint: TNotifyEvent);
    procedure Cancel(Doc: TMapDocument);
    function CursorForState: TCursor;
  end;

  IToolOverlay = interface
    ['{8A0C8C20-C34E-4B4E-9A98-A8A1BB678B14}']
    procedure RenderOverlay(Doc: TMapDocument; ViewW, ViewH: Integer);
  end;

type
  TSceneryToolProvider = function(out StyleIndex, Width, Height: Integer): Boolean of object;

function CreateToolByID(ToolID: Integer): ITool;
procedure SetSceneryToolProvider(AProvider: TSceneryToolProvider);
procedure ClearSceneryToolProvider;

implementation

var
  GSceneryToolProvider: TSceneryToolProvider = nil;

type
  THitKind = (hkNone, hkPolyVertex, hkPolygon, hkScenery, hkSpawn,
              hkCollider, hkWaypoint, hkLight);

  TBaseTool = class(TInterfacedObject, ITool)
  protected
    procedure DoRepaint(Repaint: TNotifyEvent);
    function SelectModeFromShift(Shift: TShiftState): TSelMode;
    function WorldHitRadius(const Doc: TMapDocument): Single;
  public
    procedure MouseDown(Doc: TMapDocument; UndoStack: TUndoStack;
                        WX, WY: Single; Button: TMouseButton;
                        Shift: TShiftState; Repaint: TNotifyEvent); virtual; abstract;
    procedure MouseMove(Doc: TMapDocument; WX, WY: Single;
                        Shift: TShiftState; Repaint: TNotifyEvent); virtual;
    procedure MouseUp(Doc: TMapDocument; WX, WY: Single;
                      Button: TMouseButton; Repaint: TNotifyEvent); virtual;
    procedure Cancel(Doc: TMapDocument); virtual;
    function CursorForState: TCursor; virtual;
  end;

  TSelectTool = class(TBaseTool, ITool, IToolOverlay)
  private
    FDragging: Boolean;
    FDragged: Boolean;
    FStartWorld: TVector2;
    FCurrentWorld: TVector2;
    FSelectMode: TSelMode;
    procedure SetPolySelection(var P: TEditorPoly; Mode: TSelMode);
    function TryHitVertexOrObject(Doc: TMapDocument; WX, WY, Tol: Single;
      out HitKind: THitKind; out Index1, Index2: Integer): Boolean;
    function TryHitPolygonBody(Doc: TMapDocument; WX, WY: Single; out PolyIdx: Integer): Boolean;
    procedure SelectAtPoint(Doc: TMapDocument; WX, WY: Single; Mode: TSelMode);
  public
    procedure MouseDown(Doc: TMapDocument; UndoStack: TUndoStack;
                        WX, WY: Single; Button: TMouseButton;
                        Shift: TShiftState; Repaint: TNotifyEvent); override;
    procedure MouseMove(Doc: TMapDocument; WX, WY: Single;
                        Shift: TShiftState; Repaint: TNotifyEvent); override;
    procedure MouseUp(Doc: TMapDocument; WX, WY: Single;
                      Button: TMouseButton; Repaint: TNotifyEvent); override;
    procedure Cancel(Doc: TMapDocument); override;
    function CursorForState: TCursor; override;
    procedure RenderOverlay(Doc: TMapDocument; ViewW, ViewH: Integer);
  end;

  TPolyTool = class(TBaseTool, ITool, IToolOverlay)
  private
    FVertexCount: Integer;
    FVertices: array[0..2] of TEditorVertex;
    FPreviewWorld: TVector2;
    FHasPreview: Boolean;
    procedure ResetState;
    function MakeVertex(Doc: TMapDocument; WX, WY: Single): TEditorVertex;
  public
    procedure MouseDown(Doc: TMapDocument; UndoStack: TUndoStack;
                        WX, WY: Single; Button: TMouseButton;
                        Shift: TShiftState; Repaint: TNotifyEvent); override;
    procedure MouseMove(Doc: TMapDocument; WX, WY: Single;
                        Shift: TShiftState; Repaint: TNotifyEvent); override;
    procedure MouseUp(Doc: TMapDocument; WX, WY: Single;
                      Button: TMouseButton; Repaint: TNotifyEvent); override;
    procedure Cancel(Doc: TMapDocument); override;
    function CursorForState: TCursor; override;
    procedure RenderOverlay(Doc: TMapDocument; ViewW, ViewH: Integer);
  end;

  TSceneryTool = class(TBaseTool)
  public
    procedure MouseDown(Doc: TMapDocument; UndoStack: TUndoStack;
                        WX, WY: Single; Button: TMouseButton;
                        Shift: TShiftState; Repaint: TNotifyEvent); override;
    function CursorForState: TCursor; override;
  end;

  TSpawnTool = class(TBaseTool)
  public
    procedure MouseDown(Doc: TMapDocument; UndoStack: TUndoStack;
                        WX, WY: Single; Button: TMouseButton;
                        Shift: TShiftState; Repaint: TNotifyEvent); override;
    function CursorForState: TCursor; override;
  end;

  TWaypointTool = class(TBaseTool)
  public
    procedure MouseDown(Doc: TMapDocument; UndoStack: TUndoStack;
                        WX, WY: Single; Button: TMouseButton;
                        Shift: TShiftState; Repaint: TNotifyEvent); override;
    function CursorForState: TCursor; override;
  end;

  TColliderTool = class(TBaseTool)
  public
    procedure MouseDown(Doc: TMapDocument; UndoStack: TUndoStack;
                        WX, WY: Single; Button: TMouseButton;
                        Shift: TShiftState; Repaint: TNotifyEvent); override;
    function CursorForState: TCursor; override;
  end;

  TLightTool = class(TBaseTool)
  public
    procedure MouseDown(Doc: TMapDocument; UndoStack: TUndoStack;
                        WX, WY: Single; Button: TMouseButton;
                        Shift: TShiftState; Repaint: TNotifyEvent); override;
    function CursorForState: TCursor; override;
  end;

  TSketchTool = class(TBaseTool, ITool, IToolOverlay)
  private
    FDrawing: Boolean;
    FUndoPushed: Boolean;
    FSketchUndo: TUndoStack;
    FSketchDoc: TMapDocument;
    FStartWorld: TVector2;
    FCurrentWorld: TVector2;
  public
    procedure MouseDown(Doc: TMapDocument; UndoStack: TUndoStack;
                        WX, WY: Single; Button: TMouseButton;
                        Shift: TShiftState; Repaint: TNotifyEvent); override;
    procedure MouseMove(Doc: TMapDocument; WX, WY: Single;
                        Shift: TShiftState; Repaint: TNotifyEvent); override;
    procedure MouseUp(Doc: TMapDocument; WX, WY: Single;
                      Button: TMouseButton; Repaint: TNotifyEvent); override;
    procedure Cancel(Doc: TMapDocument); override;
    function CursorForState: TCursor; override;
    procedure RenderOverlay(Doc: TMapDocument; ViewW, ViewH: Integer);
  end;

procedure DrawDashedRect(X1, Y1, X2, Y2: Single);
begin
  glDisable(GL_TEXTURE_2D);
  glColor4f(1.0, 1.0, 1.0, 1.0);
  glLineWidth(1.0);
  glEnable(GL_LINE_STIPPLE);
  glLineStipple(1, $00FF);
  glBegin(GL_LINE_LOOP);
    glVertex2f(X1, Y1);
    glVertex2f(X2, Y1);
    glVertex2f(X2, Y2);
    glVertex2f(X1, Y2);
  glEnd;
  glDisable(GL_LINE_STIPPLE);
end;

procedure DrawDashedLine(X1, Y1, X2, Y2: Single);
begin
  glDisable(GL_TEXTURE_2D);
  glColor4f(1.0, 1.0, 1.0, 1.0);
  glLineWidth(1.0);
  glEnable(GL_LINE_STIPPLE);
  glLineStipple(1, $00FF);
  glBegin(GL_LINES);
    glVertex2f(X1, Y1);
    glVertex2f(X2, Y2);
  glEnd;
  glDisable(GL_LINE_STIPPLE);
end;

function CreateToolByID(ToolID: Integer): ITool;
begin
  case ToolID of
    TOOL_SELECT:   Result := TSelectTool.Create;
    TOOL_POLY:     Result := TPolyTool.Create;
    TOOL_SCENERY:  Result := TSceneryTool.Create;
    TOOL_SPAWN:    Result := TSpawnTool.Create;
    TOOL_WAYPOINT: Result := TWaypointTool.Create;
    TOOL_COLLIDER: Result := TColliderTool.Create;
    TOOL_LIGHT:    Result := TLightTool.Create;
    TOOL_SKETCH:   Result := TSketchTool.Create;
  else
    Result := nil;
  end;
end;

procedure TBaseTool.DoRepaint(Repaint: TNotifyEvent);
begin
  if Assigned(Repaint) then
    Repaint(nil);
end;

function TBaseTool.SelectModeFromShift(Shift: TShiftState): TSelMode;
begin
  if ssCtrl in Shift then
    Result := smSubtract
  else if ssShift in Shift then
    Result := smAdd
  else
    Result := smReplace;
end;

function TBaseTool.WorldHitRadius(const Doc: TMapDocument): Single;
var
  Zoom: Single;
begin
  Zoom := 1.0;
  if (Doc <> nil) and (Doc.Zoom > 0.0001) then
    Zoom := Doc.Zoom;
  Result := DEFAULT_HIT_PIXELS / Zoom;
end;

procedure TBaseTool.MouseMove(Doc: TMapDocument; WX, WY: Single;
  Shift: TShiftState; Repaint: TNotifyEvent);
begin
end;

procedure TBaseTool.MouseUp(Doc: TMapDocument; WX, WY: Single;
  Button: TMouseButton; Repaint: TNotifyEvent);
begin
end;

procedure TBaseTool.Cancel(Doc: TMapDocument);
begin
end;

function TBaseTool.CursorForState: TCursor;
begin
  Result := crDefault;
end;

procedure TSelectTool.SetPolySelection(var P: TEditorPoly; Mode: TSelMode);
var
  J: Integer;
begin
  for J := 1 to 3 do
    case Mode of
      smReplace, smAdd:
        P.Selected[J] := True;
      smSubtract:
        P.Selected[J] := False;
    end;
end;

function TSelectTool.TryHitVertexOrObject(Doc: TMapDocument; WX, WY, Tol: Single;
  out HitKind: THitKind; out Index1, Index2: Integer): Boolean;
var
  I, J: Integer;
  Dist2, BestDist2: Single;
  DX, DY: Single;
  procedure Consider(AKind: THitKind; AIndex1, AIndex2: Integer; AX, AY: Single);
  begin
    DX := AX - WX;
    DY := AY - WY;
    Dist2 := DX * DX + DY * DY;
    if Dist2 <= BestDist2 then
    begin
      BestDist2 := Dist2;
      HitKind := AKind;
      Index1 := AIndex1;
      Index2 := AIndex2;
    end;
  end;
begin
  HitKind := hkNone;
  Index1 := -1;
  Index2 := -1;
  BestDist2 := Tol * Tol;

  for I := Doc.PolyCount - 1 downto 0 do
    for J := 3 downto 1 do
      Consider(hkPolyVertex, I, J, Doc.Polys[I].V[J].World.X, Doc.Polys[I].V[J].World.Y);

  for I := Doc.SceneryCount - 1 downto 0 do
    Consider(hkScenery, I, 0, Doc.Scenery[I].X, Doc.Scenery[I].Y);

  for I := Doc.SpawnCount - 1 downto 0 do
    Consider(hkSpawn, I, 0, Doc.Spawns[I].X, Doc.Spawns[I].Y);

  for I := Doc.ColliderCount - 1 downto 0 do
    Consider(hkCollider, I, 0, Doc.Colliders[I].X, Doc.Colliders[I].Y);

  for I := Doc.WaypointCount - 1 downto 0 do
    Consider(hkWaypoint, I, 0, Doc.Waypoints[I].X, Doc.Waypoints[I].Y);

  for I := Doc.LightCount - 1 downto 0 do
    Consider(hkLight, I, 0, Doc.Lights[I].X, Doc.Lights[I].Y);

  Result := HitKind <> hkNone;
end;

function TSelectTool.TryHitPolygonBody(Doc: TMapDocument; WX, WY: Single; out PolyIdx: Integer): Boolean;
var
  I: Integer;
begin
  PolyIdx := -1;
  for I := Doc.PolyCount - 1 downto 0 do
    if PointInPoly(WX, WY,
      Doc.Polys[I].V[1].World,
      Doc.Polys[I].V[2].World,
      Doc.Polys[I].V[3].World) then
    begin
      PolyIdx := I;
      Break;
    end;
  Result := PolyIdx >= 0;
end;

procedure TSelectTool.SelectAtPoint(Doc: TMapDocument; WX, WY: Single; Mode: TSelMode);
var
  HitKind: THitKind;
  Index1, Index2: Integer;
  Tol: Single;
begin
  if Doc = nil then
    Exit;

  Tol := WorldHitRadius(Doc);
  if Mode = smReplace then
    Doc.ClearSelection;

  if TryHitVertexOrObject(Doc, WX, WY, Tol, HitKind, Index1, Index2) then
  begin
    case HitKind of
      hkPolyVertex:
        Doc.SelectPolyVertex(Index1, Index2, Mode);
      hkScenery:
        case Mode of
          smReplace, smAdd: Doc.Scenery[Index1].Selected := True;
          smSubtract: Doc.Scenery[Index1].Selected := False;
        end;
      hkSpawn:
        case Mode of
          smReplace, smAdd: Doc.Spawns[Index1].Selected := True;
          smSubtract: Doc.Spawns[Index1].Selected := False;
        end;
      hkCollider:
        case Mode of
          smReplace, smAdd: Doc.Colliders[Index1].Selected := True;
          smSubtract: Doc.Colliders[Index1].Selected := False;
        end;
      hkWaypoint:
        case Mode of
          smReplace, smAdd: Doc.Waypoints[Index1].Selected := True;
          smSubtract: Doc.Waypoints[Index1].Selected := False;
        end;
      hkLight:
        case Mode of
          smReplace, smAdd: Doc.Lights[Index1].Selected := True;
          smSubtract: Doc.Lights[Index1].Selected := False;
        end;
    end;
    Exit;
  end;

  if TryHitPolygonBody(Doc, WX, WY, Index1) then
    SetPolySelection(Doc.Polys[Index1], Mode);
end;

procedure TSelectTool.MouseDown(Doc: TMapDocument; UndoStack: TUndoStack;
  WX, WY: Single; Button: TMouseButton; Shift: TShiftState; Repaint: TNotifyEvent);
begin
  if (Doc = nil) or (Button <> mbLeft) then
    Exit;

  FDragging := True;
  FDragged := False;
  FSelectMode := SelectModeFromShift(Shift);
  FStartWorld.X := WX;
  FStartWorld.Y := WY;
  FCurrentWorld := FStartWorld;
  DoRepaint(Repaint);
end;

procedure TSelectTool.MouseMove(Doc: TMapDocument; WX, WY: Single;
  Shift: TShiftState; Repaint: TNotifyEvent);
var
  Tol: Single;
begin
  if not FDragging then
    Exit;

  FCurrentWorld.X := WX;
  FCurrentWorld.Y := WY;
  Tol := 0.0;
  if Doc <> nil then
    Tol := 2.0 / Max(Doc.Zoom, 0.001);
  if (Abs(FCurrentWorld.X - FStartWorld.X) > Tol) or
     (Abs(FCurrentWorld.Y - FStartWorld.Y) > Tol) then
    FDragged := True;
  DoRepaint(Repaint);
end;

procedure TSelectTool.MouseUp(Doc: TMapDocument; WX, WY: Single;
  Button: TMouseButton; Repaint: TNotifyEvent);
begin
  if (Doc = nil) or (Button <> mbLeft) or not FDragging then
    Exit;

  FCurrentWorld.X := WX;
  FCurrentWorld.Y := WY;
  if FDragged then
    Doc.SelectByWorldRect(FStartWorld.X, FStartWorld.Y, FCurrentWorld.X, FCurrentWorld.Y, FSelectMode)
  else
    SelectAtPoint(Doc, WX, WY, FSelectMode);

  FDragging := False;
  FDragged := False;
  DoRepaint(Repaint);
end;

procedure TSelectTool.Cancel(Doc: TMapDocument);
begin
  FDragging := False;
  FDragged := False;
end;

function TSelectTool.CursorForState: TCursor;
begin
  Result := crDefault;
end;

procedure TSelectTool.RenderOverlay(Doc: TMapDocument; ViewW, ViewH: Integer);
var
  SX1, SY1, SX2, SY2: Single;
begin
  if (Doc = nil) or not FDragging or not FDragged then
    Exit;
  Doc.WorldToScreen(FStartWorld.X, FStartWorld.Y, SX1, SY1);
  Doc.WorldToScreen(FCurrentWorld.X, FCurrentWorld.Y, SX2, SY2);
  DrawDashedRect(SX1, SY1, SX2, SY2);
end;

procedure TPolyTool.ResetState;
begin
  FVertexCount := 0;
  FHasPreview := False;
  FillChar(FVertices, SizeOf(FVertices), 0);
end;

function TPolyTool.MakeVertex(Doc: TMapDocument; WX, WY: Single): TEditorVertex;
begin
  FillChar(Result, SizeOf(Result), 0);
  Result.World.X := WX;
  Result.World.Y := WY;
  if Doc <> nil then
    Doc.WorldToScreen(WX, WY, Result.Screen.X, Result.Screen.Y);
  Result.Color.R := 255;
  Result.Color.G := 255;
  Result.Color.B := 255;
  Result.Alpha := 255;
  Result.Tu := WX;
  Result.Tv := WY;
end;

procedure TPolyTool.MouseDown(Doc: TMapDocument; UndoStack: TUndoStack;
  WX, WY: Single; Button: TMouseButton; Shift: TShiftState; Repaint: TNotifyEvent);
begin
  if (Doc = nil) or (Button <> mbLeft) then
    Exit;

  if FVertexCount < 3 then
  begin
    FVertices[FVertexCount] := MakeVertex(Doc, WX, WY);
    Inc(FVertexCount);
    FPreviewWorld.X := WX;
    FPreviewWorld.Y := WY;
    FHasPreview := True;
  end;

  if FVertexCount = 3 then
  begin
    if UndoStack <> nil then
      UndoStack.Push(Doc);
    Doc.AddPoly(FVertices, POLY_TYPE_NORMAL);
    ResetState;
  end;

  DoRepaint(Repaint);
end;

procedure TPolyTool.MouseMove(Doc: TMapDocument; WX, WY: Single;
  Shift: TShiftState; Repaint: TNotifyEvent);
begin
  if FVertexCount = 0 then
    Exit;
  FPreviewWorld.X := WX;
  FPreviewWorld.Y := WY;
  FHasPreview := True;
  DoRepaint(Repaint);
end;

procedure TPolyTool.MouseUp(Doc: TMapDocument; WX, WY: Single;
  Button: TMouseButton; Repaint: TNotifyEvent);
begin
end;

procedure TPolyTool.Cancel(Doc: TMapDocument);
begin
  ResetState;
end;

function TPolyTool.CursorForState: TCursor;
begin
  Result := crCross;
end;

procedure TPolyTool.RenderOverlay(Doc: TMapDocument; ViewW, ViewH: Integer);
var
  SX, SY, PX, PY: Single;
  I: Integer;
begin
  if (Doc = nil) or (FVertexCount <= 0) or not FHasPreview then
    Exit;

  glDisable(GL_TEXTURE_2D);
  glColor4f(1.0, 1.0, 1.0, 1.0);
  glLineWidth(1.0);
  glEnable(GL_LINE_STIPPLE);
  glLineStipple(1, $00FF);

  glBegin(GL_LINE_STRIP);
  for I := 0 to FVertexCount - 1 do
  begin
    Doc.WorldToScreen(FVertices[I].World.X, FVertices[I].World.Y, SX, SY);
    glVertex2f(SX, SY);
  end;
  Doc.WorldToScreen(FPreviewWorld.X, FPreviewWorld.Y, PX, PY);
  glVertex2f(PX, PY);
  glEnd;

  if FVertexCount >= 2 then
  begin
    Doc.WorldToScreen(FVertices[0].World.X, FVertices[0].World.Y, SX, SY);
    DrawDashedLine(PX, PY, SX, SY);
  end;

  glDisable(GL_LINE_STIPPLE);
end;

procedure SetSceneryToolProvider(AProvider: TSceneryToolProvider);
begin
  GSceneryToolProvider := AProvider;
end;

procedure ClearSceneryToolProvider;
begin
  GSceneryToolProvider := nil;
end;

procedure TSceneryTool.MouseDown(Doc: TMapDocument; UndoStack: TUndoStack;
  WX, WY: Single; Button: TMouseButton; Shift: TShiftState; Repaint: TNotifyEvent);
var
  StyleIndex: Integer;
  ItemWidth: Integer;
  ItemHeight: Integer;
begin
  if (Doc = nil) or (Button <> mbLeft) or (not Assigned(GSceneryToolProvider)) then
    Exit;
  if not GSceneryToolProvider(StyleIndex, ItemWidth, ItemHeight) then
    Exit;
  if UndoStack <> nil then
    UndoStack.Push(Doc);
  if ItemWidth <= 0 then
    ItemWidth := 64;
  if ItemHeight <= 0 then
    ItemHeight := 64;
  Doc.AddScenery(StyleIndex, WX, WY, 1.0, 1.0, 0.0, ItemWidth, ItemHeight);
  DoRepaint(Repaint);
end;

function TSceneryTool.CursorForState: TCursor;
begin
  Result := crCross;
end;

procedure TSpawnTool.MouseDown(Doc: TMapDocument; UndoStack: TUndoStack;
  WX, WY: Single; Button: TMouseButton; Shift: TShiftState; Repaint: TNotifyEvent);
begin
  if (Doc = nil) or (Button <> mbLeft) then
    Exit;
  if UndoStack <> nil then
    UndoStack.Push(Doc);
  Doc.AddSpawn(WX, WY, 0);
  DoRepaint(Repaint);
end;

function TSpawnTool.CursorForState: TCursor;
begin
  Result := crCross;
end;

procedure TWaypointTool.MouseDown(Doc: TMapDocument; UndoStack: TUndoStack;
  WX, WY: Single; Button: TMouseButton; Shift: TShiftState; Repaint: TNotifyEvent);
begin
  if (Doc = nil) or (Button <> mbLeft) then
    Exit;
  if UndoStack <> nil then
    UndoStack.Push(Doc);
  Doc.AddWaypoint(WX, WY);
  DoRepaint(Repaint);
end;

function TWaypointTool.CursorForState: TCursor;
begin
  Result := crCross;
end;

procedure TColliderTool.MouseDown(Doc: TMapDocument; UndoStack: TUndoStack;
  WX, WY: Single; Button: TMouseButton; Shift: TShiftState; Repaint: TNotifyEvent);
begin
  if (Doc = nil) or (Button <> mbLeft) then
    Exit;
  if UndoStack <> nil then
    UndoStack.Push(Doc);
  Doc.AddCollider(WX, WY, 25.0);
  DoRepaint(Repaint);
end;

function TColliderTool.CursorForState: TCursor;
begin
  Result := crCross;
end;

procedure TLightTool.MouseDown(Doc: TMapDocument; UndoStack: TUndoStack;
  WX, WY: Single; Button: TMouseButton; Shift: TShiftState; Repaint: TNotifyEvent);
var
  C: TColor3;
begin
  if (Doc = nil) or (Button <> mbLeft) then
    Exit;
  if UndoStack <> nil then
    UndoStack.Push(Doc);
  C.R := 255;
  C.G := 255;
  C.B := 255;
  Doc.AddLight(WX, WY, 0.0, C, 1.0, DEFAULT_LIGHT_RANGE);
  DoRepaint(Repaint);
end;

function TLightTool.CursorForState: TCursor;
begin
  Result := crCross;
end;

procedure TSketchTool.MouseDown(Doc: TMapDocument; UndoStack: TUndoStack;
  WX, WY: Single; Button: TMouseButton; Shift: TShiftState; Repaint: TNotifyEvent);
begin
  if (Doc = nil) or (Button <> mbLeft) then
    Exit;
  FDrawing := True;
  FUndoPushed := False;
  FSketchUndo := UndoStack;
  FSketchDoc := Doc;
  FStartWorld.X := WX;
  FStartWorld.Y := WY;
  FCurrentWorld := FStartWorld;
  DoRepaint(Repaint);
end;

procedure TSketchTool.MouseMove(Doc: TMapDocument; WX, WY: Single;
  Shift: TShiftState; Repaint: TNotifyEvent);
begin
  if not FDrawing then
    Exit;
  FCurrentWorld.X := WX;
  FCurrentWorld.Y := WY;
  DoRepaint(Repaint);
end;

procedure TSketchTool.MouseUp(Doc: TMapDocument; WX, WY: Single;
  Button: TMouseButton; Repaint: TNotifyEvent);
var
  SL: TPMSSketchLine;
begin
  if (Doc = nil) or (Button <> mbLeft) or not FDrawing then
    Exit;

  FCurrentWorld.X := WX;
  FCurrentWorld.Y := WY;
  if (Abs(FCurrentWorld.X - FStartWorld.X) > 0.001) or
     (Abs(FCurrentWorld.Y - FStartWorld.Y) > 0.001) then
  begin
    FillChar(SL, SizeOf(SL), 0);
    SL.V[1].X := FStartWorld.X;
    SL.V[1].Y := FStartWorld.Y;
    SL.V[1].Z := 1.0;
    SL.V[2].X := FCurrentWorld.X;
    SL.V[2].Y := FCurrentWorld.Y;
    SL.V[2].Z := 1.0;
    if (FSketchUndo <> nil) and not FUndoPushed and (FSketchDoc = Doc) then
    begin
      FSketchUndo.Push(Doc);
      FUndoPushed := True;
    end;
    Doc.AddSketchLine(SL);
  end;

  FDrawing := False;
  FSketchUndo := nil;
  FSketchDoc := nil;
  FUndoPushed := False;
  DoRepaint(Repaint);
end;

procedure TSketchTool.Cancel(Doc: TMapDocument);
begin
  FDrawing := False;
  FSketchUndo := nil;
  FSketchDoc := nil;
  FUndoPushed := False;
end;

function TSketchTool.CursorForState: TCursor;
begin
  Result := crCross;
end;

procedure TSketchTool.RenderOverlay(Doc: TMapDocument; ViewW, ViewH: Integer);
var
  SX1, SY1, SX2, SY2: Single;
begin
  if (Doc = nil) or not FDrawing then
    Exit;
  Doc.WorldToScreen(FStartWorld.X, FStartWorld.Y, SX1, SY1);
  Doc.WorldToScreen(FCurrentWorld.X, FCurrentWorld.Y, SX2, SY2);
  DrawDashedLine(SX1, SY1, SX2, SY2);
end;

end.
