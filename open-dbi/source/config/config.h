// config.h - Parser for dbi.config (the on-SD INI configuration).
//
// CLEAN-ROOM: written from the observable file format documented in docs/CONFIG.md
// (section headers, key=value, ';' line comments, commented-out key => use default,
// ordered key lists for the storage/source sections). Not derived from decompiled code.
#pragma once
#include <string>
#include <vector>
#include <utility>

namespace dbi::config {

// One [Section]; preserves key insertion order (needed for the list-style sections
// like [Network sources] / [MTP Storages]).
struct Section {
    std::string name;
    std::vector<std::pair<std::string, std::string>> entries;

    const std::string* find(const std::string& key) const;
};

class Config {
public:
    // Parse INI text. Lines beginning with ';' (after trim) are comments/disabled keys.
    void parse(const std::string& text);

    std::string getString(const std::string& section, const std::string& key,
                          const std::string& def = "") const;
    bool        getBool(const std::string& section, const std::string& key, bool def = false) const;
    int         getInt(const std::string& section, const std::string& key, int def = 0) const;

    // Ordered entries of a list-style section (e.g. "Network sources"); empty if absent.
    const Section* section(const std::string& name) const;

private:
    std::vector<Section> sections_;
};

} // namespace dbi::config
