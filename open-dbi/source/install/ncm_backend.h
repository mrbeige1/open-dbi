// ncm_backend.h - a real libnx ncm/es implementation of InstallBackend.
//
// CLEAN-ROOM: written from the libnx ncm API + the public es IPC (switchbrew) and
// the install sequence in docs/ARCHITECTURE.md. Not derived from decompiled code.
//
// NOTE: writePlaceHolder/register/es-import need no title keys. setContentMeta needs
// the decrypted CNMT (NCA crypto / prod.keys) which is a separate, not-yet-built
// subsystem — see the implementation's documented limitation.
#pragma once
#include "installer.h"

namespace dbi::install {

class NcmBackend : public InstallBackend {
public:
    bool open(StorageId storage) override;
    bool generatePlaceHolderId(ContentId& outPlaceHolder) override;
    bool createPlaceHolder(const ContentId& contentId, const ContentId& placeHolder, uint64_t size) override;
    bool writePlaceHolder(const ContentId& placeHolder, uint64_t offset, const void* data, size_t len) override;
    bool registerContent(const ContentId& contentId, const ContentId& placeHolder) override;
    bool importTicket(const void* tik, size_t tikLen, const void* cert, size_t certLen) override;
    bool setContentMeta(uint64_t id, uint32_t version, uint8_t type,
                        const void* blob, size_t blobLen) override;
    bool commit() override;
    void closeStorage() override;

private:
    bool pushApplicationRecord();   // ns: make the title visible to the home menu

    void* cs_ = nullptr;   // NcmContentStorage*       (opaque here)
    void* db_ = nullptr;   // NcmContentMetaDatabase*
    int   storageId_ = 0;
    bool  open_ = false;
    // last setContentMeta key, used by commit() to push the ns application record
    uint64_t metaId_ = 0;
    uint32_t metaVer_ = 0;
    uint8_t  metaType_ = 0;
};

} // namespace dbi::install
