// content_source.h - the install "source" seam.
//
// CLEAN-ROOM: DBI's installer reads content through a single vtable File/InputStream
// interface (getSize / read / close); the transport (USB/MTP/file/gamecard/network)
// is hidden behind it (see docs/ARCHITECTURE.md, install section). We mirror that
// design here as an abstract base, written from the behavioral description — not from
// decompiled code.
#pragma once
#include <cstdint>
#include <cstddef>

namespace dbi::io {

// A random-access, read-only byte source for one logical file/content blob.
class ContentSource {
public:
    virtual ~ContentSource() = default;

    // Total size in bytes (the recovered interface's "getSize" slot).
    virtual uint64_t size() const = 0;

    // Read up to `len` bytes at absolute `offset` into `dst`.
    // Returns the number of bytes actually read (0 on EOF/short source).
    virtual size_t read(void* dst, uint64_t offset, size_t len) = 0;

    // Release any underlying resources (the "close"/commit slot).
    virtual void close() {}
};

} // namespace dbi::io
