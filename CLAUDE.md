# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this repository is

DBI is a homebrew application for the Nintendo Switch used to install `NSP`/`NSZ`/`XCI`/`XCZ` titles, browse files, and manage saves. This repo holds the distributable artifacts plus the PC-side companion script:

- `dbibackend` — a Python 3 script (no `.py` extension) that runs on a **PC** and serves local game files to DBI running on a Switch over USB. This is the only source code in the repo and the only file that can be meaningfully edited.
- `DBI.nro` — the compiled Switch homebrew binary (`data`/ELF-derived NRO). Opaque build artifact; not built from anything in this repo.
- `dbi.config` — the runtime configuration file that lives on the Switch SD card (`sdmc:/switch/DBI/`). Heavily commented INI; it documents every option DBI supports.
- `README.md` — the full end-user manual (also the bulk of the repo by size). Reference it for what config keys mean and how features behave on-device.

There is **no build system, test suite, linter, or package manifest.** Don't look for `make`, `npm`, `pytest`, etc.

## Running the backend

```bash
pip install pyusb            # also requires libusb installed on the system
./dbibackend                 # opens the Tkinter GUI, no files preloaded
./dbibackend <file_or_dir>…  # preloads files/dirs and auto-starts the server
```

When given directory arguments, the script recurses one level (files directly inside each dir). It always launches a Tkinter GUI; passing args also auto-starts serving.

## How `dbibackend` works (the protocol)

The script is a USB device-to-host server speaking DBI's custom protocol. Understanding the protocol is required before changing anything:

- **USB target:** Nintendo Switch is `idVendor=0x057E, idProduct=0x3000`. `connect_to_switch()` polls until the device appears, resets it, and resolves the IN/OUT bulk endpoints. On any `usb.core.USBError` mid-session the poll loop reconnects rather than crashing.
- **Framing:** every message is a 16-byte header `struct.pack('<4sIII', b'DBI0', cmd_type, cmd_id, data_size)`. `cmd_type` is one of `REQUEST/RESPONSE/ACK`; `cmd_id` is one of `EXIT(0)`, `LIST(3)`, `FILE_RANGE(2)` (`LIST_OLD(1)` is unused). Headers not starting with magic `DBI0` are skipped.
- **Main loop:** `poll_commands()` blocks reading 16-byte headers from the Switch and dispatches by `cmd_id`. The Switch is the initiator; the PC responds.
- **LIST:** sends the newline-joined list of filenames the Switch may request.
- **FILE_RANGE:** the Switch asks for a byte range of a named file; the PC streams it back in `BUFFER_SEGMENT_DATA_SIZE` (1 MiB) chunks. This is the hot path for installs.
- **`file_list`** is a global `dict` of `{filename: resolved Path}`. Filenames (not full paths) are the protocol identifiers, so duplicate basenames across folders collide by design.

## Conventions specific to this code

- State is shared through **module-level globals** (`in_ep`, `out_ep`, `file_list`, and the Tkinter widgets `text1`, `flist1`, `root`, the buttons). Functions `global`-declare what they mutate. Match this style rather than introducing classes/refactors unless asked.
- The server runs on a **daemon thread** (`do_start_server`) so the Tkinter mainloop stays responsive; never call blocking USB reads on the GUI thread.
- `LOG()` writes to the GUI text widget and self-trims at 1000 lines — use it, not `print`, for anything the user should see during a session (`print` output only reaches the launching terminal).
- All multi-byte protocol fields are **little-endian**; keep `struct` format strings consistent with the existing `<...>` layouts.

## Reverse-engineering effort (active)

This repo is being used to reverse-engineer `DBI.nro` toward an open-source, rebuildable NRO. Read
`PROJECT.md` (incl. the **legal/licensing** caveat) and `docs/` first; `docs/PROGRESS.md` is the live
dashboard, `docs/ARCHITECTURE.md` the subsystem map, `docs/RE-METHODOLOGY.md` the toolchain.

- **Ghidra project:** `~/DBI.gpr` (loaded via the Switch Loader as AARCH64; ~9,403 functions, ~905 named).
  Quit the Ghidra GUI before any headless run (it holds `~/DBI.lock`).
- **Tooling:** Ghidra 11.0 at `~/switch-re/ghidra_11.0_PUBLIC` (launch via `~/switch-re/start-ghidra.command`);
  devkitPro at `/opt/devkitpro`. Headless runs need `JAVA_HOME=/opt/homebrew/opt/openjdk@21/libexec/openjdk.jdk/Contents/Home`
  (the launch.properties override isn't honored in non-interactive shells).
- **Pipeline scripts:** `scripts/ghidra/` — `Inventory`, `ExportContext`, `SelectAppCandidates`,
  `DecompileTargets`, `HashLibrary`/`MatchToDBI` (FID), `ApplyNames` (takes a CSV arg). Working artifacts
  (decompiled C, FID corpus, agent outputs) live under `~/switch-re/exports/` (git-ignored).
- **Recovery loop:** `SelectAppCandidates.py` (rank unknowns by call-graph proximity to `dbi_*` anchors)
  → `DecompileTargets.py` (top batch) → one agent per function (writes `recovered/<addr>.md` +
  `names_frag/<addr>.csv`) → merge with conflict resolution → `ApplyNames.py`. Next target: the install path.
- **Key gotchas:** strings are polymorphically obfuscated (no global decoder — agents read decode blocks
  inline); FID library subtraction is capped (~21%) by a toolchain version gap (DBI newlib 4.5 vs installed 4.6).

## Editing `dbi.config`

`dbi.config` is consumed by DBI on the Switch, not by anything here. Keep its INI section structure (`[General]`, `[MainMenu]`, `[Install]`, `[MTP]`, etc.) and the leading-`;` comment-per-option style intact — the comments are the de-facto documentation and README sections mirror them.
