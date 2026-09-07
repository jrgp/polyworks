unit frmtools;

{$mode objfpc}{$H+}

interface

uses
  Classes, Controls, StdCtrls, ExtCtrls, Spin, tools;

const
  POLY_TYPE_NAMES: array[0..25] of string = (
    'Normal', 'Only Bullets Collide', 'Only Players Collide',
    'No Collide', 'Ice', 'Deadly', 'Bloody Deadly', 'Hurts',
    'Regenerates', 'Lava', 'Alpha Bullets Only', 'Alpha Players Only',
    'Bravo Bullets Only', 'Bravo Players Only', 'Charlie Bullets Only',
    'Charlie Players Only', 'Delta Bullets Only', 'Delta Players Only',
    'Bouncy', 'Explosive', 'Hurt Flaggers', 'Flagger Collide',
    'Non Flagger Collide', 'Flag Collide', 'Background', 'Background Transition'
  );

type
  TToolOptionsPanel = class(TPanel)
  private
    FActiveToolLabel: TLabel;
    FToolNameLabel: TLabel;
    FPolyTypeLabel: TLabel;
    FSpawnTeamLabel: TLabel;
    FColliderRadiusLabel: TLabel;
    FLightRangeLabel: TLabel;
    FPolyTypeCombo: TComboBox;
    FSpawnTeamCombo: TComboBox;
    FColliderRadiusSpin: TSpinEdit;
    FLightRangeSpin: TSpinEdit;
    function ToolCaption(ToolID: Integer): string;
    function GetPolyType: Integer;
    function GetSpawnTeam: Integer;
    function GetColliderRadius: Integer;
    function GetLightRange: Integer;
  public
    constructor Create(AOwner: TComponent); override;
    procedure SetActiveTool(ToolID: Integer);
    property PolyType: Integer read GetPolyType;
    property SpawnTeam: Integer read GetSpawnTeam;
    property ColliderRadius: Integer read GetColliderRadius;
    property LightRange: Integer read GetLightRange;
  end;

implementation

function TToolOptionsPanel.ToolCaption(ToolID: Integer): string;
begin
  case ToolID of
    TOOL_SELECT: Result := 'Select';
    TOOL_POLY: Result := 'Polygon';
    TOOL_SCENERY: Result := 'Scenery';
    TOOL_SPAWN: Result := 'Spawn';
    TOOL_WAYPOINT: Result := 'Waypoint';
    TOOL_CONNECTION: Result := 'Connection';
    TOOL_COLLIDER: Result := 'Collider';
    TOOL_LIGHT: Result := 'Light';
    TOOL_SKETCH: Result := 'Sketch';
  else
    Result := 'Unknown';
  end;
end;

constructor TToolOptionsPanel.Create(AOwner: TComponent);
var
  I: Integer;
