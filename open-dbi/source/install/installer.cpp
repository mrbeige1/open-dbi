// installer.cpp - see installer.h. Clean-room install state machine.
#include "installer.h"
#include "../crypto/nca.h"
#include "../crypto/ncz.h"
#include "meta_builder.h"
#include <cctype>
#include <vector>
#include <cstring>
#include <algorithm>

namespace dbi::install {

namespace {
bool endsWith(const std::string& s, const char* suf) {
    size_t n = std::strlen(suf);
    return s.size() >= n && s.compare(s.size() - n, n, suf) == 0;
}
int hexNib(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}
} // namespace

const char* phaseName(Phase p) {
    switch (p) {
        case Phase::Parse:        return "parse";
        case Phase::WriteNca:     return "write-nca";
        case Phase::ImportTicket: return "import-ticket";
        case Phase::ContentMeta:  return "content-meta";
        case Phase::Commit:       return "commit";
    }
    return "?";
}

bool parseContentId(const std::string& name, ContentId& out) {
    // NCA files are named <32 lowercase hex>.nca / .cnmt.nca
    if (name.size() < 32) return false;
    for (int i = 0; i < 16; ++i) {
        int hi = hexNib(name[i * 2]), lo = hexNib(name[i * 2 + 1]);
        if (hi < 0 || lo < 0) return false;
        out[i] = uint8_t((hi << 4) | lo);
    }
    return true;
}

bool Installer::streamIntoPlaceHolder(io::ContentSource& src, const fs::Pfs0Entry& e,
                                      const ContentId& placeHolder) {
    constexpr size_t CHUNK = 0x100000; // 1 MiB, matching the DBI0 segment size
    std::vector<uint8_t> buf(CHUNK);
    uint64_t written = 0;
    while (written < e.size) {
        size_t want = (size_t)std::min<uint64_t>(CHUNK, e.size - written);
        size_t got = src.read(buf.data(), e.offset + written, want);
        if (got == 0) return false;
        if (!backend_.writePlaceHolder(placeHolder, written, buf.data(), got)) return false;
        written += got;
    }
    return true;
}

InstallResult Installer::installNsp(io::ContentSource& nsp, StorageId storage) {
    InstallResult r;
    // `phase` tracks where we are; on a `goto fail` it still holds the phase we were
    // in, so r.failedPhase is set correctly at the fail label (no per-site edits).
    Phase phase = Phase::Parse;
    auto note = [&](Phase p, const char* detail) {
        phase = p;
        if (obs_) obs_->onPhase(p, detail);
    };
    // Declared before any `goto fail` so the jumps don't bypass its initialization.
    std::vector<uint8_t> scratch;
    {
    fs::Pfs0 pfs;
    // --- Phase 0: parse container ---
    note(Phase::Parse, "open container");
    if (!pfs.open(nsp)) { r.error = "not a PFS0/NSP"; goto fail; }
    if (!backend_.open(storage)) { r.error = "ncm open storage failed"; goto fail; }

    // --- Phase 1: per-NCA placeholder -> write -> register (.nca copied as-is, .ncz reconstructed) ---
    for (const auto& e : pfs.entries()) {
        bool isNcz = endsWith(e.name, ".ncz");
        if (!endsWith(e.name, ".nca") && !isNcz) continue;
        note(Phase::WriteNca, e.name.c_str());
        ContentId cid;
        if (!parseContentId(e.name, cid)) { r.error = "bad nca name: " + e.name; goto fail; }
        uint64_t ncaSize = isNcz ? crypto::nczReconstructedSize(nsp, e.offset) : e.size;
        if (ncaSize == 0) { r.error = "ncz size/section parse: " + e.name; goto fail; }
        ContentId ph;
        if (!backend_.generatePlaceHolderId(ph))                  { r.error = "genPlaceHolderId"; goto fail; }
        if (!backend_.createPlaceHolder(cid, ph, ncaSize))        { r.error = "createPlaceHolder"; goto fail; }
        if (isNcz) {
            struct PhSink : crypto::NcaWriteSink {
                InstallBackend* be; const ContentId* ph;
                bool write(uint64_t off, const void* d, size_t n) override { return be->writePlaceHolder(*ph, off, d, n); }
            } sink; sink.be = &backend_; sink.ph = &ph;
            if (!crypto::nczReconstruct(nsp, e.offset, e.size, sink)) { r.error = "ncz reconstruct: " + e.name; goto fail; }
        } else {
            if (!streamIntoPlaceHolder(nsp, e, ph))               { r.error = "writePlaceHolder"; goto fail; }
        }
        if (!backend_.registerContent(cid, ph))                   { r.error = "register"; goto fail; }
        ++r.ncasWritten;
    }
    // --- Phase 2: es ticket import (.tik [+ .cert]) ---
    for (const auto& e : pfs.entries()) {
        if (!endsWith(e.name, ".tik")) continue;
        note(Phase::ImportTicket, e.name.c_str());
        scratch.resize((size_t)e.size);
        if (nsp.read(scratch.data(), e.offset, scratch.size()) != scratch.size()) { r.error = "read tik"; goto fail; }
        // cert lookup (same basename + .cert) is left to the backend / a later pass.
        if (!backend_.importTicket(scratch.data(), scratch.size(), nullptr, 0)) { r.error = "es import"; goto fail; }
        ++r.ticketsImported;
    }
    // --- Phase 3: content-meta from the CNMT NCA, then commit ---
    for (const auto& e : pfs.entries()) {
        if (!endsWith(e.name, ".cnmt.nca") && !endsWith(e.name, ".cnmt")) continue;
        note(Phase::ContentMeta, e.name.c_str());
        scratch.resize((size_t)e.size);
        if (nsp.read(scratch.data(), e.offset, scratch.size()) != scratch.size()) { r.error = "read cnmt"; goto fail; }
        if (endsWith(e.name, ".cnmt.nca")) {
            // Decrypt the Meta NCA to obtain the raw CNMT (needs keys), then build
            // the ncm meta blob (adding the Meta NCA's own content record).
            if (!keyset_) { r.error = "cnmt.nca present but no keyset (needs prod.keys)"; goto fail; }
            std::vector<uint8_t> cnmt = crypto::ncaExtractCnmt(scratch.data(), scratch.size(), *keyset_);
            if (cnmt.empty()) { r.error = "CNMT decrypt/extract failed"; goto fail; }
            ContentId metaId{};
            parseContentId(e.name, metaId);
            MetaToSet ms;
            if (!buildNcmMeta(cnmt.data(), cnmt.size(), metaId, e.size, ms)) { r.error = "buildNcmMeta"; goto fail; }
            if (!backend_.setContentMeta(ms.id, ms.version, ms.type, ms.blob.data(), ms.blob.size())) { r.error = "setContentMeta"; goto fail; }
        }
        break;
    }
    note(Phase::Commit, "commit");
    if (!backend_.commit()) { r.error = "commit"; goto fail; }

    backend_.closeStorage();
    r.ok = true;
    return r;
    } // end of pfs/loops scope
fail:
    r.failedPhase = phase;
    if (obs_) obs_->onError(phase, r.error.c_str());
    backend_.closeStorage();
    return r;
}

} // namespace dbi::install
