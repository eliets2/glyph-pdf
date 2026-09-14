# SEP13-LEADS — Adversarial Confirmation Report (2026-09-14)

- **Branch under review:** `feat/sep13-leads` @ `83be3c2` (= merged `feat/parity-glm` candidate)
- **Lane:** review-only adversarial confirmation. NO `src/` production edits; new
  `tests/TestSep13Lead*` repros + CMake registration are the only tree changes.
- **Method:** each lead from `PARITY-GLM-REVIEW-2026-09-13-FINDINGS.md` was either
  (a) turned into a runtime repro whose CORRECT-contract assertion must FAIL on the
  candidate (the failure is the confirmation evidence), or (b) refuted/confirmed by
  static re-verification at the current tip when no review-only runtime probe exists.
- **Build:** `build-sec` (Ninja, Debug), UCRT64, `-j 2`. Tests run with
  `QT_QPA_PLATFORM=offscreen`, QtTest `-o …,txt` (raw outputs in `.context/results/`,
  not committed; the decisive excerpts are quoted below).
- **Verdict legend:** CONFIRMED (defect demonstrated; evidence quoted) ·
  REFUTED (claim does not hold on this candidate; pinned by a passing test/probe) ·
  INCONCLUSIVE (exact missing piece stated).

## Verdict summary

| ID | Lead (findings doc) | Verdict | Evidence |
|----|---------------------|---------|----------|
| L1 | B-T timestamp failure silently downgraded to B-B, reported `Success` | **CONFIRMED** (runtime) | `TestSep13LeadBtDowngrade` |
| L2 | `exportToHtml` raw font name into `style="…"` | **CONFIRMED** (runtime) | `TestSep13LeadConversionExport` |
| L3 | Degraded branch never reverses registry-owned disable | **CONFIRMED** (runtime) | `TestSep13LeadCapability` |
| L4 | `leafCert`/`issuerCert` leak on error paths | **CONFIRMED** (static) | §L4 |
| L5 | Proof ignores /Rotate + MediaBox origin → false `VerifiedNoTextInRegion` | **CONFIRMED** (runtime) | `TestSep13LeadRedactionProof` |
| L6 | Proof recall = source-side extraction only; pack wording overclaims | **CONFIRMED** (structural; runtime fixture not forced) | §L6 |
| L7 | Proof attribution blind to annotation/form strings → false PASS | **CONFIRMED** (runtime, mixed-page false PASS) | `TestSep13LeadRedactionProof` |
| L8 | Overlay label Y ignores MediaBox origin | **CONFIRMED** (runtime) + **ESCALATED**: excision itself misses on offset pages | `TestSep13LeadRedactionProof` |
| L9 | Merge failed-save appends phantom N+1th result + false successes | **CONFIRMED** (runtime) | `TestSep13LeadBatchMerge` |
| L10 | Cancelled merge reports successes pointing at never-written output | **CONFIRMED** (runtime) | `TestSep13LeadBatchMerge` |
| L11 | `onRejectResults`/`onReOcrRegion` lack the ReviewState guard | **CONFIRMED** (runtime) | `TestSep13LeadOcrGuards` |
| L12 | `applyChangeTypeFilters` O(rows×anchors) per toggle | **CONFIRMED** (measured perf-only) | `TestSep13LeadComparePerf` |
| L13 | `deriveColumns` misassigns ragged columns | **CONFIRMED** (runtime) | `TestSep13LeadConversionExport` |
| M1 | `releaseResidentFile` never clears `encryptionPassword` | **CONFIRMED** (static, LOW) | §M1 |
| M2 | (= L10) | **CONFIRMED** | see L10 |
| M3 | N06 candidate temp leak on failure paths | **CONFIRMED** (static, two live paths) | §M3 |
| M4 | `cipherLen` 64→32-bit truncation | **CONFIRMED** (static, negligible practical) | §M4 |
| M5 | OCR word edits never refresh canvas | **CONFIRMED** (static, display-only) | §M5 |
| M6 | CompareMode setDiffResult ordering | **INCONCLUSIVE** (perf-only redundancy; correctness claim too vague) | §M6 |
| M7 | `clusterIntoRows` merges distinct lines | **CONFIRMED** (runtime) | `TestSep13LeadConversionExport` |
| M8 | `RedactOperation` accumulates per run | **CONFIRMED** (static, LOW) | §M8 |
| L2b | OpenXLSX silent cell overwrite (~312) | **REFUTED** for this build (dead code) | §L2b |
| L2c | PPTX writer escaping (~1199) | **REFUTED** (passing probe) | `TestSep13LeadConversionExport::xmlStreamWriterEmptyElementAttributeProbe` |

