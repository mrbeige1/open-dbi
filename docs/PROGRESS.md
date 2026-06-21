# RE Progress Dashboard

Quantitative state of the reconstruction. Update as phases advance.

## Function classification (of 9,403 total defined functions)

| Bucket | Count | % | How |
|--------|------:|--:|-----|
| Library — FID strict match | 1,182 | 12.6% | devkitA64 FidDb (libnx/libc/libstdc++/libsupc++/libm), 751 uniquely named |
| Library — string-only hint | 824 | 8.8% | `ExportContext.py` keyword heuristic (libstdc++ etc.) |
| **Library subtotal** | **2,006** | **21.3%** | |
| App — recovered & named (`dbi_*`) | 326 | 3.5% | Phase 3 decompile→agent pipeline |
| Library named (`lib_*` + libnx/std) | 916 | 9.7% | FID + agent-confirmed |
| **Still `FUN_*` (unknown)** | **8,098** | **86.1%** | version-drifted library + obfuscated-string app code |

> 965 named. **Install pipeline mapped** end-to-end: `dbi_install_run` (5-phase) + `parseContainer`
> (NSP=PFS0 / XCI=HFS0 → MetaKey RB-tree + content vector) + `resolveContent` (validate
> ContentMetaKeys vs 0x430 CNMT header; type tags `0x10000..6` = base/update/DLC) + `NcmAccessor`.
> **Phase 4:** `open-dbi/` builds to a real `.nro` with the **real libnx ncm + es backend**
> (placeholder/write/register/commit + ImportTicket) and a file/USB `ContentSource`.
> **Single remaining install blocker = NCA crypto** (`setContentMeta` needs the decrypted CNMT).

> 905 of 9,403 functions are now named in `~/DBI.gpr`. The `dbi_*` set (99) spans ~13 subsystems —
> see the subsystem map below. (The 824 string-only library hints from the early triage are *not*
> applied as DB names, so they still count under `FUN_*` here.)

### Subsystems with recovered anchors (batches 1–3, 27 main functions)
saves (backup/restore/delete/refresh/extend/copy+apply extra-data) · dumps (NSP/PFS0 builder,
enqueue, exefs, resolve-targets, entry-list) · install/nca (IVFC/RomFS tree, buildCtx) · forwarder
(shortcut-NSP w/ JPEG icon) · ftp (listing formatter, server start/run) · network (Wi-Fi AP + QR) ·
mtp (USB responder + container dispatch) · pdm (play-event + Activity Log reports) · config
(load/apply, getString/getBool) · fs (temp path, register special entries) · ui (dir-listing rows,
details panel) · log (writeLog, reportSink) · app (application details)

### App-candidate work-list (call-graph proximity to app anchors)
`SelectAppCandidates.py` found **716 high-confidence app functions** (unknowns that call
`dbi_globalContext` / `dbi_util_tlsBase` / config getters / Pfs0Builder etc.). This is the prioritized
recovery queue. Batch 2 recovered the top 4; ~712 remain (plus 2 to redo after a session-limit abort).

### Recovery batches
- **Batch 1 (12 fns):** saves backup/restore, dump/NSP builder, FTP listing, temp/fs, logging.
- **Batch 2 (4 of top-8 app candidates):** `dbi_saves_deleteSelectedSaves`, `dbi_net_startAccessPointFtp`
  (confirms the **QR-code access point** the README omitted), `dbi_forwarder_buildNsp` (new **forwarder**
  subsystem — builds shortcut NSPs with JPEG icons), `dbi_pdm_buildPlayEventReport` (new **pdm** subsystem
  — the Activity Log / play-history via `pdmqry*`). +21 callee names applied.
- **Batch 10 (15 app candidates, 45 names):** more queue drain. **ftp** internals
  (`dbi_ftp_dataConnClose`, `dbi_ftp_formatAddrLine`), **mtp** (`dbi_mtp_assertMatchingTransactionId`),
  **network** (`dbi_net_setNonBlocking`), **config** nested get/set/remove
  (`dbi_config_getNestedString`/`setNested3`/`removeEntryItem`, `dbi_config_importFromDir`), and a large
  family of **report/UI menu builders** (`dbi_report_buildUserAccountsReport`/`buildStaticSection`,
  `dbi_ui_buildSelectionConfirmReport`/`buildTitleActionMenu`, `dbi_report_opContext_acquire`/`_dtor`).
  (1 of 16 — `710050f610`, a 54 KB fn — failed to decompile and was skipped.)
