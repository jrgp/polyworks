/* guidriver -- drive the real PolyWorks window with real X11 input events.
 *
 * The headless suite in src-cpp/tests covers the core, but src-cpp/ui is not
 * linked into it, so editor and interaction behaviour can only be checked by
 * operating the application.  This does that: it injects genuine pointer and
 * key events through the XTest extension, so they arrive at GLFW exactly as a
 * user's would, and it can send the window-manager close request that the
 * title bar's close button sends.
 *
 * Build:
 *     gcc -o guidriver tools/guidriver.c -lX11 -lXtst
 *
 * Use (with the application running on the same display):
 *     Xvfb :78 -screen 0 1600x1000x24 &
 *     DISPLAY=:78 ./build/bin/polyworks maps/ctf_Ash.pms &
 *     DISPLAY=:78 ./guidriver focus move 400 400 click key w sleep 500
 *     DISPLAY=:78 import -window root shot.png
 *
 * Commands, applied in order:
 *     move X Y        warp the pointer to a root-window coordinate
 *     click           press and release button 1 where the pointer is
 *     rclick          the same with button 3
 *     drag X Y        press button 1, move to X Y, release
 *     key NAME        tap a key, NAME being an X keysym name ("w", "Delete")
 *     ctrlkey NAME    the same with Control held
 *     shiftkey NAME   the same with Shift held
 *     sleep MS        wait
 *     focus           raise the PolyWorks window and give it the input focus,
 *                     which no one else will do when there is no window
 *                     manager running, as under a bare Xvfb
 *     close           send WM_DELETE_WINDOW, i.e. the title bar close button
 *
 * Every command is followed by a short settle delay so the application has a
 * frame or two to react before the next one arrives.
 */

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>
#include <X11/extensions/XTest.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define SETTLE_US 120000

/* The window we want is the one whose name mentions PolyWorks.  Without a
   window manager it is a direct child of the root, but search the whole tree
   anyway so this also works on a normal desktop, where it will be reparented
   into a frame. */
static Window findApp(Display* d, Window w) {
    char* name = NULL;
    Window root, parent, *children = NULL;
    unsigned int n = 0;

    if (XFetchName(d, w, &name) && name) {
        int match = strstr(name, "PolyWorks") != NULL;
        XFree(name);
        if (match) {
            return w;
        }
    }
    if (XQueryTree(d, w, &root, &parent, &children, &n)) {
        for (unsigned int i = 0; i < n; i++) {
            Window found = findApp(d, children[i]);
            if (found) {
                XFree(children);
                return found;
            }
        }
        if (children) {
            XFree(children);
        }
    }
    return 0;
}

static void tap(Display* d, const char* keyName, KeySym modifier) {
    KeySym sym = XStringToKeysym(keyName);
    if (sym == NoSymbol) {
        fprintf(stderr, "guidriver: unknown keysym \"%s\"\n", keyName);
        return;
    }
    KeyCode key = XKeysymToKeycode(d, sym);
    KeyCode mod = modifier ? XKeysymToKeycode(d, modifier) : 0;

    if (mod) {
        XTestFakeKeyEvent(d, mod, True, 0);
        XFlush(d);
        usleep(40000);
    }
    XTestFakeKeyEvent(d, key, True, 0);
    XFlush(d);
    usleep(40000);
    XTestFakeKeyEvent(d, key, False, 0);
    XFlush(d);
    usleep(40000);
    if (mod) {
        XTestFakeKeyEvent(d, mod, False, 0);
    }
}

static void clickButton(Display* d, unsigned int button) {
    XTestFakeButtonEvent(d, button, True, 0);
    XFlush(d);
    usleep(60000);
    XTestFakeButtonEvent(d, button, False, 0);
}

static int closeApp(Display* d) {
    Window w = findApp(d, DefaultRootWindow(d));
    if (!w) {
        fprintf(stderr, "guidriver: no PolyWorks window found\n");
        return 1;
    }

    XEvent ev;
    memset(&ev, 0, sizeof ev);
    ev.xclient.type = ClientMessage;
    ev.xclient.window = w;
    ev.xclient.message_type = XInternAtom(d, "WM_PROTOCOLS", False);
    ev.xclient.format = 32;
    ev.xclient.data.l[0] = (long)XInternAtom(d, "WM_DELETE_WINDOW", False);
    ev.xclient.data.l[1] = CurrentTime;
    XSendEvent(d, w, False, NoEventMask, &ev);
    return 0;
}

static int focusApp(Display* d) {
    Window w = findApp(d, DefaultRootWindow(d));
    if (!w) {
        fprintf(stderr, "guidriver: no PolyWorks window found\n");
        return 1;
    }
    XRaiseWindow(d, w);
    XSetInputFocus(d, w, RevertToParent, CurrentTime);
    return 0;
}

int main(int argc, char** argv) {
    Display* d = XOpenDisplay(NULL);
    if (!d) {
        fprintf(stderr, "guidriver: cannot open DISPLAY\n");
        return 1;
    }

    int major = 0, minor = 0, evbase = 0, errbase = 0;
    if (!XTestQueryExtension(d, &evbase, &errbase, &major, &minor)) {
        fprintf(stderr, "guidriver: server has no XTEST extension\n");
        return 1;
    }

    for (int i = 1; i < argc; i++) {
        const char* cmd = argv[i];

        if (!strcmp(cmd, "move") && i + 2 < argc) {
            XTestFakeMotionEvent(d, -1, atoi(argv[i + 1]), atoi(argv[i + 2]), 0);
            i += 2;
        } else if (!strcmp(cmd, "click")) {
            clickButton(d, 1);
        } else if (!strcmp(cmd, "rclick")) {
            clickButton(d, 3);
        } else if (!strcmp(cmd, "drag") && i + 2 < argc) {
            XTestFakeButtonEvent(d, 1, True, 0);
            XFlush(d);
            usleep(60000);
            XTestFakeMotionEvent(d, -1, atoi(argv[i + 1]), atoi(argv[i + 2]), 0);
            XFlush(d);
            usleep(60000);
            XTestFakeButtonEvent(d, 1, False, 0);
            i += 2;
        } else if (!strcmp(cmd, "key") && i + 1 < argc) {
            tap(d, argv[++i], 0);
        } else if (!strcmp(cmd, "ctrlkey") && i + 1 < argc) {
            tap(d, argv[++i], XK_Control_L);
        } else if (!strcmp(cmd, "shiftkey") && i + 1 < argc) {
            tap(d, argv[++i], XK_Shift_L);
        } else if (!strcmp(cmd, "sleep") && i + 1 < argc) {
            usleep(atoi(argv[++i]) * 1000);
        } else if (!strcmp(cmd, "focus")) {
            if (focusApp(d)) {
                return 2;
            }
        } else if (!strcmp(cmd, "close")) {
            if (closeApp(d)) {
                return 2;
            }
        } else {
            fprintf(stderr, "guidriver: unknown command \"%s\"\n", cmd);
            return 1;
        }

        XFlush(d);
        usleep(SETTLE_US);
    }

    XCloseDisplay(d);
    return 0;
}
