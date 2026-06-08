// save_backup.h - DBI save-backup container format.
//
// CLEAN-ROOM: the layout below is observable from any DBI save-backup ZIP (filename
// convention + the `.dbi_save_info.ini` metadata entry + the 0x200-byte
// `.dbi_save_extra` blob). Documented in docs/SYMBOLS.md / FORMATS.md. This header
// describes/produces that format; it does not contain decompiled code.
#pragma once
#include <cstdint>
#include <string>

namespace dbi::saves {

// Control entries stored inside each backup ZIP.
inline constexpr const char* INFO_ENTRY  = ".dbi_save_info.ini";
inline constexpr const char* EXTRA_ENTRY = ".dbi_save_extra";   // opaque 0x200-byte record
inline constexpr size_t       EXTRA_SIZE = 0x200;

// Metadata written to .dbi_save_info.ini ([General] section).
struct SaveInfo {
    uint64_t    titleId   = 0;     // formatted as 16 uppercase hex digits
    std::string titleName;
    std::string backupDate;        // human-formatted timestamp
    std::string account;           // user/account identity
    uint8_t     space     = 0;     // storage location enum
};

// Render the .dbi_save_info.ini body. Mirrors the observed key set/order.
std::string buildInfoIni(const SaveInfo& info);

// Backup file name: "<TID16hex>_<space>_<timestamp>_<index>.zip"
std::string backupFileName(uint64_t titleId, uint8_t space,
                          const std::string& timestamp, int index);

} // namespace dbi::saves
