// ncz.cpp - see ncz.h. Streaming NCZ -> NCA reconstruction.
#include "ncz.h"
#include "aes.h"
#include <zstd.h>
#include <cstring>
#include <vector>

namespace dbi::crypto {
namespace {

constexpr uint64_t HEADER = 0x4000;        // verbatim NCA header size in an .ncz
constexpr size_t   SEC_SZ = 0x40;          // NczSection on-disk size

struct Section { uint64_t offset, size, cryptoType; uint8_t key[16], ctr[16]; };

uint64_t rd64(const uint8_t* p){ uint64_t v=0; for(int i=0;i<8;i++) v|=(uint64_t)p[i]<<(8*i); return v; }

bool readSections(io::ContentSource& src, uint64_t nczStart,
                  std::vector<Section>& out, uint64_t& streamStart) {
    uint8_t hdr[16];
    if (src.read(hdr, nczStart + HEADER, 16) != 16) return false;
    if (std::memcmp(hdr, "NCZSECTN", 8) != 0) return false;
    uint64_t count = rd64(hdr + 8);
    if (count == 0 || count > 64) return false;
    uint64_t pos = nczStart + HEADER + 16;
    for (uint64_t i = 0; i < count; ++i) {
        uint8_t s[SEC_SZ];
        if (src.read(s, pos, SEC_SZ) != SEC_SZ) return false;
        Section sec;
        sec.offset = rd64(s); sec.size = rd64(s + 8); sec.cryptoType = rd64(s + 0x10);
        std::memcpy(sec.key, s + 0x20, 16);
        std::memcpy(sec.ctr, s + 0x30, 16);
        out.push_back(sec);
        pos += SEC_SZ;
    }
    streamStart = pos;
    return true;
}

// Encrypt/copy a 16-aligned run at NCA offset `off` (which lies within one section).
void applySection(const Section& sec, uint64_t off, const uint8_t* in, uint8_t* o, size_t len) {
    if (sec.cryptoType != 3) { std::memcpy(o, in, len); return; }   // 0/1 = plaintext
    uint8_t ctr[16]; std::memcpy(ctr, sec.ctr, 16);
    uint64_t blk = off >> 4;                                        // counter low 8 = off/16
    for (int i = 0; i < 8; ++i) ctr[15 - i] = (uint8_t)(blk >> (8 * i));
    aesCtr(sec.key, ctr, in, o, len);
}

} // namespace

uint64_t nczReconstructedSize(io::ContentSource& src, uint64_t nczStart) {
    std::vector<Section> secs; uint64_t ss;
    if (!readSections(src, nczStart, secs, ss)) return 0;
    uint64_t end = 0;
    for (auto& s : secs) if (s.offset + s.size > end) end = s.offset + s.size;
    return end;
}

bool nczReconstruct(io::ContentSource& src, uint64_t nczStart, uint64_t nczLen, NcaWriteSink& sink) {
    std::vector<Section> secs; uint64_t streamStart;
    if (!readSections(src, nczStart, secs, streamStart)) return false;

    // 1) verbatim NCA header (0x4000)
    {
        std::vector<uint8_t> hdr(HEADER);
        if (src.read(hdr.data(), nczStart, HEADER) != HEADER) return false;
        if (!sink.write(0, hdr.data(), HEADER)) return false;
    }

    // 2) zstd-stream the body, re-encrypting per section as it lands.
    ZSTD_DStream* z = ZSTD_createDStream();
    if (!z) return false;
    ZSTD_initDStream(z);

    const size_t IN = 1 << 20, OUT = 1 << 20;          // 1 MiB buffers (16-aligned)
    std::vector<uint8_t> inBuf(IN), outBuf(OUT), enc(OUT + 16), acc;
    uint64_t inPos = streamStart;
    uint64_t inEnd = nczStart + nczLen;
    uint64_t ncaOff = secs.front().offset;             // first body offset (== 0x4000)
    size_t   secIdx = 0;
    bool ok = true;

    auto flush = [&](bool final) -> bool {
        size_t n = acc.size();
        if (!final) n &= ~(size_t)0xF;                 // keep CTR 16-aligned across chunks
        size_t done = 0;
        while (done < n) {
            // advance to the section containing ncaOff
            while (secIdx < secs.size() && ncaOff >= secs[secIdx].offset + secs[secIdx].size) ++secIdx;
            if (secIdx >= secs.size()) return false;
            const Section& s = secs[secIdx];
            uint64_t secEnd = s.offset + s.size;
            size_t chunk = n - done;
            if (ncaOff + chunk > secEnd) chunk = (size_t)(secEnd - ncaOff);  // stop at section edge
            applySection(s, ncaOff, acc.data() + done, enc.data(), chunk);
            if (!sink.write(ncaOff, enc.data(), chunk)) return false;
            ncaOff += chunk; done += chunk;
        }
        acc.erase(acc.begin(), acc.begin() + done);
        return true;
    };

    while (ok) {
        // feed input
        size_t want = (inPos < inEnd) ? (size_t)((inEnd - inPos < IN) ? (inEnd - inPos) : IN) : 0;
        size_t got = want ? src.read(inBuf.data(), inPos, want) : 0;
        if (got) inPos += got;
        ZSTD_inBuffer zin{ inBuf.data(), got, 0 };
        do {
            ZSTD_outBuffer zout{ outBuf.data(), OUT, 0 };
            size_t r = ZSTD_decompressStream(z, &zout, &zin);
            if (ZSTD_isError(r)) { ok = false; break; }
            acc.insert(acc.end(), outBuf.data(), outBuf.data() + zout.pos);
            if (!flush(false)) { ok = false; break; }
            if (r == 0 && zin.pos == zin.size) { /* frame end */ }
        } while (zin.pos < zin.size);
        if (got == 0) break;                            // input exhausted
    }
    if (ok) ok = flush(true);                            // final partial block
    ZSTD_freeDStream(z);
    return ok;
}

} // namespace dbi::crypto
