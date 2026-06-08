# -*- coding: utf-8 -*-
# ExportContext.py - Ghidra headless postScript
# For every function, export cheap structural context (referenced strings,
# callee/caller counts, size) as JSONL, and apply a heuristic library-vs-app
# "hint" based on referenced library strings. Does NOT decompile (fast pass).
# Output: ~/switch-re/exports/functions_context.jsonl  +  triage_summary.txt
# @category DBI
import os
import json

prog = currentProgram          # provided by Ghidra
listing = prog.getListing()
fm = prog.getFunctionManager()

# Library string keywords -> library label. A function referencing these strings
# is a HINT (not proof) that it is third-party code to subtract.
LIB_KEYWORDS = {
    "curl":      ["libcurl", "CURLOPT", "Curl_", "Transfer-Encoding", "CURLE_", "curl_"],
    "zlib":      ["inflate", "deflate", "Mark Adler", "incorrect header check", "zlib"],
    "zstd":      ["Zstandard", "ZSTD", "zstd"],
    "mbedtls":   ["BIGNUM", "X509", "mbedtls", "AES-128", "AES-256", "PKCS"],
    "libssh2":   ["libssh2", "SSH-2.0", "SFTP", "_libssh2"],
    "newlib":    ["newlib", "/libc/", "/stdlib/", "/stdio/"],
    "libstdc++": ["basic_string", "__cxa", "_Rb_tree", "bad_alloc", "St9"],
    "nlohmann":  ["nlohmann", "json.exception", "[json.exception"],
    "fmt":       ["fmt::", "format_error"],
    "pugixml":   ["pugixml", "xpath", "PCDATA"],
    "lzma":      ["7-Zip", "Lzma", "LZMA"],
    "bzip2":     ["bzip", "bzlib"],
    "mtp":       ["capmtp", "MTP", "ObjectHandle", "StorageID"],
}
# App namespace anchors (leaked RTTI). Functions referencing these are likely app.
APP_KEYWORDS = ["dbi::", "N3dbi", "nx::utils", "N2nx", "DBI0", "dbi.config",
                "sdmc:/switch/DBI"]


def strings_in_function(func):
    out = []
    insns = listing.getInstructions(func.getBody(), True)
    for ins in insns:
        for ref in ins.getReferencesFrom():
            data = listing.getDataAt(ref.getToAddress())
            if data is not None and data.hasStringValue():
                try:
                    out.append(unicode(data.getValue()))
                except Exception:
                    pass
    return out


def classify(strs):
    hits = {}
    app_hit = False
    for s in strs:
        for kw in APP_KEYWORDS:
            if kw in s:
                app_hit = True
        for lib, kws in LIB_KEYWORDS.items():
            for kw in kws:
                if kw in s:
                    hits[lib] = hits.get(lib, 0) + 1
    if app_hit:
        return "app", hits
    if hits:
        return "library", hits
    return "unknown", hits


outdir = os.path.expanduser("~/switch-re/exports")
try:
    os.makedirs(outdir)
except OSError:
    pass

jsonl_path = os.path.join(outdir, "functions_context.jsonl")
fh = open(jsonl_path, "w")

counts = {"app": 0, "library": 0, "unknown": 0}
lib_breakdown = {}
n = 0
funcs = fm.getFunctions(True)
for func in funcs:
    n += 1
    strs = strings_in_function(func)
    cls, hits = classify(strs)
    counts[cls] += 1
    for lib in hits:
        lib_breakdown[lib] = lib_breakdown.get(lib, 0) + 1
    try:
        callees = func.getCalledFunctions(monitor).size()
        callers = func.getCallingFunctions(monitor).size()
    except Exception:
        callees = callers = -1
    rec = {
        "addr": str(func.getEntryPoint()),
        "size": func.getBody().getNumAddresses(),
        "name": func.getName(),
        "class_hint": cls,
        "lib_hits": hits,
        "callees": callees,
        "callers": callers,
        "n_strings": len(strs),
        # cap strings stored to keep file sane
        "strings": strs[:40],
    }
    fh.write(json.dumps(rec) + "\n")
    if n % 1000 == 0:
        print("  processed %d functions..." % n)
fh.close()

summary = os.path.join(outdir, "triage_summary.txt")
sh = open(summary, "w")
sh.write("DBI.nro function triage (string/RTTI heuristic)\n")
sh.write("Total functions: %d\n" % n)
sh.write("  app-hint     : %d\n" % counts["app"])
sh.write("  library-hint : %d\n" % counts["library"])
sh.write("  unknown      : %d\n" % counts["unknown"])
sh.write("\nLibrary-hit breakdown (functions referencing each library's strings):\n")
for lib in sorted(lib_breakdown, key=lambda k: -lib_breakdown[k]):
    sh.write("  %-12s %d\n" % (lib, lib_breakdown[lib]))
sh.close()

print("=================== TRIAGE COMPLETE ===================")
print("Total functions : %d" % n)
print("  app-hint      : %d" % counts["app"])
print("  library-hint  : %d" % counts["library"])
print("  unknown       : %d" % counts["unknown"])
print("Library breakdown:")
for lib in sorted(lib_breakdown, key=lambda k: -lib_breakdown[k]):
    print("  %-12s %d" % (lib, lib_breakdown[lib]))
print("Wrote %s" % jsonl_path)
print("Wrote %s" % summary)
print("=======================================================")
