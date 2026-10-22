#!/usr/bin/env python3
"""Check docs/triage/upstream-issues.md against the upstream issue snapshot.

Usage: check-triage.py <triage.md> <issues.json>

Exits 0 and prints "issues=<N>" only if every issue number of the snapshot has
exactly one row, every row has a Fork status in one of the allowed forms, each
`Count:` line matches its table, every test ID named by `fixed in` and
`not reproduced` exists as a `void T_...(` function under tests/, and every
README#anchor in a status cell exists in README.md. Errors go to stderr, one
per line, with exit code 1. Standard library only; paths of tests/ and
README.md come from the location of this script, not the working directory.
"""

import json
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent

PR_SECTION = "Pull requests"

TID = r"T[-_][A-Z0-9]+(?:[-_][A-Z0-9]+)*"
UNIT = r"(?:P\d+|\d+\.\d+)"
NOTE = r"(?:;\s*\S.*)?"
RE_FIXED = re.compile(r"^fixed in " + UNIT + r" \(([^)]+)\)" + NOTE + r"$")
RE_NOTREP = re.compile(r"^not reproduced \(([^)]+)\)" + NOTE + r"$")
RE_ANSWERED = re.compile(r"^answered \(README#faq-[a-z0-9-]+\)" + NOTE + r"$")
RE_PLANNED = re.compile(r"^planned in " + UNIT + NOTE + r"$")
RE_DEFERRED = re.compile(r"^deferred: \S.*$")
RE_OBSOLETE = re.compile(r"^obsolete" + NOTE + r"$")
RE_NOACTION = re.compile(r"^no action \(announcement\)" + NOTE + r"$")
RE_TIDANY = re.compile(r"\b" + TID + r"\b")
RE_TIDLIST = re.compile(r"^" + TID + r"(?:\s*,\s*" + TID + r")*$")
RE_ANCHOR = re.compile(r"README#([A-Za-z0-9_-]+)")
RE_ROW = re.compile(r"^\|\s*#(\d+)\s*\|")
RE_COUNT = re.compile(r"^Count:\s*(\d+)\s*$")
RE_TESTFN = re.compile(r"\bvoid\s+(T_[A-Z0-9_]+)\s*\(")
RE_HTMLANCHOR = re.compile(r"<a\s+(?:id|name)=\"([^\"]+)\"")
RE_HEADING = re.compile(r"^(#{1,6})\s+(.+?)\s*#*\s*$")
CELLS = 7
NOACTION_ISSUE = 253


def usage():
    sys.stderr.write("usage: check-triage.py <triage.md> <issues.json>\n")
    return 2


def test_functions():
    found = set()
    tests = ROOT / "tests"
    for path in sorted(tests.rglob("*")):
        if path.is_file() and path.suffix in (".cpp", ".h"):
            text = path.read_text(encoding="utf-8", errors="replace")
            found.update(RE_TESTFN.findall(text))
    return found


def split_cells(line):
    line = line.strip()
    if line.startswith("|"):
        line = line[1:]
    if line.endswith("|"):
        line = line[:-1]
    return [c.strip() for c in line.split("|")]


def slug(text):
    text = re.sub(r"[^a-z0-9 _-]", "", text.lower())
    return text.strip().replace(" ", "-")


def readme_anchors():
    anchors = set()
    path = ROOT / "README.md"
    lines = path.read_text(encoding="utf-8", errors="replace").split("\n")
    for i, line in enumerate(lines):
        anchors.update(RE_HTMLANCHOR.findall(line))
        heading = RE_HEADING.match(line)
        if heading:
            anchors.add(slug(heading.group(2)))
        elif i > 0 and lines[i - 1].strip() and re.match(r"^(=+|-+)\s*$", line):
            anchors.add(slug(lines[i - 1]))
    return anchors


def load_numbers(path, errors):
    data = json.loads(pathlib.Path(path).read_text(encoding="utf-8"))
    if not isinstance(data, list):
        errors.append("json: the snapshot is not a list")
        return set(), 0
    numbers = set()
    for obj in data:
        number = int(obj["number"])
        if number in numbers:
            errors.append("json: duplicate issue number #%d" % number)
        numbers.add(number)
    return numbers, len(data)


