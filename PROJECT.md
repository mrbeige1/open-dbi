# DBI — Open-Source Reconstruction Project

> ⚠️ **Status: early / documentation phase.** This repository is an effort to reverse-engineer
> the closed-source Nintendo Switch homebrew **DBI** (by *rashevskyv*) and reconstruct it as
> open, rebuildable C++/libnx source.

## What's here

**Committed (this project's work):**
| Path | What it is |
|------|-----------|
| `open-dbi/` | The clean-room reimplementation (builds to `.nro`) |
| `docs/` | Reconstructed specs & RE methodology |
| `scripts/ghidra/` | Ghidra automation used to study the binary |
| `README.md` · `PROJECT.md` · `CLAUDE.md` | Project docs |

**Local-only — git-ignored, NOT redistributed** (upstream DBI, copyrighted):
| Path | What it is |
|------|-----------|
| `DBI.nro` | The original compiled binary — RE input only |
| `dbibackend` | Upstream PC-side USB host script |
| `dbi.config` | Upstream sample config |
| `README.dbi-upstream.md` | The original DBI user manual |

## ⚖️ Legal / licensing — read before publishing anything

DBI is **closed-source freeware**. A decompilation-derived reimplementation is a **derivative work**.
Re-publishing it under an open-source license without the original author's permission is a real
copyright risk. Before this repo is made public:

1. **Get the original author's permission / a license grant** (the clean fix), OR
2. Commit to a **clean-room** process: the `docs/` specs are written from observable behavior only
   (README, protocol, the `dbibackend` script) and form the boundary — reimplementation code must be
   written against the specs, *not* copied from decompiled output.

**Never commit:** `DBI.nro`, extracted library/binary blobs, decompiled C, or any console keys
(`prod.keys`/`title.keys`). The `.gitignore` enforces this.

## Documentation set (`docs/`)

| Doc | Contents | Status |
|-----|----------|--------|
| **`STATUS.md`** | **Start here** — handoff: state, how to build/test, how to resume | living |
| `PROTOCOL.md` | USB (`DBI0`) + MTP wire format & command flows | drafted |
| `CONFIG.md` | Every `dbi.config` key, default, effect | drafted |
| `FEATURES.md` | Behavioral spec distilled from the README (clean-room boundary) | drafted |
| `FORMATS.md` | NSP/NSZ/XCI/XCZ/NCA/ticket layout notes | drafted |
| `RE-METHODOLOGY.md` | Toolchain, library-subtraction, reproducible Ghidra setup | drafted |
| `ARCHITECTURE.md` | Subsystem map + control/data flows (from 99 recovered fns) | drafted |
| `SYMBOLS.md` | Address → recovered-name map (~99 app + ~806 lib) | drafted |
| `PROGRESS.md` | Quantitative dashboard (classification + batches) | living |
| `BUILD.md` | devkitPro build steps for the reconstructed `.nro` | drafted |

**Reconstruction (Phase 4) started:** `open-dbi/` is a clean-room devkitPro project that **builds to
`open-dbi.nro`** — modules: DBI0 protocol, config parser, save-backup format (from the `docs/` specs).
