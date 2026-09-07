unit pw.theme;

{$mode objfpc}{$H+}

interface

uses
  Classes, SysUtils, Controls, Graphics;

const
  PW_COLOR_BG         = TColor($4A3C31);
  PW_COLOR_MAIN_BG    = TColor($000000);
  PW_COLOR_ACCENT     = TColor($614B3D);
  PW_COLOR_TEXT       = TColor($FFFFFF);
  PW_COLOR_SELECTION  = TColor($4A4ACE);

  PW_TITLEBAR_HEIGHT   = 17;
  PW_TOOL_SIZE         = 32;
  PW_PANEL_FONT_NAME   = 'Arial';
  PW_PANEL_FONT_HEIGHT = -11;
  PW_MAIN_FONT_HEIGHT  = -13;

function PWSkinsDir: string;
function PWAssetPath(const Filename: string): string;
procedure ApplyDarkTheme(AControl: TWinControl);

implementation

uses
  Forms, StdCtrls, ExtCtrls, ComCtrls, Spin, Buttons;

procedure ApplyFont(AFont: TFont; PixelHeight: Integer);
begin
  if AFont = nil then
    Exit;
  AFont.Name := PW_PANEL_FONT_NAME;
  AFont.Color := PW_COLOR_TEXT;
  AFont.Height := PixelHeight;
end;

procedure StyleControl(AControl: TControl);
begin
  if AControl is TCustomForm then
  begin
    TCustomForm(AControl).Color := PW_COLOR_BG;
    ApplyFont(TCustomForm(AControl).Font, PW_MAIN_FONT_HEIGHT);
  end
  else if AControl is TPanel then
  begin
    TPanel(AControl).BevelOuter := bvNone;
    TPanel(AControl).ParentBackground := False;
    TPanel(AControl).Color := PW_COLOR_BG;
    ApplyFont(TPanel(AControl).Font, PW_PANEL_FONT_HEIGHT);
  end
  else if AControl is TLabel then
  begin
    TLabel(AControl).ParentFont := False;
    TLabel(AControl).Font.Name := PW_PANEL_FONT_NAME;
    TLabel(AControl).Font.Height := PW_PANEL_FONT_HEIGHT;
    TLabel(AControl).Font.Color := PW_COLOR_TEXT;
    TLabel(AControl).Transparent := True;
  end
  else if AControl is TCheckBox then
  begin
    TCheckBox(AControl).ParentFont := False;
    ApplyFont(TCheckBox(AControl).Font, PW_PANEL_FONT_HEIGHT);
  end
  else if AControl is TSpeedButton then
  begin
    TSpeedButton(AControl).Flat := True;
    TSpeedButton(AControl).ParentFont := False;
    ApplyFont(TSpeedButton(AControl).Font, PW_PANEL_FONT_HEIGHT);
  end
  else if AControl is TListBox then
  begin
    TListBox(AControl).ParentFont := False;
    TListBox(AControl).Color := PW_COLOR_ACCENT;
    ApplyFont(TListBox(AControl).Font, PW_PANEL_FONT_HEIGHT);
  end
  else if AControl is TStatusBar then
  begin
    TStatusBar(AControl).ParentFont := False;
    TStatusBar(AControl).Color := PW_COLOR_BG;
    ApplyFont(TStatusBar(AControl).Font, PW_PANEL_FONT_HEIGHT);
  end
  else if AControl is TComboBox then
  begin
    TComboBox(AControl).ParentFont := False;
    ApplyFont(TComboBox(AControl).Font, PW_PANEL_FONT_HEIGHT);
  end
  else if AControl is TSpinEdit then
  begin
    TSpinEdit(AControl).ParentFont := False;
    ApplyFont(TSpinEdit(AControl).Font, PW_PANEL_FONT_HEIGHT);
  end
  else if AControl is TButton then
  begin
    TButton(AControl).ParentFont := False;
    ApplyFont(TButton(AControl).Font, PW_PANEL_FONT_HEIGHT);
  end;
end;

function PWSkinsDir: string;
var
  ExeDir: string;
  Candidates: array[0..4] of string;
  I: Integer;
begin
  ExeDir := IncludeTrailingPathDelimiter(ExtractFilePath(ParamStr(0)));
  Candidates[0] := ExeDir + 'skins' + PathDelim + 'default' + PathDelim;
  Candidates[1] := ExeDir + '..' + PathDelim + 'skins' + PathDelim + 'default' + PathDelim;
  Candidates[2] := ExeDir + '..' + PathDelim + 'Resources' + PathDelim + 'skins' + PathDelim + 'default' + PathDelim;
  Candidates[3] := ExeDir + '..' + PathDelim + '..' + PathDelim + 'installer' + PathDelim + 'skins' + PathDelim + 'default' + PathDelim;
  Candidates[4] := ExeDir + '..' + PathDelim + 'installer' + PathDelim + 'skins' + PathDelim + 'default' + PathDelim;

  for I := Low(Candidates) to High(Candidates) do
    if DirectoryExists(Candidates[I]) then
      Exit(ExpandFileName(Candidates[I]));

  Result := '';
end;

function PWAssetPath(const Filename: string): string;
var
  Dir: string;
begin
  Dir := PWSkinsDir;
  if Dir <> '' then
    Result := Dir + Filename
  else
    Result := Filename;
end;

procedure ApplyDarkTheme(AControl: TWinControl);
var
  I: Integer;
begin
  if AControl = nil then
    Exit;

  StyleControl(AControl);
  for I := 0 to AControl.ControlCount - 1 do
  begin
    StyleControl(AControl.Controls[I]);
    if AControl.Controls[I] is TWinControl then
      ApplyDarkTheme(TWinControl(AControl.Controls[I]));
  end;
end;

end.
