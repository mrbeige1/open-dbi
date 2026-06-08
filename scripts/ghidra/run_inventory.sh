#!/bin/bash
# Run the Inventory.py headless postScript against the existing DBI Ghidra project.
# Requires the Ghidra GUI to be CLOSED (project lock released).
set -e
export JAVA_HOME="/opt/homebrew/opt/openjdk@21/libexec/openjdk.jdk/Contents/Home"

GHIDRA="$HOME/switch-re/ghidra_11.0_PUBLIC"
PROJ_LOC="$HOME"          # project location (where DBI.gpr/.rep live)
PROJ_NAME="DBI"
PROGRAM="DBI.nro"         # program name inside the project
SCRIPTS="$HOME/Documents/dbibackend_reversed/scripts/ghidra"

"$GHIDRA/support/analyzeHeadless" "$PROJ_LOC" "$PROJ_NAME" \
  -process "$PROGRAM" \
  -noanalysis \
  -scriptPath "$SCRIPTS" \
  -postScript Inventory.py