---

## L1 — B-T silent downgrade to B-B reported as plain Success — CONFIRMED (runtime)

Anchor: `src/engines/SignatureManager.cpp:1390` — `qWarning() << "B-T: TSA returned
empty token — signature downgrades to B-B"`; `SignatureOutcomeDetail` carries only
`dssMissing`/`docTimestampMissing` (B-LT/B-LTA), nothing records the B-T→B-B drop;
outcome proceeds to `d->lastOutcome = SignOutcome::Success`.

Probe (`tests/TestSep13LeadBtDowngrade.cpp`): sign `tests/fixtures/signing/test_input.pdf`
at `PAdESLevel::B_T` with TSA `https://127.0.0.1:9/tsa` (unreachable). Measured run:

```
QWARN  : HTTP POST to "https://127.0.0.1:9/tsa" timed out
QWARN  : B-T: TSA returned empty token — signature downgrades to B-B
QINFO  : dead-TSA outcome: 2 dssMissing = false docTimestampMissing = false
FAIL!  : 'outcome != SignOutcome::Success || detail.dssMissing || detail.docTimestampMissing'
         SEP13 lead 1 CONFIRMED: ... the silent B-B downgrade is invisible to the caller/UI
Totals: 3 passed, 1 failed   (control `B_T` with no TSA reports Success: PASS)
```

(`2` = `SignOutcome::Success`.) **Impact:** false-success — the user believes they
produced a timestamped signature; the outcome enum's own contract ("Fully successful
at the requested PAdES level") is violated. The signed bytes are a valid B-B
signature, so integrity is not at risk — this is a disclosure/honesty defect.

## L2 — HTML export attribute injection via PDF font name — CONFIRMED (runtime)

Anchor: `src/engines/ConversionManager.cpp:359` — `.arg(el.rect.x())…arg(el.fontName)`
inside `style="…font-family: '%4';"`; text is `toHtmlEscaped()`, the font name is not.

Probe: craft a PDF whose BaseFont is `EVIL;color:red'x"y` (all legal name characters),
export to HTML. Measured:

```
style around font-family: "font-family: 'EVIL;color:red'x\"y';\">HELLO</div>"
FAIL! : '!html.contains("color:red")' — SEP13 lead 2 CONFIRMED: raw PDF font name
        concatenated into style="..." — attribute injection
```

**Impact:** any PDF whose font name carries attribute metacharacters injects/breaks
CSS-HTML in the exported file (downstream rendering of a user-supplied document).

## L3 — Degraded never reverses a registry-owned disable — CONFIRMED (runtime)

Anchor: `src/core/Capability.cpp:212-215` — the `Degraded` branch only sets a
tooltip; it never checks `capOwnedDisable` (the `Available` branch does, 200-210).

Probe (`TestSep13LeadCapability`): widget disabled via an `UnavailableRuntime`
apply, then the probe flips to `Degraded` and `registry.invalidate()` is called
(the documented production re-probe flow) before the second `applyToWidget`:

```
FAIL! : 'w.isEnabled()' returned FALSE. (SEP13 lead 3 CONFIRMED: Degraded apply left
        the registry-disabled widget stuck disabled (Degraded means 'keep the control usable'))
Totals: 4 passed, 1 failed
```

Control pins PASS: the Available branch DOES reverse the owned disable (D06), and
Degraded on a fresh enabled widget never disables. Note: without an explicit
`invalidate()`, `query()` serves the cached UnavailableRuntime capability and NO
branch runs — production callers must invalidate for transitions to be observed at
all; with the documented flow, the Degraded window is exactly the stuck-disabled
state. **Impact:** capability loss in the Degraded phase (feature unusable with a
"keep the control usable" contract); no data loss.

