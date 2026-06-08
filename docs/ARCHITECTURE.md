# DBI Architecture (reconstructed)

A big-picture map of DBI's internal structure, synthesized from the 99 recovered `dbi_*` functions
(27 fully analyzed; see `SYMBOLS.md` + `~/switch-re/exports/recovered/*.md`). Confidence varies per
subsystem; gaps are called out. This is a *reconstruction in progress*, not ground truth.

---

## 1. Process model

- Single AArch64 **homebrew NRO** on **libnx**, C++17, statically linked (libnx, newlib, libstdc++,
  libcurl, zstd/zlib/lzma/bzip2, mbedTLS, libssh2, plus fmt/pugixml/utfcpp/qrcodegen/nlohmann-json).
- **Custom framebuffer UI** on `nvnflinger` with an embedded TTF — not a UI toolkit. The main loop
  drives a two-panel file browser + menus; long operations (install/dump/serve) run with progress
  reporting back to the UI.
- **String obfuscation everywhere**: most app string literals are stored encrypted (polymorphic
  XOR + bit-rotate, per-string keys) and decoded on demand via a TLS-backed pool. Reached through
  `dbi_strpool_get` (`710067ada0`, ~562 callers) and `dbi_util_tlsBase`. This is why static string
  triage failed and why decompiled functions show inline decode loops.

## 2. Cross-cutting infrastructure (the spine)

| Component | Anchor(s) | Role |
|-----------|-----------|------|
| **Global context** | `dbi_globalContext` (`710018a9e0`, ~225 callers) | App singleton. `+0x60` = file/fs manager, `+0x150` = time source, `+0x58` = save manager handle. Threads through every subsystem. |
| **Config** | `dbi_config_loadAndApply` (`7100055a10`), `dbi_config_parse` (`71005f7c10`), `dbi_config_getString`/`getBool` | INI parse into a hash-table singleton; all `[General]`/folder/feature keys (see `CONFIG.md`). Sets language/mode globals. |
| **String pool** | `dbi_strpool_get` (`710067ada0`) | Fetch/decode an obfuscated (and possibly localized) string. |
| **FS abstraction** | `FileManager_*` (`mkdir`/`registerFile`), `FileEntry_create` (`710007c720`), `dbi_file_readHeader`/`readBlock` | A virtual file/path layer over `fsp-srv` + mounted storages; backs browsing, install, dump. |
| **Archive builder** | `Pfs0Builder_ctor`/`addNcaEntry`/`addTicketEntry`/`addStreamEntry`, `StreamWriter_init` | Streams NCAs/tickets/certs into a **PFS0 (NSP)** archive (used by dump + forwarder). |
| **Reporting/log** | `dbi_log_writeLog` (`71005f8800`), `dbi_reportSink_appendString`/`appendLine`, sinks `710019b750/b7e0` | Mutex-guarded log gated by `LogEvents`; an in-memory "report sink" buffer feeds UI/console/MTP. |
| **Time** | `dbi_system_getCurrentTime` (`71001a3420`), `dbi_time_nowId`, `dbi_time_format` | Timestamps + filename stamps for backups/dumps/logs. |

## 3. Subsystem map

### saves
Backup a title's save FS to a **ZIP** under `SavesFolder`, named `<TID>_<space>_<ts>_<idx>.zip`,
containing the file tree + `.dbi_save_info.ini` (`[General]` TitleId/TitleName/BackupDate/Account/Space)
+ optional 512-byte `.dbi_save_extra`. Functions: `backupSaveToZip`, `refreshBackupList`,
`deleteSelectedSaves` (gated by `FoolproofSaveDelete`), `extendSaveFs` (1.25×/1 MiB-aligned grow),
`copySaveExtraData`/`applySaveExtraData`, `readSaveExtraData`/`writeSaveExtraData`. Save-data id key
seen: `id & 0x1fff | 0xdb104c46534d4000`. ⚠️ `uploadBackupsOverNetwork` has an unverified obfuscated
remote-endpoint path (see `SYMBOLS.md` finding #4).

### dumps
Dump installed titles / gamecard to NSP/XCI under `DumpsFolder`. Entry points enqueue a **256-byte
dump job** (`buildContentDumpPathAndEnqueue` by name, `buildGamecardDumpPathAndEnqueue` by title-id).
`resolveContentDumpTargets` filters content records by meta-type (`0x10000..0x10005`) + per-NCA-section
presence; `buildNspDump` streams NCAs+CNMT+tickets/certs into a PFS0; `addExeFsEntriesToDumpJob` adds
ExeFS files; `buildDumpEntryList` is the menu.

### install / ncm  *(core — recovered)*
**`dbi_install_run`** (`71001e2020`, 47 KB) is the install state machine. Phases:
0. **Prepare** — open transaction, `parseContainer` (`71001db350`), `resolveContent` (`71001de820`),
   `findSingleCnmt` (`710017ed40`).
1. **Per-NCA meta-write** — look up the target storage's **`dbi_NcmAccessor`** (in a
   `std::map<StorageId,node>` at `globalContext()+8`), then `generatePlaceHolderId → getContentMeta →
   createPlaceHolder → register/pushKey`, plus `writeNcaContent` (`71001e01c0`, streamed write+verify)
   and LFS creation.
