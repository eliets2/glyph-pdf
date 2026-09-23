#!/usr/bin/env python3
"""GlyphPDF soak-48h verdict: mechanical log analysis.

Reads the soak logs and produces the tallies used by
docs/audit/SOAK-VERDICT-2026-09-20.md (first soak) and
docs/audit/RESOAK-VERDICT-2026-09-22.md (re-soak). Log-processing only;
no project code.

Usage:  python tools/soak_verdict_summary.py [SOAK_LOG [ATTEMPT1_LOG]]
        (first-soak mode; defaults D:\\soak-48h.log D:\\soak-48h-setup-attempt1.log)
        python tools/soak_verdict_summary.py --resoak [RESOAK_LOG]
        (re-soak mode; default D:\\resoak-48h.log; reads the D:\\resoak-48h.*
        marker files for window accounting)
"""
import os
import re
import sys
from collections import Counter, defaultdict
from datetime import datetime
from pathlib import Path

MAIN_DEFAULT = r"D:\soak-48h.log"
ATTEMPT1_DEFAULT = r"D:\soak-48h-setup-attempt1.log"
RESOAK_DEFAULT = r"D:\resoak-48h.log"

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
# "A crash occurred in <exe>. / While testing <func> / Exception code / Nearby symbol"
RE_CRASH = re.compile(r"^A crash occurred in (?P<exe>.+?)\.$")
RE_CRASHFN = re.compile(r"^While testing (?P<fn>\S+)$")
RE_CRASHCODE = re.compile(r"^Exception code\s+: (?P<code>\S+)$")
RE_CRASHSYM = re.compile(r"^Nearby symbol\s+: (?P<sym>\S+)$")
# re-soak specific markers
RE_RESTART = re.compile(
    r"^(?P<ts>\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2})?\s?=== RESTART DETECTED resuming at pass (?P<n>\d+) - "
    r"first start was (?P<fs>\S+), end-by remains (?P<eb>\S+), pass numbers continue ===")
RE_SOAKEND = re.compile(
    r"^(?P<ts>\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2})?\s?=== SOAK END after (?P<n>\d+) passes total, "
    r"failed passes this run: (?P<fails>\d+) - end-by was (?P<eb>\S+)")
RE_TOTALTIME = re.compile(r"^Total Test time \(real\) = (?P<s>[\d.]+) sec")
RE_TS = re.compile(r"^(\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2})")


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
        self.total_time = None     # ctest "Total Test time (real)" seconds


def parse(path):
    """Parse one soak log; returns (passes, order, appcycles)."""
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
            m = RE_TOTALTIME.match(raw.strip())
            if m:
                cur.total_time = float(m.group("s"))
                continue
            stripped = raw.strip()
            if re.search(r"/\d+ Test", raw) or stripped.startswith("Start "):
                cur.last_ctest_line = stripped[:120]
    return passes, order, appcycles


def parse_extras(path):
    """Re-soak extras: RESTART DETECTED markers, SOAK END line, crash blocks,
    all timestamped lines (for the downtime gap scan)."""
    restarts, soakend, crashes, all_ts = [], None, [], []
    cur_crash = None
    text = Path(path).read_text(encoding="utf-8", errors="replace")
    for raw in text.splitlines():
        m = RE_TS.match(raw)
        if m:
            all_ts.append(m.group(1))
        m = RE_RESTART.match(raw)
        if m:
            restarts.append({"ts": m.group("ts"), "n": int(m.group("n")),
                             "first_start": m.group("fs"), "end_by": m.group("eb")})
            continue
        m = RE_SOAKEND.match(raw)
        if m:
            soakend = {"ts": m.group("ts"), "passes": int(m.group("n")),
                       "fails": int(m.group("fails")), "end_by": m.group("eb")}
            continue
        m = RE_CRASH.match(raw)
        if m:
            cur_crash = {"exe": m.group("exe"), "fn": None, "code": None, "sym": None}
            crashes.append(cur_crash)
            continue
        if cur_crash is not None:
            m = RE_CRASHFN.match(raw)
            if m:
                cur_crash["fn"] = m.group("fn")
                continue
            m = RE_CRASHCODE.match(raw)
            if m:
                cur_crash["code"] = m.group("code")
                continue
            m = RE_CRASHSYM.match(raw)
            if m:
                cur_crash["sym"] = m.group("sym")
                cur_crash = None  # block ends
    return restarts, soakend, crashes, all_ts


