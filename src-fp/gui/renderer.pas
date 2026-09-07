unit renderer;

{$mode objfpc}{$H+}
{$PackRecords 1}

{
  TRenderer — OpenGL 2.1 map renderer for PolyWorks.

  Render order (matching VB6 Render() procedure):
    1. Clear to background colour
    2. Background gradient quad (if ShowBackground)
    3. Back-layer scenery (level=0)  [OpenGL quads with texture, rotation, scale]
    4. Mid-layer scenery (level=1)
    5. Depth-buffer scenery (types 24, 25) — rendered before main polys
    6. Main polygon pass (all non-24/25 types) with map texture
    7. Selection highlight overlay (pattern texture or coloured wireframe)
    8. Front-layer scenery (level=2)
    9. Wireframe overlay (if ShowWireframe)
   10. Vertex points (if ShowPoints)
   11. Grid (if ShowGrid)
   12. Spawn points, colliders, waypoints, connections (if ShowObjects/ShowWaypoints)
   13. Lights (if ShowLights)
   14. Sketch lines (if ShowSketch)

  All screen coordinates come from Doc.Polys[].V[].Screen which are
  pre-computed by TMapDocument.RebuildScreenCache.

  Colour key transparency: any pixel with R<10, G>245, B<10 → alpha=0.
}

interface

uses
  SysUtils, Math,
  GL, GLU,
  pw.types, pw.utils, pw.map, pw.stbimage;

type
  TViewSettings = record
    ShowPolys      : Boolean;
    ShowWireframe  : Boolean;
    ShowPoints     : Boolean;
    ShowGrid       : Boolean;
    ShowObjects    : Boolean;
    ShowWaypoints  : Boolean;
    ShowLights     : Boolean;
    ShowSketch     : Boolean;
    ShowTexture    : Boolean;
    ShowBackground : Boolean;
    ShowScenery    : Boolean;
    GridSize       : Integer;
  end;

  TTexInfo = record
    TexID   : GLuint;
    Width   : Integer;
    Height  : Integer;
    Loaded  : Boolean;
  end;

  TRenderer = class
  private
    FMapTex      : TTexInfo;
    FScenTex     : array of TTexInfo;
    FScenTexCount: Integer;
    FViewW, FViewH: Integer;

    procedure SetOrtho(W, H: Integer);
    procedure ApplyColorKey(Data: PByte; Width, Height: Integer);
    function  UploadTexture(Data: PByte; Width, Height: Integer): GLuint;
    procedure FreeTexInfo(var T: TTexInfo);
    { DrawPolygonsByType: if DepthOnly=True draws types 24/25 only, else draws the rest }
    procedure DrawPolygonsByType(const Doc: TMapDocument; const VS: TViewSettings;
                                 DepthOnly: Boolean);
    procedure DrawSceneryLayer(const Doc: TMapDocument; Level: Byte);
    procedure DrawWireframe(const Doc: TMapDocument);
    procedure DrawPoints(const Doc: TMapDocument);
    procedure DrawGrid(const Doc: TMapDocument; const VS: TViewSettings);
    procedure DrawObjects(const Doc: TMapDocument);
    procedure DrawWaypoints(const Doc: TMapDocument);
    procedure DrawLights(const Doc: TMapDocument);
    procedure DrawSketch(const Doc: TMapDocument);
    procedure DrawBackground(const Doc: TMapDocument);
    procedure DrawSelectionHighlight(const Doc: TMapDocument);
  public
    constructor Create;
    destructor  Destroy; override;

    procedure LoadMapTexture(const Path: string);
    procedure LoadSceneryTexture(Index: Integer; const Path: string);
    procedure FreeTextures;

    { Called with a valid OpenGL context active. }
    procedure Render(const Doc: TMapDocument; const VS: TViewSettings;
                     ViewW, ViewH: Integer);
  end;

function DefaultViewSettings: TViewSettings;

implementation

const
  { Poly types 24 and 25 are depth-buffer (background) polygons }
  POLYTYPE_DEPTH_BUF = 24;
  POLYTYPE_DEPTH_BUF2 = 25;

  { Selection overlay alpha }
  SEL_ALPHA = 0.3;

  { Object display sizes }
  SPAWN_RADIUS  = 6.0;
  COLLIDER_SEG  = 32;  { circle segments }
  WP_RADIUS     = 5.0;
  LIGHT_RADIUS  = 4.0;

