unit test_types;

{$mode objfpc}{$H+}

interface

uses
  fpcunit, testregistry, SysUtils,
  pw.types, pw.utils;

type
  TTypesTest = class(TTestCase)
  published
    { ---- SizeOf assertions for file-compatible types ---- }
    procedure TestSizeVertex;
    procedure TestSizeNormal;
    procedure TestSizePolyNormals;
    procedure TestSizePolygon;
    procedure TestSizePolyEntry;
    procedure TestSizeProp;
    procedure TestSizeSceneryName;
    procedure TestSizeCollider;
    procedure TestSizeSpawnPoint;
    procedure TestSizeWaypoint;
    procedure TestSizeLight;
    procedure TestSizeSketchVertex;
    procedure TestSizeSketchLine;
    procedure TestSizeOptions;

    { ---- Colour helper round-trips ---- }
    procedure TestMakeARGBRoundTrip;
    procedure TestARGBWithAlpha;
    procedure TestColor3ToARGB;
    procedure TestARGBToColor3;
    procedure TestParseHexRGB;

    { ---- Math helpers ---- }
    procedure TestMidpoint;
    procedure TestClamp;
    procedure TestIsBetween;
    procedure TestVBInt;
    procedure TestReadPascalStr;
    procedure TestWritePascalStr;
  end;

implementation

{ ---- SizeOf tests ------------------------------------------------------- }

procedure TTypesTest.TestSizeVertex;
begin
  { TCustomVertex in VB6: 7 × Single/Long = 7 × 4 = 28 bytes }
  AssertEquals('TPMSVertex size', 28, SizeOf(TPMSVertex));
end;

procedure TTypesTest.TestSizeNormal;
begin
  { TVertexHit in VB6: 3 × Single = 12 bytes }
  AssertEquals('TPMSNormal size', 12, SizeOf(TPMSNormal));
end;

procedure TTypesTest.TestSizePolyNormals;
begin
  { TPolyHit in VB6: 3 × TVertexHit = 36 bytes }
  AssertEquals('TPMSPolyNormals size', 36, SizeOf(TPMSPolyNormals));
end;

procedure TTypesTest.TestSizePolygon;
begin
  { TPolygon in VB6: 3×TCustomVertex + TPolyHit = 84 + 36 = 120 bytes }
  AssertEquals('TPMSPolygon size', 120, SizeOf(TPMSPolygon));
end;

procedure TTypesTest.TestSizePolyEntry;
begin
  { TMapFile_Polygon: TPolygon + polyType byte = 121 bytes }
  AssertEquals('TPMSPolyEntry size', 121, SizeOf(TPMSPolyEntry));
end;

procedure TTypesTest.TestSizeProp;
begin
  { TProp in VB6: Boolean(2)+Integer(2)+Long(4)+Long(4)+5×Single(20)+3×Long(12) = 44 bytes }
  AssertEquals('TPMSProp size', 44, SizeOf(TPMSProp));
end;

procedure TTypesTest.TestSizeSceneryName;
begin
  { TMapFile_Scenery: 51 bytes name + 4 bytes date = 55 bytes }
  AssertEquals('TPMSSceneryName size', 55, SizeOf(TPMSSceneryName));
end;

procedure TTypesTest.TestSizeCollider;
begin
  { TCollider in VB6: Long + 3×Single = 16 bytes }
  AssertEquals('TPMSCollider size', 16, SizeOf(TPMSCollider));
end;

procedure TTypesTest.TestSizeSpawnPoint;
begin
  { TSaveSpawnPoint in VB6: 4×Long = 16 bytes (X/Y are Long, not Single) }
  AssertEquals('TPMSSpawnPoint size', 16, SizeOf(TPMSSpawnPoint));
end;

procedure TTypesTest.TestSizeWaypoint;
begin
  { TNewWaypoint: 4+4+4+4 + 7 + 5 + 4 + 80 = 112 bytes }
  AssertEquals('TPMSWaypoint size', 112, SizeOf(TPMSWaypoint));
end;

procedure TTypesTest.TestSizeLight;
begin
  { TLightSource: Byte(1)+TColor(3)+Single(4)+Integer(2)+3×Single(12) = 22 bytes }
  AssertEquals('TPMSLight size', 22, SizeOf(TPMSLight));
end;

procedure TTypesTest.TestSizeSketchVertex;
begin
  { TSketchVertex: 3×Single = 12 bytes }
  AssertEquals('TPMSSketchVertex size', 12, SizeOf(TPMSSketchVertex));
end;

procedure TTypesTest.TestSizeSketchLine;
begin
  { TSketchLine: 2×TSketchVertex = 24 bytes }
  AssertEquals('TPMSSketchLine size', 24, SizeOf(TPMSSketchLine));
end;

procedure TTypesTest.TestSizeOptions;
begin
  { TOptions: 39+25+4+4+4+1+1+1+1+4 = 84 bytes }
  AssertEquals('TPMSOptions size', 84, SizeOf(TPMSOptions));
end;

{ ---- Colour helpers ---------------------------------------------------- }

procedure TTypesTest.TestMakeARGBRoundTrip;
var
  C: LongWord;
begin
  C := MakeARGB(255, 128, 64, 32);
  AssertEquals('Alpha', 255, Integer(GetAlpha(C)));
  AssertEquals('Red',   128, Integer(GetRed(C)));
  AssertEquals('Green',  64, Integer(GetGreen(C)));
  AssertEquals('Blue',   32, Integer(GetBlue(C)));
