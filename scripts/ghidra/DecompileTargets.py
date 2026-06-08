# -*- coding: utf-8 -*-
# DecompileTargets.py - Ghidra headless postScript
# Decompiles the functions whose entry addresses are listed in
# ~/switch-re/exports/targets.txt and writes one .c file per function plus a
# combined decompiled.txt. Feeds the Phase 3 agent recovery pipeline.
# @category DBI
import os

from ghidra.app.decompiler import DecompInterface
from ghidra.util.task import ConsoleTaskMonitor

prog = currentProgram
af = prog.getAddressFactory()
fm = prog.getFunctionManager()

exports = os.path.expanduser("~/switch-re/exports")
targets_file = os.path.join(exports, "targets.txt")
outdir = os.path.join(exports, "decompiled")
try:
    os.makedirs(outdir)
except OSError:
    pass

addrs = []
for line in open(targets_file):
    line = line.strip()
    if line and not line.startswith("#"):
        addrs.append(line)

ifc = DecompInterface()
ifc.openProgram(prog)
mon = ConsoleTaskMonitor()

combined = open(os.path.join(exports, "decompiled.txt"), "w")
ok = 0
for a in addrs:
    addr = af.getAddress(a)
    func = fm.getFunctionAt(addr)
    if func is None:
        print("  [skip] no function at %s" % a)
        continue
    res = ifc.decompileFunction(func, 90, mon)
    if res is None or not res.decompileCompleted():
        print("  [fail] decompile %s" % a)
        continue
    c = res.getDecompiledFunction().getC()
    # per-function file
    fpath = os.path.join(outdir, "%s.c" % a)
    fp = open(fpath, "w")
    fp.write(c)
    fp.close()
    combined.write("/* ===== %s (%s) size=%d ===== */\n"
                   % (a, func.getName(), func.getBody().getNumAddresses()))
    combined.write(c)
    combined.write("\n\n")
    ok += 1
    print("  [ok] %s -> %s.c (%d bytes C)" % (a, a, len(c)))
combined.close()
print("Decompiled %d/%d functions into %s" % (ok, len(addrs), outdir))
print("Combined: %s/decompiled.txt" % exports)
