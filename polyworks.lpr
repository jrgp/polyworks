program polyworks;

{$mode objfpc}{$H+}

uses
  Interfaces, Forms, frmmain;

begin
  RequireDerivedFormResource := False;
  Application.Title := 'PolyWorks';
  Application.Scaled := True;
  Application.Initialize;
  Application.CreateForm(TMainForm, MainForm);
  Application.Run;
end.
