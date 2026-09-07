unit test_lights;

{$mode objfpc}{$H+}

interface

uses
  fpcunit, testregistry,
  pw.types, pw.utils, pw.map, pw.lights;

type
  TLightsTest = class(TTestCase)
  private
    function BuildDoc(Alpha: Byte = 200): TMapDocument;
    function BaseARGB(const Doc: TMapDocument; VertIdx: Integer): LongWord;
    procedure AddLight(Doc: TMapDocument; X, Y, Z: Single; const Color: TColor3;
      Intensity, Range: Single);
  published
    procedure TestNoLightsNoChange;
    procedure TestRestoreBaseColors;
    procedure TestLightOutsideRange;
    procedure TestLightInsideRange;
    procedure TestIntensityZero;
    procedure TestAlphaPreserved;
  end;

implementation

function MakeColor3(R, G, B: Byte): TColor3;
begin
  Result.R := R;
  Result.G := G;
  Result.B := B;
end;

function TLightsTest.BuildDoc(Alpha: Byte): TMapDocument;
var
  P: TEditorPoly;
begin
  Result := TMapDocument.Create;
  Result.PolyCount := 1;
  SetLength(Result.Polys, 1);
  FillChar(P, SizeOf(P), 0);

  P.V[1].World.X := 0;   P.V[1].World.Y := 0;
  P.V[2].World.X := 100; P.V[2].World.Y := 0;
  P.V[3].World.X := 50;  P.V[3].World.Y := 100;

  P.BaseColor[1] := MakeColor3(10, 20, 30);
  P.BaseColor[2] := MakeColor3(40, 50, 60);
  P.BaseColor[3] := MakeColor3(70, 80, 90);

  P.V[1].Color := P.BaseColor[1]; P.V[1].Alpha := Alpha;
  P.V[2].Color := P.BaseColor[2]; P.V[2].Alpha := Alpha;
  P.V[3].Color := P.BaseColor[3]; P.V[3].Alpha := Alpha;

  P.ScreenV[1].Color := Color3ToARGB(P.BaseColor[1], Alpha);
  P.ScreenV[2].Color := Color3ToARGB(P.BaseColor[2], Alpha);
  P.ScreenV[3].Color := Color3ToARGB(P.BaseColor[3], Alpha);

  Result.Polys[0] := P;
end;

function TLightsTest.BaseARGB(const Doc: TMapDocument; VertIdx: Integer): LongWord;
begin
  Result := Color3ToARGB(Doc.Polys[0].BaseColor[VertIdx], Doc.Polys[0].V[VertIdx].Alpha);
end;

procedure TLightsTest.AddLight(Doc: TMapDocument; X, Y, Z: Single;
  const Color: TColor3; Intensity, Range: Single);
begin
  Doc.LightCount := 1;
  SetLength(Doc.Lights, 1);
  FillChar(Doc.Lights[0], SizeOf(TEditorLight), 0);
  Doc.Lights[0].X := X;
  Doc.Lights[0].Y := Y;
  Doc.Lights[0].Z := Z;
  Doc.Lights[0].Color := Color;
  Doc.Lights[0].Intensity := Intensity;
  Doc.Lights[0].Range := Range;
end;

procedure TLightsTest.TestNoLightsNoChange;
var
  Doc: TMapDocument;
  Before: LongWord;
begin
  Doc := BuildDoc;
  try
    Before := Doc.Polys[0].ScreenV[1].Color;
    ApplyLights(Doc);
    AssertTrue('vertex colour unchanged without lights',
      Doc.Polys[0].ScreenV[1].Color = Before);
  finally
    Doc.Free;
  end;
end;

procedure TLightsTest.TestRestoreBaseColors;
var
  Doc: TMapDocument;
begin
  Doc := BuildDoc;
  try
    AddLight(Doc, 0, 0, 255, MakeColor3(255, 255, 255), 1.0, 1000.0);
    ApplyLights(Doc);
    AssertFalse('lighting should change vertex 1 colour',
      Doc.Polys[0].ScreenV[1].Color = BaseARGB(Doc, 1));

    RestoreBaseColors(Doc);
    AssertTrue('vertex 1 restored', Doc.Polys[0].ScreenV[1].Color = BaseARGB(Doc, 1));
    AssertTrue('vertex 2 restored', Doc.Polys[0].ScreenV[2].Color = BaseARGB(Doc, 2));
    AssertTrue('vertex 3 restored', Doc.Polys[0].ScreenV[3].Color = BaseARGB(Doc, 3));
  finally
    Doc.Free;
  end;
end;

procedure TLightsTest.TestLightOutsideRange;
var
  Doc: TMapDocument;
begin
  Doc := BuildDoc;
  try
    AddLight(Doc, 1000, 1000, 255, MakeColor3(255, 255, 255), 1.0, 10.0);
    ApplyLightsToVertex(Doc, 0, 1);
    AssertTrue('out-of-range light does not change colour',
      Doc.Polys[0].ScreenV[1].Color = BaseARGB(Doc, 1));
  finally
    Doc.Free;
  end;
end;

procedure TLightsTest.TestLightInsideRange;
var
  Doc: TMapDocument;
  Color: LongWord;
begin
  Doc := BuildDoc;
  try
    AddLight(Doc, 0, 0, 255, MakeColor3(255, 255, 255), 1.0, 1000.0);
    ApplyLightsToVertex(Doc, 0, 1);
    Color := Doc.Polys[0].ScreenV[1].Color;
    AssertTrue('red brightened', GetRed(Color) > Doc.Polys[0].BaseColor[1].R);
    AssertTrue('green brightened', GetGreen(Color) > Doc.Polys[0].BaseColor[1].G);
    AssertTrue('blue brightened', GetBlue(Color) > Doc.Polys[0].BaseColor[1].B);
  finally
    Doc.Free;
  end;
end;

procedure TLightsTest.TestIntensityZero;
var
  Doc: TMapDocument;
begin
  Doc := BuildDoc;
  try
    AddLight(Doc, 0, 0, 255, MakeColor3(255, 255, 255), 0.0, 1000.0);
    ApplyLightsToVertex(Doc, 0, 1);
    AssertTrue('zero intensity preserves base colour',
      Doc.Polys[0].ScreenV[1].Color = BaseARGB(Doc, 1));
  finally
    Doc.Free;
  end;
end;

procedure TLightsTest.TestAlphaPreserved;
var
  Doc: TMapDocument;
begin
  Doc := BuildDoc(123);
  try
    AddLight(Doc, 0, 0, 255, MakeColor3(255, 255, 255), 1.0, 1000.0);
    ApplyLights(Doc);
    AssertEquals('alpha preserved', 123, Integer(GetAlpha(Doc.Polys[0].ScreenV[1].Color)));
  finally
    Doc.Free;
  end;
end;

initialization
  RegisterTest(TLightsTest);

end.
