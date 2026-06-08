# -*- coding: utf-8 -*-
# FindInstallCallers.py - Ghidra headless postScript (run on DBI.nro).
# Reads install_primitives.csv (ncm/es/spl/usbDs IPC wrappers) and finds the
# functions that CALL them, ranked by how many DISTINCT install primitives each
# touches -> the install state machine. Output: ~/switch-re/exports/install_callers.csv
# @category DBI
import os

prog = currentProgram
fm = prog.getFunctionManager()
af = prog.getAddressFactory()

_args = getScriptArgs()
_inpath = _args[0].strip() if (_args and _args[0].strip()) else "~/switch-re/exports/install_primitives.csv"
prims = {}
for line in open(os.path.expanduser(_inpath)):
    line = line.strip()
    if not line or line.startswith("address"):
        continue
    parts = line.split(",")
    a = parts[0].strip()
    nm = parts[1].strip() if len(parts) > 1 else a
    f = fm.getFunctionAt(af.getAddress(a))
    if f is not None:
        prims[f] = nm

from collections import defaultdict
callers = defaultdict(set)
for pf, nm in prims.items():
    try:
        for c in pf.getCallingFunctions(monitor):
            callers[c].add(nm)
    except Exception:
        pass

rows = []
for c, names in callers.items():
    rows.append((len(names), str(c.getEntryPoint()), c.getName(),
                 c.getBody().getNumAddresses(), "|".join(sorted(names))[:200]))
rows.sort(reverse=True)

out = open(os.path.expanduser("~/switch-re/exports/install_callers.csv"), "w")
out.write("distinct_prims,addr,name,size,primitives_called\n")
for r in rows:
    out.write("%d,%s,%s,%d,%s\n" % r)
out.close()
print("Install-path caller functions: %d" % len(rows))
for r in rows[:20]:
    print("  prims=%d %s %s size=%d" % (r[0], r[1], r[2], r[3]))
