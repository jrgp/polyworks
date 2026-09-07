unit tools;

{$mode objfpc}{$H+}

interface

uses
  Classes, SysUtils, Math, Controls, GL,
  pw.types, pw.map, pw.undo, pw.geometry;

const
  // Core editing tools (internal IDs — NOT the same as VB6 TOOL_* constants)
  TOOL_SELECT     = 0;
  TOOL_POLY       = 1;
  TOOL_SCENERY    = 2;
  TOOL_SPAWN      = 3;
  TOOL_WAYPOINT   = 4;
  TOOL_CONNECTION = 5;
  TOOL_COLLIDER   = 6;
  TOOL_LIGHT      = 7;
  TOOL_SKETCH     = 8;
  // Additional tools matching original PolyWorks VB6 panel
  TOOL_VSELECT    = 9;   // Vertex Selection
  TOOL_PSELECT    = 10;  // Polygon Selection
  TOOL_VCOLOR     = 11;  // Vertex Color
  TOOL_PCOLOR     = 12;  // Poly Color
  TOOL_TEXEDIT    = 13;  // Texture UV edit
  TOOL_COLORPICK  = 14;  // Color Picker
  TOOL_DEPTHMAP   = 15;  // Depth Map

  TOOL_MAX        = TOOL_DEPTHMAP;

  // Bitmap row in tool_gfx.bmp for each tool ID
  // The skin has 14 rows (0–13) matching original VB6 tool order
  TOOL_BITMAP_ROW: array[0..TOOL_MAX] of Integer = (
    0,  // TOOL_SELECT    → row 0 (Transform)
    1,  // TOOL_POLY      → row 1 (Poly Creation)
    7,  // TOOL_SCENERY   → row 7 (Scenery)
    9,  // TOOL_SPAWN     → row 9 (Objects)
    8,  // TOOL_WAYPOINT  → row 8 (Waypoints)
    0,  // TOOL_CONNECTION → row 0 (reuse select icon; no dedicated row)
    0,  // TOOL_COLLIDER  → row 0 (reuse)
    12, // TOOL_LIGHT     → row 12 (Lights)
    11, // TOOL_SKETCH    → row 11 (Sketch)
    2,  // TOOL_VSELECT   → row 2 (Vertex Selection)
    3,  // TOOL_PSELECT   → row 3 (Poly Selection)
    4,  // TOOL_VCOLOR    → row 4 (Vertex Color)
    5,  // TOOL_PCOLOR    → row 5 (Poly Color)
    6,  // TOOL_TEXEDIT   → row 6 (Texture)
    10, // TOOL_COLORPICK → row 10 (Color Picker)
    13  // TOOL_DEPTHMAP  → row 13 (Depth Map)
  );

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
    // Drag-mode state machine
    type TDragMode = (dmIdle, dmSelecting, dmMoving);
    var
    FDragMode: TDragMode;
    FSelectMode: TSelMode;
    // For rubber-band selection (dmSelecting)
    FSelStart: TVector2;
    FSelCurrent: TVector2;
    FSelDragged: Boolean;
    // For object move (dmMoving)
    FMoveStart: TVector2;
    FMoveLast: TVector2;
    FUndoPushed: Boolean;
    FUndoRef: TUndoStack;

    procedure SetPolySelection(var P: TEditorPoly; Mode: TSelMode);
    function TryHitVertexOrObject(Doc: TMapDocument; WX, WY, Tol: Single;
      out HitKind: THitKind; out Index1, Index2: Integer): Boolean;
    function TryHitPolygonBody(Doc: TMapDocument; WX, WY: Single; out PolyIdx: Integer): Boolean;
    function HitSelected(Doc: TMapDocument; WX, WY: Single): Boolean;
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

  { Stub tools — basic select behavior until full implementation }
  TVertexSelectTool = class(TSelectTool)
  public
    function CursorForState: TCursor; override;
  end;

  TPolySelectTool = class(TSelectTool)
  public
    procedure MouseDown(Doc: TMapDocument; UndoStack: TUndoStack;
                        WX, WY: Single; Button: TMouseButton;
                        Shift: TShiftState; Repaint: TNotifyEvent); override;
    function CursorForState: TCursor; override;
  end;

  TVertexColorTool = class(TBaseTool)
  public
    procedure MouseDown(Doc: TMapDocument; UndoStack: TUndoStack;
                        WX, WY: Single; Button: TMouseButton;
                        Shift: TShiftState; Repaint: TNotifyEvent); override;
    function CursorForState: TCursor; override;
  end;

  TPolyColorTool = class(TBaseTool)
  public
    procedure MouseDown(Doc: TMapDocument; UndoStack: TUndoStack;
                        WX, WY: Single; Button: TMouseButton;
                        Shift: TShiftState; Repaint: TNotifyEvent); override;
    function CursorForState: TCursor; override;
  end;

  TTexEditTool = class(TBaseTool)
  public
    function CursorForState: TCursor; override;
  end;

  TColorPickTool = class(TBaseTool)
  public
    function CursorForState: TCursor; override;
  end;

  TDepthMapTool = class(TBaseTool)
  public
    function CursorForState: TCursor; override;
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
    TOOL_SELECT:     Result := TSelectTool.Create;
    TOOL_POLY:       Result := TPolyTool.Create;
    TOOL_SCENERY:    Result := TSceneryTool.Create;
    TOOL_SPAWN:      Result := TSpawnTool.Create;
    TOOL_WAYPOINT:   Result := TWaypointTool.Create;
    TOOL_COLLIDER:   Result := TColliderTool.Create;
    TOOL_LIGHT:      Result := TLightTool.Create;
    TOOL_SKETCH:     Result := TSketchTool.Create;
    TOOL_VSELECT:    Result := TVertexSelectTool.Create;
    TOOL_PSELECT:    Result := TPolySelectTool.Create;
    TOOL_VCOLOR:     Result := TVertexColorTool.Create;
    TOOL_PCOLOR:     Result := TPolyColorTool.Create;
    TOOL_TEXEDIT:    Result := TTexEditTool.Create;
    TOOL_COLORPICK:  Result := TColorPickTool.Create;
    TOOL_DEPTHMAP:   Result := TDepthMapTool.Create;
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

