#pragma once
/*
 * ini_file.h — the settings store, replacing wxConfig.
 *
 * The original wrote a flat polyworks.ini next to the executable
 * (modConfig.bas), and the portable Windows distribution depends on that: a
 * copy extracted to a writable directory must keep its settings with itself,
 * not in the user profile.  When the application directory is not writable --
 * an installed copy, or a .app bundle -- the file falls back to the per-user
 * configuration directory.
 *
 * Format is the classic [section] key=value.  Values are stored verbatim;
 * leading and trailing whitespace around the '=' is stripped on read.
 */

#include <map>
#include <string>

namespace pw {

class IniFile {
public:
    /* Load from `path`.  A missing file is not an error: it simply yields an
       empty store, which is a first run. */
    void load(const std::string& path);
    bool save() const;

    const std::string& path() const { return m_path; }

    bool has(const std::string& section, const std::string& key) const;

    std::string readString(const std::string& section, const std::string& key,
                           const std::string& fallback = {}) const;
    long        readInt(const std::string& section, const std::string& key,
                        long fallback = 0) const;
    double      readDouble(const std::string& section, const std::string& key,
                           double fallback = 0.0) const;
    bool        readBool(const std::string& section, const std::string& key,
                         bool fallback = false) const;

    void write(const std::string& section, const std::string& key,
               const std::string& value);
    void write(const std::string& section, const std::string& key, long value);
    void write(const std::string& section, const std::string& key, double value);
    void write(const std::string& section, const std::string& key, bool value);

    void erase(const std::string& section, const std::string& key);

    /* Where PolyWorks keeps `name` (e.g. "polyworks.ini"): beside the
       executable when that directory can be written to, else the per-user
       configuration directory. */
    static std::string preferredPath(const std::string& name);

private:
    std::string m_path;
    /* section -> key -> value.  Ordered so a rewritten file keeps a stable,
       diffable layout instead of reshuffling on every save. */
    std::map<std::string, std::map<std::string, std::string>> m_data;
};

}  // namespace pw
