# DBI Reverse-Engineering — Status & Handoff

_Single-page handoff for picking this project up cold. Pair with `ARCHITECTURE.md` (how DBI works),
`PROGRESS.md` (live metrics), and `RE-METHODOLOGY.md` (toolchain)._

## 1. What this project is
Two parallel efforts toward an open-source, rebuildable DBI:
- **Understand** — reverse-engineer the closed-source `DBI.nro` (Ghidra) into a documented architecture.
- **Reimplement** — `open-dbi/`, a clean-room devkitPro/libnx project written **only from the `docs/`
  specs** (never from decompiled code), that builds to a real `.nro`.

⚠️ **Legal:** a decompilation-derived reimplementation re-published openly is a derivative of
closed-source freeware. Resolve licensing before publishing (author permission, or keep the strict
clean-room boundary). Never commit `DBI.nro`, decompiled C, or `prod.keys`. See `../PROJECT.md`.

## 2. Current state (snapshot)
- **Ghidra DB** `~/DBI.gpr`: 9,403 functions, **~965 named** (~150 `dbi_*` app + ~815 library).
- **Subsystems mapped:** saves, dumps, **install (full pipeline)**, forwarder, mtp, ftp, network/AP,
  pdm/activity-log, config, fs, ui, log, app, ncm wrapper tier, ticket parse/format.
- **`open-dbi/` builds** to `open-dbi.nro` (~286 KB) with a console self-test.
- **Validated code:** AES-128 (NIST ECB+CTR vectors) and the CNMT parser — verified on host **and
  confirmed on real Switch hardware** via the NRO self-test (AES enc/dec PASS, CNMT PASS, the full
  install state-machine sequence ran green against the logging backend).
- **USB protocol — hardware-validated end-to-end:** `open-dbi`'s usb:ds transport + DBI0 client
  connected to the real PC `dbibackend`, ran `LIST` (got the real filename) and `FILE_RANGE` (got 512
  bytes whose first 4 bytes = `PFS0`, the correct NSP magic). The reimplemented protocol stack works.
- **NCA crypto — hardware-validated:** the B-mode test decrypted a real 1.6 GB NSP's Meta NCA on-device
  (prod.keys parse → header AES-128-XTS → keygen/kaek → key-area ECB unwrap → section AES-128-CTR →
  PFS0 → CNMT). Recovered a structurally valid patch CNMT (titleId 0100c9a00ece6800, type 0x81, 10
  content records with consistent types/sizes).
- **FULL INSTALL — hardware-validated, end to end:** `open-dbi` installed a forwarder NSP on a Switch
  and **it appears on the home menu and launches**. Complete flow: parse → decrypt → ncm
  placeholder/write/register → `ncmContentMetaDatabaseSet` → commit → **ns `PushApplicationRecord`**
  (cmd 16, custom IPC). The clean-room reimplementation performs a real, visible install.

## 3. Environment (all on this machine)
| Thing | Location |
|------|----------|
| Ghidra 11.0 | `~/switch-re/ghidra_11.0_PUBLIC` — launch via `~/switch-re/start-ghidra.command` |
| devkitPro | `/opt/devkitpro` (JDK at `/opt/homebrew/opt/openjdk@21/...`) |
| RE scripts | `scripts/ghidra/` |
| Working artifacts (git-ignored) | `~/switch-re/exports/` (decompiled C, FID corpus, agent outputs, name CSVs) |
| Reconstruction | `open-dbi/` |

**Headless Ghidra rule:** quit the Ghidra GUI first (it holds `~/DBI.lock`); set
`JAVA_HOME=/opt/homebrew/opt/openjdk@21/libexec/openjdk.jdk/Contents/Home`.

## 4. `open-dbi` — what works vs. what's gated
**Builds & compiles, all clean-room:** DBI0 USB protocol + Switch-side client, `dbi.config` parser,
save-backup format, `ContentSource` seam, PFS0/NSP reader, the 5-phase `Installer`, a **real libnx
ncm + es backend** (placeholder/write/register/commit + ImportTicket), `FileSource` (install from SD),
AES-128 (ECB/CTR/XTS), SHA-256 (`source/crypto/sha256.*`, host-validated vs FIPS 180-4),
`prod.keys` parser, NCA header/section decrypt, CNMT parser, the cnmt.nca → CNMT →
`setContentMeta` wiring, and `CheckHash` (SHA-256 content verification in dry-run + installer).

**Needs no keys (should work once tested):** parse NSP, write/register NCAs, import tickets, commit.

