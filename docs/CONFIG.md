# `dbi.config` Reference

DBI reads its configuration from `dbi.config` (on the SD card alongside the app, i.e.
`sdmc:/switch/DBI/dbi.config`). It is a commented **INI** file: `[Section]` headers, `Key=Value`
lines, `;` line comments. A leading `;` on a `Key=` line disables that key (falls back to default).

Defaults below are the values shipped in the repo's `dbi.config`. Effects are taken from the inline
comments and the README; entries marked _(verify)_ should be confirmed against the binary in Phase 3.

> The shipped `dbi.config` sample is **not exhaustive** — the README documents additional keys not
> present in it (e.g. `UseLibUsbHsFS` for external-USB support). Those README-only keys are listed in
> `FEATURES.md`; Phase 3 should enumerate the binary's full config-key set authoritatively.

---

## `[General]`
| Key | Default | Type | Effect |
|-----|---------|------|--------|
| `ExitToHomeScreen` | `true` | bool | Exit directly to the HOME screen (vs. back to hbmenu). |
| `SavesFolder` | `sdmc:/switch/DBI/saves/` | path | Where save backups are stored. |
| `LogEvents` | `false` | bool | Log "Install", "Check integrity", and "Cleanup" processes. |
| `LogsFolder` | `sdmc:/switch/DBI/logs/` | path | Where logs are written. |
| `DumpsFolder` | `sdmc:/switch/DBI/dumps/` | path | Where game dumps are stored. |
| `AppSorting` | `LastPlayed,InstallLocation,Size,Name` | csv | Ordered sort keys for the installed-application list. |
| `SaveSorting` | `AppLastPlayed,AppName,UserUid,Size,Time` | csv | Ordered sort keys for the save list. |
| `HighlightUpdates` | `true` | bool | Highlight files in browsers that are updates to currently installed titles. |
| `RotateScreen` | `false` | bool | Rotate the screen 180°. |
| `RotateJoycon` | `false` | bool | Rotate Joy-Con input accordingly. |
| `OptimizeClockSpeed` | `true` | bool | Underclock CPU in menus to save battery. |
| `VersionsURL` | `https://raw.githubusercontent.com/blawar/titledb/master/versions.txt` | URL/path | Source of title→version data, format `<id>\|[rightsId\|]<version>`. Can be `http(s)://` or `sdmc:/`. |
| `ROSaveFS` | `false` | bool | Browse the save filesystem read-only. |
| `ShowUpdateFromHere` | `true` | bool | Show "Update all items from here…" in file-browser context menus. |
| `ShowCacheWarmingIndicator` | `true` | bool | Show the cache-warming spinner. |
| `MoveDownAfterX` | `true` | bool | Move the cursor down after a selection action. |
| `ScreenIdleTimeout` | `0` | int (s) | Screen idle timeout in seconds; `0` = never. |
| `Autorepeat` | `true` | bool | Auto-repeat navigation buttons when held. |
| `Secondcursor` | `false` | bool | Show cursors on both panels in two-panel browsing mode. |
| `FoolproofSaveDelete` | `true` | bool | Back up saves before deleting them. |

**Sort-key vocabularies** _(verify exact accepted tokens against binary):_
- `AppSorting`: `LastPlayed`, `InstallLocation`, `Size`, `Name`.
- `SaveSorting`: `AppLastPlayed`, `AppName`, `UserUid`, `Size`, `Time`.

---

## `[MainMenu]` — top-level menu item visibility
Each key toggles whether a main-menu entry is shown.
| Key | Default | Shows… |
|-----|---------|--------|
| `BrowseSD` | `true` | Browse & install files from SD card. |
| `BrowseSystem` | `false` | Browse & copy files from the SYSTEM partition. |
| `BrowseUser` | `false` | Browse & copy files from the USER partition. |
| `USBHost` | `true` | Browse & install from USB flash drives / HDD. |
| `BackendInstall` | `true` | Install from PC via `dbibackend`. |
| `GameCard` | `true` | Install from an inserted game cartridge. |
| `Network` | `true` | Browse & install from configured network sources. |
| `Local` | `true` | Browse & install from configured SD-card folders. |
| `BrowseApps` | `true` | Browse installed applications. |
| `UpdateCheck` | `true` | Check for app updates. |
| `Tickets` | `true` | View / delete installed tickets. |
| `Saves` | `true` | View / delete / manage game saves. |
| `MTP` | `true` | Run MTP responder. |
| `FTP` | `true` | Run FTP server. |
| `Tools` | `true` | Tools menu. |

---