## L4 — X509 leaf/issuer certs leak on signing error paths — CONFIRMED (static)

`src/engines/SignatureManager.cpp` `signDocumentImpl`: raw `X509*` `leafCert`/
`issuerCert` from `loadP12` (1312) have no RAII (unlike `EvpPkeyPtr pkey`). Frees
exist only at 1329-1330 (weak-key rejection), 1508-1509/1519-1520 (certify-reference
refusals), and 1687-1688 (main cleanup — placed BEFORE the B-LTA block, post-validate
and commit). Leak paths at the current tip:

- 1335-1341 `i2d_PrivateKey` failure: `return Failed` frees neither cert.
- 1556-1558 `makeUniqueCandidate` failure: `return Failed` frees neither cert.
- Outer catch handlers 1763-1773 (`PdfError` / `std::exception` / `...`): run
  `cleanupCandidate()` but free no certs — any PoDoFo/std exception between cert
  load (1312) and the 1687-1688 frees leaks both certs per attempt.

**Impact:** memory-only (bounded by signing attempts); LOW. Review-only lane — no
runtime leak-count probe without production edits.

## L5 — Redaction proof ignores /Rotate and MediaBox origin → false certification — CONFIRMED (runtime)

Anchor: `src/core/RedactionProof.cpp:685-694` `runIntersects` flips with
`pageHeight` only; 813: `pageHeight = GetMediaBox().Height` (origin discarded);
820: no-intersection ⇒ `VerifiedNoTextInRegion`.

Measured (`TestSep13LeadRedactionProof`, PDFium run rects quoted):

- Offset page `MediaBox [0 200 612 1042]`, secret line at user y=900
  (run `rect=(100,900,154.968x12)`), viewer-correct mark over it:
  `offset: status = 1 removed = QList() passed = true` → status 1 is
  `EntryStatus::VerifiedNoTextInRegion` — a clean certification of a region that
  still contains the secret. FAIL (assertion demands attribution).
- `/Rotate 90` page, secret at user (100,300): `rotated: status = 1 removed =
  QList() passed = true` — same false certification.
- Control (standard-origin page): `status = 2 (Failed) removed =
  ("TopSecretAlpha bare secrets")` — attribution and loud failure work where the
  Height-only flip is valid. PASS.

**Impact:** false-success on a security surface — the pack certifies "no text in
region" for marks that cover text on rotated/offset pages; combined with L8 the
underlying secret can also still be IN the output (see L8 escalation).

## L6 — Proof recall bound to source extraction; pack wording overclaims — CONFIRMED (structural)

Attribution derives `removedStrings` ONLY from PDFium page-content runs
(`RedactionProof.cpp:666` `inv.runs[p] = backend.extractPageTextRuns(p)`; mark
matching at 813-825). A font PDFium cannot extract (subset, no `/ToUnicode`)
yields no attribution targets, and the proof's own `ExtractedText` survivor sweep
uses the same decode-level extraction — such text is invisible to the verdict.
The report text itself (1030-1038) claims "Text encoded with non-standard glyph
encodings is covered by decode-level text extraction" — that is the overclaim:
decode-level extraction IS the gap, not the coverage.

**Missing piece (honest limitation):** no runtime fixture with an
extraction-blind font was forced in this review-only lane (needs a crafted
subset-font PDF); the verdict is structural from the code + the pack wording.
All other claims in this report are runtime-measured.

## L7 — Proof attribution blind to annotation/form strings → false PASS — CONFIRMED (runtime, decisive)

Measured across three fixture shapes (`TestSep13LeadRedactionProof`):

1. Stream-less annotation-only page: PDFium runs = 0 (attribution blindness
   proven), proof reports `passed = false` with `UNSWEPT [PageStreams] … could
   not be decoded` — errs LOUD (probe passes; documents this variant).
2. Mixed page (decodable content low on the page + `AnnotationSecret` only in a
   FreeText annotation; mark covers the annotation):
   `mixed: status = 1 removed = QList() passed = true` → **the proof certified
   `VerifiedNoTextInRegion` while the annotation secret survives in the output**.
   FAIL (assertion demands a non-PASS) — the false PASS is confirmed.