- **Batch 8/9 (32 app candidates, ~89 names):** continued draining the ranked app-candidate queue.
  Confirmed **new subsystems**: **ftp** (`dbi_ftp_cmdList` — LIST/NLST/STAT with RFC-959 reply codes),
  **network/http** (`dbi_net_httpClient_ctor` — wraps a **libcurl** easy handle; `[General] ValidateSSL`,
  HTTP auth), and the **UI settings menu** (`dbi_ui_buildConfigBoolRow`, `dbi_ui_buildConfigThemeRow`,
  `dbi_ui_titleListView_construct`, file-browser `drawStatusBar`/`drawColumn`/`onSelect`). Plus more
  install glue (`dbi_install_registerContentFile`, `_resolveMetaContents`, `_buildContentFileViews`,
  `_confirmFreeSpaceAndPrompt`, `_registerContentPathById`, `_titleHasContentFiles`), saves
  (`dbi_saves_finalizeSaveEntry`, `_promptResizeSaveData`, `_addSaveBrowseEntry`), dumps
  (`dbi_dump_recryptContentHashed` — NCA header XTS re-encrypt + dual SHA-256; `dbi_dump_buildTicketNsp`),
  pfs0 (`dbi_pfs0_buildPartitionHeader`, `dbi_install_buildNcaSections`), and a family of report builders
  (`dbi_report_buildIdListReport`/`buildSummaryReport`/`dumpContentRecord`/`appendOkLine`).
- **Batch 7 (14 of top app candidates, 62 names):** large high-score app functions, mostly the
  **install / diagnostics-report cluster**. `dbi_report_buildCleanupReport` @`71000ce3b0` (the top-level
  "Cleanup" debug-report builder; orchestrates ~12 flag-gated sections), with section handlers
  `dbi_saves_reportBackupTitles`, `dbi_report_appendNandAppLine`, `dbi_install_reportContentRecordDiff`,
  `dbi_install_reportContainerContents`, `dbi_install_reportSourceContainers`; install glue
  `dbi_install_prepareTitleEntry`, `dbi_install_commitContentEntry`, `dbi_install_registerContentFiles`,
  `dbi_pfs0_addHashedContentFile`; saves `dbi_saves_buildSaveInfoReport`, `dbi_saves_resizeSaveFileAndReport`;
  and the first **UI** anchors `dbi_ui_fileBrowser_onSelect`, `dbi_ui_appList_rebuild`. (4 more in this
  wave were decompiled but deferred when the agent session limit hit: 71001b2bd0, 710015f9e0, 710011f780,
  710011c110.)
- **Batch 5/6 (9 fns, 33 names):** **es ticket import** + **app-side archive glue**.
  `dbi_ticket_getRightsId`, `dbi_ticket_wipeSignature` (confirms `[Install] WipeSignature`),
  `dbi_install_enqueueContentMetaEntry` (stages `.tik`/`.cert`/`.nca`); `dbi_7z_extract_named_entry` /
  `dbi_archive_list_entries` driving libarchive — which named ~10 real API entry points
  (`archive_read_new`, `archive_read_open_filename`, `archive_read_next_header`, `archive_read_close/free`,
  `archive_entry_pathname/size/filetype`). The app-side codec call uses libarchive's C++ wrapper
  (`lib_archiveCpp_statEntry`/`listEntries`); the NSZ/XCZ extension router is a still-unknown virtual caller.
