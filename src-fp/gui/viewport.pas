unit viewport;

{$mode objfpc}{$H+}

interface

uses
  Classes, SysUtils, Controls, LCLType, LCLIntf, OpenGLContext, GL, Menus,
  renderer, tools, pw.map, pw.undo;

type
  { Callback type so frmmain can intercept tool-selection keypresses }
  TToolSelectCallback = procedure(ToolID: Integer) of object;

  TMapViewport = class(TOpenGLControl)
  private
    FRenderer: TRenderer;
    FTools: array[0..TOOL_SKETCH] of ITool;
    FActiveTool: Integer;
    FLastMouseX: Integer;
    FLastMouseY: Integer;
    FSpaceDown: Boolean;   // Space held → pan mode
    FPanActive: Boolean;   // actually panning (space+LMB or middle mouse)
    FOnViewChanged: TNotifyEvent;
    FOnToolSelect: TToolSelectCallback;  // notify frmmain of tool key press

    FContextMenu: TPopupMenu;  // right-click context menu

    function ActiveToolRef: ITool;
    function HasSelection: Boolean;
    procedure ApplyCursor;
    procedure NotifyViewChanged;
    procedure RenderToolOverlay;
    procedure ToolRepaint(Sender: TObject);
    procedure ZoomBy(Factor: Single);
    procedure ZoomReset;
    procedure NudgeSelection(DX, DY: Single);
    procedure BuildContextMenu;
    procedure OnContextMenuDeselect(Sender: TObject);
    procedure OnContextMenuSelectAll(Sender: TObject);
    procedure OnContextMenuDelete(Sender: TObject);
  protected
    function DoMouseWheel(Shift: TShiftState; WheelDelta: Integer;
      MousePos: TPoint): Boolean; override;
    procedure KeyDown(var Key: Word; Shift: TShiftState); override;
    procedure KeyUp(var Key: Word; Shift: TShiftState); override;
    procedure MouseDown(Button: TMouseButton; Shift: TShiftState;
      X, Y: Integer); override;
    procedure MouseMove(Shift: TShiftState; X, Y: Integer); override;
    procedure MouseUp(Button: TMouseButton; Shift: TShiftState;
      X, Y: Integer); override;
  public
    Doc: TMapDocument;
    UndoStack: TUndoStack;
    ViewSettings: TViewSettings;

    constructor Create(AOwner: TComponent); override;
    destructor Destroy; override;

    procedure SetDocument(ADoc: TMapDocument);
    procedure SetActiveTool(ToolID: Integer);
    function GetActiveTool: Integer;
    procedure RequestRepaint;
    procedure Paint; override;

    property Renderer: TRenderer read FRenderer;
    property OnViewChanged: TNotifyEvent read FOnViewChanged write FOnViewChanged;
    property OnToolSelect: TToolSelectCallback read FOnToolSelect write FOnToolSelect;
  end;

{ Load custom .cur cursor files from the given directory. }
{ On Windows, registers them in Screen.Cursors. On other platforms, no-op.   }
procedure LoadAllCursors(const CursorsDir: string);

implementation

uses
  Forms, pw.config{$IFDEF WINDOWS}, Windows{$ENDIF};

// ---------------------------------------------------------------------------
// Custom cursor IDs (registered in Screen.Cursors)
// ---------------------------------------------------------------------------
const
  CUR_SELECT   = -100;
  CUR_POLY     = -101;
  CUR_SCENERY  = -102;
  CUR_SPAWN    = -103;
  CUR_WAYPOINT = -104;
  CUR_LIGHT    = -105;
  CUR_SKETCH   = -106;
  CUR_HAND     = -107;

procedure TryLoadCursor(AID: Integer; const APath: string);
begin
  if not FileExists(APath) then
    Exit;
  try
    {$IFDEF WINDOWS}
    Screen.Cursors[AID] := Windows.LoadCursorFromFile(PChar(APath));
    {$ELSE}
    // On Linux/macOS: LCL cannot load .cur directly; leave as crDefault.
    // The cursor will fall back gracefully in ApplyCursor.
    {$ENDIF}
  except
    // Ignore any loading failure.
  end;
end;

