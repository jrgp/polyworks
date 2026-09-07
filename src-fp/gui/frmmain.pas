unit frmmain;

{$mode objfpc}{$H+}

interface

uses
  Classes, SysUtils, Forms, Controls, Dialogs, Menus, ComCtrls, LCLType,
  viewport, tools, renderer,
  pw.types, pw.utils, pw.map, pw.undo, pw.pms, pw.config,
  frmmap, frmpreferences, frmtools, frminfo, frmdisplay, frmscenery,
  frmwaypoints;

type
  TMainForm = class(TForm)
  private
    FDoc: TMapDocument;
    FUndo: TUndoStack;
    FCfg: TAppConfig;
    FIniPath: string;
    FCurrentFilename: string;

    FViewport: TMapViewport;
    FStatusBar: TStatusBar;
    FMainMenu: TMainMenu;
    FOpenDialog: TOpenDialog;
    FSaveDialog: TSaveDialog;

    FToolsForm: TWToolsForm;
    FInfoForm: TInfoForm;
    FDisplayForm: TDisplayForm;
    FSceneryForm: TSceneryForm;
    FWaypointForm: TWaypointForm;
    FPanelsPositioned: Boolean;
    FLastWaypointCount: Integer;
    FLastSceneryNameCount: Integer;

    FToolMenuItems: array[0..TOOL_SKETCH] of TMenuItem;

    FShowPolysItem: TMenuItem;
    FShowWireframeItem: TMenuItem;
    FShowPointsItem: TMenuItem;
    FShowGridItem: TMenuItem;
    FShowObjectsItem: TMenuItem;
    FShowWaypointsItem: TMenuItem;
    FShowLightsItem: TMenuItem;
    FShowSketchItem: TMenuItem;
    FShowTextureItem: TMenuItem;
    FShowBackgroundItem: TMenuItem;
    FShowSceneryItem: TMenuItem;

    function AddMenuItem(AParent: TMenuItem; const ACaption: string;
      AOnClick: TNotifyEvent; AShortCut: TShortCut = 0; ATag: PtrInt = 0;
      ACheckable: Boolean = False): TMenuItem;
    function HasSelection: Boolean;
    function SaveDocumentTo(const AFilename: string; CompileMode: Boolean): Boolean;
    function TryGetSelectedSceneryToolInfo(out StyleIndex, ItemWidth, ItemHeight: Integer): Boolean;
    procedure ApplyConfigToViewSettings;
    procedure ApplyViewSettingsToConfig;
    procedure BuildMenus;
    procedure DisplaySettingsChanged(Sender: TObject);
    procedure FloatingToolSelect(Sender: TObject; ToolID: Integer);
    procedure ViewportToolSelect(ToolID: Integer);
    procedure InvalidatePanelCaches;
    procedure LoadDocumentTextures;
    procedure PositionFloatingForms;
    procedure RefreshFloatingPanels;
    procedure SelectAll;
    procedure SetCurrentFile(const AFilename: string);
    procedure SyncToolUI;
    procedure UpdateCaption;
    procedure UpdateStatus(Sender: TObject);
    procedure ViewportChanged(Sender: TObject);

    procedure HandleFirstShow(Sender: TObject);
    procedure HandleClose(Sender: TObject; var CloseAction: TCloseAction);
    procedure NewFile(Sender: TObject);
    procedure OpenFile(Sender: TObject);
    procedure SaveFile(Sender: TObject);
    procedure SaveFileAs(Sender: TObject);
    procedure SaveAndCompileFile(Sender: TObject);
    procedure QuitApp(Sender: TObject);

    procedure UndoAction(Sender: TObject);
    procedure RedoAction(Sender: TObject);
    procedure SelectAllAction(Sender: TObject);
    procedure DeselectAllAction(Sender: TObject);
    procedure DeleteAction(Sender: TObject);

    procedure ToggleViewOption(Sender: TObject);
    procedure ZoomInAction(Sender: TObject);
    procedure ZoomOutAction(Sender: TObject);
    procedure ResetZoomAction(Sender: TObject);

    procedure MapPropertiesAction(Sender: TObject);
    procedure PreferencesAction(Sender: TObject);

    procedure SelectTool(Sender: TObject);
  public
    constructor Create(AOwner: TComponent); override;
    destructor Destroy; override;
  end;

var
  MainForm: TMainForm;

