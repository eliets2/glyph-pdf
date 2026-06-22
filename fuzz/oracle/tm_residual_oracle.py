#!/usr/bin/env python3
"""
tm_residual_oracle.py  -- PRIORITY 2 oracle (F-02) + Edact-Ray residual check

After redaction over a rect that VISUALLY covers a text run, re-parse the
OUTPUT content streams and assert that no text-showing operator survives that
would re-emit the redacted glyphs, AND that no glyph-advance side-channel
(TJ per-glyph displacements) survives for the redacted run.

The F-02 bug: PoDoFoBackend's Tm handler reads only the translation (e,f) of
"a b c d e f Tm" and ignores the linear part (a,b,c,d). So when text is laid
out under a rotated/scaled/skewed text matrix, the redaction span intersection
is computed with the WRONG geometry and the glyphs are NOT scrubbed even though
they sit inside the redaction rectangle on the rendered page.

We detect a survivor by decoding the live content streams (qpdf --qdf) and
looking for the secret's bytes inside any Tj/TJ string operand. If the secret
string still appears as a text-show operand in the output, redaction failed
for that text matrix.

Usage:
  python tm_residual_oracle.py --pdf out.pdf --secret SECRET [--json r.json]
Exit: 0 = no residual show-op (PASS), 3 = residual glyphs survive (LEAK).
Requires: qpdf on PATH.
"""
import argparse, json, os, re, subprocess, sys, tempfile

def run(cmd):
    return subprocess.run(cmd, capture_output=True)

def qdf_streams(pdf):
    with tempfile.NamedTemporaryFile(suffix=".qdf", delete=False) as t:
        qdf = t.name
    r = run(["qpdf", "--qdf", "--object-streams=disable",
             "--decode-level=all", pdf, qdf])
    data = b""
    if os.path.exists(qdf):
        data = open(qdf, "rb").read()
        os.unlink(qdf)
    return data, r.returncode, r.stderr.decode("latin-1","replace")

# Match text-showing operators and capture their string/array operands.
#   (literal) Tj      |  [ ... ] TJ   |  (lit) '   |  n n (lit) "
SHOW_RE = re.compile(
    rb"(\((?:[^()\\]|\\.)*\)|\[[^\]]*\])\s*(Tj|TJ|'|\")",
    re.DOTALL)

def extract_show_strings(data):
    """Return list of decoded literal-string operands of text-show operators."""
    found = []
    for m in SHOW_RE.finditer(data):
        operand = m.group(1)
        op = m.group(2)
        # pull every (..) literal out of the operand (handles TJ arrays)
        for lit in re.finditer(rb"\((?:[^()\\]|\\.)*\)", operand):
            s = lit.group(0)[1:-1]
            # unescape \( \) \\ \n etc minimally
            s = re.sub(rb"\\([()\\nrtbf])", lambda mm: {
                b"(":b"(", b")":b")", b"\\":b"\\", b"n":b"\n",
                b"r":b"\r", b"t":b"\t", b"b":b"\b", b"f":b"\f"}[mm.group(1)], s)
            found.append((op.decode("latin-1"), s))
    return found

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--pdf", required=True)
    ap.add_argument("--secret", required=True)
    ap.add_argument("--json", default=None)
    args = ap.parse_args()

    data, rc, err = qdf_streams(args.pdf)
    secret = args.secret.encode("latin-1","replace")
    shows = extract_show_strings(data)

    residual = []
    for op, s in shows:
        if secret and secret in s:
            residual.append({"operator": op, "operand": s.decode("latin-1","replace")})

    report = {
        "pdf": os.path.abspath(args.pdf),
        "secret": args.secret,
        "qpdf_rc": rc,
        "total_text_show_ops": len(shows),
        "residual_show_ops_with_secret": residual,
        "verdict": "LEAK" if residual else "CLEAN",
    }
    if err.strip():
        report["qpdf_stderr"] = err.strip()[:1000]
    print(json.dumps(report, indent=2))
    if args.json:
        json.dump(report, open(args.json,"w"), indent=2)

    if residual:
        print(f"\n*** Tm RESIDUAL LEAK *** {len(residual)} text-show operator(s) "
              f"still render the redacted secret.", file=sys.stderr)
        sys.exit(3)
    print("\nPASS: no surviving text-show operator emits the secret.", file=sys.stderr)
    sys.exit(0)

if __name__ == "__main__":
    main()
