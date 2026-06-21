# -*- coding: utf-8 -*-
# PickTopUnknown.py - Ghidra headless postScript.
# Reads ~/switch-re/exports/app_candidates.csv (ranked), keeps only addrs whose
# function is STILL unknown (FUN_*) and not a thunk, takes the top N, and writes
# them to targets.txt for a following DecompileTargets.py postScript.
# N and an optional skip-count come from script args: PickTopUnknown.py [N] [skip]
# @category DBI
import os

prog = currentProgram
af = prog.getAddressFactory()
fm = prog.getFunctionManager()

args = getScriptArgs()
N = int(args[0]) if len(args) >= 1 and args[0].strip() else 24
skip = int(args[1]) if len(args) >= 2 and args[1].strip() else 0

cand = os.path.expanduser("~/switch-re/exports/app_candidates.csv")
picked = []
seen_skipped = 0
for line in open(cand):
    line = line.strip()
    if not line or line.startswith("score"):
        continue
    parts = line.split(",")
    if len(parts) < 2:
        continue
    addr = parts[1].strip()
    f = fm.getFunctionAt(af.getAddress(addr))
    if f is None or f.isThunk():
        continue
    if not f.getName().startswith("FUN_"):
        continue   # already named since the CSV was generated
    if seen_skipped < skip:
        seen_skipped += 1
        continue
    picked.append((addr, parts[0], f.getBody().getNumAddresses()))
    if len(picked) >= N:
        break

outp = os.path.expanduser("~/switch-re/exports/targets.txt")
with open(outp, "w") as fh:
    fh.write("# top %d still-unknown app candidates (skip=%d)\n" % (len(picked), skip))
    for a, s, sz in picked:
        fh.write("%s\n" % a)
print("Picked %d unknown app candidates (skip=%d) -> targets.txt" % (len(picked), skip))
for a, s, sz in picked:
    print("  %s  score=%s  size=%d" % (a, s, sz))