{ Returns True if (WX,WY) is near any SELECTED vertex/object.
  Used to decide: start move, or start rubber-band. }
function TSelectTool.HitSelected(Doc: TMapDocument; WX, WY: Single): Boolean;
var
  I, J: Integer;
  Tol2, DX, DY: Single;
  Tol: Single;
begin
  Result := False;
  if Doc = nil then Exit;
  Tol := WorldHitRadius(Doc);
  Tol2 := Tol * Tol;

  for I := 0 to Doc.PolyCount - 1 do
    for J := 1 to 3 do
      if Doc.Polys[I].Selected[J] then
      begin
        DX := Doc.Polys[I].V[J].World.X - WX;
        DY := Doc.Polys[I].V[J].World.Y - WY;
        if DX * DX + DY * DY <= Tol2 then
        begin
          Result := True;
          Exit;
        end;
      end;

  for I := 0 to Doc.SceneryCount - 1 do
    if Doc.Scenery[I].Selected then
    begin
      DX := Doc.Scenery[I].X - WX;
      DY := Doc.Scenery[I].Y - WY;
      if DX * DX + DY * DY <= Tol2 then
      begin
        Result := True;
        Exit;
      end;
    end;

  for I := 0 to Doc.SpawnCount - 1 do
    if Doc.Spawns[I].Selected then
    begin
      DX := Doc.Spawns[I].X - WX;
      DY := Doc.Spawns[I].Y - WY;
      if DX * DX + DY * DY <= Tol2 then
      begin
        Result := True;
        Exit;
      end;
    end;

  for I := 0 to Doc.ColliderCount - 1 do
    if Doc.Colliders[I].Selected then
    begin
      DX := Doc.Colliders[I].X - WX;
      DY := Doc.Colliders[I].Y - WY;
      if DX * DX + DY * DY <= Tol2 then
      begin
        Result := True;
        Exit;
      end;
    end;

  for I := 0 to Doc.WaypointCount - 1 do
    if Doc.Waypoints[I].Selected then
    begin
      DX := Doc.Waypoints[I].X - WX;
      DY := Doc.Waypoints[I].Y - WY;
      if DX * DX + DY * DY <= Tol2 then
      begin
        Result := True;
        Exit;
      end;
    end;

  for I := 0 to Doc.LightCount - 1 do
    if Doc.Lights[I].Selected then
    begin
      DX := Doc.Lights[I].X - WX;
      DY := Doc.Lights[I].Y - WY;
      if DX * DX + DY * DY <= Tol2 then
      begin
        Result := True;
        Exit;
      end;
    end;

  // Also check if click is inside a selected polygon body
  for I := 0 to Doc.PolyCount - 1 do
  begin
    // Check if all 3 vertices are selected (poly body selected)
    if Doc.Polys[I].Selected[1] and Doc.Polys[I].Selected[2] and Doc.Polys[I].Selected[3] then
      if PointInPoly(WX, WY, Doc.Polys[I].V[1].World, Doc.Polys[I].V[2].World, Doc.Polys[I].V[3].World) then
      begin
        Result := True;
        Exit;
      end;
  end;
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
var
  HitKind: THitKind;
  Index1, Index2: Integer;
  Tol: Single;