- **Batch 4 (12 targets + 40 callees = 52 names):** the **NSZ/XCZ + archive decompression subsystem**,
  identified as DBI's bundled **libarchive 7-Zip reader**. `lib_archive_read_support_format_7zip`
  (registration), `lib_archive_7z_read_header`, `lib_7z_decode_block` (multi-codec dispatch) with per-codec
  wrappers `lib_zstd_decompressStream` (NSZ/XCZ, 7z method 0x4f71101), `lib_lzma_decodeStep`,
  `lib_bzip2_decompress`, `lib_inflate`, PPMd, `lib_7z_bcj2_decode` + BCJ branch filters; `lib_7z_setup_coder`
  (method-ID→codec table). DBI's own glue: `dbi_nca_decompressBlock` (bounded-output NCZ driver) and the
  separate HTTP path `dbi_net_decodeContentEncoding` (gzip/deflate). Found by string-pivot
  ("Zstd decompression failed", 7z method-ID dispatch) — the cluster is reached via an indirect codec table.
- **Batch 3 (11 fns):** `dbi_app_buildApplicationDetails`, `dbi_pdm_buildActivityLogReport`,
  `dbi_mtp_responderRun`, `dbi_fs_registerSpecialEntries`, `dbi_ui_buildDirListingRows`,
  `dbi_saves_copySaveExtraData`, `dbi_config_loadAndApply`, `dbi_saves_extendSaveFs`,
  `dbi_nca_buildIvfcLevels`, `dbi_ui_buildDetailsPanel`, `dbi_saves_applySaveExtraData`.
  98 names merged/applied (15 frags, majority-vote + override resolution).
- **Resolved earlier conflicts:** `71000f7910`=`dbi_util_formatBytes`; `71005f8500`=`dbi_reportSink_appendString`;
  `710067ada0`=`dbi_strpool_get` (the obfuscated-pool fetch, 562 callers — high propagation);
  `710019a180`=`dbi_saves_writeSaveExtraData`. Open tie: `710004f160` (libstdc++ string helper, low value).
- **Next:** re-run `SelectAppCandidates.py` (app-anchor set grew 99→ propagates), recover next batch.
  Highest-priority untouched area: the **install / USB-host / gamecard install** path (DBI's core).

### Name conflicts to resolve (two agents disagreed)
- `71005f8500`: `dbi_log_printf` vs `dbi_reportSink_appendString`
- `71000f7910`: `dbi_util_formatByteSize` vs `lib_operator_new_array`
- `710067ada0`: `dbi_i18n_getString` vs `dbi_strpool_getLocalized` (both ~ "localized string fetch")

## Key constraint discovered
**Toolchain version mismatch.** `DBI.nro` (Nov 2024) was built with **newlib-4.5.0.20241231** + an
older GCC; the installed devkitA64 is **GCC 15.2.0 / newlib-4.6.0.20260123** (2025–26). FID matching
needs near-exact code, so library match rate is throttled (~13% of the 5,569-hash corpus matched;
loose hash-only matching added 0, confirming true code divergence rather than collision). libnx — the
library DBI bundles itself — matched only ~27% of its corpus.

**Implication:** pure FID subtraction tops out around ~21% here. Closing the gap needs either the
period-correct toolchain (hard to obtain) or **version-independent** techniques.

## The version-independent lever (next)
DBI **obfuscates its own string literals** (XOR + bit-rotate via a TLS pool — see `SYMBOLS.md` finding
#1). This is *why* the string triage only tagged 12 app functions: app code references *encrypted*
strings. A **static string deobfuscator** (Task 8 / Phase 1.5) will decode the pool and back-annotate
plaintext; re-running `ExportContext.py` afterward should reclassify a large share of the 7,385
"unknown" as app (or library), independent of toolchain version. Combined with agent classification on
the residual, this is the path to separating DBI's own code.

## Artifacts
- Ghidra project: `~/DBI.gpr` (793 names applied: 42 recovered-app + 751 lib_*)
- FID corpus: `~/switch-re/fid/libhashes.jsonl` (9,253 functions, 5,569 unique hashes)
- FID matches: `~/switch-re/fid/matches.csv`
- Function context/triage: `~/switch-re/exports/functions_context.jsonl`
- Scripts: `scripts/ghidra/{Inventory,ExportContext,DecompileTargets,ApplyNames,HashLibrary,MatchToDBI}.py`
