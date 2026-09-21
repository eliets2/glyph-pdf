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

<!-- APPEND -->
