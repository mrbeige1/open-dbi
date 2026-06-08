# DBI — Behavioral Specification

> Clean-room behavioral specification for **DBI**, a Nintendo Switch homebrew title-management application.
> This document describes **observable, user-facing behavior only** — what the application does and how it
> behaves from a user's point of view. It contains no implementation details.
>
> Source: distilled from the DBI user manual (`README.md`). Where the manual references a `dbi.config` key,
> the key name is cross-referenced.

---

## 1. Overview

DBI is an all-in-one title installer and Nintendo Switch management tool. It installs game titles, updates,
and DLC, and provides file-management, save-management, and diagnostic features.

**Supported install formats:**

| Format | Description |
| --- | --- |
| `NSP` | Standard title package |
| `NSZ` | Compressed NSP (decompressed during install; on-disk size differs from file size) |
| `XCI` | Game-card image |
| `XCZ` | Compressed XCI (decompressed during install) |

**High-level capabilities:**

- Install titles from many sources: SD card, external USB drive, PC over USB (dbibackend), network HTTP
  ("Home server"), FTP, MTP, and game cartridge.
- Manage installed applications: move between NAND/SD, delete, integrity-check, dump, view content/tickets/saves.
- Full file-manager operations (copy, move, delete, create folder) over SD, USB, NAND, and MTP.
- Save data backup, restore, and management.
- Ticket browsing and management.
- Maintenance tools (orphaned-file cleanup, parental-control removal, user deletion, NTP sync, update check, random game).
- Built-in viewers/editors: images (`jpg`/`png`/`psd`), archives (`zip`/`rar`/`cbr`/`cbz`), text and hex.
- Activity log with per-game, per-user playtime statistics.

**Run modes.** DBI ships as `dbi.nro` with a `dbi.config` file, placed at `sdmc:/switch/DBI/`. It can run in:

- **Applet mode** (launched from the Album) — primary, recommended mode. Indicated by a **blue background**.
- **Application mode** (title override / forwarder) — indicated by a **black background**.

**Status bar.** Bottom-left shows SD-card used/total space; bottom-right shows NAND usable space; bottom-center
shows the DBI version (`dbi: XXX`).

---

## 2. Main Menu Items

Each entry's visibility is controlled by a `[MainMenu]` config key (see §2.1). Several entries have main-menu hotkeys.

| Menu item | Behavior | Hotkey | Config key |
| --- | --- | --- | --- |
| **Browse SD Card** | Browse and install `NSP`/`NSZ`/`XCI`/`XCZ` from the SD card; also a file manager and `.nro` launcher. | — | `BrowseSD` |
| **Browse USB0 Drive** | Browse/install from an external exFAT/FAT32 USB drive (flash drive, HDD) via USB-OTG. | — | `USBHost` |
| **Install title from DBIbackend** | Install titles from a PC over a USB-C cable using the companion `dbibackend` program. | **(Y)** | `BackendInstall` |
| **Install title from Gamecard** | Appears only when a game card is inserted; installs the card's content to SD or NAND. | — | `GameCard` |
| **Home server** | Install titles over the network (HTTP/Wi-Fi). Appears only when network sources are configured. Display name is configurable. | — | `Network` |
| **Browse installed applications** | View/manage installed titles, updates, DLC, mods; move/delete/integrity/dump; view playtime and launches. | **(L)** | `BrowseApps` |
| **Cleanup orphaned files** | Auto-clean leftover/failed-install files, downloaded firmware, unused tickets. | — | `Cleanup` |
| **Check for title updates** | Check installed titles for new updates/DLC against a configured database. | — | `UpdateCheck` |
| **Browse tickets** | View and delete installed system tickets. | — | `Tickets` |
| **Browse saves** | View, back up, restore, and delete game saves. | — | `Saves` |
| **Run MTP responder** | Start the built-in MTP server for PC/Android access over USB-C. | **(X)** | `MTP` |
| **Run FTP server** | Start the FTP server (browse on port 5000, install on port 6000). | — | `FTP` |
| **Exit** | Quit DBI (to HOS home screen or hbmenu, configurable). | **(+)** | — |

Additional file-browser entries can be exposed: **Browse System** (`BrowseSystem`) and **Browse User** (`BrowseUser`)
browse and copy from the read-only NAND SYSTEM / USER partitions; **Browse SD shortcuts** (`Local`) shows configured
folder shortcuts (see §3.8).

