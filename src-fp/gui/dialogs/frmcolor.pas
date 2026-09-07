unit frmcolor;

{$mode objfpc}{$H+}

interface

uses
  Graphics, Dialogs, pw.types, pw.utils;

function PickColor(var C: LongWord): Boolean;

implementation

function PickColor(var C: LongWord): Boolean;
var
  Dialog: TColorDialog;
  RGBColor: TColor;
  Alpha: Byte;
begin
  Dialog := TColorDialog.Create(nil);
  try
    Dialog.Color := RGBToColor(GetRed(C), GetGreen(C), GetBlue(C));
    Result := Dialog.Execute;
    if Result then
    begin
      RGBColor := ColorToRGB(Dialog.Color);
      Alpha := GetAlpha(C);
      C := MakeARGB(Alpha, Byte(LongWord(RGBColor) and $FF),
        Byte((LongWord(RGBColor) shr 8) and $FF),
        Byte((LongWord(RGBColor) shr 16) and $FF));
    end;
  finally
    Dialog.Free;
  end;
end;

end.
