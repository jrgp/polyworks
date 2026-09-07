unit test_config;

{$mode objfpc}{$H+}

interface

uses
  fpcunit, testregistry, SysUtils,
  pw.config;

type
  TConfigTest = class(TTestCase)
  private
    FIniPath: string;
    procedure AssertConfigEqual(const Expected, Actual: TAppConfig);
  protected
    procedure SetUp; override;
    procedure TearDown; override;
  published
    procedure TestDefaults;
    procedure TestRoundTrip;
    procedure TestMissingIni;
    procedure TestAddRecentFile;
    procedure TestRecentFileDedup;
  end;

implementation

const
  EPS = 1e-4;

procedure TConfigTest.SetUp;
begin
  FIniPath := IncludeTrailingPathDelimiter(GetCurrentDir) + 'test_config_roundtrip.ini';
  if FileExists(FIniPath) then
    DeleteFile(FIniPath);
end;

procedure TConfigTest.TearDown;
begin
  if FileExists(FIniPath) then
    DeleteFile(FIniPath);
end;

procedure TConfigTest.AssertConfigEqual(const Expected, Actual: TAppConfig);
var
  I: Integer;
begin
  AssertEquals('UndoDepth', Expected.UndoDepth, Actual.UndoDepth);
  AssertEquals('GridSize', Expected.GridSize, Actual.GridSize);
  AssertEquals('MinZoom', Expected.MinZoom, Actual.MinZoom, EPS);
  AssertEquals('MaxZoom', Expected.MaxZoom, Actual.MaxZoom, EPS);
  AssertEquals('SnapRadius', Expected.SnapRadius, Actual.SnapRadius, EPS);
  AssertEquals('OhSnap', Expected.OhSnap, Actual.OhSnap);
  AssertEquals('SnapToGrid', Expected.SnapToGrid, Actual.SnapToGrid);
  AssertEquals('SoldatPath', Expected.SoldatPath, Actual.SoldatPath);
  AssertEquals('Theme', Ord(Expected.Theme), Ord(Actual.Theme));
  AssertEquals('ShowPolys', Expected.ShowPolys, Actual.ShowPolys);
  AssertEquals('ShowWireframe', Expected.ShowWireframe, Actual.ShowWireframe);
  AssertEquals('ShowPoints', Expected.ShowPoints, Actual.ShowPoints);
  AssertEquals('ShowGrid', Expected.ShowGrid, Actual.ShowGrid);
  AssertEquals('ShowObjects', Expected.ShowObjects, Actual.ShowObjects);
  AssertEquals('ShowWaypoints', Expected.ShowWaypoints, Actual.ShowWaypoints);
  AssertEquals('ShowLights', Expected.ShowLights, Actual.ShowLights);
  AssertEquals('ShowSketch', Expected.ShowSketch, Actual.ShowSketch);
  for I := 0 to MAX_RECENT_FILES - 1 do
    AssertEquals('RecentFiles[' + IntToStr(I) + ']', Expected.RecentFiles[I], Actual.RecentFiles[I]);
  for I := 0 to MAX_POLY_TYPES - 1 do
    AssertEquals('PolyTypeColors[' + IntToStr(I) + ']',
      Expected.PolyTypeColors[I], Actual.PolyTypeColors[I]);
end;

procedure TConfigTest.TestDefaults;
var
  Cfg: TAppConfig;
begin
  DefaultConfig(Cfg);
  AssertEquals('UndoDepth', 16, Cfg.UndoDepth);
  AssertEquals('GridSize', 10, Cfg.GridSize);
  AssertEquals('MinZoom', 0.0625, Cfg.MinZoom, EPS);
end;

procedure TConfigTest.TestRoundTrip;
var
  Saved, Loaded: TAppConfig;
begin
  DefaultConfig(Saved);
  Saved.UndoDepth := 32;
  Saved.SnapRadius := 12.5;
  Saved.OhSnap := False;
  Saved.SnapToGrid := False;
  Saved.GridSize := 24;
  Saved.MinZoom := 0.125;
  Saved.MaxZoom := 8.0;
  Saved.SoldatPath := '/opt/soldat';
  Saved.Theme := thLight;
  Saved.ShowPolys := False;
  Saved.ShowWireframe := True;
  Saved.ShowPoints := True;
  Saved.ShowGrid := True;
  Saved.ShowObjects := False;
  Saved.ShowWaypoints := True;
  Saved.ShowLights := False;
  Saved.ShowSketch := False;
  Saved.RecentFiles[0] := 'one.pms';
  Saved.RecentFiles[1] := 'two.pms';
  Saved.RecentFiles[2] := 'three.pms';
  Saved.PolyTypeColors[0] := $00112233;
  Saved.PolyTypeColors[5] := $00ABCDEF;
  Saved.PolyTypeColors[25] := $00010203;

  SaveConfig(FIniPath, Saved);
  LoadConfig(FIniPath, Loaded);

  AssertConfigEqual(Saved, Loaded);
end;

procedure TConfigTest.TestMissingIni;
var
  Cfg: TAppConfig;
begin
  LoadConfig(FIniPath, Cfg);
  AssertEquals('UndoDepth default', 16, Cfg.UndoDepth);
  AssertEquals('GridSize default', 10, Cfg.GridSize);
  AssertEquals('MinZoom default', 0.0625, Cfg.MinZoom, EPS);
end;

procedure TConfigTest.TestAddRecentFile;
var
  Cfg: TAppConfig;
begin
  DefaultConfig(Cfg);
  AddRecentFile(Cfg, 'first.pms');
  AddRecentFile(Cfg, 'second.pms');
  AddRecentFile(Cfg, 'third.pms');
  AssertEquals('Newest first', 'third.pms', Cfg.RecentFiles[0]);
  AssertEquals('Second next', 'second.pms', Cfg.RecentFiles[1]);
  AssertEquals('Oldest last', 'first.pms', Cfg.RecentFiles[2]);
end;

procedure TConfigTest.TestRecentFileDedup;
var
  Cfg: TAppConfig;
begin
  DefaultConfig(Cfg);
  AddRecentFile(Cfg, 'alpha.pms');
  AddRecentFile(Cfg, 'beta.pms');
  AddRecentFile(Cfg, 'alpha.pms');
  AssertEquals('Dedup newest first', 'alpha.pms', Cfg.RecentFiles[0]);
  AssertEquals('Second remains', 'beta.pms', Cfg.RecentFiles[1]);
  AssertEquals('Removed duplicate tail', '', Cfg.RecentFiles[2]);
end;

initialization
  RegisterTest(TConfigTest);

end.