function DefaultViewSettings: TViewSettings;
begin
  FillChar(Result, SizeOf(Result), 0);
  Result.ShowPolys      := True;
  Result.ShowWireframe  := False;
  Result.ShowPoints     := False;
  Result.ShowGrid       := False;
  Result.ShowObjects    := True;
  Result.ShowWaypoints  := True;
  Result.ShowLights     := True;
  Result.ShowSketch     := True;
  Result.ShowTexture    := True;
  Result.ShowBackground := True;
  Result.ShowScenery    := True;
  Result.GridSize       := 10;
end;

{ ---- TRenderer ---------------------------------------------------------- }

constructor TRenderer.Create;
begin
  inherited Create;
  FillChar(FMapTex, SizeOf(FMapTex), 0);
  FScenTexCount := 0;
  SetLength(FScenTex, 0);
end;

destructor TRenderer.Destroy;
begin
  FreeTextures;
  inherited;
end;

procedure TRenderer.ApplyColorKey(Data: PByte; Width, Height: Integer);
{ Bright-green colour key: R<10, G>245, B<10 → alpha=0 }
var
  I, Pixels: Integer;
  P: PByte;
begin
  Pixels := Width * Height;
  P := Data;
  for I := 0 to Pixels - 1 do
  begin
    if (P[0] < 10) and (P[1] > 245) and (P[2] < 10) then
      P[3] := 0;
    Inc(P, 4);
  end;
end;

function TRenderer.UploadTexture(Data: PByte; Width, Height: Integer): GLuint;
var
  TexID: GLuint;
begin
  glGenTextures(1, @TexID);
  glBindTexture(GL_TEXTURE_2D, TexID);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, Width, Height, 0,
               GL_RGBA, GL_UNSIGNED_BYTE, Data);
  Result := TexID;
end;

procedure TRenderer.FreeTexInfo(var T: TTexInfo);
begin
  if T.Loaded and (T.TexID <> 0) then
  begin
    glDeleteTextures(1, @T.TexID);
    T.TexID := 0;
  end;
  T.Loaded := False;
end;

procedure TRenderer.LoadMapTexture(const Path: string);
var
  Data: PByte;
  W, H: Integer;
begin
  FreeTexInfo(FMapTex);
  if not FileExists(Path) then Exit;
  Data := StbLoad(Path, W, H);
  if Data = nil then Exit;
  ApplyColorKey(Data, W, H);
  FMapTex.TexID  := UploadTexture(Data, W, H);
  FMapTex.Width  := W;
  FMapTex.Height := H;
  FMapTex.Loaded := True;
  StbFree(Data);
end;

procedure TRenderer.LoadSceneryTexture(Index: Integer; const Path: string);
var
  Data: PByte;
  W, H: Integer;
begin
  if Index < 1 then Exit;
  if Index >= FScenTexCount then
  begin
    SetLength(FScenTex, Index + 1);
    while FScenTexCount <= Index do
    begin
      FillChar(FScenTex[FScenTexCount], SizeOf(TTexInfo), 0);
      Inc(FScenTexCount);
    end;
  end;
  FreeTexInfo(FScenTex[Index]);
  if not FileExists(Path) then Exit;
  Data := StbLoad(Path, W, H);
  if Data = nil then Exit;
  ApplyColorKey(Data, W, H);
  FScenTex[Index].TexID  := UploadTexture(Data, W, H);
  FScenTex[Index].Width  := W;
  FScenTex[Index].Height := H;
  FScenTex[Index].Loaded := True;
  StbFree(Data);
end;

procedure TRenderer.FreeTextures;
var I: Integer;
begin
  FreeTexInfo(FMapTex);
  for I := 0 to FScenTexCount - 1 do
    FreeTexInfo(FScenTex[I]);
  SetLength(FScenTex, 0);
  FScenTexCount := 0;
end;

procedure TRenderer.SetOrtho(W, H: Integer);
begin
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity;
  glOrtho(0, W, H, 0, -1, 1);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity;
end;

{ ---- Drawing helpers ---------------------------------------------------- }

procedure DrawCircle(CX, CY, R: Single; Segments: Integer);
var
  I: Integer;
  Angle: Single;
begin
  glBegin(GL_LINE_LOOP);
  for I := 0 to Segments - 1 do
  begin
    Angle := 2 * Pi * I / Segments;
    glVertex2f(CX + Cos(Angle) * R, CY + Sin(Angle) * R);
  end;
  glEnd;
