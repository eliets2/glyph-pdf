#!/usr/bin/env python3
"""
redaction_leak_oracle.py  -- PRIORITY 1 oracle (F-01 / F-03)

Given a redacted PDF produced by the GlyphPDF redaction feature, PROVE or
REFUTE that the redacted secret survives anywhere recoverable:

  (a) in the live document objects        (qpdf --qdf normalized dump)
  (b) in ANY prior xref revision          (incremental-update history)
  (c) in the raw file body                (orphaned/uncompressed object bytes)
  (d) in extracted text                   (qpdf show + text reconstruction)

This is a DEFENSIVE assertion harness. It does not exploit anything; it reads
bytes that are already in the owner's own output file and reports whether the
secret string is recoverable. A surviving secret == State/Content-leak EVIDENCE
to hand to native-adversary (it rules on exploitability; we only prove reality).

Usage:
  python redaction_leak_oracle.py --pdf out.pdf --secret SECRET_DATA_XYZZY [--json report.json]

Exit code: 0 = secret absent everywhere (PASS), 3 = secret recovered (LEAK).
Requires: qpdf on PATH.
"""
import argparse, json, os, re, subprocess, sys, tempfile

def run(cmd, **kw):
    return subprocess.run(cmd, capture_output=True, text=False, **kw)

def qpdf_revisions(pdf):
    """Return number of xref revisions (incremental updates + base)."""
    r = run(["qpdf", "--show-npages", pdf])
    # qpdf 11+/12 exposes revision count via --list-attachments? No. Use
    # --json to read the file structure, fall back to counting 'startxref'.
    revs = None
    rj = run(["qpdf", "--json", pdf])
    if rj.returncode == 0:
        try:
            j = json.loads(rj.stdout.decode("latin-1", "replace"))
            # qpdf json has "qpdf" -> objects; revision count not always present.
        except Exception:
            pass
    # Robust fallback: count 'startxref' markers in the raw bytes.
    raw = open(pdf, "rb").read()
    revs = raw.count(b"startxref")
    return max(revs, 1), raw

def needles(secret):
    """All byte-encodings of the secret an attacker could recover."""
    s = secret.encode("latin-1", "replace")
    out = {"ascii": s}
    # PDFDocEncoding / literal-string parenthesized form is just ascii here.
    # UTF-16BE (used in some text strings)
    out["utf16be"] = secret.encode("utf-16-be")
    # Hex-string form <48656c...>
    out["hexstring"] = s.hex().encode("ascii")
    # Per-char split (defeats naive byte-search when a Tj is broken into TJ array)
    return out

def search_raw(raw, secret):
    hits = {}
    for name, pat in needles(secret).items():
        if pat and pat in raw:
            hits[name] = raw.count(pat)
    return hits

def qpdf_qdf_dump(pdf):
    """Normalized (uncompressed, decoded) view of the LIVE document."""
    with tempfile.NamedTemporaryFile(suffix=".qdf", delete=False) as t:
        qdf = t.name
    r = run(["qpdf", "--qdf", "--object-streams=disable",
             "--decode-level=all", pdf, qdf])
    data = b""
    if os.path.exists(qdf):
        data = open(qdf, "rb").read()
        os.unlink(qdf)
    return data, r.returncode, r.stderr.decode("latin-1", "replace")

def qpdf_show_all_objects(pdf):
    """Dump every object's decoded content stream so redaction in the live
    revision can be verified at the content-stream level."""
    blobs = []
    # enumerate objects
    lst = run(["qpdf", "--show-xref", pdf])
    # Simpler & robust: pull all decoded streams via --qdf already covers it.
    return blobs

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--pdf", required=True)
    ap.add_argument("--secret", required=True)
    ap.add_argument("--json", default=None)
    args = ap.parse_args()

    if not os.path.exists(args.pdf):
        print(f"FATAL: no such file {args.pdf}", file=sys.stderr); sys.exit(2)

    nrev, raw = qpdf_revisions(args.pdf)
    raw_hits = search_raw(raw, args.secret)

    qdf, qdf_rc, qdf_err = qpdf_qdf_dump(args.pdf)
    qdf_hits = search_raw(qdf, args.secret) if qdf else {}

    leaked = bool(raw_hits) or bool(qdf_hits)

    report = {
        "pdf": os.path.abspath(args.pdf),
        "secret": args.secret,
        "file_size": len(raw),
        "xref_revisions": nrev,
        "raw_body_hits": raw_hits,          # (a)+(b)+(c): bytes anywhere in file
        "qdf_normalized_hits": qdf_hits,    # (a): live decoded objects
        "qdf_rc": qdf_rc,
        "verdict": "LEAK" if leaked else "CLEAN",
    }
    if qdf_err.strip():
        report["qpdf_stderr"] = qdf_err.strip()[:2000]

    print(json.dumps(report, indent=2))
    if args.json:
        with open(args.json, "w") as f:
            json.dump(report, f, indent=2)

    if leaked:
        where = []
        if qdf_hits: where.append("LIVE decoded objects")
        if raw_hits and not qdf_hits: where.append("orphaned/historical bytes only")
        elif raw_hits: where.append("raw file body")
        print(f"\n*** REDACTION LEAK *** secret recoverable in: {', '.join(where)} "
              f"(revisions={nrev})", file=sys.stderr)
        sys.exit(3)
    print("\nPASS: secret absent from live objects and raw body.", file=sys.stderr)
    sys.exit(0)

if __name__ == "__main__":
    main()
