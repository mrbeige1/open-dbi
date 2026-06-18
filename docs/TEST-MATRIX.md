# Open-DBI test matrix

A uniform way for testers to report results, so feedback is reproducible instead of
"works" / "doesn't work". Every row should be run **dry-run first** (read-only, safe) and
only then as a real install if you intend to write to NCM.

## How to run a row

On the `open-dbi` NRO (button-driven console harness — see `main.cpp`):

| Key | Action | Writes NCM? |
|-----|--------|-------------|
| **Up** | Dry-run `sdmc:/install.nsp` (parse + classify + planned install) | No |
| **Down** | Create debug report → `sdmc:/switch/open-dbi/logs/open-dbi_report.txt` | No |
| **B** | NCA decrypt test (read-only) | No |
| **A** | USB transfer test (needs PC `dbibackend`) | No |
| **L+R+X** | REAL install from `sdmc:/install.nsp` | **Yes** |
| **L+R+Y** | REAL install over USB from `dbibackend` | **Yes** |

Logs are always written to `sdmc:/switch/open-dbi/logs/open-dbi_latest.log` (own
namespace; does not collide with DBI's `sdmc:/switch/DBI/logs/`). Attach the **debug
report** (Down) to any bug report — it is redacted (no keys, titlekeys, ticket/cert
payloads, or rights-id values; only structure + libnx Result codes).

Place test files at `sdmc:/install.nsp` and keys at `sdmc:/switch/prod.keys` (use your
own dumps and keys; never share them).

## Reporting template

For each row, report:

```
build:        <from the log header, e.g. c0343c0-dirty>
case:         <which row below>
file:         <type/size, e.g. base NSP 1.6 GB>
dry-run:      <PASS/FAIL — did it parse + report the right titleId / meta type?>
install:      <PASS/FAIL/n/a>
failed phase: <parse | write-nca | import-ticket | content-meta | commit | n/a>
result codes: <any rc=0x........ lines from the log>
notes:        <home-menu launch? anything unexpected?>
```

## Matrix

| # | Case | Expected (dry-run) | Expected (install) | Milestone gate |
|---|------|--------------------|--------------------|----------------|
| 1 | Small base-game NSP | meta = Application (base), N NCAs, titleId correct | installs, launches | M0 |
| 2 | Update NSP | meta = Patch (update) | installs over base | M0 |
| 3 | DLC NSP | meta = AddOnContent (DLC) | installs, DLC visible | M0 |
| 4 | Rights-id content | `rightsId=yes` on NCAs; ≥1 ticket present | ticket imported, installs | M1 |
| 5 | No-rights-id content | `rightsId=no`; no ticket needed | installs | M1 |
| 6 | NSZ / NCZ | reconstructed size > compressed; correct CNMT | installs, launches | **M3** |
| 7 | Large file (multi-GB) | parses fully | installs without timeout | M0 / M2 |
| 8 | USB install | LIST + FILE_RANGE succeed | streamed install | **M2** |
| 9 | Intentionally broken NSP | reports the malformed structure | fails cleanly, **names failed phase**, NCM unchanged | **M4** |
| 10 | Missing `prod.keys` | parses; logs "needs prod.keys"; no crash | n/a (blocked early) | M1 |
| 11 | Missing ticket/cert (rights-id content) | dry-run WARNS "rights-id but no ticket" | fails at import-ticket | M1 |
| 12 | XCI / XCZ | (not yet supported) | (not yet supported) | **M6** |

Rows in **bold-gated** milestones depend on features that are not yet complete
(`M2` USB, `M3` NSZ correctness, `M4` rollback, `M6` XCI) — a FAIL there is expected
until that milestone lands; report it against the milestone, not as a regression.
