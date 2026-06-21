// dry_run.cpp - see dry_run.h.
#include "dry_run.h"
#include "../log/log.h"
#include "../fs/pfs0.h"
#include "../fs/cnmt.h"
#include "../crypto/nca.h"
#include "../crypto/ncz.h"
#include "../crypto/sha256.h"
#include "../install/installer.h"   // parseContentId
#include <cstring>
#include <vector>
#include <string>

namespace dbi::install {

namespace {

bool endsWith(const std::string& s, const char* suf) {
    size_t n = std::strlen(suf);
    return s.size() >= n && s.compare(s.size() - n, n, suf) == 0;
}

const char* ncaContentTypeName(crypto::NcaContentType t) {
    switch (t) {
        case crypto::NcaContentType::Program:    return "Program";
        case crypto::NcaContentType::Meta:       return "Meta";
        case crypto::NcaContentType::Control:    return "Control";
        case crypto::NcaContentType::Manual:     return "Manual";
        case crypto::NcaContentType::Data:       return "Data";
        case crypto::NcaContentType::PublicData: return "PublicData";
    }
    return "?";
}

// CNMT meta_type -> base/update/DLC (the install-target classification testers care
// about; mirrors the type tags in docs/ARCHITECTURE.md resolveContent).
const char* metaTypeName(uint8_t t) {
    switch (t) {
        case (uint8_t)fs::ContentMetaType::Application:  return "Application (base)";
        case (uint8_t)fs::ContentMetaType::Patch:        return "Patch (update)";
        case (uint8_t)fs::ContentMetaType::AddOnContent: return "AddOnContent (DLC)";
        case (uint8_t)fs::ContentMetaType::Delta:        return "Delta";
    }
    return "other";
}

const char* cnmtContentTypeName(uint8_t t) {
    static const char* names[] = {"Meta","Program","Data","Control","HtmlDoc","LegalInfo","DeltaFragment"};
    return (t < 7) ? names[t] : "?";
}

// Stream the SHA-256 of `size` bytes at `off` in 1 MiB reads (NCAs can be multi-GB,
// so we never buffer the whole file). Returns false on a short read.
bool sha256OfRange(io::ContentSource& src, uint64_t off, uint64_t size, uint8_t out[32]) {
    constexpr size_t CHUNK = 0x100000;
    std::vector<uint8_t> buf(CHUNK);
    crypto::Sha256 h;
    uint64_t done = 0;
    while (done < size) {
        size_t want = (size_t)(size - done < CHUNK ? size - done : CHUNK);
        if (src.read(buf.data(), off + done, want) != want) return false;
        h.update(buf.data(), want);
        done += want;
    }
    h.finish(out);
    return true;
}

void hex16(const uint8_t* b, char out[33]) {
    static const char* H = "0123456789abcdef";
    for (int i = 0; i < 16; ++i) { out[i*2] = H[b[i]>>4]; out[i*2+1] = H[b[i]&0xF]; }
    out[32] = 0;
}

// Decode the NCA header of a .nca/.ncz entry and log its fields. `nczStart` is the
// entry offset; for .ncz the first 0x4000 is the verbatim NCA header region, so the
// 0xC00 header read works for both. Sets anyRightsId. Needs keys.
void logNcaHeader(io::ContentSource& src, const std::string& name, uint64_t off,
                  const crypto::Keyset* ks, bool& anyRightsId) {
    if (!ks || !ks->has_header_key) {
        dbi::log::line("[dry-run]   (NCA header fields need prod.keys - skipped)");
        return;
    }
    std::vector<uint8_t> hdr(0xC00);
    if (src.read(hdr.data(), off, hdr.size()) != hdr.size()) {
        dbi::log::line("[dry-run]   (short read on NCA header)");
        return;
    }
    crypto::NcaInfo info;
    if (!crypto::ncaParseHeader(hdr.data(), hdr.size(), *ks, info)) {
        dbi::log::line("[dry-run]   header decrypt FAILED (wrong key / not NCA3)");
        return;
    }
    if (info.hasRightsId) anyRightsId = true;
    dbi::log::line("[dry-run]   contentType=%s keygen=%u kaek=%u titleId=%016llx rightsId=%s",
                   ncaContentTypeName(info.contentType), info.keygen, info.kaekIndex,
                   (unsigned long long)info.titleId, info.hasRightsId ? "yes" : "no");
}

} // namespace

DryRunResult dryRunInstall(io::ContentSource& src, const crypto::Keyset* ks) {
    DryRunResult res;
    res.keysAvailable = (ks && ks->has_header_key);

    dbi::log::line("[dry-run] ==== analyzing container (NO WRITES) ====");
    fs::Pfs0 pfs;
    if (!pfs.open(src)) {
        dbi::log::line("[dry-run] not a PFS0/NSP container");
        return res;
    }
    res.parsed = true;
    dbi::log::line("[dry-run] PFS0 OK, %zu entries; prod.keys=%s",
                   pfs.entries().size(), res.keysAvailable ? "present" : "absent");

    // Pass 1: classify + per-NCA header diagnostics.
    for (const auto& e : pfs.entries()) {
        if (endsWith(e.name, ".ncz")) {
            ++res.ncaCount;
            uint64_t full = crypto::nczReconstructedSize(src, e.offset);
            dbi::log::line("[dry-run] NCZ %s: compressed=%llu reconstructed=%llu",
                           e.name.c_str(), (unsigned long long)e.size, (unsigned long long)full);
            if (full == 0) dbi::log::line("[dry-run]   WARNING: ncz section-table parse failed");
            logNcaHeader(src, e.name, e.offset, ks, res.anyRightsId);
        } else if (endsWith(e.name, ".nca")) {
            ++res.ncaCount;
            bool isMeta = endsWith(e.name, ".cnmt.nca");
            dbi::log::line("[dry-run] NCA %s: size=%llu%s",
                           e.name.c_str(), (unsigned long long)e.size, isMeta ? " (Meta)" : "");
            logNcaHeader(src, e.name, e.offset, ks, res.anyRightsId);
        } else if (endsWith(e.name, ".tik")) {
            ++res.ticketCount;
            dbi::log::line("[dry-run] TICKET %s: size=%llu", e.name.c_str(), (unsigned long long)e.size);
        } else if (endsWith(e.name, ".cert")) {
            ++res.certCount;
            dbi::log::line("[dry-run] CERT %s: size=%llu", e.name.c_str(), (unsigned long long)e.size);
        } else {
            dbi::log::line("[dry-run] other %s: size=%llu", e.name.c_str(), (unsigned long long)e.size);
        }
    }

    // Pass 2: extract + parse the CNMT (needs keys) to report the meta target.
    // Retained for Pass 3 so each content NCA can be checked against its CNMT hash.
    std::vector<fs::CnmtContent> metaContents;
    for (const auto& e : pfs.entries()) {
        if (!endsWith(e.name, ".cnmt.nca")) continue;
        res.cnmtFound = true;
        if (!res.keysAvailable) {
            dbi::log::line("[dry-run] CNMT present in %s but prod.keys absent - cannot decode contents", e.name.c_str());
            break;
        }
        std::vector<uint8_t> nca((size_t)e.size);
        if (src.read(nca.data(), e.offset, nca.size()) != nca.size()) {
            dbi::log::line("[dry-run] CNMT: short read on Meta NCA"); break;
        }
        std::vector<uint8_t> cnmt = crypto::ncaExtractCnmt(nca.data(), nca.size(), *ks);
        if (cnmt.empty()) { dbi::log::line("[dry-run] CNMT extract failed (crypto)"); break; }
        fs::Cnmt cn;
        if (!cn.parse(cnmt.data(), cnmt.size())) { dbi::log::line("[dry-run] CNMT parse failed"); break; }
        res.metaType = cn.metaType;
        res.titleId  = cn.titleId;
        metaContents = cn.contents;
        dbi::log::line("[dry-run] CNMT: titleId=%016llx version=%u type=%s contents=%zu",
                       (unsigned long long)cn.titleId, cn.version, metaTypeName(cn.metaType), cn.contents.size());
        for (size_t i = 0; i < cn.contents.size(); ++i)
            dbi::log::line("[dry-run]   content[%zu] type=%s size=%llu",
                           i, cnmtContentTypeName(cn.contents[i].type),
                           (unsigned long long)cn.contents[i].size);
        break;
    }

    // Pass 3: CheckHash. Stream the SHA-256 of every .nca and verify it against two
    // references: the NCA naming invariant (first 16 bytes of the digest == the
    // content id in the filename, needs no keys) and, when the CNMT was decoded, the
    // full 32-byte hash recorded for that content. Read-only; never writes.
    res.hashChecked = true;
    dbi::log::line("[dry-run] ==== CheckHash (SHA-256, read-only) ====");
    for (const auto& e : pfs.entries()) {
        if (endsWith(e.name, ".ncz")) {
            ++res.hashSkipped;
            dbi::log::line("[dry-run] HASH %s: skipped (NSZ reconstruction hashing is M3)", e.name.c_str());
            continue;
        }
        if (!endsWith(e.name, ".nca")) continue;

        uint8_t digest[32];
        if (!sha256OfRange(src, e.offset, e.size, digest)) {
            ++res.hashFail;
            dbi::log::line("[dry-run] HASH %s: FAIL (short read while hashing)", e.name.c_str());
            continue;
        }

        // Naming invariant: filename id == first 16 bytes of the digest.
        ContentId fileId{};
        bool haveId = parseContentId(e.name, fileId);
        bool idMatch = haveId && std::memcmp(fileId.data(), digest, 16) == 0;

        // CNMT cross-check (only when the CNMT was decoded and lists this id).
        const fs::CnmtContent* ref = nullptr;
        for (const auto& mc : metaContents)
            if (haveId && std::memcmp(mc.id, fileId.data(), 16) == 0) { ref = &mc; break; }
        bool cnmtMatch = ref && std::memcmp(ref->hash, digest, 32) == 0;

        char got[33], want[33];
        hex16(digest, got);
        if (ref && !cnmtMatch) {
            hex16(ref->hash, want);
            ++res.hashFail;
            dbi::log::line("[dry-run] HASH %s: FAIL cnmt-hash mismatch (got %s.. want %s..)",
                           e.name.c_str(), got, want);
        } else if (!idMatch) {
            ++res.hashFail;
            dbi::log::line("[dry-run] HASH %s: FAIL name/hash mismatch (sha256[0..15]=%s..)",
                           e.name.c_str(), got);
        } else {
            ++res.hashPass;
            dbi::log::line("[dry-run] HASH %s: PASS (%s%s)", e.name.c_str(),
                           ref ? "cnmt+name" : "name", ref ? "" : "; no CNMT ref");
        }
    }
    dbi::log::line("[dry-run] CheckHash: %d pass, %d fail, %d skipped%s",
                   res.hashPass, res.hashFail, res.hashSkipped,
                   res.keysAvailable ? "" : " (no prod.keys: name-only check)");

    // Planned-install summary.
    dbi::log::line("[dry-run] PLAN: would write %d NCA(s), import %d ticket(s) (%d cert), "
                   "set meta=%s, target=SD",
                   res.ncaCount, res.ticketCount, res.certCount,
                   res.cnmtFound ? metaTypeName(res.metaType) : "<none>");
    if (res.anyRightsId && res.ticketCount == 0)
        dbi::log::line("[dry-run] WARNING: rights-id content but no ticket present - install would need a ticket");
    if (!res.keysAvailable)
        dbi::log::line("[dry-run] NOTE: place prod.keys at sdmc:/switch/prod.keys for full content decoding");
    dbi::log::line("[dry-run] ==== done (NO WRITES PERFORMED) ====");
    return res;
}

} // namespace dbi::install
