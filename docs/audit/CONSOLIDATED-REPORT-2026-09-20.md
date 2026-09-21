# CONSOLIDATED REPORT — GlyphPDF end-phase sweep (W3), 2026-09-20

- **Deliverable of:** the consolidated-report drafting lane (`feat/consolidated-report`, cut from
  mainline `feat/parity-glm` @ `ec9f16f6`). Docs-only synthesis: zero `src/`/`tests/` changes;
  every claim below cites its source document; **no new claims** — this is synthesis, not review.
- **Status vocabulary follows the ledger** (`CURRENT-EVIDENCE-LEDGER-2026-09-05.md` — the
  authoritative status source): `verified` only where a named independent verification pass
  (R14, SWEEP-W2B, SWEEP-W2C) flipped the row; everything else is
  `implemented-awaiting-review` (or the lane's own recorded status). The ledger wins on any
  wording conflict.
- **ADDENDUM SLOTS** (section 7): three lanes were still running when this report was drafted
  (ux-resume, modularity-moves, the 48 h re-soak) plus the pending verification review of the
  newest fix rows. Their verdicts are to be appended by the coordinator in section 7's marked
  slots — the body of this report does not pre-empt them.

**Primary sources** (all `docs/audit/` unless noted): SWEEP-W1-ADVERSARY-2026-09-19.md,
SWEEP-W1-SECURITY-2026-09-20.md, SWEEP-W1-FUZZ-2026-09-20.md, SWEEP-W2-TESTING-2026-09-20.md,
SWEEP-W2B-VERIFY-2026-09-20.md, SWEEP-W2C-2026-09-20.md (branch `feat/sweep-w2c` /
`feat/runintersects-precision`), SWEEP-W3-ARCHAEOLOGIST / -ARCHITECT / -DEVOPS / -EMERGENCE /
-RESEARCH / -UX-2026-09-20.md (lane branches), PERF-BASELINE-2026-09-20.md (`feat/sweep-w3-perf`),
SOAK-VERDICT-2026-09-20.md + RESOAK-2026-09-20.md (`feat/soak-48h-resume`),
CONSOLIDATION-PLAN-2026-09-20.md (`feat/consolidation-plan`), SWEEP-LEGACY-2026-09-20.md,
SEP13-LEADS-CONFIRMATION-2026-09-14.md, INDEPENDENT-REVIEW-2026-09-14.md,
CURRENT-EVIDENCE-LEDGER-2026-09-05.md. Remediation evidence: `feat/sweep-w1-fixes`
(`e8e715f2`), `feat/rotate270-fix` (merged `ec9f16f6`), `feat/sanitize-assert-mutable`
(merged), `feat/emergence-fixes` (`65500182`), `feat/runintersects-precision` (`e620757b`,
active), `feat/modularity-moves` (`3c411cc8`, in flight).

---

## 1. Executive summary

### 1.1 What the sweep covered

The end-phase sweep was a three-wave, multi-role audit of the integration candidate at the
mainline tip (W1 base `8f62a17`, W2 base `2d29a16`, W3 base `b17106a`/`ec9f16f6`):

- **W1 — attack the newest surfaces** (3 audit roles + a remediation lane):
  native-adversary (SWEEP-W1-ADVERSARY), security/claims-honesty auditor (SWEEP-W1-SECURITY),
  fuzz-harness-engineer (SWEEP-W1-FUZZ, 4,608 campaign execs + 80 hostile render probes +
  ~40 single-seed repros across 5 parsers/JSON surfaces). Remediation: `feat/sweep-w1-fixes`.
- **W2 — audit and verify the suite and the fixes** (4 audit passes + fix lanes):
  testing-specialist (suite audit, 170 ctest targets / 171 sources), the legacy
  evaluate-and-fix lane (SWEEP-LEGACY: 3 fixes SL1/SL2/SL3), and three guarantee-verification
  passes — W2 -a, W2 -b (SWEEP-W2B: the 15 W1 fixes + 3 legacy fixes, full probe +
  negative-control protocol), W2c (SWEEP-W2C: the W2B-1 re-submission). Fix lanes:
  `feat/rotate270-fix` (W2B-1), `feat/runintersects-precision` (W2c residual).
- **W3 — the system around the code** (9 audit roles + fix/ops lanes): code-archaeologist
  (dead weight), solution-architect (modularity), devops-engineer (packaging/repro/CI/disk),
  emergence-engine (feature-composition matrix), research/tracking-corpus (ground truth),
  ux-specialist (real-flow audit), ui-specialist (visual lens, parallel lane, in flight),
  perf lane (quiet-gated baseline), consolidation-analyst (branch endgame plan). Fix lanes:
  `feat/emergence-fixes` (E-1..E-6), `feat/sanitize-assert-mutable` (the soak's crash catch).
- **Ops evidence in parallel:** the first 48 h soak + verdict (SOAK-VERDICT) and the
  reboot-resilient re-soak (RESOAK, live at report time).

(The "16 roles" framing is the coordinator's composition of the audit seats above —
3 in W1, 4 in W2 counting the three verification passes separately, 9 in W3. Fix, soak and
consolidation lanes are execution seats, not audit seats.)

### 1.2 Headline numbers (reconciled against the ledger)

| Group | Confirmed findings | Remediation | Verification status (ledger vocabulary) |
|---|---|---|---|
| SEP13 leads (2026-09-14, pre-sweep baseline) | **13 leads confirmed** (L1–L13; L8 escalated to DATA-LOSS-CLASS by composition with L5+L7) + static leads M1/M3/M5/M7/M8 confirmed; M4 confirmed-but-unreachable (skipped); M6 inconclusive; L2b/L2c refuted | all fixed (`feat/sep13-leads`, `feat/sep13-fixes`, `feat/sep13-residual`, follow-ups lane) | R14 independent review flipped **21 rows verified, 1 partial** (L7 — its FINDING F1); L4/L12/RES-1/RES-2 not independently reviewed → implemented-awaiting-review (ledger §sep13 rows) |
| W1 (adversary + security + fuzz) | **15 confirmed** — W1-01..05 (5), F1–F6 (6), FZ-1..4 (4); plus 7 adversary hypotheses REFUTED with pins, 3 hypotheses open (H1–H3), ~9 security suspicions refuted, 2 fuzz surfaces CLEAN | 15/15 fixed on `feat/sweep-w1-fixes` (one commit per defect) | **15/15 verified** by SWEEP-W2B (independent probes + negative controls); F5's rotated-page caveat **lifted** by SWEEP-W2C (SWEEP-W2C §Verdicts) |
| Legacy sweep (SWEEP-LEGACY) | **3 fixes** (SL1 page-space law, SL2 office-conversion commit, SL3 form-suggestion placement); the rest of the legacy surface reviewed clean at tip | 3/3 fixed | SL2, SL3 **verified** (SWEEP-W2B); SL1 **verified** (SWEEP-W2C, after FINDING W2B-1 was raised and resolved) |
| W2 verification findings | **1 blocking finding (W2B-1)**: /Rotate 270 transposed rects; plus the FU-2 flake class demonstrated live and fixed; TestLaneScheduler classified genuinely-flaky | W2B-1 fixed (`feat/rotate270-fix` 879c171); FU-2 fixed (RESOURCE_LOCK, 15 suites) | W2B-1 **verified** by SWEEP-W2C (SL1 flip); FU-2 patch verified by full `ctest -j 2` **171/171** patched run (SWEEP-W2-TESTING §3.1) |
| Soak catch | **1 crash-class defect**: `TestSanitization` SegFault 0xc0000005 in `PoDoFo::PdfDataContainer::AssertMutable` (silent UAF; 2/58 passes) | fixed `ed04426e` (`feat/sanitize-assert-mutable`): in-place /Info + /Outlines scrub; +`TestSanitizeTrailerUaf` pin (fail-before 10/10 incl. 8x soak-signature SegFault; NC 5/5 fail) | implemented-awaiting-review (ledger §sanitize-crash lane); `TestSanitization` x3 clean at the re-soak tip (RESOAK §2) |
| W3 emergence | **6 confirmed interaction defects** (E-1..E-6) out of a 12-cell composition matrix; SAFE halves pinned by 9/9 suites at the tip | 6/6 fixed on `feat/emergence-fixes` (645f4994, 3325ea19, ee2cf661, cee2777c, 9463f6c0, c1552c26); ledger rows EM-1..EM-6 | **implemented-awaiting-review** (ledger §emergence-fix lane, lines ~1132–1149); verification review pending — ADDENDUM SLOT A4 |
| W3 audits (archaeo/arch/devops/perf/research/ux) | 0 code defects confirmed in the UX flows that ran; documented findings with dispositions: devops D1–D6 + C1–C3 + B1, perf F1–F6 (+ W2c's runIntersects precision residual → fixed `ri-fix`, implemented-awaiting-review), archaeo 0 PROVEN-SAFE / 2 NEEDS-REVIEW / 6 KEEP-ANYWAY, architect 13 upward edges (10 live + 3 dead) with a sequenced roadmap | documented (docs-only lanes by design); ri-fix fixed on `feat/runintersects-precision` | per-document dispositions; B1 seam + dead-include sweep **done on `feat/modularity-moves`, not yet merged** (AM1/AM2 implemented-awaiting-review) — ADDENDUM SLOT A2 |

**Reconciliation note.** "All remediated" holds for every confirmed *defect* finding from the
attack/verification/soak lanes (15 W1 + 3 legacy + W2B-1 + the soak UAF + 6 emergence + the
W2c precision residual). It does **not** mean zero open risk: the emergence six, the soak UAF
fix and the ri-fix are implemented-awaiting-review (not yet independently verified); L7
remains **partial** in the ledger (R14 FINDING F1 — annotation/form attribution on /Rotate
pages); W1-05/F2 carry disclosed scoping residuals; and the W3 audit lanes' operational
findings (devops, perf, ux, archaeo) are documented-with-disposition, several deliberately
not coded in-sweep (multi-lane discipline).

### 1.3 The product's honest state

- The mainline tip `ec9f16f6` carries the full sweep remediation through the rotate-270 law
  (W2B-1 fix) and the sanitize UAF fix; the emergence fixes sit on `feat/emergence-fixes`
  (not yet merged to mainline).
- Verification discipline held: every W1/legacy fix was re-proven by an independent
  verification pass through seams the committed suites do not exercise (raw dictionaries,
  PDFium, OpenSSL d2i_TS_RESP, raw bytes), each with a scoped-revert negative control that
  re-created the defect (16 NC runs + 2 textual pins in W2B; W2C's NC re-transposed all five
  of its own probe slots). No test was weakened — the one contract change (key-absence →
  data-absence in the sanitize pins) is documented as a strengthening, not a weakening
  (ledger §sanitize-crash lane).
- The heaviest remaining honest caveats: (a) the 48 h endurance claim is **not yet
  evidenced** — the first soak was host-killed at 4 h with zero candidate-attributable
  failures, and the reboot-resilient re-soak verdict is pending (ADDENDUM SLOT A3);
  (b) rotated-page annotation attribution (L7/F1) is a recorded partial; (c) the
  machine-policy trust boundary is disclosed, not enforced (W1-05 residual); (d) the ONNX
  models have no bootstrap coverage — "CI green" does not yet mean "MSI producible from a
  clean clone" (devops D1).

## 2. Findings ledger

One row per finding. Columns: ID · source wave/role · severity (as recorded by the source
lane) · one-line description · remediation commit + suite evidence · verification status.
Statuses are the ledger's. SHAs are short forms of the remediation commits unless prefixed
"NC base" (the base a negative control reverted to).

### 2.1 SEP13 confirmed leads (2026-09-14) — confirmation: SEP13-LEADS-CONFIRMATION-2026-09-14.md; fixes: ledger §sep13-leads / §sep13-fixes / §sep13-residual; verification: INDEPENDENT-REVIEW-2026-09-14.md

| ID | Source | Sev | One-line description | Fix commit + evidence | Verification status |
|---|---|---|---|---|---|
| L1 | SEP13 lead (B-T downgrade) | honesty/false-success | Dead TSA silently downgraded B-T→B-B, reported plain `Success` | d58896f; TestSep13LeadBtDowngrade 4/4; NC base revert → plain-Success fail | **verified** (R14) |
| L2 | SEP13 lead (HTML export) | injection | Raw PDF BaseFont name interpolated into `style="…"` — attribute breakout | 3247de5; TestSep13LeadConversionExport 6/6; NC re-injects `color:red` | **verified** (R14) |
| L3 | SEP13 lead (capability) | feature-loss | Degraded branch never reversed the registry-owned disable — control stuck disabled | 7b2cc97; TestSep13LeadCapability 5/5; NC stuck-disabled fail | **verified** (R14) |
| L4 | SEP13 lead (X509 leak) | static-LOW | `leafCert`/`issuerCert` leaked on signing error paths | 67b7887; RAII wrappers; compile + TestSignatureRealCrypto 23/0/1 + code-state NC | implemented-awaiting-review (R14: "not re-verified; no flip requested") |
| L5 | SEP13 lead (redaction proof) | false-certification | Proof mark→region mapping flipped Y with MediaBox HEIGHT only; /Rotate + origin ignored → false `VerifiedNoTextInRegion` | 1e2ab02; TestSep13LeadRedactionProof 8/8; shared `PageSpace::viewerToUser` | **verified** (R14); re-proven on /Rotate 270+offset by SWEEP-W2C slot 4 |
| L6 | SEP13 lead (pack disclaimer) | overclaim | Pack wording claimed decode-level extraction coverage — the exact recall gap | 811938d; wording pin in TestRedactionProof (fail-before observed) | **verified with caveat** (R14: wording inherits L7's /Rotate scope gap) |
| L7 | SEP13 lead (annotation attribution) | false-certification | Attribution blind to annotation/form-field strings → certified clean PASS over a surviving secret | f2a9d06; +TryGetReference sweep (3 sites); 8/8 rotate-0 slots | **partial** — R14 FINDING F1: `GetRect()`-based attribution misses /Rotate pages (false PASS demonstrated on the FIXED build); ledger keeps L7 partial; related probe failure disclosed as pre-existing by the ri-fix lane (owner triage) |
| L8 | SEP13 lead (excision/overlay) | DATA-LOSS-CLASS (escalated from LOW) | Excision rects flipped with HEIGHT only — on offset pages the excision MISSES while outcome reports Completed (composes with L5+L7 into silent certified secret loss) | 938401b; shared PageSpace transform; overlay slot 8/8; NC-L8 secret survives w/ Completed | **verified** (R14); re-proven on /Rotate 270+offset by SWEEP-W2C slot 4 |
| L9 | SEP13 lead (batch merge) | false-accounting | Failed merge save appended an N+1th result while per-input successes pointed at the never-written output | f1c490e (one commit for L9+L10); TestSep13LeadBatchMerge 5/5; NC "3 results for 2 files" | **verified** (R14) |
| L10 | SEP13 lead (batch merge cancel) | false-accounting | Cancel at a file boundary drained already-streamed successes against the never-written output | f1c490e; cancelledMergeMustNotReportSuccesses; NC "counted 1 success" | **verified** (R14) |
| L11 | SEP13 lead (OCR guards) | re-entrancy | `onRejectResults`/`onReOcrRegion` lacked the ReviewReady guard | 67d03f3; TestSep13LeadOcrGuards 5/5; NC both repros | **verified** (R14) |
| L12 | SEP13 lead (compare perf) | perf | `applyChangeTypeFilters` O(rows x anchors) per toggle | 4d0b606 (follow-ups lane); anchor-index memo; ratio 15.8–16.5x → ~7x; probe re-contracted to GUARD | implemented-awaiting-review (confirmation doc §L12 records the flip; no independent review named) |
| L13 | SEP13 lead (CSV columns) | data-fidelity | Ragged right-aligned column split into two spreadsheet columns | 8ed5204 (with M7); TestSep13LeadConversionExport; NC `","5"` fail | **verified** (R14) |
| M1 | SEP13 static lead | static-LOW | `releaseResidentFile` left `encryptionPassword` resident | da36edf; code-state NC (clear calls 3→4) | **verified** (R14, static counts) |
| M3 | SEP13 static lead | static-LOW | Signature candidate orphaned on commit-fail / integrity-broken paths | 36f71e7; code-state NC (cleanupCandidate 5→7) | **verified** (R14, static counts) |
| M5 | SEP13 static lead | display-only | OCR word edits never refreshed the scan canvas | 7b643d6; code-state NC (setWords 1→3) | **verified** (R14, static counts) |
| M7 | SEP13 lead (line join) | data-fidelity | 0.5x max-font join tolerance let a 24 pt heading swallow an 8 pt line 10 pt below | 8ed5204; tolerance bounded by min font; NC merged-line fail | **verified** (R14) |
| M8 / RES-2 | SEP13 static lead | static-LOW | `RedactOperation` leaked per redaction run | 1235451 (`feat/sep13-residual`); deleteLater idiom; TestRedactMarkAll pin + NC | implemented-awaiting-review |
| RES-1 | L1 UI residual | honesty | `timestampMissing` never rendered by the UI (empty missing-pieces list, label attested requested level) | 2a334ca (`feat/sep13-residual`); TestSignatureBadges pin x2 + NCs | implemented-awaiting-review |
| L2b / L2c | SEP13 leads | — | OpenXLSX silent overwrite / PPTX escaping | — | **REFUTED** (dead code / passing probe) |
| M2, M4, M6 | SEP13 leads | — | M2 = L10; M4 truncation unreachable (>2 GiB plaintext — skipped); M6 perf-only redundancy | — | M4 skipped by lane decision; M6 INCONCLUSIVE |

R14 gate context: full suite 152/154 at `39aaca3` (1 = the F1 demonstration slots, 1 =
TestEncryptedPackageSafeWrite transient, green on instructed rerun) (INDEPENDENT-REVIEW §gate).

### 2.2 W1 findings (15) — sources: SWEEP-W1-ADVERSARY-2026-09-19.md, SWEEP-W1-SECURITY-2026-09-20.md, SWEEP-W1-FUZZ-2026-09-20.md; fixes: `feat/sweep-w1-fixes`; verification: SWEEP-W2B-VERIFY-2026-09-20.md §Verdict table (+ SWEEP-W2C where noted)

| ID | Source | Sev | One-line description | Fix commit + evidence | Verification status |
|---|---|---|---|---|---|
| W1-01 | adversary | HIGH | Batch-preset naming-template literals never containment-checked — `../`, absolute paths pass; with `onConflict: overwrite` → silent arbitrary overwrite (CWE-22/73) | 9734e57: resolved-result validation in `resolveNaming` + AR-8 ask mapping; TestSweepW1PresetAdversary flips green; NC base 9734e57^ accepts traversal | **verified** (W2B: 8 hostile templates refused incl. UNC/ADS; laundered templates still bare) |
| W1-02 | adversary | MED-HIGH | 1-field=1-signer lint absent at `fromJson` — aliased sidecar fields defeat coverage verification (false "all signed") | 95a5044: SchemaInvalid at fromJson + alias audit in verify; TestSweepW1SigningAdversary; NC 95a5044^ alias accepted, verify consistent=true | **verified** (W2B: whitespace aliasing included; all-signed history warns) |
| W1-03 | adversary | MEDIUM | precheck validated the entry, not the engine's global precondition — foreign unsigned field → document mutated + permanent request deadlock | 8726cb9: precheck enumerates anchors + validateSignatures before mutation; NC 8726cb9^ precheck None | **verified** (W2B: `ForeignUnsignedField` refusal, document SHA-256 unchanged; committed slot QSKIPs by design, probe carries the strong assertion) |
| W1-04 | adversary | MEDIUM | Per-string encoding fault (TAB/CJK/emoji) aborted the ENTIRE printable-summary export | 0441192: WinAnsi-safe sanitizer at the draw boundary; NC 0441192^ one hostile comment kills export | **verified** (W2B: hostile set survives, saved artifact re-extracted via PDFium; é/—/€ verbatim — no over-sanitizing) |
| W1-05 | adversary (design) / security F1 | MED-LOW / High-Med | Machine policy loaded and machine-enforced with zero ownership/ACL/provenance check — attacker-writable `%PROGRAMDATA%\GlyphPDF\policy.json` (installer pre-creates nothing) | b5dc14e (disclosure: trustModelNote wherever overrides render) + 9a6732f (F1 claims fix: header states machine-TRUSTED, no ACL check); NCs textual + bundle-field | **verified as disclosure-only** (W2B); **the enforcement residual stands by design** — structural ACL/signed-policy close remains a documented design item (SWEEP-W2B §Residuals 3) |
| F2 | security | MEDIUM | "PAdES B-T attained" claimed for ANY non-empty TSA response — token bytes never parsed (garbage/HTML error page embedded, label overclaims) | 7a43c3c: parse as TS_RESP before embedding/claiming (three-state); NC 7a43c3c^ garbage claims B-T | **verified at the label boundary** (W2B; ground truth = real RFC 3161 TS_RESP via d2i_TS_RESP + `openssl ts -verify`); embed branch scoped to committed probe + NC + source — no live hostile-https TSA harness (residual, §2.6) |
| F3 | security | MEDIUM | Sidecar-pre-seeded `reconfirmedSha256` bypassed the re-confirm dialog — mutated document signed with no user consult | ee6ba95: re-confirm authorization moved out of the sidecar (out-of-band `userReconfirmedSha256` still authorizes); NC ee6ba95^ sidecar copy gates | **verified** (W2B) |
| F4 | security | MEDIUM | Network page + support bundle reported the TSA touchpoint Disabled while the policy-enforced URL would fire (R24(c) contract break) | 69fc6fb: touchpoint state from the policy-effective config; NC 69fc6fb^ Disabled-shown | **verified** (W2B) |
| F5 | security | LOW-MED | Sidecar anchor rect never clamped to page bounds — invisible off-page signature fields plantable | 2e4146d: off-page anchors refused; NC 2e4146d^ invisible field planted | **verified**; W2B's /Rotate 90/270 MediaBox caveat **LIFTED by SWEEP-W2C** (containment now judges the raw box; edge-crossing and just-off anchors refused on offset-270, source byte-identical) |
| F6 | security | LOW | Support bundle claimed "no file paths" while the policy statusLine embeds one | 20bdaa4: note discloses the machine-policy path exception; NC 20bdaa4^ overclaim returns | **verified** (W2B) |
| FZ-1 | fuzz S3 | CRASH/DoS | Cyclic /Fields hierarchy — unbounded recursion in `collectFieldGaps` (no depth cap, no cycle set) → bad_alloc/SEGV; crafted PDF kills the app on opening the a11y panel | 12272f6: depth cap + visited-reference set; NC 12272f6^ SEGFAULT (rc=139) on the same fixtures | **verified** (W2B: own raw-bytes self/mutual cycles + 12-deep chain bounded) |
| FZ-2 | fuzz S3 | CRASH-class | `scanAccessibility` leaked PdfError past its "never throws" contract (6 mutants, 4 base fixtures) | ddf41253: exception containment at the seam → partial report with "scan-incomplete" disclosure | **verified** (W2B: all 6 committed seeds + own broken-lazy mutant contained) |
| FZ-3 | fuzz S1 | ACCEPTS-INVALID | Sidecar magic VALUE never verified — `"glyphpdf-signrequest": 999/1.5/true/null` all decoded as valid | 824532e: value verified, found-vs-required detail; NC 824532e^ magic 999 decodes | **verified** (W2B) |
| FZ-4 | fuzz S2 | data-loss / minor | Reserved DOS device names (`CON.pdf` → device write reported as success) + unbounded rendered length (>240) | e8e715f: reserved-name + length gates in `resolveNaming` (extends W1-01); NC e8e715f^ CON.pdf renders, 304-char accepted | **verified** (W2B: token + literal, case-insensitive, Win32 trailing dots/spaces) |

Fuzz surfaces CLEAN (no findings): S4 printable-summary chain (320 execs), S5 policy file +
path resolution (736 execs) (SWEEP-W1-FUZZ §Verdict matrix). The fuzz lane's S2
template-escape finding is the same root W1-01 fixed; its reserved-DOS/length findings are
FZ-4. Adversary refutations (W1-R1..R7) and open hypotheses (W1-H1 two-instance signing,
W1-H2 consent-before-reload, W1-H3 preset-chain throw-leak) are carried in §2.6/§7.

### 2.3 Legacy sweep fixes — source: SWEEP-LEGACY-2026-09-20.md + ledger §sweep-legacy; verification: SWEEP-W2B / SWEEP-W2C

| ID | Source | Sev | One-line description | Fix commit + evidence | Verification status |
|---|---|---|---|---|---|
| SL1 | sweep-legacy lane | silent-misplacement (data-loss class on rotated/offset pages) | Annotation/form write+read-back flipped viewer Y with MediaBox HEIGHT alone; /Rotate ignored — saved geometry landed where the user did not draw while the overlay looked right | db18f5e: NEW `core/ItemSpaceTransform.h` = exact inverse of the verified `viewerToUser`; raw verbatim writes; TestLegacyOriginSpace 9/9 (FAIL-FIRST 6F/3P; scoped-revert NC) | **verified (SWEEP-W2C)** after being PARTIAL at SWEEP-W2B with FINDING W2B-1 (below) |
| SL2 | sweep-legacy lane | destructive-failure | Office-conversion tail removed the previous output BEFORE rename — a failed rename destroyed it while reporting false (phantom-success class) | 23bc970: validate product first, commit via `SafeSave::commitFileToDestination`; TestOfficeImport +3 slots; NC sentinel destroyed | **verified** (W2B; NEW exit-0-garbage slot beyond the committed suite) |
| SL3 | sweep-legacy lane | silent-misplacement | Auto-suggested form fields emitted in user space against a display-space contract — every suggestion mirrored to the opposite side of the page | 82e661c: single `userToViewer` mapping; TestAutoDetectHeuristic 5/5; NC mirrored y=640 | **verified** (W2B) |

Reviewed-clean at tip (no failing pin → no fix, per lane method): compare engine + UI flows,
OCR scan/review canvas, outlines/bookmarks, page-op bounds honesty, stamps (rides SL1),
measurement (rides SL1) (ledger §sweep-legacy, "Reviewed-clean" paragraph).

### 2.4 W2 findings and the soak catch

| ID | Source | Sev | One-line description | Fix commit + evidence | Verification status |
|---|---|---|---|---|---|
| W2B-1 | SWEEP-W2B (guarantee-verification pass) | silent-misplacement (blocks SL1 full verified) | On /Rotate 270 the SL1 law stored/READ transposed rects (embed [662 452 712 552] vs law [482 632 532 732]); root cause: vendored PoDoFo 1.1.0 `GetMediaBox()` is rotation-normalized on 90/270 and the law consumed it as rotation-independent (90 passed only because its formula never reads W/H) | 879c171 (root: `pageGeometry` from `GetMediaBoxRaw().GetNormalized()`) + SignatureFieldCreator containment-vs-raw-box + verbatim signature /Rect (CreateField double-transform corruption found and fixed) + SL3 clamp from shared geometry; tests 28b5c48 (TestRotate270PageSpace 7/7; TestLegacyOriginSpace 9/9 w/ new rot-270+offset page; TestRedactionProof rotated slot); merged at ec9f16f6; docs 9e1cde9 | **verified** (SWEEP-W2C: independent probe W2CProbeRotate270 7/7 through the production move/resize path on all 6 page shapes with own literals + raw-dict/PDFium/raw-bytes readers; scoped NC of 879c171 → 2P/5F with transposed shapes and the offset-270 secret SURVIVING excision; committed gate 240 passed / 0 failed / 3 documented skips; SL1 flipped verified; F5 caveat lifted) |
| FU-2 (suite) | SWEEP-W2-TESTING | flake class (test infra) | `%TEMP%/glyphpdf-candidates` is process-shared — candidate scanners observe debris from concurrent suites (demonstrated live: TestSignatureRealCrypto delta-fail, TestEncryptedPackageSafeWrite 6!=5, 2 unpatched full runs) | ae636a5a: `RESOURCE_LOCK GlyphpdfCandidates` on 15 writer/scanner suites (+TestMeasureCore env); incident note: first patch-script application corrupted 4 property blocks, repaired and documented | verified by run: patched full `ctest -j 2` **171/171, 0 failures** (SWEEP-W2-TESTING §3.1) |
| TestLaneScheduler (suite) | SWEEP-W2-TESTING | genuinely-flaky test | `elapsed < 1000 ms` bound straddles actual serial-time behavior on this host (3 pass / 5 fail IDLE; failures 1000–1025 ms); NOT fixed in-sweep | proposal recorded (structural overlap assertion or higher bound) — cleanup-lane residual | open (classification + proposal, SWEEP-W2-TESTING §2.1/§2.3) |
| San-UAF ("E-2" in the sanitize-crash lane's own table) | SOAK-VERDICT §4.5 (48 h soak, passes 4 and 31) | CRASH-class (silent UAF) | `sanitizeDocumentContents` RemoveKey("Info"/"Outlines") orphaned objects that PdfDocument caches; the next Save's CollectGarbage freed them under live wrappers → 0xc0000005 in `AssertMutable` on the following metadata stamp; heap-state dependent (the soak's 2/~10 intermittency) | ed04426e (`feat/sanitize-assert-mutable`): scrub IN PLACE (keys stay, dictionaries cleared, catalog-alias guard); NEW `TestSanitizeTrailerUaf` pin — fail-before 10/10 (8x soak-signature SegFault + 2x deterministic assertion), pass-after 13/13, scoped-revert NC 5/5 fail; contract pins moved key-absence → data-absence (documented as NOT a weakening); family green (TestSanitization x5, TrailerUaf, CompressStrip, RedactSanitizeBundle, RedactTransaction 38/38, ExcisionCorruption, EngineSave) | implemented-awaiting-review (ledger §sanitize-crash lane); TestSanitization x3 clean pre-soak at the re-soak tip (RESOAK §2) |
| ri-fix (W2c residual) | SWEEP-W2C §Observation | precision-only (false ALARM) | `runIntersects`' 3xfs ascender headroom over-attributed a neighbor line inside the margin — the proof FALSE-ALARMed a geometrically correct redaction (probe-w2c-loose.txt); rotation-independent; honest-failure direction unaffected | 87c4acbc / e620757b (`feat/runintersects-precision`): attribution band = the run's REAL glyph extent (font ascender..descender from PDFium char boxes; degenerate fallback ±1em); TestRedactionProof 23/23 with 2 new pins; fail-before + NC captured; 128 passed / 0 new failures across the family | implemented-awaiting-review (ledger ri-fix row; lane active). The lane also DISCLOSED a pre-existing failure — R14ProbeRedactSpace annotation-only attribution on offset+rotate — failing identically on the untouched base; owner triage requested (the L7/F1 family) |

### 2.5 W3 emergence findings (E-1..E-6) — source: SWEEP-W3-EMERGENCE-2026-09-20.md; fixes + ledger: `feat/emergence-fixes` (rows EM-1..EM-6)

| ID | Source | Sev (rank) | One-line description | Fix commit + evidence | Verification status |
|---|---|---|---|---|---|
| E-1 | emergence (matrix item 6) | rank 1 | Expired/read-only document writable via Form Builder → field-properties Apply; /K keystroke scripts also execute on read-only docs (EditPolicy asymmetry) | 645f4994: EditPolicy::mutationBlocked at panel Apply + keystroke entries + EditFormFieldCommand persistence boundary; pins 12P/1F → 13P/0F and 8P/1F → 9P/0F; NC revert → both FAIL | implemented-awaiting-review (EM-1) |
| E-2 | emergence (matrix item 1b) | rank 2 | Machine policy blocks the OCR download silently — batch reports wrong-language OCR or a non-searchable "_ocr.pdf" as success; policy whyNot reaches no user surface | 3325ea19: honor `OcrEngine::initialize` failure in the batch worker — honest policy-naming whyNot | implemented-awaiting-review (EM-2) |
| E-3 | emergence (matrix item 2d) | rank 3 (display-only) | Signature badges paint at the wrong position on rot-90/270 pages (rotation-imperfect `signatureFieldAnchors` flip feeding SignaturesPanel) | ee2cf661: badge anchors read through the page-space law; closes the W2B-1-era consumer question (consumers are badges only) | implemented-awaiting-review (EM-3) |
| E-4 | emergence (matrix item 2c) | rank 4 (narrow window) | Cross-version replay trap — a pre-fix-created unsigned field is filled post-fix with a silently corrupted /Rect (mutation gate cannot see it; hash matches) | cee2777c: fill validates the bound field's stored /Rect against the request's anchor | implemented-awaiting-review (EM-4) |
| E-5 | emergence (matrix item 5) | rank 5 (honesty) | TSA refusal under a policy-managed empty tsaUrl directs the user to a Preferences setting they cannot change and never names the policy | 9463f6c0: refusal names machine policy under policy-managed empty tsaUrl | implemented-awaiting-review (EM-5) |
| E-6 | emergence (matrix item 7) | rank 6 (narrow interleaving) | Two-instance stale-memory commit can silently drop the other instance's signature (no destination-identity precondition at the shared SafeSave boundary) | c1552c26: captureDestinationIdentity + re-hash before the atomic rename, honest refusal; wired into FormManager, SignatureManager (sign + addDocTimeStamp), SignatureFieldCreator, SigningRequestRunner; 21-site caller audit; owning battery 13 suites 219P/0F/6 env skips; NC neutered-check → silent overwrite repro | implemented-awaiting-review (EM-6) |

SAFE halves of the composition matrix (pinned, not defects): preset x AI/OCR policy
(schema-closed, §1a), mid-batch policy change (load-once snapshot, §1c), stale
`preparedSha256` re-open UX (designed re-confirm loop, §2a), post-fix lazy placement
(rotate-270 law pins, §2b), OCR skip x sanitize lifetimes (§3), a11y scan x printable
summary (planes never meet, §4), sanitize stripping applied /Alt fixes (designed;
disclosure suggested, §4-adjacent), OCSP not policy-manageable (deliberate bounded
allowlist, §5). Probe evidence: 9/9 suites passed at the emergence tip
(SWEEP-W3-EMERGENCE §8.1).

### 2.6 Residuals list (each with owner + disposition)

| # | Residual | Owner | Disposition |
|---|---|---|---|
| 1 | **W1-05/F1 enforcement residual** — a squatter CAN still enforce a planted machine policy; the fix is disclosure-only by design | policy/R26 owner | structural close (admin-tier ACL check or signed/hashed policy) is a documented design item, not scheduled in-sweep (SWEEP-W2B §Residuals 3) |
| 2 | **F2 embed-branch scoping** — label seam verified with real TS_RESP ground truth; in-CMS embed branch evidenced by committed probe + NC + source only (no live hostile-https TSA harness) | signature/security owner | accepted scoping, carried unchanged through W2C (SWEEP-W2B §Residuals 2; SWEEP-W2C §Residuals 4) |
| 3 | **L7 / R14 FINDING F1** — annotation/form attribution partial on /Rotate pages (false PASS composition demonstrated on the fixed build); ledger keeps L7 partial; ri-fix lane's base-run disclosed the related probe failure (annotation-only attribution on offset+rotate) failing identically pre/post ri-fix | redaction-proof owner | repair direction on record since R14 (attribute from raw /Rect; add a rotated L7 repro); owner triage requested (INDEPENDENT-REVIEW §F1; ledger L7 row; ri-fix ledger row) |
| 4 | **SignatureManager::signatureFieldAnchors** — pre-existing rotation-imperfect anchor read on /Rotate 90/270 (owner-owned, untouched by the W2B-1 fix); consumer side (badges) fixed by E-3 | signature owner | re-audit request stands (SWEEP-W2C §Residuals 1; ledger W2B-1 rows) |
| 5 | **Sweep-legacy residuals** — AP-stream BBox aspect on /Rotate pages (position correct; /Matrix rotation deferred); extractLinks link-rect reader and T2-2 Find&Replace replacement writer still height-only-flip (recipe = SL1's mapping); file-level PdfPageOps direct-write has no SafeSave candidate (mitigated by explicit Save-As dialogs); exportToImage out-of-range "page" option renders all pages (API/batch only); CSV valid UTF-8 without BOM (Excel mojibake = consumer note); negative /Rotate modulo (spec-legal) | owner lanes (links/redaction/writer owners) | recorded in ledger §sweep-legacy + SWEEP-LEGACY-2026-09-20.md |
| 6 | **Emergence fixes EM-1..EM-6 + San-UAF + ri-fix pending verification review** | coordinator / verification lane | ADDENDUM SLOT A4 (§7) |
| 7 | **W2C new residuals** — chained annotation re-embed appends without /NM dedup (hygiene question: can the save flow re-embed the same id twice on one lineage?); TestRedactionProof `proofFailsOnXmpSurvivor` pre-existing flake (load-sensitive; passed in W2C's runs) | annotation owner / proof owner | recorded (SWEEP-W2C §Residuals 2–3, 5); runIntersects precision itself RESOLVED by ri-fix |
| 8 | **Adversary open hypotheses** — W1-H1 two-instance signing race (needs a two-process harness; P2 multi-session concern — note E-6 now closes the commit half at the SafeSave boundary, implemented-awaiting-review); W1-H2 consent-before-reload gap (needs GUI-flow harness; the gate DOES detect and require consent); W1-H3 preset-chain throw-leak (mechanism confirmed by reading; consequence = temp pollution only) | future lanes | recorded with exact missing pieces (SWEEP-W1-ADVERSARY §HYPOTHESES) |
| 9 | **UX batch-B flows 4–7 unrun** (disk guard) + **F2b-D1** first-run preset-save breaker (one-line `mkpath` fix proposed, not yet landed) + F2a-F1 merge-output naming + offscreen drag native-confirmation residual | ux-resume lane (flows), presets lane (F2b-D1) | ADDENDUM SLOT A1 (§7); SWEEP-W3-UX §Friction inventory |
| 10 | **Devops D1** models bootstrap gap (top gap); D2 models missing in 5/9 worktrees (restored in pdf-r18 only; pdf-clean still lacks them); C1 fuzz-workflow provisioning likely cannot pass; C2/C3/D3/D4/D5/D6 stale comments + dead scripts batched for cleanup; B1 release-box VCRT; §3.5 NOT-VERIFIED register (second-machine install, VCRT adequacy on a clean box, veraPDF-bundled variant, first-run no-network) | release-hardening / cleanup pass / fuzz lane owner | dispositions per SWEEP-W3-DEVOPS §7.1; D1 recommended "early in W4 or the release-hardening pass" |
| 11 | **W2 cleanup residuals** — TestLaneScheduler bound redesign; TestSignatureValidation merge-or-retire (3 unique pins to port first; tautological QVERIFY2 at its line 132); R14ProbeBatchSkip register-or-delete (archaeologist RECOMMENDS register); TestEngineSave real-store QSettings removal (H2); soak §10.3 candidates-dir hardening beyond RESOURCE_LOCK | cleanup pass | recorded (SWEEP-W2-TESTING §Residuals; SWEEP-W3-ARCHAEOLOGIST §8; SOAK-VERDICT §10.3) |
| 12 | **Archaeologist dispositions** — 0 PROVEN-SAFE deletions (triple bar unmet); AnnotationToolBar.{cpp,h} KEEP-ANYWAY (revival marker e5a5f01 — parity lane to close the question explicitly); LibSecretStore platform-gated load-bearing (never propose on Windows-build evidence); TestImageDedup 09-09 ledger row superseded (alive, registered) | parity lane (revival question); cleanup pass (2 NEEDS-REVIEW rows) | recorded (SWEEP-W3-ARCHAEOLOGIST §1–§3, §6–§8) |
| 13 | **Research open roadmap items** (corpus reconciled, not silently upgraded) — form-JS P3 (OpenAction/doc-level + consent), send-for-signing P2–P4, presets P2/P3, T2-4 tag-tree/auto-tag/PDF-UA, T2-5 batch split/password-strip, N5 reverse wire-up, N4 offline-degraded-validation wording, N38 GPO/ADMX/MSI/license tail, Tier-3 pool; matrix CSV mechanical defects (line 302 parse, duplicate `certify` id, stale measure rows 52–54) | program backlog / matrix owner | recorded (SWEEP-W3-RESEARCH §2, §4.2, §6) |
| 14 | **Perf residuals R1–R8** — warm/cold start split, office-PDF corpus, interactive + cancel latency, frame pacing, GPU paths, multi-monitor; perf F5 observation (16x render succeeded where a 64 Mpx guard was expected — view-path scoping question); quiet-run redact-apply median variance (32→77 ms, min matches floor) flagged for re-probe before quoting either number | future perf lane | recorded (PERF-BASELINE §5–§6, §7.1 notes 2/5) |
| 15 | **Soak/consolidation residuals** — first-soak candidate `2f755244` has ~4 h endurance evidence only; re-soak (candidate `b17106a`, exe SHA-256 `509da2c8…`) verdict pending; consolidation: `origin/feat/parity-glm` 7 commits behind local tip (push step), main-merge resolution classes R2/R3/R4 need release-owner sign-off, local-only branch set out of scope | soak reading session / consolidation execution lane | ADDENDUM SLOT A3; CONSOLIDATION-PLAN §5, §9 |

## 3. Verification summary

### 3.1 What W2 / W2b / W2c verified

**Method (binding protocol, identical in all three passes)** — implementer claim →
falsifiable contract → committed suite green on the tip under test → the verifier's OWN
probe through an independent seam/read path (PoDoFo raw dictionaries, PDFium
FPDFAnnot/FPDFText, OpenSSL d2i_TS_RESP, raw file bytes, own hostile fixtures with
hand-computed literals — never just the committed suite's assertions) → negative control
(scoped revert of the fix to its NAMED base inside the worktree; the probe must FAIL;
restore; re-verify; capture) → verdict (SWEEP-W2B header; SWEEP-W2C header).

- **SWEEP-W2B** (tip `2d29a16`, probes committed at 9afc839): verified the **15 W1 fixes**
  (W1-01..05, F1–F6, FZ-1..4) + legacy SL2/SL3 = 17 rows **verified**, SL1 **partial**
  (FINDING W2B-1). Evidence: committed sweep suites 59 passed / 0 failed / 3 documented
  skips; owner suites 99 passed / 0 failed / 1 skip; tip-state probes Naming 9/9, Signing
  15/15, SummaryPolicy 10/10, A11y 7/7, OfficeSave 6/6, LegacySpace 5/8 (the three
  rot-270 failures ARE the finding); **negative controls: 16 runs + 2 textual pins — every
  reverted base FAILS its probe** (SWEEP-W2B §Suites + §Negative-control inventory).
- **SWEEP-W2C** (tip `ec9f16f`, probe W2CProbeRotate270): re-verified the W2B-1
  re-submission — independent re-derivation of the law by hand before touching code;
  5 functional slots through the PRODUCTION edit paths (move/resize via
  `embedAnnotations`, `FormManager::updateFieldRect`, F5 containment, SEP13 excision +
  proof honesty on rot270+offset, verbatim signature /Rect on field dict + widget dict +
  PDFium + raw bytes) on all six page shapes; committed gate **240 passed / 0 failed /
  3 documented skips**; NC (scoped revert of 879c171's PageSpaceTransform.h only) → probe
  2P/5F with the finding's transposed shapes and the offset-270 secret surviving excision
  (the false-success class made visible); restore → byte-identical verdicts 7/7, 7/7, 9/9,
  8/8. Verdicts: W2B-1 root fix + both consumer fixes **verified**; **SL1 flipped to
  verified**; F5's rotated-page caveat **lifted**; SEP13 L5/L8 on /Rotate 270+offset
  **verified** (SWEEP-W2C §Committed-suite gate, §Negative control, §Verdicts).
- **R14 independent review** (2026-09-14, candidate `39aaca3`): the same protocol for the
  SEP13/quick-lane/R18f/R19 packages — **21 rows flipped verified, 1 partial** (L7/F1);
  full gate 152/154 with both exceptions explained (INDEPENDENT-REVIEW §Verdict table).

### 3.2 What remains implemented-awaiting-review

- EM-1..EM-6 (emergence fixes, `feat/emergence-fixes` — ledger rows EM-1..EM-6).
- The sanitize UAF fix (ledger §sanitize-crash lane) and the ri-fix
  (`feat/runintersects-precision`).
- SEP13 L4 (static X509 RAII — R14 explicitly did not re-verify), L12 (perf fix), RES-1/RES-2
  (sep13-residual lane, post-R14), and the unmerged modularity-moves rows AM1/AM2 (B1 seam +
  dead-include sweep, `feat/modularity-moves`).
- Verification of these rows is the coordinator's ADDENDUM SLOT A4 (§7).

### 3.3 Discipline statement (as evidenced by the wave documents)

- **No test was weakened.** The one contract change in the sweep — sanitize pins moved from
  key-absence to data-absence — strengthens the assertion (the keys may legitimately remain
  as empty containers; the user data must be gone) and is documented as such with the G-04
  vector and Compress-strip pins named (ledger §sanitize-crash lane, "Contract pins updated
  WITHOUT weakening"). Where a harness artifact blocked an honest pin (L13's CR-affected
  line compare), the correction is documented as EOL-agnostic with the assertion strength
  preserved (ledger §sep13-fixes L13 row).
- **Probes were independent.** Every verification pass built its own fixtures and read
  through different seams than the committed suites (raw dictionary walks vs PoDoFo APIs,
  PDFium as a second engine, raw file bytes, hand-computed literals distinct from every
  committed suite's values) (SWEEP-W2B header; SWEEP-W2C §My independent probe).
- **Negative controls were captured.** W2B: 16 NC runs + 2 textual pins, every reverted
  base fails its probe, all reverts restored with restore-check rebuilds reproduced exactly
  (SWEEP-W2B §Negative-control inventory; `git status` on src/ clean). W2C: the NC
  re-transposed every consumer path and broke the offset-270 excision loudly; restore
  re-verified byte-identical (SWEEP-W2C §Negative control). Lane-level NCs for every fix
  are recorded per-row in the ledger.
- **Honest scoping is disclosed, not hidden** — F2's embed-branch scoping, W1-05's
  disclosure-only posture, W1-03's QSKIP-by-design committed slot (probe carries the strong
  assertion), TestOfficeImport's real-soffice skip, and the NOT-VERIFIED registers
  (SWEEP-W2B §Residuals; SWEEP-W3-DEVOPS §3.5, §7.2).

## 4. Quality architecture (sources: SWEEP-W3-ARCHITECT-2026-09-20.md, SWEEP-W3-ARCHAEOLOGIST-2026-09-20.md, SWEEP-W2-TESTING-2026-09-20.md, `feat/modularity-moves`)

### 4.1 Structure: zero cycles, the layer law, the 13 upward edges

- **Zero include cycles.** Tarjan SCC over the full file-level include graph (359 files
  scanned, 204 headers reachable, derived from `compile_commands.json` — machine-checked,
  not grep-guessed): no SCC larger than 1 file. The tree is a DAG (SWEEP-W3-ARCHITECT §0).
- **The layer law** holds in the healthy bulk: app → src-root(GpMainWindow) →
  shell/controllers → shell → modes → ui → commands → core → engines, with the sanctioned
  core→engines convention (9 edges, all healthy). Healthy-bulk edge counts recorded
  (ui→core 30, controllers→ui 32, engines→core/interfaces 17, ...) (ARCHITECT §1).
- **13 upward edges = 10 live (V1–V10) + 3 suspected-dead**, all named with evidence and
  severity (ARCHITECT §2):
  - V1 core→shell/controllers (`SigningRequestRunner.cpp:7` → SecurityController.h for the
    pure static `attainedLevelLabel`) — "the B1 finding, confirmed at this tip";
  - V2 core→ui (dead include), V3 commands→ui (undo command holding a widget pointer),
    V4–V8 five ui/shell/modes→src-root reach-ups (`qobject_cast<gp::MainWindow*>` — 8 cast
    sites in 6 files across V4–V8 for exactly four capabilities), V9 ui→modes
    (OcrScanCanvas on the review-session types), V10 modes→shell (EditPolicy placement);
  - the 3 "dead includes" of §2.2 were re-verified by the modularity-moves lane: **only
    `Sidebar.cpp:3 → GpMainWindow.h` was actually dead; the other two marks were CORRECTED
    as live** (commit 791115bb — the audit's honest erratum, recorded in its ledger row AM2).
- **Seam inventory:** 7 of 10 `I*` interfaces cleanly consumed; the weak spots are
  ISignatureManager (6 non-wiring concrete consumers; 7 cross-layer `dynamic_cast`s
  tree-wide including ConversionManager), IConversionEngine (`locateSoffice` through the
  concrete), IPdfEditorEngine (3 concrete consumers), and the missing MainWindow service
  seam — "the god-file generator" (ARCHITECT §2.3, §3, §6).

### 4.2 The extraction roadmap and its execution state

Sequenced steps 1–9, each with validated line windows, target module, exposed interface,
risk class and guarding pin (ARCHITECT §4). Execution state at report time:

- **Step 1 (B1 seam) — DONE on `feat/modularity-moves`, not yet merged to mainline:**
  `attainedLevelLabel` moved verbatim to `core/SigningLabels`, SecurityController delegates
  (commit 5c02b01d, ledger row AM1, implemented-awaiting-review) — kills the only live
  core→shell upward edge. **ADDENDUM SLOT A2** records merge status and any further moves.
- **Step 2 (dead-include sweep) — DONE on the same branch** (791115bb), with the audit
  correction above (only the Sidebar edge was real).
- Steps 3–9 (BatchPresetPanel, HotFolderController — **needs a TestHotFolder
  characterization pin FIRST**, OpenRouteCoordinator as the S4 seam pilot,
  UpdatePromptController, DocumentRecovery, ctor split, WelcomeTaskRouter) — sequenced, not
  executed. "BatchPresetRun do first" was VALIDATED by dependency evidence with one
  placement correction: `engines/BatchPresetRunner`, not `modes/` (ARCHITECT §4).
- Top-5 moves by prevention-per-effort: B1 seam (S), dead-include sweep (S),
  BatchPresetRunner extraction (S–M), MainWindow service seam S4 (M pilot + L migration),
  complete the signing seam (M) (ARCHITECT §6).

### 4.3 Suite health (source: SWEEP-W2-TESTING unless noted)

- Inventory machine-checked at the W2 tip: 170 `add_test` = 170 `ctest -N` (1:1); 172
  executables / 171 test sources; the built-but-unregistered trio is intentional (app,
  R14ProbeRedactSpace, perf tool); TestUiAccessibility200 is an intentional env-variant
  alias (§1).
- **FU-2 fixed:** the shared `%TEMP%/glyphpdf-candidates` interference class was
  live-captured unpatched (two full `-j 2` runs failing in TestSignatureRealCrypto /
  TestEncryptedPackageSafeWrite), fixed by `RESOURCE_LOCK GlyphpdfCandidates` on 15 suites
  (ae636a5a), and the patched full run is **171/171, 0 failures** (§2.2, §3.1).
- **Flake classification (all 10 recorded flakes, 30 standalone + 10 instrumented + 3
  full in-suite runs):** TestLaneScheduler = **genuinely-flaky timing guard** (test bug:
  3 pass / 5 fail idle, failures at 1000–1025 ms vs the `<1000 ms` bound — serial-time
  straddle; fix proposal recorded, deliberately not patched in-sweep); TestOllamaProvider,
  TestBatchMode, TestReadOnlyGate, TestBatchOpsCoverage, TestCommandBinding (one
  unreproducible first-run rc=2), TestWelcomeRoutes, TestEngineSave x TestRedactTransaction
  (FU-2 class, fixed), TestSep13LeadComparePerf (load-robust by design) — all clean under
  protocol (§2.1–§2.3). The re-soak's pre-soak gate independently reproduced the picture:
  three 170/171 full runs, a different one-off flake each, no deterministic red, with
  TestLaneScheduler pre-declared as machine-load sensitivity (RESOAK §2).
- **Coverage gaps closed in-sweep:** 13 feature-matrix rows had no test reference (shell
  navigation); the lane added `TestModeStripPins` (11/11: pill switching, toggle-ai,
  task-chooser composition, exclusivity) (§6).
- **Gate counts across the sweep,** as recorded per document: W2 patched gate 171/171;
  re-soak baseline 171 tests with three 170/171 one-off runs; SWEEP-W2C committed gate
  240 passed / 0 failed / 3 documented skips; emergence-fix owning battery 13 suites
  219P/0F/6 env skips; archaeologist counts 173 add_executable / 171 add_test at its tip.
  Counts are stated per-source and not averaged — registration is volatile and the ledger
  forbids hardcoding counts in prose.
- Hygiene findings H1–H4 recorded (H2: TestEngineSave sweeps a key on the REAL user
  QSettings store — flagged, cleanup-phase proposal; H4: the tautological QVERIFY2 in
  TestSignatureValidation) (§7).

### 4.4 Dead weight (source: SWEEP-W3-ARCHAEOLOGIST)

0 PROVEN-SAFE candidates (the triple bar — unreachable AND string-table clean AND
superseded/never-loaded — was met by nothing); 2 NEEDS-REVIEW (R14ProbeBatchSkip —
register recommended; TestSignatureValidation — merge-or-retire after porting 3 pins);
6 KEEP-ANYWAY (AnnotationToolBar pair with its documented revival marker; LibSecretStore
pair, platform-gated load-bearing; two history docs). A 10-item load-bearing watch list
prevents false positives (§1–§8). Nothing was deleted in-sweep.

## 5. Performance + operations (sources: PERF-BASELINE-2026-09-20.md, SWEEP-W3-DEVOPS-2026-09-20.md)

### 5.1 The measured baseline (quiet-gated; reference machine i5-12400F, Win 11, Release+LTO build at `2d29a16`)

The final baseline is the QUIET run — gate: zero build processes sustained 10 consecutive
minutes before the suite; per-block load context sampled and disclosed (PERF-BASELINE §7,
§7.2). All values median / p95 unless noted:

| Metric | Quiet run | Loaded-run median | Proposed target | Verdict |
|---|---|---|---|---|
| Cold start, whole-process external wall | **150 / 174 ms** (one 407 ms outlier disclosed) | 169 | P95 < 2 s | **~11x headroom** |
| Startup: main() → window shown | 11 / 13 ms | 13 | — | ctor-dominated (F1: createContext ~0 ms) |
| Open 20-page fixture, engine first page | 6 / 8 ms (real MainWindow path 12 / 21 ms) | 6 | P95 < 1 s to first useful page | **~50x headroom** (synthetic text fixture; complex office docs NOT in corpus, R2) |
| Paginate 40-page medium @2x | 279 / 292 ms | 302 | — | — |
| Paginate 150-page large @2x | 1298 ms (linear ~9 ms/page; no superlinear blow-up, F3) | 1395 | — | bulk op, not an interaction |
| 50-page compare @150 DPI (heaviest everyday op) | **807 / 831 ms** | 926 | — | ~461 MiB peak WS (~9 MiB/page) — the per-job memory budget still needs defining (F2) |
| Redact-apply (3 marks + save) | 77 / 131 ms — **unexplained within-block variance: median tripled vs loaded (32→77) while min (28) matches the loaded floor (29); re-probe before quoting either number** | 32 | ≤100 ms interactive-response target NOT directly measured offscreen (R3) | PARTIAL / flagged |
| Sign local P12 (no TSA) | 33 / 40 ms (first-call jitter = crypto provider init, F4) | 74 | — | — |
| Save roundtrip / batch-50 preset compress | 27 / 30 ms · 275 ms | 69 · 524 | — | — |
| Peak working set: 114.7 MB doc open · 50-page compare | **178.9 MiB · 461.4 MiB** | 179 · 461 | no numeric target existed | first calibrated datapoints for the budget definition (F2) |
| Per-scale page render (20p fixture) | 0.5x=2 … 16x=84 ms — identical loaded vs quiet | — | — | raster work, load-insensitive at these sizes |

Findings F1–F6 and residuals R1–R8 recorded in PERF-BASELINE §5–§6 (startup is
ctor-dominated; compare is the heaviest op with a visible per-page footprint; 16x render at
~128 Mpx succeeded where a 64 Mpx guard was expected — view-path scoping question; the D:
100%-full incident moved measurement outputs to `C:\perftmp`). NOT measured (disclosed):
interactive click/typing latency, cancel ack/stop, frame pacing, GPU paths, warm/cold
split, office corpus.

### 5.2 Deploy validation and dependency closure (devops, tip `b17106a`)

- **Release configure + INF02 validator + deploy: all PASS.** Release+LTO configure exit 0
  with all four shipped features TRUE (vendored podofo 1.1.0 gate + AR-11 D5 ROVER gate
  held); `validate-release-build.ps1` exit 0; `deploy.ps1` exit 0 — windeployqt, engine
  binaries, MinGW closure (4 rounds), VCRT staging (System32 fallback, warning disclosed),
  ONNX models, tessdata, licenses/veraPDF-offer compliance gates; deploy tree 407.0 MB,
  24-entry critical-file validation clean. Honest disclosure: the build task was killed at
  step [844/945] — the main exe was complete (15,702,031 bytes, stripped); the ~100
  remaining steps were test executables only, and a fresh full `ctest` from `build-devops`
  was NOT run (the 171/171 evidence is W2's at the same code commit) (DEVOPS §1.1–§1.4,
  §7.2).
- **Dependency closure: PASS (independent objdump walk, not trusting deploy's own
  closure).** `GlyphPDF.exe`: 39 imports → 24 resolve inside deploy/, 15 Windows system
  DLLs, zero unresolved third-party; all 90 staged DLLs' 150 unique imports resolve inside
  deploy/ or are Windows inbox. Verdict: the deploy tree is self-contained on Windows 10
  19041+ given the staged VCRT DLLs (DEVOPS §3.4).
- **Runtime hashes (SBOM-lite):** `deploy/GlyphPDF.exe` SHA-256
  `66d0f790f167cf90cf10f5e07601e82e6548025a88ede55e1ccf1d0c845d5000`; `pdfium.dll` = the
  G18 pin (`a487e1d2…e6905`); staged `libpodofo.dll` byte-identical to the vendored 1.1.0
  tree (the MSYS2 0.10.4 never overwrote it); onnxruntime 1.17.3, tesseract 5.5, qpdf,
  quickjs-ng, OpenSSL 3 hashes recorded (DEVOPS §2.1, §6).
- **The models-bootstrap gap (D1 — the top devops finding):**
  `scripts/bootstrap-vendor-deps.sh` covers podofo/pdfium/ort/quickjs but NOT `models/`
  (gitignored, PROVENANCE.md carries per-file URLs + SHA-256 pins); a fresh clone builds
  green but `deploy.ps1` HARD-FAILS at step [6/8]. Related D2: models were missing in 5 of
  9 worktrees (disk-full collateral); restored in pdf-r18 with all five SHA-256s matching
  their pins; **pdf-clean (integration) still lacks them** and cannot run deploy until
  restored. Fix candidate (extend the bootstrap script) deliberately not coded in-sweep
  (DEVOPS §2.2, §7.1, §7.3).
- **Other devops findings:** C1 — `glyphpdf-fuzz.yml` redaction-oracles likely cannot pass
  (`windows-latest` vs the documented windows-2022 pin; cmake+ninja missing from the
  setup-msys2 install list while step 1 runs cmake); C2/C3 stale CI comments; D3/D4 dead
  packaging scripts (`check-deps.bat` points at a deleted script; `deploy-msys2.bat`
  unreferenced); D5 deploy.ps1 claims a WiX VC++ MergeModule that does not exist (VCRT
  ships only as staged loose DLLs — do not act on that comment); D6 cosmetic version
  header; B1 — no VC++ Redist payload on this host, so deploys are dev-grade (System32
  fallback), release-grade wants the Redistributable installed; LTO exe link runs 76 LTRANS
  jobs serially (~30 min) at `-j 2` (DEVOPS §3.2–§3.3, §4, §7.1).
- **CI truthfulness (static):** ci.yml / release.yml / license-guard.yml coherent and
  honest (models QSKIP disclosed; INF05 define gate; poppler-negative job correct);
  no workflow references a deleted script; runner pinning documented (DEVOPS §4).
- **Disk runbook highlights** (everything lives on D: — a full D: stops every lane at
  once): one `build-<lane>` tree per lane, budget ~2 GB dev / ~4 GB Release+LTO, ≥15 GB
  free before configuring; the reclaim rule (build trees only, in documented order; never
  vendor trees, never models, never another lane's build without pinging); junction hygiene
  (`rmdir` the junction, not `rmdir /s` through it); `models/` is the least-redundant,
  most-expensive asset — do NOT reclaim until D1 is fixed; the 17.0 GB `build-r18-noeng`
  is the standing reclaim candidate #1, owner sign-off required (DEVOPS §5.1–§5.3).

## 6. Soak + release evidence (sources: SOAK-VERDICT-2026-09-20.md, RESOAK-2026-09-20.md, DEVOPS §6)

### 6.1 First soak (48 h attempt, candidate `2f755244`)

- **Verdict: FAIL — the soak did not complete.** The detached loop was killed by the host
  at 2026-09-15 05:45:03+03 after **4 h 00 m 21 s** (8.4 % of the window) when Windows
  Update initiated a planned OS restart (event-log + STATUS_CONTROL_C_EXIT evidence, §8);
  58 passes ran; the completion line is absent — by the committed protocol this alone is a
  failure (SOAK-VERDICT §1).
- **Candidate-side picture inside the window (NOT a 48 h exoneration):** no
  candidate-attributable failure recurrence — 35 passes exit 0 / 22 passes exit 8 / 1
  killed; the 29 failing-test events map to the documented interference/timing-flake
  families (TestReadOnlyGate 9x = 15.5 %, TestBatchMode 7x, TestEngineSave 4x,
  TestCommandBinding 3x, TestEncryptedPackageSafeWrite 2x — shared-candidates-dir and
  temp-roundtrip families); **zero app-crash suspects: 57/57 app cycles ended
  `KILLED-AFTER-60S;taskkill-exitcode=1`, no EXITED-EARLY** — the real Release exe never
  hung or crashed across 58 launches (SOAK-VERDICT §1–§4, §6).
- **The catch: `TestSanitization` SegFault 2x (passes 4 and 31)** — 0xc0000005 in
  `PoDoFo::PdfDataContainer::AssertMutable` during `testSanitizeGeneratesUniqueTrailerID`;
  classified crash-class "flake-source-with-a-real-bug-suspicion", top follow-up
  (§4.5). **Run down and fixed** by the sanitize-crash lane (ed04426e; row San-UAF in §2.4
  above) — the soak's exact frames (Save→SetModifyDate→SetModDate→AddKey→AssertMutable)
  reproduced deterministically by the new pin. SOAK-VERDICT §10 follow-ups 1 (re-soak) and
  3 (candidates-dir hardening beyond RESOURCE_LOCK) remain open.

### 6.2 Re-soak (live at report time — ADDENDUM SLOT A3)

- **Design (reboot-resilient, the §8 kill class can no longer end it):** per-pass
  heartbeat + `GlyphPDFResoak` logon Scheduled Task + relaunch guard (heartbeat stale
  >30 min) + `RESTART DETECTED resuming at pass N` markers + continuing pass numbers +
  48 h window pinned to first start; the resume path was proven live on day one via a
  controlled kill drill (SOAK-VERDICT §12; RESOAK §1).
- **Candidate advanced:** re-soak runs `b17106a` (six major waves after `2f755244`),
  branch `feat/soak-48h-resume` (worktree pdf-keyC, off-limits to other lanes), Release
  rebuild exit 0, **exe SHA-256 `509da2c8fc602411c6c1cfe45f7603ca1ede42f4d77af97058330b985f867853`**
  (16,666,639 bytes); the first verdict's binary analysis (`791749b5…`) remains valid for
  `2f755244` only (SOAK-VERDICT §12).
- **Pre-soak gate recorded honestly (not clean-green):** three full serial runs at the new
  tip = 170/171 each, a different one-off flake each (`readExpiryDate` roundtrip family
  x2, TestLaneScheduler timing x1); no deterministic red → proceed; TestLaneScheduler's
  ~1 % margin pre-declared as machine-load sensitivity, not a soak finding;
  `TestSanitization` x3 clean pre-check at the new tip (RESOAK §2).
- **Start verification:** first start verified; PASS 1 green end-to-end; resume drill;
  PASS 3 under the loop PID (RESOAK §3).
- **Verdict protocol (for the reading session at/after 2026-09-22T20:49:07+03):** read
  `D:\resoak-48h.log`; completion = final `=== SOAK END after N passes … ===` + the
  `.done` file; every `RESTART DETECTED` marker is a SURVIVED restart, not a termination;
  `EXITED-EARLY` app cycles remain crash suspects; failing tests classify per
  SOAK-VERDICT §4 plus RESOAK §2 pre-declarations; `tools/soak_verdict_summary.py` is
  marker-compatible with the new log (RESOAK §4).
- **Honest scope carried over:** offscreen-only, no installer, no fuzz campaign, hard-kill
  app cycles, single machine; the OS-restart logon path was proven by controlled kill +
  manual guard run, not a real reboot; the endurance conclusion does not transfer from
  `2f755244` — this soak re-establishes it for `b17106a` (RESOAK §5).

### 6.3 Release-evidence hashes (consolidated)

| Artifact | SHA-256 (prefix) | Source |
|---|---|---|
| First-soak exe (candidate `2f755244`) | `791749b5…1244c` (re-verified 2026-09-20) | SOAK-VERDICT §11 |
| Re-soak exe (candidate `b17106a`) | `509da2c8…853` | SOAK-VERDICT §12 |
| Devops deploy exe (b17106a Release+LTO, stripped) | `66d0f790…5000` | DEVOPS §6 |
| Perf harness exe (2d29a16 Release+LTO) | `2d3002bd…b38f` | PERF-BASELINE §1 |
| pdfium.dll | `a487e1d2…e6905` (= G18 pin) | DEVOPS §2.1/§6 |
| libpodofo.dll (vendored 1.1.0) | `b25f21f9…5087` | DEVOPS §6 |
| onnxruntime.dll (1.17.3) | `55ea8474…267` | DEVOPS §6 |

<!-- APPEND -->



