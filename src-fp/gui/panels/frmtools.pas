unit frmtools;

{$mode objfpc}{$H+}

interface

uses
  Classes, SysUtils, Forms, Controls, ExtCtrls, Graphics,
  tools, pw.titlepanel;

type
  TToolSelectEvent = procedure(Sender: TObject; ToolID: Integer) of object;

  TPWToolButton = class(TCustomControl)
  private
    FSkin: TBitmap;
    FSlotIndex: Integer;
    FToolID: Integer;
    FRowIndex: Integer;
    FHovered: Boolean;
    FSelected: Boolean;
    FOnToolSelect: TToolSelectEvent;
    procedure SetSelected(AValue: Boolean);
  protected
    procedure Click; override;
    procedure MouseEnter; override;
    procedure MouseLeave; override;
    procedure Paint; override;
  public
    constructor Create(AOwner: TComponent; ASkin: TBitmap; ASlotIndex,
      AToolID, ARowIndex: Integer); reintroduce;
    property ToolID: Integer read FToolID;
    property Selected: Boolean read FSelected write SetSelected;
    property OnToolSelect: TToolSelectEvent read FOnToolSelect write FOnToolSelect;
  end;

  TWToolsForm = class(TForm)
  private
    FTitleBar: TPWTitlePanel;
    FToolBitmap: TBitmap;
    FButtons: array of TPWToolButton;
    FActiveTool: Integer;
    FOnToolSelect: TToolSelectEvent;
    procedure HandleHide(Sender: TObject);
    procedure HandleToolSelect(Sender: TObject; ToolID: Integer);
  public
    constructor Create(AOwner: TComponent); override;
    destructor Destroy; override;
    procedure SetActiveTool(ToolID: Integer);
    property OnToolSelect: TToolSelectEvent read FOnToolSelect write FOnToolSelect;
  end;

implementation

uses
  Types, pw.theme;

type
  TToolLayout = record
    ToolID: Integer;    // Our internal tool ID (for callbacks)
    BmpRow: Integer;    // Row in tool_gfx.bmp (original VB6 ordering)
    SlotPos: Integer;   // Position in the 2-column grid (0=top-left)
    Tooltip: string;
  end;

const
  // All 14 original PolyWorks tools in original order (left-to-right, top-to-bottom)
  // SlotPos 0..1 = row 1, 2..3 = row 2, etc.
  TOOL_LAYOUTS: array[0..13] of TToolLayout = (
    (ToolID: TOOL_SELECT;    BmpRow: 0;  SlotPos: 0;  Tooltip: 'Transform (M)'),
    (ToolID: TOOL_POLY;      BmpRow: 1;  SlotPos: 1;  Tooltip: 'Poly Creation (C)'),
    (ToolID: TOOL_VSELECT;   BmpRow: 2;  SlotPos: 2;  Tooltip: 'Vertex Selection (V)'),
    (ToolID: TOOL_PSELECT;   BmpRow: 3;  SlotPos: 3;  Tooltip: 'Poly Selection (P)'),
    (ToolID: TOOL_VCOLOR;    BmpRow: 4;  SlotPos: 4;  Tooltip: 'Vertex Color (E)'),
    (ToolID: TOOL_PCOLOR;    BmpRow: 5;  SlotPos: 5;  Tooltip: 'Poly Color (R)'),
    (ToolID: TOOL_TEXEDIT;   BmpRow: 6;  SlotPos: 6;  Tooltip: 'Texture (T)'),
    (ToolID: TOOL_SCENERY;   BmpRow: 7;  SlotPos: 7;  Tooltip: 'Scenery (Y)'),
    (ToolID: TOOL_WAYPOINT;  BmpRow: 8;  SlotPos: 8;  Tooltip: 'Waypoints'),
    (ToolID: TOOL_SPAWN;     BmpRow: 9;  SlotPos: 9;  Tooltip: 'Objects'),
    (ToolID: TOOL_COLORPICK; BmpRow: 10; SlotPos: 10; Tooltip: 'Color Picker (,)'),
    (ToolID: TOOL_SKETCH;    BmpRow: 11; SlotPos: 11; Tooltip: 'Sketch (.)'),
    (ToolID: TOOL_LIGHT;     BmpRow: 12; SlotPos: 12; Tooltip: 'Lights'),
    (ToolID: TOOL_DEPTHMAP;  BmpRow: 13; SlotPos: 13; Tooltip: 'Depth Map')
  );

constructor TPWToolButton.Create(AOwner: TComponent; ASkin: TBitmap;
  ASlotIndex, AToolID, ARowIndex: Integer);
begin
  inherited Create(AOwner);
  ControlStyle := ControlStyle + [csOpaque];
  Color := PW_COLOR_BG;
  Width := PW_TOOL_SIZE;
  Height := PW_TOOL_SIZE;
  FSkin := ASkin;
  FSlotIndex := ASlotIndex;  // position in grid (0..13)
  FToolID := AToolID;
  FRowIndex := ARowIndex;    // bitmap row in tool_gfx.bmp
  Left := (ASlotIndex mod 2) * PW_TOOL_SIZE;
  Top := PW_TITLEBAR_HEIGHT + (ASlotIndex div 2) * PW_TOOL_SIZE;
  Cursor := crHandPoint;