begin
  if (Doc = nil) or (Button <> mbLeft) then
    Exit;

  Tol := WorldHitRadius(Doc);

  // If clicking near an already-selected item → start a MOVE operation
  if HitSelected(Doc, WX, WY) then
  begin
    FDragMode := dmMoving;
    FMoveStart.X := WX;
    FMoveStart.Y := WY;
    FMoveLast.X := WX;
    FMoveLast.Y := WY;
    FUndoPushed := False;
    FUndoRef := UndoStack;
    DoRepaint(Repaint);
    Exit;
  end;

  // If clicking near an unselected item → select it, then start move
  if TryHitVertexOrObject(Doc, WX, WY, Tol, HitKind, Index1, Index2) or
     TryHitPolygonBody(Doc, WX, WY, Index1) then
  begin
    // Select the hit item (replace selection unless modifier held)
    FSelectMode := SelectModeFromShift(Shift);
    SelectAtPoint(Doc, WX, WY, FSelectMode);
    // Start move from the hit item
    FDragMode := dmMoving;
    FMoveStart.X := WX;
    FMoveStart.Y := WY;
    FMoveLast.X := WX;
    FMoveLast.Y := WY;
    FUndoPushed := False;
    FUndoRef := UndoStack;
    DoRepaint(Repaint);
    Exit;
  end;

  // Clicked on empty space → start rubber-band selection
  FDragMode := dmSelecting;
  FSelectMode := SelectModeFromShift(Shift);
  FSelStart.X := WX;
  FSelStart.Y := WY;
  FSelCurrent := FSelStart;
  FSelDragged := False;
  if FSelectMode = smReplace then
    Doc.ClearSelection;
  DoRepaint(Repaint);
end;

procedure TSelectTool.MouseMove(Doc: TMapDocument; WX, WY: Single;
  Shift: TShiftState; Repaint: TNotifyEvent);
const
  kMoveTolerance = 2.0;
var
  DX, DY: Single;
  WTol: Single;
begin
  case FDragMode of
    dmMoving:
    begin
      DX := WX - FMoveLast.X;
      DY := WY - FMoveLast.Y;
      // Only start actually moving after the user drags far enough
      if not FUndoPushed then
      begin
        if Doc <> nil then
          WTol := kMoveTolerance / Max(Doc.Zoom, 0.001)
        else
          WTol := kMoveTolerance;
        if (Abs(WX - FMoveStart.X) > WTol) or (Abs(WY - FMoveStart.Y) > WTol) then
        begin
          // Push undo before first mutation
          if (FUndoRef <> nil) and (Doc <> nil) then
            FUndoRef.Push(Doc);
          FUndoPushed := True;
        end;
      end;
      if FUndoPushed and (Doc <> nil) and ((DX <> 0) or (DY <> 0)) then
      begin
        Doc.MoveSelectedWorld(DX, DY);
        FMoveLast.X := WX;
        FMoveLast.Y := WY;
        DoRepaint(Repaint);
      end;
    end;

    dmSelecting:
    begin
      FSelCurrent.X := WX;
      FSelCurrent.Y := WY;
      if Doc <> nil then
        WTol := kMoveTolerance / Max(Doc.Zoom, 0.001)
      else
        WTol := kMoveTolerance;
      if (Abs(FSelCurrent.X - FSelStart.X) > WTol) or
         (Abs(FSelCurrent.Y - FSelStart.Y) > WTol) then
        FSelDragged := True;
      DoRepaint(Repaint);
    end;
  end;
end;

procedure TSelectTool.MouseUp(Doc: TMapDocument; WX, WY: Single;
  Button: TMouseButton; Repaint: TNotifyEvent);