3. Mixed page with content near the (mis-mapped) mark band: the content line is
   attributed instead (Height-only flip, see L5) and the proof fails loudly with
   `SURVIVOR [extracted-text] …` — loud again.

**Impact:** false-success on a security surface: redacting an annotation whose
text never entered page content can be certified clean while
`AnnotationSecret` remains in the output verbatim (annotation objects are swept
only for derived targets — which never included the annotation string).

## L8 — Overlay/excision Y ignores MediaBox origin — CONFIRMED + ESCALATED (runtime)

Anchors: `src/engines/RedactOperation.cpp:189,222` and the same flip in
`PoDoFoBackend::applyRedactions` (2545-2546) — `pageHeight = GetMediaBox().Height`,
lower-left origin discarded.

Measured on the offset page (`MediaBox [0 200 612 1042]`, mark over the secret at
user y≈882..912):

```
redact outcome: 0 ("")                      ← RedactOutcome::Completed — SUCCESS reported
run "TopSecretAlpha bare secrets" rect=(100,900,…)   ← secret STILL IN THE OUTPUT
run "LABEL" rect=(229.105,695.187,…)        ← label in the flipped band ~[682..712],
                                               200pt (the ignored origin) below the mark
FAIL! : excision must remove the secret line (if this fails the coordinate bug
        reaches the excision itself — CRITICAL)
```

**Escalation vs the original LOW rating:** the findings note claimed "excision
itself unaffected". Measured: on offset-origin pages the excision misses (the
PoDoFo redaction rect is flipped with the same Height-only math), the operation
still reports `Completed`, and the secret survives. Combined with L5 (the proof
certifies the region clean) this is user-data-loss with a false-success report —
the highest-severity measured outcome of this lane.

## L9 — Merge failed-save phantom result + false successes — CONFIRMED (runtime)

Anchor: `src/modes/BatchMode.cpp:1535-1540` `failOutput` appends an extra result
(`inputPath = outPath`, "the failed item is the output artifact") beyond the
per-input results added at 1574.

Measured (doomed save into a nonexistent directory, 2 input files):

```
save-failure accounting: success = 2 fail = 1 fileCount = 2 output exists = false
FAIL! : 'accounted <= bm.fileCount()' — SEP13 lead 9 CONFIRMED: 3 results accounted
        for 2 input files — the failed-save 'output artifact' item is a phantom
```

Both per-input results were `success=true` pointing at the never-written output
(companion false-success; same class as L10). **Impact:** false accounting; user
believes files were merged.

## L10 — Cancelled merge counts successes against a never-written output — CONFIRMED (runtime)

Anchors: per-input `addResult` at 1574; `if (promise.isCanceled()) return;` at
1546 (loop top, after earlier results were already added) and 1583 (pre-save,
after ALL results); `onBatchFinished` (1679-1700) drains results with no cancel
check.

Measured (cancel queued during the second file boundary, 3 files):

```
post-cancel accounting: success = 1 fail = 0 skip = 0 output exists = false
FAIL! : 'bm.successCount() == 0' — SEP13 lead 10 CONFIRMED: cancelled merge counted
        1 input(s) as successes pointing at the never-written merge output
```

Timing note: a cancel landing at the FIRST boundary (before any `addResult`)
yields success=0 (contract holds); the defect window is between the first
per-input result and the pre-save cancel check. **Impact:** false-success
accounting; output file absent though the summary claims a merged input.

## L11 — OCR reject/re-OCR lack the ReviewState guard — CONFIRMED (runtime)

Anchors: `src/modes/OCRMode.cpp:609` accept guard; `onRejectResults` (617-628)
clears words/session/canvas from ANY state; `onReOcrRegion` (707-709) emits
unconditionally (private slot; the context menu is its entry).

```
FAIL! : rejectDuringSavingMustBeIgnored — onRejectResults ran during Saving —
        state jumped to 0 (Idle) instead of staying Saving
FAIL! : reOcrOutsideReviewReadyMustBeIgnored — reOcrRegionRequested emitted from Idle
Totals: 3 passed, 1 failed (accept control pin PASS)
```

