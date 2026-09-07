unit pw.lights;

{$mode objfpc}{$H+}
{$PackRecords 1}

interface

uses
  pw.types, pw.map;

{ Apply all lights in Doc.Lights to Doc.Polys[].ScreenV.Color in place.
  Reads base colors from Doc.Polys[].BaseColor.
  Does not touch WorldV. }
procedure ApplyLights(Doc: TMapDocument);

{ Restore ScreenV.Color from BaseColor for all polygons. }
procedure RestoreBaseColors(Doc: TMapDocument);

{ Apply lights to a single vertex only (used after editing). }
procedure ApplyLightsToVertex(Doc: TMapDocument; PolyIdx, VertIdx: Integer);

implementation

uses
  Math, pw.utils;

const
  EDITOR_VERTEX_Z = 1.0;

function VertexAlpha(const Poly: TEditorPoly; VertIdx: Integer): Byte;
begin
  if Poly.ScreenV[VertIdx].Color <> 0 then
    Result := GetAlpha(Poly.ScreenV[VertIdx].Color)
  else
    Result := Poly.V[VertIdx].Alpha;
end;

function BaseVertexARGB(const Poly: TEditorPoly; VertIdx: Integer): LongWord;
var
  Base: TColor3;
begin
  Base := Poly.BaseColor[VertIdx];
  Result := MakeARGB(VertexAlpha(Poly, VertIdx), Base.R, Base.G, Base.B);
end;

procedure ComputePolyNormal(const Poly: TEditorPoly; out NX, NY, NZ: Single);
var
  V1X, V1Y, V1Z: Single;
  V2X, V2Y, V2Z: Single;
  Mag: Single;
begin
  V1X := Poly.V[1].World.X - Poly.V[2].World.X;
  V1Y := Poly.V[1].World.Y - Poly.V[2].World.Y;
  V1Z := EDITOR_VERTEX_Z - EDITOR_VERTEX_Z;

  V2X := Poly.V[1].World.X - Poly.V[3].World.X;
  V2Y := Poly.V[1].World.Y - Poly.V[3].World.Y;
  V2Z := EDITOR_VERTEX_Z - EDITOR_VERTEX_Z;

  NX := (V1Y * V2Z) - (V1Z * V2Y);
  NY := (V1Z * V2X) - (V1X * V2Z);
  NZ := (V1X * V2Y) - (V1Y * V2X);

  Mag := Sqrt(Sqr(NX) + Sqr(NY) + Sqr(NZ));
  if Mag > 0 then
  begin
    NX := NX / Mag;
    NY := NY / Mag;
    NZ := NZ / Mag;
  end
  else
  begin
    NX := 0;
    NY := 0;
    NZ := 0;
  end;
end;

procedure ApplyLightsToVertex(Doc: TMapDocument; PolyIdx, VertIdx: Integer);
var
  NX, NY, NZ: Single;
  Base: TColor3;
  LightDirX, LightDirY, LightDirZ: Single;
  LightMag, DiffuseFactor, Falloff, Factor: Single;
  RVal, GVal, BVal: Single;
  K: Integer;
begin
  if (Doc = nil) or (PolyIdx < 0) or (PolyIdx >= Doc.PolyCount) or
     (VertIdx < 1) or (VertIdx > 3) then
    Exit;

  Doc.Polys[PolyIdx].ScreenV[VertIdx].Color := BaseVertexARGB(Doc.Polys[PolyIdx], VertIdx);
  if Doc.LightCount <= 0 then
    Exit;

  ComputePolyNormal(Doc.Polys[PolyIdx], NX, NY, NZ);
  Base := Doc.Polys[PolyIdx].BaseColor[VertIdx];
  RVal := Base.R;
  GVal := Base.G;
  BVal := Base.B;

  for K := 0 to Doc.LightCount - 1 do
  begin
    LightDirX := Doc.Lights[K].X - Doc.Polys[PolyIdx].V[VertIdx].World.X;
    LightDirY := Doc.Lights[K].Y - Doc.Polys[PolyIdx].V[VertIdx].World.Y;
    LightDirZ := Doc.Lights[K].Z - EDITOR_VERTEX_Z;

    LightMag := Sqrt(Sqr(LightDirX) + Sqr(LightDirY) + Sqr(LightDirZ));
    if LightMag > 0 then
    begin
      LightDirX := LightDirX / LightMag;
      LightDirY := LightDirY / LightMag;
      LightDirZ := LightDirZ / LightMag;
    end
    else
    begin
      LightDirX := 0;
      LightDirY := 0;
      LightDirZ := 0;
    end;

    DiffuseFactor := (NX * LightDirX) + (NY * LightDirY) + (NZ * LightDirZ);
    if DiffuseFactor < 0 then
      DiffuseFactor := 0;

    if Doc.Lights[K].Range <= 0 then
      Falloff := 1.0
    else if LightMag <= Doc.Lights[K].Range then
      Falloff := 1.0 - (LightMag / Doc.Lights[K].Range)
    else
      Falloff := 0.0;

    Factor := Doc.Lights[K].Intensity * DiffuseFactor * Falloff;
    if Factor <= 0 then
      Continue;

    RVal := RVal + (Doc.Lights[K].Color.R * Factor);
    GVal := GVal + (Doc.Lights[K].Color.G * Factor);
    BVal := BVal + (Doc.Lights[K].Color.B * Factor);
  end;

  if RVal > 255 then RVal := 255;
  if GVal > 255 then GVal := 255;
  if BVal > 255 then BVal := 255;

  Doc.Polys[PolyIdx].ScreenV[VertIdx].Color := MakeARGB(
    VertexAlpha(Doc.Polys[PolyIdx], VertIdx),
    Byte(Round(RVal)), Byte(Round(GVal)), Byte(Round(BVal)));
end;

procedure RestoreBaseColors(Doc: TMapDocument);
var
  I, J: Integer;
begin
  if Doc = nil then
    Exit;

  for I := 0 to Doc.PolyCount - 1 do
    for J := 1 to 3 do
      Doc.Polys[I].ScreenV[J].Color := BaseVertexARGB(Doc.Polys[I], J);
end;

procedure ApplyLights(Doc: TMapDocument);
var
  I, J: Integer;
begin
  if (Doc = nil) or (Doc.LightCount <= 0) then
    Exit;

  for I := 0 to Doc.PolyCount - 1 do
    for J := 1 to 3 do
      ApplyLightsToVertex(Doc, I, J);
end;

end.