begin
  inherited Create(AOwner);
  BevelOuter := bvNone;
  Caption := '';
  Width := 260;
  Height := 180;

  FActiveToolLabel := TLabel.Create(Self);
  FActiveToolLabel.Parent := Self;
  FActiveToolLabel.Left := 8;
  FActiveToolLabel.Top := 8;
  FActiveToolLabel.Caption := 'Active Tool:';

  FToolNameLabel := TLabel.Create(Self);
  FToolNameLabel.Parent := Self;
  FToolNameLabel.Left := 88;
  FToolNameLabel.Top := 8;
  FToolNameLabel.Caption := 'Select';

  FPolyTypeLabel := TLabel.Create(Self);
  FPolyTypeLabel.Parent := Self;
  FPolyTypeLabel.Left := 8;
  FPolyTypeLabel.Top := 40;
  FPolyTypeLabel.Caption := 'Poly Type:';

  FPolyTypeCombo := TComboBox.Create(Self);
  FPolyTypeCombo.Parent := Self;
  FPolyTypeCombo.Left := 88;
  FPolyTypeCombo.Top := 36;
  FPolyTypeCombo.Width := 160;
  FPolyTypeCombo.Style := csDropDownList;
  for I := Low(POLY_TYPE_NAMES) to High(POLY_TYPE_NAMES) do
    FPolyTypeCombo.Items.Add(POLY_TYPE_NAMES[I]);
  FPolyTypeCombo.ItemIndex := 0;

  FSpawnTeamLabel := TLabel.Create(Self);
  FSpawnTeamLabel.Parent := Self;
  FSpawnTeamLabel.Left := 8;
  FSpawnTeamLabel.Top := 72;
  FSpawnTeamLabel.Caption := 'Spawn Team:';

  FSpawnTeamCombo := TComboBox.Create(Self);
  FSpawnTeamCombo.Parent := Self;
  FSpawnTeamCombo.Left := 88;
  FSpawnTeamCombo.Top := 68;
  FSpawnTeamCombo.Width := 160;
  FSpawnTeamCombo.Style := csDropDownList;
  FSpawnTeamCombo.Items.Add('General');
  FSpawnTeamCombo.Items.Add('Alpha');
  FSpawnTeamCombo.Items.Add('Bravo');
  FSpawnTeamCombo.Items.Add('Charlie');
  FSpawnTeamCombo.Items.Add('Delta');
  FSpawnTeamCombo.Items.Add('Frogger');
  FSpawnTeamCombo.Items.Add('Yellow');
  FSpawnTeamCombo.Items.Add('Red');
  FSpawnTeamCombo.ItemIndex := 0;

  FColliderRadiusLabel := TLabel.Create(Self);
  FColliderRadiusLabel.Parent := Self;
  FColliderRadiusLabel.Left := 8;
  FColliderRadiusLabel.Top := 104;
  FColliderRadiusLabel.Caption := 'Radius:';

  FColliderRadiusSpin := TSpinEdit.Create(Self);
  FColliderRadiusSpin.Parent := Self;
  FColliderRadiusSpin.Left := 88;
  FColliderRadiusSpin.Top := 100;
  FColliderRadiusSpin.Width := 80;
  FColliderRadiusSpin.MinValue := 1;
  FColliderRadiusSpin.MaxValue := 1000;
  FColliderRadiusSpin.Value := 25;

  FLightRangeLabel := TLabel.Create(Self);
  FLightRangeLabel.Parent := Self;
  FLightRangeLabel.Left := 8;
  FLightRangeLabel.Top := 136;
  FLightRangeLabel.Caption := 'Light Range:';

  FLightRangeSpin := TSpinEdit.Create(Self);
  FLightRangeSpin.Parent := Self;
  FLightRangeSpin.Left := 88;
  FLightRangeSpin.Top := 132;
  FLightRangeSpin.Width := 80;
  FLightRangeSpin.MinValue := 1;
  FLightRangeSpin.MaxValue := 4096;
  FLightRangeSpin.Value := 100;

  SetActiveTool(TOOL_SELECT);
end;

procedure TToolOptionsPanel.SetActiveTool(ToolID: Integer);
var
  ShowPoly, ShowSpawn, ShowCollider, ShowLight: Boolean;
begin
  FToolNameLabel.Caption := ToolCaption(ToolID);
  ShowPoly := ToolID = TOOL_POLY;
  ShowSpawn := ToolID = TOOL_SPAWN;
  ShowCollider := ToolID = TOOL_COLLIDER;
  ShowLight := ToolID = TOOL_LIGHT;

  FPolyTypeLabel.Visible := ShowPoly;
  FPolyTypeCombo.Visible := ShowPoly;
  FSpawnTeamLabel.Visible := ShowSpawn;
  FSpawnTeamCombo.Visible := ShowSpawn;
  FColliderRadiusLabel.Visible := ShowCollider;
  FColliderRadiusSpin.Visible := ShowCollider;
  FLightRangeLabel.Visible := ShowLight;
  FLightRangeSpin.Visible := ShowLight;
end;

function TToolOptionsPanel.GetPolyType: Integer;
begin
  Result := FPolyTypeCombo.ItemIndex;
end;

function TToolOptionsPanel.GetSpawnTeam: Integer;
begin
  Result := FSpawnTeamCombo.ItemIndex;
end;

function TToolOptionsPanel.GetColliderRadius: Integer;
begin
  Result := FColliderRadiusSpin.Value;
end;

function TToolOptionsPanel.GetLightRange: Integer;
begin
  Result := FLightRangeSpin.Value;
end;

end.
