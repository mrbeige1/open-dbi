# -*- coding: utf-8 -*-
# Discover.py - Ghidra headless postScript. Multi-hunt discovery in one run:
#  (1) Ticket/es neighborhood: callers+callees of dbi_ticket_parse @710018c2d0.
#  (2) Data xrefs to the libarchive format-registration entries (find the app-side
#      codec-table builder that takes their address).
#  (3) Generic helper: for any seed addr in args, dump callers/callees.
# Writes target lists under ~/switch-re/exports/ and prints a report.
# @category DBI
import os

prog = currentProgram
af = prog.getAddressFactory()
fm = prog.getFunctionManager()
ref = prog.getReferenceManager()
st = prog.getSymbolTable()

def fn(a):
    return fm.getFunctionAt(af.getAddress(a))
def faddr(a):
    f = fm.getFunctionContaining(af.getAddress(a))
    return f
def label(f):
    if f is None: return "?"
    return "%s %s (size=%d)" % (str(f.getEntryPoint()), f.getName(), f.getBody().getNumAddresses())

union = set()

# (1) ticket neighborhood
print("==== (1) TICKET / es NEIGHBORHOOD: dbi_ticket_parse @710018c2d0 ====")
tk = fn("710018c2d0")
if tk is None:
    print("  [missing] 710018c2d0")
else:
    print("  TARGET", label(tk))
    try: callers = list(tk.getCallingFunctions(monitor))
    except: callers = []
    try: callees = list(tk.getCalledFunctions(monitor))
    except: callees = []
    print("  CALLERS (%d):" % len(callers))
    for c in callers:
        print("    <- %s" % label(c))
        if c.getName().startswith("FUN_") and not c.isThunk(): union.add(str(c.getEntryPoint()))
    print("  CALLEES (%d):" % len(callees))
    for c in callees:
        mark = "" if c.getName().startswith("FUN_") else "  [named]"
        print("    -> %s%s" % (label(c), mark))
        if c.getName().startswith("FUN_") and not c.isThunk(): union.add(str(c.getEntryPoint()))
    union.add("710018c2d0")

# (2) data xrefs to libarchive format registration entries
print("\n==== (2) DATA XREFS to codec/format registration entries ====")
reg_entries = ["71007a06d0"]   # lib_archive_read_support_format_7zip
for a in reg_entries:
    addr = af.getAddress(a)
    print("  refs to %s:" % a)
    cnt = 0
    for r in ref.getReferencesTo(addr):
        frm = r.getFromAddress()
        holder = fm.getFunctionContaining(frm)
        rtype = r.getReferenceType()
        print("    from %s in %s  (%s)" % (str(frm), label(holder), rtype))
        if holder is not None and holder.getName().startswith("FUN_"):
            union.add(str(holder.getEntryPoint()))
        cnt += 1
    if cnt == 0:
        print("    (no static refs - address only reached via computed/table init)")

# (3) generic seeds from args
args = getScriptArgs()
extra = [a.strip() for a in " ".join(args).split() if a.strip()] if args else []
for a in extra:
    f = fn(a)
    print("\n==== (3) SEED %s ====" % a)
    if f is None:
        print("  [missing]"); continue
    print("  ", label(f))
    try: callers = list(f.getCallingFunctions(monitor))
    except: callers = []
    for c in callers:
        print("    <- %s" % label(c))
        if c.getName().startswith("FUN_") and not c.isThunk(): union.add(str(c.getEntryPoint()))
    try: callees = list(f.getCalledFunctions(monitor))
    except: callees = []
    for c in callees:
        mark = "" if c.getName().startswith("FUN_") else "  [named]"
        print("    -> %s%s" % (label(c), mark))
        if c.getName().startswith("FUN_") and not c.isThunk(): union.add(str(c.getEntryPoint()))

norm = sorted(set(x.replace("0x","") for x in union))
outp = os.path.expanduser("~/switch-re/exports/targets_discover.txt")
with open(outp, "w") as fh:
    for a in norm: fh.write(a+"\n")
print("\n==== wrote %d discovery targets to %s ====" % (len(norm), outp))
