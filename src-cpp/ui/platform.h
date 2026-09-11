#pragma once
/*
 * platform.h — the handful of OS services the UI needs that neither the C++
 * standard library nor GLFW provides.
 *
 * This exists because the GUI toolkit is no longer a portability layer.
 * wxWidgets supplied wxStandardPaths, wxConfig, wxFileDialog and wxExecute;
 * with Dear ImGui the application is responsible for them.  Everything here is
 * deliberately small and free of UI dependencies so it can be unit tested.
 */

#include <string>
#include <vector>

namespace pw {

/* Absolute path of the running executable, resolved through the platform's
   own mechanism (/proc/self/exe, _NSGetExecutablePath, GetModuleFileName)
   rather than argv[0], which is whatever the caller chose to pass. */
std::string executablePath();

/* Directory containing the executable.  This is the VB6 `appPath`, and the
   anchor for every portable asset lookup: a distribution extracted to an
   arbitrary directory must find its own files relative to this. */
std::string appDir();

/* Directory a macOS bundle keeps its data in (Contents/Resources).  Empty on
   other platforms and for a non-bundled build. */
std::string bundleResourcesDir();

/* Per-user writable directory for settings, used only when appDir() is not
   writable (an installed, read-only copy). */
std::string userConfigDir();

/* installer/skins/default equivalent: the directory holding cursors, skin
   bitmaps and notfound.bmp.  Searched relative to the executable, the bundle
   and the working directory, in that order, so that a portable extraction, an
   installed copy and a development checkout all work. */
std::string skinsPath();

/* The Soldat/OpenSoldat game data directory inferred from the skins path.
   Preferences can override it; this is only the starting guess. */
/* The directory holding skins/, palettes/ and lists/ (VB6 appPath). */
std::string appDataDir();

std::string inferredGameDir();

bool fileExists(const std::string& path);
bool dirExists(const std::string& path);

/* Launch a program detached from PolyWorks.  Returns false when it could not
   be started (File > Run OpenSoldat). */
bool launchProgram(const std::string& path, const std::vector<std::string>& args);

/* Open a URL or file with the desktop's default handler (Help). */
void openInShell(const std::string& target);

}  // namespace pw
