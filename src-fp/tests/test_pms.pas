unit test_pms;

{$mode objfpc}{$H+}

interface

uses
  fpcunit, testregistry, SysUtils, Classes,
  pw.types, pw.utils, pw.pms;

type
  TPMSTest = class(TTestCase)
  private
    FTempDir: string;
    function  TempFile(const Name: string): string;
    function  BuildMinimalPMS(PolyCount: Integer; MapID: LongInt): TMemoryStream;
    procedure WritePMSHeader(S: TStream; const MapName, TexName: string;
                             PolyCount: Integer; MapID: LongInt);
  protected
    procedure SetUp; override;
    procedure TearDown; override;
  published
    { Real map loading }
    procedure TestLoadRealMap_Arena;
    procedure TestLoadRealMap_AllMaps;

    { Basic load/save round-trip }
    procedure TestSaveLoadRoundTrip;
    procedure TestSaveNativeMapRandomID;

    { Specific field validation }
    procedure TestOptionsMapName;
    procedure TestOptionsTextureName;
    procedure TestStartJetIs4Bytes;
    procedure TestSectorTableAllZerosInNative;

    { Error handling }
    procedure TestLoadMissingFile;
    procedure TestLoadWrongVersion;
    procedure TestLoadTruncated;
    procedure TestStartJetRoundTrip;
  end;

implementation

const
  MAPS_DIR = '../../maps/';  { relative to tests/ directory }

{ ---- Helpers ------------------------------------------------------------ }

function TPMSTest.TempFile(const Name: string): string;
begin
  Result := FTempDir + PathDelim + Name;
end;

procedure TPMSTest.SetUp;
begin
  FTempDir := GetTempDir + 'pw_test_' + IntToStr(GetProcessID);
  ForceDirectories(FTempDir);
end;

procedure TPMSTest.TearDown;
var
  SR: TSearchRec;
begin
  { Clean up temp files }
  if FindFirst(FTempDir + PathDelim + '*', faAnyFile, SR) = 0 then
  begin
    repeat
      if (SR.Name <> '.') and (SR.Name <> '..') then
        DeleteFile(FTempDir + PathDelim + SR.Name);
    until FindNext(SR) <> 0;
    FindClose(SR);
  end;
  RmDir(FTempDir);
end;

procedure TPMSTest.WritePMSHeader(S: TStream; const MapName, TexName: string;
                                   PolyCount: Integer; MapID: LongInt);
var
  Version: LongInt;
  Opt: TPMSOptions;
  SectorDiv: LongInt;
  SectorNumConst: LongInt;
  Zero2: SmallInt;
  ZeroL: LongInt;
  I, J: Integer;
begin
  Version := 11;
  S.Write(Version, 4);

  FillChar(Opt, SizeOf(Opt), 0);
  WritePascalStr(MapName, Opt.MapName, 38);
  WritePascalStr(TexName, Opt.TextureName, 24);
  Opt.MapRandomID := MapID;
  Opt.StartJet := 100;
  S.Write(Opt, SizeOf(Opt));

  ZeroL := PolyCount;
  S.Write(ZeroL, 4);  { poly count }

  { (no polygon data — minimal map) }

  SectorDiv := 100;
  SectorNumConst := 25;
  S.Write(SectorDiv, 4);
  S.Write(SectorNumConst, 4);

  Zero2 := 0;
  for I := 0 to 50 do
    for J := 0 to 50 do
      S.Write(Zero2, 2);

  { scenery count = 0 }
  ZeroL := 0; S.Write(ZeroL, 4);
  { scenery elements = 0 }
  S.Write(ZeroL, 4);
  { collider count = 0 }
  S.Write(ZeroL, 4);
  { spawn count = 0 }
  S.Write(ZeroL, 4);
  { waypoint count = 0 }
  S.Write(ZeroL, 4);

  if MapID < 0 then
  begin
    { lightCount + sketchCount as SmallInt }
    Zero2 := 0; S.Write(Zero2, 2);
    S.Write(Zero2, 2);
  end
  else
  begin
    { 4 × SmallInt zero trailer for compiled maps }
    Zero2 := 0;
    S.Write(Zero2, 2); S.Write(Zero2, 2);
    S.Write(Zero2, 2); S.Write(Zero2, 2);
  end;
end;

function TPMSTest.BuildMinimalPMS(PolyCount: Integer; MapID: LongInt): TMemoryStream;
begin
  Result := TMemoryStream.Create;
  WritePMSHeader(Result, 'TestMap', 'test.bmp', PolyCount, MapID);
  Result.Position := 0;
