program polyworks;

{$mode objfpc}{$H+}

uses
  Interfaces, Forms, frmmain, frminfo, frmdisplay, frmscenery, frmwaypoints,
  frmtools, frmcolor, frmtexture, frmmap, frmpreferences;

begin
  RequireDerivedFormResource := False;
  Application.Title := 'PolyWorks';
  Application.Scaled := True;
  Application.Initialize;
  Application.CreateForm(TMainForm, MainForm);
  Application.Run;
end.