### 2.1 Main-menu visibility (`[MainMenu]`)

Every menu item above can be shown/hidden with its config key (`true`/`Yes` = shown, `false`/`No` = hidden).

---

## 3. Installation Sources

### 3.1 USB from PC — dbibackend

Installs titles from a PC over USB-C without first copying them to the SD card.

- Requires the companion `dbibackend` program on the PC (`dbibackend.exe` for Windows, or the cross-platform
  `dbibackend` script).
- **Workflow:** launch dbibackend → select files → "Start server" → connect USB-C cable → select
  **Install title from DBIbackend** on the Switch.
- **Windows driver:** requires the **libusbK (v3.1.0.0)** driver, installable via Zadig while DBI is in this mode.
- **Quick send (Windows):** right-click files/folders → **Send from dbibackend** (configured via a `shell:sendto`
  shortcut) queues them immediately.
- **Command line:** files can be passed as arguments, e.g. `dbibackend.exe "File1.nsp" "File2.nsp"`.
- After files are queued, installation uses the same install-options dialog as SD/USB browsing (see §4.1).
- Alternative third-party clients exist (headless Python, NSW-DBI on nodegui).

### 3.2 External USB drive (USB0)

- Browse and install from an exFAT/FAT32 USB mass-storage device connected via USB-OTG.
- Requires the external-USB driver to be enabled: **Use external USB drives** (`UseLibUsbHsFS`).
- Behaves like SD browsing, including the install-options dialog and "Delete after install" option.

### 3.3 SD card

- Browse, select, and install title files from the SD card.
- Doubles as a file manager and `.nro` launcher (see §4).

### 3.4 Game card

- The **Install title from Gamecard** entry appears only when a cartridge is inserted.
- Installs the card's title to SD or NAND.

### 3.5 Network — Home server (HTTP)

Installs titles over Wi-Fi/LAN from an HTTP server with directory listing enabled (Apache, nginx, Mongoose,
Python SimpleHTTP, rclone, etc.).

- Enabled by configuring the `[Network sources]` section; the menu entry name is the configured display name.
- **Source format:** `<display name>=<type>|<URL>`, e.g. `Home server=ApacheHTTP|http://192.168.1.47/Nintendo/Switch/`.
- Supports HTTP Basic auth and domain names / remote VPS:
  `http://user:password@host:port/Nintendo/Switch/`.
- Server type `URLList` is used for an **NSP Indexer** source (server-side NSP indexing).
- Once connected, presents the standard install interface (see §4.1).
- **Chunked HTTP/FTP transfer** (`ChunkedTransfer`) improves reliability over poor connections.

### 3.6 FTP

- **Run FTP server** starts DBI's FTP server: **port 5000** for browsing SD files, **port 6000** for installing files.
- Options (`[FTP]`): **Turn off screen** (`TurnOffScreen`), **Read file date** (`ReadMT`), and
  **Start local Access point** (`UseAP`).
- An FTP source can also be defined as a network source: `Test FTP=FTP|ftp://anonymous:password@192.168.1.24:2121/`.
- **Local access point** (`[Access point]`): DBI can act as a Wi-Fi access point that FTP clients connect to directly.
  Settings: **SSID** (`SSID`), **Password** (`Password`), **Use 5 GHz** (`Use5GHz`; off = 2.4 GHz),
  **Use hidden SSID** (`Hidden`).

> **SFTP:** The source manual describes FTP only. No SFTP source or server is documented; if SFTP support exists it
> is not described in the manual and is therefore out of scope for this spec.

### 3.7 MTP (SD install / NAND install)

When the MTP responder is running, the PC exposes **SD install** and **NAND install** virtual folders; dropping a
title file into either installs it to SD or NAND respectively. See §3.7 of the MTP section (§6 / §8 below) for the
full storage list. Compressed `NSZ`/`XCZ` files are decompressed during install, so post-install size differs.

### 3.8 Local SD-card shortcuts (`[Local sources]`)

- Configurable quick-access menu entries that open a chosen SD folder, e.g. `Homebrew Shortcut=sdmc:/switch`.
- Visibility controlled by **Browse SD shortcuts** (`Local`).

---

## 4. Browsing & File Management