end;

{ ---- Tests: Real map loading ------------------------------------------- }

procedure TPMSTest.TestLoadRealMap_Arena;
var
  Data: TPMSData;
  Err: string;
  Res: TPMSLoadResult;
  MapPath: string;
begin
  MapPath := ExtractFilePath(ParamStr(0)) + '../' + MAPS_DIR + 'Arena.pms';
  if not FileExists(MapPath) then
  begin
    { Try alternate path }
    MapPath := '../../maps/Arena.pms';
  end;
  if not FileExists(MapPath) then
  begin
    { Skip if maps not found in test runner context }
    Exit;
  end;

  Res := LoadPMS(MapPath, Data, Err);
  AssertEquals('Arena.pms loaded OK', Ord(plrOK), Ord(Res));
  AssertEquals('Arena version', 11, Integer(Data.Version));
  AssertTrue('Arena poly count > 0', Data.PolyCount > 0);
  AssertTrue('Arena map name not empty', ReadPascalStr(Data.Options.MapName) <> '');
  AssertTrue('Arena texture name not empty', ReadPascalStr(Data.Options.TextureName) <> '');
  AssertTrue('Arena MapRandomID > 0', Data.Options.MapRandomID > 0);
end;

procedure TPMSTest.TestLoadRealMap_AllMaps;
var
  Data: TPMSData;
  Err: string;
  Res: TPMSLoadResult;
  SR: TSearchRec;
  MapsDir: string;
  Loaded, Failed: Integer;
  FailList: string;
begin
  MapsDir := '../../maps/';
  if not DirectoryExists(MapsDir) then
    Exit;  { skip if not found from test runner context }

  Loaded := 0; Failed := 0; FailList := '';
  if FindFirst(MapsDir + '*.pms', faAnyFile, SR) = 0 then
  begin
    repeat
      Res := LoadPMS(MapsDir + SR.Name, Data, Err);
      if Res = plrOK then
        Inc(Loaded)
      else
      begin
        Inc(Failed);
        FailList := FailList + SR.Name + ': ' + Err + #10;
      end;
    until FindNext(SR) <> 0;
    FindClose(SR);
  end;

  if Failed > 0 then
    Fail(Format('%d maps failed to load:%n%s', [Failed, FailList]));
  AssertTrue('At least some maps loaded', Loaded > 0);
end;

{ ---- Tests: Basic save/load -------------------------------------------- }

procedure TPMSTest.TestSaveLoadRoundTrip;
var
  Data, Data2: TPMSData;
  Err: string;
  Res: TPMSLoadResult;
  Fn: string;
begin
  FillChar(Data, SizeOf(Data), 0);
  WritePascalStr('RoundTripMap', Data.Options.MapName, 38);
  WritePascalStr('grass.bmp', Data.Options.TextureName, 24);
  Data.Options.StartJet := 150;
  Data.Options.GrenadePacks := 2;
  Data.PolyCount := 0;
  SetLength(Data.Polys, 0);

  Fn := TempFile('roundtrip.pms');
  AssertTrue('SavePMS succeeded', SavePMS(Fn, Data, Err));
  Res := LoadPMS(Fn, Data2, Err);
  AssertEquals('Reload OK', Ord(plrOK), Ord(Res));
  AssertEquals('MapName round-trip',
    ReadPascalStr(Data.Options.MapName),
    ReadPascalStr(Data2.Options.MapName));
  AssertEquals('TextureName round-trip',
    ReadPascalStr(Data.Options.TextureName),
    ReadPascalStr(Data2.Options.TextureName));
  AssertEquals('StartJet round-trip', Integer(Data.Options.StartJet),
                                      Integer(Data2.Options.StartJet));
  AssertEquals('GrenadePacks round-trip', Integer(Data.Options.GrenadePacks),
                                          Integer(Data2.Options.GrenadePacks));
  AssertEquals('MapRandomID=-1 in native', -1, Integer(Data2.Options.MapRandomID));
end;

procedure TPMSTest.TestSaveNativeMapRandomID;
var
  Data, Data2: TPMSData;
  Err: string;
  Fn: string;
begin
  FillChar(Data, SizeOf(Data), 0);
  Data.Options.MapRandomID := 999;  { should be overridden to -1 }
  Fn := TempFile('native_id.pms');
  AssertTrue('SavePMS succeeded', SavePMS(Fn, Data, Err));
  AssertEquals('Reload OK', Ord(plrOK), Ord(LoadPMS(Fn, Data2, Err)));
  AssertEquals('MapRandomID forced to -1', -1, Integer(Data2.Options.MapRandomID));
