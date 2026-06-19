// dry_run.h - parse + classify an NSP/NSZ and log the *planned* install without
// touching ncm. Lets testers validate many files safely (no risk of a broken
// partial install) and produces the structured log a debug report bundles up.
//
// CLEAN-ROOM: reuses the existing fs/crypto modules (Pfs0, Cnmt, ncaParseHeader,
// ncaExtractCnmt, nczReconstructedSize); it is a read-only generalization of the
// ncaDecryptTest() harness, written against docs/FORMATS.md + docs/ARCHITECTURE.md.
#pragma once
#include "../io/content_source.h"
#include "../crypto/keyset.h"

namespace dbi::install {

// Summary of what an install *would* do, returned for the report bundle.
struct DryRunResult {
    bool     parsed = false;       // container parsed as PFS0
    int      ncaCount = 0;         // .nca + .ncz entries
    int      ticketCount = 0;      // .tik entries
    int      certCount = 0;        // .cert entries
    bool     cnmtFound = false;
    uint8_t  metaType = 0;         // CNMT meta_type (0x80 base / 0x81 update / 0x82 DLC)
    uint64_t titleId = 0;
    bool     anyRightsId = false;  // any NCA carries a rights-id (titlekey crypto)
    bool     keysAvailable = false;// prod.keys present (NCA-header fields decoded)
    // CheckHash (SHA-256) verification pass:
    bool     hashChecked = false;  // the verification pass ran
    int      hashPass = 0;         // NCAs whose SHA-256 matched its reference
    int      hashFail = 0;         // NCAs whose SHA-256 did NOT match (corruption)
    int      hashSkipped = 0;      // .ncz entries (reconstruction hashing is M3)
};

// Parse `src`, log every entry's structure + a planned-install summary via dbi::log,
// and return the summary. `ks` may be null; NCA-header fields and CNMT extraction
// that require decryption are then logged as "needs prod.keys" and skipped.
// Performs NO backend/ncm writes.
DryRunResult dryRunInstall(io::ContentSource& src, const crypto::Keyset* ks);

} // namespace dbi::install
