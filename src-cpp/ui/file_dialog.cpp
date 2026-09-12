#include "file_dialog.h"

#if defined(PW_HAVE_NFD)

/* glfw3native.h only declares the accessor for a platform whose macro is
   defined first, and nfd_glfw3.h includes it. */
#if defined(_WIN32)
#define GLFW_EXPOSE_NATIVE_WIN32
#elif defined(__APPLE__)
#define GLFW_EXPOSE_NATIVE_COCOA
#endif

#include <nfd.h>
#include <nfd_glfw3.h>

#include <string>

namespace pw {
namespace {

/* NFD_Init/NFD_Quit bracket every call rather than being done once at
   start-up.  On Windows NFD_Init is CoInitializeEx and NFD_Quit is
   CoUninitialize, and holding COM initialised for the process lifetime from a
   library that does not own the apartment is how apartment-mode conflicts
   start.  The pair costs nothing next to showing a dialog. */
struct NfdSession {
    bool ok = false;
    NfdSession() { ok = (NFD_Init() == NFD_OKAY); }
    ~NfdSession() {
        if (ok) {
            NFD_Quit();
        }
    }
    NfdSession(const NfdSession&) = delete;
    NfdSession& operator=(const NfdSession&) = delete;
};

nfdwindowhandle_t parentHandle(GLFWwindow* parent) {
    nfdwindowhandle_t handle{};
    if (parent != nullptr) {
        /* Documented to be ignorable: a value-initialised handle just means an
           unparented dialog, which is still perfectly usable. */
        NFD_GetNativeWindowFromGLFWWindow(parent, &handle);
    }
    return handle;
}

/* The rest of the UI spells extensions with the dot (".pms"); NFD wants them
   without.  An empty filter list means "any file", which is what an empty
   extension has always meant here. */
std::string bareExtension(const std::string& extension) {
    if (!extension.empty() && extension.front() == '.') {
        return extension.substr(1);
    }
    return extension;
}

const char* orNull(const std::string& s) {
    return s.empty() ? nullptr : s.c_str();
}

FileDialogResult finish(nfdresult_t r, nfdu8char_t* path, std::string& out) {
    if (r == NFD_OKAY && path != nullptr) {
        out.assign(path);
        NFD_FreePathU8(path);
        return FileDialogResult::Chosen;
    }
    /* NFD_ERROR is reported as a cancellation on purpose: there is nothing
       useful the editor can do about it, and falling through to "the user
       chose nothing" is exactly the right behaviour. */
    return FileDialogResult::Cancelled;
}

}  // namespace

bool haveNativeFileDialog() { return true; }

FileDialogResult nativeOpenFile(GLFWwindow* parent, const std::string& extension,
                                const std::string& startDir, std::string& out) {
    NfdSession session;
    if (!session.ok) {
        return FileDialogResult::Unsupported;
    }

    const std::string ext = bareExtension(extension);
    const nfdu8filteritem_t filter{"PolyWorks", ext.c_str()};

    nfdopendialogu8args_t args{};
    args.filterList = ext.empty() ? nullptr : &filter;
    args.filterCount = ext.empty() ? 0 : 1;
    args.defaultPath = orNull(startDir);
    args.parentWindow = parentHandle(parent);

    nfdu8char_t* path = nullptr;
    return finish(NFD_OpenDialogU8_With(&path, &args), path, out);
}

FileDialogResult nativeSaveFile(GLFWwindow* parent, const std::string& extension,
                                const std::string& startDir,
                                const std::string& startName, std::string& out) {
    NfdSession session;
    if (!session.ok) {
        return FileDialogResult::Unsupported;
    }

    const std::string ext = bareExtension(extension);
    const nfdu8filteritem_t filter{"PolyWorks", ext.c_str()};

    nfdsavedialogu8args_t args{};
    args.filterList = ext.empty() ? nullptr : &filter;
    args.filterCount = ext.empty() ? 0 : 1;
    args.defaultPath = orNull(startDir);
    args.defaultName = orNull(startName);
    args.parentWindow = parentHandle(parent);

    nfdu8char_t* path = nullptr;
    return finish(NFD_SaveDialogU8_With(&path, &args), path, out);
}

FileDialogResult nativePickFolder(GLFWwindow* parent, const std::string& startDir,
                                  std::string& out) {
    NfdSession session;
    if (!session.ok) {
        return FileDialogResult::Unsupported;
    }

    nfdpickfolderu8args_t args{};
    args.defaultPath = orNull(startDir);
    args.parentWindow = parentHandle(parent);

    nfdu8char_t* path = nullptr;
    return finish(NFD_PickFolderU8_With(&path, &args), path, out);
}

}  // namespace pw

#else  /* no native chooser on this platform */

namespace pw {

bool haveNativeFileDialog() { return false; }

FileDialogResult nativeOpenFile(GLFWwindow*, const std::string&,
                                const std::string&, std::string&) {
    return FileDialogResult::Unsupported;
}

FileDialogResult nativeSaveFile(GLFWwindow*, const std::string&,
                                const std::string&, const std::string&,
                                std::string&) {
    return FileDialogResult::Unsupported;
}

FileDialogResult nativePickFolder(GLFWwindow*, const std::string&, std::string&) {
    return FileDialogResult::Unsupported;
}

}  // namespace pw

#endif