end;

procedure DrawFilledCircle(CX, CY, R: Single; Segments: Integer);
var
  I: Integer;
  Angle: Single;
begin
  glBegin(GL_TRIANGLE_FAN);
  glVertex2f(CX, CY);
  for I := 0 to Segments do
  begin
    Angle := 2 * Pi * I / Segments;
    glVertex2f(CX + Cos(Angle) * R, CY + Sin(Angle) * R);
  end;
  glEnd;
end;

{ ---- Background --------------------------------------------------------- }

procedure TRenderer.DrawBackground(const Doc: TMapDocument);
var
  C1, C2: TColor3;
  R1,G1,B1, R2,G2,B2: Single;
begin
  C1 := ARGBToColor3(Doc.Options.BgColor1);
  C2 := ARGBToColor3(Doc.Options.BgColor2);
  R1 := C1.R / 255; G1 := C1.G / 255; B1 := C1.B / 255;
  R2 := C2.R / 255; G2 := C2.G / 255; B2 := C2.B / 255;

  glDisable(GL_TEXTURE_2D);
  glBegin(GL_TRIANGLE_STRIP);
    glColor3f(R1, G1, B1); glVertex2f(0, 0);
    glColor3f(R1, G1, B1); glVertex2f(FViewW, 0);
    glColor3f(R2, G2, B2); glVertex2f(0, FViewH);
    glColor3f(R2, G2, B2); glVertex2f(FViewW, FViewH);
  glEnd;
end;

{ ---- Polygons ----------------------------------------------------------- }

procedure TRenderer.DrawPolygonsByType(const Doc: TMapDocument; const VS: TViewSettings;
                                       DepthOnly: Boolean);
var
  I, J: Integer;
  P: ^TEditorPoly;
  C: LongWord;
  R, G, B, A: Single;
  IsDepth: Boolean;
begin
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  if VS.ShowTexture and FMapTex.Loaded then
  begin
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, FMapTex.TexID);
  end
  else
    glDisable(GL_TEXTURE_2D);

  glBegin(GL_TRIANGLES);
  for I := 0 to Doc.PolyCount - 1 do
  begin
    P := @Doc.Polys[I];
    IsDepth := P^.PolyType in [POLYTYPE_DEPTH_BUF, POLYTYPE_DEPTH_BUF2];
    if DepthOnly <> IsDepth then Continue;

    for J := 1 to 3 do
    begin
      C := P^.ScreenV[J].Color;
      A := GetAlpha(C) / 255;
      R := GetRed(C)   / 255;
      G := GetGreen(C) / 255;
      B := GetBlue(C)  / 255;
      glColor4f(R, G, B, A);
      glTexCoord2f(P^.V[J].Tu, P^.V[J].Tv);
      glVertex2f(P^.V[J].Screen.X, P^.V[J].Screen.Y);
    end;
  end;
  glEnd;

  glDisable(GL_TEXTURE_2D);
  glDisable(GL_BLEND);
end;

procedure TRenderer.DrawSelectionHighlight(const Doc: TMapDocument);
{ Draw selected polygons with a semi-transparent overlay }
var
  I, J: Integer;
  P: ^TEditorPoly;
begin
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glDisable(GL_TEXTURE_2D);

  for I := 0 to Doc.PolyCount - 1 do
  begin
    P := @Doc.Polys[I];
    if not (P^.Selected[1] or P^.Selected[2] or P^.Selected[3]) then Continue;

    { Polygon type colour hint }
    glColor4f(1.0, 1.0, 0.0, 0.25);
    glBegin(GL_TRIANGLES);
    for J := 1 to 3 do
      glVertex2f(P^.V[J].Screen.X, P^.V[J].Screen.Y);
    glEnd;

    { Highlight selected vertices }
    glPointSize(8.0);
    glBegin(GL_POINTS);
    for J := 1 to 3 do
      if P^.Selected[J] then
      begin
        glColor4f(1.0, 0.0, 0.0, 0.9);
        glVertex2f(P^.V[J].Screen.X, P^.V[J].Screen.Y);
      end;
    glEnd;
    glPointSize(1.0);
  end;

  glDisable(GL_BLEND);
end;

{ ---- Wireframe & Points ------------------------------------------------- }

