# -*- coding: utf-8 -*-
# ApplyNames.py - Ghidra headless postScript
# Reads scripts/ghidra/names.csv (address,name) and renames the functions in
# the program, tagging each with a "DBI-RE" source comment. Idempotent: re-running
# just updates names. Run after DecompileTargets/agent recovery to back-annotate.
# @category DBI
import os

from ghidra.program.model.symbol import SourceType

prog = currentProgram
af = prog.getAddressFactory()
fm = prog.getFunctionManager()

# Optional script arg = path to the names CSV (defaults to scripts/ghidra/names.csv).
_args = getScriptArgs()
if _args and _args[0].strip():
    csv_path = os.path.expanduser(_args[0].strip())
else:
    csv_path = os.path.join(
        os.path.expanduser("~/Documents/open-dbi/open-dbi/scripts/ghidra"),
        "names.csv")

applied = 0
missing = 0
for line in open(csv_path):
    line = line.strip()
    if not line or line.startswith("#") or line.startswith("address"):
        continue
    parts = line.split(",", 1)
    if len(parts) != 2:
        continue
    addr_s, name = parts[0].strip(), parts[1].strip()
    addr = af.getAddress(addr_s)
    func = fm.getFunctionAt(addr)
    if func is None:
        print("  [miss] no function at %s (%s)" % (addr_s, name))
        missing += 1
        continue
    old = func.getName()
    func.setName(name, SourceType.USER_DEFINED)
    # leave a breadcrumb in the plate comment
    pc = func.getComment()
    tag = "[DBI-RE] recovered name; was %s" % old
    if pc is None or "[DBI-RE]" not in pc:
        func.setComment(tag if pc is None else (pc + "\n" + tag))
    applied += 1
    print("  [ok] %s  %s -> %s" % (addr_s, old, name))

print("Applied %d names, %d missing." % (applied, missing))
