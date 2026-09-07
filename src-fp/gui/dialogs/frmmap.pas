unit frmmap;

{$mode objfpc}{$H+}

interface

uses
  Classes, SysUtils, Forms, Controls, StdCtrls, Spin, Dialogs,
  pw.types, pw.utils;

type
  TMapPropertiesDialog = class(TForm)
  private
    FMapNameEdit: TEdit;
    FTextureNameEdit: TEdit;
    FTextureBrowseButton: TButton;
    FStartJetSpin: TSpinEdit;
    FGrenadeSpin: TSpinEdit;
    FMedikitSpin: TSpinEdit;
    FWeatherCombo: TComboBox;
    FStepsCombo: TComboBox;
    FTopColorButton: TButton;
    FBottomColorButton: TButton;
    { FOpenDialog is intentionally NOT created in the constructor.
      On macOS Cocoa (Lazarus 2.x), creating TOpenDialog/NSOpenPanel during
      form construction can corrupt the Cocoa event routing and cause the
      lclSyncCheck: unrecognized-selector crash when ShowModal is called.
      Create it lazily in BrowseTexture and free it immediately after use. }
    FTopColor: LongWord;
    FBottomColor: LongWord;
    procedure BrowseTexture(Sender: TObject);
    procedure PickTopColor(Sender: TObject);
    procedure PickBottomColor(Sender: TObject);
    procedure UpdateColorButtons;
  public
    constructor Create(AOwner: TComponent); override;
    procedure LoadFromOptions(const Opts: TPMSOptions);
    procedure SaveToOptions(var Opts: TPMSOptions);
  end;

function ShowMapPropertiesDialog(var Opts: TPMSOptions): Boolean;

implementation

uses
  frmcolor;

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

constructor TMapPropertiesDialog.Create(AOwner: TComponent);
var
  OKButton: TButton;
  CancelButton: TButton;
begin
  inherited CreateNew(AOwner);
  // bsSingle creates a plain NSWindow on Cocoa; bsDialog creates NSPanel
  // which has subtle differences in macOS modal event-loop handling.
  BorderStyle := bsSingle;
  BorderIcons := [biSystemMenu];
  Position := poScreenCenter;
  Caption := 'Map Properties';
  ClientWidth := 420;
  ClientHeight := 300;

  AddDialogLabel(Self, Self, 'Map Name:', 8, 12);
  FMapNameEdit := TEdit.Create(Self);
  FMapNameEdit.Parent := Self;
  FMapNameEdit.Left := 128;
  FMapNameEdit.Top := 8;
  FMapNameEdit.Width := 272;

  AddDialogLabel(Self, Self, 'Texture Name:', 8, 40);
  FTextureNameEdit := TEdit.Create(Self);
  FTextureNameEdit.Parent := Self;
  FTextureNameEdit.Left := 128;
  FTextureNameEdit.Top := 36;
  FTextureNameEdit.Width := 192;

  FTextureBrowseButton := TButton.Create(Self);
  FTextureBrowseButton.Parent := Self;
  FTextureBrowseButton.Left := 326;
  FTextureBrowseButton.Top := 35;
  FTextureBrowseButton.Width := 74;
  FTextureBrowseButton.Caption := 'Browse...';
  FTextureBrowseButton.OnClick := @BrowseTexture;

  AddDialogLabel(Self, Self, 'Start Jet:', 8, 68);
  FStartJetSpin := TSpinEdit.Create(Self);
  FStartJetSpin.Parent := Self;
  FStartJetSpin.Left := 128;
  FStartJetSpin.Top := 64;
  FStartJetSpin.Width := 96;
  FStartJetSpin.MinValue := 0;
  FStartJetSpin.MaxValue := 65535;

  AddDialogLabel(Self, Self, 'Grenade Packs:', 8, 96);
  FGrenadeSpin := TSpinEdit.Create(Self);
  FGrenadeSpin.Parent := Self;
  FGrenadeSpin.Left := 128;
  FGrenadeSpin.Top := 92;
  FGrenadeSpin.Width := 96;
  FGrenadeSpin.MinValue := 0;
  FGrenadeSpin.MaxValue := 255;

  AddDialogLabel(Self, Self, 'Medikits:', 8, 124);
  FMedikitSpin := TSpinEdit.Create(Self);
  FMedikitSpin.Parent := Self;
  FMedikitSpin.Left := 128;
  FMedikitSpin.Top := 120;
  FMedikitSpin.Width := 96;
  FMedikitSpin.MinValue := 0;
  FMedikitSpin.MaxValue := 255;

  AddDialogLabel(Self, Self, 'Weather:', 8, 152);
  FWeatherCombo := TComboBox.Create(Self);
  FWeatherCombo.Parent := Self;
  FWeatherCombo.Left := 128;
  FWeatherCombo.Top := 148;
  FWeatherCombo.Width := 160;
  FWeatherCombo.Style := csDropDownList;
  FWeatherCombo.Items.Add('None');
  FWeatherCombo.Items.Add('Rain');
  FWeatherCombo.Items.Add('Sandstorm');
  FWeatherCombo.Items.Add('Snow');

  AddDialogLabel(Self, Self, 'Steps:', 8, 180);
  FStepsCombo := TComboBox.Create(Self);
  FStepsCombo.Parent := Self;
  FStepsCombo.Left := 128;
  FStepsCombo.Top := 176;
  FStepsCombo.Width := 160;
  FStepsCombo.Style := csDropDownList;
  FStepsCombo.Items.Add('Hard');
  FStepsCombo.Items.Add('Soft');
  FStepsCombo.Items.Add('None');

  AddDialogLabel(Self, Self, 'Top Background:', 8, 212);
  FTopColorButton := TButton.Create(Self);
  FTopColorButton.Parent := Self;
  FTopColorButton.Left := 128;
  FTopColorButton.Top := 208;
  FTopColorButton.Width := 120;
  FTopColorButton.OnClick := @PickTopColor;

  AddDialogLabel(Self, Self, 'Bottom Background:', 8, 240);
  FBottomColorButton := TButton.Create(Self);
  FBottomColorButton.Parent := Self;
  FBottomColorButton.Left := 128;
  FBottomColorButton.Top := 236;
  FBottomColorButton.Width := 120;
  FBottomColorButton.OnClick := @PickBottomColor;

  OKButton := TButton.Create(Self);
  OKButton.Parent := Self;
  OKButton.Left := 244;
  OKButton.Top := 264;
  OKButton.Width := 75;
  OKButton.Caption := 'OK';
  OKButton.ModalResult := mrOK;
  OKButton.Default := True;

  CancelButton := TButton.Create(Self);
  CancelButton.Parent := Self;
  CancelButton.Left := 325;
  CancelButton.Top := 264;
  CancelButton.Width := 75;
  CancelButton.Caption := 'Cancel';
  CancelButton.ModalResult := mrCancel;
  CancelButton.Cancel := True;

  FTopColor := $FF000000;
  FBottomColor := $FF000000;
  UpdateColorButtons;