procedure TRenderer.DrawWireframe(const Doc: TMapDocument);
var I: Integer; P: ^TEditorPoly;
begin
  glDisable(GL_TEXTURE_2D);
  glColor3f(0.4, 0.4, 0.4);
  glBegin(GL_LINES);
  for I := 0 to Doc.PolyCount - 1 do
  begin
    P := @Doc.Polys[I];
    glVertex2f(P^.V[1].Screen.X, P^.V[1].Screen.Y);
    glVertex2f(P^.V[2].Screen.X, P^.V[2].Screen.Y);
    glVertex2f(P^.V[2].Screen.X, P^.V[2].Screen.Y);
    glVertex2f(P^.V[3].Screen.X, P^.V[3].Screen.Y);
    glVertex2f(P^.V[3].Screen.X, P^.V[3].Screen.Y);
    glVertex2f(P^.V[1].Screen.X, P^.V[1].Screen.Y);
  end;
  glEnd;
end;

procedure TRenderer.DrawPoints(const Doc: TMapDocument);
var I, J: Integer; P: ^TEditorPoly;
begin
  glDisable(GL_TEXTURE_2D);
  glColor3f(1.0, 1.0, 0.0);
  glPointSize(4.0);
  glBegin(GL_POINTS);
  for I := 0 to Doc.PolyCount - 1 do
  begin
    P := @Doc.Polys[I];
    for J := 1 to 3 do
      glVertex2f(P^.V[J].Screen.X, P^.V[J].Screen.Y);
  end;
  glEnd;
  glPointSize(1.0);
end;

{ ---- Grid --------------------------------------------------------------- }

procedure TRenderer.DrawGrid(const Doc: TMapDocument; const VS: TViewSettings);
var
  GS, SX, SY: Single;
  X, Y: Single;
  StartX, StartY, EndX, EndY: Single;
  WX1, WY1, WX2, WY2: Single;
begin
  GS := VS.GridSize * Doc.Zoom;
  if GS < 4 then Exit;  { too small to draw }

  { World coords of viewport corners }
  Doc.ScreenToWorld(0, 0, WX1, WY1);
  Doc.ScreenToWorld(FViewW, FViewH, WX2, WY2);

  StartX := Floor(WX1 / VS.GridSize) * VS.GridSize;
  StartY := Floor(WY1 / VS.GridSize) * VS.GridSize;
  EndX   := WX2;
  EndY   := WY2;

  glDisable(GL_TEXTURE_2D);
  glColor4f(0.3, 0.3, 0.3, 0.5);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glBegin(GL_LINES);

  X := StartX;
  while X <= EndX do
  begin
    Doc.WorldToScreen(X, WY1, SX, SY);
    glVertex2f(SX, 0);
    glVertex2f(SX, FViewH);
    X := X + VS.GridSize;
  end;

  Y := StartY;
  while Y <= EndY do
  begin
    Doc.WorldToScreen(WX1, Y, SX, SY);
    glVertex2f(0, SY);
    glVertex2f(FViewW, SY);
    Y := Y + VS.GridSize;
  end;

  glEnd;
  glDisable(GL_BLEND);
end;

{ ---- Game objects ------------------------------------------------------- }

procedure TRenderer.DrawObjects(const Doc: TMapDocument);
var
  I: Integer;
  SX, SY: Single;
  { Spawn team colours: 0=general/white, 1=alpha/red, 2=bravo/blue, ... }
  TeamColors: array[0..5] of array[0..2] of Single = (
    (1.0, 1.0, 1.0),  { general }
    (1.0, 0.2, 0.2),  { alpha }
    (0.2, 0.2, 1.0),  { bravo }
    (0.2, 0.8, 0.2),  { charlie }
    (1.0, 0.8, 0.2),  { delta }
    (0.8, 0.2, 0.8)   { spec }
  );
  TC: Integer;
