# -*- coding: utf-8 -*-
# Inventory.py - Ghidra headless postScript
# Verifies analysis quality on DBI.nro and exports a function inventory.
# Run via analyzeHeadless in -process mode (see scripts/ghidra/run_inventory.sh).
# @category DBI
import os

prog = currentProgram  # noqa: provided by Ghidra
print("=================== DBI.nro INVENTORY ===================")
print("Program     : %s" % prog.getName())
print("Language    : %s" % prog.getLanguageID())
print("Compiler    : %s" % prog.getCompilerSpec().getCompilerSpecID())
print("Image base  : %s" % prog.getImageBase())

mem = prog.getMemory()
print("--- Memory blocks ---")
for b in mem.getBlocks():
    print("  %-12s %s - %s  exec=%-5s  size=%d"
          % (b.getName(), b.getStart(), b.getEnd(), b.isExecute(), b.getSize()))

fm = prog.getFunctionManager()
funcs = list(fm.getFunctions(True))
externals = list(fm.getExternalFunctions())
print("--- Functions ---")
print("Defined functions : %d" % len(funcs))
print("External functions: %d" % len(externals))

# Count how many functions have a non-default (recovered/named) symbol vs FUN_*/thunks
named = 0
thunks = 0
for fn in funcs:
    if fn.isThunk():
        thunks += 1
    n = fn.getName()
    if not (n.startswith("FUN_") or n.startswith("thunk_FUN_")):
        named += 1
print("Named (non-FUN_*) : %d" % named)
print("Thunks            : %d" % thunks)

# Export inventory CSV
outdir = os.path.expanduser("~/switch-re/exports")
try:
    os.makedirs(outdir)
except OSError:
    pass
csv_path = os.path.join(outdir, "functions.csv")
fh = open(csv_path, "w")
fh.write("address,size,name,namespace\n")
for fn in funcs:
    name = fn.getName().replace(",", ";")
    ns = fn.getParentNamespace().getName().replace(",", ";")
    fh.write("%s,%d,%s,%s\n"
             % (fn.getEntryPoint(), fn.getBody().getNumAddresses(), name, ns))
fh.close()
print("Wrote function inventory: %s" % csv_path)
print("========================================================")