implementation

uses
  Graphics, pw.theme;

const
  VIEW_SHOW_POLYS      = 1;
  VIEW_SHOW_WIREFRAME  = 2;
  VIEW_SHOW_POINTS     = 3;
  VIEW_SHOW_GRID       = 4;
  VIEW_SHOW_OBJECTS    = 5;
  VIEW_SHOW_WAYPOINTS  = 6;
  VIEW_SHOW_LIGHTS     = 7;
  VIEW_SHOW_SKETCH     = 8;
  VIEW_SHOW_TEXTURE    = 9;
  VIEW_SHOW_BACKGROUND = 10;
  VIEW_SHOW_SCENERY    = 11;

function ToolCaption(ToolID: Integer): string;
begin
  case ToolID of
    TOOL_SELECT:   Result := 'Select';
    TOOL_POLY:     Result := 'Add Polygon';
    TOOL_SCENERY:  Result := 'Add Scenery';
    TOOL_SPAWN:    Result := 'Add Spawn';
    TOOL_WAYPOINT: Result := 'Add Waypoint';
    TOOL_COLLIDER: Result := 'Add Collider';
    TOOL_LIGHT:    Result := 'Add Light';
    TOOL_SKETCH:   Result := 'Draw Sketch';
  else
    Result := 'Tool';
  end;
end;

constructor TMainForm.Create(AOwner: TComponent);
begin
  inherited CreateNew(AOwner, 1);
  Caption := 'PolyWorks';
  Width := 1280;
  Height := 800;
  Position := poScreenCenter;
  KeyPreview := True;
  Color := PW_COLOR_MAIN_BG;
  Font.Name := PW_PANEL_FONT_NAME;
  Font.Height := PW_MAIN_FONT_HEIGHT;
  OnShow := @HandleFirstShow;
  OnClose := @HandleClose;

  FIniPath := ChangeFileExt(ParamStr(0), '.ini');
  LoadConfig(FIniPath, FCfg);

  FDoc := TMapDocument.Create;
  FDoc.RebuildScreenCache;
  FUndo := TUndoStack.Create(FCfg.UndoDepth);

  FOpenDialog := TOpenDialog.Create(Self);
  FOpenDialog.Filter := 'Soldat map (*.pms)|*.pms|All files|*.*';
  FOpenDialog.DefaultExt := 'pms';

  FSaveDialog := TSaveDialog.Create(Self);
  FSaveDialog.Filter := 'Soldat map (*.pms)|*.pms|All files|*.*';
  FSaveDialog.DefaultExt := 'pms';

  BuildMenus;

  FStatusBar := TStatusBar.Create(Self);
  FStatusBar.Parent := Self;
  FStatusBar.Align := alBottom;
  FStatusBar.SimplePanel := True;
  FStatusBar.Color := PW_COLOR_BG;
  FStatusBar.ParentFont := False;
  FStatusBar.Font.Name := PW_PANEL_FONT_NAME;
  FStatusBar.Font.Height := PW_PANEL_FONT_HEIGHT;
  FStatusBar.Font.Color := PW_COLOR_TEXT;

  FViewport := TMapViewport.Create(Self);
  FViewport.Parent := Self;
  FViewport.Align := alClient;
  FViewport.Color := PW_COLOR_MAIN_BG;
  FViewport.SetDocument(FDoc);
  FViewport.UndoStack := FUndo;
  FViewport.OnViewChanged := @ViewportChanged;
  FViewport.OnToolSelect := @ViewportToolSelect;

  FToolsForm := TWToolsForm.Create(Self);
  FToolsForm.OnToolSelect := @FloatingToolSelect;

  FInfoForm := TInfoForm.Create(Self);
  FDisplayForm := TDisplayForm.Create(Self);
  FDisplayForm.OnChange := @DisplaySettingsChanged;
  FSceneryForm := TSceneryForm.Create(Self);
  FWaypointForm := TWaypointForm.Create(Self);

  SetSceneryToolProvider(@TryGetSelectedSceneryToolInfo);
  InvalidatePanelCaches;
  ApplyConfigToViewSettings;
  FViewport.SetActiveTool(TOOL_SELECT);
  SyncToolUI;
  RefreshFloatingPanels;
  UpdateCaption;
  UpdateStatus(nil);
end;

