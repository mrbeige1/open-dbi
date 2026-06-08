# Recovered Symbols

Address → recovered name map produced by the Phase 3 decompile→agent pipeline.
Base image address is `0x7100000000` (Ghidra default for this NRO); the binary file offset is
`addr - 0x7100000000`. Names/types are **reconstructions** from decompiled C, not ground truth —
each carries the evidence it was derived from. Confidence: **H**igh / **M**edium / **L**ow.

> ⚠️ Provenance: this map is derived from decompilation of the closed-source binary. Keep it out of
> any public clean-room reimplementation repo unless the licensing question (see `../PROJECT.md`) is
> resolved.

## Architectural findings (read first)

1. **String obfuscation.** DBI stores many of its own string literals **encrypted** (per-string XOR
   against a 64-bit key, then a bit-rotate / nibble-swap pass), lazily decoded at first use and
   registered for cleanup via an atexit/`__cxa_guard` shim. The encrypted pool is reached through a
   **thread-local accessor** (`FUN_7100770f28`). Observed keys so far: `0x9b8fb543c1f1abf9`,
   `0x35733d37eb615dc1`, `0x11999b45fb657bfb`, `0x6b3b21e79b9191f1`, `0x4d7959afff65e3d9`.
   **Consequence:** plaintext-string triage drastically *undercounts* app code (only 12 of 9,403
   functions tagged). A static **string deobfuscator** (Phase 1.5, see `RE-METHODOLOGY.md`) is the
   highest-leverage next tool — it will both improve triage and give every future agent real strings.
2. **Global singleton.** `dbi::globalContext()` (`FUN_710018a9e0`, ~225 callers) returns the app
   context; `+0x60` = the file/fs manager sub-object, `+0x150` = the time source. It threads through
   nearly every subsystem.
3. **Config access** is centralized: `config::getString(section,key,default)` and
   `config::getBool(...)`. All folder paths come from the `[General]` section with the documented
   defaults (`DumpsFolder`, `SavesFolder`, `LogsFolder`, `TempFolder`), always normalized to a
   trailing `/`.
4. **⚠️ Needs verification — possible network upload of saves.** `dbi::saves::uploadBackupsOverNetwork`
   (`FUN_710014e890`), when called with bit0 set, decrypts an obfuscated **host/path/token** and
   builds HTTP-style request fields, iterating over local save backups. This *may* be a save-export
   or telemetry path to a hardcoded endpoint. Treat as a hypothesis from one decompilation pass —
   confirm by decoding the obfuscated strings and tracing the request builder before asserting it.

## Recovered functions — the 12 confirmed-app targets

