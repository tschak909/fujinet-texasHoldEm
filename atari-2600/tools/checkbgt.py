#!/usr/bin/env python3
"""checkbgt.py -- every bank that draws the table must agree about its rows.

CDBGT is the display kernel's row-background table, and each bank has to carry
its own copy: a bank switch replaces every byte of $1000-$17FF, and each bank
is a different screen -- the lobby's list wants no bands where the table's
eight two-row seats need them to separate at all.

THREE COPIES IS THE ARCHITECTURE. THREE SOURCES OF TRUTH WAS A BUG. When this
port moved the seats to row 0 and the board to 16-17, cdgame and cdcomp were
edited and cdnet was not -- and cdnet draws the table too, because NFRAME keeps
the picture up through a /state rather than blanking the screen for the round
trip. The symptom is easy to misread: the table looked right, and every poll
the grey bands jumped by one row for the length of the transaction.

They come from a shared include now, so this cannot drift -- which is exactly
when a check is worth having, because the next person to need a bank-local
tweak will reach for a local copy. It reads the bytes back out of each bank's
own listing rather than trusting the source.

ONLY THE BANKS THAT DRAW THE TABLE ARE PASSED IN. cdlobby, cdmenu and cdname
draw different screens and their tables are correctly different -- a list wants
no bands at all. The caller names the set; a bank in the set with no CDBGT at
all is skipped rather than failed, because a bank that draws nothing has
nothing to disagree about.

Usage: checkbgt.py NAME=listing.lst [NAME=listing.lst ...]
"""
import re
import sys

ROWS = 22               # 21 rows plus the sentinel the last seam programmes
SYM = re.compile(r"\bCDBGT\s*:\s*([0-9A-F]{4})\s+C\b")
# An AS listing line, with the include-depth prefix that "(N)" adds.
LINE = re.compile(r"\s*(?:\(\d+\)\s*)?\d+/([0-9A-Fa-f]{4})\s*:\s*"
                  r"((?:[0-9A-Fa-f]{2} )+)")


def table(path):
    """The ROWS bytes emitted at CDBGT, or None if this bank has no CDBGT."""
    text = open(path, encoding="utf-8", errors="replace").read()
    sym = SYM.search(text)
    if not sym:
        return None
    base = int(sym.group(1), 16)
    seen = {}
    for line in text.splitlines():
        m = LINE.match(line)
        if not m:
            continue
        addr = int(m.group(1), 16)
        for i, byte in enumerate(m.group(2).split()):
            if base <= addr + i < base + ROWS:
                seen[addr + i] = byte.upper()
    if len(seen) != ROWS:
        return ("INCOMPLETE", base, len(seen))
    return ("OK", base, " ".join(seen[base + i] for i in range(ROWS)))


def main(argv):
    if len(argv) < 2:
        sys.exit(__doc__)
    tables, bad = {}, False
    for arg in argv[1:]:
        name, _, path = arg.partition("=")
        got = table(path)
        if got is None:
            continue                    # a bank that draws no table: fine
        kind, base, rest = got
        if kind == "INCOMPLETE":
            print(f"checkbgt: {name}: CDBGT at ${base:04X} emitted {rest} of "
                  f"{ROWS} bytes", file=sys.stderr)
            bad = True
            continue
        tables[name] = rest

    if not tables:
        print("checkbgt: no bank defines CDBGT", file=sys.stderr)
        return 1

    distinct = set(tables.values())
    if len(distinct) > 1:
        print("checkbgt: the table banks DISAGREE about their row backgrounds "
              "-- the bands will jump whenever one of them is the live bank:",
              file=sys.stderr)
        for name, row in sorted(tables.items()):
            print(f"  {name:8s} {row}", file=sys.stderr)
        return 1
    if bad:
        return 1

    print(f"checkbgt: {len(tables)} banks agree on {ROWS} row backgrounds "
          f"({', '.join(sorted(tables))})")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
