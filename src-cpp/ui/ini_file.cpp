#include "ini_file.h"

#include "platform.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <system_error>

namespace fs = std::filesystem;

namespace pw {
namespace {

std::string trim(const std::string& s) {
    const auto first = s.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return {};
    }
    const auto last = s.find_last_not_of(" \t\r\n");
    return s.substr(first, last - first + 1);
}

}  // namespace

void IniFile::load(const std::string& path) {
    m_path = path;
    m_data.clear();

    std::ifstream in(path);
    if (!in) {
        return;
    }

    std::string section;
    std::string line;
    while (std::getline(in, line)) {
        const std::string t = trim(line);
        if (t.empty() || t[0] == ';' || t[0] == '#') {
            continue;
        }
        if (t.front() == '[' && t.back() == ']') {
            section = trim(t.substr(1, t.size() - 2));
            continue;
        }
        const auto eq = t.find('=');
        if (eq == std::string::npos) {
            continue;
        }
        m_data[section][trim(t.substr(0, eq))] = trim(t.substr(eq + 1));
    }
}

bool IniFile::save() const {
    if (m_path.empty()) {
        return false;
    }
    std::error_code ec;
    fs::create_directories(fs::path(m_path).parent_path(), ec);

    /* Written through a temporary and renamed: a crash or a full disk part way
       through must not leave a truncated settings file behind, which would
       silently reset every preference. */
    const std::string tmp = m_path + ".tmp";
    {
        std::ofstream out(tmp, std::ios::trunc);
        if (!out) {
            return false;
        }
        for (const auto& [section, keys] : m_data) {
            if (!section.empty()) {
                out << '[' << section << "]\n";
            }
            for (const auto& [key, value] : keys) {
                out << key << '=' << value << '\n';
            }
            out << '\n';
        }
        if (!out) {
            return false;
        }
    }
    fs::rename(tmp, m_path, ec);
    if (ec) {
        fs::remove(tmp, ec);
        return false;
    }
    return true;
}

bool IniFile::has(const std::string& section, const std::string& key) const {
    const auto s = m_data.find(section);
    return s != m_data.end() && s->second.count(key) != 0;
}

std::string IniFile::readString(const std::string& section,
                                const std::string& key,
                                const std::string& fallback) const {
    const auto s = m_data.find(section);
    if (s == m_data.end()) {
        return fallback;
    }
    const auto k = s->second.find(key);
    return k == s->second.end() ? fallback : k->second;
}

long IniFile::readInt(const std::string& section, const std::string& key,
                      long fallback) const {
    const std::string v = readString(section, key);
    if (v.empty()) {
        return fallback;
    }
    try {
        return std::stol(v);
    } catch (const std::exception&) {
        return fallback;
    }
}

double IniFile::readDouble(const std::string& section, const std::string& key,
                           double fallback) const {
    const std::string v = readString(section, key);
    if (v.empty()) {
        return fallback;
    }
    try {
        return std::stod(v);
    } catch (const std::exception&) {
        return fallback;
    }
}

bool IniFile::readBool(const std::string& section, const std::string& key,
                       bool fallback) const {
    const std::string v = readString(section, key);
    if (v.empty()) {
        return fallback;
    }
    /* "1"/"0" is what this writes; "true"/"True" is accepted because the
       original's workspace files (installer/Workspace/current.ini) use it. */
    return v == "1" || v == "true" || v == "True" || v == "TRUE" ||
           v == "yes" || v == "-1";
}

void IniFile::write(const std::string& section, const std::string& key,
                    const std::string& value) {
    m_data[section][key] = value;
}

void IniFile::write(const std::string& section, const std::string& key,
                    long value) {
    m_data[section][key] = std::to_string(value);
}

void IniFile::write(const std::string& section, const std::string& key,
                    double value) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.6g", value);
    m_data[section][key] = buf;
}

void IniFile::write(const std::string& section, const std::string& key,
                    bool value) {
    m_data[section][key] = value ? "1" : "0";
}

void IniFile::erase(const std::string& section, const std::string& key) {
    const auto s = m_data.find(section);
    if (s != m_data.end()) {
        s->second.erase(key);
    }
}

std::string IniFile::preferredPath(const std::string& name) {
    /* A macOS .app is a signed, read-only container: writing into
       Contents/MacOS would invalidate the code signature and make Gatekeeper
       refuse to launch it.  Settings for a bundled build are always per-user. */
    if (!bundleResourcesDir().empty()) {
        return (fs::path(userConfigDir()) / name).string();
    }

    const fs::path beside = fs::path(appDir()) / name;

    /* Writability is tested, not guessed: on Windows the Program Files
       directory is read-only for a normal user but perfectly readable, and on
       macOS the application bundle must never be written to. */
    if (fileExists(beside.string())) {
        std::ofstream probe(beside.string(), std::ios::app);
        if (probe) {
            return beside.string();
        }
    } else {
        std::ofstream probe(beside.string(), std::ios::app);
        if (probe) {
            probe.close();
            std::error_code ec;
            fs::remove(beside, ec);
            return beside.string();
        }
    }

    return (fs::path(userConfigDir()) / name).string();
}

}  // namespace pw
