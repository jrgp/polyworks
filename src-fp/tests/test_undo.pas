unit test_undo;

{$mode objfpc}{$H+}

interface

uses
  fpcunit, testregistry, SysUtils,
  pw.types, pw.map, pw.undo;

type
  TUndoTest = class(TTestCase)
  private
    Doc : TMapDocument;
    Undo: TUndoStack;
    function MakeVertex(X, Y: Single): TEditorVertex;
    function MakeSketchLine(X1, Y1, X2, Y2: Single): TPMSSketchLine;
    procedure AddTriangle(BaseX: Single);
  protected
    procedure SetUp; override;
    procedure TearDown; override;
  published
    procedure TestPushUndo;
    procedure TestMultipleUndo;
    procedure TestRingWrapAround;
    procedure TestRedoClearedByPush;
    procedure TestRedoEmpty;
    procedure TestUndoEmpty;
    procedure TestZoomScrollNotRestored;
    procedure TestClear;
    procedure TestSketchNotSnapshotted;
  end;

implementation

function TUndoTest.MakeVertex(X, Y: Single): TEditorVertex;
begin
  FillChar(Result, SizeOf(Result), 0);
  Result.World.X := X;
  Result.World.Y := Y;
  Result.Color.R := 255;
  Result.Color.G := 255;
  Result.Color.B := 255;
  Result.Alpha := 255;
end;

function TUndoTest.MakeSketchLine(X1, Y1, X2, Y2: Single): TPMSSketchLine;
begin
  FillChar(Result, SizeOf(Result), 0);
  Result.V[1].X := X1;
  Result.V[1].Y := Y1;
  Result.V[1].Z := 1.0;
  Result.V[2].X := X2;
  Result.V[2].Y := Y2;
  Result.V[2].Z := 1.0;
end;

procedure TUndoTest.AddTriangle(BaseX: Single);
var
  V: array[0..2] of TEditorVertex;
begin
  V[0] := MakeVertex(BaseX + 0, 0);
  V[1] := MakeVertex(BaseX + 100, 0);
  V[2] := MakeVertex(BaseX + 50, 100);
  Doc.AddPoly(V, POLY_TYPE_NORMAL);
end;

procedure TUndoTest.SetUp;
begin
  Doc := TMapDocument.Create;
  Undo := TUndoStack.Create(4);
end;

procedure TUndoTest.TearDown;
begin
  Undo.Free;
  Doc.Free;
end;

procedure TUndoTest.TestPushUndo;
begin
  Undo.Push(Doc);
  AddTriangle(0);
  Doc.Modified := False;

  AssertTrue('Undo should succeed after one push', Undo.Undo(Doc));
  AssertEquals('Polygon count restored', 0, Doc.PolyCount);
  AssertTrue('Redo should become available after undo', Undo.CanRedo);
  AssertEquals('Redo depth', 1, Undo.RedoDepth);
  AssertTrue('Undo marks document modified', Doc.Modified);
end;

procedure TUndoTest.TestMultipleUndo;
var
  I: Integer;
begin
  for I := 1 to 3 do
  begin
    Undo.Push(Doc);
    AddTriangle(I * 200);
  end;

  AssertEquals('Three undo snapshots stored', 3, Undo.UndoDepth);
  for I := 1 to 3 do
    AssertTrue('Undo step should succeed', Undo.Undo(Doc));

  AssertEquals('Initial state restored', 0, Doc.PolyCount);
  AssertFalse('No more undo available', Undo.CanUndo);
  AssertEquals('Three redo steps available', 3, Undo.RedoDepth);
end;

procedure TUndoTest.TestRingWrapAround;
var
  I: Integer;
begin
  Undo.Free;
  Undo := TUndoStack.Create(3);

  for I := 1 to 4 do
  begin
    Undo.Push(Doc);
    AddTriangle(I * 200);
  end;

  AssertEquals('Undo depth capped by ring size', 3, Undo.UndoDepth);
  for I := 1 to 3 do
    AssertTrue('Only three undos should succeed', Undo.Undo(Doc));

  AssertEquals('Oldest retained snapshot restored', 1, Doc.PolyCount);
  AssertFalse('Fourth undo should fail', Undo.Undo(Doc));
end;

procedure TUndoTest.TestRedoClearedByPush;
begin
  Undo.Push(Doc);
  AddTriangle(0);
  Undo.Push(Doc);
  AddTriangle(200);

  AssertTrue('Undo should succeed', Undo.Undo(Doc));
  AssertTrue('Redo available after undo', Undo.CanRedo);

  Undo.Push(Doc);
  AssertFalse('New push clears redo history', Undo.CanRedo);
  AssertEquals('Redo depth cleared', 0, Undo.RedoDepth);
end;

procedure TUndoTest.TestRedoEmpty;
begin
  AssertFalse('Redo with empty stack should fail', Undo.Redo(Doc));
end;

procedure TUndoTest.TestUndoEmpty;
begin
  AssertFalse('Undo with empty stack should fail', Undo.Undo(Doc));
end;

procedure TUndoTest.TestZoomScrollNotRestored;
begin
  Undo.Push(Doc);
  AddTriangle(0);
  Doc.SetZoom(2.0, 0, 0);
  Doc.Scroll(10.0, 20.0);

  AssertTrue('Undo should succeed', Undo.Undo(Doc));
  AssertEquals('Polygon removed by undo', 0, Doc.PolyCount);
  AssertEquals('Zoom is not restored', 2.0, Doc.Zoom);
  AssertEquals('ScrollX is not restored', 10.0, Doc.ScrollX);
  AssertEquals('ScrollY is not restored', 20.0, Doc.ScrollY);
end;

procedure TUndoTest.TestClear;
begin
  Undo.Push(Doc);
  AddTriangle(0);
  AssertTrue('Undo should succeed', Undo.Undo(Doc));
  AssertTrue('Redo should be available before clear', Undo.CanRedo);

  Undo.Clear;

  AssertFalse('Clear removes undo history', Undo.CanUndo);
  AssertFalse('Clear removes redo history', Undo.CanRedo);
  AssertEquals('Undo depth cleared', 0, Undo.UndoDepth);
  AssertEquals('Redo depth cleared', 0, Undo.RedoDepth);
end;

procedure TUndoTest.TestSketchNotSnapshotted;
begin
  Undo.Push(Doc);
  Doc.AddSketchLine(MakeSketchLine(0, 0, 10, 10));
  AddTriangle(0);

  Undo.Push(Doc);
  Doc.AddSketchLine(MakeSketchLine(10, 10, 20, 20));
  AddTriangle(200);

  AssertEquals('Sketch count before undo', 2, Doc.SketchCount);
  AssertTrue('Undo should succeed', Undo.Undo(Doc));
  AssertEquals('Polygon state restored', 1, Doc.PolyCount);
  AssertEquals('Sketch lines are not restored by undo', 2, Doc.SketchCount);
end;

initialization
  RegisterTest(TUndoTest);

end.
