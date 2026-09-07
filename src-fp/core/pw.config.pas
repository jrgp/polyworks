unit pw.config;

{$mode objfpc}{$H+}

interface

uses
  SysUtils, IniFiles, pw.types;

const
  MAX_RECENT_FILES = 10;
  MAX_POLY_TYPES   = 26;

type
  TTheme = (thLight, thDark);

  TAppConfig = record
    UndoDepth       : Integer;    { default 16 }
    SnapRadius      : Single;     { default 8.0 }
    OhSnap          : Boolean;    { vertex snap on/off }
    SnapToGrid      : Boolean;    { grid snap on/off }
    GridSize        : Integer;    { default 10 }
    MinZoom         : Single;     { default 0.0625 }
    MaxZoom         : Single;     { default 16.0 }
    SoldatPath      : string;
    RecentFiles     : array[0..MAX_RECENT_FILES-1] of string;
    Theme           : TTheme;
    ShowPolys       : Boolean;
    ShowWireframe   : Boolean;
    ShowPoints      : Boolean;
    ShowGrid        : Boolean;
    ShowObjects     : Boolean;
    ShowWaypoints   : Boolean;
    ShowLights      : Boolean;
    ShowSketch      : Boolean;
    PolyTypeColors  : array[0..MAX_POLY_TYPES-1] of LongWord;
  end;

procedure DefaultConfig(out Cfg: TAppConfig);
procedure LoadConfig(const IniPath: string; out Cfg: TAppConfig);
procedure SaveConfig(const IniPath: string; const Cfg: TAppConfig);
function  DetectSoldatPath: string;
procedure AddRecentFile(var Cfg: TAppConfig; const Filename: string);

implementation

uses
  Math, pw.utils
  {$IFDEF WINDOWS}, Registry{$ENDIF};

const
  DefaultPolyTypeColors: array[0..MAX_POLY_TYPES - 1] of LongWord = (
    $00CE4D4A, { 0  normal/selection color in VB6 editor }
    $007ACC29, { 1  only bullets }
    $00CCCC29, { 2  only players }
    $0029CC29, { 3  no collide }
    $0029CCCC, { 4  ice }
    $00CC297A, { 5  deadly }
    $00CC29CC, { 6  bloody deadly }
    $00CC2929, { 7  hurts }
    $002929CC, { 8  regenerates }
    $00CC7A29, { 9  lava }
    $007A7A29, { 10 alpha bullets }
    $007A2929, { 11 alpha players }
    $007A7A29, { 12 bravo bullets }
    $007A2929, { 13 bravo players }
    $007A7A29, { 14 charlie bullets }
    $007A2929, { 15 charlie players }
    $007A7A29, { 16 delta bullets }
    $007A2929, { 17 delta players }
    $00297ACC, { 18 bouncy }
    $00CCCCCC, { 19 explosive }
    $00CCCC7A, { 20 hit multiply / hurt flaggers }
    $007A7ACC, { 21 collider / only flagger }
    $007A29CC, { 22 no pass / non flagger }
    $0029297A, { 23 shift / flag collides }
    $00292929, { 24 weather / back }
    $007A7A7A  { 25 no footsteps / back transition }
  );

  PolyTypeColorKeys: array[0..MAX_POLY_TYPES - 1] of string = (
    'Normal', 'OnlyBullets', 'OnlyPlayer', 'DoesntCollide', 'Ice', 'Deadly',
    'BloodyDeadly', 'Hurts', 'Regenerates', 'Lava', 'TeamBullets',
    'TeamPlayers', 'BravoBullets', 'BravoPlayers', 'CharlieBullets',
    'CharliePlayers', 'DeltaBullets', 'DeltaPlayers', 'Bouncy', 'Explosive',
    'HurtFlaggers', 'OnlyFlagger', 'NonFlagger', 'FlagCollides', 'Back',
    'BackTransition'
  );

function InvariantFormatSettings: TFormatSettings;
begin
  Result := DefaultFormatSettings;
  Result.DecimalSeparator := '.';
end;

function ReadFloatValue(Ini: TIniFile; const Section, Ident: string;
  Default: Single): Single;
var
  S: string;
  Fmt: TFormatSettings;
begin
  Fmt := InvariantFormatSettings;
  S := Trim(Ini.ReadString(Section, Ident, ''));
  if S = '' then
    Exit(Default);
  S := StringReplace(S, ',', '.', [rfReplaceAll]);
  if not TryStrToFloat(S, Result, Fmt) then
    Result := Default;
end;

procedure WriteFloatValue(Ini: TIniFile; const Section, Ident: string;
  Value: Single);
var
  Fmt: TFormatSettings;
begin
  Fmt := InvariantFormatSettings;
  Ini.WriteString(Section, Ident, FloatToStr(Value, Fmt));
end;

function ReadThemeValue(Ini: TIniFile; const Section, Ident: string;
  Default: TTheme): TTheme;
var
  S: string;
