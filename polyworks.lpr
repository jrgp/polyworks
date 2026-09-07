program polyworks;

{$mode objfpc}{$H+}

{$IFDEF UNIX}
uses
  Interfaces, Forms, Dialogs, SysUtils, LCLType, frmmain,
  BaseUnix, Unix;
{$ELSE}
uses
  Interfaces, Forms, Dialogs, SysUtils, LCLType, frmmain;
{$ENDIF}

type
  TAppExceptionHandler = class
    procedure HandleException(Sender: TObject; E: Exception);
  end;

procedure TAppExceptionHandler.HandleException(Sender: TObject; E: Exception);
var
  LogFile: TextFile;
  LogPath: string;
  Msg: string;
begin
  Msg := E.ClassName + ': ' + E.Message;
  WriteLn(StdErr, '[PolyWorks exception] ', DateTimeToStr(Now));
  WriteLn(StdErr, Msg);

  LogPath := IncludeTrailingPathDelimiter(GetEnvironmentVariable('HOME')) + '.polyworks-crash.log';
  try
    AssignFile(LogFile, LogPath);
    if FileExists(LogPath) then
      Append(LogFile)
    else
      Rewrite(LogFile);
    WriteLn(LogFile, DateTimeToStr(Now), ' - ', Msg);
    CloseFile(LogFile);
  except
    // Cannot even write to log; ignore
  end;

  // Use ShowMessage rather than Application.MessageBox to avoid re-entrancy on macOS
  try
    ShowMessage('PolyWorks encountered an error:' + LineEnding + Msg +
      LineEnding + '(logged to: ' + LogPath + ')');
  except
    // If even ShowMessage fails, we already wrote to stderr
  end;
end;

{$IFDEF UNIX}
var
  OldSEGVHandler: SigActionRec;

procedure SigSegvHandler(Sig: LongInt; Info: PSigInfo; Ctx: PSigContext); cdecl;
var
  LogFile: TextFile;
  LogPath: string;
begin
  WriteLn(StdErr, '[PolyWorks] Fatal signal: ', Sig);
  // Try to log
  LogPath := IncludeTrailingPathDelimiter(GetEnvironmentVariable('HOME')) + '.polyworks-crash.log';
  try
    AssignFile(LogFile, LogPath);
    if FileExists(LogPath) then
      Append(LogFile)
    else
      Rewrite(LogFile);
    WriteLn(LogFile, DateTimeToStr(Now), ' - Fatal signal ', Sig);
    CloseFile(LogFile);
  except end;
  // Re-raise the default handler
  FpSigAction(Sig, @OldSEGVHandler, nil);
end;
{$ENDIF}

var
  ExHandler: TAppExceptionHandler;
{$IFDEF UNIX}
  SigAction: SigActionRec;
{$ENDIF}

begin
  RequireDerivedFormResource := False;
  Application.Title := 'PolyWorks';
  Application.Scaled := True;
  Application.Initialize;

  ExHandler := TAppExceptionHandler.Create;
  Application.OnException := @ExHandler.HandleException;

{$IFDEF UNIX}
  // Install SIGSEGV/SIGBUS handler for native crash logging
  FillChar(SigAction, SizeOf(SigAction), 0);
  SigAction.sa_handler := @SigSegvHandler;
  FpSigAction(SIGSEGV, @SigAction, @OldSEGVHandler);
  FpSigAction(SIGBUS, @SigAction, nil);
{$ENDIF}

  Application.CreateForm(TMainForm, MainForm);
  Application.Run;
  ExHandler.Free;
end.
