// log.cpp - see log.h. Append-per-line file sink + console mirror.
#include "log.h"
#include "../build_version.h"
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <ctime>
#ifdef __SWITCH__
#include <switch.h>
#include <sys/stat.h>
#endif

namespace dbi::log {
namespace {

[[maybe_unused]] const char* kLogDir = "sdmc:/switch/open-dbi/logs";
const char* kLogPath = "sdmc:/switch/open-dbi/logs/open-dbi_latest.log";

void ensureDir() {
#ifdef __SWITCH__
    // Create the path one level at a time; EEXIST is fine.
    mkdir("sdmc:/switch", 0777);
    mkdir("sdmc:/switch/open-dbi", 0777);
    mkdir(kLogDir, 0777);
#endif
}

void nowString(char* buf, size_t n) {
    time_t t = time(nullptr);
    if (t > 0) {
        struct tm tmv;
        struct tm* g = gmtime_r(&t, &tmv);
        if (g && strftime(buf, n, "%Y-%m-%d %H:%M:%S UTC", g) > 0) return;
    }
    std::snprintf(buf, n, "time-unavailable");
}

// Mirror to console, then append to the SD log (open/append/close per line so the
// file is never held open - crash-safe and lets the report read it freely).
void emit(const char* s) {
    std::fputs(s, stdout);
#ifdef __SWITCH__
    consoleUpdate(NULL);
#endif
    FILE* f = std::fopen(kLogPath, "a");
    if (f) { std::fputs(s, f); std::fclose(f); }
}

} // namespace

const char* logPath() { return kLogPath; }

void init() {
    ensureDir();
    FILE* f = std::fopen(kLogPath, "w");   // truncate for this run
    if (f) std::fclose(f);
    char ts[40];
    nowString(ts, sizeof ts);
    line("==== Open-DBI %s  @ %s ====", OPEN_DBI_VERSION, ts);
}

void line(const char* fmt, ...) {
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    int n = std::vsnprintf(buf, sizeof buf - 2, fmt, ap);
    va_end(ap);
    if (n < 0) return;
    size_t len = (size_t)n;
    if (len > sizeof buf - 2) len = sizeof buf - 2;   // vsnprintf truncation
    buf[len] = '\n';
    buf[len + 1] = '\0';
    emit(buf);
}

void result(const char* phase, const char* op, uint32_t rc) {
    if (rc == 0) {
        line("[%s] %s: rc=0x%08X OK", phase, op, rc);
    } else {
        // libnx Result layout: module = bits[0..8], description = bits[9..21].
        line("[%s] %s: rc=0x%08X FAIL (module=%u desc=%u)",
             phase, op, rc, rc & 0x1FFu, (rc >> 9) & 0x1FFFu);
    }
}

void flush() { /* each line is flushed by close in emit() */ }

} // namespace dbi::log