begin
  S := Trim(LowerCase(Ini.ReadString(Section, Ident, '')));
  if (S = 'dark') or (S = '1') then
    Result := thDark
  else if (S = 'light') or (S = '0') then
    Result := thLight
  else
    Result := Default;
end;

procedure WriteThemeValue(Ini: TIniFile; const Section, Ident: string;
  Value: TTheme);
begin
  case Value of
    thDark: Ini.WriteString(Section, Ident, 'dark');
  else
    Ini.WriteString(Section, Ident, 'light');
  end;
end;

procedure DefaultConfig(out Cfg: TAppConfig);
var
  I: Integer;
begin
  FillChar(Cfg, SizeOf(Cfg), 0);
  Cfg.UndoDepth := 16;
  Cfg.SnapRadius := 8.0;
  Cfg.OhSnap := True;
  Cfg.SnapToGrid := True;
  Cfg.GridSize := 10;
  Cfg.MinZoom := 0.0625;
  Cfg.MaxZoom := 16.0;
  Cfg.SoldatPath := DetectSoldatPath;
  Cfg.Theme := thDark;
  Cfg.ShowPolys := True;
  Cfg.ShowWireframe := False;
  Cfg.ShowPoints := False;
  Cfg.ShowGrid := False;
  Cfg.ShowObjects := True;
  Cfg.ShowWaypoints := False;
  Cfg.ShowLights := True;
  Cfg.ShowSketch := True;
  for I := 0 to MAX_POLY_TYPES - 1 do
    Cfg.PolyTypeColors[I] := DefaultPolyTypeColors[I];
end;

procedure LoadConfig(const IniPath: string; out Cfg: TAppConfig);
var
  Ini: TIniFile;
  I: Integer;
begin
  DefaultConfig(Cfg);
  if not FileExists(IniPath) then
    Exit;

  Ini := TIniFile.Create(IniPath);
  try
    Cfg.SoldatPath := Ini.ReadString('Preferences', 'Dir', Cfg.SoldatPath);
    Cfg.UndoDepth := Ini.ReadInteger('Preferences', 'MaxUndo', Cfg.UndoDepth);
    Cfg.GridSize := Ini.ReadInteger('Preferences', 'GridSpacing', Cfg.GridSize);
    Cfg.MinZoom := ReadFloatValue(Ini, 'Preferences', 'MinZoom', Cfg.MinZoom);
    Cfg.MaxZoom := ReadFloatValue(Ini, 'Preferences', 'MaxZoom', Cfg.MaxZoom);
    Cfg.Theme := ReadThemeValue(Ini, 'Preferences', 'Theme', Cfg.Theme);

    if Cfg.MinZoom > Cfg.MaxZoom then
    begin
      Cfg.MinZoom := Cfg.MinZoom + Cfg.MaxZoom;
      Cfg.MaxZoom := Cfg.MinZoom - Cfg.MaxZoom;
      Cfg.MinZoom := Cfg.MinZoom - Cfg.MaxZoom;
    end
    else if SameValue(Cfg.MinZoom, Cfg.MaxZoom) then
    begin
      Cfg.MinZoom := 0.0625;
      Cfg.MaxZoom := 16.0;
    end;

    Cfg.ShowPolys := Ini.ReadBool('Display', 'Polys', Cfg.ShowPolys);
    Cfg.ShowWireframe := Ini.ReadBool('Display', 'Wireframe', Cfg.ShowWireframe);
    Cfg.ShowPoints := Ini.ReadBool('Display', 'Points', Cfg.ShowPoints);
    Cfg.ShowGrid := Ini.ReadBool('Display', 'Grid', Cfg.ShowGrid);
    Cfg.ShowObjects := Ini.ReadBool('Display', 'Objects', Cfg.ShowObjects);
    Cfg.ShowWaypoints := Ini.ReadBool('Display', 'Waypoints', Cfg.ShowWaypoints);
    Cfg.ShowLights := Ini.ReadBool('Display', 'Lights', Cfg.ShowLights);
    Cfg.ShowSketch := Ini.ReadBool('Display', 'Sketch', Cfg.ShowSketch);

    Cfg.OhSnap := Ini.ReadBool('ToolSettings', 'SnapVertices', Cfg.OhSnap);
    Cfg.SnapToGrid := Ini.ReadBool('ToolSettings', 'SnapToGrid', Cfg.SnapToGrid);
    Cfg.SnapRadius := ReadFloatValue(Ini, 'ToolSettings', 'SnapRadius', Cfg.SnapRadius);

    for I := 0 to MAX_RECENT_FILES - 1 do
      Cfg.RecentFiles[I] := Ini.ReadString('RecentFiles', Format('%.2d', [I + 1]), '');

    for I := 0 to MAX_POLY_TYPES - 1 do
      Cfg.PolyTypeColors[I] := ParseHexRGB(
        Ini.ReadString('PolyTypeColors', PolyTypeColorKeys[I],
        IntToHex(DefaultPolyTypeColors[I] and $00FFFFFF, 6))
      );
  finally
    Ini.Free;
  end;
