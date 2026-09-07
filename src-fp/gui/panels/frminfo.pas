unit frminfo;

{$mode objfpc}{$H+}

interface

uses
  Classes, SysUtils, Forms, Controls, StdCtrls, ExtCtrls,
  pw.map, pw.titlepanel;

type
  TInfoForm = class(TForm)
  private
    FTitleBar: TPWTitlePanel;
    FContent: TPanel;
    FMapNameValue: TLabel;
    FTextureValue: TLabel;
    FStartJetValue: TLabel;
    FGrenadesValue: TLabel;
    FMedikitsValue: TLabel;
    FWeatherValue: TLabel;
    FStepsValue: TLabel;
    FPolyCountValue: TLabel;
    FSceneryCountValue: TLabel;
    FSpawnCountValue: TLabel;
    procedure HandleHide(Sender: TObject);
    function AddRow(const ACaption: string; TopPos: Integer): TLabel;
    function WeatherName(AValue: Byte): string;
    function StepsName(AValue: Byte): string;
  public
    constructor Create(AOwner: TComponent); override;
    procedure Refresh(const Doc: TMapDocument); reintroduce;
  end;

implementation

uses
  pw.types, pw.utils, pw.theme;

function TInfoForm.AddRow(const ACaption: string; TopPos: Integer): TLabel;
var
  NameLabel: TLabel;
begin
  NameLabel := TLabel.Create(Self);
  NameLabel.Parent := FContent;
  NameLabel.Left := 8;
  NameLabel.Top := TopPos;
  NameLabel.Caption := ACaption;

  Result := TLabel.Create(Self);
  Result.Parent := FContent;
  Result.Left := 106;
  Result.Top := TopPos;
  Result.Width := 94;
  Result.AutoSize := False;
  Result.Caption := '-';
end;

function TInfoForm.WeatherName(AValue: Byte): string;
begin
  case AValue of
    1: Result := 'Rain';
    2: Result := 'Sandstorm';
    3: Result := 'Snow';
  else
    Result := 'None';
  end;
end;

function TInfoForm.StepsName(AValue: Byte): string;
begin
  case AValue of
    0: Result := 'Hard';
    1: Result := 'Soft';
    2: Result := 'None';
  else
    Result := IntToStr(AValue);
  end;
end;

constructor TInfoForm.Create(AOwner: TComponent);
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
  ClientHeight := 208;

  FTitleBar := TPWTitlePanel.Create(Self, 'titlebar_properties.bmp');
  FTitleBar.Parent := Self;
  FTitleBar.OnHideClick := @HandleHide;

  FContent := TPanel.Create(Self);
  FContent.Parent := Self;
  FContent.Align := alClient;
  FContent.BevelOuter := bvNone;
  FContent.Caption := '';

  FMapNameValue := AddRow('Map Name:', 8);
  FTextureValue := AddRow('Texture:', 24);
  FStartJetValue := AddRow('Start Jet:', 40);
  FGrenadesValue := AddRow('Grenades:', 56);
  FMedikitsValue := AddRow('Medikits:', 72);
  FWeatherValue := AddRow('Weather:', 88);
  FStepsValue := AddRow('Steps:', 104);
  FPolyCountValue := AddRow('Polygons:', 120);
  FSceneryCountValue := AddRow('Scenery:', 136);
  FSpawnCountValue := AddRow('Spawns:', 152);

  ApplyDarkTheme(Self);
end;

procedure TInfoForm.HandleHide(Sender: TObject);
begin
  Hide;
end;

procedure TInfoForm.Refresh(const Doc: TMapDocument);
begin
  if Doc = nil then
  begin
    FMapNameValue.Caption := '-';
    FTextureValue.Caption := '-';
    FStartJetValue.Caption := '-';
    FGrenadesValue.Caption := '-';
    FMedikitsValue.Caption := '-';
    FWeatherValue.Caption := '-';
    FStepsValue.Caption := '-';
    FPolyCountValue.Caption := '0';
    FSceneryCountValue.Caption := '0';
    FSpawnCountValue.Caption := '0';
    Exit;
  end;

  FMapNameValue.Caption := ReadPascalStr(Doc.Options.MapName);
  FTextureValue.Caption := ReadPascalStr(Doc.Options.TextureName);
  FStartJetValue.Caption := IntToStr(Doc.Options.StartJet);
  FGrenadesValue.Caption := IntToStr(Doc.Options.GrenadePacks);
  FMedikitsValue.Caption := IntToStr(Doc.Options.Medikits);
  FWeatherValue.Caption := WeatherName(Doc.Options.Weather);
  FStepsValue.Caption := StepsName(Doc.Options.Steps);
  FPolyCountValue.Caption := IntToStr(Doc.PolyCount);
  FSceneryCountValue.Caption := IntToStr(Doc.SceneryCount);
  FSpawnCountValue.Caption := IntToStr(Doc.SpawnCount);
end;

end.
