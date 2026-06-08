# Open-DBI

An **open-source, clean-room reconstruction** of the Nintendo Switch homebrew **DBI**
(an NSP/NSZ/XCI installer + file/save manager), built from scratch with devkitPro + libnx.

This repository contains:
- **[`open-dbi/`](open-dbi/)** — the reimplementation. Builds to a Switch `.nro`. Already installs
  titles (USB or SD), including compressed **NSZ**, with a custom-implemented NCA crypto stack.
- **[`docs/`](docs/)** — the behavioral/format specifications and reverse-engineering methodology the
  reimplementation was written from. Start with [`docs/STATUS.md`](docs/STATUS.md).
- **[`scripts/ghidra/`](scripts/ghidra/)** — the Ghidra automation used to study the original binary.

> **Status:** the install path is functional and hardware-validated (USB + SD, NSP + NSZ, real `ncm`/`es`
> + `ns` application records). The framebuffer UI and several non-install features are not reimplemented.
> See [`docs/STATUS.md`](docs/STATUS.md) for the precise state.

## ⚖️ Legal / licensing — read before using or publishing

- **Derivative-work risk.** DBI is **closed-source freeware** (author: *rashevskyv*). This project was
  produced by reverse-engineering it. Even though the code here is written clean-room *from behavioral
  specs* (not copied from decompiled output), publishing a reimplementation of someone else's
  closed-source software without permission is a real copyright risk. **Resolve this** (e.g. obtain the
  original author's permission) before treating this as freely licensed. The repo ships **no upstream
  DBI files** (binary, manual, host script, config are all git-ignored).
- **Content protection.** The NCA crypto here uses **user-supplied `prod.keys`** to read content you
  own, the same as established open tools (hactool/nstool/Goldleaf). No keys are included or distributed.
- **Intended use.** This is homebrew tooling for managing **content you are legally entitled to** (your
  own dumps, homebrew, forwarders). Don't use it to obtain or distribute copyrighted games.

A `LICENSE` is intentionally left for you to choose once the points above are resolved — see
[`docs/STATUS.md`](docs/STATUS.md) §1.

## Build

```sh
export DEVKITPRO=/opt/devkitpro
cd open-dbi && make            # -> open-dbi/open-dbi.nro
```
Requires devkitPro with `switch-dev` + `switch-libzstd`. See [`docs/BUILD.md`](docs/BUILD.md).

## Documentation map
| Doc | Contents |
|-----|----------|
| [`docs/STATUS.md`](docs/STATUS.md) | **Start here** — current state, how to build/test, how to resume |
| [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) | How DBI works — subsystem map + install pipeline |
| [`docs/PROTOCOL.md`](docs/PROTOCOL.md) | The `DBI0` USB protocol |
| [`docs/CONFIG.md`](docs/CONFIG.md) · [`docs/FORMATS.md`](docs/FORMATS.md) | config + on-disk formats |
| [`docs/FEATURES.md`](docs/FEATURES.md) | behavioral spec of DBI's features |
| [`docs/RE-METHODOLOGY.md`](docs/RE-METHODOLOGY.md) | toolchain + how the RE was done |
| [`docs/PROGRESS.md`](docs/PROGRESS.md) · [`docs/SYMBOLS.md`](docs/SYMBOLS.md) | metrics + recovered symbols¹ |

¹ `SYMBOLS.md` is decompilation-derived; consider excluding it if you want a strict clean-room repo.
