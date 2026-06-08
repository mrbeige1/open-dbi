# Reverse-Engineering Methodology & Setup

This is the reproducible procedure for turning `DBI.nro` into an annotated, eventually-rebuildable
project. It also contains the **install checklist** for the tools you need.

---

## 0. Install checklist (do these first)

| Tool | Purpose | Install |
|------|---------|---------|
| **Ghidra** (≥ 11.x) | Primary decompiler/disassembler | https://ghidra-sre.org |
| **Ghidra Switch Loader** (`Adubbz/Ghidra-Switch-Loader`) | Load NRO/NSO (MOD0, segments, relocs, dynamic imports) | GitHub release matching your Ghidra version |
| **devkitPro + devkitA64 + libnx + switch-tools** | Toolchain to (a) generate library signatures and (b) rebuild the `.nro` | https://devkitpro.org/wiki/Getting_Started (`pacman -S switch-dev`) |
| **pyhidra** *or* **ghidra_bridge** | Script Ghidra from Python (batch export + apply annotations) | `pip install pyhidra` / `pip install ghidra_bridge` |
| **rizin + Cutter** *(optional, free)* and/or **IDA Pro + FLIRT** *(optional, paid)* | Alternate decompiler + library signature matching | rizin.re / hex-rays |
| **Diaphora** *or* **BinDiff** | Diff binary against reference library builds to auto-name functions | GitHub / zynamics |
| **hactool / nstool** | Parse NSP/XCI/NCA/ticket to understand & test the installer | GitHub (`SciresM/hactool`, `jakcron/nstool`) |
| **PyUSB + libusb** | Run the existing `dbibackend` host for protocol validation | `pip install pyusb` + system libusb |

Reference (not installed): the **switchbrew wiki** for `ncm`, `es`, `ns`, `fsp-srv`, NCA/PFS0/HFS0/ticket layouts.

---

## 1. Measured facts about `DBI.nro` (baseline)
- AArch64, **fully stripped** of normal symbols.
- Segments (from the NRO header): `.text` `0x0..0x83b000` (~8.4 MB code), `.rodata`
  `0x83b000..0xae0000` (~2.7 MB), `.data` `0xae0000..0xafe000` (~120 KB), `.bss` ~2.4 MB.
- An **ASET** asset blob follows the NRO body at `0xafe000` (icon + NACP + romfs).
- Build: devkitA64 + libnx, GCC, newlib 4.5.0, C++17.

## 2. Bundled libraries to identify & subtract (the high-leverage step)
Detected from version banners / symbols in the binary:

| Library | Version evidence | Subtraction approach |
|---------|------------------|----------------------|
| **libnx** | "Copyright 2017-2018 libnx Authors" | Build matching libnx; sign or BinDiff |
| **newlib (libc)** | "newlib-4.5.0.20241231" | devkitA64 ships it; sign the archive |
| **libstdc++** | `St…`, `__gnu_cxx…` mangled names | From devkitA64 GCC; sign |
| **libcurl** | "CLIENT libcurl 7.69.1" | Build 7.69.1 w/ devkitA64; sign/BinDiff |
| **zlib** | "inflate 1.3.1 … Mark Adler" | Build 1.3.1; sign |
| **zstd** | "For Zstandard software", `LICENSE.zstd` | Build; sign |
| **mbedTLS** | "BIGNUM …", `AES-*`, `X509 -` | Build; sign |
| **libssh2** | "SSH-2.0-libssh2_1.10.0", Sara Golemon © | Build 1.10.0; sign |
| **nlohmann::json** | `nlohmann::json_abi_v3_11_3` | Header-only; match by RTTI/strings |
| **fmtlib** | `fmt::v11::format_error` | Match by RTTI |
| **pugixml** | `pugi::xpath_exception` | Match by RTTI/strings |
| **utfcpp** | `utf8::invalid_utf8` etc. | Match by RTTI |
| **qrcodegen** | `qrcodegen::data_too_long` | Match by RTTI |
| **POCO (partial)** | `dbi::utils::poco::*` exceptions | Vendored subset — treat as app-adjacent |
| **LZMA/7-Zip, bzip2** | decompression error strings | Build/sign |
| **MTP** (`capmtp`/`mtp-server-nx`) | `DBI MTP`, `capmtp` | App-adjacent vendored stack |

**Procedure:** compile each library at the detected version with the *same devkitA64 toolchain*,
generate FLIRT (IDA) or Diaphora/BinDiff signatures, apply them to the Ghidra/IDA database. Anchor
ambiguous matches with `.rodata` strings (license blobs, version banners, error templates). Goal:
**>70% of functions auto-named as library code**, leaving the tagged "unknown = DBI app" set.

## 3. Symbol-recovery shortcut — leaked C++ RTTI
Although normal symbols are stripped, **C++ exception/RTTI type names survive** (the runtime needs
them). These reveal the app's namespace/class skeleton. Recovered so far:
- `dbi::mtp::MtpInitiatorException`
- `dbi::utils::poco::{Exception, RuntimeException, DataException, SyntaxException, URISyntaxException}`
- `nx::utils::fs::BreakScanException`

→ The app uses `dbi::` and `nx::` namespaces with at least `dbi::mtp`, `dbi::utils::poco`,
`nx::utils::fs` sub-namespaces. Harvest **all** `__class_type_info` / `__si_class_type_info` /
`__vmi_class_type_info` vtables and their associated name strings to reconstruct the class hierarchy
(base/derived links come from the type_info structures). Feed these names back as Ghidra labels.

## 4. Decompilation export → agent pipeline (Phase 3)
1. `analyzeHeadless` the project; run a script that, for every function in the **DBI-app set**,
   exports decompiled C + its xrefs + referenced strings to one file per function (or per namespace).
2. Fan out agents over that work-list (batched): each agent classifies the subsystem, infers intent
   from string/xref anchors, proposes a clean name, recovers struct/enum types, and writes a comment.
3. Apply the proposed names/types back into Ghidra via pyhidra; **re-run analysis** so xrefs and
   types propagate; re-export. Repeat until the call graph is legible.
4. Maintain `SYMBOLS.md` (address → name → subsystem → purpose) and `ARCHITECTURE.md` (subsystem map).

## 5. Reimplementation (Phase 4) & validation (Phase 5)
- Scaffold a devkitPro Switch app (Makefile + libnx); reimplement subsystem-by-subsystem against
  `FEATURES.md`/`PROTOCOL.md`/`FORMATS.md` + the Phase 3 annotations.
- **Validate the `DBI0` protocol** by pointing the existing `../dbibackend` at the rebuilt app and
  confirming LIST + chunked FILE_RANGE installs behave identically.
- Build gate: compiles to `.nro` with devkitPro and boots.
- Optional confidence: BinDiff the rebuilt `.nro` per-function against `DBI.nro`.

## 6. Hygiene
- Keep the Ghidra project, exports, and decompiled C **out of git** (`.gitignore` already excludes them).
- The `docs/` specs are written from observable behavior only → they are the clean-room boundary if
  the project later needs to drop the decompilation-derived path for licensing reasons.

---

## Note on README vs binary capability gap
`FEATURES.md` is distilled from the README and documents **FTP only, no QR code**. The binary,
however, bundles **libssh2 (SFTP)** and **qrcodegen**, and the sample `dbi.config` includes `SFTP`
network-source examples. Conclusion: the shipping binary supports more than the manual documents
(SFTP sources; likely a QR code for the access point). Treat the binary — not the README — as the
authority on actual capabilities; note these as features to confirm in Phase 3.