| Address | Proposed name | Subsystem | Conf | Purpose |
|---------|---------------|-----------|:----:|---------|
| `710060d100` | `dbi::dump::buildNspDump` | dumps | H | Core NSP/PFS0 builder: streams a title's NCAs + CNMT NCA + tickets/certs into a PFS0 archive, returns sanitized output path. |
| `71001535f0` | `dbi::saves::backupSaveToZip` | saves | H | Mounts a title's save FS and writes a ZIP to `SavesFolder` with `.dbi_save_info.ini` + optional 512-byte `.dbi_save_extra`. |
| `7100159d60` | `dbi::saves::refreshBackupList` | saves | H | Rebuilds + sorts the in-memory `vector<BackupEntry>` cache from the saves folder; sets loaded flag. |
| `710014e890` | `dbi::saves::uploadBackupsOverNetwork` | saves/net | M | Enumerates backups; bit0 path decrypts an obfuscated endpoint and builds HTTP requests (see finding #4). |
| `710060ab30` | `dbi::dump::buildContentDumpPathAndEnqueue` | dumps | H | Builds dump dest path (by name), allocates a 256-byte dump job, enqueues to worker. |
| `7100604d20` | `dbi::dump::buildGamecardDumpPathAndEnqueue` | dumps | H | Same as above but keyed by `{titleId,contentMeta,tag}` (dump-by-title-id). |
| `710018c400` | `dbi::dump::resolveContentDumpTargets` | dumps | H | Walks content records, filters by meta-type `0x10000..0x10005` + per-NCA-section flags, emits output paths. |
| `71003b3a90` | `dbi::dump::addExeFsEntriesToDumpJob` | dumps | M | Queues a title's ExeFS sub-files (`exefs/<name>`) into a dump job via an RB-tree merge. |
| `710036d2e0` | `dbi::dump::buildDumpEntryList` | dumps/ui | M | Builds the selectable dump-target list with per-row action/destructor callbacks. |
| `7100294280` | `dbi::ftp::formatListingEntry` | ftp | H | Formats one FTP listing line: MLST/MLSD facts or `ls -l`, CRLF-terminated, into a 1 MiB session buffer. |
| `71001ae4a0` | `dbi::fs::TempPathProvider::ctor` | fs | M | Resolves `General/TempFolder` (default `sdmc:/switch/DBI/temp/`), registers/mounts via a resolver vtable. |
| `71005f8800` | `dbi::log::writeLog` | log | H | Mutex-guarded log emitter; `<msg>@<timestamp>`; gated by `General/LogEvents`; fans out to file + console/MTP sinks. |

## Recovered shared helpers (high propagation value)

These are the most-called functions; naming them clarifies thousands of call sites. Several are the
"high-caller unknowns" from the triage, now identified.

| Address | Name | Kind | ~Callers |
|---------|------|------|---------:|
| `7100813700` | `memcpy` / `string::_M_construct` | libc/libstdc++ | 1628 |
| `7100770f28` | `dbi::util::tlsBase` (obfuscated-string pool accessor) | app/runtime | 2373 |
| `71007b33a0` | `operator new` / `allocate(size)` | libstdc++ | 1035 |
| `thunk_FUN_7100820000` | `operator delete` | libstdc++ | — |
| `71007e6840` | `std::string::append` | libstdc++ | 228 |
| `710018a9e0` | `dbi::globalContext()` | app | 225 |
| `7100839460` / `71008394e0` | `Mutex::lock` / `Mutex::unlock` | runtime | 223 |
| `71000f6730` | `std::string::starts_with(prefix,len)` | libstdc++ | 62 |
| `7100054b00` | `dbi::config::getBool(section,key,default)` | app | 43 |
| `71000f6540` | `std::string::ends_with(char)` | libstdc++ | 31 |
| `7100057bc0` | `dbi::config::getString(out,settings,section,key,default)` | app | 27 |
| `71005fdbb0` | `std::string::assign(begin,end)` | libstdc++ | — |
| `71005fe210` | `std::string::assign(len)` | libstdc++ | — |
| `71007e63a0` | `std::string::reserve` | libstdc++ | — |
| `71001a3420` | `dbi::system::getCurrentTime()` | app | — |
| `71001aeed0` | `dbi::time::nowId()` (128-bit time/id) | app | — |

## Recovered subsystem helpers

**PFS0/NSP builder family (dumps):**
| Address | Name |
|---------|------|
| `7100601ed0` | `Pfs0Builder::ctor` |
| `7100602050` | `StreamWriter::init(out,src,count,chunkSize)` |
| `7100603630` | `Pfs0Builder::addStreamEntry` |
| `7100602c30` | `Pfs0Builder::addNcaEntry` |
| `7100603330` | `Pfs0Builder::addTicketEntry` |
| `710007c720` | `FileEntry::create(out,fileMgr,relPath,…)` |
| `710010bd30` | `FileManager::mkdir` |
| `710010b4d0` | `FileManager::registerFile` |
| `7100622160`/`710062d1c0`/`71006009c0` | archive finalize / write-header / destroy |
| `71000a1a50` | `shared_ptr::release` |
| `71006746a0` | hex id → string formatter (NCA-id filenames) |

**Saves helpers:**
| Address | Name |
|---------|------|
| `710019a100` | `dbi::saves::readSaveExtraData` (fills 512-byte `.dbi_save_extra`) |
| `710013cd10` | `dbi::saves::getAccountString` |
| `7100099e20` | `dbi::ns::getTitleName(titleId)` (NS control data) |
| `710016dfb0` | `dbi::time::format(ts,mode)` (0=human, 1=filename) |
| `710010bb80`,`7100076c30`,`71000771a0`,`7100077280` | zip writer: create / beginEntry / writeEntry / endEntry |

## Recovered data structures

```c
// --- saves ---
struct SaveTarget {        // backupSaveToZip param
    u8  space;             // +0x08 storage location enum
    u8  index;             // +0x09
    u64 titleId;           // +0x28
};
struct BackupEntry {       // 0x28 each; cached in global vector _DAT_7100afec60/68
    std::string filename;  // +0x00 (SSO buffer @ +0x10)
    void*       parsedMeta;// +0x20 (freed by FUN_71007f9c60; parsed .ini / icon)
};
// .dbi_save_info.ini : [General] TitleId, TitleName, BackupDate, Account, Space
// .dbi_save_extra    : opaque 0x200-byte record (likely FsSaveDataExtraData)

// --- dumps ---
struct DumpJob {           // 0x100 (256B); vtable from PTR_DAT_7100af5490
    u32 stateFlags;        // +0x58
    u64 timeIdLow;         // +0x78 (timestamp & flag byte)
    u64 timeHigh;          // +0x80
    // +0xb8/+0xc0 self-referential intrusive list head
};
struct ContentRef { u64 id; u64 versionFlags; };          // 0x10, vector element
enum  ContentMetaType : u32 { /* base 0x10000; maps to ncm meta types */ };
struct NcaContentRecord {  // 0x48; vector element for buildNspDump param_6
    u64 id;                // +0x00
    u8  typeIndex;         // +0x08
    std::shared_ptr<NcaReader> readerA; // +0x20 ptr / +0x28 ctrl
    std::shared_ptr<NcaReader> readerB; // +0x30 ptr / +0x38 ctrl
    u8  flag;              // +0x40
};
struct TicketRecord { void* data; void* ctrl; u32 a; u64 b; };  // 0x20

// --- ftp / fs / log ---
struct FtpSession {        // formatListingEntry param
    long server_time;      // +0x2050
    int  list_mode;        // +0x205c 1=MLST 2=MLSD 3=LIST 4=LIST+space
    uint fact_mask;        // +0x2060 bit0 Type,1 Size,2 Modify,3 Perm,4 UNIX.mode
    char scratch[0x100F88];// +0x2088 (1 MiB output buffer)
    long cursor;           // +0x103090
};
struct LogContext {
    void*  line_buf;       // +0x18 / cap +0x20
    u8     active;         // +0x58
    char*  msg_ptr;        // +0x60 / len +0x68
    u8     enabled_known;  // +0x80
    u8     enabled;        // +0x81
    u8     written;        // +0x82
    /* mutex @ +0x84 */
};
```

## How this was produced
`scripts/ghidra/DecompileTargets.py` exported the 12 functions' C; four parallel recovery agents
analyzed them; results consolidated here. Names are applied back into the Ghidra DB via
`scripts/ghidra/ApplyNames.py` + `scripts/ghidra/names.csv` so subsequent decompilation reads cleanly.

## Batch 2–3 recovered functions (top app-candidates by call-graph proximity)

Selected via `SelectAppCandidates.py` (unknown funcs calling app anchors). Full per-function analyses
in `~/switch-re/exports/recovered/<addr>.md`. Names + ~70 callees applied to the DB.

| Address | Name | Subsystem | Conf |
|---------|------|-----------|:----:|
| `7100055a10` | `dbi_config_loadAndApply` | config | M-H |
| `71003f8c30` | `dbi_app_buildApplicationDetails` | app/fs | M |
| `7100081d20` | `dbi_pdm_buildPlayEventReport` | pdm | H |
| `71000872c0` | `dbi_pdm_buildActivityLogReport` | pdm | H |
| `710020c150` | `dbi_mtp_responderRun` | mtp/usb | H |
| `7100318550` | `dbi_net_startAccessPointFtp` | network/ftp | M-H |
| `71006075e0` | `dbi_forwarder_buildNsp` | forwarder/install | M-H |
| `710061c280` | `dbi_nca_buildIvfcLevels` | install/nca | H |
| `71005e6f00` | `dbi_ui_buildDirListingRows` | ui/fs | H |
| `71003e9290` | `dbi_ui_buildDetailsPanel` | ui | M-H |
| `710015c9e0` | `dbi_saves_deleteSelectedSaves` | saves | H |
| `7100283690` | `dbi_saves_extendSaveFs` | saves | H |
| `710027d1e0` | `dbi_saves_copySaveExtraData` | saves | H |
| `710024cb90` | `dbi_saves_applySaveExtraData` | saves | H |

**New cross-cutting anchors named:** `dbi_strpool_get` (`710067ada0`, obfuscated-string-pool fetch,
~562 callers), `dbi_util_formatBytes` (`71000f7910`), `dbi_reportSink_appendString`/`appendLine`
(`71005f8500`/`71005f8590`), `dbi_config_parse` (`71005f7c10`), `dbi_ncm_queryContentMetaRecord`
(`71001a1320`), `dbi_saves_readSaveExtraData`/`writeSaveExtraData` (`710019a100`/`710019a180`),
`Pfs0Builder`/`StreamWriter` family extensions, plus `lib_jpeg_encode`, `lib_mt19937_genrandRefill`,
`lib_image_*`.

**Confirmed via recovery:** the **QR-code access point** and **MTP/USB responder** features; DBI's
**forwarder** (shortcut-NSP) and **pdm Activity-Log** subsystems; the **NCA IVFC/RomFS** build path.
