/*
 * main.cpp — process entry point.
 *
 * The original is a VB6 application whose Sub Main opens frmOpenSoldatMapEditor
 * and hands it the command line (modMain.bas).  This does the same: everything
 * of substance is in App.
 */

#include "app.h"

#include <cstdio>

int main(int argc, char** argv) {
    pw::App app;
    if (!app.initialise(argc, argv)) {
        return 1;
    }
    app.run();
    app.shutdown();
    return 0;
}

#if defined(_WIN32)
/* PolyWorks.exe is linked -mwindows so that no console appears, which means
   the entry point is WinMain.  GLFW does not need the instance handles, and
   __argc/__argv give the same command line main() would have received. */
#include <windows.h>
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    return main(__argc, __argv);
}
#endif
