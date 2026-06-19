// installer.h - the NSP install sequence, reconstructed clean-room from the
// behavioral description of dbi_install_run in docs/ARCHITECTURE.md (NOT from
// decompiled code).
//
// The installer drives an abstract InstallBackend (the ncm/es operations) so the
// algorithm is testable; a real libnx-ncm backend is a separate implementation.
#pragma once
#include <cstdint>
#include <string>
#include <array>
#include "../io/content_source.h"
#include "../fs/pfs0.h"
#include "../crypto/keyset.h"

namespace dbi::install {

using ContentId = std::array<uint8_t, 16>;   // NCA id (16 bytes)

// Storage target (mirrors ncm StorageId: 1=BuiltInUser(NAND), 5=SdCard).
enum class StorageId : uint8_t { NandUser = 1, SdCard = 5 };

// The set of operations the installer performs, abstracted from libnx ncm/es.
class InstallBackend {
public:
    virtual ~InstallBackend() = default;
    virtual bool open(StorageId storage) = 0;
    // ncm placeholder lifecycle
    virtual bool generatePlaceHolderId(ContentId& outPlaceHolder) = 0;
    virtual bool createPlaceHolder(const ContentId& contentId, const ContentId& placeHolder, uint64_t size) = 0;
    virtual bool writePlaceHolder(const ContentId& placeHolder, uint64_t offset, const void* data, size_t len) = 0;
    virtual bool registerContent(const ContentId& contentId, const ContentId& placeHolder) = 0;
    // es ticket import
    virtual bool importTicket(const void* tik, size_t tikLen, const void* cert, size_t certLen) = 0;
    // ncm content-meta (blob built by meta_builder) + commit
    virtual bool setContentMeta(uint64_t id, uint32_t version, uint8_t type,
                                const void* blob, size_t blobLen) = 0;
    virtual bool commit() = 0;
    virtual void closeStorage() = 0;
};

// The install proceeds through these phases in order; failedPhase records the one
// that aborted so logs/reports can name the exact step ("open storage, create
// placeholder, write, register, import ticket, set meta, commit").
enum class Phase { Parse, WriteNca, ImportTicket, ContentMeta, Commit };
const char* phaseName(Phase p);

// Optional observer the installer notifies at each phase boundary and on failure.
// Default no-op; main.cpp wires one that forwards to dbi::log. Keeping it an
// abstract hook keeps the install core free of any console/log dependency.
struct InstallObserver {
    virtual ~InstallObserver() = default;
    virtual void onPhase(Phase p, const char* detail) {}   // entering / progressing a phase
    virtual void onError(Phase p, const char* op) {}        // failure in phase p at op
};

struct InstallResult {
    bool ok = false;
    int ncasWritten = 0;
    int ticketsImported = 0;
    Phase failedPhase = Phase::Parse;   // meaningful only when ok == false
    std::string error;
};

class Installer {
public:
    explicit Installer(InstallBackend& backend) : backend_(backend) {}

    // Optional: provide keys so the cnmt.nca can be decrypted to obtain the CNMT
    // (required for setContentMeta). Without it, the meta step is skipped.
    void setKeyset(const crypto::Keyset* ks) { keyset_ = ks; }

    // Optional progress/error observer (see InstallObserver). Default: none.
    void setObserver(InstallObserver* obs) { obs_ = obs; }

    // Install an NSP read through `nsp` into `storage`. Implements the recovered
    // phase order: parse container -> per-NCA placeholder/write/register ->
    // ticket import -> setContentMeta -> commit.
    InstallResult installNsp(io::ContentSource& nsp, StorageId storage);

private:
    InstallBackend& backend_;
    const crypto::Keyset* keyset_ = nullptr;
    InstallObserver* obs_ = nullptr;
    bool streamIntoPlaceHolder(io::ContentSource& src, const fs::Pfs0Entry& e,
                               const ContentId& placeHolder);
};

// Parse a 16-hex-char NCA filename prefix (e.g. "<32hex>.nca") into a ContentId.
bool parseContentId(const std::string& name, ContentId& out);

} // namespace dbi::install