destructor TMainForm.Destroy;
begin
  ClearSceneryToolProvider;
  ApplyViewSettingsToConfig;
  SaveConfig(FIniPath, FCfg);
  FUndo.Free;
  FDoc.Free;
  inherited Destroy;
end;

function TMainForm.AddMenuItem(AParent: TMenuItem; const ACaption: string;
  AOnClick: TNotifyEvent; AShortCut: TShortCut; ATag: PtrInt;
  ACheckable: Boolean): TMenuItem;
begin
  Result := TMenuItem.Create(Self);
  Result.Caption := ACaption;
  Result.Tag := ATag;
  Result.ShortCut := AShortCut;
  Result.AutoCheck := ACheckable;
  if ACheckable then
    Result.Checked := True;
  Result.OnClick := AOnClick;
  AParent.Add(Result);
end;

function TMainForm.HasSelection: Boolean;
var
  I, J: Integer;
begin
  Result := False;
  for I := 0 to FDoc.PolyCount - 1 do
    for J := 1 to 3 do
      if FDoc.Polys[I].Selected[J] then
        Exit(True);
  for I := 0 to FDoc.SceneryCount - 1 do
    if FDoc.Scenery[I].Selected then Exit(True);
  for I := 0 to FDoc.SpawnCount - 1 do
    if FDoc.Spawns[I].Selected then Exit(True);
  for I := 0 to FDoc.ColliderCount - 1 do
    if FDoc.Colliders[I].Selected then Exit(True);
  for I := 0 to FDoc.WaypointCount - 1 do
    if FDoc.Waypoints[I].Selected then Exit(True);
  for I := 0 to FDoc.LightCount - 1 do
    if FDoc.Lights[I].Selected then Exit(True);
end;

function TMainForm.SaveDocumentTo(const AFilename: string; CompileMode: Boolean): Boolean;
var
  Data: TPMSData;
  Err: string;
begin
  Result := False;
  FDoc.SaveToPMS(Data);
  if CompileMode then
    Result := CompilePMS(AFilename, Data, Err)
  else
    Result := SavePMS(AFilename, Data, Err);

  if not Result then
  begin
    MessageDlg('Save failed', Err, mtError, [mbOK], 0);
    Exit;
  end;

  if not CompileMode then
  begin
    FDoc.Modified := False;
    SetCurrentFile(AFilename);
  end;

  UpdateCaption;
  UpdateStatus(nil);
end;

function TMainForm.TryGetSelectedSceneryToolInfo(out StyleIndex, ItemWidth,
  ItemHeight: Integer): Boolean;
begin
  Result := False;
  StyleIndex := 0;
  ItemWidth := 64;
  ItemHeight := 64;
  if FSceneryForm = nil then
    Exit;

  StyleIndex := FSceneryForm.GetSelectedStyle;
  if StyleIndex <= 0 then
    Exit;

  if not FViewport.Renderer.GetSceneryTextureSize(StyleIndex, ItemWidth, ItemHeight) then
  begin
    ItemWidth := 64;
    ItemHeight := 64;
  end;
  Result := True;
end;

procedure TMainForm.ApplyConfigToViewSettings;
begin
  FViewport.ViewSettings := DefaultViewSettings;
  FViewport.ViewSettings.ShowPolys := FCfg.ShowPolys;
  FViewport.ViewSettings.ShowWireframe := FCfg.ShowWireframe;
  FViewport.ViewSettings.ShowPoints := FCfg.ShowPoints;
  FViewport.ViewSettings.ShowGrid := FCfg.ShowGrid;
  FViewport.ViewSettings.ShowObjects := FCfg.ShowObjects;
  FViewport.ViewSettings.ShowWaypoints := FCfg.ShowWaypoints;
  FViewport.ViewSettings.ShowLights := FCfg.ShowLights;
  FViewport.ViewSettings.ShowSketch := FCfg.ShowSketch;
  FViewport.ViewSettings.GridSize := FCfg.GridSize;

  FShowPolysItem.Checked := FViewport.ViewSettings.ShowPolys;
  FShowWireframeItem.Checked := FViewport.ViewSettings.ShowWireframe;
  FShowPointsItem.Checked := FViewport.ViewSettings.ShowPoints;
  FShowGridItem.Checked := FViewport.ViewSettings.ShowGrid;
  FShowObjectsItem.Checked := FViewport.ViewSettings.ShowObjects;
  FShowWaypointsItem.Checked := FViewport.ViewSettings.ShowWaypoints;
  FShowLightsItem.Checked := FViewport.ViewSettings.ShowLights;
  FShowSketchItem.Checked := FViewport.ViewSettings.ShowSketch;
  FShowTextureItem.Checked := FViewport.ViewSettings.ShowTexture;
  FShowBackgroundItem.Checked := FViewport.ViewSettings.ShowBackground;
  FShowSceneryItem.Checked := FViewport.ViewSettings.ShowScenery;
  if FDisplayForm <> nil then
    FDisplayForm.SetViewSettings(FViewport.ViewSettings);
