unit frmdisplay;

{$mode objfpc}{$H+}

interface

uses
  Classes, Controls, StdCtrls, ExtCtrls, Spin, renderer;

type
  TDisplayPanel = class(TPanel)
  private
    FOnChange: TNotifyEvent;
    FLoading: Boolean;
    FShowPolys: TCheckBox;
    FShowWireframe: TCheckBox;
    FShowPoints: TCheckBox;
    FShowGrid: TCheckBox;
    FShowObjects: TCheckBox;
    FShowWaypoints: TCheckBox;
    FShowLights: TCheckBox;
    FShowSketch: TCheckBox;
    FShowTexture: TCheckBox;
    FShowBackground: TCheckBox;
    FShowScenery: TCheckBox;
    FGridSize: TSpinEdit;
    function AddCheckBox(const ACaption: string; TopPos: Integer): TCheckBox;
    procedure ControlChanged(Sender: TObject);
  public
    constructor Create(AOwner: TComponent); override;
    procedure GetViewSettings(out VS: TViewSettings);
    procedure SetViewSettings(const VS: TViewSettings);
    property OnChange: TNotifyEvent read FOnChange write FOnChange;
  end;

implementation

function TDisplayPanel.AddCheckBox(const ACaption: string; TopPos: Integer): TCheckBox;
begin
  Result := TCheckBox.Create(Self);
  Result.Parent := Self;
  Result.Left := 8;
  Result.Top := TopPos;
  Result.Width := 180;
  Result.Caption := ACaption;
  Result.OnChange := @ControlChanged;
end;

procedure TDisplayPanel.ControlChanged(Sender: TObject);
begin
  if FLoading then
    Exit;
  if Assigned(FOnChange) then
    FOnChange(Self);
end;

constructor TDisplayPanel.Create(AOwner: TComponent);
var
  GridLabel: TLabel;
begin
  inherited Create(AOwner);
  BevelOuter := bvNone;
  Caption := '';
  Width := 220;
  Height := 312;

  FShowPolys := AddCheckBox('Show polygons', 8);
  FShowWireframe := AddCheckBox('Show wireframe', 30);
  FShowPoints := AddCheckBox('Show points', 52);
  FShowGrid := AddCheckBox('Show grid', 74);
  FShowObjects := AddCheckBox('Show objects', 96);
  FShowWaypoints := AddCheckBox('Show waypoints', 118);
  FShowLights := AddCheckBox('Show lights', 140);
  FShowSketch := AddCheckBox('Show sketch', 162);
  FShowTexture := AddCheckBox('Show texture', 184);
  FShowBackground := AddCheckBox('Show background', 206);
  FShowScenery := AddCheckBox('Show scenery', 228);

  GridLabel := TLabel.Create(Self);
  GridLabel.Parent := Self;
  GridLabel.Left := 8;
  GridLabel.Top := 258;
  GridLabel.Caption := 'Grid size:';

  FGridSize := TSpinEdit.Create(Self);
  FGridSize.Parent := Self;
  FGridSize.Left := 80;
  FGridSize.Top := 254;
  FGridSize.Width := 72;
  FGridSize.MinValue := 1;
  FGridSize.MaxValue := 1024;
  FGridSize.Value := 10;
  FGridSize.OnChange := @ControlChanged;

  SetViewSettings(DefaultViewSettings);
end;

procedure TDisplayPanel.GetViewSettings(out VS: TViewSettings);
begin
  VS := DefaultViewSettings;
  VS.ShowPolys := FShowPolys.Checked;
  VS.ShowWireframe := FShowWireframe.Checked;
  VS.ShowPoints := FShowPoints.Checked;
  VS.ShowGrid := FShowGrid.Checked;
  VS.ShowObjects := FShowObjects.Checked;
  VS.ShowWaypoints := FShowWaypoints.Checked;
  VS.ShowLights := FShowLights.Checked;
  VS.ShowSketch := FShowSketch.Checked;
  VS.ShowTexture := FShowTexture.Checked;
  VS.ShowBackground := FShowBackground.Checked;
  VS.ShowScenery := FShowScenery.Checked;
  VS.GridSize := FGridSize.Value;
end;

procedure TDisplayPanel.SetViewSettings(const VS: TViewSettings);
begin
  FLoading := True;
  try
    FShowPolys.Checked := VS.ShowPolys;
    FShowWireframe.Checked := VS.ShowWireframe;
    FShowPoints.Checked := VS.ShowPoints;
    FShowGrid.Checked := VS.ShowGrid;
    FShowObjects.Checked := VS.ShowObjects;
    FShowWaypoints.Checked := VS.ShowWaypoints;
    FShowLights.Checked := VS.ShowLights;
    FShowSketch.Checked := VS.ShowSketch;
    FShowTexture.Checked := VS.ShowTexture;
    FShowBackground.Checked := VS.ShowBackground;
    FShowScenery.Checked := VS.ShowScenery;
    FGridSize.Value := VS.GridSize;
  finally
    FLoading := False;
  end;
end;

end.
