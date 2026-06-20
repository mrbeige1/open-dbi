#!/usr/bin/env python3
# merge_batch.py - host-side merge of names_frag/*.csv for a batch.
# Rule: the FIRST line of each frag file is the authoritative name for that
# frag's target addr. Callee names are vote-counted; an authoritative name
# always wins over votes. Usage:
#   merge_batch.py <out_csv> <addr1> <addr2> ...
import os, sys, collections

EXPORTS = os.path.expanduser("~/switch-re/exports")
out_csv = sys.argv[1]
batch = [a.lower().replace("0x", "") for a in sys.argv[2:]]

authoritative = {}
votes = collections.defaultdict(collections.Counter)
missing = []
for t in batch:
    p = os.path.join(EXPORTS, "names_frag", "%s.csv" % t)
    if not os.path.exists(p):
        missing.append(t); continue
    first = True
    for line in open(p):
        line = line.strip()
        if not line or line.startswith("#") or line.lower().startswith("address"):
            continue
        a, _, n = line.partition(",")
        a = a.strip().lower().replace("0x", ""); n = n.strip()
        if not n:
            continue
        if first:
            authoritative[a] = n; first = False
        votes[a][n] += 1

final = {}
conflicts = []
for a, cnt in votes.items():
    if a in authoritative:
        final[a] = authoritative[a]
        if any(k != authoritative[a] for k in cnt):
            conflicts.append((a, final[a], dict(cnt), "auth"))
    else:
        top = cnt.most_common()
        final[a] = top[0][0]
        if len(cnt) > 1:
            conflicts.append((a, final[a], dict(cnt), "vote"))

with open(os.path.join(EXPORTS, out_csv), "w") as f:
    f.write("address,name\n")
    for a in sorted(final):
        f.write("%s,%s\n" % (a, final[a]))

print("merged %d names -> %s" % (len(final), out_csv))
if missing:
    print("MISSING frags:", missing)
for a, chosen, cnt, how in sorted(conflicts):
    print("  [%s] %s -> %s   (saw %s)" % (how, a, chosen, cnt))
