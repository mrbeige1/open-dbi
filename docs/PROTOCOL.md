# DBI Transport Protocols

DBI installs/serves files over several transports. This document specifies the two that are
device-native and reverse-engineered here:

1. **The `DBI0` USB protocol** — the custom bulk-USB protocol DBI uses to pull files from a PC
   running the `dbibackend` host script. Fully specified below (derived from the open `dbibackend`
   script and cross-checked against strings in `DBI.nro`).
2. **MTP** — standard Media Transfer Protocol responder/initiator (`DBI MTP`, vendored `capmtp` /
   `mtp-server-nx`). Summarized; the wire format is the PTP/MTP standard.

Other transports (HTTP home-server, FTP, SFTP via libssh2) use their standard protocols and are
covered in `FEATURES.md`.

---

## 1. The `DBI0` USB protocol

### Roles
- **Switch = initiator.** DBI on the console sends commands; the PC responds. (The PC `dbibackend`
  is a passive server: it blocks reading command headers and answers them.)
- **PC = file server.** Holds a map of `{ filename → absolute path }` and streams requested byte
  ranges back.

### USB device
| Field | Value |
|-------|-------|
| idVendor | `0x057E` (Nintendo) |
| idProduct | `0x3000` |
| Interface | configuration `(0,0)`; one bulk **IN** + one bulk **OUT** endpoint |

The host resets the device, sets configuration, and resolves the IN/OUT bulk endpoints by direction.
On any USB error mid-session, the host loops back to waiting for the device (hot reconnect).

### Packet header (16 bytes, little-endian)

Every message — in both directions — begins with this fixed 16-byte header:

```
offset  size  field      type     notes
0x00    4     magic      char[4]  always ASCII "DBI0"
0x04    4     cmd_type   u32      0=REQUEST, 1=RESPONSE, 2=ACK
0x08    4     cmd_id     u32      0=EXIT, 1=LIST_OLD(unused), 2=FILE_RANGE, 3=LIST
0x0C    4     data_size  u32      meaning depends on command (see below)
```

`struct` format (as in `dbibackend`): `'<4sIII'`. Headers whose magic is not `DBI0` are ignored.

### Command IDs
| Value | Name | Direction (initiator→) | Purpose |
|------:|------|------------------------|---------|
| 0 | `CMD_ID_EXIT` | Switch → PC | Tell host to shut down the server |
| 1 | `CMD_ID_LIST_OLD` | — | Legacy list command; **not implemented** by the host |
| 2 | `CMD_ID_FILE_RANGE` | Switch → PC | Request a byte range of a named file |
| 3 | `CMD_ID_LIST` | Switch → PC | Request the list of available filenames |

### Command type
| Value | Name | Meaning |
|------:|------|---------|
| 0 | `CMD_TYPE_REQUEST` | An initiating request |
| 1 | `CMD_TYPE_RESPONSE` | A response carrying/announcing a payload |
| 2 | `CMD_TYPE_ACK` | Acknowledgement (host ready to receive the request body) |

### Flows

#### `LIST` (cmd_id 3) — enumerate available files
```
Switch → PC : [DBI0, REQUEST, LIST, 0]
PC          : builds payload = "name1\nname2\n...\n" (UTF-8), len = N bytes
PC → Switch : [DBI0, RESPONSE, LIST, N]
  if N > 0:
    Switch → PC : [DBI0, ACK, LIST, *]      (16-byte ack header; host reads & ignores fields)
    PC → Switch : <N bytes of newline-joined filename list>
```
Filenames (basenames, not full paths) are the identifiers used by all later requests. Duplicate
basenames across folders collide by design (last one wins in the host map).

#### `FILE_RANGE` (cmd_id 2) — stream a byte range
The request body (length = the incoming header's `data_size`) is:
```
offset  size  field          type      notes
0x00    4     range_size     u32       number of bytes to send back
0x04    8     range_offset   u64       byte offset into the file
0x0C    4     nsp_name_len   u32       length of the filename that follows
0x10    var   nsp_name       char[]    UTF-8 filename (key into the file map)
```
Sequence:
```
Switch → PC : [DBI0, REQUEST, FILE_RANGE, data_size]
PC → Switch : [DBI0, ACK,      FILE_RANGE, data_size]   (host ready for the request body)
Switch → PC : <file_range_header bytes>                  (the struct above)
PC → Switch : [DBI0, RESPONSE, FILE_RANGE, range_size]   (announces payload size)
Switch → PC : [DBI0, ACK,      FILE_RANGE, *]            (16-byte ack)
PC → Switch : <range_size bytes>  streamed in chunks of BUFFER_SEGMENT_DATA_SIZE (0x100000 = 1 MiB)
```
The host `seek`s to `range_offset` and writes `range_size` bytes in 1 MiB chunks (final chunk
truncated to the remainder). This is the install hot path: DBI requests successive ranges as it
writes content to NAND/SD.

#### `EXIT` (cmd_id 0) — shut down host
```
Switch → PC : [DBI0, REQUEST, EXIT, 0]
PC → Switch : [DBI0, RESPONSE, EXIT, 0]
PC          : quits the GUI / server loop
```

### Notes & open questions for verification against the binary
- The host's main loop only dispatches `EXIT`, `FILE_RANGE`, `LIST`. The Switch side presumably
  issues `LIST` first, then a series of `FILE_RANGE` requests per file it installs.
- `data_size` on the ACK packets is read but unused by the host — confirm whether the DBI side
  validates it.
- All multi-byte fields are little-endian; the only `u64` is `range_offset`.
- Endpoint MTU / max bulk transfer size is left to libusb/libnx; the 1 MiB chunking is a host-side
  software segment size, not a USB limit.

---

## 2. MTP (Media Transfer Protocol)

DBI exposes an **MTP responder** ("Run MTP responder" menu) and also acts as an **MTP initiator**
for mounting external/USB content. Strings in the binary indicate a vendored MTP stack
(`capmtp`, `mtp-server-nx`, `DBI MTP`, namespace `dbi::mtp`, exception `dbi::mtp::MtpInitiatorException`).

- **Wire format:** standard USB PTP/MTP (operation/response/data containers). Not custom — implement
  against the MTP spec, not by reversing the framing.
- **Custom storages:** DBI supports user-defined MTP storages (`[MTP Storages]` config section,
  "MTP custom storages") that map MTP storage IDs to on-device paths — see `CONFIG.md`.
- **Debug logging:** `Start/Finish MTP debug log` (`Mtp_Debug`, `MtpI_Debug`) toggles a protocol trace.

Reverse-engineering focus for MTP is therefore the **storage/object mapping logic and the
DBI-specific operation handlers**, not the transport framing.

---

## Cross-references
- Host implementation of the `DBI0` protocol: `../dbibackend`
- Config keys affecting transports: `CONFIG.md` (`[MTP]`, `[MTP Storages]`, `[FTP]`, `[Access point]`)
- User-facing description of each source: `FEATURES.md`
