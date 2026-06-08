// nca.cpp - see nca.h. Clean-room NCA decrypt from switchbrew + validated AES.
#include "nca.h"
#include "aes.h"
#include <cstring>

namespace dbi::crypto {

// --- header field offsets (switchbrew NCA header) ---
namespace off {
constexpr size_t MAGIC=0x200, CONTENT_TYPE=0x205, KEYGEN_OLD=0x206, KAEK_INDEX=0x207,
                 CONTENT_SIZE=0x208, TITLE_ID=0x210, KEYGEN=0x220, RIGHTS_ID=0x230,
                 FS_ENTRIES=0x240, KEY_AREA=0x300, FS_HEADERS=0x400;
}
constexpr size_t HEADER_LEN = 0xC00;

static uint64_t rd64(const uint8_t* p){ uint64_t v=0; for(int i=0;i<8;i++) v|=(uint64_t)p[i]<<(8*i); return v; }
static uint32_t rd32(const uint8_t* p){ return p[0]|(p[1]<<8)|(p[2]<<16)|((uint32_t)p[3]<<24); }

bool ncaParseHeader(const uint8_t* enc, size_t encLen, const Keyset& ks, NcaInfo& out) {
    if (encLen < HEADER_LEN || !ks.has_header_key) return false;
    uint8_t hdr[HEADER_LEN];
    const uint8_t* k1 = ks.header_key;          // first 16 = data key
    const uint8_t* k2 = ks.header_key + 16;     // last 16 = tweak key
    for (int s = 0; s < (int)(HEADER_LEN/0x200); ++s)        // 6 sectors of 0x200
        aesXtsDecryptSector(k1, k2, (uint64_t)s, enc + s*0x200, hdr + s*0x200, 0x200);

    if (std::memcmp(hdr+off::MAGIC, "NCA3", 4) != 0) return false;  // (NCA2/0 not handled yet)
    out.contentType = (NcaContentType)hdr[off::CONTENT_TYPE];
    out.kaekIndex   = hdr[off::KAEK_INDEX];
    out.contentSize = rd64(hdr+off::CONTENT_SIZE);
    out.titleId     = rd64(hdr+off::TITLE_ID);
    uint8_t gOld = hdr[off::KEYGEN_OLD], gNew = hdr[off::KEYGEN];
    uint8_t g = gOld > gNew ? gOld : gNew;
    out.keygen = (g > 0) ? (uint8_t)(g - 1) : 0;
    std::memcpy(out.rightsId, hdr+off::RIGHTS_ID, 16);
    for (int i=0;i<16;i++) if (out.rightsId[i]) { out.hasRightsId = true; break; }
    std::memcpy(out.sections, hdr+off::FS_ENTRIES, sizeof(out.sections));
    std::memcpy(out.fsHeader, hdr+off::FS_HEADERS, 4*0x200);

    // Select key-area-encryption-key and ECB-decrypt the 4 key-area entries.
    const uint8_t (*kak)[16] = ks.kak_application;
    if (out.kaekIndex==1) kak = ks.kak_ocean; else if (out.kaekIndex==2) kak = ks.kak_system;
    if (out.keygen >= MAX_GEN) return false;
    Aes128 ke; ke.setKey(kak[out.keygen]);
    for (int i=0;i<4;i++) ke.decryptBlock(hdr+off::KEY_AREA+i*16, out.decKeyArea[i]);
    out.valid = true;
    return true;
}

bool ncaDecryptSection(const NcaInfo& info, int sectionIdx, uint64_t absOffset,
                       const uint8_t* in, uint8_t* out, size_t len) {
    if (sectionIdx<0 || sectionIdx>3 || !info.valid) return false;
    // FS header encryption_type @ +4: 1=None, 3=Ctr.
    uint8_t encType = info.fsHeader[sectionIdx][4];
    if (encType == 1) { std::memmove(out, in, len); return true; }      // unencrypted
    if (encType != 3) return false;                                      // XTS/BKTR not handled yet
    // Section CTR: high 8 bytes from FS header secure value @ +0x140 (big-endian
    // as stored), low 8 bytes = (absOffset >> 4) big-endian.  UNVERIFIED.
    uint8_t ctr[16];
    const uint8_t* sv = info.fsHeader[sectionIdx] + 0x140;
    for (int i=0;i<8;i++) ctr[i] = sv[7-i];
    uint64_t blk = absOffset >> 4;
    for (int i=0;i<8;i++) ctr[15-i] = (uint8_t)(blk >> (8*i));
    aesCtr(info.decKeyArea[2], ctr, in, out, len);
    return true;
}

// Minimal PFS0 walk over a decrypted in-memory buffer to find "*.cnmt".
std::vector<uint8_t> ncaExtractCnmt(const uint8_t* nca, size_t ncaLen, const Keyset& ks) {
    std::vector<uint8_t> empty;
    NcaInfo info;
    if (!ncaParseHeader(nca, ncaLen, ks, info)) return empty;
    if (info.contentType != NcaContentType::Meta) return empty;
    // Meta NCA: the CNMT lives in section 0 (a PFS0). Section data starts at
    // sections[0].start_block * 0x200.  UNVERIFIED against a real NCA.
    uint64_t secOff = (uint64_t)info.sections[0].start_block * 0x200;
    uint64_t secEnd = (uint64_t)info.sections[0].end_block   * 0x200;
    if (secOff >= ncaLen || secEnd <= secOff || secEnd > ncaLen) return empty;
    size_t secLen = (size_t)(secEnd - secOff);
    std::vector<uint8_t> sec(secLen);
    if (!ncaDecryptSection(info, 0, secOff, nca + secOff, sec.data(), secLen)) return empty;
    // The PFS0 may sit at a hashed offset inside the section; for a simple metadata
    // PFS0 it is typically at the start. Scan for the "PFS0" magic to be safe.
    size_t p = 0; bool found=false;
    for (size_t i=0;i+4<=secLen;i+=0x10) if (std::memcmp(sec.data()+i,"PFS0",4)==0){p=i;found=true;break;}
    if (!found) return empty;
    const uint8_t* h = sec.data()+p;
    uint32_t count = rd32(h+4), strSize = rd32(h+8);
    size_t entriesOff = p+0x10, strOff = entriesOff + (size_t)count*0x18, dataOff = strOff + strSize;
    for (uint32_t i=0;i<count;i++){
        const uint8_t* e = sec.data()+entriesOff+i*0x18;
        uint64_t fo = rd64(e), fsz = rd64(e+8); uint32_t no = rd32(e+16);
        const char* nm = (const char*)(sec.data()+strOff+no);
        if (std::strstr(nm, ".cnmt")) {
            std::vector<uint8_t> cnmt(fsz);
            if (dataOff+fo+fsz <= secLen) std::memcpy(cnmt.data(), sec.data()+dataOff+fo, fsz);
            return cnmt;
        }
    }
    return empty;
}

} // namespace dbi::crypto
