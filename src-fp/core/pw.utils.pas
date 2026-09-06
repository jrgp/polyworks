unit pw.utils;

{$mode objfpc}{$H+}

interface

uses
  pw.types, Math, SysUtils;

{ ---- Colour helpers ---------------------------------------------------- }

{ Pack ARGB components into a LongWord (0xAARRGGBB). }
function  MakeARGB(A, R, G, B: Byte): LongWord;

{ Extract components from an ARGB LongWord. }
function  GetAlpha(Color: LongWord): Byte;
function  GetRed  (Color: LongWord): Byte;
function  GetGreen(Color: LongWord): Byte;
function  GetBlue (Color: LongWord): Byte;

{ Build ARGB keeping existing alpha, replacing RGB. }
function  ARGBWithAlpha(Alpha: Byte; RGB: LongWord): LongWord;

{ Convert a TColor3 and alpha to ARGB LongWord. }
function  Color3ToARGB(const C: TColor3; Alpha: Byte): LongWord;

{ Extract TColor3 from ARGB LongWord. }
function  ARGBToColor3(Color: LongWord): TColor3;

{ Parse a 6-digit hex colour string "RRGGBB" → LongWord (0x00RRGGBB). }
function  ParseHexRGB(const S: string): LongWord;

{ ---- Angle helpers ----------------------------------------------------- }

{ Angle (radians) from (x1,y1) to (x2,y2) in VB6 Atan2 convention. }
function  AngleBetween(X1, Y1, X2, Y2: Single): Single;

{ ---- Numeric helpers --------------------------------------------------- }

{ VB6 Midpoint(a,b) = (a+b)/2 as used in IsCW. }
function  Midpoint(A, B: Single): Single; inline;

{ Clamp value into [Lo, Hi]. }
function  Clamp(V, Lo, Hi: Single): Single; inline;

{ True iff Lo <= V <= Hi. }
function  IsBetween(V, Lo, Hi: Single): Boolean; inline;

{ VB6 Int() — floor toward negative infinity (NOT Trunc). }
function  VBInt(V: Single): Integer; inline;

{ ---- String helpers for PMS byte-array format -------------------------- }

{ Read a VB6-style length-prefixed byte array into a Pascal string.
  Buf[0] = length byte; Buf[1..Len] = char bytes. }
function  ReadPascalStr(const Buf: array of Byte): string;

{ Write a Pascal string into a VB6-style 39-byte mapName array.
  Truncates to MaxLen chars. Remaining bytes are left as zeros. }
procedure WritePascalStr(const S: string; var Buf: array of Byte;
                         MaxLen: Integer);

implementation

function MakeARGB(A, R, G, B: Byte): LongWord;
begin
  Result := (LongWord(A) shl 24) or (LongWord(R) shl 16)
           or (LongWord(G) shl 8) or LongWord(B);
end;

function GetAlpha(Color: LongWord): Byte;
begin
  Result := Byte(Color shr 24);
end;

function GetRed(Color: LongWord): Byte;
begin
  Result := Byte(Color shr 16);
end;

function GetGreen(Color: LongWord): Byte;
begin
  Result := Byte(Color shr 8);
end;

function GetBlue(Color: LongWord): Byte;
begin
  Result := Byte(Color);
end;

function ARGBWithAlpha(Alpha: Byte; RGB: LongWord): LongWord;
begin
  Result := (LongWord(Alpha) shl 24) or (RGB and $00FFFFFF);
end;

function Color3ToARGB(const C: TColor3; Alpha: Byte): LongWord;
begin
  Result := MakeARGB(Alpha, C.R, C.G, C.B);
end;

function ARGBToColor3(Color: LongWord): TColor3;
begin
  Result.R := GetRed(Color);
  Result.G := GetGreen(Color);
  Result.B := GetBlue(Color);
end;

function ParseHexRGB(const S: string): LongWord;
var
  V: Int64;
begin
  V := 0;
  if not TryStrToInt64('$' + S, V) then
    V := 0;
  Result := LongWord(V) and $00FFFFFF;
end;

function AngleBetween(X1, Y1, X2, Y2: Single): Single;
begin
  Result := ArcTan2(Y2 - Y1, X2 - X1);
end;

function Midpoint(A, B: Single): Single;
begin
  Result := (A + B) * 0.5;
end;

function Clamp(V, Lo, Hi: Single): Single;
begin
  if V < Lo then Result := Lo
  else if V > Hi then Result := Hi
  else Result := V;
end;

function IsBetween(V, Lo, Hi: Single): Boolean;
begin
  Result := (V >= Lo) and (V <= Hi);
end;

function VBInt(V: Single): Integer;
begin
  Result := Floor(V);
end;

function ReadPascalStr(const Buf: array of Byte): string;
var
  Len, I: Integer;
begin
  if Length(Buf) = 0 then
  begin
    Result := '';
    Exit;
  end;
  Len := Buf[0];
  if Len > High(Buf) then
    Len := High(Buf);
  SetLength(Result, Len);
  for I := 1 to Len do
    Result[I] := Chr(Buf[I]);
end;

procedure WritePascalStr(const S: string; var Buf: array of Byte;
                         MaxLen: Integer);
var
  Len, I: Integer;
begin
  Len := Length(S);
  if Len > MaxLen then Len := MaxLen;
  if Len > High(Buf) then Len := High(Buf);
  FillChar(Buf[0], Length(Buf), 0);
  Buf[0] := Byte(Len);
  for I := 1 to Len do
    Buf[I] := Byte(Ord(S[I]));
end;

end.
