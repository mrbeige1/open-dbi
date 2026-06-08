# -*- coding: utf-8 -*-
# SelectAppCandidates.py - Ghidra headless postScript (run on DBI.nro).
# Ranks still-unknown functions (FUN_*) by how strongly they look like DBI's own
# code, using call-graph proximity to known app anchors -- a version-independent
# signal that survives the string obfuscation. Writes a ranked CSV.
# Output: ~/switch-re/exports/app_candidates.csv
# @category DBI
import os

prog = currentProgram
fm = prog.getFunctionManager()

# Strong app-indicator callees (recovered names). Calling these ~= app code.
STRONG = set([
    "dbi_util_tlsBase",        # obfuscated-string pool accessor (libs never use this)
    "dbi_globalContext",
    "dbi_config_getString",
    "dbi_config_getBool",
])
# Any callee whose name starts with these prefixes counts as an app-neighbor.
APP_PREFIXES = ("dbi_", "Pfs0Builder_", "FileManager_", "FileEntry_", "StreamWriter_")

def is_unknown(f):
    n = f.getName()
    return n.startswith("FUN_") and not f.isThunk()

def is_app_named(n):
    if n in STRONG:
        return True
    return n.startswith(APP_PREFIXES)

rows = []
for f in fm.getFunctions(True):
    if not is_unknown(f):
        continue
    try:
        callees = f.getCalledFunctions(monitor)
    except Exception:
        continue
    strong = 0
    appnb = 0
    for c in callees:
        nm = c.getName()
        if nm in STRONG:
            strong += 1
        if is_app_named(nm):
            appnb += 1
    if appnb == 0 and strong == 0:
        continue
    try:
        ncallers = f.getCallingFunctions(monitor).size()
    except Exception:
        ncallers = -1
    size = f.getBody().getNumAddresses()
    # score: strong anchors weigh most; then app-neighbors; mild size/centrality bonus
    score = strong * 10 + appnb * 3 + (1 if size >= 256 else 0) + (1 if ncallers >= 2 else 0)
    rows.append((score, str(f.getEntryPoint()), size, ncallers, len(list(callees)), strong, appnb))

rows.sort(reverse=True)
out = os.path.expanduser("~/switch-re/exports/app_candidates.csv")
fh = open(out, "w")
fh.write("score,addr,size,callers,callees,strong_anchor_calls,app_neighbor_calls\n")
for r in rows:
    fh.write("%d,%s,%d,%d,%d,%d,%d\n" % r)
fh.close()
print("App candidates (unknown funcs adjacent to app anchors): %d" % len(rows))
print("Wrote %s" % out)
# quick histogram of scores
from collections import Counter
c = Counter(r[0] for r in rows)
print("Top score buckets: %s" % sorted(c.items(), reverse=True)[:8])
