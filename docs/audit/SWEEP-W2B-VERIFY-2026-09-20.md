# SWEEP-W2B VERIFY — guarantee-verification-engine, second W2 pass (2026-09-20)

- **Role**: guarantee-verification-engine, W2 wave -b variant of the END-PHASE
  ponytail sweep. Scope: the 15 W1-remediated sweep-fix rows
  (W1-01..05 adversary, F1-F6 security, FZ-1..4 fuzz) + the legacy sweep's
  3 fixes (SL1 page-space law, SL2 conversion SafeSave, SL3 forms suggestion
  placement) — complementary to the -a pass (`.context/sweep-w2-verify-wip.md`).
- **Tip under test**: `feat/sweep-w2-verify-b` = `feat/parity-glm` @ **2d29a16**
  (full W1 remediation + legacy fixes; 22588a1 accessibility/R24 base is its ancestor).
- **Build**: fresh `build-w2b` (Debug, Ninja, UCRT64, `-j 2`), vendored podofo 1.1.0
  verified at configure (`podofo_DIR=third_party/podofo/install`), offscreen, serial.
- **Method per row (binding protocol)**: implementer claim → falsifiable contract →
  committed suite green on the tip → MY OWN probe through an independent seam/read
  path (PoDoFo raw dictionaries, PDFium FPDFAnnot/FPDFText, OpenSSL d2i, qpdf where
  applicable, on SAVED artifacts — never just the committed suite's assertions) →
  negative control (scoped revert of the fix to its NAMED base inside this
  worktree, probe must FAIL, restore, re-verify green, capture) → verdict.
- **Probes** (committed on this branch, registered at the CMakeLists tail; run
  manually, not ctest members): `tests/W2BProbe{Naming,Signing,SummaryPolicy,
  A11y,LegacySpace,OfficeSave}.cpp`. All hostile inputs/fixtures are written
  independently of the fix lanes' repros; expected literals hand-computed.
- **Handoff**: `.context/sweep-w2b-wip.md` (NC plan with exact bases).
- **Evidence**: `.context/sweep-w2b-evidence/` (suite/probe/NC captures, the real
  RFC 3161 TS_RESP fixture `resp.ts` and its generator materials).

## Recovered prior state (start of this pass)

The worktree carried the dead -a pass mid-negative-control: `PageSpaceTransform.h`
was left with its "W2-NC-L5L8 rotation-blind" production hack unrestored and its
probe registrations uncommitted. Both archived verbatim at
`.context/sweep-w2a-orphan-uncommitted.patch`, then the production file RESTORED
to committed state before any verification ran (the hack would have corrupted the
SL1 page-space-law verification). The -a pass's handoff, untracked probes and
build dir were left untouched.

## Verdict table

(filled per milestone)

| Fix | Verdict | Committed suite | My probe + independent read path | Negative control (base → observed) | Tip SHA |

## Suites + totals

(filled per milestone)

## Residuals / partials

(filled per milestone)

## Final state

(git log, ledger flips, handoff pointer — filled at lane end)