end;

procedure TTypesTest.TestARGBWithAlpha;
var
  C: LongWord;
begin
  C := MakeARGB(0, 100, 150, 200);
  C := ARGBWithAlpha(127, C);
  AssertEquals('Alpha after set', 127, Integer(GetAlpha(C)));
  AssertEquals('Red preserved',  100, Integer(GetRed(C)));
  AssertEquals('Green preserved', 150, Integer(GetGreen(C)));
  AssertEquals('Blue preserved',  200, Integer(GetBlue(C)));
end;

procedure TTypesTest.TestColor3ToARGB;
var
  C3: TColor3;
  ARGB: LongWord;
begin
  C3.R := 10; C3.G := 20; C3.B := 30;
  ARGB := Color3ToARGB(C3, 200);
  AssertEquals('Alpha', 200, Integer(GetAlpha(ARGB)));
  AssertEquals('R', 10, Integer(GetRed(ARGB)));
  AssertEquals('G', 20, Integer(GetGreen(ARGB)));
  AssertEquals('B', 30, Integer(GetBlue(ARGB)));
end;

procedure TTypesTest.TestARGBToColor3;
var
  C3: TColor3;
  ARGB: LongWord;
begin
  ARGB := MakeARGB(255, 77, 88, 99);
  C3 := ARGBToColor3(ARGB);
  AssertEquals('R', 77, Integer(C3.R));
  AssertEquals('G', 88, Integer(C3.G));
  AssertEquals('B', 99, Integer(C3.B));
end;

procedure TTypesTest.TestParseHexRGB;
begin
  AssertEquals('Red hex',  $FF0000, Integer(ParseHexRGB('FF0000')));
  AssertEquals('Green hex', $00FF00, Integer(ParseHexRGB('00FF00')));
  AssertEquals('Blue hex',  $0000FF, Integer(ParseHexRGB('0000FF')));
  AssertEquals('Black', 0, Integer(ParseHexRGB('000000')));
  AssertEquals('White', $FFFFFF, Integer(ParseHexRGB('FFFFFF')));
  AssertEquals('Invalid', 0, Integer(ParseHexRGB('')));
end;

{ ---- Math helpers ------------------------------------------------------- }

procedure TTypesTest.TestMidpoint;
begin
  AssertEquals('Midpoint 0,10', 5.0, Midpoint(0, 10));
  AssertEquals('Midpoint -5,5', 0.0, Midpoint(-5, 5));
  AssertEquals('Midpoint same', 3.0, Midpoint(3, 3));
end;

procedure TTypesTest.TestClamp;
begin
  AssertEquals('Clamp below', 1.0, Clamp(0.0, 1.0, 5.0));
  AssertEquals('Clamp above', 5.0, Clamp(9.0, 1.0, 5.0));
  AssertEquals('Clamp inside', 3.0, Clamp(3.0, 1.0, 5.0));
  AssertEquals('Clamp at lo', 1.0, Clamp(1.0, 1.0, 5.0));
  AssertEquals('Clamp at hi', 5.0, Clamp(5.0, 1.0, 5.0));
end;

procedure TTypesTest.TestIsBetween;
begin
  AssertTrue('IsBetween inside', IsBetween(3.0, 1.0, 5.0));
  AssertTrue('IsBetween at lo', IsBetween(1.0, 1.0, 5.0));
  AssertTrue('IsBetween at hi', IsBetween(5.0, 1.0, 5.0));
  AssertFalse('IsBetween below', IsBetween(0.9, 1.0, 5.0));
  AssertFalse('IsBetween above', IsBetween(5.1, 1.0, 5.0));
end;

procedure TTypesTest.TestVBInt;
begin
  AssertEquals('VBInt positive', 3, VBInt(3.9));
  AssertEquals('VBInt exact', 3, VBInt(3.0));
  AssertEquals('VBInt negative truncate toward -inf', -4, VBInt(-3.1));
  AssertEquals('VBInt negative exact', -3, VBInt(-3.0));
  AssertEquals('VBInt zero', 0, VBInt(0.0));
end;

procedure TTypesTest.TestReadPascalStr;
var
  Buf: array[0..10] of Byte;
begin
  FillChar(Buf, SizeOf(Buf), 0);
  Buf[0] := 3;
  Buf[1] := Ord('H');
  Buf[2] := Ord('i');
  Buf[3] := Ord('!');
  AssertEquals('ReadPascalStr', 'Hi!', ReadPascalStr(Buf));
end;

procedure TTypesTest.TestWritePascalStr;
var
  Buf: array[0..10] of Byte;
  S: string;
begin
  FillChar(Buf, SizeOf(Buf), $FF);  { pre-fill with non-zero }
  WritePascalStr('Hello', Buf, 9);
  AssertEquals('Length byte', 5, Integer(Buf[0]));
  AssertEquals('First char', Ord('H'), Integer(Buf[1]));
  AssertEquals('Last char', Ord('o'), Integer(Buf[5]));
  AssertEquals('Pad byte', 0, Integer(Buf[6]));
  S := ReadPascalStr(Buf);
  AssertEquals('Round-trip', 'Hello', S);
end;

initialization
  RegisterTest(TTypesTest);

end.