end;

{ ---- Tests: Options field validation ----------------------------------- }

procedure TPMSTest.TestOptionsMapName;
var
  Data: TPMSData;
  Err: string;
  Fn: string;
  Name: string;
begin
  FillChar(Data, SizeOf(Data), 0);
  WritePascalStr('My Test Map', Data.Options.MapName, 38);
  Fn := TempFile('mapname.pms');
  SavePMS(Fn, Data, Err);
  LoadPMS(Fn, Data, Err);
  Name := ReadPascalStr(Data.Options.MapName);
  AssertEquals('Map name preserved', 'My Test Map', Name);
end;

procedure TPMSTest.TestOptionsTextureName;
var
  Data: TPMSData;
  Err: string;
  Fn: string;
begin
  FillChar(Data, SizeOf(Data), 0);
  WritePascalStr('banana.bmp', Data.Options.TextureName, 24);
  Fn := TempFile('texname.pms');
  SavePMS(Fn, Data, Err);
  LoadPMS(Fn, Data, Err);
  AssertEquals('Texture name', 'banana.bmp',
    ReadPascalStr(Data.Options.TextureName));
end;

procedure TPMSTest.TestStartJetIs4Bytes;
{ Verifies that StartJet occupies 4 bytes in the file by checking that the
  TOptions struct compiles to the expected size. }
begin
  { If StartJet were 1 byte, TOptions would be 81 bytes, not 84.
    This test already covered by TestSizeOptions in test_types. }
  AssertEquals('TOptions is 84 bytes (StartJet is Long)', 84, SizeOf(TPMSOptions));
end;

procedure TPMSTest.TestStartJetRoundTrip;
var
  Data: TPMSData;
  Err: string;
  Fn: string;
begin
  FillChar(Data, SizeOf(Data), 0);
  Data.Options.StartJet := 500;
  Fn := TempFile('startjet.pms');
  SavePMS(Fn, Data, Err);
  LoadPMS(Fn, Data, Err);
  AssertEquals('StartJet 500 preserved', 500, Integer(Data.Options.StartJet));
end;

procedure TPMSTest.TestSectorTableAllZerosInNative;
var
  Data, Data2: TPMSData;
  Err: string;
  Fn: string;
  I, J: Integer;
begin
  FillChar(Data, SizeOf(Data), 0);
  Fn := TempFile('sector_zeros.pms');
  SavePMS(Fn, Data, Err);
  LoadPMS(Fn, Data2, Err);
  { In native format, all sector cells should have zero polygon count }
  for I := 0 to SECTOR_CELLS - 1 do
    for J := 0 to SECTOR_CELLS - 1 do
      AssertEquals(Format('Sector [%d][%d] count = 0', [I, J]),
        0, Integer(Data2.Sectors[I][J].PolyCount));
end;

{ ---- Tests: Error handling ---------------------------------------------- }

procedure TPMSTest.TestLoadMissingFile;
var
  Data: TPMSData;
  Err: string;
  Res: TPMSLoadResult;
begin
  Res := LoadPMS('/nonexistent/path/does_not_exist.pms', Data, Err);
  AssertEquals('Missing file result', Ord(plrFileNotFound), Ord(Res));
  AssertTrue('Error string set', Err <> '');
end;

procedure TPMSTest.TestLoadWrongVersion;
var
  S: TFileStream;
  Data: TPMSData;
  Err: string;
  Fn: string;
  Version: LongInt;
begin
  Fn := TempFile('badversion.pms');
  S := TFileStream.Create(Fn, fmCreate);
  Version := 99;  { wrong version }
  S.Write(Version, 4);
  S.Free;
  AssertEquals('Wrong version result',
    Ord(plrVersionMismatch), Ord(LoadPMS(Fn, Data, Err)));
end;

procedure TPMSTest.TestLoadTruncated;
var
  S: TFileStream;
  Data: TPMSData;
  Err: string;
  Fn: string;
  Version: LongInt;
begin
  Fn := TempFile('truncated.pms');
  S := TFileStream.Create(Fn, fmCreate);
  Version := 11;
  S.Write(Version, 4);
  { Write only the version — file is truncated before Options }
  S.Free;
  AssertEquals('Truncated file result',
    Ord(plrTruncated), Ord(LoadPMS(Fn, Data, Err)));
end;

initialization
  RegisterTest(TPMSTest);

end.