**Gated / unverified:**
1. **NCA decrypt vs a real NCA — VALIDATED on hardware (keygen 0, kaekIndex 0, standard crypto).**
   `source/crypto/nca.cpp` (header XTS, key-area ECB unwrap, section CTR, PFS0 offset) decrypted a
   real base-game Meta NCA on-device and extracted a coherent CNMT (titleId `01d05aeff2ee0000`,
   type 0x80, 2 content records); SHA-256 (oneshot+stream) and CNMT hash-capture also PASS on-device.
   The former `UNVERIFIED` markers are now confirmed for this path. **Remaining (M1 hardening):**
   widen across other keygens / kaek indices and titlekey (rights-id) content.
2. **ncm meta blob** — `NcmBackend::setContentMeta` receives the decrypted CNMT but still must build
   the `NcmContentMetaHeader` + `NcmContentInfo` record array and call `ncmContentMetaDatabaseSet`.
3. **`usb:ds` transport** — the DBI0 client runs over an abstract `BulkTransport`; the libnx
   `usb:ds` implementation isn't written (SD-card install via `FileSource` works without it).
4. **NSZ/XCZ decompression** — still **not located** in `DBI.nro` (see §6).

## 5. How to build & test `open-dbi` on a real Switch
```sh
cd open-dbi && export DEVKITPRO=/opt/devkitpro && make      # -> open-dbi.nro
```
1. Copy `open-dbi.nro` to the SD card (`/switch/`), launch via hbmenu. The console self-test prints
   the AES/CNMT/config/install-sequence results — **confirm AES & CNMT say PASS on-device**.
2. **To exercise a real SD install** (once items 4.1–4.2 are done): put `prod.keys` at
   `sdmc:/switch/prod.keys` and a test NSP at `sdmc:/install.nsp`; wire a small entry that does:
   `crypto::Keyset ks; ks.load("sdmc:/switch/prod.keys"); FileSource nsp("sdmc:/install.nsp");
   NcmBackend be; Installer i(be); i.setKeyset(&ks); i.installNsp(nsp, StorageId::SdCard);`
   (a stub of this is in `main.cpp`'s self-test using a logging backend — swap in `NcmBackend`).
3. **To test the DBI0-USB path:** implement the `usb:ds` `BulkTransport`, then drive the built NRO
   with the existing `../dbibackend` host script and confirm LIST + chunked FILE_RANGE.

> Validate on hardware you own, with your own dumps/keys. Don't commit keys or copyrighted content.

## 6. Resuming the RE (Track B)
The recovery loop (all scripts in `scripts/ghidra/`):
```
SelectAppCandidates.py  (rank unknowns by call-graph proximity to dbi_* anchors)
  -> DecompileTargets.py (writes ~/switch-re/exports/decompiled/<addr>.c for targets.txt)
  -> one agent per function (writes recovered/<addr>.md + names_frag/<addr>.csv)
  -> merge name fragments (majority vote + override map) -> ApplyNames.py <csv>
```
Also: `FindInstallCallers.py <prims.csv>` (find callers of a primitive set),
`FindStringXrefs.py` (string→function), `HashLibrary.py`/`MatchToDBI.py` (FID library subtraction).

**Highest-value unfinished RE targets:**
- **NSZ/XCZ decompressor** — find callers of the zstd/lzma entry points (FID didn't name them due to
  version drift; try xrefs from the `NczSection`/decompression error strings, or BSim).
- **es ticket import** path (`dbi_ticket_parse` @`710018c2d0` + `splCrypto*`) — confirms titlekey decrypt.
- **UI render layer** (nvnflinger framebuffer + TTF; the menu/event loop) — large, obfuscated.
- ~8,400 functions still `FUN_*` (mix of version-drifted library + obfuscated app code).

## 7. Known caveats
- **String obfuscation:** DBI encrypts its own string literals (polymorphic XOR+rotate via a TLS
  pool, `dbi_strpool_get`). No global decoder — agents read decode blocks inline. Exact string values
  often unrecoverable statically.
- **FID ceiling (~21%):** library subtraction is capped by a toolchain version gap (DBI newlib 4.5 vs
  installed 4.6 / GCC 15).
- **Two functions were reclassified** mid-recovery (`71001e01c0` = status log, not NCA writer;
  `7100123330` = metadata reader, not ticket/decompress) — agents declined to mislabel them. Trust the
  `recovered/*.md` notes over earlier guesses.

## 8. Doc index
`PROJECT.md` · `ARCHITECTURE.md` · `PROGRESS.md` · `PROTOCOL.md` · `CONFIG.md` · `FEATURES.md` ·
`FORMATS.md` · `SYMBOLS.md` · `RE-METHODOLOGY.md` · `BUILD.md` · `MILESTONES.md` ·
`TEST-MATRIX.md` · `open-dbi/README.md`
