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
    ToolID: Integer;
    SlotIndex: Integer;
    RowIndex: Integer;
  end;

const
  TOOL_LAYOUTS: array[0..6] of TToolLayout = (
    (ToolID: TOOL_SELECT;   SlotIndex: 0;  RowIndex: 0),
    (ToolID: TOOL_POLY;     SlotIndex: 1;  RowIndex: 1),
    (ToolID: TOOL_SCENERY;  SlotIndex: 7;  RowIndex: 7),
    (ToolID: TOOL_WAYPOINT; SlotIndex: 8;  RowIndex: 8),
    (ToolID: TOOL_SPAWN;    SlotIndex: 9;  RowIndex: 9),
    (ToolID: TOOL_SKETCH;   SlotIndex: 11; RowIndex: 11),
    (ToolID: TOOL_LIGHT;    SlotIndex: 12; RowIndex: 12)
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
  FSlotIndex := ASlotIndex;
  FToolID := AToolID;
  FRowIndex := ARowIndex;
  Left := (FSlotIndex mod 2) * PW_TOOL_SIZE;
  Top := PW_TITLEBAR_HEIGHT + (FSlotIndex div 2) * PW_TOOL_SIZE;
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
begin
  inherited CreateNew(AOwner, 1);
  BorderStyle := bsNone;
  BorderIcons := [];
  FormStyle := fsStayOnTop;
  ShowInTaskBar := stNever;
  Position := poDesigned;
  Caption := '';
  Color := PW_COLOR_BG;
  ClientWidth := 64;
  ClientHeight := 241;
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

  SetLength(FButtons, Length(TOOL_LAYOUTS));
  for I := Low(TOOL_LAYOUTS) to High(TOOL_LAYOUTS) do
  begin
    Button := TPWToolButton.Create(Self, FToolBitmap, TOOL_LAYOUTS[I].SlotIndex,
      TOOL_LAYOUTS[I].ToolID, TOOL_LAYOUTS[I].RowIndex);
    Button.Parent := Self;
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
