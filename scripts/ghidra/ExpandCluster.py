# -*- coding: utf-8 -*-
# ExpandCluster.py - Ghidra headless postScript.
# Given a seed set of addresses (the decompression cluster), map one hop of
# callers and callees, print a readable neighborhood, and write the union of
# seeds + unknown (FUN_*) neighbors to targets.txt for DecompileTargets.py.
# Seeds are passed as script args (space-separated hex addrs); falls back to the
# NSZ/XCZ decompression cluster found via functions_context.jsonl.
# @category DBI
import os

prog = currentProgram
af = prog.getAddressFactory()
fm = prog.getFunctionManager()

args = getScriptArgs()
if args and args[0].strip():
    seeds = [a.strip() for a in " ".join(args).split() if a.strip()]
else:
    seeds = ["71006ceaa0","71006ceb50","71006cebf0","71006cec90","71006ced60",
             "71006cf200","7100700bc0","7100700e30","710079b5c0","710079b680"]

def fn(a):
    return fm.getFunctionAt(af.getAddress(a))

def label(f):
    return "%s %s (size=%d)" % (str(f.getEntryPoint()), f.getName(),
                                f.getBody().getNumAddresses())

union = set(seeds)
print("==== CLUSTER NEIGHBORHOOD ====")
for a in seeds:
    f = fn(a)
    if f is None:
        print("[missing] %s" % a); continue
    print("\n### SEED %s" % label(f))
    try:
        callers = list(f.getCallingFunctions(monitor))
    except Exception:
        callers = []
    try:
        callees = list(f.getCalledFunctions(monitor))
    except Exception:
        callees = []
    print("  CALLERS (%d):" % len(callers))
    for c in callers:
        print("    <- %s" % label(c))
        if c.getName().startswith("FUN_") and not c.isThunk():
            union.add(str(c.getEntryPoint()))
    print("  CALLEES (%d):" % len(callees))
    for c in callees:
        mark = "" if c.getName().startswith("FUN_") else "  [named]"
        print("    -> %s%s" % (label(c), mark))
        if c.getName().startswith("FUN_") and not c.isThunk():
            union.add(str(c.getEntryPoint()))

# normalize to bare hex (strip any leading zeros style from toString)
norm = sorted(set(x.replace("0x","") for x in union))
outp = os.path.expanduser("~/switch-re/exports/targets_decomp.txt")
fh = open(outp, "w")
for a in norm:
    fh.write(a + "\n")
fh.close()
print("\n==== wrote %d targets to %s ====" % (len(norm), outp))