**Impact:** a reject during an in-flight save wipes review words/session and asks
the host to drop pending save state; re-OCR re-entrancy can be requested while
another run is Running/Saving. Workflow/integrity defect, no direct data loss.

## L12 — Filter toggle recomputes anchor roles O(rows×anchors) — CONFIRMED (measured, perf-only)

Anchors: `CompareMode.cpp` `applyChangeTypeFilters` per-row remap (~388-417);
`CompareWidget.cpp:152/160` `anchorIndexFor*` linear scans.

Measured (`TestSep13LeadComparePerf`; the probe asserts the quadratic signature):

```
anchor-role recompute: rows = 2000 -> 9 ms; rows = 8000 -> 151 ms;
ratio = 16.7778 (linear expectation ~4x, O(rows x anchors) expectation ~16x)
Totals: 3 passed, 0 failed
```

Direction note: this probe is a measuring instrument — it asserts the
superlinear growth (ratio ≥ 8 at 4× rows), so a DEFECT presence makes it PASS;
the measured 16.8× ≈ 16× is the confirmation. **Impact:** perf-only — each
filter toggle on a large diff stalls the GUI thread; mapping result stays
correct (U04 rebuild).

## L13 — deriveColumns misassigns ragged (right-aligned) columns — CONFIRMED (runtime)

Anchor: `src/engines/ConversionManager.cpp` `deriveColumns` — tolerance
`qMax(3.0, 0.5 * el.fontSize)`; an x-start beyond every anchor's tolerance
APPENDS A NEW ANCHOR (`best < 0 → anchors.append`), splitting one visual column.

Measured ("Total" at x=260, "5" at x=295, same visual column, CSV export):

```
CSV row1: "\"Total\"" | row2: "\"\",\"5\""
FAIL! : SEP13 lead 13 CONFIRMED: deriveColumns split one visual column into two
        spreadsheet columns (row2 = "","5", expected "5")
```

**Impact:** wrong data placement in exported spreadsheets (a "5" total lands in a
different column than its label row) — silent data corruption in an output artifact.

## M7 — clusterIntoRows merges distinct lines — CONFIRMED (runtime)

Anchor: `ConversionManager.cpp:194` — join tolerance `qMax(1.0, 0.5 *
qMax(el.fontSize, g.maxFont))` = 12pt under a 24pt line.

Measured (8pt line 10pt below a 24pt line, Text export):

```
text lines: ("BOLDHEAD tiny detail\r", "\r")
FAIL! : SEP13 M7 CONFIRMED: one output row carries BOTH the 24pt and the 8pt line
        ("BOLDHEAD tiny detail") — the half-max-font join tolerance swallowed the small line
```

**Impact:** information-losing line merge in exported text/HTML (and column
upstream of L13/M7): the small line's independence is silently destroyed.

## L2b — OpenXLSX silent cell overwrite (~312) — REFUTED for this build

`build-sec/CMakeCache.txt`: `OpenXLSX_DIR:PATH=OpenXLSX_DIR-NOTFOUND` ⇒
`OpenXLSX_FOUND` false ⇒ `HAS_OPENXLSX` never defined (guard at
`CMakeLists.txt:800-802`) ⇒ the OpenXLSX export branch
(`ConversionManager.cpp:297-313`, including the silent-overwrite `wks.cell(…)`
assignment) is DEAD CODE in this build. The LIVE duplicate-cell defect is the
in-house OOXML writer (`exportToExcelInHouse`) already tracked as confirmed
finding #3 in the findings doc — same user-visible symptom, different code path.
Re-open L2b only if a build enables OpenXLSX.

## L2c — PPTX writer attribute/text escaping (~1199) — REFUTED (passing probe)

`TestSep13LeadConversionExport::xmlStreamWriterEmptyElementAttributeProbe` (PASS)
measures the exact writer pattern:

```
produced: <a:rPr xmlns:a="…drawingml/2006/main"><a:latin typeface="Evil;color:red'&amp;&lt;X"/></a:rPr>
round-trip: latinFound = true typeface = "Evil;color:red'&<X"
```

`&` and `<` are XML-escaped in attribute values, `writeAttribute` after
`writeEmptyElement` lands ON the element, and the output round-trips. The
escaping half of the lead does not hold; the writer's remaining duty is C0
control stripping (documented at `ConversionManager.cpp:624,633`).

