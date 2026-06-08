// pfs0.h - minimal PFS0 (NSP partition) reader over a ContentSource.
//
// CLEAN-ROOM from the public PFS0 layout (docs/FORMATS.md + switchbrew): header
// "PFS0", file count, string-table size, then count entries {dataOffset(u64),
// size(u64), nameOffset(u32), pad}, then the string table, then the data region.
#pragma once
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include "../io/content_source.h"

namespace dbi::fs {

struct Pfs0Entry {
    std::string name;
    uint64_t    offset; // absolute offset within the source
    uint64_t    size;
};

class Pfs0 {
public:
    // Parse the PFS0 directory from `src`. Returns false if magic/sizes are invalid.
    bool open(io::ContentSource& src);
    const std::vector<Pfs0Entry>& entries() const { return entries_; }

private:
    std::vector<Pfs0Entry> entries_;
};

inline bool Pfs0::open(io::ContentSource& src) {
    entries_.clear();
    uint8_t hdr[16];
    if (src.read(hdr, 0, 16) != 16) return false;
    if (std::memcmp(hdr, "PFS0", 4) != 0) return false;
    auto u32 = [](const uint8_t* p) {
        return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
    };
    const uint32_t count   = u32(hdr + 4);
    const uint32_t strSize = u32(hdr + 8);
    const uint64_t entriesOff = 16;
    const uint64_t strTableOff = entriesOff + uint64_t(count) * 0x18;
    const uint64_t dataOff = strTableOff + strSize;

    std::vector<uint8_t> ents(uint64_t(count) * 0x18);
    if (count && src.read(ents.data(), entriesOff, ents.size()) != ents.size()) return false;
    std::vector<uint8_t> strs(strSize);
    if (strSize && src.read(strs.data(), strTableOff, strSize) != strSize) return false;

    auto u64 = [](const uint8_t* p) {
        uint64_t v = 0; for (int i = 7; i >= 0; --i) v = (v << 8) | p[i]; return v;
    };
    for (uint32_t i = 0; i < count; ++i) {
        const uint8_t* e = ents.data() + i * 0x18;
        Pfs0Entry pe;
        pe.offset = dataOff + u64(e + 0);
        pe.size   = u64(e + 8);
        uint32_t nameOff = u32(e + 16);
        const char* nm = (nameOff < strSize) ? reinterpret_cast<const char*>(strs.data() + nameOff) : "";
        pe.name = std::string(nm);
        entries_.push_back(std::move(pe));
    }
    return true;
}

} // namespace dbi::fs
