// config.cpp - see config.h. Clean-room INI parser for dbi.config.
#include "config.h"
#include <algorithm>
#include <cctype>
#include <cstdlib>

namespace dbi::config {

namespace {
std::string trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && std::isspace((unsigned char)s[a])) ++a;
    while (b > a && std::isspace((unsigned char)s[b - 1])) --b;
    return s.substr(a, b - a);
}
bool iequals(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i)
        if (std::tolower((unsigned char)a[i]) != std::tolower((unsigned char)b[i])) return false;
    return true;
}
} // namespace

const std::string* Section::find(const std::string& key) const {
    for (const auto& kv : entries)
        if (iequals(kv.first, key)) return &kv.second;
    return nullptr;
}

void Config::parse(const std::string& text) {
    sections_.clear();
    Section* cur = nullptr;
    size_t pos = 0;
    while (pos <= text.size()) {
        size_t nl = text.find('\n', pos);
        std::string line = trim(text.substr(pos, (nl == std::string::npos ? text.size() : nl) - pos));
        pos = (nl == std::string::npos) ? text.size() + 1 : nl + 1;

        if (line.empty() || line[0] == ';') continue;            // comment / disabled key
        if (line.front() == '[' && line.back() == ']') {          // [Section]
            sections_.push_back(Section{line.substr(1, line.size() - 2), {}});
            cur = &sections_.back();
            continue;
        }
        size_t eq = line.find('=');
        if (eq == std::string::npos || cur == nullptr) continue;
        cur->entries.emplace_back(trim(line.substr(0, eq)), trim(line.substr(eq + 1)));
    }
}

const Section* Config::section(const std::string& name) const {
    for (const auto& s : sections_)
        if (iequals(s.name, name)) return &s;
    return nullptr;
}

std::string Config::getString(const std::string& section, const std::string& key,
                              const std::string& def) const {
    if (const Section* s = this->section(section))
        if (const std::string* v = s->find(key)) return *v;
    return def;
}

bool Config::getBool(const std::string& section, const std::string& key, bool def) const {
    if (const Section* s = this->section(section))
        if (const std::string* v = s->find(key)) return iequals(trim(*v), "true");
    return def;
}

int Config::getInt(const std::string& section, const std::string& key, int def) const {
    if (const Section* s = this->section(section))
        if (const std::string* v = s->find(key)) {
            char* end = nullptr;
            long n = std::strtol(v->c_str(), &end, 10);
            if (end != v->c_str()) return (int)n;
        }
    return def;
}

} // namespace dbi::config
