unit frmscenery;

{$mode objfpc}{$H+}

interface

uses
  Classes, SysUtils, Forms, Controls, StdCtrls, ExtCtrls,
  pw.map, pw.titlepanel;

type
  TSceneryForm = class(TForm)
  private
    FTitleBar: TPWTitlePanel;
    FContent: TPanel;
    FSelectedLabel: TLabel;
    FListBox: TListBox;
    procedure HandleHide(Sender: TObject);
    procedure ListBoxClick(Sender: TObject);
  public
    constructor Create(AOwner: TComponent); override;
    procedure RefreshList(const Doc: TMapDocument);
    function GetSelectedStyle: Integer;
  end;

implementation

uses
  pw.theme;

procedure TSceneryForm.HandleHide(Sender: TObject);
begin
  Hide;
end;

procedure TSceneryForm.ListBoxClick(Sender: TObject);
var
  StyleIdx: Integer;
begin
  StyleIdx := GetSelectedStyle;
  if StyleIdx > 0 then
    FSelectedLabel.Caption := 'Selected: ' + FListBox.Items[FListBox.ItemIndex]
  else
    FSelectedLabel.Caption := 'Selected: None';
end;

constructor TSceneryForm.Create(AOwner: TComponent);
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
  ClientHeight := 170;

  FTitleBar := TPWTitlePanel.Create(Self, 'titlebar_scenery.bmp');
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
  FListBox.Height := 112;
  FListBox.OnClick := @ListBoxClick;

  FSelectedLabel := TLabel.Create(Self);
  FSelectedLabel.Parent := FContent;
  FSelectedLabel.Left := 8;
  FSelectedLabel.Top := 128;
  FSelectedLabel.Width := 192;
  FSelectedLabel.AutoSize := False;
  FSelectedLabel.Caption := 'Selected: None';

  ApplyDarkTheme(Self);
end;

procedure TSceneryForm.RefreshList(const Doc: TMapDocument);
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

function TSceneryForm.GetSelectedStyle: Integer;
begin
  if FListBox.ItemIndex >= 0 then
    Result := FListBox.ItemIndex + 1
  else
    Result := 0;
end;

end.
