// dbi0.h - The DBI0 USB wire protocol.
//
// CLEAN-ROOM: this header is written solely from the observable protocol spec in
// docs/PROTOCOL.md (itself derived from the open `dbibackend` host script), NOT from
// any decompiled DBI code. It describes the bytes on the wire, nothing more.
#pragma once
#include <cstdint>
#include <cstddef>

namespace dbi::proto {

// Every message begins with this 16-byte little-endian header.
//   magic    : 4 bytes, always "DBI0"
//   cmd_type : u32  (REQUEST=0, RESPONSE=1, ACK=2)
//   cmd_id   : u32  (EXIT=0, LIST_OLD=1[unused], FILE_RANGE=2, LIST=3)
//   data_size: u32  (command-dependent)
inline constexpr char     MAGIC[4]      = {'D', 'B', 'I', '0'};
inline constexpr uint32_t MAGIC_LE      = 0x30494244u; // "DBI0" as LE u32
inline constexpr size_t   HEADER_SIZE   = 16;
inline constexpr size_t   SEGMENT_SIZE  = 0x100000;    // 1 MiB transfer chunk

enum class CmdType : uint32_t { Request = 0, Response = 1, Ack = 2 };
enum class CmdId   : uint32_t { Exit = 0, ListOld = 1, FileRange = 2, List = 3 };

#pragma pack(push, 1)
struct Header {
    char     magic[4];
    uint32_t cmd_type;   // CmdType
    uint32_t cmd_id;     // CmdId
    uint32_t data_size;
};
static_assert(sizeof(Header) == HEADER_SIZE, "DBI0 header must be 16 bytes");

// FILE_RANGE request body (data_size bytes), little-endian:
//   range_size  : u32  bytes requested
//   range_offset: u64  offset into the named file
//   name_len    : u32  length of the UTF-8 filename that follows
//   name        : char[name_len]
struct FileRangeHeader {
    uint32_t range_size;
    uint64_t range_offset;
    uint32_t name_len;
    // followed by name_len bytes of UTF-8 filename
};
static_assert(sizeof(FileRangeHeader) == 16, "FileRange fixed part is 16 bytes");
#pragma pack(pop)

// Build a header into a 16-byte buffer (little-endian).
inline void writeHeader(uint8_t out[HEADER_SIZE], CmdType type, CmdId id, uint32_t dataSize) {
    out[0] = 'D'; out[1] = 'B'; out[2] = 'I'; out[3] = '0';
    const auto put32 = [](uint8_t* p, uint32_t v) {
        p[0] = uint8_t(v); p[1] = uint8_t(v >> 8); p[2] = uint8_t(v >> 16); p[3] = uint8_t(v >> 24);
    };
    put32(out + 4,  static_cast<uint32_t>(type));
    put32(out + 8,  static_cast<uint32_t>(id));
    put32(out + 12, dataSize);
}

// Parse a 16-byte header; returns false if the magic is not "DBI0".
inline bool parseHeader(const uint8_t in[HEADER_SIZE], CmdType& type, CmdId& id, uint32_t& dataSize) {
    if (in[0] != 'D' || in[1] != 'B' || in[2] != 'I' || in[3] != '0') return false;
    const auto get32 = [](const uint8_t* p) -> uint32_t {
        return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
    };
    type     = static_cast<CmdType>(get32(in + 4));
    id       = static_cast<CmdId>(get32(in + 8));
    dataSize = get32(in + 12);
    return true;
}

} // namespace dbi::proto
