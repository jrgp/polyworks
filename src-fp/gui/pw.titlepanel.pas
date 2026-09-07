unit pw.titlepanel;

{$mode objfpc}{$H+}

interface

uses
  Classes, Controls, ExtCtrls, Graphics, Buttons;

type
  TPWTitlePanel = class(TPanel)
  private
    FBitmap: TBitmap;
    FDragActive: Boolean;
    FDragStartX: Integer;
    FDragStartY: Integer;
    FCloseButton: TSpeedButton;
    FOnHideClick: TNotifyEvent;
    procedure CloseButtonClick(Sender: TObject);
    procedure LoadTitleBitmap(const Filename: string);
  protected
    procedure MouseDown(Button: TMouseButton; Shift: TShiftState; X, Y: Integer); override;
    procedure MouseMove(Shift: TShiftState; X, Y: Integer); override;
    procedure MouseUp(Button: TMouseButton; Shift: TShiftState; X, Y: Integer); override;
    procedure Paint; override;
    procedure Resize; override;
  public
    constructor Create(AOwner: TComponent; const ABitmapFile: string); reintroduce;
    destructor Destroy; override;
    property OnHideClick: TNotifyEvent read FOnHideClick write FOnHideClick;
  end;

implementation

uses
  SysUtils, Types, Forms, LCLIntf, pw.theme;

constructor TPWTitlePanel.Create(AOwner: TComponent; const ABitmapFile: string);
begin
  inherited Create(AOwner);
  BevelOuter := bvNone;
  Caption := '';
  Align := alTop;
  Height := PW_TITLEBAR_HEIGHT;
  ParentBackground := False;
  Color := PW_COLOR_BG;

  FBitmap := TBitmap.Create;
  LoadTitleBitmap(ABitmapFile);

  FCloseButton := TSpeedButton.Create(Self);
  FCloseButton.Parent := Self;
  FCloseButton.Width := 16;
  FCloseButton.Height := 16;
  FCloseButton.Caption := 'x';
  FCloseButton.Flat := True;
  FCloseButton.OnClick := @CloseButtonClick;
  FCloseButton.Cursor := crHandPoint;
  FCloseButton.ParentFont := False;
  FCloseButton.Font.Name := PW_PANEL_FONT_NAME;
  FCloseButton.Font.Height := PW_PANEL_FONT_HEIGHT;
  FCloseButton.Font.Color := PW_COLOR_TEXT;
  Resize;
end;

destructor TPWTitlePanel.Destroy;
begin
  FBitmap.Free;
  inherited Destroy;
end;

procedure TPWTitlePanel.CloseButtonClick(Sender: TObject);
var
  ParentForm: TCustomForm;
begin
  if Assigned(FOnHideClick) then
    FOnHideClick(Self)
  else
  begin
    ParentForm := GetParentForm(Self);
    if ParentForm <> nil then
      ParentForm.Hide;
  end;
end;

procedure TPWTitlePanel.LoadTitleBitmap(const Filename: string);
var
  FullPath: string;
begin
  FBitmap.SetSize(0, 0);
  FullPath := PWAssetPath(Filename);
  if FileExists(FullPath) then
    FBitmap.LoadFromFile(FullPath);
  Invalidate;
end;

procedure TPWTitlePanel.MouseDown(Button: TMouseButton; Shift: TShiftState; X,
  Y: Integer);
var
  CurPos: TPoint;
begin
  inherited MouseDown(Button, Shift, X, Y);
  if Button <> mbLeft then
    Exit;
  FDragActive := True;
  GetCursorPos(CurPos);
  FDragStartX := CurPos.X;
  FDragStartY := CurPos.Y;
  MouseCapture := True;
end;

procedure TPWTitlePanel.MouseMove(Shift: TShiftState; X, Y: Integer);
var
  CurPos: TPoint;
  ParentForm: TCustomForm;
  DX: Integer;
  DY: Integer;
begin
  inherited MouseMove(Shift, X, Y);
  if not FDragActive then
    Exit;

  GetCursorPos(CurPos);
  DX := CurPos.X - FDragStartX;
  DY := CurPos.Y - FDragStartY;
  ParentForm := GetParentForm(Self);
  if ParentForm <> nil then
    ParentForm.SetBounds(ParentForm.Left + DX, ParentForm.Top + DY,
      ParentForm.Width, ParentForm.Height);
  FDragStartX := CurPos.X;
  FDragStartY := CurPos.Y;
end;

procedure TPWTitlePanel.MouseUp(Button: TMouseButton; Shift: TShiftState; X,
  Y: Integer);
begin
  inherited MouseUp(Button, Shift, X, Y);
  FDragActive := False;
  MouseCapture := False;
end;

procedure TPWTitlePanel.Paint;
begin
  inherited Paint;
  if (FBitmap.Width > 0) and (FBitmap.Height > 0) then
    Canvas.StretchDraw(Rect(0, 0, Width, Height), FBitmap)
  else
  begin
    Canvas.Brush.Color := PW_COLOR_ACCENT;
    Canvas.FillRect(ClientRect);
  end;
end;

procedure TPWTitlePanel.Resize;
var
  BtnLeft: Integer;
begin
  inherited Resize;
  if FCloseButton <> nil then
  begin
    BtnLeft := Width - 16;
    if BtnLeft < 0 then
      BtnLeft := 0;
    FCloseButton.SetBounds(BtnLeft, 0, 16, 16);
  end;
end;

end.
