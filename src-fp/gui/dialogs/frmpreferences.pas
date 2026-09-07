unit frmpreferences;

{$mode objfpc}{$H+}

interface

uses
  Classes, SysUtils, Forms, Controls, StdCtrls, Spin, Dialogs, pw.config;

type
  TPreferencesDialog = class(TForm)
  private
    FSoldatPathEdit: TEdit;
    FBrowseButton: TButton;
    FUndoDepthSpin: TSpinEdit;
    FSnapToGridCheck: TCheckBox;
    FGridSizeSpin: TSpinEdit;
    FOhSnapCheck: TCheckBox;
    FSnapRadiusEdit: TEdit;
    FSelectDirDialog: TSelectDirectoryDialog;
    procedure BrowseSoldatPath(Sender: TObject);
  public
    constructor Create(AOwner: TComponent); override;
    procedure LoadFromConfig(const Cfg: TAppConfig);
    procedure SaveToConfig(var Cfg: TAppConfig);
  end;

function ShowPreferencesDialog(var Cfg: TAppConfig): Boolean;

implementation

procedure AddDialogLabel(AOwner: TComponent; AParent: TWinControl;
  const ACaption: string; ALeft, ATop: Integer);
var
  L: TLabel;
begin
  L := TLabel.Create(AOwner);
  L.Parent := AParent;
  L.Left := ALeft;
  L.Top := ATop;
  L.Caption := ACaption;
end;

function InvariantFormatSettings: TFormatSettings;
begin
  Result := DefaultFormatSettings;
  Result.DecimalSeparator := '.';
end;

function ParseSnapRadius(const S: string; DefaultValue: Single): Single;
var
  Fmt: TFormatSettings;
  Normalized: string;
begin
  Fmt := InvariantFormatSettings;
  Normalized := Trim(S);
  if Normalized = '' then
    Exit(DefaultValue);
  Normalized := StringReplace(Normalized, ',', '.', [rfReplaceAll]);
  if not TryStrToFloat(Normalized, Result, Fmt) then
    Result := DefaultValue;
end;

constructor TPreferencesDialog.Create(AOwner: TComponent);
var
  OKButton: TButton;
  CancelButton: TButton;
begin
  inherited CreateNew(AOwner);
  BorderStyle := bsSingle; // bsDialog creates NSPanel on macOS (lclSyncCheck crash workaround)
  BorderIcons := [];
  Position := poScreenCenter;
  Caption := 'Preferences';
  ClientWidth := 440;
  ClientHeight := 240;

  AddDialogLabel(Self, Self, 'Soldat Path:', 8, 12);
  FSoldatPathEdit := TEdit.Create(Self);
  FSoldatPathEdit.Parent := Self;
  FSoldatPathEdit.Left := 128;
  FSoldatPathEdit.Top := 8;
  FSoldatPathEdit.Width := 224;

  FBrowseButton := TButton.Create(Self);
  FBrowseButton.Parent := Self;
  FBrowseButton.Left := 358;
  FBrowseButton.Top := 7;
  FBrowseButton.Width := 74;
  FBrowseButton.Caption := 'Browse...';
  FBrowseButton.OnClick := @BrowseSoldatPath;

  AddDialogLabel(Self, Self, 'Undo Depth:', 8, 44);
  FUndoDepthSpin := TSpinEdit.Create(Self);
  FUndoDepthSpin.Parent := Self;
  FUndoDepthSpin.Left := 128;
  FUndoDepthSpin.Top := 40;
  FUndoDepthSpin.Width := 96;
  FUndoDepthSpin.MinValue := 1;
  FUndoDepthSpin.MaxValue := 500;

  FSnapToGridCheck := TCheckBox.Create(Self);
  FSnapToGridCheck.Parent := Self;
  FSnapToGridCheck.Left := 8;
  FSnapToGridCheck.Top := 72;
  FSnapToGridCheck.Caption := 'Snap to grid';

  AddDialogLabel(Self, Self, 'Grid Size:', 8, 100);
  FGridSizeSpin := TSpinEdit.Create(Self);
  FGridSizeSpin.Parent := Self;
  FGridSizeSpin.Left := 128;
  FGridSizeSpin.Top := 96;
  FGridSizeSpin.Width := 96;
  FGridSizeSpin.MinValue := 1;
  FGridSizeSpin.MaxValue := 100;

  FOhSnapCheck := TCheckBox.Create(Self);
  FOhSnapCheck.Parent := Self;
  FOhSnapCheck.Left := 8;
  FOhSnapCheck.Top := 128;
  FOhSnapCheck.Caption := 'Snap to vertices';

  AddDialogLabel(Self, Self, 'Snap Radius:', 8, 156);
  FSnapRadiusEdit := TEdit.Create(Self);
  FSnapRadiusEdit.Parent := Self;
  FSnapRadiusEdit.Left := 128;
  FSnapRadiusEdit.Top := 152;
  FSnapRadiusEdit.Width := 96;

  OKButton := TButton.Create(Self);
  OKButton.Parent := Self;
  OKButton.Left := 276;
  OKButton.Top := 200;
  OKButton.Width := 75;
  OKButton.Caption := 'OK';
  OKButton.ModalResult := mrOK;
  OKButton.Default := True;

  CancelButton := TButton.Create(Self);
  CancelButton.Parent := Self;
  CancelButton.Left := 357;
  CancelButton.Top := 200;
  CancelButton.Width := 75;
  CancelButton.Caption := 'Cancel';
  CancelButton.ModalResult := mrCancel;
  CancelButton.Cancel := True;

  FSelectDirDialog := TSelectDirectoryDialog.Create(Self);
end;

procedure TPreferencesDialog.BrowseSoldatPath(Sender: TObject);
begin
  FSelectDirDialog.FileName := FSoldatPathEdit.Text;
  if FSelectDirDialog.Execute then
    FSoldatPathEdit.Text := FSelectDirDialog.FileName;
end;

procedure TPreferencesDialog.LoadFromConfig(const Cfg: TAppConfig);
begin
  FSoldatPathEdit.Text := Cfg.SoldatPath;
  FUndoDepthSpin.Value := Cfg.UndoDepth;
  FSnapToGridCheck.Checked := Cfg.SnapToGrid;
  FGridSizeSpin.Value := Cfg.GridSize;
  FOhSnapCheck.Checked := Cfg.OhSnap;
  FSnapRadiusEdit.Text := FloatToStr(Cfg.SnapRadius, InvariantFormatSettings);
end;

procedure TPreferencesDialog.SaveToConfig(var Cfg: TAppConfig);
begin
  Cfg.SoldatPath := Trim(FSoldatPathEdit.Text);
  Cfg.UndoDepth := FUndoDepthSpin.Value;
  Cfg.SnapToGrid := FSnapToGridCheck.Checked;
  Cfg.GridSize := FGridSizeSpin.Value;
  Cfg.OhSnap := FOhSnapCheck.Checked;
  Cfg.SnapRadius := ParseSnapRadius(FSnapRadiusEdit.Text, Cfg.SnapRadius);
end;

function ShowPreferencesDialog(var Cfg: TAppConfig): Boolean;
var
  Dialog: TPreferencesDialog;
begin
  Dialog := TPreferencesDialog.Create(nil);
  try
    Dialog.LoadFromConfig(Cfg);
    Result := Dialog.ShowModal = mrOK;
    if Result then
      Dialog.SaveToConfig(Cfg);
  finally
    Dialog.Free;
  end;
end;

end.
