#!/usr/bin/env python3
"""Validate docs/helpcards.json (stdlib only).

Checks structure, every length limit, allowed kinds, duplicate titles inside a page,
that every `related` id ("page.zone" or "concepts.id") exists, and, when docs/zones.txt
exists, that every listed zone has a card.

docs/zones.txt format (any of these per line, '#' comments allowed):
    zone
    page zone
    page.zone
"""
import json
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CARDS = os.path.join(ROOT, "docs", "helpcards.json")
ZONES = os.path.join(ROOT, "docs", "zones.txt")

KINDS = {"button", "toggle", "slider", "pad", "stepper", "readout", "tab", "key", "list", "area"}
HOW = {"Touch", "Keys", "Both"}
LIM = {"title": 28, "summary": 64, "does": 230, "recall": 110, "ask": 80}
USE_TEXT, TIP, INTRO = 90, 100, 160
REQUIRED = ["title", "kind", "summary", "does", "use", "keys", "recall", "tips", "related", "ask"]
ALLOWED = set(REQUIRED) | {"match"}

errors = []


def err(msg):
    errors.append(msg)


def check_card(where, c):
    if not isinstance(c, dict):
        err(f"{where}: card is not an object")
        return
    for k in REQUIRED:
        if k not in c:
            err(f"{where}: missing '{k}'")
    for k in c:
        if k not in ALLOWED:
            err(f"{where}: unknown field '{k}'")
    for k, n in LIM.items():
        v = c.get(k)
        if not isinstance(v, str) or not v.strip():
            err(f"{where}: '{k}' must be a non-empty string")
        elif len(v) > n:
            err(f"{where}: '{k}' is {len(v)} chars (max {n}): {v[:50]}...")
    if c.get("kind") not in KINDS:
        err(f"{where}: bad kind {c.get('kind')!r}")
    use = c.get("use")
    if not isinstance(use, list) or not (1 <= len(use) <= 4):
        err(f"{where}: 'use' must be a list of 1..4 items")
    else:
        for i, u in enumerate(use):
            if not isinstance(u, dict) or set(u) != {"how", "text"}:
                err(f"{where}: use[{i}] must have exactly how,text")
                continue
            if u["how"] not in HOW:
                err(f"{where}: use[{i}] bad how {u['how']!r}")
            if not isinstance(u["text"], str) or not u["text"].strip() or len(u["text"]) > USE_TEXT:
                err(f"{where}: use[{i}] text empty or over {USE_TEXT}: {str(u['text'])[:40]}")
    keys = c.get("keys")
    if not isinstance(keys, list) or not all(isinstance(k, str) and k for k in keys):
        err(f"{where}: 'keys' must be a list of non-empty strings")
    tips = c.get("tips")
    if not isinstance(tips, list) or len(tips) > 2:
        err(f"{where}: 'tips' must be a list of at most 2")
    else:
        for t in tips:
            if not isinstance(t, str) or not t.strip() or len(t) > TIP:
                err(f"{where}: tip empty or over {TIP}: {str(t)[:40]}")
    if not isinstance(c.get("related"), list) or not all(isinstance(r, str) for r in c.get("related", [])):
        err(f"{where}: 'related' must be a list of strings")
    if "match" in c:
        m = c["match"]
        if not isinstance(m, list) or len(m) < 2 or not all(isinstance(x, str) and x for x in m):
            err(f"{where}: 'match' must be a list of 2+ zone names")


def main():
    try:
        with open(CARDS, encoding="utf-8") as f:
            data = json.load(f)
    except Exception as e:  # noqa: BLE001
        print(f"cannot read {CARDS}: {e}")
        return 1
    if data.get("version") != 1:
        err("version must be 1")
    pages = data.get("pages")
    concepts = data.get("concepts")
    if not isinstance(pages, dict) or not pages:
        err("'pages' must be a non-empty object")
        pages = {}
    if not isinstance(concepts, dict):
        err("'concepts' must be an object")
        concepts = {}

    ids = set()          # every valid id for `related`
    covered = {}         # page -> set of zone names with a card
    for pname, p in pages.items():
        if not isinstance(p, dict):
            err(f"page {pname}: not an object")
            continue
        for k in ("title", "intro", "cards"):
            if k not in p:
                err(f"page {pname}: missing '{k}'")
        if not isinstance(p.get("title"), str) or not p.get("title", "").strip():
            err(f"page {pname}: empty title")
        intro = p.get("intro", "")
        if not isinstance(intro, str) or not intro.strip() or len(intro) > INTRO:
            err(f"page {pname}: intro empty or {len(intro) if isinstance(intro, str) else '?'} chars (max {INTRO})")
        titles = {}
        covered[pname] = set()
        for zone, c in (p.get("cards") or {}).items():
            check_card(f"{pname}.{zone}", c)
            if not isinstance(c, dict):
                continue
            names = [zone] + (c.get("match") or [])
            if "match" in c and zone not in c["match"]:
                err(f"{pname}.{zone}: 'match' should include the card's own zone name")
            for n in names:
                ids.add(f"{pname}.{n}")
                covered[pname].add(n)
            t = c.get("title")
            if t in titles:
                err(f"page {pname}: duplicate title {t!r} on {titles[t]} and {zone}")
            else:
                titles[t] = zone
    for cname, c in concepts.items():
        check_card(f"concepts.{cname}", c)
        ids.add(f"concepts.{cname}")
    ctitles = {}
    for cname, c in concepts.items():
        t = c.get("title") if isinstance(c, dict) else None
        if t in ctitles:
            err(f"concepts: duplicate title {t!r} on {ctitles[t]} and {cname}")
        ctitles[t] = cname

    for pname, p in pages.items():
        for zone, c in (p.get("cards") or {}).items():
            for r in (c.get("related") or []) if isinstance(c, dict) else []:
                if r not in ids:
                    err(f"{pname}.{zone}: related id {r!r} does not exist")
    for cname, c in concepts.items():
        for r in (c.get("related") or []) if isinstance(c, dict) else []:
            if r not in ids:
                err(f"concepts.{cname}: related id {r!r} does not exist")

    if os.path.exists(ZONES):
        with open(ZONES, encoding="utf-8") as f:
            for ln in f:
                ln = ln.split("#", 1)[0].strip()
                if not ln:
                    continue
                if "." in ln and " " not in ln:
                    page, zone = ln.split(".", 1)
                    pgs = [page]
                elif " " in ln:
                    page, zone = ln.split(None, 1)
                    pgs = [page]
                else:
                    zone, pgs = ln, list(covered)
                if not any(zone in covered.get(pg, ()) for pg in pgs):
                    err(f"zones.txt: no card for {ln!r}")

    ncards = sum(len(p.get("cards", {})) for p in pages.values() if isinstance(p, dict))
    if errors:
        for e in errors:
            print("ERROR:", e)
        print(f"FAILED: {len(errors)} problem(s); {len(pages)} pages, {ncards} cards, {len(concepts)} concepts")
        return 1
    print(f"OK: {len(pages)} pages, {ncards} cards, {len(concepts)} concepts"
          + ("" if os.path.exists(ZONES) else " (docs/zones.txt not present: zone coverage not checked)"))
    return 0


if __name__ == "__main__":
    sys.exit(main())