end;

procedure TMainForm.ApplyViewSettingsToConfig;
begin
  FCfg.ShowPolys := FViewport.ViewSettings.ShowPolys;
  FCfg.ShowWireframe := FViewport.ViewSettings.ShowWireframe;
  FCfg.ShowPoints := FViewport.ViewSettings.ShowPoints;
  FCfg.ShowGrid := FViewport.ViewSettings.ShowGrid;
  FCfg.ShowObjects := FViewport.ViewSettings.ShowObjects;
  FCfg.ShowWaypoints := FViewport.ViewSettings.ShowWaypoints;
  FCfg.ShowLights := FViewport.ViewSettings.ShowLights;
  FCfg.ShowSketch := FViewport.ViewSettings.ShowSketch;
  FCfg.GridSize := FViewport.ViewSettings.GridSize;
end;

procedure TMainForm.BuildMenus;
var
  FileMenu, EditMenu, ViewMenu, ToolsMenu: TMenuItem;
  ToolID: Integer;
begin
  FMainMenu := TMainMenu.Create(Self);
  Menu := FMainMenu;

  FileMenu := TMenuItem.Create(Self);
  FileMenu.Caption := '&File';
  FMainMenu.Items.Add(FileMenu);
  AddMenuItem(FileMenu, '&New', @NewFile, ShortCut(VK_N, [ssCtrl]));
  AddMenuItem(FileMenu, '&Open...', @OpenFile, ShortCut(VK_O, [ssCtrl]));
  AddMenuItem(FileMenu, '&Save', @SaveFile, ShortCut(VK_S, [ssCtrl]));
  AddMenuItem(FileMenu, 'Save &As...', @SaveFileAs, ShortCut(VK_S, [ssCtrl, ssShift]));
  AddMenuItem(FileMenu, 'Save and &Compile...', @SaveAndCompileFile, ShortCut(VK_F9, []));
  FileMenu.AddSeparator;
  AddMenuItem(FileMenu, '&Quit', @QuitApp, ShortCut(VK_F4, [ssAlt]));

  EditMenu := TMenuItem.Create(Self);
  EditMenu.Caption := '&Edit';
  FMainMenu.Items.Add(EditMenu);
  AddMenuItem(EditMenu, '&Undo', @UndoAction, ShortCut(VK_Z, [ssCtrl]));
  AddMenuItem(EditMenu, '&Redo', @RedoAction, ShortCut(VK_Y, [ssCtrl]));
  EditMenu.AddSeparator;
  AddMenuItem(EditMenu, 'Select &All', @SelectAllAction, ShortCut(VK_A, [ssCtrl]));
  AddMenuItem(EditMenu, '&Deselect All', @DeselectAllAction, ShortCut(VK_D, [ssCtrl]));
  EditMenu.AddSeparator;
  AddMenuItem(EditMenu, '&Delete', @DeleteAction, VK_DELETE);
  EditMenu.AddSeparator;
  AddMenuItem(EditMenu, '&Map Properties...', @MapPropertiesAction, ShortCut(VK_M, [ssCtrl]));
  AddMenuItem(EditMenu, 'P&references...', @PreferencesAction, ShortCut(VK_P, [ssCtrl]));

  ViewMenu := TMenuItem.Create(Self);
  ViewMenu.Caption := '&View';
  FMainMenu.Items.Add(ViewMenu);
  FShowPolysItem := AddMenuItem(ViewMenu, 'Show &Polygons', @ToggleViewOption, 0, VIEW_SHOW_POLYS, True);
  FShowWireframeItem := AddMenuItem(ViewMenu, 'Show &Wireframe', @ToggleViewOption, 0, VIEW_SHOW_WIREFRAME, True);
  FShowPointsItem := AddMenuItem(ViewMenu, 'Show Po&ints', @ToggleViewOption, 0, VIEW_SHOW_POINTS, True);
  FShowGridItem := AddMenuItem(ViewMenu, 'Show &Grid', @ToggleViewOption, 0, VIEW_SHOW_GRID, True);
  FShowObjectsItem := AddMenuItem(ViewMenu, 'Show &Objects', @ToggleViewOption, 0, VIEW_SHOW_OBJECTS, True);
  FShowWaypointsItem := AddMenuItem(ViewMenu, 'Show &Waypoints', @ToggleViewOption, 0, VIEW_SHOW_WAYPOINTS, True);
  FShowLightsItem := AddMenuItem(ViewMenu, 'Show &Lights', @ToggleViewOption, 0, VIEW_SHOW_LIGHTS, True);
  FShowSketchItem := AddMenuItem(ViewMenu, 'Show S&ketch', @ToggleViewOption, 0, VIEW_SHOW_SKETCH, True);
  FShowTextureItem := AddMenuItem(ViewMenu, 'Show Te&xture', @ToggleViewOption, 0, VIEW_SHOW_TEXTURE, True);
  FShowBackgroundItem := AddMenuItem(ViewMenu, 'Show &Background', @ToggleViewOption, 0, VIEW_SHOW_BACKGROUND, True);
  FShowSceneryItem := AddMenuItem(ViewMenu, 'Show Sc&enery', @ToggleViewOption, 0, VIEW_SHOW_SCENERY, True);
  ViewMenu.AddSeparator;
  AddMenuItem(ViewMenu, 'Zoom &In', @ZoomInAction, ShortCut(VK_ADD, []));
  AddMenuItem(ViewMenu, 'Zoom &Out', @ZoomOutAction, ShortCut(VK_SUBTRACT, []));
  AddMenuItem(ViewMenu, '&Reset Zoom', @ResetZoomAction, ShortCut(VK_MULTIPLY, []));

  ToolsMenu := TMenuItem.Create(Self);
  ToolsMenu.Caption := '&Tools';
  FMainMenu.Items.Add(ToolsMenu);
  for ToolID in [TOOL_SELECT, TOOL_POLY, TOOL_SCENERY, TOOL_SPAWN,
                 TOOL_WAYPOINT, TOOL_COLLIDER, TOOL_LIGHT, TOOL_SKETCH] do
  begin
    FToolMenuItems[ToolID] := AddMenuItem(ToolsMenu, ToolCaption(ToolID), @SelectTool, 0, ToolID, True);
    FToolMenuItems[ToolID].AutoCheck := False;
  end;
