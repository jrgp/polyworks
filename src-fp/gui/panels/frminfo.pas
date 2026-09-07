unit frminfo;

{$mode objfpc}{$H+}

interface

uses
  Classes, SysUtils, Controls, StdCtrls, ExtCtrls, Graphics,
  pw.types, pw.utils, pw.map;

type
  TInfoPanel = class(TPanel)
  private
    FTitleLabel: TLabel;
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
    function AddRow(const ACaption: string; TopPos: Integer): TLabel;
    function WeatherName(AValue: Byte): string;
    function StepsName(AValue: Byte): string;
  public
    constructor Create(AOwner: TComponent); override;
    procedure Refresh(const Doc: TMapDocument); reintroduce;
  end;

implementation

function TInfoPanel.AddRow(const ACaption: string; TopPos: Integer): TLabel;
var
  NameLabel: TLabel;
begin
  NameLabel := TLabel.Create(Self);
  NameLabel.Parent := Self;
  NameLabel.Left := 8;
  NameLabel.Top := TopPos;
  NameLabel.Caption := ACaption;

  Result := TLabel.Create(Self);
  Result.Parent := Self;
  Result.Left := 128;
  Result.Top := TopPos;
  Result.Caption := '-';
end;

function TInfoPanel.WeatherName(AValue: Byte): string;
begin
  case AValue of
    1: Result := 'Rain';
    2: Result := 'Sandstorm';
    3: Result := 'Snow';
  else
    Result := 'None';
  end;
end;

function TInfoPanel.StepsName(AValue: Byte): string;
begin
  case AValue of
    0: Result := 'Hard';
    1: Result := 'Soft';
    2: Result := 'None';
  else
    Result := IntToStr(AValue);
  end;
end;

constructor TInfoPanel.Create(AOwner: TComponent);
begin
  inherited Create(AOwner);
  BevelOuter := bvNone;
  Caption := '';
  Width := 280;
  Height := 248;

  FTitleLabel := TLabel.Create(Self);
  FTitleLabel.Parent := Self;
  FTitleLabel.Left := 8;
  FTitleLabel.Top := 8;
  FTitleLabel.Caption := 'Map Information';
  FTitleLabel.Font.Style := [fsBold];

  FMapNameValue := AddRow('Map Name:', 32);
  FTextureValue := AddRow('Texture:', 52);
  FStartJetValue := AddRow('Start Jet:', 72);
  FGrenadesValue := AddRow('Grenades:', 92);
  FMedikitsValue := AddRow('Medikits:', 112);
  FWeatherValue := AddRow('Weather:', 132);
  FStepsValue := AddRow('Steps:', 152);
  FPolyCountValue := AddRow('Polygons:', 172);
  FSceneryCountValue := AddRow('Scenery:', 192);
  FSpawnCountValue := AddRow('Spawns:', 212);
end;

procedure TInfoPanel.Refresh(const Doc: TMapDocument);
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
