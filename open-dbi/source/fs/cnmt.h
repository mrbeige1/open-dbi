// cnmt.h - parse a PackagedContentMeta (CNMT) blob.
//
// CLEAN-ROOM from the public CNMT layout (switchbrew): a 0x20-byte header
// (title_id, version, meta_type, ext_header_size, content_count, ...), an extended
// header, then content_count PackagedContentInfo records (0x38 = SHA256 + 0x18 info).
#pragma once
#include <cstdint>
#include <cstring>
#include <vector>

namespace dbi::fs {

enum class ContentMetaType : uint8_t {
    Application=0x80, Patch=0x81, AddOnContent=0x82, Delta=0x83
};

struct CnmtContent {
    uint8_t  hash[32]; // SHA-256 of the content NCA (CheckHash reference)
    uint8_t  id[16];   // = first 16 bytes of `hash`; also the NCA filename
    uint64_t size;     // 6-byte field, zero-extended
    uint8_t  type;     // 0=Meta,1=Program,2=Data,3=Control,4=HtmlDoc,5=LegalInfo,6=DeltaFragment
};

struct Cnmt {
    uint64_t titleId = 0;
    uint32_t version = 0;
    uint8_t  metaType = 0;
    std::vector<CnmtContent> contents;

    bool parse(const uint8_t* d, size_t len) {
        if (len < 0x20) return false;
        auto rd64=[&](size_t o){ uint64_t v=0; for(int i=0;i<8;i++) v|=(uint64_t)d[o+i]<<(8*i); return v; };
        auto rd32=[&](size_t o){ return (uint32_t)(d[o]|(d[o+1]<<8)|(d[o+2]<<16)|((uint32_t)d[o+3]<<24)); };
        auto rd16=[&](size_t o){ return (uint16_t)(d[o]|(d[o+1]<<8)); };
        titleId  = rd64(0x00);
        version  = rd32(0x08);
        metaType = d[0x0C];
        uint16_t extSize = rd16(0x0E);
        uint16_t count   = rd16(0x10);
        size_t pos = 0x20 + extSize;
        for (uint16_t i=0;i<count;i++){
            size_t rec = pos + (size_t)i*0x38;
            if (rec + 0x38 > len) break;
            const uint8_t* info = d + rec + 0x20;   // record = 0x20 SHA-256 hash + 0x18 info
            CnmtContent c{};
            std::memcpy(c.hash, d + rec, 32);        // the content NCA's SHA-256
            std::memcpy(c.id, info, 16);
            uint64_t sz=0; for(int b=0;b<6;b++) sz|=(uint64_t)info[16+b]<<(8*b);
            c.size = sz;
            c.type = info[22];
            contents.push_back(c);
        }
        return true;
    }
};

} // namespace dbi::fs
