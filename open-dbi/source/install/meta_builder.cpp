// meta_builder.cpp - see meta_builder.h.
#include "meta_builder.h"
#include <cstring>

namespace dbi::install {

namespace {
inline uint16_t rd16(const uint8_t* p){ return (uint16_t)(p[0] | (p[1]<<8)); }
inline uint32_t rd32(const uint8_t* p){ return p[0]|(p[1]<<8)|(p[2]<<16)|((uint32_t)p[3]<<24); }
inline uint64_t rd64(const uint8_t* p){ uint64_t v=0; for(int i=0;i<8;i++) v|=(uint64_t)p[i]<<(8*i); return v; }
void put16(std::vector<uint8_t>& v, uint16_t x){ v.push_back((uint8_t)x); v.push_back((uint8_t)(x>>8)); }
} // namespace

bool buildNcmMeta(const uint8_t* c, size_t len, const ContentId& metaNcaId, uint64_t metaNcaSize, MetaToSet& out) {
    if (len < 0x20) return false;
    // --- PackagedContentMetaHeader (0x20) ---
    out.id   = rd64(c + 0x00);
    out.version = rd32(c + 0x08);
    out.type = c[0x0C];
    uint16_t extSize = rd16(c + 0x0E);
    uint16_t count   = rd16(c + 0x10);
    uint16_t metaCnt = rd16(c + 0x12);
    uint8_t  attrs   = c[0x14];

    size_t extOff   = 0x20;
    size_t ciOff    = extOff + extSize;                 // PackagedContentInfo[count], 0x38 each
    size_t cmiOff   = ciOff + (size_t)count * 0x38;     // ContentMetaInfo[metaCnt], 0x10 each
    if (cmiOff + (size_t)metaCnt * 0x10 > len) return false;

    std::vector<uint8_t>& b = out.blob;
    b.clear();
    // --- NcmContentMetaHeader (0x08): content_count = count + 1 (Meta NCA) ---
    put16(b, extSize);
    put16(b, (uint16_t)(count + 1));
    put16(b, metaCnt);
    b.push_back(attrs);
    b.push_back(0);                                     // storage_id (None)
    // --- extended header (copied verbatim) ---
    b.insert(b.end(), c + extOff, c + extOff + extSize);
    // --- NcmContentInfo for the Meta NCA itself (0x18) ---
    b.insert(b.end(), metaNcaId.begin(), metaNcaId.end());        // content_id (0x10)
    uint32_t lo = (uint32_t)(metaNcaSize & 0xFFFFFFFFu);
    uint8_t  hi = (uint8_t)((metaNcaSize >> 32) & 0xFF);
    b.push_back((uint8_t)lo); b.push_back((uint8_t)(lo>>8)); b.push_back((uint8_t)(lo>>16)); b.push_back((uint8_t)(lo>>24));
    b.push_back(hi);                                              // size_high
    b.push_back(0);                                              // attr
    b.push_back(0);                                              // content_type = Meta
    b.push_back(0);                                              // id_offset
    // --- the CNMT's content infos (strip the 0x20 hash from each PackagedContentInfo) ---
    for (uint16_t i = 0; i < count; ++i) {
        const uint8_t* info = c + ciOff + (size_t)i * 0x38 + 0x20;   // NcmContentInfo (0x18)
        b.insert(b.end(), info, info + 0x18);
    }
    // --- content meta infos (copied verbatim) ---
    b.insert(b.end(), c + cmiOff, c + cmiOff + (size_t)metaCnt * 0x10);
    return true;
}

} // namespace dbi::install