procedure LoadAllCursors(const CursorsDir: string);
begin
  TryLoadCursor(CUR_SELECT,   CursorsDir + 'vselect.cur');
  TryLoadCursor(CUR_POLY,     CursorsDir + 'create.cur');
  TryLoadCursor(CUR_SCENERY,  CursorsDir + 'scenery.cur');
  TryLoadCursor(CUR_SPAWN,    CursorsDir + 'objects.cur');
  TryLoadCursor(CUR_WAYPOINT, CursorsDir + 'waypoint.cur');
  TryLoadCursor(CUR_LIGHT,    CursorsDir + 'light.cur');
  TryLoadCursor(CUR_SKETCH,   CursorsDir + 'sketch.cur');
  TryLoadCursor(CUR_HAND,     CursorsDir + 'hand.cur');
end;

// ---------------------------------------------------------------------------
// Map tool ID → cursor ID; falls back to a suitable built-in
// ---------------------------------------------------------------------------
function CursorForTool(ToolID: Integer): TCursor;
begin
  case ToolID of
    TOOL_SELECT:   Result := CUR_SELECT;
    TOOL_POLY:     Result := CUR_POLY;
    TOOL_SCENERY:  Result := CUR_SCENERY;
    TOOL_SPAWN:    Result := CUR_SPAWN;
    TOOL_WAYPOINT: Result := CUR_WAYPOINT;
    TOOL_LIGHT:    Result := CUR_LIGHT;
    TOOL_SKETCH:   Result := CUR_SKETCH;
  else
    Result := crCross;
  end;

  // If the custom cursor was never loaded (non-Windows or file missing),
  // Screen.Cursors[id] is 0; fall back to a sensible built-in.
  {$IFNDEF WINDOWS}
  case ToolID of
    TOOL_SELECT:               Result := crArrow;
    TOOL_POLY, TOOL_SCENERY,
    TOOL_SPAWN, TOOL_WAYPOINT,
    TOOL_LIGHT, TOOL_SKETCH:   Result := crCross;
  else
    Result := crDefault;
  end;
  {$ELSE}
  if Screen.Cursors[Result] = 0 then
  begin
    case ToolID of
      TOOL_SELECT:               Result := crArrow;
      TOOL_POLY, TOOL_SCENERY,
      TOOL_SPAWN, TOOL_WAYPOINT,
      TOOL_LIGHT, TOOL_SKETCH:   Result := crCross;
    else
      Result := crDefault;
    end;
  end;
  {$ENDIF}
end;

// ---------------------------------------------------------------------------
// TMapViewport private helpers
// ---------------------------------------------------------------------------

function TMapViewport.ActiveToolRef: ITool;
begin
  if (FActiveTool >= Low(FTools)) and (FActiveTool <= High(FTools)) then
    Result := FTools[FActiveTool]
  else
    Result := nil;
end;

function TMapViewport.HasSelection: Boolean;
var
  I, J: Integer;
begin
  Result := False;
  if Doc = nil then
    Exit;

  for I := 0 to Doc.PolyCount - 1 do
    for J := 1 to 3 do
      if Doc.Polys[I].Selected[J] then
        Exit(True);

  for I := 0 to Doc.SceneryCount - 1 do
    if Doc.Scenery[I].Selected then Exit(True);
  for I := 0 to Doc.SpawnCount - 1 do
    if Doc.Spawns[I].Selected then Exit(True);
  for I := 0 to Doc.ColliderCount - 1 do
    if Doc.Colliders[I].Selected then Exit(True);
  for I := 0 to Doc.WaypointCount - 1 do
    if Doc.Waypoints[I].Selected then Exit(True);
  for I := 0 to Doc.LightCount - 1 do
    if Doc.Lights[I].Selected then Exit(True);
end;

procedure TMapViewport.ApplyCursor;
begin
  if FSpaceDown or FPanActive then
    Cursor := CUR_HAND
  else
    Cursor := CursorForTool(FActiveTool);
end;

procedure TMapViewport.NotifyViewChanged;
begin
  if Assigned(FOnViewChanged) then
    FOnViewChanged(Self);
end;

procedure TMapViewport.RenderToolOverlay;
var
  Overlay: IToolOverlay;
  Tool: ITool;
