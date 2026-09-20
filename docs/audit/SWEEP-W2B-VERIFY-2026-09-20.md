# SWEEP-W2B VERIFY — guarantee-verification-engine, second W2 pass (2026-09-20)

- **Role**: guarantee-verification-engine, W2 wave -b variant of the END-PHASE
  ponytail sweep. Scope: the 15 W1-remediated sweep-fix rows
  (W1-01..05 adversary, F1-F6 security, FZ-1..4 fuzz) + the legacy sweep's
  3 fixes (SL1 page-space law, SL2 conversion SafeSave, SL3 forms suggestion
  placement) — complementary to the -a pass (`.context/sweep-w2-verify-wip.md`).
- **Tip under test**: `feat/sweep-w2-verify-b` = `feat/parity-glm` @ **2d29a16**
  (full W1 remediation + legacy fixes; 22588a1 accessibility/R24 base is its ancestor).
  Probes committed at **9afc839**; verification commits at the branch tip.
- **Build**: fresh `build-w2b` (Debug, Ninja, UCRT64, `-j 2`), vendored podofo 1.1.0
  verified at configure (`podofo_DIR=third_party/podofo/install`), offscreen, serial.
- **Method per row (binding protocol)**: implementer claim → falsifiable contract →
  committed suite green on the tip → MY OWN probe through an independent seam/read
  path (PoDoFo raw dictionaries, PDFium FPDFAnnot/FPDFText, OpenSSL d2i_TS_RESP,
  raw file bytes, own hostile fixtures and hand-computed literals — never just the
  committed suite's assertions) → negative control (scoped revert of the fix to its
  NAMED base inside this worktree, probe must FAIL, restore, re-verify, capture) →
  verdict.
- **Probes** (committed on this branch, registered at the CMakeLists tail; run
  manually, not ctest members): `tests/W2BProbe{Naming,Signing,SummaryPolicy,
  A11y,LegacySpace,OfficeSave}.cpp`.
- **Evidence**: `.context/sweep-w2b-evidence/` — `suite-*.txt` (committed suites),
  `owner-*.txt` (owner suites), `probe-*.txt` (tip-state probes),
  `nc-*.txt` (negative controls), `probe-leg-embed-diag.txt` (the W2B-1
  root-cause diagnostics), `resp.ts`/`resp.hex` + generator materials (the real
  RFC 3161 TS_RESP fixture), `run-nc-battery.sh`.
- **Handoff**: `.context/sweep-w2b-wip.md` (final state at lane end).

## Recovered prior state (start of this pass)

The worktree carried the dead -a pass mid-negative-control: `PageSpaceTransform.h`
was left with its "W2-NC-L5L8 rotation-blind" production hack unrestored and its
probe registrations uncommitted. Both archived verbatim at
`.context/sweep-w2a-orphan-uncommitted.patch`, then the production file RESTORED
to committed state before any verification ran. The -a pass's handoff, untracked
probes and `.context` captures were left untouched. (Their `build-w2` dir — a
regenerable 18 GB artifact with no evidence content — was removed to unblock a
100%-full D: drive that had killed DLL staging and probe compilation.)

## Committed-suite gate (all GREEN on 2d29a16)