## M1 — releaseResidentFile leaves encryptionPassword in memory — CONFIRMED (static, LOW)

`PoDoFoBackend.cpp:1683-1711`: on release, `d->document.reset()`,
`d->currentFile.clear()`, `d->reseatBuffer.clear()` — but `d->encryptionPassword`
(190) is not cleared; it survives until the next resolve sets/clears it
(3194/3219/403/448). LOW: memory residue only.

## M3 — N06 signing candidate temp leak — CONFIRMED (static, two live paths)

`cleanupCandidate()` runs on the post-validate empty path (1727, SEP13:4 fix
present) and on all three catch handlers — but NOT on:

- commit-fail `SafeSave::commitFileToDestination` failure (~1755-1758): `return
  Failed` with no cleanup — the reserved candidate file is orphaned;
- integrity-broken post-validate branch (~1743-1752) with `replaceOutput=true`:
  the comment says "the CANDIDATE is broken — drop it" but no drop/cleanup call
  exists — same orphaning.

**Impact:** temp-file leak per occurrence in the candidate directory (LOW-MED);
no correctness impact.

## M4 — cipherLen 64→32-bit truncation — CONFIRMED (static, negligible practical)

`EncryptedFileSecretStore.cpp:173-177`: `int cipherLen`; `EVP_EncryptUpdate`
receives `static_cast<int>(plaintext.size())`. Truncation requires a >2 GiB
single plaintext — not reachable with real secret-store entries. Correctness
landmine, not a practical defect.

## M5 — OCR word edits never refresh the scan canvas — CONFIRMED (static, display-only)

`m_scanCanvas->setWords` is called only at 628 (reject clears), 775 (results
arrive) and 1256 (clear); `applyWordCorrection` (909) / `markWordDeleted` (930)
mutate `m_reviewWords` without refreshing the canvas → stale overlay until a
state transition re-renders. Display-only (the review state that is exported is
the mutated one).

## M6 — CompareMode setDiffResult ordering — INCONCLUSIVE

Measured code reading: `setDiffResult` at 236/263 is idempotent under the
double-apply that arrives via `onDiffFinished` (~395-401) — the effect is ONE
redundant anchor/tree rebuild (perf), and `applyChangeTypeFilters` remaps rows
deterministically (U04). The original claim ("call ordering / CHANGES-tree row
ordering") names no concrete wrong ordering to assert, and none was observed.
Missing piece: a concrete input where row order differs from spec. Without that
the correctness half is unconfirmable; the perf half is already L12.

## M8 — RedactOperation instances accumulate per run — CONFIRMED (static, LOW)

`src/modes/RedactMode.cpp:539` `auto* op = new RedactOperation(request, this);` —
parented to the long-lived RedactMode; the only `deleteLater` in the file (529)
belongs to `m_redactProgress`, never to the op. Each redaction run leaks one
operation object (with its request copy). LOW: slow-bounded memory growth.

---

## Cross-cutting measured synthesis (L5 + L7 + L8)

On offset-origin pages the three defects compose into the worst measured
outcome of this lane: a redaction over a secret **reports Completed**, the
secret **remains in the output**, and the proof **certifies the region
VerifiedNoTextInRegion**. On standard-origin pages with annotation-resident
secrets, the proof can certify a clean PASS (L7 mixed fixture). Both are
false-success outcomes on security surfaces.

## Residuals / coverage gaps

- L6: no runtime fixture with an extraction-blind font (needs crafted subset
  font); verdict structural.
- M6: no concrete mis-ordering input; correctness half unconfirmable.
- L4/M1/M3/M4/M5/M8: static confirmations — runtime leak-count/state probes
  would require production seams (review-only lane).
- Lane boundary honored: BatchMode.cpp, TextMatchFinder.*, FindReplaceDialog.*,
  EditController.*, FormManager*, SecurityController* untouched; all probes use
  public test seams.
- The B-T probe exercises the TSA-failure path via the harness's 20 s
  httpPost timeout (queued POST + blocked GUI event loop) — equivalent
  empty-token downgrade, slower than a refused connection.