The SD/USB browser doubles as a full file manager. Navigation: **(A)** opens a folder, **(B)** returns.
`.nro` homebrew files can be launched directly by highlighting and pressing **(A)**.

### 4.1 Installing from a browser

- Select one or more files with **(X)**; **(Y)** inverts the selection (selects all if none selected). Selected
  filenames turn light blue.
- Press **(A)** to confirm — an **install-options dialog** appears:

| Option | Behavior |
| --- | --- |
| **Total transfer size** | Total size of selected files. |
| **Total install size** | Free space required to install. |
| **Install target** | **NAND**, **SD**, or **AUTO** (SD first, falls back to NAND if SD lacks space). |
| **Delete after install** | Deletes source files after a successful install (read-only attribute must be cleared). Off by default. SD/USB only. |
| **Turn off screen** | Turns the screen off during install to save battery; turns back on when done. Handheld mode only. |
| **Start install** | Begins. On success: "Installation Complete. Press B to return". |

- Installing a new update for a game automatically removes the old update.

### 4.2 File-manager operations

- Copy, move, delete files and folders, and create folders.
- Selection coloring follows the color codes in §11.
- **Highlight update files** (`HighlightUpdates`) highlights files that are updates for installed titles.
- **Show 'Update From Here'** (`ShowUpdateFromHere`) adds an "Update all titles" context-menu action that updates
  installed games from all available sources (SD/USB/HTTP/FTP).

### 4.3 Archive handling

- Browse into and read `zip` and `rar` archives, and `cbr`/`cbz` comic containers, as if they were folders.

### 4.4 Image viewer

- View `jpg`, `png`, and `psd` images.
- **Set as avatar…** (context menu, **(+)**): sets the selected image as the user avatar; auto-cropped to square and
  scaled down. Prepare the image with the desired aspect ratio in advance to control cropping.

### 4.5 Text / hex viewer and editor

Any file can be opened as **text** or **hex**. Non-empty files open in viewing mode; **(L3)** switches to editing.
A new empty text file can be created from the context menu (**(+)** → **Create a new file…**); opening an empty text
file launches the editor automatically.

**Viewing mode**

| Control | Action |
| --- | --- |
| DPAD / Left Stick / Right Stick | Scroll text |
| (L) / (R) / (ZL) / (ZR) | Previous / next page |
| (R3) | Toggle text / hex view |
| (L3) | Switch to editing mode |
| (+) | Context menu: **Editing**, **Encoding** (not persisted across reopen), **Line Wrapping** |

**Editing mode**

| Control | Action |
| --- | --- |
| Right Stick | Move around on-screen keyboard |
| DPAD / Left Stick | Move within text |
| (A) | Type highlighted character |
| (X) | Backspace |
| (B) | Open save menu |
| (Y) | Space |
| (L)+LEFT / (L)+RIGHT | Line start (HOME) / line end (END) |
| (R)+LEFT / (R)+RIGHT | Next word / previous word |
| (ZL) | Change case |
| (ZR) | New line (Enter) |
| (R3) | Switch language |
| (L3) | Switch to viewing mode |

Closing or switching to viewing mode prompts to save if changes were made.

---

## 5. Installed Applications

**Browse installed applications** lists installed titles, updates, and DLC, with size, display and hex version,
TitleID, total playtime, launch count, and LayeredFS-mod presence. Top-center shows the total count and sort order.
Quick-launch a highlighted title with **(L3)**.

**Left-bracket annotations per entry:**

| Code | Meaning |
| --- | --- |
| **N / S / M / G** | Location: NAND / SD / Mixed (both) / Gamecard |
| **b** | BASE game |
| **u** | Update installed |
| **d** | DLC installed |
| **l** | LayeredFS mod or cheats present at `sdmc:/atmosphere/contents/<titleID>/` |

A title shown **in red** means only an update and/or DLC is installed — the base game is **not** installed.

**Cache/size options:** **Show cache warming indicator** (`ShowCacheWarmingIndicator`) shows a spinner while caching
title info; **Show LFS folder size (slow)** (`CalculateLFSSize`) calculates installed-mod size (slows menu opening).
**Check hash during install** (`CheckHash`) verifies `.nca` hashes during installation.

### 5.1 Title context menu (**(+)**)