end;

procedure TPWToolButton.SetSelected(AValue: Boolean);
begin
  if FSelected = AValue then
    Exit;
  FSelected := AValue;
  Invalidate;
end;

procedure TPWToolButton.Click;
begin
  inherited Click;
  if Assigned(FOnToolSelect) then
    FOnToolSelect(Self, FToolID);
end;

procedure TPWToolButton.MouseEnter;
begin
  inherited MouseEnter;
  FHovered := True;
  Invalidate;
end;

procedure TPWToolButton.MouseLeave;
begin
  inherited MouseLeave;
  FHovered := False;
  Invalidate;
end;

procedure TPWToolButton.Paint;
var
  DestRect: TRect;
  SrcRect: TRect;
  StateCol: Integer;
begin
  Canvas.Brush.Color := PW_COLOR_BG;
  Canvas.FillRect(ClientRect);

  if FSelected then
    StateCol := 2
  else if FHovered then
    StateCol := 1
  else
    StateCol := 0;

  if (FSkin <> nil) and (FSkin.Width >= 96) and
     (FSkin.Height >= (FRowIndex + 1) * PW_TOOL_SIZE) then
  begin
    DestRect := Rect(0, 0, PW_TOOL_SIZE, PW_TOOL_SIZE);
    SrcRect := Rect(StateCol * PW_TOOL_SIZE, FRowIndex * PW_TOOL_SIZE,
      StateCol * PW_TOOL_SIZE + PW_TOOL_SIZE,
      FRowIndex * PW_TOOL_SIZE + PW_TOOL_SIZE);
    Canvas.CopyRect(DestRect, FSkin.Canvas, SrcRect);
  end
  else
  begin
    Canvas.Brush.Color := PW_COLOR_ACCENT;
    Canvas.FillRect(ClientRect);
  end;
end;

constructor TWToolsForm.Create(AOwner: TComponent);
var
  I: Integer;
  Button: TPWToolButton;
  SkinPath: string;
  NumTools: Integer;
  NumRows: Integer;
begin
  inherited CreateNew(AOwner, 1);
  BorderStyle := bsNone;
  BorderIcons := [];
  FormStyle := fsStayOnTop;
  ShowInTaskBar := stNever;
  Position := poDesigned;
  Caption := '';
  Color := PW_COLOR_BG;

  NumTools := Length(TOOL_LAYOUTS);
  NumRows := (NumTools + 1) div 2;

  ClientWidth := 64;
  ClientHeight := PW_TITLEBAR_HEIGHT + NumRows * PW_TOOL_SIZE;
  Constraints.MinWidth := ClientWidth;
  Constraints.MaxWidth := ClientWidth;
  Constraints.MinHeight := ClientHeight;
  Constraints.MaxHeight := ClientHeight;

  FTitleBar := TPWTitlePanel.Create(Self, 'titlebar_tools.bmp');
  FTitleBar.Parent := Self;
  FTitleBar.OnHideClick := @HandleHide;

  FToolBitmap := TBitmap.Create;
  SkinPath := PWAssetPath('tool_gfx.bmp');
  if FileExists(SkinPath) then
    FToolBitmap.LoadFromFile(SkinPath);

  SetLength(FButtons, NumTools);
  for I := 0 to NumTools - 1 do
  begin
    Button := TPWToolButton.Create(Self, FToolBitmap, TOOL_LAYOUTS[I].SlotPos,
      TOOL_LAYOUTS[I].ToolID, TOOL_LAYOUTS[I].BmpRow);
    Button.Parent := Self;
    Button.Hint := TOOL_LAYOUTS[I].Tooltip;
    Button.ShowHint := True;
    Button.OnToolSelect := @HandleToolSelect;
    FButtons[I] := Button;
  end;

  ApplyDarkTheme(Self);
  SetActiveTool(TOOL_SELECT);
end;

destructor TWToolsForm.Destroy;
begin
  FToolBitmap.Free;
  inherited Destroy;
end;

procedure TWToolsForm.HandleHide(Sender: TObject);
begin
  Hide;
end;

procedure TWToolsForm.HandleToolSelect(Sender: TObject; ToolID: Integer);
begin
  SetActiveTool(ToolID);
  if Assigned(FOnToolSelect) then
    FOnToolSelect(Self, ToolID);
end;

procedure TWToolsForm.SetActiveTool(ToolID: Integer);
var
  I: Integer;
begin
  FActiveTool := ToolID;
  for I := Low(FButtons) to High(FButtons) do
    if FButtons[I] <> nil then
      FButtons[I].Selected := FButtons[I].ToolID = ToolID;
end;

end.