end;

procedure TMapPropertiesDialog.BrowseTexture(Sender: TObject);
var
  Dlg: TOpenDialog;
begin
  { Create TOpenDialog lazily to avoid triggering Cocoa NSOpenPanel
    initialization during form construction (macOS lclSyncCheck: crash). }
  Dlg := TOpenDialog.Create(Self);
  try
    Dlg.Filter := 'Image files|*.bmp;*.png|Bitmap files|*.bmp|PNG files|*.png|All files|*.*';
    if Dlg.Execute then
      FTextureNameEdit.Text := ExtractFileName(Dlg.FileName);
  finally
    Dlg.Free;
  end;
end;

procedure TMapPropertiesDialog.PickTopColor(Sender: TObject);
begin
  if PickColor(FTopColor) then
    UpdateColorButtons;
end;

procedure TMapPropertiesDialog.PickBottomColor(Sender: TObject);
begin
  if PickColor(FBottomColor) then
    UpdateColorButtons;
end;

procedure TMapPropertiesDialog.UpdateColorButtons;
begin
  FTopColorButton.Caption := '$' + IntToHex(FTopColor, 8);
  FBottomColorButton.Caption := '$' + IntToHex(FBottomColor, 8);
end;

procedure TMapPropertiesDialog.LoadFromOptions(const Opts: TPMSOptions);
begin
  FMapNameEdit.Text := ReadPascalStr(Opts.MapName);
  FTextureNameEdit.Text := ReadPascalStr(Opts.TextureName);
  FStartJetSpin.Value := Opts.StartJet;
  FGrenadeSpin.Value := Opts.GrenadePacks;
  FMedikitSpin.Value := Opts.Medikits;
  FWeatherCombo.ItemIndex := Opts.Weather;
  if (FWeatherCombo.ItemIndex < 0) or (FWeatherCombo.ItemIndex >= FWeatherCombo.Items.Count) then
    FWeatherCombo.ItemIndex := 0;
  FStepsCombo.ItemIndex := Opts.Steps;
  if (FStepsCombo.ItemIndex < 0) or (FStepsCombo.ItemIndex >= FStepsCombo.Items.Count) then
    FStepsCombo.ItemIndex := 0;
  FTopColor := Opts.BgColor1;
  FBottomColor := Opts.BgColor2;
  UpdateColorButtons;
end;

procedure TMapPropertiesDialog.SaveToOptions(var Opts: TPMSOptions);
begin
  WritePascalStr(FMapNameEdit.Text, Opts.MapName, High(Opts.MapName));
  WritePascalStr(FTextureNameEdit.Text, Opts.TextureName, High(Opts.TextureName));
  Opts.StartJet := FStartJetSpin.Value;
  Opts.GrenadePacks := Byte(FGrenadeSpin.Value);
  Opts.Medikits := Byte(FMedikitSpin.Value);
  Opts.Weather := Byte(FWeatherCombo.ItemIndex);
  Opts.Steps := Byte(FStepsCombo.ItemIndex);
  Opts.BgColor1 := FTopColor;
  Opts.BgColor2 := FBottomColor;
end;

function ShowMapPropertiesDialog(var Opts: TPMSOptions): Boolean;
var
  Dialog: TMapPropertiesDialog;
begin
  Result := False;
  Dialog := TMapPropertiesDialog.Create(Application);
  try
    Dialog.LoadFromOptions(Opts);
    Result := Dialog.ShowModal = mrOK;
    if Result then
      Dialog.SaveToOptions(Opts);
  finally
    Dialog.Free;
  end;
end;

end.
