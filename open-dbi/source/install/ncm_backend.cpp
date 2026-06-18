// ncm_backend.cpp - real libnx ncm + custom es IPC InstallBackend. See ncm_backend.h.
#include "ncm_backend.h"
#include "../log/log.h"
#include <switch.h>
#include <cstdio>
#include <cstring>

namespace dbi::install {

static NcmContentId    toContentId(const ContentId& a)    { NcmContentId c;    std::memcpy(c.c, a.data(), 16); return c; }
static NcmPlaceHolderId toPlaceHolderId(const ContentId& a){ NcmPlaceHolderId p; std::memcpy(&p.uuid, a.data(), 16); return p; }

bool NcmBackend::open(StorageId storage) {
    Result rc = ncmInitialize();
    dbi::log::result("ncm", "ncmInitialize", rc);
    if (R_FAILED(rc)) return false;
    storageId_ = (int)storage;
    auto* cs = new NcmContentStorage{};
    auto* db = new NcmContentMetaDatabase{};
    rc = ncmOpenContentStorage(cs, (NcmStorageId)storageId_);
    dbi::log::result("ncm", "openContentStorage", rc);
    if (R_FAILED(rc)) { delete cs; delete db; return false; }
    rc = ncmOpenContentMetaDatabase(db, (NcmStorageId)storageId_);
    dbi::log::result("ncm", "openContentMetaDatabase", rc);
    if (R_FAILED(rc)) { ncmContentStorageClose(cs); delete cs; delete db; return false; }
    cs_ = cs; db_ = db; open_ = true;
    return true;
}

bool NcmBackend::generatePlaceHolderId(ContentId& out) {
    NcmPlaceHolderId ph;
    Result rc = ncmContentStorageGeneratePlaceHolderId((NcmContentStorage*)cs_, &ph);
    if (R_FAILED(rc)) { dbi::log::result("ncm", "generatePlaceHolderId", rc); return false; }
    std::memcpy(out.data(), &ph.uuid, 16);
    return true;
}

bool NcmBackend::createPlaceHolder(const ContentId& cid, const ContentId& ph, uint64_t size) {
    NcmContentId c = toContentId(cid); NcmPlaceHolderId p = toPlaceHolderId(ph);
    Result rc = ncmContentStorageCreatePlaceHolder((NcmContentStorage*)cs_, &c, &p, (s64)size);
    dbi::log::result("ncm", "createPlaceHolder", rc);
    return R_SUCCEEDED(rc);
}

bool NcmBackend::writePlaceHolder(const ContentId& ph, uint64_t offset, const void* data, size_t len) {
    NcmPlaceHolderId p = toPlaceHolderId(ph);
    // Called once per 1 MiB chunk - log failures only so the log isn't flooded.
    Result rc = ncmContentStorageWritePlaceHolder((NcmContentStorage*)cs_, &p, offset, data, len);
    if (R_FAILED(rc)) dbi::log::result("ncm", "writePlaceHolder", rc);
    return R_SUCCEEDED(rc);
}

bool NcmBackend::registerContent(const ContentId& cid, const ContentId& ph) {
    NcmContentId c = toContentId(cid); NcmPlaceHolderId p = toPlaceHolderId(ph);
    Result rc = ncmContentStorageRegister((NcmContentStorage*)cs_, &c, &p);
    dbi::log::result("ncm", "registerContent", rc);
    return R_SUCCEEDED(rc);
}

// es ImportTicket (switchbrew: service "es", cmd 1; two In MapAlias buffers).
bool NcmBackend::importTicket(const void* tik, size_t tikLen, const void* cert, size_t certLen) {
    Service es;
    Result rc = smGetService(&es, "es");
    if (R_FAILED(rc)) { dbi::log::result("es", "smGetService(es)", rc); return false; }
    const struct { } in = {};
    // If no cert provided, pass an empty buffer (DBI bundles .cert alongside .tik).
    rc = serviceDispatchIn(&es, 1, in,
        .buffer_attrs = { SfBufferAttr_In | SfBufferAttr_HipcMapAlias,
                          SfBufferAttr_In | SfBufferAttr_HipcMapAlias },
        .buffers = { { tik, tikLen }, { cert, certLen } });
    dbi::log::result("es", "importTicket", rc);
    serviceClose(&es);
    return R_SUCCEEDED(rc);
}

bool NcmBackend::setContentMeta(uint64_t id, uint32_t version, uint8_t type,
                                const void* blob, size_t blobLen) {
    NcmContentMetaKey key{};
    key.id = id; key.version = version; key.type = type;
    key.install_type = NcmContentInstallType_Full;
    Result rc = ncmContentMetaDatabaseSet((NcmContentMetaDatabase*)db_, &key, blob, blobLen);
    dbi::log::result("ncm", "contentMetaDatabaseSet", rc);
    if (R_FAILED(rc)) return false;
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

    Result rc = nsInitialize();
    dbi::log::result("ns", "nsInitialize", rc);
    if (R_FAILED(rc)) return false;
    Service am;
    rc = nsGetApplicationManagerInterface(&am);
    if (R_FAILED(rc)) { dbi::log::result("ns", "getApplicationManagerInterface", rc); nsExit(); return false; }

    struct { u8 last_modified_event; u64 application_id; } in{};
    in.last_modified_event = 3;   // "installed"
    in.application_id = appId;
    rc = serviceDispatchIn(&am, 16, in,
        .buffer_attrs = { SfBufferAttr_In | SfBufferAttr_HipcMapAlias },
        .buffers = { { &rec, sizeof(rec) } });
    serviceClose(&am);
    nsExit();
    dbi::log::result("ns", "pushApplicationRecord", rc);
    if (R_FAILED(rc)) return false;
    dbi::log::line("[ns] pushApplicationRecord OK (appId=%016llx)", (unsigned long long)appId);
    return true;
}

bool NcmBackend::commit() {
    if (!db_) return false;
    Result rc = ncmContentMetaDatabaseCommit((NcmContentMetaDatabase*)db_);
    dbi::log::result("ncm", "contentMetaDatabaseCommit", rc);
    if (R_FAILED(rc)) return false;
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