def tally(passes):
    """test -> dict(count, passes, kinds, sigs)"""
    t = defaultdict(lambda: {"count": 0, "passes": [], "kinds": Counter(), "sigs": []})
    for n in sorted(passes):
        for name, kind in passes[n].failing:
            t[name]["count"] += 1
            t[name]["passes"].append(n)
            t[name]["kinds"][kind] += 1
    return t


def ranges(nums):
    """[1,2,3,7,9,10] -> '1-3, 7, 9-10' (compact pass lists)"""
    if not nums:
        return "(none)"
    out, s, p = [], nums[0], nums[0]
    for n in nums[1:]:
        if n == p + 1:
            p = n
            continue
        out.append(f"{s}-{p}" if p > s else f"{s}")
        s = p = n
    out.append(f"{s}-{p}" if p > s else f"{s}")
    return ", ".join(out)


def pct(a, b):
    return f"{100.0 * a / b:.1f}%" if b else "n/a"


# ----------------------------------------------------------------------------
# first-soak mode (SOAK-VERDICT-2026-09-20) — unchanged behavior
# ----------------------------------------------------------------------------

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
    if "--resoak" in sys.argv:
        return resoak_main()

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


# ----------------------------------------------------------------------------
# re-soak mode (RESOAK-VERDICT-2026-09-22)
# ----------------------------------------------------------------------------

# First-soak + baseline + RESOAK-2026-09-20 §2 pre-declared failure families.
# (family id, classification, tests, signature regexes)
FAMILIES = [
    ("TIMING-LANE-PREDECLARED",
     "load-era timing guard, pre-declared RESOAK-2026-09-20 §2 (marginal 1000 ms budget)",
     {"TestLaneScheduler"},
     [r"elapsed < 10 \* 100", r"Cross-page pipeline took \d+ms"]),
    ("TIMING-PERF-GUARD",
     "load-era timing guard (wall-clock assertion under machine load)",
     {"TestSep13LeadComparePerf", "TestWelcomeRoutes", "TestResourceLimits"},
     [r"took \d+ms, expected", r"elapsed.*returned FALSE"]),
    ("READ-ROUNDTRIP",
     "temp-file write/read-back roundtrip flake (verdict §4.1/§4.4 family: "
     "readExpiryDate(dest).isValid() + UI-enablement follow-ons)",
     {"TestReadOnlyGate", "TestCommandBinding"},
     [r"readExpiryDate\(.*\)\.isValid\(\)", r"menuSave->isEnabled", r"dialogSeen",
      r"isReadOnly\(\)' returned FALSE", r"ribbonButtonsMirror", r"menuItemsShare", r"sampledEnabledActions"]),
    ("FU2-SHARED-TEMPDIR",
     "shared %TEMP%/glyphpdf-candidates / output-visibility interference "
     "(verdict §4.2/§4.3/§4.6 family; RESOURCE_LOCK set partial at b17106a)",
     {"TestBatchMode", "TestEngineSave", "TestEncryptedPackageSafeWrite", "TestSanitization"},
     [r"leftoverCandidates", r"successCount\(\)", r"Compared values are not the same",
      r"sanitizeDocument"]),
    ("OCSP-TRANSIENT",
     "known 1.7% environment-sensitive trust-status transient (verdict §5)",
     {"TestSignatureRealCrypto"},
     [r"acceptable.*returned FALSE", r"trustStatus", r"OcspCertIdMismatch"]),
    ("CRASH-CLASS",
     "0xc0000005 in vendored PoDoFo path during testSanitizeGeneratesUniqueTrailerID "
     "(verdict §4.5 / follow-up §10.2)",
     {"TestSanitization"},
     [r"Exception: SegFault", r"AssertMutable", r"GetRawData"]),
    ("ASYNC-VISIBILITY-ONEOFF",
     "async dispatch result-visibility one-off under load (resultCount 0 vs 1)",
     {"TestOllamaProvider"},
     [r"resultCount\(\): 0", r"endpointPolicyHttps"]),
]

# tests a failing test can belong to without being "new"
KNOWN_TEST_NAMES = set().union(*[f[2] for f in FAMILIES])


def classify_test(name, kinds, sigs):
    """Mechanical classification of one failing-test tally entry.
    Returns (family_id, classification, matched_bool)."""
    # crash-kind events take precedence over any suite/family membership
    if any(str(k).startswith("Exception") for k in kinds):
        fam, cls, _tests, _pats = FAMILIES[5]  # CRASH-CLASS
        return fam, cls, True
    for fam, cls, tests, pats in FAMILIES:
        if name in tests:
            return fam, cls, True
    # unknown test name: try signature match against family patterns
    joined = " | ".join(sigs)
    for fam, cls, _tests, pats in FAMILIES:
        if any(re.search(p, joined) for p in pats):
            return fam, cls + " [by-signature]", True
    return "NEW-SIGNATURE", "NOT in first-soak families - new watch item", False


