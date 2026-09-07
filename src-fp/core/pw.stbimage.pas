unit pw.stbimage;

{$mode objfpc}{$H+}
{$LinkLib pw_stb_image}
{$LinkLib c}

{ Pascal bindings for pw_stb_image.c (stb_image wrapper). }

interface

type
  PByte = ^Byte;

{ Load an image from file. Returns RGBA pixel data (4 bytes/pixel), nil on failure.
  Free with StbFree(). }
function StbLoad(const Filename: string; out Width, Height: Integer): PByte;

{ Free pixel data returned by StbLoad. }
procedure StbFree(Data: PByte);

{ Return last failure reason string (valid until next StbLoad call). }
function StbFailureReason: string;

implementation

uses SysUtils;

{ External C functions }
function pw_stb_load(filename: PAnsiChar; width, height: PInteger): PByte;
  cdecl; external;

procedure pw_stb_free(data: PByte);
  cdecl; external;

function pw_stb_failure_reason: PAnsiChar;
  cdecl; external;

function StbLoad(const Filename: string; out Width, Height: Integer): PByte;
begin
  Result := pw_stb_load(PAnsiChar(AnsiString(Filename)), @Width, @Height);
end;

procedure StbFree(Data: PByte);
begin
  if Data <> nil then
    pw_stb_free(Data);
end;

function StbFailureReason: string;
var
  P: PAnsiChar;
begin
  P := pw_stb_failure_reason;
  if P <> nil then
    Result := string(AnsiString(P))
  else
    Result := '';
end;

end.