begin
  glDisable(GL_TEXTURE_2D);

  { Spawns }
  for I := 0 to Doc.SpawnCount - 1 do
  begin
    Doc.WorldToScreen(Doc.Spawns[I].X, Doc.Spawns[I].Y, SX, SY);
    TC := Doc.Spawns[I].Team;
    if TC > 5 then TC := 0;
    glColor3f(TeamColors[TC][0], TeamColors[TC][1], TeamColors[TC][2]);
    DrawFilledCircle(SX, SY, SPAWN_RADIUS, 12);
    if Doc.Spawns[I].Selected then
    begin
      glColor3f(1.0, 1.0, 0.0);
      DrawCircle(SX, SY, SPAWN_RADIUS + 2, 12);
    end;
  end;

  { Colliders }
  glColor3f(0.0, 1.0, 1.0);
  for I := 0 to Doc.ColliderCount - 1 do
  begin
    Doc.WorldToScreen(Doc.Colliders[I].X, Doc.Colliders[I].Y, SX, SY);
    DrawCircle(SX, SY, Doc.Colliders[I].Radius * Doc.Zoom, COLLIDER_SEG);
    if Doc.Colliders[I].Selected then
    begin
      glColor3f(1.0, 1.0, 0.0);
      DrawCircle(SX, SY, Doc.Colliders[I].Radius * Doc.Zoom + 2, COLLIDER_SEG);
      glColor3f(0.0, 1.0, 1.0);
    end;
  end;
end;

procedure TRenderer.DrawWaypoints(const Doc: TMapDocument);
var
  I: Integer;
  SX, SY, SX2, SY2: Single;
begin
  glDisable(GL_TEXTURE_2D);

  { Connections }
  glColor3f(0.5, 0.5, 1.0);
  glBegin(GL_LINES);
  for I := 0 to Doc.ConnCount - 1 do
  begin
    if (Doc.Connections[I].Point1 < 1) or
       (Doc.Connections[I].Point1 > Doc.WaypointCount) then Continue;
    if (Doc.Connections[I].Point2 < 1) or
       (Doc.Connections[I].Point2 > Doc.WaypointCount) then Continue;
    Doc.WorldToScreen(
      Doc.Waypoints[Doc.Connections[I].Point1 - 1].X,
      Doc.Waypoints[Doc.Connections[I].Point1 - 1].Y, SX, SY);
    Doc.WorldToScreen(
      Doc.Waypoints[Doc.Connections[I].Point2 - 1].X,
      Doc.Waypoints[Doc.Connections[I].Point2 - 1].Y, SX2, SY2);
    glVertex2f(SX, SY);
    glVertex2f(SX2, SY2);
  end;
  glEnd;

  { Waypoint nodes }
  for I := 0 to Doc.WaypointCount - 1 do
  begin
    Doc.WorldToScreen(Doc.Waypoints[I].X, Doc.Waypoints[I].Y, SX, SY);
    if Doc.Waypoints[I].Selected then
      glColor3f(1.0, 1.0, 0.0)
    else
      glColor3f(0.2, 0.6, 1.0);
    DrawFilledCircle(SX, SY, WP_RADIUS, 8);
    glColor3f(0.0, 0.0, 0.0);
    DrawCircle(SX, SY, WP_RADIUS, 8);
  end;
end;

procedure TRenderer.DrawLights(const Doc: TMapDocument);
var
  I: Integer;
  SX, SY: Single;
begin
  glDisable(GL_TEXTURE_2D);
  for I := 0 to Doc.LightCount - 1 do
  begin
    Doc.WorldToScreen(Doc.Lights[I].X, Doc.Lights[I].Y, SX, SY);
    glColor3f(Doc.Lights[I].Color.R / 255, Doc.Lights[I].Color.G / 255,
              Doc.Lights[I].Color.B / 255);
    DrawFilledCircle(SX, SY, LIGHT_RADIUS, 8);
    if Doc.Lights[I].Selected then
    begin
      glColor3f(1.0, 1.0, 0.0);
      DrawCircle(SX, SY, LIGHT_RADIUS + 2, 8);
    end;
    { Draw range circle }
    glColor4f(Doc.Lights[I].Color.R / 255, Doc.Lights[I].Color.G / 255,
              Doc.Lights[I].Color.B / 255, 0.25);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    DrawCircle(SX, SY, Doc.Lights[I].Range * Doc.Zoom, 32);
    glDisable(GL_BLEND);
  end;
end;

procedure TRenderer.DrawSketch(const Doc: TMapDocument);
var
  I: Integer;
  SX1, SY1, SX2, SY2: Single;
begin
  glDisable(GL_TEXTURE_2D);
  glColor3f(0.8, 0.4, 0.0);
  glBegin(GL_LINES);
  for I := 0 to Doc.SketchCount - 1 do
  begin
    Doc.WorldToScreen(Doc.Sketch[I].V[1].X, Doc.Sketch[I].V[1].Y, SX1, SY1);
    Doc.WorldToScreen(Doc.Sketch[I].V[2].X, Doc.Sketch[I].V[2].Y, SX2, SY2);
    glVertex2f(SX1, SY1);
    glVertex2f(SX2, SY2);
  end;
  glEnd;