end;

procedure TMainForm.DisplaySettingsChanged(Sender: TObject);
var
  VS: TViewSettings;
begin
  FDisplayForm.GetViewSettings(VS);
  FViewport.ViewSettings := VS;
  FShowPolysItem.Checked := VS.ShowPolys;
  FShowWireframeItem.Checked := VS.ShowWireframe;
  FShowPointsItem.Checked := VS.ShowPoints;
  FShowGridItem.Checked := VS.ShowGrid;
  FShowObjectsItem.Checked := VS.ShowObjects;
  FShowWaypointsItem.Checked := VS.ShowWaypoints;
  FShowLightsItem.Checked := VS.ShowLights;
  FShowSketchItem.Checked := VS.ShowSketch;
  FShowTextureItem.Checked := VS.ShowTexture;
  FShowBackgroundItem.Checked := VS.ShowBackground;
  FShowSceneryItem.Checked := VS.ShowScenery;
  ApplyViewSettingsToConfig;
  FViewport.RequestRepaint;
  UpdateStatus(nil);
end;

procedure TMainForm.FloatingToolSelect(Sender: TObject; ToolID: Integer);
begin
  FViewport.SetActiveTool(ToolID);
  SyncToolUI;
  FViewport.SetFocus;
end;

procedure TMainForm.ViewportToolSelect(ToolID: Integer);
begin
  // Keyboard hotkey changed tool in viewport; sync the tools panel and menu
  SyncToolUI;
end;

procedure TMainForm.InvalidatePanelCaches;
begin
  FLastWaypointCount := -1;
  FLastSceneryNameCount := -1;
end;

procedure TMainForm.LoadDocumentTextures;
var
  TexName: string;
  I: Integer;
  SoldatPath: string;
  SceneryPath: string;
