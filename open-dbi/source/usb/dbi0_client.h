// dbi0_client.h - Switch-side DBI0 protocol client (the counterpart to the open
// `dbibackend` PC host). CLEAN-ROOM from docs/PROTOCOL.md: the Switch is the
// initiator; it sends REQUEST headers and the host responds.
#pragma once
#include <string>
#include <vector>
#include "transport.h"
#include "../io/content_source.h"

namespace dbi::usb {

class Dbi0Client {
public:
    explicit Dbi0Client(BulkTransport& t) : t_(t) {}

    // LIST: returns the host's available filenames (newline-joined list, split).
    bool listFiles(std::vector<std::string>& out);

    // FILE_RANGE: read `len` bytes of `name` at `offset` into `dst`.
    // Returns bytes received (streamed in 1 MiB segments like the host).
    size_t fileRange(const std::string& name, uint64_t offset, void* dst, size_t len);

    // EXIT: tell the host to stop serving.
    void sendExit();

private:
    BulkTransport& t_;
    bool readHeader(class HeaderView& hv);
};

// A ContentSource backed by one host file over FILE_RANGE. `size()` must be supplied
// by the caller (the DBI0 protocol is pure range-read; total size is learned from the
// container header, not the wire protocol).
class Dbi0FileSource : public io::ContentSource {
public:
    Dbi0FileSource(Dbi0Client& c, std::string name, uint64_t knownSize)
        : c_(c), name_(std::move(name)), size_(knownSize) {}
    uint64_t size() const override { return size_; }
    size_t read(void* dst, uint64_t offset, size_t len) override {
        return c_.fileRange(name_, offset, dst, len);
    }
private:
    Dbi0Client& c_;
    std::string name_;
    uint64_t    size_;
};

} // namespace dbi::usb
