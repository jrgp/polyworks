unit frmscenery;

{$mode objfpc}{$H+}

interface

uses
  Classes, SysUtils, Controls, StdCtrls, ExtCtrls, pw.map;

type
  TSceneryPanel = class(TPanel)
  private
    FTitleLabel: TLabel;
    FSelectedLabel: TLabel;
    FListBox: TListBox;
    procedure ListBoxClick(Sender: TObject);
  public
    constructor Create(AOwner: TComponent); override;
    procedure RefreshList(const Doc: TMapDocument);
    function GetSelectedStyle: Integer;
  end;

implementation

procedure TSceneryPanel.ListBoxClick(Sender: TObject);
var
  StyleIdx: Integer;
begin
  StyleIdx := GetSelectedStyle;
  if StyleIdx > 0 then
    FSelectedLabel.Caption := 'Selected: ' + FListBox.Items[FListBox.ItemIndex]
  else
    FSelectedLabel.Caption := 'Selected: None';
end;

constructor TSceneryPanel.Create(AOwner: TComponent);
begin
  inherited Create(AOwner);
  BevelOuter := bvNone;
  Caption := '';
  Width := 240;
  Height := 300;

  FTitleLabel := TLabel.Create(Self);
  FTitleLabel.Parent := Self;
  FTitleLabel.Left := 8;
  FTitleLabel.Top := 8;
  FTitleLabel.Caption := 'Scenery Styles';

  FListBox := TListBox.Create(Self);
  FListBox.Parent := Self;
  FListBox.Left := 8;
  FListBox.Top := 28;
  FListBox.Width := 220;
  FListBox.Height := 228;
  FListBox.OnClick := @ListBoxClick;

  FSelectedLabel := TLabel.Create(Self);
  FSelectedLabel.Parent := Self;
  FSelectedLabel.Left := 8;
  FSelectedLabel.Top := 264;
  FSelectedLabel.Width := 220;
  FSelectedLabel.Caption := 'Selected: None';
end;

procedure TSceneryPanel.RefreshList(const Doc: TMapDocument);
var
  I: Integer;
begin
  FListBox.Items.BeginUpdate;
  try
    FListBox.Clear;
    if Doc <> nil then
      for I := 1 to Doc.ScenNameCount do
        FListBox.Items.Add(Doc.SceneryNames[I]);
  finally
    FListBox.Items.EndUpdate;
  end;

  FListBox.ItemIndex := -1;
  FSelectedLabel.Caption := 'Selected: None';
end;

function TSceneryPanel.GetSelectedStyle: Integer;
begin
  if FListBox.ItemIndex >= 0 then
    Result := FListBox.ItemIndex + 1
  else
    Result := 0;
end;

end.
