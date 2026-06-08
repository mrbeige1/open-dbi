# Open-DBI

An **open-source, clean-room reconstruction** of [DBI](../PROJECT.md) for the Nintendo Switch,
built with devkitPro + libnx.

> ⚠️ **Clean-room rule.** Code here is written **only from the behavioral/format specs** in
> [`../docs/`](../docs) (`PROTOCOL.md`, `CONFIG.md`, `FORMATS.md`, `FEATURES.md`) — which describe
> *observable behavior*, not decompiled code. The decompilation under `~/switch-re/` and
> `../docs/SYMBOLS.md` is used **only to understand** the original; its code is never copied here.
> See the licensing note in [`../PROJECT.md`](../PROJECT.md) before publishing.

## Status: foundation (builds to an NRO)

This is an early vertical-slice scaffold, not yet a usable installer. The `main` is a console
self-test that exercises the reconstructed modules.

| Module | Source | Spec | State |
|--------|--------|------|-------|
| DBI0 USB protocol framing | `source/proto/dbi0.h` | `docs/PROTOCOL.md` | header + pack/parse ✅ |
| DBI0 USB client (Switch side) | `source/usb/` | `docs/PROTOCOL.md` | LIST + FILE_RANGE over abstract transport ✅ |
| `dbi.config` parser | `source/config/` | `docs/CONFIG.md` | parse + getString/Bool/Int ✅ |
| Save-backup format | `source/saves/` | `docs/FORMATS.md`,`SYMBOLS.md` | `.dbi_save_info.ini` + filename ✅ |
| ContentSource seam + PFS0/NSP reader | `source/io/`,`source/fs/` | `docs/ARCHITECTURE.md`,`FORMATS.md` | ✅ |
| Install state machine | `source/install/installer.cpp` | `docs/ARCHITECTURE.md` | 5-phase sequence over `InstallBackend` ✅ |
| **Real libnx ncm + es backend** | `source/install/ncm_backend.cpp` | libnx ncm + es IPC | placeholder/write/register/commit + ImportTicket ✅ |
| File `ContentSource` (install from SD) | `source/io/file_source.h` | — | ✅ |
| **AES-128 (ECB/CTR/XTS)** | `source/crypto/aes.cpp` | FIPS-197 / IEEE-1619 | ✅ **host-validated vs NIST ECB+CTR vectors** |
| **`prod.keys` parser** | `source/crypto/keyset.cpp` | hactool key format | ✅ (header_key, kak_*, titlekek, master_key by gen) |
| NCA header/section decrypt | `source/crypto/nca.cpp` | switchbrew NCA | ✅ **hardware-validated** vs a real 1.6 GB NSP (header XTS + kak unwrap + CTR section all correct; recovered a valid Meta NCA) |
| CNMT parser | `source/fs/cnmt.h` | switchbrew CNMT | ✅ **host- and hardware-validated** (decrypted a real patch CNMT: titleId 0100c9a00ece6800, 10 contents w/ sane types/sizes) |
| CNMT extract wired into install | `source/install/installer.cpp` | — | ✅ (cnmt.nca → `ncaExtractCnmt` → `setContentMeta`, gated on a `Keyset`) |
| `setContentMeta` build ncm meta blob | `source/install/meta_builder.cpp` + `ncm_backend.cpp` | switchbrew ncm | ✅ implemented (CNMT→`NcmContentMetaHeader`+`NcmContentInfo`[+Meta record]→`ncmContentMetaDatabaseSet`); **real-install test pending** |
| **Full SD install** (real ncm/es) | `installer.cpp` + `ncm_backend.cpp` | — | ✅ **hardware-validated** — installed a real NSP on a Switch (placeholder→write→register→setContentMeta→commit, `ok=1`) |
| USB transport (libnx usb:ds) | `source/usb/usbds_transport.cpp` | USB spec + libnx | ✅ **hardware-validated** vs real `dbibackend` (LIST + FILE_RANGE; received bytes verified = `PFS0`) |
| **NSZ/XCZ decompression** | `source/crypto/ncz.cpp` | nsz NCZ format + libzstd | 🟡 implemented (streaming zstd → per-section AES-CTR re-encrypt → ncm); **hardware test pending** |
| framebuffer UI, other features | — | _RE done; not reimplemented_ | ⏳ |

### What works vs. what's gated
Content **write/register/ticket-import/commit** use real ncm/es and need no keys. The crypto **core
is built and host-validated** (AES vs NIST ECB+CTR; CNMT parser vs a known blob). The NCA decrypt is
**assembled and wired** (NSP → cnmt.nca → decrypt → CNMT). Two items remain to a complete SD install:
1. **Validate the NCA decrypt against a real NCA** (the XTS sector tweak, section CTR IV, and PFS0
   offset inside the section are spec-implemented but marked UNVERIFIED — need `prod.keys` + a Meta NCA).
2. **Build the ncm meta blob** in `setContentMeta` from the parsed CNMT (`NcmContentMetaHeader` +
   `NcmContentInfo` records) and call `ncmContentMetaDatabaseSet`.

Note: DBI uses spl hardware keyslots; `open-dbi` takes the software-`prod.keys` route (hactool/nstool
style), which needs no spl.

## Build
```sh
export DEVKITPRO=/opt/devkitpro
make
```
Produces `open-dbi.nro`. See [`../docs/BUILD.md`](../docs/BUILD.md) for toolchain setup.

## Layout
```
source/proto/    DBI0 USB wire protocol (clean-room from PROTOCOL.md)
source/config/   dbi.config INI parser (from CONFIG.md)
source/saves/    save-backup container format (from FORMATS.md/SYMBOLS.md)
source/main.cpp  console self-test harness
```