begin
  Tool := ActiveToolRef;
  if (Tool <> nil) and Supports(Tool, IToolOverlay, Overlay) then
    Overlay.RenderOverlay(Doc, ClientWidth, ClientHeight);
end;

procedure TMapViewport.ToolRepaint(Sender: TObject);
begin
  RequestRepaint;
  NotifyViewChanged;
end;

procedure TMapViewport.ZoomBy(Factor: Single);
var
  NewZoom: Single;
  CX, CY: Integer;
begin
  if Doc = nil then Exit;
  NewZoom := Doc.Zoom * Factor;
  CX := ClientWidth div 2;
  CY := ClientHeight div 2;
  Doc.SetZoom(NewZoom, CX, CY);
  RequestRepaint;
  NotifyViewChanged;
end;

procedure TMapViewport.ZoomReset;
begin
  if Doc = nil then Exit;
  Doc.SetZoom(1.0, ClientWidth div 2, ClientHeight div 2);
  Doc.ScrollX := 0;
  Doc.ScrollY := 0;
  RequestRepaint;
  NotifyViewChanged;
end;

procedure TMapViewport.NudgeSelection(DX, DY: Single);
begin
  if Doc = nil then Exit;
  if not HasSelection then Exit;
  if UndoStack <> nil then
    UndoStack.Push(Doc);
  Doc.MoveSelectedWorld(DX, DY);
  RequestRepaint;
  NotifyViewChanged;
end;

procedure TMapViewport.BuildContextMenu;
var
  Item: TMenuItem;
begin
  FContextMenu := TPopupMenu.Create(Self);

  Item := TMenuItem.Create(FContextMenu);
  Item.Caption := 'Select All';
  Item.OnClick := @OnContextMenuSelectAll;
  FContextMenu.Items.Add(Item);

  Item := TMenuItem.Create(FContextMenu);
  Item.Caption := 'Deselect All';
  Item.OnClick := @OnContextMenuDeselect;
  FContextMenu.Items.Add(Item);

  Item := TMenuItem.Create(FContextMenu);
  Item.Caption := '-';
  FContextMenu.Items.Add(Item);

  Item := TMenuItem.Create(FContextMenu);
  Item.Caption := 'Delete Selected';
  Item.OnClick := @OnContextMenuDelete;
  FContextMenu.Items.Add(Item);
end;

procedure TMapViewport.OnContextMenuDeselect(Sender: TObject);
begin
  if Doc <> nil then
  begin
    Doc.ClearSelection;
    RequestRepaint;
    NotifyViewChanged;
  end;
end;

procedure TMapViewport.OnContextMenuSelectAll(Sender: TObject);
var
  I, J: Integer;
begin
  if Doc = nil then Exit;
  for I := 0 to Doc.PolyCount - 1 do
    for J := 1 to 3 do
      Doc.Polys[I].Selected[J] := True;
  for I := 0 to Doc.SceneryCount - 1 do Doc.Scenery[I].Selected := True;
  for I := 0 to Doc.SpawnCount - 1 do Doc.Spawns[I].Selected := True;
  for I := 0 to Doc.ColliderCount - 1 do Doc.Colliders[I].Selected := True;
  for I := 0 to Doc.WaypointCount - 1 do Doc.Waypoints[I].Selected := True;
  for I := 0 to Doc.LightCount - 1 do Doc.Lights[I].Selected := True;
  RequestRepaint;
  NotifyViewChanged;
end;

procedure TMapViewport.OnContextMenuDelete(Sender: TObject);
begin
  if (Doc <> nil) and HasSelection then
  begin
    if UndoStack <> nil then UndoStack.Push(Doc);
    Doc.DeleteSelected;
    RequestRepaint;
    NotifyViewChanged;
  end;
end;

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

constructor TMapViewport.Create(AOwner: TComponent);
var
  I: Integer;
begin
  inherited Create(AOwner);
  DoubleBuffered := True;
  TabStop := True;
  ParentDoubleBuffered := False;
  AutoResizeViewport := True;
  OpenGLMajorVersion := 2;
  OpenGLMinorVersion := 1;
  FRenderer := TRenderer.Create;
  ViewSettings := DefaultViewSettings;
  for I := Low(FTools) to High(FTools) do
    FTools[I] := CreateToolByID(I);
  FActiveTool := TOOL_SELECT;
  FSpaceDown := False;
  FPanActive := False;
  BuildContextMenu;
  ApplyCursor;