begin
  if not FViewport.MakeCurrent then
    Exit;

  FViewport.Renderer.FreeTextures;
  SoldatPath := IncludeTrailingPathDelimiter(FCfg.SoldatPath);
  TexName := ReadPascalStr(FDoc.Options.TextureName);
  if (SoldatPath <> '') and (TexName <> '') then
    FViewport.Renderer.LoadMapTexture(SoldatPath + 'textures/' + TexName);

  if SoldatPath <> '' then
    for I := 1 to FDoc.ScenNameCount do
      if FDoc.SceneryNames[I] <> '' then
      begin
        SceneryPath := SoldatPath + 'scenery-gfx/' + FDoc.SceneryNames[I];
        if not FileExists(SceneryPath) then
          SceneryPath := SoldatPath + 'scenery/gfx/' + FDoc.SceneryNames[I];
        FViewport.Renderer.LoadSceneryTexture(I, SceneryPath);
      end;
end;

procedure TMainForm.PositionFloatingForms;
var
  RightX: Integer;
  RightX2: Integer;
  TopY: Integer;
begin
  RightX := Left + Width - FInfoForm.Width - 24;
  RightX2 := RightX - FSceneryForm.Width - 12;
  TopY := Top + 56;

  FToolsForm.SetBounds(Left + 8, TopY, FToolsForm.Width, FToolsForm.Height);
  FInfoForm.SetBounds(RightX, TopY, FInfoForm.Width, FInfoForm.Height);
  FDisplayForm.SetBounds(RightX, TopY + FInfoForm.Height + 12,
    FDisplayForm.Width, FDisplayForm.Height);
  FSceneryForm.SetBounds(RightX2, TopY, FSceneryForm.Width, FSceneryForm.Height);
  FWaypointForm.SetBounds(RightX2, TopY + FSceneryForm.Height + 12,
    FWaypointForm.Width, FWaypointForm.Height);
end;

procedure TMainForm.RefreshFloatingPanels;
begin
  if FInfoForm <> nil then
    FInfoForm.Refresh(FDoc);

  if (FDisplayForm <> nil) and not FDisplayForm.Visible then
    FDisplayForm.SetViewSettings(FViewport.ViewSettings);

  if (FWaypointForm <> nil) and (FLastWaypointCount <> FDoc.WaypointCount) then
  begin
    FWaypointForm.Refresh(FDoc);
    FLastWaypointCount := FDoc.WaypointCount;
  end;

  if (FSceneryForm <> nil) and (FLastSceneryNameCount <> FDoc.ScenNameCount) then
  begin
    FSceneryForm.RefreshList(FDoc);
    FLastSceneryNameCount := FDoc.ScenNameCount;
  end;
end;

procedure TMainForm.SelectAll;
var
  I, J: Integer;
begin
  for I := 0 to FDoc.PolyCount - 1 do
    for J := 1 to 3 do
      FDoc.Polys[I].Selected[J] := True;
  for I := 0 to FDoc.SceneryCount - 1 do
    FDoc.Scenery[I].Selected := True;
  for I := 0 to FDoc.SpawnCount - 1 do
    FDoc.Spawns[I].Selected := True;
  for I := 0 to FDoc.ColliderCount - 1 do
    FDoc.Colliders[I].Selected := True;
  for I := 0 to FDoc.WaypointCount - 1 do
    FDoc.Waypoints[I].Selected := True;
  for I := 0 to FDoc.LightCount - 1 do
    FDoc.Lights[I].Selected := True;
end;

procedure TMainForm.SetCurrentFile(const AFilename: string);
begin
  FCurrentFilename := AFilename;
  if AFilename <> '' then
    AddRecentFile(FCfg, AFilename);
end;

procedure TMainForm.SyncToolUI;
var
  I: Integer;
  ActiveTool: Integer;
begin
  ActiveTool := FViewport.GetActiveTool;
  for I := Low(FToolMenuItems) to High(FToolMenuItems) do
    if FToolMenuItems[I] <> nil then
      FToolMenuItems[I].Checked := I = ActiveTool;
  if FToolsForm <> nil then
    FToolsForm.SetActiveTool(ActiveTool);
end;

procedure TMainForm.UpdateCaption;
begin
  if FCurrentFilename <> '' then
    Caption := 'PolyWorks - ' + ExtractFileName(FCurrentFilename)
  else
    Caption := 'PolyWorks';
  if FDoc.Modified then
    Caption := Caption + ' *';
