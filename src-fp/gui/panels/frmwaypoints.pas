unit frmwaypoints;

{$mode objfpc}{$H+}

interface

uses
  Classes, SysUtils, Controls, StdCtrls, ExtCtrls, pw.map;

type
  TWaypointsPanel = class(TPanel)
  private
    FOnWaypointSelect: TNotifyEvent;
    FTitleLabel: TLabel;
    FListBox: TListBox;
    procedure ListBoxClick(Sender: TObject);
  public
    constructor Create(AOwner: TComponent); override;
    procedure Refresh(const Doc: TMapDocument); reintroduce;
    function GetSelectedWaypoint: Integer;
    property OnWaypointSelect: TNotifyEvent
      read FOnWaypointSelect write FOnWaypointSelect;
  end;

implementation

procedure TWaypointsPanel.ListBoxClick(Sender: TObject);
begin
  if Assigned(FOnWaypointSelect) then
    FOnWaypointSelect(Self);
end;

constructor TWaypointsPanel.Create(AOwner: TComponent);
begin
  inherited Create(AOwner);
  BevelOuter := bvNone;
  Caption := '';
  Width := 260;
  Height := 300;

  FTitleLabel := TLabel.Create(Self);
  FTitleLabel.Parent := Self;
  FTitleLabel.Left := 8;
  FTitleLabel.Top := 8;
  FTitleLabel.Caption := 'Waypoints';

  FListBox := TListBox.Create(Self);
  FListBox.Parent := Self;
  FListBox.Left := 8;
  FListBox.Top := 28;
  FListBox.Width := 240;
  FListBox.Height := 260;
  FListBox.OnClick := @ListBoxClick;
end;

procedure TWaypointsPanel.Refresh(const Doc: TMapDocument);
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

function TWaypointsPanel.GetSelectedWaypoint: Integer;
begin
  Result := FListBox.ItemIndex;
end;

end.