| Action | Behavior |
| --- | --- |
| **Delete title** | Delete the selected titles. |
| **Move title to SD/NAND** | Move titles to the opposite location; both options appear for mixed-location content. |
| **Reset required version** | Reset the required system-version check (Atmosphère debug must be enabled). |
| **Check integrity** | Verify data integrity of selected titles. |
| **Expose contents via MTP** | Mount the selected titles' content via MTP. |
| **Dump to SD** | Dump game/DLC/update to SD at `DumpsFolder` (default `switch/DBI/dumps`). |
| **Content info** | SDK version, key generation, ID, patch info, and more. |

### 5.2 Detailed game menu (**(A)**)

Pressing **(A)** on a title opens the detailed menu showing icon, TitleID, name, author, version, supported languages,
LFS-mod presence, total playtime, launches, total/NAND/SD size, total saves size, and forced language. **(ZL)**/**(ZR)**
move between titles. Three tabs, switched with **(L)**/**(R)**:

#### 5.2.1 Content records

Format: `[Location] Type | version [number] | Size`.

- **Location:** NAND or SD.
- **Type:** **Application** (base), **Update**, or **Addon** (DLC, with its number).
- **version:** decimal and `[hex]` (e.g. `786432` = 0.12.0.0).
- **(A)** opens the record's contents read-only.

**Records context menu (**(+)**):** Delete record · Move records to SD/NAND · Reset required version · **Force language**
(forcibly launch in a chosen language; shown in the "Forced Language" field) · Check integrity · Expose contents via
MTP · Dump to SD · Content info.

#### 5.2.2 Tickets

See §7. Lists the title's tickets (Personalized / Common). Context menu (**(+)**) allows deleting selected tickets.

#### 5.2.3 Saves

Per-account save view. If no save exists, one can be created via the context menu (**(+)**) for the selected account.
If a save exists:

| Action | Behavior |
| --- | --- |
| **Backup** | Back up the save (default folder `switch/DBI/saves`, `SavesFolder`). |
| **Restore** | Restore the save's backup. |
| **Save info…** | Type, size, account name, etc. |
| **Increase save size** | Increase the space allocated to the save by a specified amount. |
| **Delete** | Delete the save. |

---

## 6. Saves

**Browse saves** views, backs up, restores, and deletes saves. Entry format:
`[Account] Game-Name Backup-date Size` (Backup-date only on the Backups tab; **Account** shows the account name for
Account-type saves, otherwise the save type). Three tabs via **(L)**/**(R)**:

- **Installed** — saves for installed games.
- **Uninstalled** — saves for uninstalled games.
- **Backups** — created backups.

**Read-only view:** **Browse saves in RO mode** (`ROSaveFS`) opens the save filesystem read-only.
**Foolproof delete:** `FoolproofSaveDelete` backs up saves before deletion. **Backup folder:** `SavesFolder`.

### 6.1 Installed / Uninstalled context menu (**(+)**)

Backup · Open · **Save info…** (Id, type, size, creation time…) · Delete · **Select same app** (select all saves for
the game) · **Browse app(s)** (jump to the title's content records; **(ZL)**/**(ZR)** switch cards; Installed tab only).

### 6.2 Backups context menu (**(+)**)

**Validate saves** (integrity check) · Restore · Open · Delete · Browse app(s) · **Select same user**.
If multiple backups for the same game+user are selected, only the most recent is restored.

### 6.3 Saves via MTP & clean-save restore

- Saves can be backed up/restored by copying to/from the PC's MTP **Saves** folder. The game need not be launched
  before restoring.
- **Clean user saves** (decrypted `USER:/saves`, e.g. recovered from a damaged emuNAND via PC/Tegra Explorer) can be
  placed in the saves backup folder and restored normally; such a user's name appears in curly brackets `{}`.

### 6.4 Save sorting

`SaveSorting` (config-only) controls sort order: e.g. `AppLastPlayed,AppName,UserUid,Size,SaveId`.

---

## 7. Tickets

A **ticket** (encrypted title key) records launch rights, installed per game (`000`-suffixed TitleID),
update (`800`), and DLC.

- **Personalized ticket** (`[c]`) — issued when installing from the eShop; unique per account/console.
- **Common ticket** (`[p]`) — used for updates and as a workaround for non-eShop titles; unsigned-key, signature only.
- `+` next to a ticket indicates the corresponding game is installed.