2. **Content records** — filter by `ContentMetaType`, then `processContentRecord` (`7100123330`) —
   **es ticket import + NSZ/XCZ decompression happen here** — then commit.
3. **Commit pass** — `register` + `setContentMeta`.
4. **Cleanup/abort** via a sticky error flag at `this+0x75`.

**Content source = a vtable `File`/`InputStream` interface** (slots: `getSize`/`read`/`close`). The
installer never branches on transport — USB/MTP/file/network all present as this interface (the inline
adapter is a `MemoryFile`). **This is the clean seam `open-dbi` reimplements against.**

**ncm wrapper tier** — `dbi_NcmAccessor`: one object owning `NcmContentStorage` (`+0x04`) +
`NcmContentMetaDatabase` (`+0x14`). Write methods: `dbi_ncm_generatePlaceHolderId/createPlaceHolder/
writePlaceHolder/registerContent/setContentMeta` (`710019d*`); read methods: list/get
(`710019e*`). `dbi_ncm_moveTitleToStorage` (`710013fdf0`) relocates a title between storages.

**Install write workers** — `dbi_install_writeContentFromMemory(+WithOptions)` resolve a per-session
installer (RB-map at `globalContext()+0x10`, via `dbi_install_getSession`) carrying the storage target,
then run the placeholder→write→register sequence per content blob.

NCA construction: `nca_buildIvfcLevels` builds the 6-level **IVFC/RomFS hash tree** (`ivfc_0..5`,
16 KiB blocks), `nca_buildCtx_init`/`dtor`.

**Still open in install:** the concrete `File` adapters bridging each transport (DBI0-USB
`FILE_RANGE`, MTP, gamecard) to the `InputStream` interface; and **where es-ticket-import + NSZ/XCZ
decompression actually live** — verified *not* in `71001e01c0` (a status-log emitter, renamed
`dbi_install_reportNcaContentStatus`) nor in `7100123330` (a metadata reader that scans for the
`nnSdk` version key). Next RE step: find callers of the libnx `esImportTicket` wrapper and of the
zstd/lzma decompressors.

### forwarder
`forwarder_buildNsp`: strips `sdmc:`, reads a source file, makes a 256×256 JPEG icon, and assembles a
complete NSP from baked-in NCA templates → produces installable shortcut/forwarder NSPs.

### serving modes — mtp / ftp / network
- `mtp_responderRun`: device-side **MTP-over-USB** — `usbDsWaitReady`, 12-byte container header,
  opcode→handler `std::map` dispatch; falls back to FTP if USB not ready. Honors `ReportAndroidExtension`.
- `ftp_serverStart`/`serverRun` + `ftp_formatListingEntry` (MLST/MLSD/`ls -l` into a 1 MiB buffer).
- `net_startAccessPointFtp`: brings up a **Wi-Fi soft-AP** (`[Access point]` config; 15-char MT19937
  random password if unset), shows credentials + **QR grid**, then runs the FTP server.

### pdm (Activity Log)
`pdm_buildPlayEventReport` / `buildActivityLogReport`: query the system **play-event history**
(`pdmqryQueryPlayEvent`, range query), coalesce 0x38-byte events into 0x68-byte sessions, format a text
report. Config: `ActivityLog`, `HideUnknownUsers`.

### ui
Custom framebuffer UI. Recovered: `ui_buildDirListingRows` (file-browser rows; decodes `"<DIR>"`,
sorts, colorizes, resolves title names via `dbi_ns_getTitleName`), `ui_buildDetailsPanel`
(`71003e9290`, the item properties panel — Build ID / title IDs / timestamps / used-free-total sizes,
3-mode renderer). Rendering primitives (nvnflinger/font) not yet mapped.

### app
`app_buildApplicationDetails`: aggregates per-title info (ncm content-meta via
`ncm_queryContentMetaRecord`, pdm play stats, save enumeration, LFS size gated by `CalculateLFSSize`);
wraps work in `appletSetCpuBoostMode(1/0)`.

## 4. Switch services used (observed/inferred)
`fsp-srv` (all file I/O + save mounts + gamecard), `ncm` (content meta/records, install registration),
`es` (tickets), `ns` (title control/name, app records), `pdm` (play history), `account` (users),
`usb:ds` (MTP device), `nifm`/`ldn` (FTP + access point), `set` (settings), `applet` (CPU boost),
`csrng`/mbedTLS (NCA crypto: AES-128-XTS/CTR, RSA), plus `ssl`/libcurl/libssh2 (HTTP/FTP/SFTP sources).

## 5. Known gaps / next targets
1. **Install transport adapters** — the `File`/`InputStream` implementations bridging DBI0-USB
   `FILE_RANGE`, MTP, gamecard, and network into the (now-understood) installer; plus the
   `processContentRecord` internals (es ticket import + NSZ/XCZ decompress). *(Install core itself: done.)*
2. **UI rendering layer** (nvnflinger framebuffer + TTF; menu/event loop) — large, obfuscated.
3. **Network install sources** (HTTP `ApacheHTTP`/`URLList`, FTP, SFTP clients via curl/libssh2).
4. **NSZ/XCZ decompression** dispatch (zstd/lzma/bzip2/zlib) on the install side.
5. **Verify** the saves network-upload finding.
6. ~8,445 functions still `FUN_*` (mix of version-drifted library + obfuscated app code).