| Suite | Result |
|---|---|
| TestSweepW1PresetAdversary (W1-01 repros) | 6/6 |
| TestSweepW1SigningAdversary (W1-02/W1-03 repros) | 5 passed, 1 QSKIP (foreignUnsignedField slot QSKIPs BY DESIGN once precheck refuses — the refusal path is exactly what this slot's else-branch documents; my W1-03 probe supplies the strong runtime assertion) |
| TestSweepW1SummaryPolicyAdversary (W1-04/W1-05) | 8 passed, 1 QSKIP (documented diagnostic triage slot) |
| TestSweepW1SecProbe (F2/F3/F5/F4) | 6/6 |
| TestLegacyOriginSpace (SL1) | 9/9 |
| TestOfficeImport (SL2) | 10 passed, 1 QSKIP (real soffice absent) |
| TestAutoDetectHeuristic (SL3) | 5/5 |
| Owners: TestBatchPresets 14, TestSendForSigning 11+1 skip, TestPolicyController 12, TestSupportBundle 9, TestNetworkDisclosure 7, TestAccessibilityChecker 10, TestAccessibilityFixes 9, TestAccessibilityPanel 7, TestPrintableSummary 11, TestReviewSummary 9 | all 0 failed |

## Verdict table

Verdicts use the engine taxonomy: verified / partially verified / contradicted /
unproven. "NC" = negative control (named base → my probe's observed outcome).

| Fix | Verdict | Committed suite (tip) | My probe (independent path) | NC (base → observed on the base) |
|---|---|---|---|---|
| W1-01 preset-naming containment + AR-8 | **verified** | TestSweepW1PresetAdversary 6/6, TestBatchPresets 14/14 | W2BProbeNaming: 8 hostile templates (traversal, nested, drive-abs, UNC, ADS, dir-component) refused at parse with `output.naming` error; resolver rejects non-bare results; laundered backslash templates still resolve to a bare in-dir name (invariant); benign/UTF-8/240-char controls resolve | 9734e57^ → probe FAILS: parse accepts `....//....//evil_{n}.pdf`, resolver accepts `../evil_1.pdf` (nc-w1-01) |
| W1-02 1-field=1-signer at fromJson + verify alias audit | **verified** | TestSweepW1SigningAdversary green | W2BProbeSigning: aliased unsigned sidecar → SchemaInvalid naming both entries (whitespace aliasing included); aliased all-signed history parses but verifyAgainstDocument warns "bound to the same field" and consistent=false; distinct-binding control parses clean | 95a5044^ → probe FAILS: fromJson accepts the alias (error=0), verify reports consistent=true with no alias warning (nc-w1-02) |
| W1-03 precheck consults global one-unsigned-field precondition | **verified** | TestSweepW1SigningAdversary green (slot QSKIPs into the refusal branch) | W2BProbeSigning: doc + foreign unsigned `U_foreign` + lazy-anchor entry → `ForeignUnsignedField` refusal naming the field, document SHA-256 unchanged across the refused precheck (pure read); managed-field control not refused | 8726cb9^ → probe FAILS: precheck returns None — the step would proceed to mutate and deadlock (nc-w1-03) |
| W1-04 WinAnsi-safe per-string sanitizer at the draw boundary | **verified** | TestSweepW1SummaryPolicyAdversary 8P+1, TestPrintableSummary 11/11, TestReviewSummary 9/9 | W2BProbeSummaryPolicy: CJK author+text, TAB, 0x01, emoji, embedded NUL comment → writePrintable succeeds, SAVED artifact reopens via PoDoFo AND its text layer extracted via PDFium; "shown as" disclosure present in the artifact; Latin-1 é, em-dash, € survive verbatim (no over-sanitizing) | 0441192^ → probe FAILS: one hostile comment aborts the whole export (`PdfErrorCode::InvalidFontData`) (nc-w1-04) |
| W1-05 machine-policy trust-model disclosure | **verified** (disclosure-only fix; enforcement intentionally unchanged — the W1-05 residual stands) | TestPolicyController 12/12, TestSupportBundle 9/9 | W2BProbeSummaryPolicy: squatter policy via GLYPHPDF_POLICY_PATH → statusLine carries `trustModelNote()` ("machine-trusted … %PROGRAMDATA%") wherever the override renders; bundle policy.trustModel carries the canonical sentence | b5dc14e^ → probe FAILS: bundle loses the trustModel field; textual: pre-fix PolicyController.cpp has zero `trustModelNote` (nc-w1-05) |
| F1 policy header claim honesty | **verified** (claims fix — textual NC by nature) | TestPolicyController 12/12 | Source-level: header now states machine-TRUSTED, no ownership/ACL/signature/hash verification, pre-creatable location, names the structural close as owned elsewhere | 9a6732f^ → the refuted claim returns verbatim: "A machine administrator can deploy a policy file that OVERRIDES selected user preferences" |
| F2 garbage TSA token never claims B-T (three-state) | **verified** at the user-trust boundary (label); embed branch evidenced by committed probe + fix NC + source (no live https TSA harness in scope) | TestSweepW1SecProbe 6/6 | W2BProbeSigning: garbage (attempted, unparseable) → B-B floor; validated → B-T attained; absent attempt → B-T (previews stay honest — the 2d29a16 refinement); unreachable TSA → B-B; ground truth: the positive fixture IS a parseable RFC 3161 TS_RESP (d2i_TS_RESP + granted status; openssl ts -verify OK) and garbage is NOT parseable | 7a43c3c^ → probe FAILS exactly at the garbage floor (claims B-T) while the valid-token branch still passes — the regression is isolated to the gate (nc-f2) |
| F3 re-confirm authorization out of the sidecar | **verified** | TestSweepW1SecProbe 6/6, TestSendForSigning 11+1 | W2BProbeSigning: mutated bytes + sidecar-pre-seeded reconfirmedSha256 → precheck = DocumentChanged (user consulted); the OUT-OF-BAND `FillStepInput::userReconfirmedSha256` still authorizes (control) | ee6ba95^ → probe FAILS: the sidecar copy gates again — precheck proceeds past DocumentChanged (nc-f3) |
| F4 TSA touchpoint from the policy-EFFECTIVE value | **verified** | TestNetworkDisclosure 7/7 | W2BProbeSummaryPolicy: policy-managed signing/tsaUrl + empty user setting → TSA row enabled=true with the policy disclosure; no-policy control reads Disabled | 69fc6fb^ → probe FAILS: TSA shown DISABLED while the policy URL will fire (nc-f4) |
| F5 off-page signature anchors refused | **verified** on unrotated pages; inherits the W2B-1 geometry caveat on /Rotate 90/270 (the containment compares against PoDoFo's rotation-normalized MediaBox — see W2B-1) | TestSweepW1SecProbe 6/6, TestSendForSigning green | W2BProbeSigning: (200000,200000) anchor → refused with "outside page" error; independent PoDoFo reopen shows NO field (no partial write); on-page control still places | 2e4146d^ → probe FAILS: off-page anchor accepted — invisible field planted (nc-f5) |
| F6 bundle privacy-note honesty | **verified** | TestSupportBundle 9/9 | W2BProbeSummaryPolicy: privacyNote discloses "File paths appear only for the machine-policy location" and no longer claims "no file paths" | 20bdaa4^ → probe FAILS: "no file paths" overclaim returns (nc-f6) |
| FZ-1 cyclic /Fields bounded walk | **verified** | TestAccessibilityChecker 10/10, TestAccessibilityFixes 9/9, TestAccessibilityPanel 7/7 | W2BProbeA11y: MY OWN raw-bytes fixtures — self-referencing and mutually-referencing /Kids containers and a 12-deep chain → scan returns bounded reports, no crash, no throw | 12272f6^ → probe SEGFAULTS (rc=139) on the same fixtures — unbounded recursion reproduced (nc-fz12) |
| FZ-2 scanAccessibility never-throws seam | **verified** | same owner suites | W2BProbeA11y: all 6 committed scan-throw seeds + my own broken-lazy-object mutant return reports (partial with "scan-incomplete" where triggered) — no exception escapes on the tip | same NC run: the reverted build dies (segfault) on the cycle fixtures before the seed slot — the seam is demonstrably unguarded there; the lane's own repro logs (rc=42, FINDING S3_SCAN_THREW_PODOFO, 6/6 seeds) document the throw behavior pre-fix |
| FZ-3 sidecar magic VALUE verified | **verified** | TestSendForSigning green | W2BProbeSigning: magic 999 / 1.5 / true / null / "one" / trailing-duplicate-2 all refused MissingMagic with a found-vs-required detail; value-1 control decodes | 824532e^ → probe FAILS: magic 999 decodes as a valid request (nc-fz-3) |
| FZ-4 reserved DOS names + bounded render | **verified** | TestBatchPresets 14/14 | W2BProbeNaming: CON/con/Nul/AUX/PRN/COM1/com9/LPT1/lpt4 via token AND literal refused; trailing dots/spaces Win32 semantics; >240-char renders refused; 230-char and reserved-letter-containing controls pass | e8e715f^ (W1-01 kept) → probe FAILS: `CON.pdf` renders (device write), 304-char render accepted; traversal slots still pass (scoped isolation proven) (nc-fz-4) |
| SL1 annotation/form page-space law | **PARTIALLY VERIFIED** — see FINDING W2B-1 | TestLegacyOriginSpace 9/9 (fixture has no /Rotate 270 page) | W2BProbeLegacySpace (own 4-page fixture rot {0,90,180,270}, rot90+offset, display (60,80,100×50), hand-computed literals, raw-dict AND PDFium reads): rot 0 [60 662 160 712] ✓, rot90+offset [80 260 130 360] ✓, rot180 [452 80 552 130] ✓ (law maps y direct), form create/update on rot90+offset ✓, foreign-rect read-back on rot90+offset ✓; **/Rotate 270 FAILS everywhere** | db18f5e^ → probe FAILS on the shapes the fix claims: embed [280 260 330 360], form [250 240 290 360], extract surfaces (80,252) instead of (60,80) on the offset/rotated page (nc-sl1) |
| SL2 conversion SafeSave commit | **verified** | TestOfficeImport 10+1 skip | W2BProbeOfficeSave (own sentinel + fake-soffice re-exec): exit-0-no-product → failure + sentinel byte-identical; **exit-0-GARBAGE product → refused honestly + sentinel intact** (new assertion beyond the committed suite); commit-fault armed → failure + sentinel intact → retry replaces; success control replaces destination and consumes the candidate | 23bc970^ → probe FAILS both halves: sentinel DESTROYED by the failed run; garbage product committed over the destination as false success (nc-sl2) |
| SL3 form suggestions under the label | **verified** | TestAutoDetectHeuristic 5/5 | W2BProbeLegacySpace (own fixture `90 640 Td (Amount: ) Tj`): suggestion at display (138, 135.2) — under the label, not mirrored; placing stores RAW /Rect y 640..656.8 | 82e661c^ → probe FAILS: suggestion y=640 (content Y emitted as display Y — the double flip) (nc-sl3) |

## FINDING W2B-1 (NEW — blocks SL1 from full verified; owner: fix lane / space-law consumers)

**On /Rotate 270 pages the SL1 page-space law stores and reads TRANSPOSED
annotation and form-field rects** — silent misplacement, the exact defect class
SL1 claimed to fix:

- embed of display (60,80,100×50) on a /Rotate 270 Letter page stores raw
  /Rect **[662 452 712 552]**; the law requires **[482 632 532 732]**.
- `FormManager::addTextField` display (40,50,120×40) stores **[702 452 742 572]**
  vs law **[522 632 552 752]**.
- `extractAnnotations` read-back surfaces display **(-120,260,-20,310)** for a
  foreign spec-correct [482 632 50 100] instead of (60,80,100×50).

**Root cause** (diagnostics: `.context/sweep-w2b-evidence/probe-leg-embed-diag.txt`):
the saved FILE is correct (hand-verified bytes: MediaBox corners + /Rotate 270
verbatim); **vendored PoDoFo 1.1.0 `PdfPage::GetMediaBox()` returns the
rotation-normalized (W/H-swapped) box on /Rotate 90 and /Rotate 270 pages**
(`842 612` / `792 612` observed). `gp::PageSpace::pageGeometry()` documents
"MediaBox width (rotation-independent)" and consumes GetMediaBox directly. The
/Rotate 90 branch of the law never reads W/H — so 90 pages pass BY FORMULA SHAPE
(which is also why the committed TestLegacyOriginSpace, whose fixture covers rot
{0, 90} + offset, is green) — while the /Rotate 270 branch uses both, producing
a doubled rotation (transposed rects). /Rotate 0 and 180 are unaffected
(180 does not use W/H either).

**Blast radius / cross-cutting**: every consumer of `pageGeometry(page)` on
/Rotate 270 pages (annotation embed + extract, FormManager field create/move,
stamps riding the same boundary) and every W/H-dependent consumer on /Rotate 90
pages — concretely including F5's media-box containment check in
`SignatureFieldCreator` (compares the converted user rect against the swapped
box) and, to be re-audited by their owners, the SEP13 L5/L8 redaction geometry
paths on 270-degree pages.

**Suggested direction** (owner lane): derive PageGeometry from the raw page
dictionary (or correct GetMediaBox output by swapping back under odd rotation),
and add a /Rotate 270 page + a GetMediaBox-vs-file-bytes pin to
TestLegacyOriginSpace. Fix is review-only from this lane — production code was
never modified beyond the scoped NC reverts (all restored + re-verified).

## Negative-control inventory (all in .context/sweep-w2b-evidence/)

nc-w1-01, nc-fz-4, nc-w1-02, nc-w1-03, nc-f3, nc-w1-04, nc-w1-05 (+ textual
pins for PolicyController.cpp/PolicyController.h), nc-f4, nc-f5, nc-f2, nc-f6,
nc-fz12 (segfault, rc=139), nc-fz-3, nc-sl2, nc-sl3, nc-sl1. Every scoped revert
was restored (`git checkout HEAD -- <paths>`) and the restore-check rebuild +
full probe re-run reproduced the tip-state results exactly (5 suites green;
Legacy 5/8 with only the three W2B-1 rot-270 failures). `git status` on src/ is
clean.

## Residuals / partials

1. **W2B-1 (SL1 partial)**: /Rotate 270 transposed rects — see above. SL2/SL3
   unaffected (their probes pass on unrotated/rot-90 pages where the law holds).
2. F2 scoping: the label seam (the user-trust boundary) is verified with real
   TS_RESP ground truth on both branches; the in-CMS embed branch inside
   signDocumentImpl is verified via the committed probe, the fix's own captured
   NC, and source audit — a live hostile-https-TSA harness was out of scope
   (SignatureManager enforces https, and no test seam exists to inject a CA).
3. W1-05: disclosure-only by design — the squatter CAN still enforce a planted
   policy; the trust model is now disclosed at every enforcement surface (the
   structural ACL/signed-policy close remains a documented design item).
4. W1-03: the committed repro slot QSKIPs on the tip (its else-branch is the
   refusal); the strong runtime assertion (exact refusal code + pure-read byte
   identity) is carried by my probe.
5. TestOfficeImport's real-soffice slot skips (no LibreOffice in the test env);
   the three SafeSave slots (including my garbage-product probe) carry the
   contract.

## Suites + totals (tip)

- Committed sweep suites: 7 targets — 59 passed, 0 failed, 3 documented skips.
- Owner suites: 10 targets — 99 passed, 0 failed, 1 documented skip.
- My probes (tip): Naming 9/9, Signing 15/15, SummaryPolicy 10/10, A11y 7/7,
  OfficeSave 6/6 — all green; LegacySpace 5/8 with the three W2B-1 /Rotate 270
  failures (the finding).
- Negative controls: 16 runs + 2 textual pins; every reverted base FAILS its
  probe (defect detected each time).

## Final state

- Branch `feat/sweep-w2-verify-b`: 2d29a16 + 9afc839 (probes + this doc) + the
  ledger flip commit. Nothing pushed. No other worktree touched.
- Ledger: `sweep-legacy` rows SL1/SL2/SL3 flipped; new `sweep-w2b-verify`
  section appended with the per-row verdicts above.
- Handoff: `.context/sweep-w2b-wip.md` (final state, W2B-1 hand-off note).
