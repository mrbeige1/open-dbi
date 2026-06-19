# Open-DBI milestones

Recalibrated to the project's *actual* state (see `STATUS.md` §4 for the live
"works vs. gated" list and `PROGRESS.md` for RE metrics). Earlier roadmap notes were
scattered across `STATUS.md` §4/§6; this file consolidates them.

The `open-dbi/` reimplementation already builds to a real `.nro` and has installed a
forwarder NSP from SD on real hardware end-to-end. So the milestones below start from
"SD install works" rather than treating it as a goal, and target the remaining gaps.

Each milestone lists an **exit criterion** (what makes it "done") so testers and
contributors agree on the bar. The new diagnostics from this change — structured SD
logging, the dry-run mode, and the debug-report bundle — are the instrument used to
verify every milestone (see `TEST-MATRIX.md`).

---

## M0 — Reliable NSP install from SD ✅ (done / hardening)
Install a standard NSP from `sdmc:/install.nsp` into NCM and have it appear and launch.
- **Status:** hardware-validated end-to-end (parse → decrypt → ncm placeholder/write/
  register → `ncmContentMetaDatabaseSet` → commit → ns `PushApplicationRecord`).
- **Remaining hardening:** broaden beyond the validated sample (more title types, sizes).
- **Exit criterion:** the `TEST-MATRIX.md` SD rows (base / update / DLC) all pass with a
  clean log and the title launches.

## M1 — NCA crypto correctness across keygens
The decrypt path (header XTS tweak, key-area ECB unwrap, section CTR IV, PFS0 offset)
is implemented to spec but parts are still marked `UNVERIFIED` in `source/crypto/nca.cpp`.
- **Goal:** confirm correct decrypt across master-key generations / `kaek` indices, both
  standard-crypto and titlekey (rights-id) content.
- **Exit criterion:** dry-run reports the correct `titleId`/CNMT for NSPs spanning several
  keygens and rights-id / no-rights-id, with `CheckHash` passing on real installs.
- **In progress:** `CheckHash` is implemented — a clean-room SHA-256 (`source/crypto/sha256.*`,
  host-validated vs FIPS 180-4 vectors) verifies each content NCA against the CNMT's recorded
  hash (and the filename naming invariant). Dry-run runs the check read-only and reports
  per-NCA PASS/FAIL; the installer aborts in the content-meta phase on a mismatch when
  `[Install] CheckHash` is set (default on). `.ncz` content is hashed from its reconstructed
  bytes (full NSZ verification lands with M3). The `UNVERIFIED` crypto markers are now exercised
  end-to-end by this check — confirming them is the remaining on-hardware step.

## M2 — USB install robustness
SD install works without USB; the `usb:ds` `BulkTransport` is the newer path.
- **Goal:** harden `usb:ds` transport + the DBI0 client for full streamed installs
  (timeouts, reconnect, large transfers) against the PC `dbibackend`.
- **Exit criterion:** the USB rows of `TEST-MATRIX.md` (incl. a large title) install
  cleanly and reproducibly.

## M3 — NSZ/NCZ correctness
`source/crypto/ncz.cpp` reconstructs NCAs from `.ncz` (zstd) to spec; the original DBI's
decompressor calling path is still **not located** (see `ARCHITECTURE.md` §5).
- **Goal:** validate reconstructed NCAs byte-match the original NCA, multi-codec coverage.
- **Exit criterion:** dry-run reports correct reconstructed sizes and the NSZ rows install
  and launch identically to their NSP equivalents.

## M4 — Cleanup / rollback on failed installs
Today a failed `installNsp` only `closeStorage()`s, leaving orphaned placeholders /
registered content. The new `InstallResult::failedPhase` + structured logging
(this change) are the prerequisite for safe rollback.
- **Goal:** on failure at any phase, delete created placeholders and unregister any
  content written in this attempt; never leave a half-installed title.
- **Exit criterion:** the "intentionally broken NSP" row leaves NCM unchanged (verified
  via dry-run before/after) and the log names the exact failed phase.

## M5 — UI / quality-of-life
`main.cpp` is a button-driven console harness; there is no real UI yet.
- **Goal:** a navigable file/title UI (list, details, install, saves) on the recovered
  framebuffer/event-loop design from `ARCHITECTURE.md` §ui.
- **Exit criterion:** install + saves browse driven entirely from on-screen UI.

## M6 — XCI/XCZ and other formats
- **Goal:** HFS0/XCI container support (and XCZ) through the same `ContentSource` +
  `Installer` seams; gamecard dump/install parity.
- **Exit criterion:** the XCI rows of `TEST-MATRIX.md` install and launch.
