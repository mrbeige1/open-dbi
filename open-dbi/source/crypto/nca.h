// nca.h - NCA (Nintendo Content Archive) header + section decryption.
//
// CLEAN-ROOM from the public NCA format (switchbrew) + our validated AES (aes.h):
//   header (0xC00) = AES-128-XTS with header_key, 0x200 sectors, big-endian tweak
//   key area (0x40 @ 0x300) = AES-128-ECB with key_area_key[kaek_index][keygen]
//   sections = AES-128-CTR with decrypted key_area[2], IV = ctr_high || (off>>4)
//
// STATUS: header field offsets are static_assert-checked; the full decrypt path is
// implemented to spec but NOT yet validated against a real NCA (needs prod.keys +
// a Meta NCA on hardware). Marked UNVERIFIED where end-to-end testing is required.
#pragma once
#include <cstdint>
#include <cstddef>
#include <vector>
#include "keyset.h"

namespace dbi::crypto {

enum class NcaContentType : uint8_t { Program=0, Meta=1, Control=2, Manual=3, Data=4, PublicData=5 };

#pragma pack(push,1)
struct NcaFsEntry { uint32_t start_block; uint32_t end_block; uint8_t _pad[8]; };  // 0x10, units of 0x200
static_assert(sizeof(NcaFsEntry)==0x10, "NcaFsEntry");
#pragma pack(pop)

// Decrypted view of the parts of an NCA header we need.
struct NcaInfo {
    bool            valid = false;
    NcaContentType  contentType = NcaContentType::Program;
    uint8_t         keygen = 0;          // effective master-key generation index
    uint8_t         kaekIndex = 0;       // 0=application,1=ocean,2=system
    uint64_t        contentSize = 0;
    uint64_t        titleId = 0;
    uint8_t         rightsId[16] = {};
    bool            hasRightsId = false; // titlekey-crypto vs standard-crypto
    NcaFsEntry      sections[4] = {};
    uint8_t         decKeyArea[4][16] = {};  // ECB-decrypted key area
    // raw decrypted FS headers (0x200 each) for the 4 sections
    uint8_t         fsHeader[4][0x200] = {};
};

// Decrypt the 0xC00 header in `enc` (>=0xC00 bytes) with the keyset; fill `out`.
// Returns false if the magic is not NCA3 or required keys are missing.
bool ncaParseHeader(const uint8_t* enc, size_t encLen, const Keyset& ks, NcaInfo& out);

// CTR-decrypt `len` bytes of section data read at absolute NCA offset `absOffset`
// for section `sectionIdx`. `in`/`out` may alias. `absOffset` must be 16-aligned.
// UNVERIFIED against a real NCA.
bool ncaDecryptSection(const NcaInfo& info, int sectionIdx, uint64_t absOffset,
                       const uint8_t* in, uint8_t* out, size_t len);

// Source big enough to random-read an NCA (e.g. our io::ContentSource wrapper is
// adapted by the caller). Extract the raw CNMT bytes from a Meta NCA's PFS0 section.
// Returns the .cnmt contents, or empty on failure. UNVERIFIED end-to-end.
std::vector<uint8_t> ncaExtractCnmt(const uint8_t* nca, size_t ncaLen, const Keyset& ks);

} // namespace dbi::crypto