end;

procedure SaveConfig(const IniPath: string; const Cfg: TAppConfig);
var
  Ini: TIniFile;
  I: Integer;
begin
  Ini := TIniFile.Create(IniPath);
  try
    Ini.WriteString('Preferences', 'Dir', Cfg.SoldatPath);
    Ini.WriteInteger('Preferences', 'MaxUndo', Cfg.UndoDepth);
    Ini.WriteInteger('Preferences', 'GridSpacing', Cfg.GridSize);
    WriteFloatValue(Ini, 'Preferences', 'MinZoom', Cfg.MinZoom);
    WriteFloatValue(Ini, 'Preferences', 'MaxZoom', Cfg.MaxZoom);
    WriteThemeValue(Ini, 'Preferences', 'Theme', Cfg.Theme);

    Ini.WriteBool('Display', 'Polys', Cfg.ShowPolys);
    Ini.WriteBool('Display', 'Wireframe', Cfg.ShowWireframe);
    Ini.WriteBool('Display', 'Points', Cfg.ShowPoints);
    Ini.WriteBool('Display', 'Grid', Cfg.ShowGrid);
    Ini.WriteBool('Display', 'Objects', Cfg.ShowObjects);
    Ini.WriteBool('Display', 'Waypoints', Cfg.ShowWaypoints);
    Ini.WriteBool('Display', 'Lights', Cfg.ShowLights);
    Ini.WriteBool('Display', 'Sketch', Cfg.ShowSketch);

    Ini.WriteBool('ToolSettings', 'SnapVertices', Cfg.OhSnap);
    Ini.WriteBool('ToolSettings', 'SnapToGrid', Cfg.SnapToGrid);
    WriteFloatValue(Ini, 'ToolSettings', 'SnapRadius', Cfg.SnapRadius);

    for I := 0 to MAX_RECENT_FILES - 1 do
      Ini.WriteString('RecentFiles', Format('%.2d', [I + 1]), Cfg.RecentFiles[I]);

    for I := 0 to MAX_POLY_TYPES - 1 do
      Ini.WriteString('PolyTypeColors', PolyTypeColorKeys[I],
        IntToHex(Cfg.PolyTypeColors[I] and $00FFFFFF, 6));

    Ini.UpdateFile;
  finally
    Ini.Free;
  end;
end;

function DetectSoldatPath: string;
{$IFDEF WINDOWS}
const
  RegKeys: array[0..3] of string = (
    'SOFTWARE\\Soldat',
    'SOFTWARE\\WOW6432Node\\Soldat',
    'SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Soldat',
    'SOFTWARE\\WOW6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Soldat_is1'
  );
  ValueNames: array[0..3] of string = (
    'Path',
    'InstallLocation',
    'AppPath',
    'Inno Setup: App Path'
  );
var
  Reg: TRegistry;
  KeyIdx, ValueIdx: Integer;
  Candidate: string;
begin
  Result := '';
  Reg := TRegistry.Create;
  try
    Reg.RootKey := HKEY_LOCAL_MACHINE;
    for KeyIdx := Low(RegKeys) to High(RegKeys) do
      if Reg.OpenKeyReadOnly(RegKeys[KeyIdx]) then
      try
        for ValueIdx := Low(ValueNames) to High(ValueNames) do
          if Reg.ValueExists(ValueNames[ValueIdx]) then
          begin
            Candidate := Trim(Reg.ReadString(ValueNames[ValueIdx]));
            Candidate := StringReplace(Candidate, '"', '', [rfReplaceAll]);
            Candidate := StringReplace(Candidate, '''', '', [rfReplaceAll]);
            Candidate := StringReplace(Candidate, '/', '\\', [rfReplaceAll]);
            if FileExists(Candidate) then
              Candidate := ExtractFileDir(Candidate);
            if DirectoryExists(Candidate) then
              Exit(IncludeTrailingPathDelimiter(Candidate));
          end;
      finally
        Reg.CloseKey;
      end;
  finally
    Reg.Free;
  end;
end;
{$ELSE}
begin
  Result := '';
end;
{$ENDIF}

procedure AddRecentFile(var Cfg: TAppConfig; const Filename: string);
var
  NewFiles: array[0..MAX_RECENT_FILES - 1] of string;
  I, Dest: Integer;
begin
  if Trim(Filename) = '' then
    Exit;

  FillChar(NewFiles, SizeOf(NewFiles), 0);
  NewFiles[0] := Filename;
  Dest := 1;

  for I := 0 to MAX_RECENT_FILES - 1 do
    if (Cfg.RecentFiles[I] <> '') and (not SameFileName(Cfg.RecentFiles[I], Filename)) then
    begin
      if Dest > High(NewFiles) then
        Break;
      NewFiles[Dest] := Cfg.RecentFiles[I];
      Inc(Dest);
    end;

  for I := 0 to MAX_RECENT_FILES - 1 do
    Cfg.RecentFiles[I] := NewFiles[I];
end;

end.
