unit frmdisplay;

{$mode objfpc}{$H+}

interface

uses
  Classes, Forms, Controls, StdCtrls, ExtCtrls, Spin,
  renderer, pw.titlepanel;

type
  TDisplayForm = class(TForm)
  private
    FOnChange: TNotifyEvent;
    FLoading: Boolean;
    FTitleBar: TPWTitlePanel;
    FContent: TPanel;
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
    procedure HandleHide(Sender: TObject);
    function AddCheckBox(const ACaption: string; LeftPos, TopPos: Integer): TCheckBox;
    procedure ControlChanged(Sender: TObject);
  public
    constructor Create(AOwner: TComponent); override;
    procedure GetViewSettings(out VS: TViewSettings);
    procedure SetViewSettings(const VS: TViewSettings);
    property OnChange: TNotifyEvent read FOnChange write FOnChange;
  end;

implementation

uses
  pw.theme;

function TDisplayForm.AddCheckBox(const ACaption: string; LeftPos,
  TopPos: Integer): TCheckBox;
begin
  Result := TCheckBox.Create(Self);
  Result.Parent := FContent;
  Result.Left := LeftPos;
  Result.Top := TopPos;
  Result.Width := 94;
  Result.Caption := ACaption;
  Result.OnChange := @ControlChanged;
end;

procedure TDisplayForm.ControlChanged(Sender: TObject);
begin
  if FLoading then
    Exit;
  if Assigned(FOnChange) then
    FOnChange(Self);
end;

constructor TDisplayForm.Create(AOwner: TComponent);
var
  GridLabel: TLabel;
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

  FTitleBar := TPWTitlePanel.Create(Self, 'titlebar_display.bmp');
  FTitleBar.Parent := Self;
  FTitleBar.OnHideClick := @HandleHide;

  FContent := TPanel.Create(Self);
  FContent.Parent := Self;
  FContent.Align := alClient;
  FContent.BevelOuter := bvNone;
  FContent.Caption := '';

  FShowPolys := AddCheckBox('Polygons', 8, 6);
  FShowWireframe := AddCheckBox('Wireframe', 8, 24);
  FShowPoints := AddCheckBox('Points', 8, 42);
  FShowGrid := AddCheckBox('Grid', 8, 60);
  FShowObjects := AddCheckBox('Objects', 8, 78);
  FShowWaypoints := AddCheckBox('Waypoints', 8, 96);

  FShowLights := AddCheckBox('Lights', 108, 6);
  FShowSketch := AddCheckBox('Sketch', 108, 24);
  FShowTexture := AddCheckBox('Texture', 108, 42);
  FShowBackground := AddCheckBox('Backdrop', 108, 60);
  FShowScenery := AddCheckBox('Scenery', 108, 78);

  GridLabel := TLabel.Create(Self);
  GridLabel.Parent := FContent;
  GridLabel.Left := 8;
  GridLabel.Top := 118;
  GridLabel.Caption := 'Grid size:';

  FGridSize := TSpinEdit.Create(Self);
  FGridSize.Parent := FContent;
  FGridSize.Left := 66;
  FGridSize.Top := 114;
  FGridSize.Width := 56;
  FGridSize.MinValue := 1;
  FGridSize.MaxValue := 1024;
  FGridSize.Value := 10;
  FGridSize.OnChange := @ControlChanged;

  ApplyDarkTheme(Self);
  SetViewSettings(DefaultViewSettings);
end;

procedure TDisplayForm.HandleHide(Sender: TObject);
begin
  Hide;
end;

procedure TDisplayForm.GetViewSettings(out VS: TViewSettings);
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

procedure TDisplayForm.SetViewSettings(const VS: TViewSettings);
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
