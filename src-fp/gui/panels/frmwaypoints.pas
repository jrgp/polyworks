unit frmwaypoints;

{$mode objfpc}{$H+}

interface

uses
  Classes, SysUtils, Forms, Controls, StdCtrls, ExtCtrls,
  pw.map, pw.titlepanel;

type
  TWaypointForm = class(TForm)
  private
    FOnWaypointSelect: TNotifyEvent;
    FTitleBar: TPWTitlePanel;
    FContent: TPanel;
    FListBox: TListBox;
    procedure HandleHide(Sender: TObject);
    procedure ListBoxClick(Sender: TObject);
  public
    constructor Create(AOwner: TComponent); override;
    procedure Refresh(const Doc: TMapDocument); reintroduce;
    function GetSelectedWaypoint: Integer;
    property OnWaypointSelect: TNotifyEvent
      read FOnWaypointSelect write FOnWaypointSelect;
  end;

implementation

uses
  pw.theme;

procedure TWaypointForm.HandleHide(Sender: TObject);
begin
  Hide;
end;

procedure TWaypointForm.ListBoxClick(Sender: TObject);
begin
  if Assigned(FOnWaypointSelect) then
    FOnWaypointSelect(Self);
end;

constructor TWaypointForm.Create(AOwner: TComponent);
begin
  inherited CreateNew(AOwner, 1);
  BorderStyle := bsNone;
  BorderIcons := [];
  FormStyle := fsStayOnTop;
  ShowInTaskBar := stNever;
  Position := poDesigned;
  Caption := '';
  Color := PW_COLOR_BG;
  ClientWidth := 208;
  ClientHeight := 160;

  FTitleBar := TPWTitlePanel.Create(Self, 'titlebar_waypoints.bmp');
  FTitleBar.Parent := Self;
  FTitleBar.OnHideClick := @HandleHide;

  FContent := TPanel.Create(Self);
  FContent.Parent := Self;
  FContent.Align := alClient;
  FContent.BevelOuter := bvNone;
  FContent.Caption := '';

  FListBox := TListBox.Create(Self);
  FListBox.Parent := FContent;
  FListBox.Left := 8;
  FListBox.Top := 8;
  FListBox.Width := 192;
  FListBox.Height := 128;
  FListBox.OnClick := @ListBoxClick;

  ApplyDarkTheme(Self);
end;

procedure TWaypointForm.Refresh(const Doc: TMapDocument);
var
  I: Integer;
begin
  FListBox.Items.BeginUpdate;
  try
    FListBox.Clear;
    if Doc <> nil then
      for I := 0 to Doc.WaypointCount - 1 do
        FListBox.Items.Add(Format('%d: (%.1f, %.1f)',
          [I, Doc.Waypoints[I].X, Doc.Waypoints[I].Y]));
  finally
    FListBox.Items.EndUpdate;
  end;
  FListBox.ItemIndex := -1;
end;

function TWaypointForm.GetSelectedWaypoint: Integer;
begin
  Result := FListBox.ItemIndex;
end;

end.
