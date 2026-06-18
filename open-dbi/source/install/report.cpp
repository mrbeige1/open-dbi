// report.cpp - see report.h.
#include "report.h"
#include "dry_run.h"
#include "../log/log.h"
#include "../build_version.h"
#include <cstdio>

namespace dbi::install {

static const char* kReportPath = "sdmc:/switch/open-dbi/logs/open-dbi_report.txt";

bool writeDebugReport(const LastOp& last, io::ContentSource* src, const crypto::Keyset* ks) {
    // 1) Append the last-op summary + a fresh structure snapshot to the live log, so
    //    the copied log carries everything the report should contain.
    dbi::log::line("---- debug report requested ----");
    dbi::log::line("[report] last op: %s | attempted=%d ok=%d | phase=%s | error=%s",
                   last.name.c_str(), (int)last.attempted, (int)last.ok,
                   phaseName(last.failedPhase),
                   last.error.empty() ? "(none)" : last.error.c_str());
    if (src) {
        dbi::log::line("[report] structure snapshot follows:");
        dryRunInstall(*src, ks);
    } else {
        dbi::log::line("[report] no input file available for a structure snapshot");
    }

    // 2) Copy the latest log into the report file behind a header + redaction notice.
    FILE* out = std::fopen(kReportPath, "w");
    if (!out) { dbi::log::line("[report] cannot open %s", kReportPath); return false; }
    std::fprintf(out,
        "==================== Open-DBI debug report ====================\n"
        "build: %s\n"
        "REDACTED: contains only file structure, metadata, and libnx Result\n"
        "codes. No keys, titlekeys, ticket/cert payloads, or rights-id values.\n"
        "Safe to attach to a bug report.\n"
        "===============================================================\n\n"
        "----- last operation -----\n"
        "name=%s attempted=%d ok=%d failedPhase=%s\n"
        "error=%s\n\n"
        "----- latest log (%s) -----\n",
        OPEN_DBI_VERSION,
        last.name.c_str(), (int)last.attempted, (int)last.ok, phaseName(last.failedPhase),
        last.error.empty() ? "(none)" : last.error.c_str(),
        dbi::log::logPath());

    FILE* in = std::fopen(dbi::log::logPath(), "rb");
    if (in) {
        char buf[1024];
        size_t n;
        while ((n = std::fread(buf, 1, sizeof buf, in)) > 0) std::fwrite(buf, 1, n, out);
        std::fclose(in);
    } else {
        std::fprintf(out, "(log file unavailable)\n");
    }
    std::fclose(out);
    dbi::log::line("[report] wrote %s", kReportPath);
    return true;
}

} // namespace dbi::install
