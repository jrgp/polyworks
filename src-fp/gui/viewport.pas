unit viewport;

{$mode objfpc}{$H+}

interface

uses
  Classes, SysUtils, Controls, LCLType, OpenGLContext, GL,
  renderer, tools, pw.map, pw.undo;

type
  TMapViewport = class(TOpenGLControl)
  private
    FRenderer: TRenderer;
    FTools: array[0..TOOL_SKETCH] of ITool;
    FActiveTool: Integer;
    FLastMouseX: Integer;
    FLastMouseY: Integer;
    FOnViewChanged: TNotifyEvent;
    function ActiveToolRef: ITool;
    function HasSelection: Boolean;
    procedure ApplyCursor;
    procedure NotifyViewChanged;
    procedure RenderToolOverlay;
    procedure ToolRepaint(Sender: TObject);
  protected
    function DoMouseWheel(Shift: TShiftState; WheelDelta: Integer;
      MousePos: TPoint): Boolean; override;
    procedure KeyDown(var Key: Word; Shift: TShiftState); override;
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
  end;

implementation

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
var
  Tool: ITool;
begin
  Tool := ActiveToolRef;
  if Tool <> nil then
    Cursor := Tool.CursorForState
  else
    Cursor := crDefault;
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

procedure TMapViewport.KeyDown(var Key: Word; Shift: TShiftState);
var
  ScrollStep: Single;
begin
  inherited KeyDown(Key, Shift);
  if Doc = nil then
    Exit;

  ScrollStep := 10.0;
  if Doc.Zoom > 0.0001 then
    ScrollStep := ScrollStep / Doc.Zoom;

  if (ssCtrl in Shift) and (Key = VK_Z) then
  begin
    if (UndoStack <> nil) and UndoStack.Undo(Doc) then
    begin
      RequestRepaint;
      NotifyViewChanged;
    end;
    Key := 0;
    Exit;
  end;

  if (ssCtrl in Shift) and (Key = VK_Y) then
  begin
    if (UndoStack <> nil) and UndoStack.Redo(Doc) then
    begin
      RequestRepaint;
      NotifyViewChanged;
    end;
    Key := 0;
    Exit;
  end;

  case Key of
    VK_DELETE:
      begin
        if HasSelection then
        begin
          if UndoStack <> nil then
            UndoStack.Push(Doc);
          Doc.DeleteSelected;
          RequestRepaint;
          NotifyViewChanged;
        end;
        Key := 0;
      end;
    VK_LEFT:
      begin
        Doc.Scroll(-ScrollStep, 0.0);
        RequestRepaint;
        NotifyViewChanged;
        Key := 0;
      end;
    VK_RIGHT:
      begin
        Doc.Scroll(ScrollStep, 0.0);
        RequestRepaint;
        NotifyViewChanged;
        Key := 0;
      end;
    VK_UP:
      begin
        Doc.Scroll(0.0, -ScrollStep);
        RequestRepaint;
        NotifyViewChanged;
        Key := 0;
      end;
    VK_DOWN:
      begin
        Doc.Scroll(0.0, ScrollStep);
        RequestRepaint;
        NotifyViewChanged;
        Key := 0;
      end;
  end;
end;

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
begin
  inherited MouseMove(Shift, X, Y);

  if (Doc <> nil) and (ssMiddle in Shift) and (Doc.Zoom > 0.0001) then
  begin
    Doc.Scroll(-(X - FLastMouseX) / Doc.Zoom, -(Y - FLastMouseY) / Doc.Zoom);
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