begin
  if (Doc = nil) or (Button <> mbLeft) then
    Exit;

  case FDragMode of
    dmMoving:
    begin
      // Move is already applied incrementally; nothing extra to do.
      // If no actual movement occurred (just a click), treat as selection click.
      if not FUndoPushed then
        SelectAtPoint(Doc, WX, WY, smReplace);
      FDragMode := dmIdle;
      FUndoPushed := False;
      DoRepaint(Repaint);
    end;

    dmSelecting:
    begin
      FSelCurrent.X := WX;
      FSelCurrent.Y := WY;
      if FSelDragged then
        Doc.SelectByWorldRect(FSelStart.X, FSelStart.Y, FSelCurrent.X, FSelCurrent.Y, FSelectMode)
      else
        SelectAtPoint(Doc, WX, WY, FSelectMode);
      FDragMode := dmIdle;
      FSelDragged := False;
      DoRepaint(Repaint);
    end;

    dmIdle: ; // Nothing to do
  end;
end;

procedure TSelectTool.Cancel(Doc: TMapDocument);
begin
  // If we were moving and pushed undo, revert to snapshot
  if (FDragMode = dmMoving) and FUndoPushed and (FUndoRef <> nil) and (Doc <> nil) then
    FUndoRef.Undo(Doc);
  FDragMode := dmIdle;
  FSelDragged := False;
  FUndoPushed := False;
end;

function TSelectTool.CursorForState: TCursor;
begin
  case FDragMode of
    dmMoving: Result := crSizeAll;
  else
    Result := crDefault;
  end;
end;

procedure TSelectTool.RenderOverlay(Doc: TMapDocument; ViewW, ViewH: Integer);
var
  SX1, SY1, SX2, SY2: Single;
begin
  if (Doc = nil) or (FDragMode <> dmSelecting) or not FSelDragged then
    Exit;
  Doc.WorldToScreen(FSelStart.X, FSelStart.Y, SX1, SY1);
  Doc.WorldToScreen(FSelCurrent.X, FSelCurrent.Y, SX2, SY2);
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

// ---------------------------------------------------------------------------
// Stub tools — additional panel tools with basic select fallback behavior
// ---------------------------------------------------------------------------

function TVertexSelectTool.CursorForState: TCursor;
begin
  Result := crCross;
end;

// TPolySelectTool — selects whole polygon bodies only (no individual vertices)
procedure TPolySelectTool.MouseDown(Doc: TMapDocument; UndoStack: TUndoStack;
  WX, WY: Single; Button: TMouseButton; Shift: TShiftState; Repaint: TNotifyEvent);
var
  Mode: TSelMode;
  PolyIdx: Integer;
begin
  if (Doc = nil) or (Button <> mbLeft) then
    Exit;
  Mode := SelectModeFromShift(Shift);
  if Mode = smReplace then
    Doc.ClearSelection;
  if TryHitPolygonBody(Doc, WX, WY, PolyIdx) then
  begin
    case Mode of
      smReplace, smAdd:
      begin
        Doc.Polys[PolyIdx].Selected[1] := True;
        Doc.Polys[PolyIdx].Selected[2] := True;
        Doc.Polys[PolyIdx].Selected[3] := True;
      end;
      smSubtract:
      begin
        Doc.Polys[PolyIdx].Selected[1] := False;
        Doc.Polys[PolyIdx].Selected[2] := False;
        Doc.Polys[PolyIdx].Selected[3] := False;
      end;
    end;
  end;
  DoRepaint(Repaint);
end;

function TPolySelectTool.CursorForState: TCursor;
begin
  Result := crCross;
end;

// TVertexColorTool — placeholder; TODO: open color dialog and apply to selected vertices
procedure TVertexColorTool.MouseDown(Doc: TMapDocument; UndoStack: TUndoStack;
  WX, WY: Single; Button: TMouseButton; Shift: TShiftState; Repaint: TNotifyEvent);
begin
  // Stub: select vertex at click point for now
end;

function TVertexColorTool.CursorForState: TCursor;
begin
  Result := crCross;
end;

// TPolyColorTool — placeholder; TODO: fill polygon color
procedure TPolyColorTool.MouseDown(Doc: TMapDocument; UndoStack: TUndoStack;
  WX, WY: Single; Button: TMouseButton; Shift: TShiftState; Repaint: TNotifyEvent);
begin
  // Stub: select polygon at click point for now
end;

function TPolyColorTool.CursorForState: TCursor;
begin
  Result := crCross;
end;

function TTexEditTool.CursorForState: TCursor;
begin
  Result := crCross;
end;

function TColorPickTool.CursorForState: TCursor;
begin
  Result := crCross;
end;

function TDepthMapTool.CursorForState: TCursor;
begin
  Result := crCross;
end;

end.
