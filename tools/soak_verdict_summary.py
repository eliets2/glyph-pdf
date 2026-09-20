#!/usr/bin/env python3
"""GlyphPDF soak-48h verdict: mechanical log analysis.

Reads the soak logs and produces the tallies used by
docs/audit/SOAK-VERDICT-2026-09-20.md. Log-processing only; no project code.

Usage:  python tools/soak_verdict_summary.py [SOAK_LOG [ATTEMPT1_LOG]]
        (defaults: D:\\soak-48h.log  D:\\soak-48h-setup-attempt1.log)
"""
import os
import re
import sys
from collections import Counter, defaultdict
from datetime import datetime
from pathlib import Path

MAIN_DEFAULT = r"D:\soak-48h.log"
ATTEMPT1_DEFAULT = r"D:\soak-48h-setup-attempt1.log"

# === SOAK PASS n START ===            (optional "YYYY-MM-DDTHH:MM:SS " prefix)
RE_START = re.compile(r"^(?P<ts>\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2})?\s?=== SOAK PASS (?P<n>\d+) START ===")
# RESULT: both "FAIL - ctest exit=8 -" (v2 bat) and "FAIL (ctest exit=8)" (attempt-1 bat)
RE_RESULT = re.compile(
    r"^(?P<ts>\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2})?\s?=== SOAK PASS (?P<n>\d+) RESULT: (?P<verdict>PASS|FAIL)[^\d]*(?P<exit>\d+)")
# ctest per-test result lines; counted ONLY before the RESULT line of the pass
# (the bat re-echoes failing lines after RESULT for its "failing tests:" block)
RE_TESTFAIL = re.compile(
    r"^\s*\d+/\d+ Test\s+#\d+:\s+(?P<name>\S+?)\s*\.+\s*\*\*\*(?P<kind>Failed|Timeout|Exception: \S+)")
# --- APP CYCLE PASS n: PdfWorkstation.exe [OUTCOME] - legend ---
RE_APPCYCLE = re.compile(
    r"^(?P<ts>\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2})?\s?--- APP CYCLE PASS (?P<n>\d+): PdfWorkstation\.exe\s*(?P<outcome>KILLED-AFTER-60S;taskkill-exitcode=\d+|EXITED-EARLY;exitcode=\S+)?\s*-")
# QtTest failure signatures (captured output of failed tests)
RE_FAILSIG = re.compile(r"^(?P<kind>FAIL!|QFATAL)\s*:\s+(?P<rest>.+)$")


class PassRec:
    def __init__(self, n, start_ts):
        self.n = n
        self.start_ts = start_ts
        self.result_ts = None
        self.verdict = None
        self.exit = None
        self.ts_on_result = False
        self.failing = []          # (name, kind) in ctest-output order, pre-RESULT
        self.failsigs = []         # FAIL!/QFATAL lines, pre-RESULT
        self.last_ctest_line = ""  # last ctest progress line before RESULT


def parse(path):
    """Parse one soak log; returns (passes, appcycles, orphans)."""
    passes = {}          # n -> PassRec
    order = []
    appcycles = {}       # n -> dict(ts, outcome, ts_present)
    cur = None
    in_result = False
    text = Path(path).read_text(encoding="utf-8", errors="replace")
    for raw in text.splitlines():
        m = RE_START.match(raw)
        if m:
            cur = PassRec(int(m.group("n")), m.group("ts"))
            passes[cur.n] = cur
            order.append(cur.n)
            in_result = False
            continue
        m = RE_RESULT.match(raw)
        if m:
            n = int(m.group("n"))
            rec = passes.setdefault(n, PassRec(n, None))
            rec.result_ts = m.group("ts")
            rec.ts_on_result = bool(m.group("ts"))
            rec.verdict = m.group("verdict")
            rec.exit = int(m.group("exit"))
            in_result = True
            continue
        m = RE_APPCYCLE.match(raw)
        if m:
            appcycles[int(m.group("n"))] = {
                "ts": m.group("ts"),
                "ts_present": bool(m.group("ts")),
                "outcome": m.group("outcome"),  # None = loop died before outcome
            }
            in_result = True  # everything after this in the pass is post-ctest
            continue
        if cur is not None and not in_result:
            m = RE_TESTFAIL.match(raw)
            if m:
                cur.failing.append((m.group("name"), m.group("kind")))
                continue
            m = RE_FAILSIG.match(raw)
            if m:
                cur.failsigs.append(raw.rstrip()[:200])
                continue
            stripped = raw.strip()
            if "/153 Test" in raw or stripped.startswith("Start "):
                cur.last_ctest_line = stripped[:120]
    return passes, order, appcycles


def tally(passes):
    """test -> dict(count, passes, kinds, sigs)"""
    t = defaultdict(lambda: {"count": 0, "passes": [], "kinds": Counter(), "sigs": []})
    for n in sorted(passes):
        for name, kind in passes[n].failing:
            t[name]["count"] += 1
            t[name]["passes"].append(n)
            t[name]["kinds"][kind] += 1
    return t


