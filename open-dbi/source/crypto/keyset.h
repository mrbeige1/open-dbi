// keyset.h - parse a standard Switch `prod.keys` file (the hactool format).
//
// CLEAN-ROOM from the public key-file format: "name = hexvalue" lines, with the
// generation-indexed names DBI itself uses (key_area_key_application_{:02x},
// key_area_key_ocean_{:02x}, key_area_key_system_{:02x}, titlekek_{:02x},
// header_key, master_key_{:02x}). Confirmed against DBI's plaintext format strings.
#pragma once
#include <cstdint>
#include <string>

namespace dbi::crypto {

inline constexpr int MAX_GEN = 32;

struct Keyset {
    bool    has_header_key = false;
    uint8_t header_key[32] = {};                      // two AES-128 keys (XTS)
    uint8_t kak_application[MAX_GEN][16] = {};
    uint8_t kak_ocean[MAX_GEN][16] = {};
    uint8_t kak_system[MAX_GEN][16] = {};
    uint8_t titlekek[MAX_GEN][16] = {};
    uint8_t master_key[MAX_GEN][16] = {};

    // Parse keys from text (the file contents). Returns number of keys recognized.
    int parse(const std::string& text);
    // Convenience: load from a path (e.g. "sdmc:/switch/prod.keys").
    int load(const char* path);
};

} // namespace dbi::crypto
