// ncz.h - reconstruct an original NCA from a compressed .ncz (NSZ/XCZ).
//
// CLEAN-ROOM from the public NCZ format (the `nsz` tool): the .ncz is the first
// 0x4000 of the NCA verbatim, then an "NCZSECTN" section table, then a zstd stream
// of the decrypted body. Reconstruction = copy header, zstd-decompress the body,
// and AES-CTR re-encrypt each section with the key+counter the table carries
// (so no prod.keys are needed for the content itself). Streamed, never fully in RAM.
#pragma once
#include <cstdint>
#include <cstddef>
#include "../io/content_source.h"

namespace dbi::crypto {

// Receives reconstructed NCA bytes at their NCA offset (sequential, ascending).
struct NcaWriteSink {
    virtual ~NcaWriteSink() = default;
    virtual bool write(uint64_t ncaOffset, const void* data, size_t len) = 0;
};

// Reconstructed NCA size from the .ncz section table (read at nczStart). 0 on error.
uint64_t nczReconstructedSize(io::ContentSource& src, uint64_t nczStart);

// Reconstruct the NCA from the .ncz at [nczStart, nczStart+nczLen) into `sink`.
bool nczReconstruct(io::ContentSource& src, uint64_t nczStart, uint64_t nczLen, NcaWriteSink& sink);

} // namespace dbi::crypto
