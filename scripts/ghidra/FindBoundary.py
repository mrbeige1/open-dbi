# -*- coding: utf-8 -*-
# FindBoundary.py - Ghidra headless postScript.
# BFS upward through callers from seed addresses until a NAMED (non-FUN_)
# function is reached. Reports the named "entry" functions that reach the seeds
# -- i.e. where DBI's own code (or a recognizable library) calls into the
# unknown cluster. Seeds via script args; default = decompression lib entries.
# @category DBI
import os, collections

prog = currentProgram
af = prog.getAddressFactory()
fm = prog.getFunctionManager()

args = getScriptArgs()
if args and args[0].strip():
    seeds = [a.strip() for a in " ".join(args).split() if a.strip()]
else:
    seeds = ["710079ef70", "71007a06d0", "710079d970"]

def fn(a):
    return fm.getFunctionAt(af.getAddress(a))

MAX_DEPTH = 8
seen = set()
named_entries = {}     # addr -> (name, depth, via)
q = collections.deque()
for s in seeds:
    q.append((s, 0, None))
    seen.add(s)

while q:
    addr, depth, via = q.popleft()
    f = fn(addr)
    if f is None or depth > MAX_DEPTH:
        continue
    try:
        callers = list(f.getCallingFunctions(monitor))
    except Exception:
        callers = []
    for c in callers:
        nm = c.getName()
        ca = str(c.getEntryPoint())
        if not nm.startswith("FUN_") and not nm.startswith("thunk_"):
            # reached a named function = boundary
            if ca not in named_entries:
                named_entries[ca] = (nm, depth + 1, addr)
            continue
        if ca not in seen and len(seen) < 4000:
            seen.add(ca)
            q.append((ca, depth + 1, addr))

print("==== NAMED ENTRY POINTS reaching the cluster (boundary) ====")
if not named_entries:
    print("  (none within depth %d - cluster is self-contained / indirect-call only)" % MAX_DEPTH)
for ca, (nm, depth, via) in sorted(named_entries.items(), key=lambda kv: kv[1][1]):
    print("  %s  %s   (depth=%d, via %s)" % (ca, nm, depth, via))
print("\nVisited %d unknown functions while walking up." % len(seen))
