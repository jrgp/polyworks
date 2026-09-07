program polyworks;

{$mode objfpc}{$H+}

uses
  Interfaces, Forms, Dialogs, SysUtils, LCLType, frmmain;

type
  TAppExceptionHandler = class
    procedure HandleException(Sender: TObject; E: Exception);
  end;

procedure TAppExceptionHandler.HandleException(Sender: TObject; E: Exception);
var
  LogFile: TextFile;
  LogPath: string;
begin
  WriteLn(StdErr, '[PolyWorks crash] ', DateTimeToStr(Now));
  WriteLn(StdErr, 'Exception: ', E.ClassName, ': ', E.Message);

  LogPath := IncludeTrailingPathDelimiter(GetEnvironmentVariable('HOME')) + '.polyworks-crash.log';
  try
    AssignFile(LogFile, LogPath);
    if FileExists(LogPath) then
      Append(LogFile)
    else
      Rewrite(LogFile);
    WriteLn(LogFile, DateTimeToStr(Now), ' - ', E.ClassName, ': ', E.Message);
    CloseFile(LogFile);
  except
  end;

  Application.MessageBox(
    PChar('PolyWorks encountered an error: ' + E.Message + LineEnding +
      'Details written to: ' + LogPath),
    'PolyWorks Error', MB_OK or MB_ICONERROR);
end;

var
  ExHandler: TAppExceptionHandler;

begin
  RequireDerivedFormResource := False;
  Application.Title := 'PolyWorks';
  Application.Scaled := True;
  Application.Initialize;
  ExHandler := TAppExceptionHandler.Create;
  Application.OnException := @ExHandler.HandleException;
  Application.CreateForm(TMainForm, MainForm);
  Application.Run;
  ExHandler.Free;
end.
