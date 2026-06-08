# -*- coding: utf-8 -*-
# FindStringXrefs.py - Ghidra headless postScript. For each target substring, find
# defined strings containing it and report the functions that reference them.
# Targets come from a comma-separated script arg (default: NCA-key substrings).
# Output: ~/switch-re/exports/string_xrefs.csv
# @category DBI
import os

prog = currentProgram
listing = prog.getListing()
fm = prog.getFunctionManager()
refmgr = prog.getReferenceManager()

_args = getScriptArgs()
targets = (_args[0].split(",") if _args and _args[0].strip()
           else ["key_area_key_application", "key_area_key_ocean", "titlekek",
                 "header_key", "master_key", "Titlekey", "GetRightsId", "aes_kek"])

out = open(os.path.expanduser("~/switch-re/exports/string_xrefs.csv"), "w")
out.write("string_addr,func_addr,func_name,matched,text\n")
seen_funcs = {}
data = listing.getDefinedData(True)
for d in data:
    if not d.hasStringValue():
        continue
    try:
        s = unicode(d.getValue())
    except Exception:
        continue
    hit = None
    for t in targets:
        if t in s:
            hit = t; break
    if hit is None:
        continue
    for ref in refmgr.getReferencesTo(d.getAddress()):
        f = fm.getFunctionContaining(ref.getFromAddress())
        if f is None:
            continue
        fa = str(f.getEntryPoint())
        seen_funcs[fa] = seen_funcs.get(fa, 0) + 1
        out.write("%s,%s,%s,%s,%s\n" % (d.getAddress(), fa, f.getName(), hit,
                                        s[:40].replace(",", " ").replace("\n", " ")))
out.close()
print("Functions referencing key/crypto strings: %d" % len(seen_funcs))
for fa in sorted(seen_funcs, key=lambda k: -seen_funcs[k])[:20]:
    f = fm.getFunctionContaining(prog.getAddressFactory().getAddress(fa))
    print("  %s  refs=%d  %s" % (fa, seen_funcs[fa], f.getName() if f else "?"))