end;

destructor TMapViewport.Destroy;
var
  I: Integer;
  Tool: ITool;
begin
  for I := Low(FTools) to High(FTools) do
  begin
    Tool := FTools[I];
    if Tool <> nil then
      Tool.Cancel(Doc);
    FTools[I] := nil;
  end;
  FRenderer.Free;
  inherited Destroy;
end;

// ---------------------------------------------------------------------------
// Public
// ---------------------------------------------------------------------------

procedure TMapViewport.SetDocument(ADoc: TMapDocument);
begin
  Doc := ADoc;
  RequestRepaint;
  NotifyViewChanged;
end;

procedure TMapViewport.SetActiveTool(ToolID: Integer);
var
  OldTool: ITool;
begin
  if (ToolID < Low(FTools)) or (ToolID > High(FTools)) or (FTools[ToolID] = nil) then
    ToolID := TOOL_SELECT;

  if ToolID = FActiveTool then
    Exit;

  OldTool := ActiveToolRef;
  if OldTool <> nil then
    OldTool.Cancel(Doc);

  FActiveTool := ToolID;
  ApplyCursor;
  RequestRepaint;
end;

function TMapViewport.GetActiveTool: Integer;
begin
  Result := FActiveTool;
end;

procedure TMapViewport.RequestRepaint;
begin
  Invalidate;
end;

// ---------------------------------------------------------------------------
// Input — mouse wheel
// ---------------------------------------------------------------------------

function TMapViewport.DoMouseWheel(Shift: TShiftState; WheelDelta: Integer;
  MousePos: TPoint): Boolean;
var
  P: TPoint;
  NewZoom: Single;
begin
  Result := inherited DoMouseWheel(Shift, WheelDelta, MousePos);
  if Doc = nil then
    Exit;

  P := ScreenToClient(MousePos);
  if WheelDelta > 0 then
    NewZoom := Doc.Zoom * 1.25
  else
    NewZoom := Doc.Zoom / 1.25;
  Doc.SetZoom(NewZoom, P.X, P.Y);
  RequestRepaint;
  NotifyViewChanged;
  Result := True;
end;

// ---------------------------------------------------------------------------
// Input — keyboard
// ---------------------------------------------------------------------------

procedure TMapViewport.KeyDown(var Key: Word; Shift: TShiftState);
var
  ScrollStep, NudgeStep: Single;
  NewToolID: Integer;
  Tool: ITool;
