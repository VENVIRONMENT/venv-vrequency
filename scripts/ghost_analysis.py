#!/usr/bin/env python3
"""Which three-key chords could confuse the Q20's key matrix?

The Q20 keyboard is an 8x7 switch matrix (positions read from the Vial layout
in ~/HackberryPi_CM5_Q20.vil). If the membrane has no diodes, three pressed
keys that sit on three corners of a matrix rectangle make the fourth corner
look pressed too ("ghosting"), or the firmware hides one of them ("blocking").
This lists the note chords (one key per string) that form such a rectangle and
what sits on the fourth corner. Whether the hardware really ghosts has to be
checked with Tendril's KEY TEST screen.
"""
M = {  # key -> (matrix row, column) on layer 0
    'W': (0, 1), 'G': (0, 2), 'S': (0, 3), 'L': (0, 4), 'H': (0, 5),
    'Q': (1, 1), 'R': (1, 2), 'E': (1, 3), 'O': (1, 4), 'U': (1, 5),
    'ALT': (2, 1), 'F': (2, 2), 'CTRL': (2, 3), 'K': (2, 4), 'J': (2, 5),
    'SPACE': (3, 1), 'C': (3, 2), 'Z': (3, 3), 'M': (3, 4), 'N': (3, 5),
    'NUM': (4, 1), 'T': (4, 2), 'D': (4, 3), 'I': (4, 4), 'Y': (4, 5),
    'SHIFT': (5, 1), 'V': (5, 2), 'X': (5, 3), '$': (5, 4), 'B': (5, 5),
    'A': (6, 1), 'SYM': (6, 2), 'P': (6, 3), 'BKSP': (6, 4), 'ENTER': (6, 5),
}
POS = {v: k for k, v in M.items()}
ROWS = ["ZXCVBNM", "ASDFGHJKL", "QWERTYUIOP"]

def fourth(a, b, c):
    """Return the ghost position if a, b, c are three corners of a rectangle."""
    for p, q, r in ((a, b, c), (b, a, c), (c, a, b)):
        # p is the corner sharing a row with one key and a column with the other
        for x, y in ((q, r), (r, q)):
            if p[0] == x[0] and p[1] == y[1] and p != x and p != y:
                return (y[0], x[1])
    return None

risky, total = [], 0
for lo in ROWS[0]:
    for mid in ROWS[1]:
        for hi in ROWS[2]:
            total += 1
            g = fourth(M[lo], M[mid], M[hi])
            if g and g in POS and POS[g] not in (lo, mid, hi):
                risky.append((lo, mid, hi, POS[g]))
print(f"{len(risky)} of {total} one-key-per-string chords touch a matrix rectangle:")
for lo, mid, hi, g in risky:
    print(f"  {lo}+{mid}+{hi}  -> fourth corner: {g}")
pairs = []
for r in ROWS:
    for i, a in enumerate(r):
        for b in r[i + 1:]:
            for mod in ('SHIFT', 'SPACE', 'CTRL', 'ALT'):
                g = fourth(M[a], M[b], M[mod])
                if g and g in POS:
                    pairs.append((mod, a, b, POS[g]))
print(f"\n{len(pairs)} slide pairs (two keys on one string) + a modifier touch a rectangle:")
for mod, a, b, g in pairs:
    print(f"  {mod}+{a}+{b}  -> fourth corner: {g}")