## `[Applications]`
| Key | Default | Effect |
|-----|---------|--------|
| `CalculateLFSSize` | `true` | Whether to compute LayeredFS mod size. |

---

## `[Install]`
| Key | Default | Effect |
|-----|---------|--------|
| `CheckHash` | `true` | Verify NCA hashes during install. |
| `ChunkedTransfer` | `false` | Use chunked network transfer (helps on poor connections). |

---

## `[MTP]`
| Key | Default | Effect |
|-----|---------|--------|
| `LogAllFiles` | `false` | Log all files; if off, only files ≥ 2 MB are shown during transfer. |
| `ShowCombinedNSP` | `true` | Show a virtual single multi-title NSP (base + latest update + all DLC). |
| `ShowMAC` | `true` | Show virtual "Mods & cheats" folder → `sdmc:/atmosphere/contents/<TITLEID>`. |
| `MACasTID` | `true` | Use TitleID (not name) for the "Mods & cheats" folder. |
| `CustomStorages` | `true` | Expose user-defined microSD shortcuts as separate MTP storages (see `[MTP custom storages]`). |
| `TurnOffScreen` | `false` | Turn the screen off when entering MTP mode. |
| `ReportAndroidExtension` | `true` | Report the Android MTP extension (some initiators assume Android quirks). |
| `SingleURB` | `false` | Use single-URB transfers — slower but more stable. |
| `BufferSize` | `512` | MTP transmission buffer size in KB. |
| `NewBufferTimeout` | `2500` | MTP transmission timeout in ms. |

---

## `[FTP]`
| Key | Default | Effect |
|-----|---------|--------|
| `TurnOffScreen` | `false` | Turn the screen off when entering FTP mode. |
| `UseAP` | `false` | Start a local Wi-Fi access point for the FTP server (see `[Access point]`). |
| `ReadMT` | `false` | Read file modification times (can slow large directories). |

---

## `[Access point]` — local AP used when `[FTP] UseAP=true`
| Key | Default | Effect |
|-----|---------|--------|
| `SSID` | _(empty)_ | AP network name. |
| `Password` | _(empty)_ | AP password. |
| `Use5GHz` | `true` | Use the 5 GHz band. |
| `Hidden` | `false` | Hide the SSID. |

> The README documents a QR-code shown for the access point (binary bundles `qrcodegen`) so a phone
> can join quickly — _(verify which screen renders it)_.

---

## `[MTP Storages]` — enable/disable built-in MTP storages
Keyed by `<storage-id>: <name>=<bool>`.
| ID | Name | Default |
|---:|------|---------|
| 1 | SD Card | `true` |
| 2 | Nand USER | `true` |
| 3 | Nand SYSTEM | `true` |
| 4 | Installed games | `false` |
| 5 | SD Card install | `true` |
| 6 | NAND install | `true` |
| 7 | Saves | `true` |
| 8 | Album | `true` |
| 9 | Gamecard | `false` |

---

## `[Network sources]` — install sources for the Network menu
Format: `<display name>=<type>|<URL>`. Supported `<type>` values (from sample entries):
| Type | URL example | Notes |
|------|-------------|-------|
| `URLList` | `http://host/nspindexer/index.php?DBI` | An index endpoint returning a file list. |
| `ApacheHTTP` | `http://host/Nintendo/Switch/` | Apache-style auto-index HTTP directory. |
| `FTP` | `ftp://host:2121/` | FTP source. |
| `SFTP` | `sftp://user:password@host/` | SFTP via libssh2. Password may be replaced by key files `sdmc:/switch/.ssh/id_rsa` + `id_rsa.pub` (libcurl needs the keypair, not just the private key). |

---

## `[Local sources]` — main-menu shortcuts to SD-card folders
Format: `<display name>=<path>`. Default: `DBILogs=sdmc:/switch/DBI/logs`.

## `[MTP custom storages]` — user MTP storages (when `[MTP] CustomStorages=true`)
Format: `<display name>=<path>`. Default: `DBILogs=sdmc:/switch/DBI/logs`.

## `[Title name override]` — display-name overrides
Format: `<UPPERCASED TID>=<Desired name>`. Example: `010023901191C000=Naheulbeuk`.

---

## Parsing notes for reimplementation
- Vendored INI/URI handling appears to come from a partial **POCO** port (`dbi::utils::poco`,
  exceptions `SyntaxException`/`URISyntaxException`/`DataException`) — relevant when reimplementing
  the config + network-source URL parser.
- Booleans are `true`/`false`; a commented-out key (`;Key=...`) means "use default".
- Storage/source sections use a `key=value` list rather than fixed keys; preserve insertion order.
