// meta_builder.h - convert a decrypted CNMT (PackagedContentMeta) into the ncm
// content-meta-database blob that ncmContentMetaDatabaseSet expects.
//
// CLEAN-ROOM from the public CNMT + ncm meta layout (switchbrew / libnx ncm_types):
//   in : PackagedContentMetaHeader(0x20) + ext header + N*PackagedContentInfo(0x38)
//        + M*ContentMetaInfo(0x10) [+ digest]
//   out: NcmContentMetaHeader(0x08) + ext header + (N+1)*NcmContentInfo(0x18)
//        + M*ContentMetaInfo(0x10)
//   (+1 = the Meta NCA's own content record, which the installer must add.)
#pragma once
#include <cstdint>
#include <vector>
#include "installer.h"   // for ContentId

namespace dbi::install {

struct MetaToSet {
    uint64_t id = 0;
    uint32_t version = 0;
    uint8_t  type = 0;          // NcmContentMetaType (0x80 App, 0x81 Patch, ...)
    std::vector<uint8_t> blob;  // the ncm meta-db payload
};

// Build the ncm meta payload from a raw decrypted CNMT plus the Meta NCA's own
// content id + size. Returns false if the CNMT is malformed.
bool buildNcmMeta(const uint8_t* cnmt, size_t len,
                  const ContentId& metaNcaId, uint64_t metaNcaSize, MetaToSet& out);

} // namespace dbi::install