**Browse tickets** lists installed tickets. Context menu (**(+)**) shows the selected count and offers:

- **Delete tickets** — delete the selected tickets.
- **Select same game** — select all tickets for the selected game.

Guidance: leave tickets untouched in most cases; deleting them can cause launch errors.

---

## 8. Tools

### 8.1 Maintenance tools

| Tool | Behavior |
| --- | --- |
| **Cleanup orphaned files** | Auto-clean leftover game files, failed/interrupted installs, downloaded firmware updates, and unused tickets. |
| **Delete parental controls** | Fully remove parental controls; no reboot required. |
| **Delete user…** | Remove the selected user from the system (the user's saves remain). |
| **Run random game** | Launch a random installed game. |
| **NTP time sync** | Sync console time with a remote NTP server (requires internet and a correct timezone). |
| **Check for title updates** | Check installed games for updates/DLC against the configured database (`VersionsURL`). |

`VersionsURL` accepts a remote URL or an SD file (e.g.
`https://raw.githubusercontent.com/blawar/titledb/master/versions.txt` or `sdmc:/versions.txt`).

### 8.2 MTP responder

**Run MTP responder** starts the built-in MTP server for PC/Android (USB-C OTG). On start, a window shows the account
nickname, UID, and save count. Exit MTP with **(X)** or **(B)**. The on-screen status can be toggled with **(-)**;
**Turn off screen** (`[MTP] TurnOffScreen`) blanks the screen on start.

Exposed MTP storages (each toggleable in `[MTP Storages]`, all on by default):

| # | Storage | Behavior |
| --- | --- | --- |
| 1 | **SD Card** | Browse/copy/delete SD files. Files >4 GB dropped here are auto-split into an archived folder the Switch reads as one file. (`1: SD Card`) |
| 2 | **NAND User** | Read-only browse/copy of the internal USER partition. (`2: Nand USER`) |
| 3 | **NAND System** | Read-only browse/copy of the internal SYSTEM partition. (`3: Nand SYSTEM`) |
| 4 | **Installed games** | Dump installed titles to PC as NSP (copy the game's folder). Generates a common ticket with personal info stripped. Game/update/DLC dumped as separate files; mods/cheats under `Mods & Cheats`; combined multi-content NSPs at the directory root. (`4: Installed games`) |
| 5 | **SD install** | Drop title files to install to SD. (`5: SD Card install`) |
| 6 | **NAND install** | Drop title files to install to NAND. (`6: NAND install`) |
| 7 | **Saves** | Browse/back up/restore/delete saves for all types: Account, System, BCAT, Temporary, Cache, SystemBCAT, Device. (`7: Saves`) |
| 8 | **Album** | Direct access to screenshots/videos per title (like OFW 11.0.0). (`8: Album`) |
| 9 | **Gamecard** | With a card inserted, dump full/trimmed `.XCI` plus built-in update; the personal RSA certificate is removed and dumped separately. (`9: Gamecard`) |
| # | **Custom Storage** | User-defined virtual drives from `[MTP custom storages]` (`<name>=<path>`); shown if **Show custom storages** (`CustomStorages`) is on. |

**MTP display options (`[MTP]`):** **Show combined NSP** (`ShowCombinedNSP`) toggles multi-title NSPs in Installed
games; **Show 'Mods & Cheats' folder** (`ShowMAC`) toggles the virtual mods folder
(→ `sdmc:/atmosphere/contents/<TITLEID>/`); **Use TitleID for 'Mods & Cheats'** (`MACasTID`) labels that folder by
TitleID; **Android extensions** (`ReportAndroidExtension`) toggles an Android-compatible command set (helps some
libmtp-based Mac/Linux clients). **Log all files** (`LogAllFiles`, config-only) logs all files vs. only files ≥ ~4 MB.

**Common MTP workflows:**

- **Mount installed content:** Browse installed applications → select with **(X)** → **(+)** → **Expose contents via MTP**.
- **Install mods:** MTP → Installed games → game folder → **Mods & Cheats** → copy the *contents* of the TitleID folder
  (e.g. the `romfs` folder), not the TitleID folder itself.

### 8.3 FTP server

See §3.6.

---

## 9. Activity Log

Shows per-game, per-user playtime statistics as graphs by date. Two tabs via **(L)**/**(R)**:

- **Applications** — list of games with launch statistics, in three columns: game title, launch count, time spent.
  A status line reads e.g. `[All players] 2023 January. Total: 72 hours (by play time)`. Pressing **(A)** on a game
  opens its **Activity**; pressing **(A)** drills down (year → month → day → hour).
- **Activity** — combined diagram across all games (per-game diagrams via the Applications tab).

**Hotkeys:** (L)/(R) switch tab · (ZL)/(ZR) change date · (Y) change period (all time / day / month / year) ·
(X) change sort (title / launches / time) · (+) select a user.

---

## 10. Buttons / Controls

### Main-menu and global controls

| Button | Action |
| --- | --- |
| **(A)** | Select / confirm; open folder; launch `.nro` |
| **(B)** | Cancel / back; exits DBI from the main menu |
| **(X)** | Select file; main-menu hotkey for **Run MTP responder** |
| **(Y)** | Invert selection (select all if none); main-menu hotkey for **Install from DBIbackend** |
| **(ZL) / (ZR)** | Page through menus; cycle titles in the detailed game menu |
| **(L)** | Main-menu hotkey for **Browse installed applications**; tab switch in detailed/save views |
| **(R)** | Change sort order of files/titles; tab switch |
| **(L3)** | Launch a game from the application list / detailed game menu |
| **(+)** | Open context menu (delete, reset version, MTP mount, etc.) |
| **(-)** | Toggle screen on/off in MTP / install modes |

Text-viewer and editor controls are listed in §4.5.

**Navigation options:** **Autorepeat buttons when holding** (`Autorepeat`) lets held buttons repeat navigation;
**Put cursor down after selection** (`MoveDownAfterX`) advances the cursor after selecting with **(X)**;
**Cursor on both panels** (`Secondcursor`) shows a cursor on the inactive panel in two-panel mode.

---

## 11. Warnings & Errors

### 11.1 Warnings (shown in orange — not errors)

| Warning | Meaning |
| --- | --- |
| `[SIGNATURE: Invalid]`, `[SIGNATURE: XCI->NSP]`, `[HASH NOT MATCHED TO META]`, `[HASH FIXED IN META]` | Header signature mismatches from conversion/editing/custom NSP/forwarder. Not errors. |
| `HASH MISMATCH` | Usually fine (e.g. converted from cartridge); occasionally a real integrity issue — re-download/re-check, try another cable/port/SD. |
| `[DELTA SKIPPED]` | Unneeded update fragments were skipped, as expected. |
| `No tickets found` | Title has no tickets (e.g. XCI dump or Standard Crypto conversion); does not affect functionality. |
| `Application uses AddonContent titleId`, `Application uses Update titleId` | Non-standard homebrew NSP layout; fine if it launches. |
| `This application base is not stand alone. Make sure you installed update` | Sparse Storage title — install its update before launching. |

### 11.2 Errors

| Error | Meaning / action |
| --- | --- |
| `USB communication failed` | Check/replace USB cable and PC port. |
| `Cannot parse content meta` → **Old firmware** | Firmware too old; update CFW/system. |
| `Cannot parse content meta` → **Unexpected error** | File corrupted; re-download. |
| `Invalid PFS0 magic!` | Installer file corrupted; re-download and verify. |
| `[INVALID NCA MAGIC]` | Update OFW/CFW; if it persists, re-verify the file. |
| `Installation aborted` | Transmission error; check cable/port and update DBI. |
| `Nothing to install` | Rename the file/path to remove special/Cyrillic characters. |
| `Transfer error`, `[TRANSFER CRC ERROR]`, `[TRANSFER ABORTED]` | Check cable/port/file/SD; for MTP, launch DBI via a title while holding **(R)** rather than applet mode. |
| `Error occurred: Invalid argument` | Update DBI. |
| `SOME CONTENTS ARE MISSING. APPLICATION WILL BE UNUSABLE` | Corrupt SD filesystem or bad media; run chkdsk/h2testw, reformat to FAT32. |
| `[NOT ENOUGH SPACE]`, `[CAN NOT CREATE PLACEHOLDER]` | Insufficient SD/NAND space, or bad media. |
| `Extra buffers exceeded. Media write speed is too low` | For MTP, launch via a title holding **(R)**; use a faster SD/cable/port. |
| `No tickets found but they are required` | Incomplete dump (no ticket but has title rights); use another dump. |
| `Invalid personalized ticket` | Dump left a personalized ticket instead of a common one; use a correct dump. |
| `No ES sigpatches!` | ES sigpatches missing/outdated; install the latest. |

### 11.3 Color codes

**All menus**

| Color | Meaning |
| --- | --- |
| White on black | Focused |
| Blue | Selected (with **(X)**) |

**Browse SD Card**

| Color | Meaning |
| --- | --- |
| White | Folder |
| Light grey | File |
| Dark grey | Installed game |
| Green | Update or DLC for an installed game |

**Browse installed applications**

| Color | Meaning |
| --- | --- |
| White | Installed game |
| Red | Update/DLC installed without the base game |

**Logs — during installation**

| Color | Meaning |
| --- | --- |
| Green | No errors |
| Orange | No errors, but warnings (e.g. XCI-convert NSP, hash fixed in meta) |
| Red | Error — file not installed |

**Logs — after installation**

| Color | Meaning |
| --- | --- |
| Green | Finished with no errors |
| Yellow | Finished with no errors but with warnings |
| Red | Finished with errors |

---

## Appendix — `dbi.config` Cross-reference

`dbi.config` lives beside `dbi.nro`. `true`/`Yes` and `false`/`No` are equivalent. Key sections and notable keys:

| Section | Key | Purpose |
| --- | --- | --- |
| `[General]` | `UseLibUsbHsFS` | Enable external USB drive support |
| | `ExitToHomeScreen` | Exit to HOS home screen (true) vs. hbmenu (false) |
| | `SavesFolder` | Save-backup folder |
| | `LogEvents` / `LogsFolder` | Log install/integrity/cleanup events; log folder |
| | `DumpsFolder` | Dump destination folder |
| | `AppSorting` / `SaveSorting` | Sort orders (config-only) |
| | `HighlightUpdates` | Highlight update files in browsers |
| | `RotateScreen` / `RotateJoycon` | Flip screen / controls 180° |
| | `OptimizeClockSpeed` | Underclock in menus (can cause lag on bad exit) |
| | `ROSaveFS` | Browse saves read-only |
| | `ShowUpdateFromHere` | Show "Update all titles" context action |
| | `VersionsURL` | Update-check database source |
| | `ShowCacheWarmingIndicator` | Cache-warming spinner |
| | `MoveDownAfterX` | Advance cursor after **(X)** |
| | `ScreenIdleTimeout` | Screen-off timeout (seconds) |
| | `Autorepeat` / `Secondcursor` | Held-button repeat / inactive-panel cursor |
| | `FoolproofSaveDelete` | Back up saves before delete |
| `[MainMenu]` | (per item) | Show/hide each main-menu entry (§2.1) |
| `[Applications]` | `CalculateLFSSize` | Calculate installed-mod size (slow) |
| `[Install]` | `CheckHash` | Verify `.nca` hash during install |
| | `ChunkedTransfer` | Chunked HTTP/FTP transfer |
| `[MTP]` | `ShowCombinedNSP`, `ShowMAC`, `MACasTID`, `CustomStorages`, `TurnOffScreen`, `ReportAndroidExtension`, `LogAllFiles` | MTP display/behavior options |
| `[FTP]` | `TurnOffScreen`, `UseAP`, `ReadMT` | FTP screen-off, local AP, read file dates |
| `[Access point]` | `SSID`, `Password`, `Use5GHz`, `Hidden` | Local Wi-Fi AP settings |
| `[MTP Storages]` | `1: SD Card` … `9: Gamecard` | Per-storage visibility in MTP |
| `[Network sources]` | `<name>=<type>\|<URL>` | HTTP/FTP/URLList install sources |
| `[Local sources]` | `<name>=<path>` | SD-folder shortcut menu entries |
| `[MTP custom storages]` | `<name>=<path>` | Custom MTP virtual drives |
| `[Title name override]` | `<UPPERCASE TID>=<name>` | Override displayed title names |

## 12. Exit Behavior

**Exit** quits DBI. With `ExitToHomeScreen=true` it goes directly to the HOS home screen; with `false` it returns to
hbmenu. If DBI was launched from a title/forwarder, exiting may reboot the console or leave a black screen.
Always exit through the **Exit** menu item — an incorrect exit combined with `OptimizeClockSpeed` can cause startup-screen
lag.