def resoak_main():
    log = RESOAK_DEFAULT
    for a in sys.argv[2:]:
        if not a.startswith("-"):
            log = a
    passes, order, appcycles = parse(log)
    restarts, soakend, crashes, all_ts = parse_extras(log)

    print("=" * 78)
    print(f"RE-SOAK VERDICT ANALYSIS: {log}")
    st = os.stat(log)
    print(f"size={st.st_size} bytes  mtime={datetime.fromtimestamp(st.st_mtime):%Y-%m-%d %H:%M:%S}")
    print(f"rotation .log.1 exists: {os.path.exists(log + '.1')}")

    # -- header verification -------------------------------------------------
    head = Path(log).read_text(encoding="utf-8", errors="replace")[:2000]
    cand = re.search(r"candidate = ([0-9a-f]{40})", head)
    exe = re.search(r"exe sha256 = ([0-9a-f]{64})", head)
    print("\n-- header --")
    print(f"  candidate: {cand.group(1) if cand else 'NOT FOUND'}")
    print(f"  exe sha256: {exe.group(1) if exe else 'NOT FOUND'}")
    disk_sha = None
    disk_exe = Path(r"C:\Users\User\Projects\pdf-keyC\build-soak\PdfWorkstation.exe")
    if disk_exe.exists():
        import hashlib
        disk_sha = hashlib.sha256(disk_exe.read_bytes()).hexdigest()
        print(f"  build-soak exe on disk NOW: {disk_sha}  match={disk_sha == (exe.group(1) if exe else None)}")

    # -- window accounting ----------------------------------------------------
    print("\n-- window accounting --")
    state = Path(r"D:\resoak-48h.state")
    endf = Path(r"D:\resoak-48h.end")
    donef = Path(r"D:\resoak-48h.done")
    if state.exists():
        print(f"  state: {state.read_text(encoding='utf-8', errors='replace').strip()!r}")
    if endf.exists():
        print(f"  end-by file: {endf.read_text(encoding='utf-8', errors='replace').strip()}")
    print(f"  done marker exists: {donef.exists()}"
          + (f" ({donef.read_text(encoding='utf-8', errors='replace').strip()})" if donef.exists() else ""))
    if soakend:
        print(f"  SOAK END line: after {soakend['passes']} passes, failed={soakend['fails']}, ts={soakend['ts']}, end-by was {soakend['end_by']}")
    else:
        print("  SOAK END line: ABSENT (window not completed!)")
    if all_ts:
        t0 = datetime.strptime(all_ts[0], "%Y-%m-%dT%H:%M:%S")
        t1 = datetime.strptime(all_ts[-1], "%Y-%m-%dT%H:%M:%S")
        print(f"  first timestamped line: {all_ts[0]}   last: {all_ts[-1]}  span={((t1 - t0).total_seconds() / 3600):.2f} h")
        gaps = []
        prev = t0
        for s in all_ts[1:]:
            t = datetime.strptime(s, "%Y-%m-%dT%H:%M:%S")
            if (t - prev).total_seconds() > 1200:  # healthy pass <= ~17 min
                gaps.append((prev, t, (t - prev).total_seconds() / 60))
            prev = t
        print(f"  timestamp gaps > 20 min (hidden downtime / unmarked restart): {len(gaps)}")
        for g in gaps:
            print(f"    {g[0]} -> {g[1]} ({g[2]:.0f} min)")

    # -- pass accounting ------------------------------------------------------
    results = [n for n in order if passes[n].verdict]
    ok = [n for n in results if passes[n].exit == 0]
    bad = [n for n in results if passes[n].exit != 0]
    verdicts = Counter(passes[n].verdict for n in results)
    exits = Counter(passes[n].exit for n in results)
    no_result = sorted(set(order) - set(results))
    dup_starts = [n for n, c in Counter(order).items() if c > 1]
    contiguous = order == sorted(set(order)) and (not order or order[-1] - order[0] + 1 == len(set(order)))
    print("\n-- pass accounting --")
    print(f"  START markers: {len(order)} (unique pass numbers: {len(set(order))}, out-of-order/dup: {len(order) != len(set(order)) or order != sorted(order)})")
    print(f"  numbering contiguous from {order[0]} to {order[-1]}: {contiguous}")
    print(f"  RESULT markers: {len(results)}  verdicts: {dict(verdicts)}")
    print(f"  RESULT-less passes (START without RESULT): {no_result}")
    print(f"  ctest exit histogram: {dict(sorted(exits.items()))}")
    print(f"  exit=0 passes: {len(ok)} ({pct(len(ok), len(results))} of {len(results)} with results)   exit!=0 passes: {len(bad)} ({pct(len(bad), len(results))})")
    exotic = {n: passes[n].exit for n in bad if passes[n].exit not in (8,)}
    print(f"  exits other than 8 on FAIL: {exotic if exotic else 'none'}")
    no_ts = [n for n in results if not passes[n].ts_on_result]
    print(f"  RESULT lines missing timestamp prefix: {no_ts if no_ts else 'none'}")
    fail_no_event = [n for n in bad if not passes[n].failing]
    print(f"  FAIL passes with 0 recorded failing tests: {fail_no_event if fail_no_event else 'none'}")
    ev_per_fail = Counter(len(passes[n].failing) for n in bad)
    print(f"  failing-test events per FAIL pass: {dict(sorted(ev_per_fail.items()))}"
          f"  (total events {sum(k * v for k, v in ev_per_fail.items())})")

    # -- restarts ---------------------------------------------------------------
    print("\n-- restart / resume accounting --")
    print(f"  RESTART DETECTED markers: {len(restarts)}")
    for r in restarts:
        nxt = next((n for n in order if n > r["n"] and passes[n].start_ts), None)
        print(f"    ts={r['ts']} resuming at pass {r['n']} (first start {r['first_start']}, end-by {r['end_by']})"
              f" -> next START = pass {nxt} (continues cleanly: {nxt == r['n'] + 1})")
    drill_only = len(restarts) == 1 and restarts and restarts[0]["ts"] and restarts[0]["ts"] < "2026-09-21"
    print(f"  real OS reboots in-window (markers beyond the day-one drill + gap scan): "
          f"{max(0, len(restarts) - 1)} markers; gap-scan hidden downtime: see above")

    # -- per-test tally + classification ---------------------------------------]
    print("\n-- per-test tally across ALL failed passes (with classification) --")
    t = tally(passes)
    n_results = len(results)
    n_bad = len(bad)
    rows = []
    for name, d in sorted(t.items(), key=lambda kv: -kv[1]["count"]):
        sigs = sorted({re.sub(r"^FAIL!\s*:\s*", "", s).split("(")[0] for s in
                       [s for n in sorted(d['passes']) for s in passes[n].failsigs
                        if s.split(":")[1].strip().startswith(name)]})
        fam, cls, matched = classify_test(name, dict(d["kinds"]), sigs)
        rows.append((name, d, fam, cls, matched, sigs))
        rate_all = pct(d["count"], n_results)
        rate_fail = pct(d["count"], n_bad)
        flag = ""
        if d["count"] * 100 >= 30 * n_results:
            flag = "  *** >=30% OF ALL PASSES - candidate-attributable finding per protocol ***"
        print(f"  {name}: {d['count']}x = {rate_all} of {n_results} result passes ({rate_fail} of {n_bad} FAIL passes)"
              f"  kinds={dict(d['kinds'])}")
        print(f"      family={fam}  classification={cls}{flag}")
        print(f"      passes: {ranges(d['passes'])}")
        for s in sigs[:4]:
            print(f"      sig: {s}")
        if len(sigs) > 4:
            print(f"      ... {len(sigs) - 4} more distinct signatures")
    new_tests = [r[0] for r in rows if not r[4]]
    print(f"  NEW vs first-soak families (test names): {new_tests if new_tests else 'none'}")

    # -- crash-class ------------------------------------------------------------
    print("\n-- crash-class events (A crash occurred ...) --")
    print(f"  total: {len(crashes)}")
    for i, c in enumerate(crashes, 1):
        print(f"   #{i}: exe={os.path.basename(c['exe'])} while-testing={c['fn']} code={c['code']} symbol={c['sym']}")

    # -- app cycles ---------------------------------------------------------------
    print("\n-- app cycles --")
    killed = [n for n, a in sorted(appcycles.items()) if a["outcome"] and a["outcome"].startswith("KILLED")]
    early = [(n, a) for n, a in sorted(appcycles.items()) if a["outcome"] and a["outcome"].startswith("EXITED-EARLY")]
    noout = [n for n, a in sorted(appcycles.items()) if not a["outcome"]]
    print(f"  launches recorded: {len(appcycles)}")
    print(f"  KILLED-AFTER-60S (normal): {len(killed)}")
    print(f"  EXITED-EARLY (crash suspects): {len(early)}"
          + (" " + ", ".join(f"pass {n} exit={a['outcome'].split('=', 1)[1]}" for n, a in early) if early else ""))
    print(f"  outcome never recorded: {noout if noout else 'none'}")

    # -- load-context: durations ---------------------------------------------------
    print("\n-- load context: per-pass ctest duration (result_ts - start_ts) --")
    durs = []
    for n in results:
        p = passes[n]
        if p.start_ts and p.result_ts:
            dt = (datetime.strptime(p.result_ts, "%Y-%m-%dT%H:%M:%S")
                  - datetime.strptime(p.start_ts, "%Y-%m-%dT%H:%M:%S")).total_seconds()
            durs.append((n, dt, p.verdict))
    if durs:
        ds = sorted(d for _, d, _ in durs)
        med = ds[len(ds) // 2]
        p90 = ds[int(len(ds) * 0.9)]
        print(f"  n={len(durs)}  min={ds[0]:.0f}s  median={med:.0f}s  p90={p90:.0f}s  max={ds[-1]:.0f}s")
        slow = sorted(durs, key=lambda x: -x[1])[:15]
        print(f"  15 slowest passes: " + ", ".join(f"p{n}={d:.0f}s({v})" for n, d, v in slow))
        q = ds[len(ds) // 4]
        fast = [n for n, d, v in durs if d <= q]
        slowq = [n for n, d, v in durs if d >= ds[int(len(ds) * 0.75)]]
        fq = [n for n in fast if passes[n].verdict == "FAIL"]
        sq = [n for n in slowq if passes[n].verdict == "FAIL"]
        print(f"  FAIL share in fastest quartile (<= {q:.0f}s): {len(fq)}/{len(fast)} = {pct(len(fq), len(fast))}")
        print(f"  FAIL share in slowest quartile (>= {ds[int(len(ds) * 0.75)]:.0f}s): {len(sq)}/{len(slowq)} = {pct(len(sq), len(slowq))}")
    lane_ms = []
    for n in sorted(passes):
        for s in passes[n].failsigs:
            m = re.search(r"Cross-page pipeline took (\d+)ms", s)
            if m:
                lane_ms.append((int(m.group(1)), n))
    if lane_ms:
        vals = sorted(v for v, _ in lane_ms)
        print(f"  TestLaneScheduler 'Cross-page pipeline took Nms' over budget: n={len(vals)} "
              f"min={vals[0]}ms median={vals[len(vals)//2]}ms max={vals[-1]}ms (budget 1000ms)")

    # -- known one-off reconciliation ------------------------------------------------
    print("\n-- first-soak one-off watch items in re-soak --")
    for k in ["TestWelcomeRoutes", "TestResourceLimits", "TestEngineSave",
              "TestEncryptedPackageSafeWrite", "TestSanitization", "TestSignatureRealCrypto"]:
        d = t.get(k)
        print(f"  {k}: {d['count'] if d else 0}x {('passes ' + ranges(d['passes'])) if d else ''}")

    # -- time-bucketed failure rates (load-era vs persistent) ----------------------
    print("\n-- time buckets (24 h from first START): FAIL rate + top test rates --")
    tss = {n: passes[n].start_ts for n in order if passes[n].start_ts}
    if tss:
        t0 = datetime.strptime(min(tss.values()), "%Y-%m-%dT%H:%M:%S")
        buckets = defaultdict(list)
        for n in results:
            if passes[n].start_ts:
                dt = (datetime.strptime(passes[n].start_ts, "%Y-%m-%dT%H:%M:%S") - t0).total_seconds()
                buckets[int(dt // 86400)].append(n)
        for b in sorted(buckets):
            ns = buckets[b]
            nf = [n for n in ns if passes[n].verdict == "FAIL"]
            line = f"  h {b*24:3d}-{b*24+24:3d}: passes {ns[0]}-{ns[-1]} n={len(ns)} FAIL={len(nf)} ({pct(len(nf), len(ns))})"
            for name, d in sorted(t.items(), key=lambda kv: -kv[1]["count"])[:3]:
                cnt = sum(1 for n in ns if name in dict(passes[n].failing or []).keys() or
                          any(f[0] == name for f in passes[n].failing))
                line += f"  {name}={cnt} ({pct(cnt, len(ns))})"
            print(line)
    print()
    return 0


if __name__ == "__main__":
    sys.exit(main())
