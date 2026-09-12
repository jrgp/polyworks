#pragma once
/*
 * file_dialog.h — the platform's own file chooser, where it has one.
 *
 * The original opens the Windows common dialog for File > Open and File >
 * Save As, so a native chooser is what is being preserved, not a concession
 * to modern taste.  It also matters practically: only the real dialog knows
 * about the sidebar, recent places, network volumes, drag-and-drop, the
 * search field, and -- on macOS -- the file-access prompts that a sandboxed
 * application must go through to read anything outside its container.
 *
 * Windows and macOS get the native chooser (Win32 IFileDialog and Cocoa
 * NSOpenPanel/NSSavePanel, through Native File Dialog Extended).  Linux does
 * not: every backend there needs GTK or a D-Bus portal, and dragging GTK back
 * in would undo the ImGui migration, so it keeps the built-in browser.
 *
 * Callers must therefore handle Unsupported, not assume a path comes back.
 * The dialog is genuinely modal and blocks the calling thread, which is safe
 * here only because it is called from the frame loop on the main thread; the
 * frame it is called from simply takes as long as the user takes.
 */

#include <string>

struct GLFWwindow;

namespace pw {

enum class FileDialogResult {
    Chosen,       /* `out` holds the path */
    Cancelled,    /* the user dismissed the dialog */
    Unsupported,  /* no native chooser on this platform: use the fallback */
};

/* True when this build has a native chooser at all.  Callers use it to decide
   between the native dialog and the in-application browser *before* doing any
   work, so the two paths do not both run. */
bool haveNativeFileDialog();

/* `extension` is a bare suffix with the leading dot, as the rest of the UI
   spells it (".pms"); it is turned into the platform's filter form.  An empty
   extension means "any file".  `parent` may be null, and only affects which
   window the dialog is modal to. */
FileDialogResult nativeOpenFile(GLFWwindow* parent, const std::string& extension,
                                const std::string& startDir, std::string& out);

FileDialogResult nativeSaveFile(GLFWwindow* parent, const std::string& extension,
                                const std::string& startDir,
                                const std::string& startName, std::string& out);

FileDialogResult nativePickFolder(GLFWwindow* parent, const std::string& startDir,
                                  std::string& out);

}  // namespace pw
