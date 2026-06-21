// sha256.h - SHA-256 (FIPS 180-4) for NCA content verification (CheckHash).
//
// CLEAN-ROOM from FIPS 180-4. Streaming interface so multi-GB NCAs can be hashed
// in fixed-size chunks without buffering the whole file. An NCA's "content id" is
// the first 16 bytes of the SHA-256 of the NCA, and the CNMT records the full 32;
// CheckHash recomputes the digest and compares against those references.
#pragma once
#include <cstdint>
#include <cstddef>

namespace dbi::crypto {

class Sha256 {
public:
    Sha256() { reset(); }
    void reset();
    void update(const void* data, size_t len);   // feed any number of bytes
    void finish(uint8_t out[32]);                 // finalize -> out; state is reset
private:
    void processBlock(const uint8_t b[64]);
    uint32_t h_[8];
    uint64_t bits_;      // total message length in bits
    uint8_t  buf_[64];   // partial-block buffer
    size_t   buflen_;    // bytes currently in buf_
};

// One-shot convenience (small buffers only - use the streaming class for files).
void sha256(const void* data, size_t len, uint8_t out[32]);

} // namespace dbi::crypto