def report(title, path, passes, order, appcycles, excluded_note=""):
    print("=" * 78)
    print(f"{title}: {path}")
    if excluded_note:
        print(f"NOTE: {excluded_note}")
    st = os.stat(path)
    print(f"size={st.st_size} bytes  mtime={datetime.fromtimestamp(st.st_mtime):%Y-%m-%d %H:%M:%S}")
    starts = [n for n in order if passes[n].start_ts or True]
    results = [n for n in order if passes[n].verdict]
    print(f"pass STARTs: {len(starts)}   pass RESULTs: {len(results)}")
    first = next((p.start_ts for n in order if (p := passes[n]).start_ts), None)
    last_start = next((p.start_ts for n in reversed(order) if (p := passes[n]).start_ts), None)
    print(f"first START ts: {first}   last START ts: {last_start}")
    verdicts = Counter(p.verdict for n in order if (p := passes[n]).verdict)
    exits = Counter(p.exit for n in order if (p := passes[n]).verdict)
    print(f"verdicts: {dict(verdicts)}")
    print(f"exit codes: {dict(sorted(exits.items()))}")
    # passes with exit 0 vs nonzero
    ok = [n for n in order if passes[n].verdict and passes[n].exit == 0]
    bad = [n for n in order if passes[n].verdict and passes[n].exit != 0]
    print(f"ctest exit=0 passes: {len(ok)}   ctest exit!=0 passes: {len(bad)}")
    no_ts = [n for n in order if passes[n].verdict and not passes[n].ts_on_result]
    print(f"RESULT lines MISSING timestamp prefix: {no_ts}")

    print("\n-- failed passes (pass: exit -> tests) --")
    for n in bad:
        p = passes[n]
        tests = ", ".join(f"{name}[{kind}]" for name, kind in p.failing) or "(none recorded - killed mid-run)"
        print(f"  pass {n}: exit={p.exit} -> {tests}")

    print("\n-- per-test tally across ALL failed passes --")
    t = tally(passes)
    for name, d in sorted(t.items(), key=lambda kv: -kv[1]["count"]):
        print(f"  {name}: {d['count']}x in passes {d['passes']}  kinds={dict(d['kinds'])}")
    if not t:
        print("  (none)")

    print("\n-- app cycles --")
    killed = [n for n, a in sorted(appcycles.items()) if a["outcome"] and a["outcome"].startswith("KILLED")]
    early = [(n, a) for n, a in sorted(appcycles.items()) if a["outcome"] and a["outcome"].startswith("EXITED-EARLY")]
    noout = [n for n, a in sorted(appcycles.items()) if not a["outcome"]]
    print(f"  launches recorded: {len(appcycles)}")
    print(f"  KILLED-AFTER-60S (normal): {len(killed)}")
    print(f"  EXITED-EARLY (crash suspects): {len(early)}" +
          (" " + ", ".join(f"pass {n} exit={a['outcome'].split('=', 1)[1]}" for n, a in early) if early else ""))
    print(f"  outcome never recorded (loop died mid-cycle): {noout}")

    print("\n-- QtTest FAIL!/QFATAL signatures by test (deduped, first 3 each) --")
    seen = {}
    for n in sorted(passes):
        for s in passes[n].failsigs:
            key = re.sub(r"^\s*\S+\s*:\s+", "", s)[:120]
            test = key.split("::")[0] if "::" in key else key
            seen.setdefault(test, [])
            if key not in [k for k, _ in seen[test]]:
                seen[test].append((key, n))
    for test, entries in sorted(seen.items()):
        print(f"  {test}:")
        for key, n in entries[:3]:
            print(f"    pass {n}: {key}")
    print()


def main():
    main_log = sys.argv[1] if len(sys.argv) > 1 else MAIN_DEFAULT
    attempt1 = sys.argv[2] if len(sys.argv) > 2 else ATTEMPT1_DEFAULT

    passes, order, appcycles = parse(main_log)
    report("MAIN SOAK LOG (counts toward verdict)", main_log, passes, order, appcycles)

    # pass-58 detail
    p58 = passes.get(58)
    if p58:
        print("-- PASS 58 detail (the exit=1073807364 event) --")
        print(f"  start ts: {p58.start_ts}  result ts: {p58.result_ts} (timestamped={p58.ts_on_result})")
        print(f"  verdict={p58.verdict} exit={p58.exit} failing_tests_recorded={p58.failing or '(empty)'}")
        print(f"  last ctest line before kill: {p58.last_ctest_line}")
        a58 = appcycles.get(58)
        if a58:
            print(f"  app-cycle line: ts={a58['ts']} (timestamped={a58['ts_present']}) outcome={a58['outcome']!r}")
        print(f"  hex exit: {p58.exit:#010x}  (0x40010004 = STATUS_CONTROL_C_EXIT: console close/Ctrl+C broadcast)")
        print()

    # one-off reconciliation
    known = ["TestBatchMode", "TestWelcomeRoutes", "TestReadOnlyGate", "TestSignatureRealCrypto"]
    print("-- known one-off watch items in MAIN log --")
    t = tally(passes)
    for k in known:
        d = t.get(k)
        print(f"  {k}: {d['count'] if d else 0}x in passes {d['passes'] if d else []}")
    print()

    if os.path.exists(attempt1):
        p2, o2, a2 = parse(attempt1)
        report("SETUP ATTEMPT-1 LOG (EXCLUDED from verdict tallies - buggy harness)",
               attempt1, p2, o2, a2,
               "script-harness bug (PATH hid powershell + unescaped ')' broke if/else); "
               "passes here must NOT be counted")


if __name__ == "__main__":
    main()
