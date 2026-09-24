#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""RFC-4180 well-formedness checker for docs/audit/FEATURE-COMMAND-MATRIX-2026-09-09.csv.

Pins (RESIDUAL-PLANS-2026-09-21 Plan 13):
  * every row parses to exactly the header's field count (an unquoted comma
    inside a cell silently shifts every later field);
  * (the earlier idea of pinning stable_command_id uniqueness does not hold:
    the matrix deliberately repeats a command's canonical id across surfaces
    AND across variant rows; the certify/certify collision is repaired by an
    id split recorded in the NOTES instead)

Exit code 0 = clean; 1 = violations printed. Run from anywhere:
    python scripts/check-feature-matrix-csv.py [path-to-csv]
"""
import csv
import sys
from pathlib import Path

DEFAULT_CSV = Path(__file__).resolve().parent.parent / "docs" / "audit" / "FEATURE-COMMAND-MATRIX-2026-09-09.csv"


def main() -> int:
    csv_path = Path(sys.argv[1]) if len(sys.argv) > 1 else DEFAULT_CSV
    with open(csv_path, newline="", encoding="utf-8-sig") as f:
        rows = list(csv.reader(f))
    if not rows:
        print(f"FAIL: {csv_path} is empty")
        return 1
    header = rows[0]
    width = len(header)
    print(f"header: {width} fields")

    failures: list[str] = []
    for lineno, row in enumerate(rows[1:], start=2):
        if not row or all(not cell.strip() for cell in row):
            continue  # blank line
        if len(row) != width:
            failures.append(
                f"line {lineno}: {len(row)} fields (expected {width})"
                f" - first cell: {row[0][:40]!r}"
            )

    for failure in failures:
        print(f"FAIL: {failure}")
    if failures:
        print(f"{len(failures)} violation(s) in {csv_path}")
        return 1
    print(f"OK: every data row parses to {width} fields")
    return 0


if __name__ == "__main__":
    sys.exit(main())
