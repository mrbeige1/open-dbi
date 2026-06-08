// ncm_backend.cpp - real libnx ncm + custom es IPC InstallBackend. See ncm_backend.h.
#include "ncm_backend.h"
#include <switch.h>
#include <cstdio>
#include <cstring>

namespace dbi::install {

static NcmContentId    toContentId(const ContentId& a)    { NcmContentId c;    std::memcpy(c.c, a.data(), 16); return c; }
static NcmPlaceHolderId toPlaceHolderId(const ContentId& a){ NcmPlaceHolderId p; std::memcpy(&p.uuid, a.data(), 16); return p; }

bool NcmBackend::open(StorageId storage) {
    if (R_FAILED(ncmInitialize())) return false;
    storageId_ = (int)storage;
    auto* cs = new NcmContentStorage{};
    auto* db = new NcmContentMetaDatabase{};
    if (R_FAILED(ncmOpenContentStorage(cs, (NcmStorageId)storageId_)))      { delete cs; delete db; return false; }
    if (R_FAILED(ncmOpenContentMetaDatabase(db, (NcmStorageId)storageId_))) { ncmContentStorageClose(cs); delete cs; delete db; return false; }
    cs_ = cs; db_ = db; open_ = true;
    return true;
}

bool NcmBackend::generatePlaceHolderId(ContentId& out) {
    NcmPlaceHolderId ph;
    if (R_FAILED(ncmContentStorageGeneratePlaceHolderId((NcmContentStorage*)cs_, &ph))) return false;
    std::memcpy(out.data(), &ph.uuid, 16);
    return true;
}

bool NcmBackend::createPlaceHolder(const ContentId& cid, const ContentId& ph, uint64_t size) {
    NcmContentId c = toContentId(cid); NcmPlaceHolderId p = toPlaceHolderId(ph);
    return R_SUCCEEDED(ncmContentStorageCreatePlaceHolder((NcmContentStorage*)cs_, &c, &p, (s64)size));
}

bool NcmBackend::writePlaceHolder(const ContentId& ph, uint64_t offset, const void* data, size_t len) {
    NcmPlaceHolderId p = toPlaceHolderId(ph);
    return R_SUCCEEDED(ncmContentStorageWritePlaceHolder((NcmContentStorage*)cs_, &p, offset, data, len));
}

bool NcmBackend::registerContent(const ContentId& cid, const ContentId& ph) {
    NcmContentId c = toContentId(cid); NcmPlaceHolderId p = toPlaceHolderId(ph);
    return R_SUCCEEDED(ncmContentStorageRegister((NcmContentStorage*)cs_, &c, &p));
}

// es ImportTicket (switchbrew: service "es", cmd 1; two In MapAlias buffers).
bool NcmBackend::importTicket(const void* tik, size_t tikLen, const void* cert, size_t certLen) {
    Service es;
    if (R_FAILED(smGetService(&es, "es"))) return false;
    const struct { } in = {};
    // If no cert provided, pass an empty buffer (DBI bundles .cert alongside .tik).
    bool ok = R_SUCCEEDED(serviceDispatchIn(&es, 1, in,
        .buffer_attrs = { SfBufferAttr_In | SfBufferAttr_HipcMapAlias,
                          SfBufferAttr_In | SfBufferAttr_HipcMapAlias },
        .buffers = { { tik, tikLen }, { cert, certLen } }));
    serviceClose(&es);
    return ok;
}

bool NcmBackend::setContentMeta(uint64_t id, uint32_t version, uint8_t type,
                                const void* blob, size_t blobLen) {
    NcmContentMetaKey key{};
    key.id = id; key.version = version; key.type = type;
    key.install_type = NcmContentInstallType_Full;
    Result rc = ncmContentMetaDatabaseSet((NcmContentMetaDatabase*)db_, &key, blob, blobLen);
    if (R_FAILED(rc)) { printf("  [ncm] metaDatabaseSet rc=0x%08X\n", rc); return false; }
    metaId_ = id; metaVer_ = version; metaType_ = type;   // for the ns record in commit()
    return true;
}

// ns: PushApplicationRecord (IApplicationManagerInterface cmd 16) so the home menu
// shows the title. CLEAN-ROOM from the public ns:am IPC.
bool NcmBackend::pushApplicationRecord() {
    // Record's application id: Patch (0x81) -> base (clear 0x800); else use the meta id.
    uint64_t appId = metaId_;
    if (metaType_ == 0x81) appId = metaId_ & ~(uint64_t)0x800;          // patch -> base
    else if (metaType_ == 0x82) appId = metaId_ & ~(uint64_t)0xFFF;     // add-on -> base (approx)

    struct ContentStorageRecord { NcmContentMetaKey key; u64 storage_id; } rec{};
    rec.key.id = metaId_; rec.key.version = metaVer_; rec.key.type = metaType_;
    rec.key.install_type = NcmContentInstallType_Full;
    rec.storage_id = (u64)storageId_;

    if (R_FAILED(nsInitialize())) { printf("  [ns] nsInitialize failed\n"); return false; }
    Service am;
    Result rc = nsGetApplicationManagerInterface(&am);
    if (R_FAILED(rc)) { printf("  [ns] getAppManager rc=0x%08X\n", rc); nsExit(); return false; }

    struct { u8 last_modified_event; u64 application_id; } in{};
    in.last_modified_event = 3;   // "installed"
    in.application_id = appId;
    rc = serviceDispatchIn(&am, 16, in,
        .buffer_attrs = { SfBufferAttr_In | SfBufferAttr_HipcMapAlias },
        .buffers = { { &rec, sizeof(rec) } });
    serviceClose(&am);
    nsExit();
    if (R_FAILED(rc)) { printf("  [ns] PushApplicationRecord rc=0x%08X (appId=%016llx)\n", rc, (unsigned long long)appId); return false; }
    printf("  [ns] PushApplicationRecord OK (appId=%016llx)\n", (unsigned long long)appId);
    return true;
}

bool NcmBackend::commit() {
    if (!db_) return false;
    if (R_FAILED(ncmContentMetaDatabaseCommit((NcmContentMetaDatabase*)db_))) return false;
    // Register the application record so the title appears on the home menu.
    pushApplicationRecord();   // non-fatal: content is installed either way
    return true;
}

void NcmBackend::closeStorage() {
    if (cs_) { ncmContentStorageClose((NcmContentStorage*)cs_); delete (NcmContentStorage*)cs_; cs_ = nullptr; }
    if (db_) { ncmContentMetaDatabaseClose((NcmContentMetaDatabase*)db_); delete (NcmContentMetaDatabase*)db_; db_ = nullptr; }
    if (open_) { ncmExit(); open_ = false; }
}

} // namespace dbi::install
