// report.h - generate a single, uploadable, REDACTED debug report.
//
// Writes sdmc:/switch/open-dbi/logs/open-dbi_report.txt: build version, the last
// install attempt's outcome, a fresh dry-run structure dump, and a copy of the
// latest log. Testers can attach this one file to a bug report.
//
// REDACTION CONTRACT: the report contains only structural metadata + libnx Result
// codes. It never includes prod.keys, titlekeys, ticket/cert payloads, or rights-id
// values - by construction, because every source it draws from (the log and the
// dry-run) only ever emits those safe fields.
#pragma once
#include <string>
#include "../io/content_source.h"
#include "../crypto/keyset.h"
#include "installer.h"   // Phase, phaseName

namespace dbi::install {

// Outcome of the most recent operation, recorded by main.cpp for the report.
struct LastOp {
    std::string name = "(none)";
    bool        attempted = false;
    bool        ok = false;
    Phase       failedPhase = Phase::Parse;   // meaningful only when attempted && !ok
    std::string error;
};

// Write the report. If `src` is non-null, a dry-run structure snapshot of it is
// included. `ks` may be null. Returns true on success.
bool writeDebugReport(const LastOp& last, io::ContentSource* src, const crypto::Keyset* ks);

} // namespace dbi::install
