# PGR status table — Phase C-final (2026-09-24)

Authoritative per-ID status after the Phase C integration onto
`review/consolidated-parity` (PR #2). This table supersedes the snapshot
statuses in [`SECURITY-QUALITY-REVIEW-parity-glm.md`](SECURITY-QUALITY-REVIEW-parity-glm.md)
§3 (taken at `1991d9c1`) and §6b.

- **Fix on PR** — the cherry-picked commit carrying the fix (`cherry-pick -x`
  records the source SHA in the trailer). Base of the Phase C picks: `7d4d8d08`.
- **Test** — the suite that pins the fix, with the pass/fail count measured on
  the integrated tree (build-review, Release, offscreen, this machine). Counts
  marked *(lane)* were measured on the preparer branch and are not re-runnable
  on the PR because the fix is not landed here.
- On-line fixes (landed before Phase C, during the parity-glm bring-in) carry
  the report §3/§6b evidence pointer; their commits are twinned in the PR's
  ancestry per [`CONSOLIDATION-LEDGER-2026-09-24.md`](CONSOLIDATION-LEDGER-2026-09-24.md).

## Master table (PGR-01 … PGR-45 + round-2 triage)

| ID | Sev | Title | Status | Fix on PR | Test |
|----|-----|-------|--------|-----------|------|
| PGR-01 | HIGH | CID `/W` OOB heap write + DoS | **Fixed** (on line) | twinned pre-Phase C (report §3: `kMaxCid` cap) | — (report §3) |
| PGR-02 | HIGH | `#else` compile break | **Fixed** (on line) | twinned pre-Phase C | — (report §3) |
| PGR-03 | HIGH | Excel duplicate cell refs → invalid XLSX | **Fixed** (on line) | twinned pre-Phase C (SEP13:3) | — (report §3) |
| PGR-04 | HIGH | Secret AEAD not AAD-bound to entry | **Fixed** (on line) | twinned pre-Phase C (SEP13:5) | TestSecretStore |
| PGR-05 | HIGH | Unbounded response buffering (DoS) | **Fixed** (on line) | twinned pre-Phase C (SEP13:6) | TestOllamaProvider |
| PGR-06 | HIGH | Encrypted-doc rollback drops password → lock-out | **Fixed** (Phase C.1) | `0b06214a` | TestEngineSave 22P/0F |
| PGR-07 | MED | `RedactOperation` accumulating leak | **Fixed** (on line) | twinned pre-Phase C | — (report §3) |
| PGR-08 | MED | Post-sign re-validation fail-open | **Fixed** (on line) ⚠ superseded by PGR-21 fix | twinned pre-Phase C (SEP13:4) | TestSignatureRealCrypto |
| PGR-09 | HIGH | Proof false-PASS on rotated/offset pages | **Fixed** (on line) | twinned pre-Phase C (L8) | TestRedactionProof (`/Rotate 270` pin) |
| PGR-10 | MED-HIGH | Proof false-PASS on unextractable text | **Fixed** — unverifiable entries can no longer certify (Phase C.4) | `862f9d5c` | TestRedactionProof 28P/0F; TestSep13LeadRedactionProof 11P/0F; TestRedactTransaction 42P/0F |
| PGR-11 | LOW | Overlay-label Y ignores MediaBox origin | **Fixed** (on line) | twinned pre-Phase C (L8) | — (report §3) |
| PGR-12 | CRITICAL | Batch merge reports success, no file written | **Fixed** (on line) | twinned pre-Phase C | TestMergeSuccess / TestSep13LeadBatchMerge |
| PGR-13 | CRITICAL-if-real | Proof attribution misses annotation text | **Fixed** (on line) | twinned pre-Phase C (L7) | TestSep13LeadRedactionProof 11P/0F |
| PGR-14 | HIGH | Read-only/edit-policy bypass (page ops) | **Fixed** (on line) | twinned pre-Phase C | — (report §3) |
| PGR-15 | HIGH | Read-only/edit-policy bypass (inline Replace) | **Fixed** (on line) | twinned pre-Phase C | — (report §3) |
| PGR-16 | HIGH | CSV formula injection (export) | **Fixed** (Phase C.3) | `17e759f5` | TestConversionExtraction 18P/0F |
| PGR-17 | HIGH | CSV formula injection (comments export) | **Fixed** (Phase C.3) | `7d6a1d83` | TestCommentsReview 10P/0F |
| PGR-18 | HIGH | Unescaped HTML into OCR overlay | **Fixed** (Phase C.3) | `0ce53c76` | TestOcrReviewLifecycle 34P/0F |
| PGR-19 | HIGH | Invalid calibration leaves stale scale | **Fixed** (Phase C.3) | `e233e58b` | TestMeasureCore 22P/0F |
| PGR-20 | HIGH | Legacy secret-blob DPAPI-binding gap | **Fixed** — forged blobs rejected, legacy `0x02` re-wrapped on read (Phase C.2) | `2d893a21` | TestSecretStore 21P/0F |
| PGR-21 | **CRITICAL** | In-place re-sign deletes the only copy | **Fixed** — candidate transaction + checked atomic commit (Phase C.1) | `5cec76cb` | TestSignatureRealCrypto 26P/0F/1skip |
| PGR-22 | HIGH | Save-path re-seat use-after-free | **Fixed** — local buffer + both-or-nothing publication (Phase C.1) | `6ed13c52` | TestEngineSave 22P/0F |
| PGR-23 | HIGH | Proof certifies nested compressed containers | **Open** — no fix landed in Phase C; highest remaining open finding | — | — |
| PGR-24 | MED | Signing-candidate temp-file leak | **Fixed** (on line, M3) | twinned pre-Phase C | TestSignatureRealCrypto |
| PGR-25 | MED | Secret-store lost-update race (multi-process) | **Fixed** — `QLockFile` serialization (Phase C.2) | `99f8d3a6` | TestSecretStore 21P/0F |
| PGR-26 | LOW | `CRED_PERSIST_ENTERPRISE` roams credentials | **Fixed** — per-machine persist (Phase C.2) | `37561af9` | TestSecretStore 21P/0F |
| PGR-27 | HIGH | Checked redo applied the edit twice | **Fixed** (§6b) | twinned pre-Phase C | TestCheckedMutationCoverage |
| PGR-28 | MED | Eye Care use-after-free | **Fixed** (§6b) | twinned pre-Phase C | TestViewingModes |
| PGR-29 | HIGH | Image move/resize/rotate never worked | **Fixed** (§6b) | twinned pre-Phase C | byte-exact operand replace pins |
| PGR-30 | HIGH | `listImages` placement reported wrong | **Fixed** (§6b) | twinned pre-Phase C | — (§6b) |
| PGR-31 | MED | Image rotation pivoted on wrong point | **Fixed** (§6b) | twinned pre-Phase C | 30° rotate + undo pin |
| PGR-32 | MED | Refused commit could crash the app | **Fixed** (§6b) | twinned pre-Phase C | `catch (std::exception&)` |
| PGR-33 | LOW | `editTextInline` `Tf` dead code | **Open as-is** (owner decision — archived, do not enable) | — | — |
| PGR-34 | — | UX-flow harness race (3 of 11 flows) | **Fixed** (consolidation bring-in) | picked during Phase B | TestSweepW3UxFlows 180/180 |
| PGR-35 | HIGH | Form disclosures rendered as rich text (UI spoofing/beacon) | **Fixed** (Phase C.5 formjs) | `40c9c382` | TestFormJsAdversarial 24P/0F/1skip (`pgr35PanelDisclosuresRenderAsPlainText`); TestFormKeystroke 9P/0F |
| PGR-36 | MED | `AFSimple_Calculate` accepted inherited prototype names | **Fixed** (Phase C.5 formjs) | `abe351cb` | TestFormJsCalc 49P/0F + adversarial suite |
| PGR-37 | MED | NaN/Infinity `event.value` wiped committed `/V` | **Fixed** (Phase C.5 formjs) | `b5c63cbc` | TestFormJsCalc 49P/0F |
| PGR-38 | LOW | `__proto__` field names vanish from snapshot | **Fixed** (Phase C.5 formjs) | `82f309ab` | TestFormJsAdversarial snapshot pin |
| PGR-39 | LOW | Shim leaked 9 internal helpers as globals | **Fixed** (Phase C.5 formjs) | `82f309ab` | TestFormJsAdversarial surface pin |
| PGR-40 | HIGH | quickjs-ng CPU-deadline bypass (sparse-array natives) | **Deferred — check-lane verified the fix is NOT in 0.15.1 either.** Pin coordinated: enforced `GLYPHPDF_QUICKJS_PIN` bumped 0.15.0 → 0.15.1 (Phase C.6, `77db5be5` ← `020c0734`); same-machine A/B probe (`Array(2^31)` includes/indexOf/join) shows the interrupt handler is still never polled inside native sparse-array scans on 0.15.1; the pin's probe auto-arms when a carrying release lands. Staged runtime `libqjs-0.dll` sha256 `cc92ba7e…` = 0.15.1-1, hash-verified at integration. | `77db5be5` (pin coordination only) | TestFormJsAdversarial `nativeSparseArrayScansAbideTheDeadline` (disclosed skip, auto-arms) |
| PGR-41 | LOW | Cascade cross-event tamper window (`FormJsRunner`) | **Deferred** — fresh-runtime-per-event is a design decision (platform-inherent) | — | pinned test flips deliberately |
| PGR-42 | HIGH | *(D2 lane "PGR-35")* Batch cross-file output-path collisions overwrite silently | **Fixed on `feat/pgr-d2` — NOT on PR** | `39058fa9` (off-PR) | TestPgr35BatchCollision 4P *(lane)* |
| PGR-43 | HIGH | *(D2 lane "PGR-36")* Stale signing-progress panel replays cross-document steps | **Fixed on `feat/pgr-d2` — NOT on PR** | `3fd91495` (off-PR) | TestPgr36StaleSigningPanel 3P *(lane)* |
| PGR-44 | **CRITICAL** | *(D2 lane "PGR-37")* Pattern/replace excision space-law regression (rot90/270 + offset-origin survive) | **Fixed on `feat/pgr-d2` — NOT on PR** | `abe093aa` (off-PR) | TestPgr37PageSpaceLaw 9P *(lane)*; TestPatternRedact 8, TestFindReplace 9, TestRotate270PageSpace 8, TestLegacyOriginSpace 9 *(lane)* |
| PGR-45 | MEDIUM | *(D2 lane "PGR-38")* Batch progress/ETA skewed by mid-run list edits | **Fixed on `feat/pgr-d2` — NOT on PR** | `958bd7c0` (off-PR) | TestBatchMode 17P *(lane)* |
| PGR-46 | MED-HIGH | PatternRedactor `extractCharsFromOpenDoc` mixed-space flip — RedactMode **mark-all** places viewer marks from those rects, so pattern-redaction marks on rotated/offset pages land away from the matched text (same class the T2-2 fix corrected for Find&Replace) | **Open — recorded, not fixed** (residual-exec lane discovery; owner/redaction-lane surface) | — | — (evidence: `9b2b2727` commit message; RESIDUAL-PLANS-2026-09-21 Plan 5(b), CONSOLIDATED-REPORT §2.6) |

### ID collision note (PGR-42…45)

The `feat/pgr-d2` lane numbered its four findings PGR-35…38 in parallel with the
`feat/formjs-review` lane, whose PGR-35…41 were already committed to this
report (§10). The D2 findings are renumbered **PGR-42…45** here (D2-35→42,
D2-36→43, D2-37→44, D2-38→45); the fixes on `feat/pgr-d2` keep their original
commit messages/IDs, cross-referenced by SHA above. Renumbering is editorial
only — no code or branch was changed.

### Round-2 triage items landed with Phase C (not PGR-numbered)

| Item | Branch ref | Fix on PR | Test |
|------|-----------|-----------|------|
| T1 OCR review-state re-entrancy completion (word-inspector gates, semantic-delivery run-button) | `feat/pgr-c4` `a5addfbb` | `95352e53` | TestOcrReviewLifecycle 34P/0F (2 new lifecycle pins) |
| T3 Compare structural-anchor memoization | `feat/pgr-c4` `280a17ff` | `9e9e987a` | TestSep13LeadComparePerf 4P/0F (ratio 3.7x); TestCompareEntry 26P/0F; TestCompareIntegration 8P/0F |
| T7 `AiOptions` maxTokens/temperature forwarded to Ollama | `feat/pgr-c4` `632b93ac` | `9344eae5` | TestOllamaProvider 63P/0F (`optionsForwardMaxTokensAndTemperature`) |
| T6 Secret-store key/plaintext zeroization | `feat/pgr-c4` `9f0ddb63` | `6722356f` | TestSecretStore 21P/0F (suite-equivalence NC; zeroization itself inspection-verified — no behavioral pin possible) |

## Tally

- **Fixed (all lines):** 38 of 46 IDs (PGR-01…22, 24, 25, 26, 27…32, 34, 35…39, 42…45 — of which 42…45 sit on `feat/pgr-d2`, off this PR).
- **Open on this PR:** PGR-23 (HIGH), PGR-33 (LOW, owner as-is), PGR-46 (MED-HIGH, recorded-not-fixed, redaction lane queue).
- **Deferred:** PGR-40 (HIGH — fix not in any MSYS2-carried release; pin coordinated to 0.15.1, probe auto-arms), PGR-41 (LOW, design decision).
- **On-PR fix coverage:** every CRITICAL finding (PGR-12, PGR-21, D2's PGR-44-pending) is fixed or pending-integration; PGR-23 is the only HIGH open on the PR besides deferred PGR-40.
