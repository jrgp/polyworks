unit frmtexture;

{$mode objfpc}{$H+}

interface

uses
  Classes, SysUtils, Forms, Controls, StdCtrls;

type
  TTextureBrowserDialog = class(TForm)
  private
    FDirLabel: TLabel;
    FPreviewLabel: TLabel;
    FListBox: TListBox;
    FOKButton: TButton;
    FCancelButton: TButton;
    procedure ListBoxClick(Sender: TObject);
  public
    constructor Create(AOwner: TComponent); override;
    procedure LoadFromDir(const Dir: string);
    function SelectedFile: string;
  end;

function ShowTextureBrowser(const Dir: string; out Filename: string): Boolean;

implementation

procedure TTextureBrowserDialog.ListBoxClick(Sender: TObject);
begin
  if FListBox.ItemIndex >= 0 then
    FPreviewLabel.Caption := 'Preview: ' + FListBox.Items[FListBox.ItemIndex]
  else
    FPreviewLabel.Caption := 'Preview: None';
end;

constructor TTextureBrowserDialog.Create(AOwner: TComponent);
begin
  inherited CreateNew(AOwner);
  BorderStyle := bsSingle; // bsDialog creates NSPanel on macOS (lclSyncCheck crash workaround)
  BorderIcons := [];
  Position := poScreenCenter;
  Caption := 'Texture Browser';
  ClientWidth := 420;
  ClientHeight := 320;

  FDirLabel := TLabel.Create(Self);
  FDirLabel.Parent := Self;
  FDirLabel.Left := 8;
  FDirLabel.Top := 8;
  FDirLabel.Width := 400;
  FDirLabel.Caption := 'Directory:';

  FListBox := TListBox.Create(Self);
  FListBox.Parent := Self;
  FListBox.Left := 8;
  FListBox.Top := 32;
  FListBox.Width := 400;
  FListBox.Height := 224;
  FListBox.Sorted := True;
  FListBox.OnClick := @ListBoxClick;

  FPreviewLabel := TLabel.Create(Self);
  FPreviewLabel.Parent := Self;
  FPreviewLabel.Left := 8;
  FPreviewLabel.Top := 264;
  FPreviewLabel.Width := 400;
  FPreviewLabel.Caption := 'Preview: None';

  FOKButton := TButton.Create(Self);
  FOKButton.Parent := Self;
  FOKButton.Left := 252;
  FOKButton.Top := 288;
  FOKButton.Width := 75;
  FOKButton.Caption := 'OK';
  FOKButton.ModalResult := mrOK;
  FOKButton.Default := True;

  FCancelButton := TButton.Create(Self);
  FCancelButton.Parent := Self;
  FCancelButton.Left := 333;
  FCancelButton.Top := 288;
  FCancelButton.Width := 75;
  FCancelButton.Caption := 'Cancel';
  FCancelButton.ModalResult := mrCancel;
  FCancelButton.Cancel := True;
end;

procedure TTextureBrowserDialog.LoadFromDir(const Dir: string);
var
  SR: TSearchRec;
  Ext: string;
begin
  FDirLabel.Caption := 'Directory: ' + Dir;
  FListBox.Items.BeginUpdate;
  try
    FListBox.Clear;
    if DirectoryExists(Dir) and (FindFirst(IncludeTrailingPathDelimiter(Dir) + '*.*', faAnyFile, SR) = 0) then
    begin
      repeat
        if (SR.Attr and faDirectory) = 0 then
        begin
          Ext := LowerCase(ExtractFileExt(SR.Name));
          if (Ext = '.bmp') or (Ext = '.png') then
            FListBox.Items.Add(SR.Name);
        end;
      until FindNext(SR) <> 0;
      FindClose(SR);
    end;
  finally
    FListBox.Items.EndUpdate;
  end;
  FListBox.ItemIndex := -1;
  FPreviewLabel.Caption := 'Preview: None';
end;

function TTextureBrowserDialog.SelectedFile: string;
begin
  if FListBox.ItemIndex >= 0 then
    Result := FListBox.Items[FListBox.ItemIndex]
  else
    Result := '';
end;

function ShowTextureBrowser(const Dir: string; out Filename: string): Boolean;
var
  Dialog: TTextureBrowserDialog;
begin
  Dialog := TTextureBrowserDialog.Create(nil);
  try
    Dialog.LoadFromDir(Dir);
    Result := Dialog.ShowModal = mrOK;
    if Result then
      Filename := Dialog.SelectedFile
    else
      Filename := '';
  finally
    Dialog.Free;
  end;
end;

end.
