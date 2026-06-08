# -*- coding: utf-8 -*-
# HashLibrary.py - Ghidra headless postScript (run with -process over a project of
# imported library object files). Appends a JSONL line per function with its FID
# hash + symbol name, building the signature corpus to match against DBI.nro.
# Output: ~/switch-re/fid/libhashes.jsonl  (delete before a fresh run)
# @category DBI
import os
import json

from ghidra.feature.fid.service import FidService

svc = FidService()
fm = currentProgram.getFunctionManager()
out = os.path.expanduser("~/switch-re/fid/libhashes.jsonl")
fh = open(out, "a")
prog = currentProgram.getName()
n = 0
for f in fm.getFunctions(True):
    if f.isThunk():
        continue
    try:
        q = svc.hashFunction(f)
    except Exception:
        q = None
    if q is None:        # too small to hash
        continue
    rec = {
        "h": q.getFullHash(),
        "s": q.getSpecificHash(),
        "cu": q.getCodeUnitSize(),
        "name": f.getName(),
        "lib": prog,
    }
    fh.write(json.dumps(rec) + "\n")
    n += 1
fh.close()
print("HashLibrary: hashed %d functions in %s" % (n, prog))
