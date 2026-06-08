// transport.h - abstract bulk byte transport (so the DBI0 protocol logic is
// testable without real USB hardware). A libnx usb:ds-backed implementation can be
// dropped in later; tests use an in-memory loopback.
#pragma once
#include <cstdint>
#include <cstddef>

namespace dbi::usb {

class BulkTransport {
public:
    virtual ~BulkTransport() = default;
    // Write all `len` bytes toward the host (bulk OUT). Returns bytes written.
    virtual size_t write(const void* src, size_t len) = 0;
    // Read exactly up to `len` bytes from the host (bulk IN). Returns bytes read.
    virtual size_t read(void* dst, size_t len) = 0;
};

} // namespace dbi::usb
