# Building Open-DBI

The reconstructed app lives in [`../open-dbi/`](../open-dbi) and builds to a Switch `.nro` with
devkitPro + libnx.

## Toolchain
Install devkitPro and the Switch group (see `RE-METHODOLOGY.md §0`):
```sh
# macOS: installer pkg from github.com/devkitPro/pacman, then:
#   (Apple Silicon also needs Rosetta: softwareupdate --install-rosetta)
sudo dkp-pacman -Syu --noconfirm switch-dev switch-libzstd
```
- `switch-dev` provides devkitA64 (GCC), libnx, and the NRO tools (`elf2nro`, `nacptool`).
- `switch-libzstd` is **required**: the `Makefile` links `-lzstd` for `source/crypto/ncz.cpp`
  (NSZ/NCZ decompression). Without it the link fails with `cannot find -lzstd`.
  Note the package is `switch-libzstd`, *not* `switch-zstd`.
- `-Syu` refreshes the package db first; the installer pkg ships a stale snapshot, so a plain
  `-S` can report `target not found` for newer packages.

## Build
```sh
cd open-dbi
export DEVKITPRO=/opt/devkitpro
export PATH=$DEVKITPRO/tools/bin:$DEVKITPRO/devkitA64/bin:$PATH   # if not already on PATH
make            # -> open-dbi.nro  (and .elf / .nacp)
make clean
```

## Notes
- The libnx app build has **C++ exceptions and RTTI disabled** by default — avoid `try/catch` and
  `std::stoi`/`dynamic_cast`; use `strtol` etc. (the config parser already does).
- `SOURCES` in the `Makefile` lists each source subdir explicitly (`source source/proto source/config
  source/saves`); add new subdirs there.
- Output is a homebrew `.nro` (verify: bytes at offset `0x10` are `NRO0`). Run on hardware via hbmenu
  or in a Switch emulator for testing.
- Current `main` is a console self-test, not the full UI.