end;

procedure TMainForm.UpdateStatus(Sender: TObject);
begin
  RefreshFloatingPanels;
  FStatusBar.SimpleText := Format('Polys: %d | Scenery: %d | Spawns: %d | Zoom: %d%%',
    [FDoc.PolyCount, FDoc.SceneryCount, FDoc.SpawnCount, Round(FDoc.Zoom * 100)]);
  UpdateCaption;
end;

procedure TMainForm.ViewportChanged(Sender: TObject);
begin
  UpdateStatus(Sender);
end;

procedure TMainForm.HandleFirstShow(Sender: TObject);
begin
  if FPanelsPositioned then
    Exit;
  PositionFloatingForms;
  FToolsForm.Show;
  FInfoForm.Show;
  FDisplayForm.Show;
  FSceneryForm.Show;
  FWaypointForm.Show;
  FPanelsPositioned := True;
  // Load custom cursors from skins directory (Windows: .cur files; others: no-op)
  LoadAllCursors(PWSkinsDir + 'cursors' + PathDelim);
end;

procedure TMainForm.NewFile(Sender: TObject);
begin
  FDoc.NewMap;
  FDoc.RebuildScreenCache;
  FUndo.Clear;
  FViewport.Renderer.FreeTextures;
  SetCurrentFile('');
  InvalidatePanelCaches;
  FViewport.RequestRepaint;
  UpdateStatus(nil);
end;

procedure TMainForm.OpenFile(Sender: TObject);
var
  Data: TPMSData;
  Err: string;
begin
  if not FOpenDialog.Execute then
    Exit;
  if LoadPMS(FOpenDialog.FileName, Data, Err) <> plrOK then
  begin
    MessageDlg('Open failed', Err, mtError, [mbOK], 0);
    Exit;
  end;

  FDoc.LoadFromPMS(Data);
  FDoc.RebuildScreenCache;
  FUndo.Clear;
  SetCurrentFile(FOpenDialog.FileName);
  LoadDocumentTextures;
  InvalidatePanelCaches;
  FViewport.RequestRepaint;
  UpdateStatus(nil);
end;

procedure TMainForm.SaveFile(Sender: TObject);
begin
  if FCurrentFilename = '' then
    SaveFileAs(Sender)
  else
    SaveDocumentTo(FCurrentFilename, False);
end;

procedure TMainForm.SaveFileAs(Sender: TObject);
begin
  if FCurrentFilename <> '' then
    FSaveDialog.FileName := FCurrentFilename;
  if FSaveDialog.Execute then
    SaveDocumentTo(FSaveDialog.FileName, False);
end;

procedure TMainForm.SaveAndCompileFile(Sender: TObject);
begin
  if FCurrentFilename <> '' then
    FSaveDialog.FileName := FCurrentFilename;
  if FSaveDialog.Execute then
    SaveDocumentTo(FSaveDialog.FileName, True);
end;

procedure TMainForm.HandleClose(Sender: TObject; var CloseAction: TCloseAction);
begin
  // Prompt for unsaved changes
  if FDoc.Modified then
  begin
    case Application.MessageBox(
      PChar('The current map has unsaved changes. Quit anyway?'),
      PChar('PolyWorks'), MB_YESNO or MB_ICONQUESTION) of
      IDNO:
      begin
        CloseAction := caNone;
        Exit;
      end;
    end;
  end;
  // Release OpenGL resources while context is still valid
  if FViewport <> nil then
    FViewport.FreeResources;
  CloseAction := caFree;
  // Ensure the process actually terminates
  Application.Terminate;
end;

procedure TMainForm.QuitApp(Sender: TObject);
begin
  Close; // will trigger HandleClose
end;

procedure TMainForm.UndoAction(Sender: TObject);
begin
  if FUndo.Undo(FDoc) then
  begin
    InvalidatePanelCaches;
    LoadDocumentTextures;
    FViewport.RequestRepaint;
    UpdateStatus(nil);
  end;
end;

procedure TMainForm.RedoAction(Sender: TObject);
begin
  if FUndo.Redo(FDoc) then
  begin
    InvalidatePanelCaches;
    LoadDocumentTextures;
    FViewport.RequestRepaint;
    UpdateStatus(nil);
  end;
