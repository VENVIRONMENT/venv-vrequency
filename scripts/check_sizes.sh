#!/usr/bin/env bash
# check_sizes.sh — the two rules of docs/ARCHITECTURE.md that a machine can check.
#
#   1. No source file is longer than 200 lines. A file may be up to 400 lines when it has a line of
#      its own in the table "Files over 200 lines" of docs/ARCHITECTURE.md saying why.
#   2. The layers hold: core/ includes no Qt header and nothing outside core/; audio/ includes
#      nothing of the app and, of the engine, only core/dsp.h; a page (app/pages/) never
#      includes the window (app/shell/mainwindow.h).
#
# Run from anywhere: scripts/check_sizes.sh        Exit status 0 when both rules hold.
set -u
cd "$(dirname "$0")/.."
LIMIT=200
HARD=400
DOC=docs/ARCHITECTURE.md
bad=0
# only the table under "### Files over 200 lines" holds reasons
reasons() { awk '/^### Files over 200 lines/{f=1;next} /^#/{f=0} f' "$DOC" 2>/dev/null; }

files=$(git ls-files 'core/*' 'audio/*' 'app/*' 'tests/*' 'scripts/*' | grep -E '\.(cpp|h|py|sh)$')

for f in $files; do
    [ -f "$f" ] || continue
    n=$(wc -l < "$f")
    if [ "$n" -gt "$HARD" ]; then
        echo "too long: $f has $n lines (the limit with a reason is $HARD)"; bad=1
    elif [ "$n" -gt "$LIMIT" ]; then
        if ! reasons | grep -q -F "| \`$f\` |"; then
            echo "too long: $f has $n lines and no reason in $DOC"; bad=1
        fi
    fi
done

# a reason for a file that no longer needs one is stale
if [ -f "$DOC" ]; then
    for f in $(reasons | grep -o -E '^\| `[^`]+` \|' | sed -e 's/^| `//' -e 's/` |$//'); do
        if [ ! -f "$f" ]; then echo "stale reason in $DOC: $f does not exist"; bad=1
        elif [ "$(wc -l < "$f")" -le "$LIMIT" ]; then echo "stale reason in $DOC: $f is within $LIMIT lines now"; bad=1
        fi
    done
fi

inc() { grep -n -E '^[[:space:]]*#[[:space:]]*include' "$1"; }
# report the lines read from stdin as layer breaks in file $1 (prints nothing when there are none)
report() { local hits; hits=$(cat); [ -n "$hits" ] && echo "$hits" | sed "s|^|layer: $1:|"; }

for f in $(echo "$files" | grep -E '^core/'); do
    out=$(inc "$f" | grep -E '[<"]Q[A-Za-z]|"(app|audio|tests)/' | report "$f")
    [ -n "$out" ] && { echo "$out"; bad=1; }
done
for f in $(echo "$files" | grep -E '^audio/'); do
    out=$(inc "$f" | grep -E '"(app|tests)/|"core/' | grep -v '"core/dsp.h"' | report "$f")
    [ -n "$out" ] && { echo "$out"; bad=1; }
    out=$(inc "$f" | grep -E '[<"]Q[A-Za-z]' | report "$f")
    [ -n "$out" ] && { echo "$out"; bad=1; }
done
for f in $(echo "$files" | grep -E '^app/pages/'); do
    out=$(inc "$f" | grep -F '"app/shell/mainwindow.h"' | report "$f")
    [ -n "$out" ] && { echo "$out"; bad=1; }
done

if [ "$bad" = 0 ]; then echo "OK: $(echo "$files" | wc -l) files, none over $LIMIT lines without a reason; the layers hold"; fi
exit $bad