def parse_triage(path, errors):
    """Return rows as (section, number, status) and the Count lines."""
    rows = []
    counts = {}
    section = None
    fork_index = None
    text = pathlib.Path(path).read_text(encoding="utf-8")
    for line in text.split("\n"):
        heading = re.match(r"^## (.+)$", line)
        if heading:
            section = heading.group(1).strip()
            fork_index = None
            continue
        if line.startswith("Count:"):
            count = RE_COUNT.match(line)
            if count and section is not None:
                counts[section] = int(count.group(1))
            else:
                errors.append("section %s: bad Count: line: %s" % (section, line))
            continue
        if re.match(r"^\|\s*#\s*\|", line):
            header = split_cells(line)
            fork_index = header.index("Fork status") if "Fork status" in header else None
            continue
        row = RE_ROW.match(line)
        if not row:
            continue
        number = int(row.group(1))
        cells = split_cells(line)
        if len(cells) != CELLS or fork_index is None or fork_index >= len(cells):
            errors.append("#%d: expected %d cells" % (number, CELLS))
            continue
        rows.append((section, number, cells[fork_index]))
    return rows, counts


def check_status(number, status, tests, anchors, errors):
    forms = (RE_FIXED, RE_NOTREP, RE_ANSWERED, RE_PLANNED, RE_DEFERRED,
             RE_OBSOLETE, RE_NOACTION)
    if not any(form.match(status) for form in forms):
        errors.append("#%d: bad Fork status: %s" % (number, status))
        return
    if RE_NOACTION.match(status) and number != NOACTION_ISSUE:
        errors.append("#%d: no action (announcement) is only for #%d"
                      % (number, NOACTION_ISSUE))
    for form in (RE_FIXED, RE_NOTREP):
        match = form.match(status)
        if not match:
            continue
        content = match.group(1)
        if not RE_TIDLIST.match(content):
            errors.append("#%d: bad test ID list: %s" % (number, content))
            continue
    for tid in RE_TIDANY.findall(status):
        name = tid.replace("-", "_")
        if name not in tests:
            errors.append("#%d: unknown test ID %s" % (number, name))
    for anchor in RE_ANCHOR.findall(status):
        if anchor not in anchors:
            errors.append("#%d: README anchor %s does not exist" % (number, anchor))


def main(argv):
    if len(argv) != 3:
        return usage()
    errors = []
    numbers, total = load_numbers(argv[2], errors)
    rows, counts = parse_triage(argv[1], errors)
    tests = test_functions()
    anchors = readme_anchors()

    seen = {}
    seen_pr = {}
    per_section = {}
    for section, number, status in rows:
        per_section[section] = per_section.get(section, 0) + 1
        check_status(number, status, tests, anchors, errors)
        if section == PR_SECTION:
            seen_pr[number] = seen_pr.get(number, 0) + 1
            continue
        seen[number] = seen.get(number, 0) + 1

    for number in sorted(seen):
        if seen[number] > 1:
            errors.append("#%d: %d rows, expected exactly one" % (number, seen[number]))
        if number not in numbers:
            errors.append("#%d: not in the issue snapshot" % number)
    for number in sorted(numbers - set(seen)):
        errors.append("#%d: no row" % number)
    for number in sorted(seen_pr):
        if seen_pr[number] > 1:
            errors.append("#%d: %d pull request rows, expected exactly one"
                          % (number, seen_pr[number]))
    for section in per_section:
        if section not in counts:
            errors.append("section %s: no Count: line" % section)
    for section, count in counts.items():
        if per_section.get(section, 0) != count:
            errors.append("section %s: Count: %d but %d rows"
                          % (section, count, per_section.get(section, 0)))

    if errors:
        for message in errors:
            sys.stderr.write("triage: %s\n" % message)
        return 1
    sys.stdout.write("issues=%d\n" % total)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
