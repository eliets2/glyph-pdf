# AD-02 evidence — NUL-whitespace ISA/shadow-scan evasion (feat/sec-consume-ocsp)

Base: `2ccfd5ba`. Suite: `TestSignatureRealCrypto` (34 slots after this lane).
Hypothesis verdict: **CONFIRMED** (dynamic), and the falsification run surfaced a
SECOND, independent hole in the same classifier.

## Falsification result

`testNulWhitespaceObjectHeaderShadowScan` builds a real B_B-signed base, then
appends a hand-crafted raw-bytes incremental revision whose redefinition of a
base stream object uses a NUL-whitespace header (`12 0<NUL>obj`), with a
well-formed xref/trailer/startxref chain anchored at the base's `%%EOF`.

1. **PoDoFo honors it** — vendored PoDoFo 1.1.0 resolves the base object number
   to the attacker payload stream (header contract:
   `PdfUtils.h IsCharWhitespace('\0') == true`). The parser-side premise of the
   hypothesis holds.
2. **The scanner passed the revision** (fail-before log: `!classifiedLegit`
   FALSE, empty reason): PCRE2 `\s` never matches NUL, so the classifier's
   `(\d+)\s+(\d+)\s+obj` scan saw no objects and returned true — evasion
   CONFIRMED at scanner level. (End-to-end consequence asserted in the same
   test: the verdict must leave Valid.)
3. **Fix**: explicit PDF-whitespace class (ISO 32000-1 §7.2.2 incl. NUL):
   `([0-9]+)[ \t\n\x0B\f\r\x{0000}]+([0-9]+)[ \t\n\x0B\f\r\x{0000}]+obj` —
   tokenize-consistent with the vendored parser.

## Second hole found by the same test (independent of NUL)

With the regex fixed, the pin STILL failed: the "1b /Info ModDate save-noise"
allowlist branch continued on ANY modified base object whenever the base /Info
(wherever it lives) happened to be intact — it never checked that the matched
object IS the /Info object (`rawInfoObj == objNum` guard missing). That
whitelists a plain modified non-catalog object (the classic shadow attack)
without any NUL trickery. NC log
`02-nc-reverted-1b-guard-only-TestSignatureRealCrypto.log` shows the pin
failing with ONLY the 1b guard reverted (regex fix kept) — the 1b hole alone
suffices. Fix: scope the allowance to the matched /Info object. The app's own
B-LT /Info-noise allowance (PROGRAM-CONSOLIDATION 1.7c pins) still passes
(full suite green).

## Pin semantics (must never be weakened)

The single test asserts, always: (a) records whether vendored PoDoFo honors the
NUL-header redefinition (refutation record), (b) `isLegitimateIncrementalAppend`
(refused), and (c) the end-to-end `validateSignatures` verdict leaves
Valid/ValidWithDSS with `isValid == false`.

## Totals

- fail-before: 33 passed, 1 failed (AD-02 pin RED — evasion).
- NC (1b guard reverted, regex fixed): 33 passed, 1 failed (independent hole).
- pass-after ×3 serial: 34 passed, 0 failed each.

## Owner-item hardening (out of this wave's scope)

The audit's deeper remediation stands: enumerate the appended revision's
objects from the parsed structure (the new xref's entries) instead of scanning
raw bytes textually — this removes the whole class (including UTF-8-decode
mangling of invalid sequences through `QString::fromUtf8`). The committed fix
is the minimal tokenize-consistent correction.
