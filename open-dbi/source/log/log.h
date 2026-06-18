// log.h - structured, always-on diagnostic log for the open-dbi NRO.
//
// Writes to sdmc:/switch/open-dbi/logs/open-dbi_latest.log (own namespace; does
// NOT collide with DBI's sdmc:/switch/DBI/logs/) and mirrors every line to the
// console. The file is appended per line and closed each time, so a log survives
// a hang/crash mid-install - which is exactly when testers need it.
//
// Redaction contract: callers must never pass keys, titlekeys, ticket/cert bytes,
// or rights-id values. Only structural metadata + libnx Result codes belong here,
// so the log (and the report bundle built from it) is safe to upload.
#pragma once
#include <cstdint>

namespace dbi::log {

// Truncate the SD log and write the run header (build version + timestamp).
void init();

// printf-style line: appended to the SD log and mirrored to the console.
void line(const char* fmt, ...) __attribute__((format(printf, 1, 2)));

// Log a libnx Result as hex, tagged with phase + operation. Logged whether the
// Result indicates success (rc==0) or failure; decodes module/description on fail.
void result(const char* phase, const char* op, uint32_t rc);

// No-op kept for API symmetry (each line is already flushed to disk).
void flush();

// Absolute path of the current log file (used by the report bundle).
const char* logPath();

} // namespace dbi::log
