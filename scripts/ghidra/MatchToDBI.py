# -*- coding: utf-8 -*-
# MatchToDBI.py - Ghidra headless postScript (run on DBI.nro).
# Loads the library FID-hash corpus (~/switch-re/fid/libhashes.jsonl) and matches
# each DBI function against it. Reports match rates at two strictness levels and
# writes ~/switch-re/fid/matches.csv. Pass "apply" as a script arg to also rename
# uniquely-matched functions in the DB (lib_<name>).
# @category DBI
import os
import json

from ghidra.feature.fid.service import FidService
from ghidra.program.model.symbol import SourceType

APPLY = ("apply" in [a.lower() for a in getScriptArgs()])

# Load corpus -> two indexes: by fullHash, and by (fullHash, codeUnitSize)
by_full = {}
by_full_cu = {}
path = os.path.expanduser("~/switch-re/fid/libhashes.jsonl")
for line in open(path):
    line = line.strip()
    if not line:
        continue
    r = json.loads(line)
    by_full.setdefault(r["h"], set()).add(r["name"])
    by_full_cu.setdefault((r["h"], r["cu"]), set()).add(r["name"])

svc = FidService()
fm = currentProgram.getFunctionManager()

total = 0
hashable = 0
m_full = 0          # matched on fullHash (loose)
m_full_cu = 0       # matched on (fullHash, codeUnitSize) (strict)
unique_strict = 0   # strict match resolving to exactly one name
applied = 0

out = open(os.path.expanduser("~/switch-re/fid/matches.csv"), "w")
out.write("addr,dbi_name,strictness,lib_names\n")
for f in fm.getFunctions(True):
    total += 1
    if f.isThunk():
        continue
    try:
        q = svc.hashFunction(f)
    except Exception:
        q = None
    if q is None:
        continue
    hashable += 1
    h = q.getFullHash()
    key = (h, q.getCodeUnitSize())
    if key in by_full_cu:
        m_full_cu += 1
        names = by_full_cu[key]
        out.write("%s,%s,strict,%s\n"
                  % (f.getEntryPoint(), f.getName(), "|".join(sorted(names))[:160]))
        if len(names) == 1:
            unique_strict += 1
            if APPLY and f.getName().startswith("FUN_"):
                nm = list(names)[0]
                safe = "lib_" + "".join(c if (c.isalnum() or c == "_") else "_" for c in nm)[:96]
                f.setName(safe, SourceType.IMPORTED)
                applied += 1
    elif h in by_full:
        m_full += 1
        out.write("%s,%s,loose,%s\n"
                  % (f.getEntryPoint(), f.getName(), "|".join(sorted(by_full[h]))[:160]))
out.close()

print("=================== FID MATCH (libnx pilot) ===================")
print("DBI total functions   : %d" % total)
print("  hashable (non-tiny) : %d" % hashable)
print("  strict (hash+size)  : %d" % m_full_cu)
print("    of which unique   : %d" % unique_strict)
print("  loose (hash only)   : %d (additional)" % m_full)
print("  corpus size         : %d fullhashes / %d (hash,cu) keys"
      % (len(by_full), len(by_full_cu)))
if APPLY:
    print("  applied names       : %d" % applied)
print("===============================================================")