begin
  inherited KeyDown(Key, Shift);
  if Doc = nil then
    Exit;

  // --- Space: activate pan mode ---
  if Key = VK_SPACE then
  begin
    if not FSpaceDown then
    begin
      FSpaceDown := True;
      ApplyCursor;
    end;
    Key := 0;
    Exit;
  end;

  // --- Ctrl + key combinations ---
  if ssCtrl in Shift then
  begin
    case Key of
      VK_Z:
        begin
          if (UndoStack <> nil) and UndoStack.Undo(Doc) then
          begin
            RequestRepaint;
            NotifyViewChanged;
          end;
          Key := 0; Exit;
        end;
      VK_Y:
        begin
          if (UndoStack <> nil) and UndoStack.Redo(Doc) then
          begin
            RequestRepaint;
            NotifyViewChanged;
          end;
          Key := 0; Exit;
        end;
      VK_A:  // Select All
        begin
          OnContextMenuSelectAll(nil);
          Key := 0; Exit;
        end;
      VK_D:  // Deselect All
        begin
          Doc.ClearSelection;
          RequestRepaint;
          NotifyViewChanged;
          Key := 0; Exit;
        end;
      Ord('='), $BB:  // Ctrl+= or Ctrl++ → zoom in (VK_OEM_PLUS=$BB)
        begin
          ZoomBy(2.0);
          Key := 0; Exit;
        end;
      Ord('-'), $BD:  // Ctrl+- → zoom out (VK_OEM_MINUS=$BD)
        begin
          ZoomBy(0.5);
          Key := 0; Exit;
        end;
      Ord('0'), VK_NUMPAD0:   // Ctrl+0 → reset zoom
        begin
          ZoomReset;
          Key := 0; Exit;
        end;
    end;
    Exit;  // don't process any Ctrl+other as tool key
  end;

  // --- Tool selection hotkeys (no modifier) ---
  if (Shift * [ssCtrl, ssAlt]) = [] then
  begin
    NewToolID := -1;
    case Key of
      Ord('M'): NewToolID := TOOL_SELECT;    // Transform/Move
      Ord('C'): NewToolID := TOOL_POLY;      // Create polygon
      Ord('Y'): NewToolID := TOOL_SCENERY;   // Scenery
      Ord('O'): NewToolID := TOOL_SPAWN;     // Objects/Spawn
      Ord('T'): NewToolID := TOOL_WAYPOINT;  // Waypoints
      Ord('L'): NewToolID := TOOL_LIGHT;     // Lights
      Ord('.'): NewToolID := TOOL_SKETCH;    // Sketch
    end;
    if NewToolID >= 0 then
    begin
      SetActiveTool(NewToolID);
      if Assigned(FOnToolSelect) then
        FOnToolSelect(NewToolID);
      Key := 0;
      Exit;
    end;

    // [ / ] cycle tools
    if Key = Ord('[') then
    begin
      NewToolID := FActiveTool - 1;
      if NewToolID < Low(FTools) then NewToolID := High(FTools);
      SetActiveTool(NewToolID);
      if Assigned(FOnToolSelect) then FOnToolSelect(NewToolID);
      Key := 0; Exit;
    end;
    if Key = Ord(']') then
    begin
      NewToolID := FActiveTool + 1;
      if NewToolID > High(FTools) then NewToolID := Low(FTools);
      SetActiveTool(NewToolID);
      if Assigned(FOnToolSelect) then FOnToolSelect(NewToolID);
      Key := 0; Exit;
    end;
  end;

  // --- Numpad zoom ---
  case Key of
    VK_ADD:      begin ZoomBy(2.0);  Key := 0; Exit; end;
    VK_SUBTRACT: begin ZoomBy(0.5);  Key := 0; Exit; end;
    VK_MULTIPLY: begin ZoomReset;    Key := 0; Exit; end;
  end;

  // --- Escape: cancel current tool action ---
  if Key = VK_ESCAPE then
  begin
    Tool := ActiveToolRef;
    if Tool <> nil then
      Tool.Cancel(Doc);
    Doc.ClearSelection;
    RequestRepaint;
    NotifyViewChanged;
    Key := 0;
    Exit;
  end;

  // --- Delete ---
  if Key = VK_DELETE then
  begin
    if HasSelection then
    begin
      if UndoStack <> nil then UndoStack.Push(Doc);
      Doc.DeleteSelected;
      RequestRepaint;
      NotifyViewChanged;
    end;
    Key := 0;
    Exit;
  end;

  // --- Arrow keys: nudge selection or scroll viewport ---
  if Key in [VK_LEFT, VK_RIGHT, VK_UP, VK_DOWN] then
  begin
    // Step = 1 world unit; Shift = grid-sized step
    NudgeStep := 1.0;
    if ssShift in Shift then
      NudgeStep := ViewSettings.GridSize;

    ScrollStep := 10.0;
    if Doc.Zoom > 0.0001 then
      ScrollStep := ScrollStep / Doc.Zoom;

    if HasSelection then
    begin
      case Key of
        VK_LEFT:  NudgeSelection(-NudgeStep, 0.0);
        VK_RIGHT: NudgeSelection( NudgeStep, 0.0);
        VK_UP:    NudgeSelection(0.0, -NudgeStep);
        VK_DOWN:  NudgeSelection(0.0,  NudgeStep);
      end;
    end
    else
    begin
      case Key of
        VK_LEFT:  begin Doc.Scroll(-ScrollStep, 0.0); end;
        VK_RIGHT: begin Doc.Scroll( ScrollStep, 0.0); end;
        VK_UP:    begin Doc.Scroll(0.0, -ScrollStep); end;
        VK_DOWN:  begin Doc.Scroll(0.0,  ScrollStep); end;
      end;
      RequestRepaint;
      NotifyViewChanged;
    end;
    Key := 0;
  end;
