program polyworks_tests;

{$mode objfpc}{$H+}

uses
  Classes, SysUtils, fpcunit, testutils, testregistry, consoletestrunner,
  { Core units }
  pw.types, pw.utils, pw.pms, pw.geometry, pw.map,
  { Test units }
  test_types, test_geometry, test_pms;

var
  Application: TTestRunner;

begin
  Application := TTestRunner.Create(nil);
  Application.Initialize;
  Application.Title := 'PolyWorks Test Suite';
  Application.Run;
  Application.Free;
end.
