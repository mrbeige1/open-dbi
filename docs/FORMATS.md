# On-Disk & Container Formats

DBI installs and inspects Nintendo Switch content containers. This document summarizes the formats
it must parse, anchored by strings found in `DBI.nro` and cross-referenced to the public
[switchbrew](https://switchbrew.org/wiki/) documentation. **Switchbrew is the authoritative source**
for byte-level layouts — this file maps DBI's behavior onto those formats and records evidence.

> ⚠️ Encryption keys (`prod.keys`/`title.keys`) are required to decrypt NCA content. DBI loads them
> at runtime; never commit them. This project documents *formats*, not keys.

## Container hierarchy
```
XCI / XCZ  (gamecard image)         NSP / NSZ  (eShop-style package)
  └─ HFS0 partitions                  └─ PFS0 (PartitionFS)
       └─ secure: NCA, ticket, cert        └─ NCA, .tik (ticket), .cert, .cnmt(.nca)
                                                 └─ CNMT (content meta) lists the NCAs
```

| Token in binary | Format | Role |
|-----------------|--------|------|
| `PFS0` _(verify magic present)_ | PartitionFS | NSP container directory |
| `HFS0` _(verify)_ | HashedFS | XCI partition format (root/normal/secure/logo) |
| `.nca` | NCA | Nintendo Content Archive — the encrypted content unit |
| `.cnmt`, `.cnmt.nca` | CNMT | Content Meta — enumerates content + types + versions |
| `.tik` | Ticket | Per-title rights key (titlekey) holder |
| `.cert` | Certificate chain | Accompanies tickets |
| `/control.nacp` | NACP | Title metadata (name/author/version) |
| `.xci` / `.xcz` | XCI/XCZ | Gamecard image (XCZ = compressed) |
| `.nsp` / `.nsz` | NSP/NSZ | Package (NSZ = compressed) |

## NCA crypto (mbedTLS-backed)
Evidence: `AES-128-XTS`, `AesCtr`, `AES-128-CTR`, RSA, `DecodedNcaSubsection`, `Titlekey`,
`rightsId`, `keygen`/`masterkey` references.
- **NCA header**: AES-128-**XTS** encrypted (header key).
- **NCA sections**: AES-128-**CTR** (or XTS) using a key derived from the key-area / titlekey.
- **Titlekey** path: ticket holds the titlekey, decrypted with a master/common key (keygen index).
- **Integrity**: section hashes (PFS0 hash tables / IVFC for RomFS). `[Install] CheckHash` toggles
  NCA hash verification on install.

## NSZ / XCZ compression
NSZ/XCZ are the original NSP/XCI with NCA contents compressed per-section. Evidence shows **multiple
decompressors bundled**, so the parser must dispatch by section codec:
- **Zstandard** (`zstd`, "For Zstandard software") — the primary NSZ codec.
- **LZMA / 7-Zip** ("No memory for 7-Zip decompression", "Lzma decompression failed").
- **bzip2** ("Failed to clean up bzip2 decompressor").
- **zlib/inflate** 1.3.1 ("bad zlib header").

The NSZ container marks compressed sections with an `NCZSECTN`-style header table (block layout with
per-section codec, offset, size, and the crypto counter to resume CTR). _(Confirm exact header layout
against binary in Phase 3 — search the `DecodedNcaSubsection` code path.)_

## Install targets
- **SD card install** vs **NAND install** (both exposed as MTP storages 5/6, and as install
  destinations). Uses `ncm` (Content Manager) to register placeholders and commit content, and `es`
  to import tickets. `ns` for application records / launch metadata.

## Gamecard (XCI) specifics
- Reads from the inserted cartridge (`fsp-srv` gamecard, `9: Gamecard` MTP storage).
- Can **trim** XCI dumps ("(trimmed).xci") — strip the unused padding tail.

## Title/version data
- `VersionsURL` (see `CONFIG.md`) supplies `<id>|[rightsId|]<version>` lines used by `HighlightUpdates`
  and the update checker. Default source is blawar's titledb `versions.txt`.

## What Phase 3 must recover precisely
1. Exact NSZ/XCZ section-header struct and codec dispatch (the `DecodedNcaSubsection` path).
2. The keygen/titlekey derivation order (which master key index, how rightsId maps to key).
3. The NCM placeholder→register→commit sequence and ES ticket import order (the install state machine).
4. CNMT parsing → which NCAs are written for a given install type (base/update/DLC, combined NSP).
