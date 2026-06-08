// dbi0_client.cpp - see dbi0_client.h. CLEAN-ROOM implementation of the DBI0
// flows exactly as specified in docs/PROTOCOL.md (cross-checked against the open
// `dbibackend` host script). Switch = initiator.
#include "dbi0_client.h"
#include "../proto/dbi0.h"
#include <cstring>

namespace dbi::usb {

using namespace dbi::proto;

// Read a 16-byte header from the host; validate magic.
struct HeaderView { CmdType type; CmdId id; uint32_t dataSize; };

bool Dbi0Client::readHeader(HeaderView& hv) {
    uint8_t buf[HEADER_SIZE];
    if (t_.read(buf, HEADER_SIZE) != HEADER_SIZE) return false;
    return parseHeader(buf, hv.type, hv.id, hv.dataSize);
}

static void sendHeader(BulkTransport& t, CmdType type, CmdId id, uint32_t dataSize) {
    uint8_t buf[HEADER_SIZE];
    writeHeader(buf, type, id, dataSize);
    t.write(buf, HEADER_SIZE);
}

bool Dbi0Client::listFiles(std::vector<std::string>& out) {
    out.clear();
    // Switch -> host: REQUEST LIST
    sendHeader(t_, CmdType::Request, CmdId::List, 0);
    // host -> Switch: RESPONSE LIST, data_size = N
    HeaderView resp;
    if (!readHeader(resp) || resp.id != CmdId::List) return false;
    const uint32_t n = resp.dataSize;
    if (n == 0) return true;
    // Switch -> host: ACK LIST, then read N bytes of "name1\nname2\n..."
    sendHeader(t_, CmdType::Ack, CmdId::List, n);
    std::string blob;
    blob.resize(n);
    if (t_.read(&blob[0], n) != n) return false;
    size_t start = 0;
    while (start < blob.size()) {
        size_t nl = blob.find('\n', start);
        if (nl == std::string::npos) nl = blob.size();
        if (nl > start) out.emplace_back(blob.substr(start, nl - start));
        start = nl + 1;
    }
    return true;
}

size_t Dbi0Client::fileRange(const std::string& name, uint64_t offset, void* dst, size_t len) {
    // Build the request body: range_size(u32) range_offset(u64) name_len(u32) name
    const uint32_t bodySize = 16 + (uint32_t)name.size();
    // Switch -> host: REQUEST FILE_RANGE, data_size = bodySize
    sendHeader(t_, CmdType::Request, CmdId::FileRange, bodySize);
    // host -> Switch: ACK FILE_RANGE
    HeaderView ack;
    if (!readHeader(ack) || ack.id != CmdId::FileRange) return 0;
    // Switch -> host: the request body
    std::string body;
    body.resize(bodySize);
    uint8_t* p = reinterpret_cast<uint8_t*>(&body[0]);
    const uint32_t rsz = (uint32_t)len;
    std::memcpy(p + 0, &rsz, 4);
    std::memcpy(p + 4, &offset, 8);
    const uint32_t nlen = (uint32_t)name.size();
    std::memcpy(p + 12, &nlen, 4);
    std::memcpy(p + 16, name.data(), name.size());
    t_.write(body.data(), bodySize);
    // host -> Switch: RESPONSE FILE_RANGE, data_size = range_size
    HeaderView resp;
    if (!readHeader(resp) || resp.id != CmdId::FileRange) return 0;
    const uint32_t toRead = resp.dataSize < rsz ? resp.dataSize : rsz;
    // Switch -> host: ACK, then stream the payload (host sends in 1 MiB segments)
    sendHeader(t_, CmdType::Ack, CmdId::FileRange, toRead);
    size_t got = 0;
    uint8_t* out = reinterpret_cast<uint8_t*>(dst);
    while (got < toRead) {
        size_t want = toRead - got;
        if (want > SEGMENT_SIZE) want = SEGMENT_SIZE;
        size_t r = t_.read(out + got, want);
        if (r == 0) break;
        got += r;
    }
    return got;
}

void Dbi0Client::sendExit() {
    sendHeader(t_, CmdType::Request, CmdId::Exit, 0);
    HeaderView resp;
    readHeader(resp); // host replies RESPONSE EXIT
}

} // namespace dbi::usb