end;

procedure TMainForm.SelectAllAction(Sender: TObject);
begin
  SelectAll;
  FViewport.RequestRepaint;
  UpdateStatus(nil);
end;

procedure TMainForm.DeselectAllAction(Sender: TObject);
begin
  FDoc.ClearSelection;
  FViewport.RequestRepaint;
  UpdateStatus(nil);
end;

procedure TMainForm.DeleteAction(Sender: TObject);
begin
  if not HasSelection then
    Exit;
  FUndo.Push(FDoc);
  FDoc.DeleteSelected;
  InvalidatePanelCaches;
  FViewport.RequestRepaint;
  UpdateStatus(nil);
end;

procedure TMainForm.ToggleViewOption(Sender: TObject);
var
  Item: TMenuItem;
begin
  Item := Sender as TMenuItem;
  case Item.Tag of
    VIEW_SHOW_POLYS:      FViewport.ViewSettings.ShowPolys := Item.Checked;
    VIEW_SHOW_WIREFRAME:  FViewport.ViewSettings.ShowWireframe := Item.Checked;
    VIEW_SHOW_POINTS:     FViewport.ViewSettings.ShowPoints := Item.Checked;
    VIEW_SHOW_GRID:       FViewport.ViewSettings.ShowGrid := Item.Checked;
    VIEW_SHOW_OBJECTS:    FViewport.ViewSettings.ShowObjects := Item.Checked;
    VIEW_SHOW_WAYPOINTS:  FViewport.ViewSettings.ShowWaypoints := Item.Checked;
    VIEW_SHOW_LIGHTS:     FViewport.ViewSettings.ShowLights := Item.Checked;
    VIEW_SHOW_SKETCH:     FViewport.ViewSettings.ShowSketch := Item.Checked;
    VIEW_SHOW_TEXTURE:    FViewport.ViewSettings.ShowTexture := Item.Checked;
    VIEW_SHOW_BACKGROUND: FViewport.ViewSettings.ShowBackground := Item.Checked;
    VIEW_SHOW_SCENERY:    FViewport.ViewSettings.ShowScenery := Item.Checked;
  end;
  if FDisplayForm <> nil then
    FDisplayForm.SetViewSettings(FViewport.ViewSettings);
  ApplyViewSettingsToConfig;
  FViewport.RequestRepaint;
  UpdateStatus(nil);
end;

procedure TMainForm.ZoomInAction(Sender: TObject);
begin
  FDoc.SetZoom(FDoc.Zoom * 1.25, FViewport.ClientWidth * 0.5, FViewport.ClientHeight * 0.5);
  FViewport.RequestRepaint;
  UpdateStatus(nil);
end;

procedure TMainForm.ZoomOutAction(Sender: TObject);
begin
  FDoc.SetZoom(FDoc.Zoom / 1.25, FViewport.ClientWidth * 0.5, FViewport.ClientHeight * 0.5);
  FViewport.RequestRepaint;
  UpdateStatus(nil);
end;

procedure TMainForm.ResetZoomAction(Sender: TObject);
begin
  FDoc.SetZoom(1.0, FViewport.ClientWidth * 0.5, FViewport.ClientHeight * 0.5);
  FViewport.RequestRepaint;
  UpdateStatus(nil);
end;

procedure TMainForm.SelectTool(Sender: TObject);
var
  ToolID: Integer;
begin
  ToolID := (Sender as TComponent).Tag;
  FViewport.SetActiveTool(ToolID);
  SyncToolUI;
  FViewport.SetFocus;
end;

procedure TMainForm.MapPropertiesAction(Sender: TObject);
begin
  if ShowMapPropertiesDialog(FDoc.Options) then
  begin
    FDoc.Modified := True;
    FDoc.RebuildScreenCache;
    LoadDocumentTextures;
    InvalidatePanelCaches;
    FViewport.RequestRepaint;
    UpdateStatus(nil);
  end;
end;

procedure TMainForm.PreferencesAction(Sender: TObject);
begin
  if ShowPreferencesDialog(FCfg) then
  begin
    FUndo.Free;
    FUndo := TUndoStack.Create(FCfg.UndoDepth);
    FViewport.UndoStack := FUndo;
    ApplyConfigToViewSettings;
    FViewport.RequestRepaint;
    UpdateStatus(nil);
  end;
end;

end.
