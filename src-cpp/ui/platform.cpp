#include "platform.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <system_error>

#if defined(_WIN32)
#include <windows.h>
#include <shellapi.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#include <unistd.h>
#include <sys/wait.h>
#else
#include <unistd.h>
#include <sys/wait.h>
#endif

namespace fs = std::filesystem;

namespace pw {
namespace {

fs::path exePathImpl() {
#if defined(_WIN32)
    std::wstring buf(MAX_PATH, L'\0');
    for (;;) {
        const DWORD n = GetModuleFileNameW(nullptr, buf.data(),
                                           static_cast<DWORD>(buf.size()));
        if (n == 0) {
            return {};
        }
        if (n < buf.size()) {
            buf.resize(n);
            break;
        }
        buf.resize(buf.size() * 2);
    }
    return fs::path(buf);
#elif defined(__APPLE__)
    uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);
    std::string buf(size, '\0');
    if (_NSGetExecutablePath(buf.data(), &size) != 0) {
        return {};
    }
    buf.resize(std::char_traits<char>::length(buf.c_str()));
    std::error_code ec;
    const fs::path canonical = fs::weakly_canonical(fs::path(buf), ec);
    return ec ? fs::path(buf) : canonical;
#else
    std::error_code ec;
    const fs::path link = fs::read_symlink("/proc/self/exe", ec);
    return ec ? fs::path() : link;
#endif
}

}  // namespace

std::string executablePath() {
    static const std::string cached = exePathImpl().string();
    return cached;
}

std::string appDir() {
    const fs::path exe(executablePath());
    return exe.empty() ? std::string(".") : exe.parent_path().string();
}

std::string bundleResourcesDir() {
#if defined(__APPLE__)
    /* .../PolyWorks.app/Contents/MacOS/polyworks -> .../Contents/Resources */
    const fs::path exe(executablePath());
    if (exe.empty()) {
        return {};
    }
    const fs::path macos = exe.parent_path();
    if (macos.filename() != "MacOS") {
        return {};
    }
    const fs::path resources = macos.parent_path() / "Resources";
    return dirExists(resources.string()) ? resources.string() : std::string();
#else
    return {};
#endif
}

std::string userConfigDir() {
    std::error_code ec;
#if defined(_WIN32)
    if (const char* appdata = std::getenv("APPDATA")) {
        const fs::path dir = fs::path(appdata) / "PolyWorks";
        fs::create_directories(dir, ec);
        return dir.string();
    }
#elif defined(__APPLE__)
    if (const char* home = std::getenv("HOME")) {
        const fs::path dir =
            fs::path(home) / "Library" / "Application Support" / "PolyWorks";
        fs::create_directories(dir, ec);
        return dir.string();
    }
#else
    if (const char* xdg = std::getenv("XDG_CONFIG_HOME")) {
        const fs::path dir = fs::path(xdg) / "polyworks";
        fs::create_directories(dir, ec);
        return dir.string();
    }
    if (const char* home = std::getenv("HOME")) {
        const fs::path dir = fs::path(home) / ".config" / "polyworks";
        fs::create_directories(dir, ec);
        return dir.string();
    }
#endif
    return appDir();
}

bool fileExists(const std::string& path) {
    std::error_code ec;
    return !path.empty() && fs::is_regular_file(path, ec);
}

bool dirExists(const std::string& path) {
    std::error_code ec;
    return !path.empty() && fs::is_directory(path, ec);
}

std::string skinsPath() {
    std::vector<fs::path> candidates;
    const fs::path exe(appDir());
    candidates.push_back(exe / "skins" / "default");

    const std::string res = bundleResourcesDir();
    if (!res.empty()) {
        candidates.push_back(fs::path(res) / "skins" / "default");
    }

    /* Installed layout: <prefix>/bin/polyworks, data under <prefix>/share. */
    candidates.push_back(exe.parent_path() / "share" / "polyworks" / "skins" / "default");

    /* Development checkout, run from the repository root or from build/. */
    std::error_code ec;
    const fs::path cwd = fs::current_path(ec);
    if (!ec) {
        candidates.push_back(cwd / "installer" / "skins" / "default");
        candidates.push_back(cwd / "skins" / "default");
        candidates.push_back(cwd / ".." / "installer" / "skins" / "default");
    }

    for (const fs::path& candidate : candidates) {
        if (dirExists(candidate.string())) {
            return fs::weakly_canonical(candidate, ec).string();
        }
    }
    return {};
}

std::string appDataDir() {
    /* The original keeps skins/, palettes/ and lists/ together under appPath.
       In a packaged build that is the executable's own directory; in a
       development checkout it is installer/, which is where the repository's
       copies of all three live.  skinsPath() already knows how to find the
       first of them, so the rest follow from it. */
    const std::string skins = skinsPath();
    if (!skins.empty()) {
        return fs::path(skins).parent_path().parent_path().string();
    }
    return appDir();
}

std::string inferredGameDir() {
    /* <game>/skins/default -> <game>.  In a development checkout that yields
       installer/, which is where the repository's own skins live; the real
       game directory is a preference. */
    const std::string skins = skinsPath();
    if (skins.empty()) {
        return {};
    }
    return fs::path(skins).parent_path().parent_path().string();
}

bool launchProgram(const std::string& path,
                   const std::vector<std::string>& args) {
    if (!fileExists(path)) {
        return false;
    }
#if defined(_WIN32)
    std::string cmd = "\"" + path + "\"";
    for (const std::string& a : args) {
        cmd += " \"" + a + "\"";
    }
    STARTUPINFOA si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    const std::string workDir = fs::path(path).parent_path().string();
    if (!CreateProcessA(nullptr, cmd.data(), nullptr, nullptr, FALSE, 0, nullptr,
                        workDir.empty() ? nullptr : workDir.c_str(), &si, &pi)) {
        return false;
    }
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return true;
#else
    const pid_t pid = fork();
    if (pid < 0) {
        return false;
    }
    if (pid == 0) {
        /* Detach from PolyWorks so the game outlives the editor and never
           becomes a zombie we have to reap. */
        if (fork() == 0) {
            std::vector<char*> argv;
            argv.push_back(const_cast<char*>(path.c_str()));
            for (const std::string& a : args) {
                argv.push_back(const_cast<char*>(a.c_str()));
            }
            argv.push_back(nullptr);
            const std::string workDir = fs::path(path).parent_path().string();
            if (!workDir.empty()) {
                if (chdir(workDir.c_str()) != 0) { /* keep the inherited cwd */ }
            }
            execv(path.c_str(), argv.data());
        }
        _exit(0);
    }
    int status = 0;
    waitpid(pid, &status, 0);
    return true;
#endif
}

void openInShell(const std::string& target) {
#if defined(_WIN32)
    ShellExecuteA(nullptr, "open", target.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
#elif defined(__APPLE__)
    launchProgram("/usr/bin/open", {target});
#else
    launchProgram("/usr/bin/xdg-open", {target});
#endif
}

}  // namespace pw