end;

procedure TMapViewport.KeyUp(var Key: Word; Shift: TShiftState);
begin
  inherited KeyUp(Key, Shift);
  if Key = VK_SPACE then
  begin
    FSpaceDown := False;
    FPanActive := False;
    ApplyCursor;
    Key := 0;
  end;
end;

// ---------------------------------------------------------------------------
// Input — mouse
// ---------------------------------------------------------------------------

procedure TMapViewport.MouseDown(Button: TMouseButton; Shift: TShiftState;
  X, Y: Integer);
var
  Tool: ITool;
  WX, WY: Single;
begin
  inherited MouseDown(Button, Shift, X, Y);
  SetFocus;
  FLastMouseX := X;
  FLastMouseY := Y;

  // Middle mouse or Space+Left → start panning
  if (Button = mbMiddle) or (Button = mbLeft) and FSpaceDown then
  begin
    FPanActive := True;
    ApplyCursor;
    Exit;
  end;

  // Right click → context menu
  if Button = mbRight then
  begin
    // Cancel any in-progress tool action on right click
    Tool := ActiveToolRef;
    if Tool <> nil then
      Tool.Cancel(Doc);
    RequestRepaint;
    FContextMenu.PopupComponent := Self;
    FContextMenu.Popup(
      Self.ClientToScreen(Point(X, Y)).X,
      Self.ClientToScreen(Point(X, Y)).Y);
    Exit;
  end;

  // Left click → tool action
  if (Button = mbLeft) and (Doc <> nil) then
  begin
    Tool := ActiveToolRef;
    if Tool <> nil then
    begin
      Doc.ScreenToWorld(X, Y, WX, WY);
      Tool.MouseDown(Doc, UndoStack, WX, WY, Button, Shift, @ToolRepaint);
      ApplyCursor;
    end;
  end;
end;

procedure TMapViewport.MouseMove(Shift: TShiftState; X, Y: Integer);
var
  Tool: ITool;
  WX, WY: Single;
  DX, DY: Integer;
begin
  inherited MouseMove(Shift, X, Y);
  DX := X - FLastMouseX;
  DY := Y - FLastMouseY;

  // Pan: middle mouse or space+left drag
  if FPanActive and (Doc <> nil) and (Doc.Zoom > 0.0001) then
  begin
    Doc.Scroll(-DX / Doc.Zoom, -DY / Doc.Zoom);
    RequestRepaint;
    NotifyViewChanged;
  end;

  FLastMouseX := X;
  FLastMouseY := Y;

  if Doc <> nil then
  begin
    Tool := ActiveToolRef;
    if Tool <> nil then
    begin
      Doc.ScreenToWorld(X, Y, WX, WY);
      Tool.MouseMove(Doc, WX, WY, Shift, @ToolRepaint);
      ApplyCursor;
    end;
  end;
end;

procedure TMapViewport.MouseUp(Button: TMouseButton; Shift: TShiftState;
  X, Y: Integer);
var
  Tool: ITool;
  WX, WY: Single;
begin
  inherited MouseUp(Button, Shift, X, Y);
  FLastMouseX := X;
  FLastMouseY := Y;

  // End pan
  if (Button = mbMiddle) or ((Button = mbLeft) and FPanActive) then
  begin
    FPanActive := False;
    ApplyCursor;
    Exit;
  end;

  if (Button = mbLeft) and (Doc <> nil) then
  begin
    Tool := ActiveToolRef;
    if Tool <> nil then
    begin
      Doc.ScreenToWorld(X, Y, WX, WY);
      Tool.MouseUp(Doc, WX, WY, Button, @ToolRepaint);
      ApplyCursor;
    end;
  end;
end;

// ---------------------------------------------------------------------------
// Rendering
// ---------------------------------------------------------------------------

procedure TMapViewport.Paint;
begin
  if not MakeCurrent then
    Exit;

  if Doc <> nil then
    FRenderer.Render(Doc, ViewSettings, ClientWidth, ClientHeight)
  else
  begin
    glViewport(0, 0, ClientWidth, ClientHeight);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
  end;

  RenderToolOverlay;
  SwapBuffers;
end;

end.