end;

{ ---- Scenery ------------------------------------------------------------ }

procedure TRenderer.DrawSceneryLayer(const Doc: TMapDocument; Level: Byte);
var
  I: Integer;
  S: ^TEditorScenery;
  Tex: ^TTexInfo;
  StyleIdx: Integer;
  SX, SY: Single;
  W, H: Single;
  Alpha: Single;
  R, G, B: Single;
  C: LongWord;
begin
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  for I := 0 to Doc.SceneryCount - 1 do
  begin
    S := @Doc.Scenery[I];
    if S^.Level <> Level then Continue;

    StyleIdx := S^.Style;
    if (StyleIdx < 1) or (StyleIdx >= FScenTexCount) then Continue;
    Tex := @FScenTex[StyleIdx];
    if not Tex^.Loaded then Continue;

    Doc.WorldToScreen(S^.X, S^.Y, SX, SY);
    W := S^.Width  * S^.ScaleX * Doc.Zoom;
    H := S^.Height * S^.ScaleY * Doc.Zoom;

    Alpha := S^.Alpha / 255;
    C     := LongWord(S^.Color);
    R     := GetRed(C)   / 255;
    G     := GetGreen(C) / 255;
    B     := GetBlue(C)  / 255;

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, Tex^.TexID);
    glColor4f(R, G, B, Alpha);

    { Rotate around top-left origin (matching VB6 D3DXSprite behavior) }
    glPushMatrix;
    glTranslatef(SX, SY, 0);
    glRotatef(-S^.Rotation * 180 / Pi, 0, 0, 1);
    glBegin(GL_QUADS);
      glTexCoord2f(0, 0); glVertex2f(0, 0);
      glTexCoord2f(1, 0); glVertex2f(W, 0);
      glTexCoord2f(1, 1); glVertex2f(W, H);
      glTexCoord2f(0, 1); glVertex2f(0, H);
    glEnd;
    glPopMatrix;
  end;

  glDisable(GL_TEXTURE_2D);
  glDisable(GL_BLEND);
end;

{ ---- Main render -------------------------------------------------------- }

procedure TRenderer.Render(const Doc: TMapDocument; const VS: TViewSettings;
                           ViewW, ViewH: Integer);
begin
  FViewW := ViewW;
  FViewH := ViewH;

  glViewport(0, 0, ViewW, ViewH);
  SetOrtho(ViewW, ViewH);

  glClearColor(0, 0, 0, 1);
  glClear(GL_COLOR_BUFFER_BIT);

  { 1. Background gradient }
  if VS.ShowBackground then
    DrawBackground(Doc);

  { 2. Type 24/25 polygons (depth-buffer, drawn below scenery) }
  if VS.ShowPolys and (Doc.PolyCount > 0) then
    DrawPolygonsByType(Doc, VS, True);

  { 3. Back-layer scenery (level=0) }
  if VS.ShowScenery then
    DrawSceneryLayer(Doc, 0);

  { 4. Mid-layer scenery (level=1) }
  if VS.ShowScenery then
    DrawSceneryLayer(Doc, 1);

  { 5. Main polygon pass (all except types 24/25) }
  if VS.ShowPolys and (Doc.PolyCount > 0) then
    DrawPolygonsByType(Doc, VS, False);

  { 6. Selection highlight overlay }
  if Doc.PolyCount > 0 then
    DrawSelectionHighlight(Doc);

  { 7. Front-layer scenery (level=2) }
  if VS.ShowScenery then
    DrawSceneryLayer(Doc, 2);

  { 8. Wireframe overlay }
  if VS.ShowWireframe then
    DrawWireframe(Doc);

  { 9. Vertex points }
  if VS.ShowPoints then
    DrawPoints(Doc);

  { 10. Grid }
  if VS.ShowGrid then
    DrawGrid(Doc, VS);

  { 11. Game objects (spawns, colliders) }
  if VS.ShowObjects then
    DrawObjects(Doc);

  { 12. Waypoints and connections }
  if VS.ShowWaypoints then
    DrawWaypoints(Doc);

  { 13. Lights }
  if VS.ShowLights then
    DrawLights(Doc);

  { 14. Sketch lines }
  if VS.ShowSketch then
    DrawSketch(Doc);
end;

end.
