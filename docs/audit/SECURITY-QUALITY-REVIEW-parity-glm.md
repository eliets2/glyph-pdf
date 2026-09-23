# GlyphPDF — Security & Quality Review: `feat/parity-glm`

| | |
|---|---|
| **Document type** | Security & Quality Review Report |
| **Subject** | `feat/parity-glm` parity/hardening line, landed on `review/consolidated-parity` (consolidation PR → `main`) |
| **Reviewed scope** | base `main` `703fa34` → branch tip `9ba3cea` — ~180 changed source/test files, ~37,000 LOC |
| **Status re-verified at** | PR head `1991d9c1` (2026-09-23) — every finding re-read at its current location |
| **Not yet reviewed** | the 319 commits that landed on the line after `9ba3cea` — see [§7](#7-not-yet-reviewed--delta-after-9ba3cea) |
| **Date** | 2026-09-17 (review) · 2026-09-23 (status re-verification + specialist pass) |
| **Status** | 13 of 20 original findings fixed on the line · 7 open · 6 new from the specialist pass (5 open, incl. 1 CRITICAL) · 8 more found while verifying the PR (7 fixed in it — [§6b](#6b-found-while-verifying-the-consolidation-2026-09-23)) |
| **Detailed findings appendix** | [`PARITY-GLM-REVIEW-2026-09-13-FINDINGS.md`](PARITY-GLM-REVIEW-2026-09-13-FINDINGS.md) |

---

## 1. Executive summary

`feat/parity-glm` is a large parity + hardening line (≈175 commits at review time) whose engineering quality is **high overall** — heavily test-backed (more test code than source added), with many prior audit defects already fixed on the line. This review was an exhaustive, multi-method pass over the full diff.

It surfaced **defects that must be addressed before the line is landed**, spanning three risk classes:

- **Memory-safety / DoS** — an out-of-bounds heap write reachable from a crafted PDF (`PGR-01`), and unbounded-buffer / unbounded-allocation denials of service (`PGR-05`, part of `PGR-01`).
- **Security controls** — a secret-store integrity gap (`PGR-04`), read-only/edit-policy enforcement bypasses (`PGR-14`, `PGR-15`), and CSV/HTML injection in export/UI surfaces (`PGR-16`, `PGR-17`, `PGR-18`).
- **Data integrity & correctness** — a batch-merge operation that reports success when no file was written (`PGR-12`), redaction-proof certifications that can pass without actually checking (`PGR-09`, `PGR-10`, `PGR-13`), a fail-open signature re-validation (`PGR-08`), and an encrypted-document rollback that locks the user out (`PGR-06`).

A **build break on a valid build configuration** (`PGR-02`) was also confirmed.

### Status at the PR head (`1991d9c1`, 2026-09-23)

The line kept moving during and after the review, and its own sessions fixed most of what the review found. Re-reading every finding at the PR head:

- **Fixed on the line (13):** PGR-01, 02, 03, 04, 05, 07, 08, 09, 11, 12, 13, 14, 15.
- **Still open (7):** PGR-06 (encrypted rollback lock-out), PGR-10 (limitation now *disclosed*, verdict unchanged), PGR-16/17 (CSV formula injection), PGR-18 (the correction text is still unescaped — only the word text was escaped), PGR-19 (stale calibration factor), PGR-20 (narrowed: legacy AES blobs are still accepted on Windows).
- **New from the specialist pass on the PR code (§6):**
  - **PGR-21 · CRITICAL** — an in-place re-sign whose post-validation fails **deletes the user's only copy of the document**. The PGR-08 fix added a *second* branch with the same deletion.
  - **PGR-22 · HIGH** — a use-after-free on the save path.
  - **PGR-23 · HIGH** — the redaction proof certifies nested compressed attachments it cannot see into.
  - PGR-25 · MEDIUM and PGR-26 · LOW are also open; PGR-24 · MEDIUM is already fixed.

Verifying the PR itself found eight more defects ([§6b](#6b-found-while-verifying-the-consolidation-2026-09-23)). Seven of them are **fixed in this PR**, with regression tests, including image move/resize/rotate never working on a real PDF and Edit ▸ Redo applying four edit types twice.

**Bottom line:** fix **PGR-21 and PGR-22** before this lands on `main`. Fix the rest of the open list (§3) before the next release. Because of [§7](#7-not-yet-reviewed--delta-after-9ba3cea), a green status column does **not** mean the PR is reviewed: about 52K lines landed after the review snapshot, including a JavaScript runtime for PDF forms, and no review pass has covered them.

---

## 2. Methodology

The branch was reviewed by four independent methods and the results cross-checked:

1. **Manual audit + inline confirmation** — direct reading of the highest-blast-radius commits, with a clean build + full `ctest` run (which independently caught a flaky batch test, since fixed on the branch by its own G12 commit).
2. **`/code-review high`** — the deep single-pass reviewer over the net diff.
3. **Multi-agent review workflow** — 14 subsystem *finder* lanes (redaction, PoDoFo engine, signatures, save/forms, secret-crypto, AI, pages, OCR, conversion, compare, batch, capability/measure, shell-architecture, UI), each reading the branch code and reporting substantiated defects with concrete failure scenarios, followed by **adversarial verification** (a skeptic prompted to *refute* each finding).
4. **Static red-flag sweep** over the full 37K-line diff (weakened tests, swallowed exceptions, unsafe C, TLS-disabling, injection sinks) — clean.

On 2026-09-22/23 two further passes ran against the consolidated PR code:

5. **Specialist pass** (native-adversary, guarantee-verification-engine and emergence-engine agent definitions), which produced PGR-21 … PGR-26 (§6).
6. **Status re-verification**: each finding's location was re-read at `1991d9c1`. "Fixed" means the defect, as described, no longer follows from the code at that location. Where the line added a regression test for the fix, the test is named.

### Confidence tiers

Each finding carries a confidence tier; **remediate in this order**:

| Tier | Meaning |
|------|---------|
| **HIGH-CONFIDENCE** | Confirmed by ≥2 independent verifiers, cross-confirmed by two methods, or inline-confirmed by direct code reading. |
| **TRIAGE (single-vote)** | Surfaced by a finder and refute-tested by **one** adversarial verifier. The verification pass was reduced from 3 skeptics to 1 to fit the account usage cap for a full 14-lane run. Treat as a prioritized to-do list; **re-confirm each before landing a fix.** |

Coverage was **complete** for the reviewed scope — all 14 subsystem lanes ran to completion.

---

## 3. Remediation tracking

Status values: **Open** · **Open (narrowed)** · **Open (disclosed)** · **Fixed** · Won't fix · Duplicate. Statuses are as of the PR head `1991d9c1`.

| ID | Sev | Confidence | Location (review-time) | Title | Status @ `1991d9c1` | Evidence at PR head |
|----|-----|-----------|----------|-------|--------|----------|
| PGR-01 | HIGH | High | `PoDoFoBackend.cpp:~2528` | CID `/W` OOB heap write + DoS | **Fixed** | `PoDoFoBackend.cpp:3027-3065` — `kMaxCid = 65535` cap, negative CIDs rejected (`c < 0`) |
| PGR-02 | HIGH | High | `Capability.cpp:~382` | `#else` compile break (non-RapidOCR build) | **Fixed** | `Capability.cpp:396` — `Capability c;` hoisted above `#ifdef HAS_RAPIDOCR` |
| PGR-03 | HIGH | High | `ConversionManager.cpp:~848` | Excel export duplicate cell refs → invalid XLSX | **Fixed** | `ConversionManager.cpp:936-950` (SEP13:3) — one cell per column, last-write-wins, ordered emission |
| PGR-04 | HIGH | High | `EncryptedFileSecretStore.cpp:~250` | AES-GCM not AAD-bound to entry → secret substitution | **Fixed** | SEP13:5 — new writes are `0x03` (GCM AAD = service) / `0x04` (DPAPI entropy = service); legacy residue tracked as PGR-20 |
| PGR-05 | HIGH | High | `OllamaProvider.cpp:~290` | No response-size cap → memory-exhaustion DoS | **Fixed** | `OllamaProvider.cpp:237-246, 308-323` (SEP13:6) — 64 MiB default cap, `setReadBufferSize`, abort on overflow |
| PGR-06 | HIGH | High | `PoDoFoBackend.cpp:~236` | Encrypted-doc rollback drops password → lock-out | **Open** | `PoDoFoBackend.cpp:400-410` — `encryptionPassword.clear()` still precedes a password-less `Load()` |
| PGR-07 | MED | High | `RedactMode.cpp:539` | `RedactOperation` accumulating leak | **Fixed** | `RedactMode.cpp:593` — `finished` → `deleteLater` |
| PGR-08 | MED | High (2 methods) | `SignatureManager.cpp:~1677` | Post-sign re-validation fail-open | **Fixed** ⚠ | `SignatureManager.cpp:1800-1807` (SEP13:4) — empty result now fails; **but the new branch deletes the in-place original → PGR-21** |
| PGR-09 | HIGH | High (code-review) | `RedactionProof.cpp:~693` | Proof false-PASS on rotated/offset pages | **Fixed** | `RedactionProof.cpp:742-786` (SEP13 L8 shared transform); regression `tests/TestRedactionProof.cpp:122-148` (`/Rotate 270` page) |
| PGR-10 | MED-HIGH | High (code-review) | `RedactionProof.cpp:~905` | Proof false-PASS on unextractable text | **Open (disclosed)** | `RedactionProof.cpp:1071-1074` still certifies `VerifiedNoTextInRegion`; the pack text (`:1198-1208`) now states such text "is NOT covered by a PASS" |
| PGR-11 | LOW | High (code-review) | `RedactOperation.cpp:~222` | Overlay-label Y ignores MediaBox origin | **Fixed** | `RedactOperation.cpp:168-194` (SEP13 L8 shared viewer→user transform) |
| PGR-12 | **CRITICAL** | Triage | `BatchMode.cpp:1436` | Batch merge reports success though no file written | **Fixed** | `BatchMode.cpp:2018-2060` — results published only after `dst.Save()`; a save failure re-marks every success as failed |
| PGR-13 | CRITICAL-if-real | Triage | `RedactionProof.cpp:819` | Proof attribution misses annotation text | **Fixed** | `RedactionProof.cpp:707-760` (SEP13 L7 annotation/form-field attribution); `tests/TestSep13LeadRedactionProof.cpp` |
| PGR-14 | HIGH | Triage | `PagesController.cpp:338` | Read-only/edit-policy bypass (page ops) | **Fixed** | `PagesController.cpp:329, 388, 405` — `mutationBlocked()` gates |
| PGR-15 | HIGH | Triage | `EditController.cpp:371` | Read-only/edit-policy bypass (inline Replace) | **Fixed** | `EditController.cpp:538-571` routes through `replaceAllInDocument()`, gated at `:434` |
| PGR-16 | HIGH | Triage | `ConversionManager.cpp:410` | CSV/formula injection (CSV export) | **Open** | `ConversionManager.cpp:~480` — only `"` doubling; no leading `= + - @` neutralization |
| PGR-17 | HIGH | Triage | `CommentsWidget.cpp:752` | CSV/formula injection (comments export) | **Open** | `CommentsWidget.cpp:820-828` — `csvEscapeField` quotes only |
| PGR-18 | HIGH | Triage | `OCRMode.cpp:1037` | Unescaped HTML injected into OCR overlay | **Open** | `OCRMode.cpp:1101-1121` — only the word text is escaped; the correction (`rec.reviewedText`) enters `title='…'` raw |
| PGR-19 | HIGH | Triage | `MeasureCore.h:353` | Invalid calibration leaves stale `unitsPerPt` | **Open** | `MeasureCore.h:344-356` — unchanged |
| PGR-20 | HIGH | Triage | `EncryptedFileSecretStore.cpp:196` | Legacy-blob DPAPI-binding gap | **Open (narrowed)** | Windows default path still accepts `0x01`/`0x03` AES blobs under a key derived from non-secret identifiers — see §5 |
| PGR-21 | **CRITICAL** | High (specialist + inline) | `SignatureManager.cpp:1323, 1619, 1800-1823` | In-place re-sign deletes the user's only copy on failed post-validation | **Open** | §6 |
| PGR-22 | HIGH | High (inline) | `PoDoFoBackend.cpp:617-638` | Save-path re-seat use-after-free | **Open** | §6 |
| PGR-23 | HIGH | High (specialist + inline) | `RedactionProof.cpp:624-647` | Proof false-PASS on nested compressed containers | **Open** | §6 |
| PGR-24 | MED | Specialist | `SignatureManager.cpp:1326-1329` | Signing-candidate temp-file leak | **Fixed** | M3 (SEP13) — `cleanupCandidate()` on every failure exit |
| PGR-25 | MED | Specialist | `EncryptedFileSecretStore.cpp:~236` | Secret-store lost-update race (multi-process) | **Open** | §6 |
| PGR-26 | LOW | Specialist | `CredentialManager.cpp:46` | `CRED_PERSIST_ENTERPRISE` roams credentials | **Open** | §6 |

About 30 more MEDIUM/LOW triage items are catalogued with scenarios in the [findings appendix](PARITY-GLM-REVIEW-2026-09-13-FINDINGS.md#round-2-complete--all-14-lanes-sonnet-single-vote-adversarial-verify). They include OCR review-state guards, compare-tree ordering, conversion column/row clustering and key/plaintext zeroization. They were **not** re-verified at the PR head.

---

## 4. High-confidence findings

> Confirmed by ≥2 verifiers, two independent methods, or inline reading. **Act on these first.**

### PGR-01 · HIGH · CID `/W` array out-of-bounds heap write + DoS — **Fixed**
**`src/engines/podofo/PoDoFoBackend.cpp:~2528` — `ensureCompleteCidSets()`**
A Type0 font's descendant-CIDFont `/W` array is parsed with no bounds validation (`GetNumber() → int64_t`, no clamp to the valid `0..65535` CID range).
- **Negative CID** (`/W [-5 [500 500 500]]`) → `bits` sized `(-3/8)+1 = 1`, then `bits[static_cast<size_t>(-5)/8]` (~2.3e18) is a **wild out-of-bounds heap write**; the `0x80u >> (cid % 8)` negative shift is additionally UB. Heap corruption escapes `exportPdfA`'s `catch(std::exception)`.
- **Huge range** (`/W [0 4000000000 500]`) → ~4e9-entry `std::set` + ~500 MB allocation = **OOM/hang DoS**.
- **Reachable** via **Export → PDF/A** on any opened crafted PDF.
- **Fix:** reject/clamp `cid` to `0 ≤ cid ≤ 65535` before insert and before indexing; cap the range span.
- **@ `1991d9c1`:** fixed at `:3027-3065` — `constexpr int64_t kMaxCid = 65535`, over-range CIDs are dropped, and both the single-CID form (`:3052`) and the range form (`:3065`) reject `c < 0`.

### PGR-02 · HIGH · `#else` branch compile error (config-dependent build break) — **Fixed**
**`src/core/Capability.cpp:~382` — `probeOcrRapidModels()`**
`Capability c;` is declared only inside the `#if` (RapidOCR) branch; the `#else` branch does `c.status = …` and `return c;` with `c` undeclared → **compile error on any build without the RapidOCR/ONNX engine.** The current CI config compiles the `#if` branch, so it is latent today.
- **Fix:** hoist `Capability c;` above the `#if`/`#else`.
- **@ `1991d9c1`:** fixed — `Capability c;` at `:396`, above `#ifdef HAS_RAPIDOCR` at `:397`.

### PGR-03 · HIGH · Excel export emits duplicate cell references → invalid XLSX — **Fixed**
**`src/engines/ConversionManager.cpp:~848` — `exportToExcelInHouse()`**
The per-row loop writes `r="<col><row>"` from `el.column` with no dedup. Two non-empty runs sharing a geometry-derived column emit **duplicate `<c r="B2">` refs** in one `<row>` → invalid OOXML; Excel repairs/rejects the file.
- **Fix:** merge or skip runs resolving to the same column.
- **@ `1991d9c1`:** fixed (SEP13:3, `:936-950`). Runs are stable-sorted and emitted through an ordered map with one cell per column. The policy is last-write-wins, which matches the OpenXLSX path, and it also restores strictly increasing `r` order.

### PGR-04 · HIGH · Secret store: AEAD not bound to entry identity — **Fixed**
**`src/core/EncryptedFileSecretStore.cpp:~250`**
AES-256-GCM ciphertexts are never bound (no AAD) to the JSON `service` key they are filed under, and `resolveKey()` is service-independent. A local actor with **write access to `secrets.enc.json` — no AES key or DPAPI access needed** — can swap the base64 blobs of two entries; each still decrypts + authenticates under its own nonce/tag, so `readSecret("api_key_prod")` silently returns the swapped secret. The Windows DPAPI path shares the flaw (constant description string, `pOptionalEntropy = nullptr`).
- **Fix:** feed the `service` name as GCM AAD (and as DPAPI optional-entropy) and verify it on read.
- **@ `1991d9c1`:** fixed for all new writes (SEP13:5). `0x03` carries the service name as GCM AAD, and `0x04` (the Windows default) carries it as DPAPI entropy; a swapped blob now fails loudly. The legacy formats `0x01` and `0x02` are still *read* without binding, to allow migration. That residue, and what it enables on Windows, is tracked as **PGR-20**.

### PGR-05 · HIGH · Unbounded response buffering (DoS) — **Fixed**
**`src/engines/ai/OllamaProvider.cpp:~290`**
`reply->readAll()` buffers the entire HTTP response body with no size cap (`setReadBufferSize()` never called) → a malicious/compromised endpoint returns a huge body and exhausts memory.
- **Fix:** enforce a maximum response size and abort when exceeded.
- **@ `1991d9c1`:** fixed (SEP13:6). `maxResponseBytes` defaults to 64 MiB and is bounded to 4 KiB … 1 GiB. `setReadBufferSize` is set at `:308`, the body is accumulated incrementally, and the reply is aborted once the cap is exceeded (`:316-323`).

### PGR-06 · HIGH · Encrypted-document rollback drops the password → lock-out — **Open**
**`src/engines/podofo/PoDoFoBackend.cpp:~236` — `restoreResidentFromSource()`**
After the document has been encrypted this session, a *failed* mutation-commit triggers `restoreResidentFromSource()`, which **clears `d->encryptionPassword` and then `Load()`s the on-disk (now-encrypted) file with no password** → PoDoFo throws, the resident document is dropped, and every subsequent path-based operation re-throws (same gap at `:208`). One failed commit locks the user out of editing their own intact, on-disk-valid encrypted document for the rest of the session.
- **Fix:** reload with the retained `encryptionPassword`; do not clear it before a successful reload.
- **@ `1991d9c1`:** unchanged — `:405` clears the password, then `:410` runs `restored->Load(src.toUtf8().constData())` with no password.

### PGR-07 · MEDIUM · `RedactOperation` accumulating leak — **Fixed**
**`src/modes/RedactMode.cpp:539`**
`runRedactOperation()` allocates `new RedactOperation(request, this)` parented to the long-lived `RedactMode` widget and never `deleteLater()`s it (contrast the sibling `QProgressDialog`, which is explicitly deleted). Every Apply-Redactions in a session accumulates a `RedactOperation` (holding the full per-page mark set) until the widget is destroyed — unbounded growth in an iterative redaction workflow.
- **Fix:** `op->deleteLater()` (or track+replace) when the operation finishes.
- **@ `1991d9c1`:** fixed — `:593` `connect(op, &RedactOperation::finished, op, &QObject::deleteLater)`.

### PGR-08 · MEDIUM · Post-sign re-validation is fail-open — **Fixed (see PGR-21)**
**`src/engines/SignatureManager.cpp:~1677`** *(found by both `/code-review high` and the workflow)*
The D6 post-condition re-validation only fails when a returned `SignatureInfo` has `integrityIntact == false`. An **empty** `validateSignatures()` result (signature not detected/parsed on the candidate) makes the loop body never run → the document is committed as successfully signed though its signatures could not be confirmed.
- **Fix:** treat an empty / again-unparseable validation result as failure.
- **@ `1991d9c1`:** fixed (SEP13:4, `:1800-1807`) — an empty result now fails. **However**, the new branch "mirrors the broken-integrity branch" with `if (!replaceOutput) QFile::remove(outputPath);`. That *adds* a second path to the in-place data loss described in **PGR-21**.

### PGR-09 · HIGH · Redaction-proof false-PASS on rotated / offset pages — **Fixed**
**`src/core/RedactionProof.cpp:~693`**
The proof maps redaction marks with `pageHeight` only, ignoring page `/Rotate` and non-zero MediaBox origin. On rotated/offset pages no source run is attributed, `removedStrings` stays empty, and the entry is certified **`verified-no-text-in-region`** — a **false certification** of a redaction the proof never actually checked.
- **Fix:** apply page rotation and MediaBox-origin translation in the mark↔run coordinate mapping.
- **@ `1991d9c1`:** fixed (SEP13 L8). Marks go through the shared viewer→user transform, which honours the MediaBox origin and `/Rotate` (`:742-786`). A regression test covers a `/Rotate 270` page (`tests/TestRedactionProof.cpp:122-148`).

### PGR-10 · MED-HIGH · Redaction-proof false-PASS on unextractable text — **Open (disclosed)**
**`src/core/RedactionProof.cpp:~905`**
Proof recall depends entirely on source-side PDFium extraction; text PDFium cannot extract (subset font with no `/ToUnicode`) yields empty `removedStrings` → a passing verdict, contradicting the pack's claim that non-standard encodings are covered.
- **Fix:** detect the no-extractable-text case and downgrade the verdict to "unverifiable," not PASS.
- **@ `1991d9c1`:** half done. The proof-pack text (`:1198-1208`) now states the limitation honestly: text whose encoding defeats extraction "is NOT covered by a PASS". The per-entry status is still `VerifiedNoTextInRegion` (`:1071-1074`), though. A reader who looks only at the entry still sees *verified*. The entry should be downgraded, for example when the marked region has glyph-drawing operators but extraction yields nothing.

### PGR-11 · LOW · Overlay-label Y ignores MediaBox origin — **Fixed**
**`src/engines/RedactOperation.cpp:~222`** — burn-in overlay-label Y uses `pageHeight` without the MediaBox lower-left origin → the label is mispositioned on offset pages. The excision itself is unaffected (cosmetic).
- **@ `1991d9c1`:** fixed (SEP13 L8, `:168-194`) — uses the same shared viewer→user transform.

---

## 5. Triage findings (single-vote — re-confirm before fixing)

> Surfaced by a finder and refute-tested by **one** adversarial verifier. Each was re-read at `1991d9c1`; the open ones below were re-confirmed by that reading. Full scenarios are in the [appendix](PARITY-GLM-REVIEW-2026-09-13-FINDINGS.md).

### PGR-12 · CRITICAL · Batch merge reports success though no file was written — **Fixed**
**`src/modes/BatchMode.cpp:1436`** — the merge worker `promise.addResult(r)` with `r.success = true` and a fabricated `outputPath` inside the per-file loop **before** `dst.Save(outPath)` is ever called. If the final `Save()` throws, every input is still reported as a successful merge to a file that does not exist — a silent data-integrity failure in a destructive operation.
- **Fix:** report success only after `dst.Save()` succeeds; on save failure mark the batch failed.
- **@ `1991d9c1`:** fixed (`:2018-2060`). Per-input results are collected and published only after `dst.Save()` returns. A save exception re-marks every appended success as failed and clears its `outputPath`. A cancel before the save publishes nothing, and a merge where nothing was appended publishes each file's own failure.

### PGR-13 · CRITICAL-if-real · Redaction-proof attribution misses annotation text — **Fixed**
**`src/core/RedactionProof.cpp:819`** — attribution derives `removedStrings` only from PDFium page-content runs; sensitive text living in an annotation (FreeText/Stamp/Highlight-with-Contents) over the marked region may go unattributed → unswept, while the proof still certifies. Companion to PGR-09/PGR-10.
- **@ `1991d9c1`:** fixed (SEP13 L7, `:707-760`). Annotation `/Contents` and form-field `/V` strings whose `/Rect` meets the mark are now attribution targets. Covered by `tests/TestSep13LeadRedactionProof.cpp`.

### PGR-14 / PGR-15 · HIGH · Read-only / edit-policy enforcement bypass — **Fixed**
**`src/shell/controllers/PagesController.cpp:338`** (Delete/Rotate/InsertBlank pages) and **`src/shell/controllers/EditController.cpp:371`** (single "Replace" in the Find/Replace bar) perform document mutations with **no `EditPolicy::mutationBlocked()` check** — so a read-only or expiry-guarded document can be mutated through these paths.
- **Fix:** gate both through the shared `EditPolicy::mutationBlocked()` boundary used elsewhere.
- **@ `1991d9c1`:** fixed in both places:
  - `PagesController` gates at `:329`, `:388` and `:405`.
  - The single Replace (`EditController.cpp:538-571`) now goes through `replaceAllInDocument()`, which checks `mutationBlocked()` at `:434`.
  - The consolidation also carries the dispatch-gates line's S2-1 (reorder-panel Apply) and S2-2 (form tab-order Apply) gates. `EditPolicy.h` lists every direct route that is gated.

### PGR-16 / PGR-17 · HIGH · CSV / formula injection — **Open**
**`src/engines/ConversionManager.cpp:410` (`exportToCsv`)** and **`src/ui/CommentsWidget.cpp:752` (`csvEscapeField`)** — cells are only double-quote-escaped; a value beginning `=`, `+`, `-`, or `@` (from PDF-controlled text or a comment) becomes a live formula/DDE payload when the CSV is opened in a spreadsheet.
- **Fix:** prefix a `'` (or escape) any field starting with `= + - @` (also tab and CR, per OWASP).
- **@ `1991d9c1`:** unchanged in both places. `ConversionManager.cpp:~480` and `CommentsWidget.cpp:820-828` still only double the quotes.

### PGR-18 · HIGH · Unescaped HTML into the OCR overlay — **Open**
**`src/modes/OCRMode.cpp:1037`** — a word correction is injected unescaped into the confidence-overlay anchor's `title` attribute; a crafted correction can break out of the attribute / inject markup.
- **Fix:** HTML-escape the correction before templating.
- **@ `1991d9c1`:** still open.
  - At `:1101-1121`, only `shown` (the word text) goes through `toHtmlEscaped()`.
  - The correction arrives through `extra = tr(" | corrected to: %1").arg(rec.reviewedText)` and is placed raw inside the single-quoted `title='…'` attribute.
  - A second problem: the template is filled by chained `.arg()` calls. A `%1` … `%6` typed into a correction therefore captures the later `.arg(escaped)` substitution.
  - **Fix:** escape `extra` too, and build the anchor with a single multi-argument `.arg(a, b, …)`.

### PGR-19 · HIGH · Invalid measurement calibration leaves stale scale — **Open**
**`src/core/MeasureCore.h:353` — `scaleFrom()`** — clears the `calibrated` flag on an invalid pt-labeled calibration but leaves `unitsPerPt` at its non-1.0 persisted value (contrast the unknown-unit branch which resets to 1.0), so an "uncalibrated" scale still reports wrong measurements.
- **Fix:** reset `unitsPerPt = 1.0` on the invalid-calibration path.
- **@ `1991d9c1`:** unchanged (`:344-356`). `s.unitsPerPt = unitsPerPt` is assigned before `s.calibrated = calibrated && *unit != Unit::Pt`, so a `pt` scale keeps a factor other than 1.0.

### PGR-20 · HIGH · Legacy secret-blob DPAPI-binding gap — **Open (narrowed)**
**`src/core/EncryptedFileSecretStore.cpp:196`** — `decrypt()` still accepts legacy `0x01` AES-GCM blobs on the default Windows path, but the diff removed the DPAPI-binding step from the legacy seed derivation, weakening the protection previously provided for those blobs.
- **@ `1991d9c1`:** narrowed. SEP13:5 stopped *writing* every legacy format, and on Windows the default writer produces `0x04` only. `decrypt()` still lets `0x01` **and** `0x03` AES blobs fall through to `resolveKey()` on Windows, though. That key is a deterministic SHA-256 of the home path, `QSysInfo::machineUniqueId()` and a constant, none of which is secret.
  - The file's own format header says pre-EC04 Windows `0x01` blobs "could never" be reread, so no legitimate Windows default-path blob uses this key.
  - Its only remaining effect: anyone who can write `secrets.enc.json` can **forge an entry the store accepts** (secret injection).
  - Legacy `0x02` DPAPI blobs are also still read with no entry binding, so they can still be swapped between entries during migration.
- **Fix:**
  - On Windows with no key override, reject `0x01` and `0x03`.
  - Re-wrap `0x02` blobs as `0x04` on first read, then stop accepting `0x02`.

The remaining MEDIUM/LOW triage items are itemised with scenarios in the [findings appendix](PARITY-GLM-REVIEW-2026-09-13-FINDINGS.md). They were **not** re-verified at the PR head:
- OCR review-state re-entrancy guards at `:617` and `:707`, and the `applyWordCorrection` canvas refresh.
- Compare filter re-computation and tree ordering.
- The tolerances in conversion `deriveColumns` and `clusterIntoRows`.
- The `releaseResidentFile` password lingering.
- Key and plaintext zeroization.
- `AiOptions` ignoring `maxTokens` and `temperature`.

---

## 6. Specialist pass on the PR code (2026-09-22/23)

> The native-adversary, guarantee-verification-engine and emergence-engine definitions were run against the consolidated code. Each finding below was then confirmed by reading the code at `1991d9c1`.

### PGR-21 · CRITICAL · In-place re-sign deletes the user's only copy — **Open**
**`src/engines/SignatureManager.cpp:1323, 1619, 1800-1823`**

`replaceOutput = (inputPath != outputPath)` (`:1323`) and `signTarget = replaceOutput ? signingCandidate : outputPath` (`:1619`). An **in-place** signature (input == output) therefore writes straight into the user's document. There is no candidate and no backup; the in-place incremental append is a deliberate, tested contract. If post-validation then fails, **both** failure branches run `QFile::remove(outputPath)` when `!replaceOutput`:
- the broken-integrity branch (`:1809-1823`);
- the empty-result branch that the PGR-08 fix added (`:1800-1807`).

In in-place mode, `outputPath` *is* the user's original file. So a signature that does not re-validate, for example a malformed prior signature or a parser disagreement, **deletes the only copy of the document**, including all of its pre-existing content. The candidate-based path (`replaceOutput == true`) is safe: it drops only the candidate.

- **Fix:** make in-place signing go through a candidate as well:
  1. Copy the input bytes to a unique candidate.
  2. Append the signature there and validate the candidate.
  3. Commit the candidate over the original only on success.
  4. On failure, remove only the candidate. Never `QFile::remove(outputPath)` when `outputPath == inputPath`.
- **Test:** force a post-validation failure (seam) on an in-place sign. Assert that the original file still exists, byte-identical.

### PGR-22 · HIGH · Save-path re-seat use-after-free — **Open**
**`src/engines/podofo/PoDoFoBackend.cpp:617-638` — `saveDocument()`**

After a validated save, the resident document is re-seated from the candidate's bytes, which are held in `d->reseatBuffer`. PoDoFo's `LoadFromBuffer` keeps a *reference* to that buffer and reads objects from it lazily for the document's whole lifetime.

- **The bug:** on every save after the first, `d->reseatBuffer = candidateFile.readAll();` (`:617`) frees the bytes that the **still-resident** `d->document` parses from.
- **If the re-seat load then throws:** the catch block runs `d->reseatBuffer.clear()` and returns `false` (`:627-632`). The old `d->document` stays resident on freed memory, and the next lazy object or stream access is a **use-after-free**.
- **Fix:** read into a local `QByteArray newBuffer` and load `reseeded` from it. Only on success do `d->document = std::move(reseeded); d->reseatBuffer = std::move(newBuffer);`. On failure, leave both members untouched.
- **Test:** an ASan build with a seam that makes the second re-seat throw, followed by a page or object access.

### PGR-23 · HIGH · Redaction proof certifies nested compressed containers — **Open**
**`src/core/RedactionProof.cpp:624-647` (EmbeddedFiles) and the DecodedStreams surface (`:416+`)**

- **The gap:** each embedded file's payload gets exactly one layer of decoding (its own stream filter), then a literal byte search (`containsAny`). A secret inside a **nested** compressed container is invisible to that search. Examples: an attached PDF whose own content streams are Flate-compressed, or a ZIP/DOCX/XLSX attachment.
- **The result:** the surface reports `Clean`, and the entry certifies that "none survive on any swept surface". That is the same false-certification class as PGR-09, 10 and 13.
- **Fix:**
  - Recurse into embedded PDFs: parse each one and sweep its decoded streams, with a depth cap.
  - Report any container the sweep cannot decode (ZIP/OOXML, unknown filters, encrypted payloads) as `Unswept`, never `Clean`.
- **Test:** a document with an attached PDF that contains the secret in a Flate-compressed content stream. Assert the verdict is not PASS.

### PGR-24 · MEDIUM · Signing-candidate temp-file leak — **Fixed**
**`src/engines/SignatureManager.cpp:1326-1329`** — M3 (SEP13) added `cleanupCandidate()`, which runs on every failure exit, including the catch blocks (`:1804`, `:1819`, `:1839`). Confirmed at `1991d9c1`.

### PGR-25 · MEDIUM · Secret-store lost-update race — **Open**
**`src/core/EncryptedFileSecretStore.cpp` (write path `:319-350`, `:383-396`)** — each write reads `secrets.enc.json`, modifies the JSON and commits it with `QSaveFile`. The commit is atomic per write, but nothing locks the whole read-modify-write. If two GlyphPDF instances store different secrets at the same time, the last commit silently drops the other entry.
- **Fix:** hold a `QLockFile` (next to the store) across the read-modify-write.

### PGR-26 · LOW · Credentials persisted with `CRED_PERSIST_ENTERPRISE` — **Open**
**`src/core/CredentialManager.cpp:46`** — `cred.Persist = CRED_PERSIST_ENTERPRISE` makes Windows roam the credential to every machine the user logs on to in a domain with roaming profiles. That is wider exposure than a desktop app's API keys need.
- **Fix:** `CRED_PERSIST_LOCAL_MACHINE`.

---

## 6b. Found while verifying the consolidation (2026-09-23)

The PR was built from scratch (Release, MSYS2 UCRT64), run in full under `ctest -j6`, and the ported features were tested against real, rendered PDFs. That surfaced these defects. **Six are fixed in this PR**, each with a regression test. Line numbers refer to the consolidation commit `1991d9c1`.

| ID | Sev | Where | Defect | Status |
|----|-----|-------|--------|--------|
| PGR-27 | HIGH | `CropPageCommand`, `DeleteImageCommand`, `ReplaceImageCommand`, `EditFormFieldCommand` | **Checked redo applied the edit twice.** `CheckedHistory::redo()` performs the mutation in `applyChecked()` and arms it; each command's `redo()` must then only consume the arm. These four never consumed it. For a deleted image, the second delete fails, the command turns obsolete, and Qt drops it from the history. | **Fixed** (`fix(history)…`), with `TestCheckedMutationCoverage` counts |
| PGR-28 | MED | `PdfViewerWidget::toggleEyeCareMode` (also on `main`) | **Eye Care use-after-free.** The colorize effect was cached, but `setGraphicsEffect(nullptr)` deletes it. The second toggle-on re-installed a deleted object. | **Fixed** (`feat(viewing)…`), with `TestViewingModes` |
| PGR-29 | HIGH | `rewriteImageMatrix` (`PoDoFoBackend.cpp:~3582`) | **Image move / resize / rotate never worked on a real PDF.** The image `Do` was matched as a plain operator, but PoDoFo 1.x reports it as `DoXObject`; EC03 fixed `listImages`/`deleteImage` but not this. The commands ignore the `false` result and pushed no-op history entries. Once it did run, three more problems showed up in the replace: it deleted a same-line `q`, matched `" cm"` inside strings, and threw on an array `/Contents`. | **Fixed**: byte-exact operand replace via `gp::content` |
| PGR-30 | HIGH | `listImages` (`PoDoFoBackend.cpp:~3786`) | **Every non-symmetric image placement was reported wrong.** The six `cm` operands were read forwards, but `PdfVariantStack` indexes from the top: a 200×200 image at (100,400) came back as 412×200 at (0,200), rotated 14°. Every image edit computes from this. | **Fixed** (the convention the redaction walk already uses) |
| PGR-31 | MED | `rotateImage` | **Rotation pivoted on the wrong point.** It turned about the midpoint of the image's first edge, not its centre, so each rotation also moved the image and rotate + undo did not return it. | **Fixed**; 30° rotate + undo now returns exactly |
| PGR-32 | MED | `moveImage`, `resizeImage`, `rotateImage`, `replaceImage`, `deleteImage`, `deleteObjectAt`, `optimizeDocument` | **A refused commit could crash the app.** It threw `std::runtime_error`, which their `catch (PdfError&)` let escape through the undo commands (a failed save, e.g. a locked file). | **Fixed**: `catch (std::exception&)`; `commitMutation` has already rolled back |
| PGR-33 | LOW | `editTextInline` `Tf` parsing (`PoDoFoBackend.cpp:~1310`) | **Dead code.** The name and size are read from swapped stack slots, so the original font is never picked up. Enabling it would start using possibly-subset embedded fonts for new text, which is a behaviour change with its own risk. | **Open**: needs a design decision |
| PGR-34 | — | `TestSweepW3UxFlows` (flows 2a, 2b, 3) | The completion, preset and redaction modals were never captured, so 3 of 11 flows failed. This was **not caused by the consolidation**: a build of its source branch tip failed identically, even with the test's settings store wiped. Root cause (found in the `feat/ux-integration-fixes2` lane): stale modal-driver chains consumed the *next* slot's modals, and flow 2b read a counter that raced the queued accounting. It came with one product fix: the batch-merge completion feedback now names the merged output (F2a-F1). | **Fixed** (cherry-picked into this PR); the full suite is 180/180 |

Also fixed, test infrastructure only: the parallel suites shared one temp root, so the SafeSave "no candidate left behind" checks deleted and counted each other's files. TestFormSafety, TestEngineSave, TestEncryptedPackageSafeWrite and TestWelcomeRoutes failed at `-j6` and passed alone. Every test now gets its own `TMP`/`TEMP`/`TMPDIR`.

---

## 7. Not yet reviewed — delta after `9ba3cea`

The review snapshot was `9ba3cea`. The consolidated PR carries the line **up to its final tip**. That adds 319 more commits (266 non-merge) touching 262 source/test files (+51,970 / −1,312 lines). No review pass has read that code, apart from the spot re-reads in §3 and the §6 specialist pass. Areas that need a full review before merge:

| Area | New / changed code | Why it matters |
|------|-------------------|----------------|
| `src/engines/formjs/` — **JavaScript runtime for PDF forms** | `AFormShim.cpp` (845 lines), `FormJsRunner.cpp` (680), `FormJsSandbox.cpp` (605) | Runs document-supplied script. Sandbox escape, resource limits, re-entrancy and host-API exposure are all untested by any review. **Highest priority.** ~~untested~~ → **Reviewed 2026-09-23/24 — see §10** |
| Batch presets | `BatchPreset.cpp` (926) | Persisted, user-shareable batch definitions: parsing, path handling, destructive-operation defaults |
| Signing requests | `SigningRequestRunner.cpp` (496) | Creates signature fields in place (R26) |
| Batch | `BatchMode.cpp` (+1,081) | Destructive multi-file operations |
| PoDoFo engine | `PoDoFoBackend.cpp` (+892) | Where PGR-22 lives. The parser-facing surface grew again |

Recommended: run the review workflow (or `/code-review ultra`, which is user-triggered and billed) on `9ba3cea..1991d9c1`, with the formjs sandbox as its own lane.

---

## 8. Coverage matrix

| Subsystem lane | Reviewed (to `9ba3cea`) | Method(s) | Re-verified @ `1991d9c1` |
|----------------|----------|-----------|----|
| redaction (proof + excision) | ✅ | workflow · code-review · inline · specialist | ✅ findings |
| PoDoFo engine | ✅ | workflow · inline · specialist | ✅ findings |
| signatures / crypto | ✅ | workflow · code-review · specialist | ✅ findings |
| save / forms | ✅ | workflow · manual (R01 SafeSave) | ✅ findings |
| secret store / crypto | ✅ | workflow · specialist | ✅ findings |
| AI provider (SSRF / lifetime) | ✅ | workflow | ✅ findings |
| pages | ✅ | workflow | ✅ findings |
| OCR | ✅ | workflow | ✅ findings |
| conversion / OOXML | ✅ | workflow | ✅ findings |
| compare | ✅ | workflow | — |
| batch | ✅ | workflow · manual (G12) | ✅ findings |
| capability / measure | ✅ | workflow | ✅ findings |
| shell / architecture | ✅ | workflow | ✅ findings |
| UI | ✅ | workflow | ✅ findings |
| **forms JavaScript (formjs)** | ❌ (new after `9ba3cea`) | adversarial review lane · attack stories · 24-test adversarial suite | ✅ §10 — PGR-35..39 fixed, PGR-40/41 deferred with reasons |

"Re-verified" means the listed findings were re-read at the PR head. It does not mean the lane's later code was reviewed; see §7.

Static red-flag sweep across the reviewed 37K-line diff: **clean** (no weakened tests, swallowed exceptions, unsafe C, TLS-disabling, or shell-exec sinks introduced).

---

## 9. Caveats & operating notes

- **Single-vote tier is triage-grade.** To complete a full 14-lane × 3-vote adversarial review within the account usage cap, the verifier count was reduced to one. §5 items were re-read at the PR head; that reading is one more vote, not a second adversarial pass.
- **The line was a moving target.** Its tip advanced during the multi-day review, and the responsible session fixed findings in parallel (the batch-test flake this review found was fixed on the branch as commit G12). The status column in §3 records the state at `1991d9c1` only.
- **`main` status.** `main` did not carry this line during the review. The consolidation PR (`review/consolidated-parity` → `main`) lands it as a single squashed commit. The pre-purge history of the source branches is intentionally not republished, and the per-branch history is preserved in local archive refs and offline backups. The E-1 redaction-excision fix depends on the line's broader redaction rewrite, and it reaches `main` with that rewrite through this PR.
- **Review-tooling lesson (recorded for future runs):** a full 14-lane × 3-vote workflow exceeds this account's usage cap; use single-vote verification or split lanes across cap windows.

---

*Companion document (raw per-finding detail, all 49 candidates with reproduction scenarios): [`PARITY-GLM-REVIEW-2026-09-13-FINDINGS.md`](PARITY-GLM-REVIEW-2026-09-13-FINDINGS.md).*

---

## 10. Delta review — the formjs lane (2026-09-23/24)

The §7 highest-priority area — the never-reviewed JavaScript runtime for PDF
forms (`src/engines/formjs/`, quickjs-ng 0.15.0) — was reviewed as its own
adversarial lane on branch `feat/formjs-review` (base `8a0a8b3d`, the PR
head). Method: every checklist item attacked as an adversary would
(prototype-pollution chains, host-object reaches, budget-interaction
exploits), findings ranked by exploitability, results written as attack
stories. Full detail, attack narratives and evidence:
**[`FORMJS-THREAT-MODEL-2026-09-24.md`](FORMJS-THREAT-MODEL-2026-09-24.md)**.

New adversarial suite: `tests/TestFormJsAdversarial.cpp` — 24 tests, 0
failed, 1 disclosed skip. All prior formjs suites stay green:
`TestFormJsCalc` 49/49, `TestFormKeystroke` 9/9.

| ID | Sev | Where | Defect (attack story) | Status |
|----|-----|-------|------------------------|--------|
| PGR-35 | HIGH | `FormFieldPropertiesPanel.cpp` (6 labels + apply dialog), `FormsController.cpp` (2 import dialogs) | Document-derived text — script failure reasons and a FORMAT script's **output** — rendered as rich text in disclosure labels/dialogs: UI spoofing inside the trusted chrome, `<img>` local-file/UNC beacons. | **Fixed** (`fix(forms-security)…`, `155f3bb7`): `Qt::PlainText` on every disclosure surface; test drives the real panel offscreen through a hostile `/AA /K` rejection and `/AA /F` preview |
| PGR-36 | MED | `AFormShim.cpp` `AFSimple_Calculate` | `op in actions` walked the prototype chain — `AFSimple_Calculate('toString', …)` accepted inherited names as operations and silently wrote garbage to `/V`. | **Fixed** (`dc3240e9`): own-property membership; honest TypeError; field keeps its committed value |
| PGR-37 | MED | `AFormShim.cpp` `__gpEndEvent` | `event.value = 0/0` → JSON null with `hasValue=true` → the cascade silently **wiped the committed `/V`** at every save. | **Fixed** (`77b57a7e`): non-finite number = no usable value; committed value stands |
| PGR-38 | LOW | `FormJsSandbox.cpp` `installFieldSnapshot` | JSON embedded as a JS object literal: a field literally named `__proto__` vanished from every script's view (the literal invokes the setter; JSON.parse defines an own property). | **Fixed** (`f6e1953c`): embed via `JSON.parse` of an escaped string literal |
| PGR-39 | LOW | `AFormShim.cpp` | The shim leaked 9 internal helpers (`__printf`, `__scand`, …) as writable globals beyond the documented surface — rewireable by scripts for later cascade events. | **Fixed** (`f6e1953c`): shim body wrapped in an IIFE; surface pin tightened to exactly the documented set |
| PGR-40 | HIGH | quickjs-ng **0.15.0** (pinned dependency) | **CPU-deadline bypass:** native sparse-array scans never poll the interrupt handler and never allocate per hole — `Array(2^31).indexOf` 44.8 s, `includes` 53.2 s, `lastIndexOf` 56.6 s, `flat` 61.8 s, `sort(2^28)` 12.8 s, `join` 37 s, measured with the exact sandbox contract. One expression in any `/AA` script = unkillable UI freeze, repeatable per save/keystroke. | **Deferred — dependency bump.** Upstream added array-method interrupt checks after 0.15.0 (verified in master); MSYS2 still packages 0.15.0-1 (verified 2026-09-23). Probe harness in `.context/qjs-probe.c`; the suite's `nativeSparseArrayScansAbideTheDeadline` pin auto-arms when the package is bumped |
| PGR-41 | LOW | `FormJsRunner.cpp` cascade | Cross-event tamper window: a script ending without a committed write leaves its rewiring of the shared sandbox in place for the next event's inputs. Platform-inherent (Acrobat/pdf.js share it); proposed fix is a fresh runtime per event — design decision. | **Deferred**, characterized by a pinned test that flips deliberately |

Refuted by attack (worth recording): catastrophic regex backtracking IS
interrupt-polled in quickjs-ng 0.15.0 (all ReDoS probes abort exactly at the
deadline); no host capability is reachable through the `Function`
constructor, indirect eval, or the async-function machinery; the host never
pumps promise jobs, so async continuations never run at all; the engine
global surface is exactly the documented shim set.

Lane commits, in order: `697e7dcf` (adversarial suite) → `155f3bb7` (PGR-35)
→ `dc3240e9` (PGR-36) → `77b57a7e` (PGR-37) → `f6e1953c` (PGR-38+39) → the
threat model + this section. Every fix commit is test-backed; no test was
weakened (one characterization pin was tightened in its fix commit).
