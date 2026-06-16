# GlyphPDF — Audit Remediation Prompts (2026-06-16)

**Purpose:** Self-contained Claude Code execution prompts that close every finding from the
2026-06-16 full-codebase audit (8 parallel domain reviews: crypto/signatures, PDF object
backend, redaction/encryption, subprocess/IO/OCR, concurrency/performance, core
architecture/Djot, build/packaging/tests, UI/UX). Each prompt below is written in the house
7-H format and is **ready to paste as the first message in a fresh Claude Code session rooted
at `C:\Users\User\Projects\pdf`**. One prompt = one session. Do not add commentary on paste.

**Audit headline:** features were wired to the UI before being wired to reality, and
tests/labels assert the happy path is real when it isn't. None of the four headline claims
(true redaction, lossless Djot interchange, PAdES B-LTA, signed MSI) are fully true in the
v1.0.1 code. **Do not ship any public 1.x until AR-PROMPT-1 … AR-PROMPT-3 + AR-PROMPT-11 are
complete.**

**Severity legend:** 🔴 CRITICAL (release blocker / security / data loss / crash) ·
🟠 HIGH · 🟡 MEDIUM · ⚪ LOW.

---

## Index & dependency-aware execution order

| Prompt | Theme | Severity span | Depends on |
|---|---|---|---|
| **AR-PROMPT-1** | Safety hotfix bundle — crashes + data loss | 🔴 | none (do first) |
| **AR-PROMPT-2** | Redaction unification & completeness (flagship) | 🔴🟠 | none |
| **AR-PROMPT-3** | Signature/crypto conformance & security | 🔴🟠 | none |
| **AR-PROMPT-4** | Content-stream injection + PDF backend hardening | 🔴🟠 | none |
| **AR-PROMPT-5** | Subprocess / IO / OCR hardening | 🟠🟡 | AR-PROMPT-1 (taskkill) |
| **AR-PROMPT-6** | Concurrency & performance | 🔴🟠🟡 | AR-PROMPT-1 (prefetch UAF) |
| **AR-PROMPT-7** | Annotations-in-PDF + UI threading + close safety | 🔴🟠 | none |
| **AR-PROMPT-8** | UI truth & dead-surface cleanup | 🔴🟠🟡 | none |
| **AR-PROMPT-9** | Djot dual-model correctness + ProvenanceGuard | 🔴🟠 | none |
| **AR-PROMPT-10** | Architecture refactors (interfaces, DI, errors, secrets) | 🟠🟡 | AR-PROMPT-3,4 landed |
| **AR-PROMPT-11** | Build / release hardening + legal (signing, AGPL/GPL) | 🔴🟠 | none |
| **AR-PROMPT-12** | Test-coverage backfill + CI gates | 🟠 | the prompts whose code it tests |

**Recommended order:** 1 → (2,3,4 in parallel) → (5,6,7,8,9 in parallel) → 10 → 11 → 12.
Run 11 before any tag; run 12 last so it asserts the real, fixed behavior.

**Standing rules for every prompt (house protocol):**
- PHASE 0 reading before code: `knowledge/agent-execution-anti-patterns.md`,
  `projects/glyphpdf/08-lessons-learned.md`, `06-non-negotiables.md`, `01-current-state.md`.
- Verify tests on disk (result-file mtime newer than edits) — never trust self-report (Pattern 1/2).
- Atomic commits per deliverable. `git status` clean before each commit. **Local commits only**
  unless the user says push. Branch first if on `main`.
- Do not introduce `TODO(audit-*)`/`FIXME`; do not add "scheduled for future engine update" dialogs.
- After completion run `ctest --output-on-failure -j4 --repeat-until-fail 3` and report pass/fail honestly.

---

<!-- ===================================================================== -->
# AR-PROMPT-1 — Safety hotfix bundle (crashes + data loss)

**Paste this entire block into a fresh CC session. Estimated effort: 2-4 h. Risk: LOW.**

<session_metadata>
Phase: Audit remediation — emergency safety fixes
Priority: 🔴 BLOCKING — two deterministic crashes + one silent data-loss bug reachable in shipped v1.0.1
Depends on: nothing
Agents: /backend (primary)
Estimated context: ~25% | Risk: LOW — localized, high-confidence fixes
</session_metadata>

<role>
You are a senior C++17/Qt6 engineer. You fix crashes and data-loss bugs with the smallest correct
diff, add a regression test for each, and never expand scope.
</role>

<project_context>
GlyphPDF is a privacy-first native Windows PDF workstation (C++17/Qt6.11, MSYS2 ucrt64, PoDoFo 1.1,
PDFium). 4 static-lib layers core→engines→commands→ui. These three defects all ship in v1.0.1.
</project_context>

<objective>
Eliminate three reachable defects with minimal diffs + regression tests: (A) null-deref crash in
text watermarking, (B) cross-thread use-after-free in render prefetch on document close, (C)
unconditional force-kill of the user's running LibreOffice that destroys their unsaved work.
</objective>

<files_to_read>
src/engines/podofo/PoDoFoBackend.cpp (watermark + SearchFont call sites ~128, 744, 1007, 1036, 1354, 2997)
src/engines/RenderCache.cpp + .h (prefetch lambda ~256-300; destructor ~36)
src/engines/ConversionManager.cpp (convertOfficeToPdf ~405-486; taskkill ~431)
src/engines/AutosaveManager.cpp + .h (retry singleShot ~109-123)
src/modes/AIChatPanel.cpp (void* QListWidgetItem property ~107-128)
</files_to_read>

<deliverables>

### D1 — Watermark null-deref (🔴 crash)
**File:** `PoDoFoBackend.cpp:2997`. `addTextWatermark` dereferences `SearchFont("Helvetica")` directly;
every other call site null-checks and falls back to `GetStandard14Font`. On a doc with no loaded
Helvetica, `SearchFont` returns nullptr → crash.
**Acceptance:** guard with fallback: `const PdfFont* f = doc.GetFonts().SearchFont("Helvetica"); if (!f) f = &doc.GetFonts().GetStandard14Font(PdfStandard14FontType::Helvetica); painter.TextState.SetFont(*f, ...);`
Add a test in `tests/TestIntegration.cpp` (or new `TestWatermark`) that text-watermarks a freshly
created PDF with no fonts and asserts success (no crash).

### D2 — Render-prefetch use-after-free (🔴 crash)
**File:** `RenderCache.cpp:277,297`. The prefetch lambda captures `renderer` (an `IPdfRenderer*`) by raw
pointer and calls `renderer->renderPage()` on a pool thread; closing/replacing the document frees the
backend mid-render. `weak_from_this()` only protects the cache, not the renderer.
**Acceptance:** make document-close cancel + join prefetch before the renderer is torn down
(`m_prefetchCancelToken++; if (m_prefetchFuture.isRunning()) m_prefetchFuture.waitForFinished();`) and/or
hold the renderer via `weak_ptr`/`shared_ptr` so the lambda can re-check liveness past the token check
at line 300. Document the ownership contract in a comment. Add a stress test that starts a prefetch then
clears the cache/renderer and asserts no crash under `--repeat-until-fail`.

### D3 — LibreOffice force-kill data loss (🔴 data loss)
**File:** `ConversionManager.cpp:431-433`. Every Office→PDF conversion runs
`taskkill /F /IM soffice.bin /IM soffice.exe`, killing ALL the user's LibreOffice instances (and their
unsaved work) before launching its own. This became reachable in shipped builds when the
`#ifndef HAS_LIBREOFFICE` guard was removed in v1.0.1.
**Acceptance:** remove the blanket pre-kill. Launch soffice with a private profile so there is never a
shared lock to clear: add `--env:UserInstallation=file:///<temp>/glyphpdf-soffice-<pid>` (use
`TempFileManager` for the dir). Keep only the timeout-path `taskkill /F /T /PID <ownpid>` (already
correct at ~460). Verify Office import still works when LibreOffice is present.

### D4 — Autosave retry UAF (🔴 latent crash)
**File:** `AutosaveManager.cpp:109-123`. On rename failure the watcher schedules a 250 ms
`QTimer::singleShot(..., this, lambda)` capturing `this`/`m_document` with no `QPointer` guard; closing
the doc within 250 ms → UAF.
**Acceptance:** route the retry through a member `QTimer` (child of `this`, auto-cancelled on destruction)
or guard with `QPointer`. Make `m_saving` `std::atomic<bool>`. Add/extend a `TestAutosave` case for the
rename-fail-then-destroy window.

### D5 — AIChatPanel void* UAF (🟠)
**File:** `AIChatPanel.cpp:107-128`. A `QListWidgetItem*` is smuggled through a dynamic `void*` property;
list clear / document switch / a second send racing `onAiFinished` dangles it.
**Acceptance:** replace with a typed member pointer (or `QPointer<QListWidgetItem>`), disable the input
(both button AND `QLineEdit::returnPressed`) while a request is in flight, and clear/guard on document change.
</deliverables>

<constraints>
- Smallest correct diff per fix; no opportunistic refactors (those are AR-PROMPT-6/8/10).
- Each deliverable = its own atomic commit with a regression test.
- Do not push; local commits only.
</constraints>

<success_criteria>
- [ ] D1-D5 fixed with the exact mechanisms above; each has a regression test that fails pre-fix, passes post-fix
- [ ] `ctest --output-on-failure -j4 --repeat-until-fail 3` fully green (note any pre-existing skips)
- [ ] No new `TODO(audit-*)`/`FIXME`; `git status` clean; 5 atomic commits
- [ ] Final report (<200 words): what was reachable, how each was triggered, test added
</success_criteria>

---
<!-- ===================================================================== -->
# AR-PROMPT-2 — Redaction unification & completeness (flagship)

**Paste this entire block into a fresh CC session. Estimated effort: 2-4 days. Risk: HIGH (security feature).**

<session_metadata>
Phase: Audit remediation — flagship security feature
Priority: 🔴 BLOCKING — redaction can leak secrets that the product's headline promises to remove
Depends on: nothing (but coordinate with AR-PROMPT-4 on the save chokepoint)
Agents: /security (primary), /backend
Estimated context: ~55% | Risk: HIGH — get every leak path; verify on disk, not by self-report
</session_metadata>

<role>
You are a senior document-security engineer who has read NSA "Redacting with Confidence" and the
Edact-Ray glyph-advance literature. You assume any un-excised byte is recoverable and you prove removal
by re-opening the output and scanning every decoded stream AND every dictionary value.
</role>

<project_context>
GlyphPDF positions on "Edact-Ray-defended TRUE redaction" (content excised from the content stream, never
black rectangles) — a §6 non-negotiable. The audit found TWO redaction implementations of very different
quality, and the UI can invoke the weak one.
</project_context>

<current_state>
- Strong path: `PoDoFoBackend::applyRedactions` (`:1558`) + `redactCanvasRecursively` — real content-stream
  surgery, CTM-aware IMAGE excision, SMask neutralize, Form-XObject recursion, Tr==3 OCR scrub, numeric-TJ
  Edact-Ray normalization, struct-tree MCID cleanup, signed-doc refusal.
- Weak path: `PdfPageOps::applyRedactionsToFile` (`:109`), reached via `PdfViewerWidget::applyRedactions`
  (`:900`) — whitespace tokenization, no XObject recursion, images pass through, falls back to a black box
  and returns `true`.
- `sanitizeDocument` (`PoDoFoBackend.cpp:1883`) strips Info/XMP/EmbeddedFiles/JavaScript/Outlines/OCG/Thumb
  but is NEVER called from any redaction path.
</current_state>

<objective>
Collapse to ONE trustworthy redaction pipeline, fix the text-CTM blind spot, chain document-level sanitize,
force redaction to a new file (never overwrite the only copy), fix the silent page-range widening, and add a
post-save verification gate that re-opens the output and asserts secret strings/pixels are gone everywhere.
</objective>

<files_to_read>
src/engines/podofo/PoDoFoBackend.cpp (applyRedactions 1558+, isIntersectingSpan 1196, CTM tracking 1208-1224, text cursor 1403, numeric-TJ 1428-1446, sanitizeDocument 1883)
src/engines/podofo/PdfPageOps.cpp/.h (applyRedactionsToFile 109)
src/engines/PdfEditorEngine.cpp (applyPatternRedactions 1144-1202; redaction gate 1119-1142)
src/engines/PatternRedactor.cpp/.h (findMatches, mergeCharBoxes 178; namedPattern 60)
src/ui/PdfViewerWidget.cpp (applyRedactions 900-938; redactAllMatches)
src/modes/RedactMode.cpp (page-range parser 285-302; onApplyRedactions 379-387)
src/shell/controllers/SecurityController.cpp (redaction confirm copy 425-427)
tests/TestRedaction.cpp (testRedactionOnSignedDocIsBlocked 619 — currently NOT a slot)
</files_to_read>

<deliverables>

### D1 — Delete the weak path (🔴)
Remove `PdfPageOps::applyRedactionsToFile` and `PdfViewerWidget::applyRedactions`/`redactAllMatches`; route
all UI redaction (RedactMode, FindBar "Redact All") through `PoDoFoBackend::applyRedactions`. No code path may
return success after only painting a rectangle.

### D2 — Text CTM fix (🔴 leak)
At `PoDoFoBackend.cpp:1403`, transform the text cursor `(textX, textY)` and `(textX+totalAdvance, textY)`
through the current CTM × text matrix before the intersection test — mirror the image path (1208-1224). Add a
fixture with body text drawn under `2 0 0 2 0 0 cm` and assert the glyphs are gone from the decoded stream.

### D3 — Chain sanitize + GC save (🔴 leak)
After a successful content redaction, run `sanitizeDocument`'s object-graph scrub (Info, XMP `/Metadata`,
`/Names/EmbeddedFiles`, `/Names/JavaScript`, `/OpenAction`, `/Outlines`, `/OCProperties`, per-page `/Thumb`,
`/PieceInfo`) over the in-memory doc, then save with PoDoFo's garbage-collection/clean option so orphaned
objects (e.g. removed-annotation `/AP` streams) are not serialized. All-or-nothing.

### D4 — Redact to a new file (🔴 data loss)
`applyPatternRedactions`/RedactMode must write to a new file (default `{stem}_redacted.pdf`), keep the original,
and swap only on full success. On any per-page failure, reload/snapshot-restore the in-memory doc before
returning false (no half-redacted in-memory state). Fix `RedactMode.cpp:379-387` to stop claiming "not modified"
after a partial mutation.

### D5 — Page-range parser (🔴 scope)
Replace the parser at `RedactMode.cpp:285-302` (which falls through to `{-1,-1}` = ALL PAGES on any input it
can't parse) with PagesMode's comma-list parser. Invalid input is a hard error, never "all pages".

### D6 — Pattern match coverage (🟠)
`PatternRedactor::mergeCharBoxes` (`:178`) skips null char boxes, under-covering matches. If any char in a match
lacks a box, fall back to a conservative full-line-height rect spanning the match, or fail the match loudly.

### D7 — Post-redaction verification gate (🔴 trust)
Add a verification pass: re-open the saved output and assert the redacted strings are absent from every decoded
content stream AND every dictionary value (Info/XMP/embedded files/bookmarks), and redacted image regions are
blanked. Extend `testRedactedTextUnextractable` accordingly. Move `testRedactionOnSignedDocIsBlocked`
(`tests/TestRedaction.cpp:619`) into `private slots:` so it actually runs.

### D8 — Reconcile the confirm copy (🔴 trust)
`SecurityController.cpp:425-427` tells users the tool "cannot guarantee secure removal … use a dedicated
redaction tool." Once D1-D7 land, replace with copy that accurately describes the real Edact-Ray pipeline
(or, if a gap remains, treat it as a release blocker, not a disclaimer).
</deliverables>

<constraints>
- Redaction MUST be a full rewrite, never `WriteUpdate` (excised bytes must not survive in incremental history).
- Keep the Edact-Ray numeric-`[ N ] TJ` normalization (do not regress to per-glyph advances).
- Do not weaken the signed-doc refusal (ER-2 guard).
- Verify removal on disk; do not trust in-memory state or self-report.
</constraints>

<success_criteria>
- [ ] Exactly one redaction code path; weak path deleted; no path returns success on visual-only output
- [ ] Transformed text, images, OCR Tr==3, AND document-level hidden data all excised — proven by re-opening output
- [ ] Redaction writes a new file; original preserved; partial failure leaves no half-redacted state
- [ ] Page-range parse failure is an error, never all-pages
- [ ] `testRedactionOnSignedDocIsBlocked` runs (is a slot) and passes; verification test scans streams + dicts
- [ ] ctest green incl. new fixtures; atomic commits per deliverable; confirm copy matches reality
</success_criteria>

---
<!-- ===================================================================== -->
# AR-PROMPT-3 — Signature / crypto conformance & security

**Paste this entire block into a fresh CC session. Estimated effort: 3-5 days. Risk: HIGH.**

<session_metadata>
Phase: Audit remediation — PAdES correctness + crypto security
Priority: 🔴 BLOCKING — a shipped OCSP-verification bypass + B-T signatures that aren't timestamped + a signature-spoofing bypass
Depends on: nothing
Agents: /security (primary)
Estimated context: ~60% | Risk: HIGH — spec-conformance; add post-condition re-validation
</session_metadata>

<role>
You are a PKI/PAdES engineer fluent in ETSI EN 319 122/132, RFC 3161/6960, and OpenSSL CMS/OCSP/X509 memory
rules. You never ship a test hook in a production code path.
</role>

<project_context>
`SignatureManager.cpp` (~1785 lines) does P12 loading, OCSP/TSA networking, PoDoFo PDF surgery, DSS/VRI
construction, and a ~500-line validation state machine. Headline claim: local PAdES B-LTA.
</project_context>

<deliverables>

### D1 — Remove the OCSP verification bypass (🔴 security; §6 non-negotiable)
`SignatureManager.cpp:1111-1120` sets `verifyOk = true` for any cert whose filename contains `revoked`/`_cert`
and a local `.der` exists; `fetchOcspResponse` (`:314-336`) loads arbitrary local DER next to the cert.
**Fix:** gate BOTH the local-DER loading and the verify bypass behind a compile-time `#ifdef GLYPHPDF_TESTING`
(or the existing `setTrustStoreForTest` injection). Production must never embed an unverified OCSP response.

### D2 — OCSP responder trust + freshness (🔴/🟠)
`OCSP_basic_verify(basic, certs, store, 0)` (`:1110`) is passed the signer's own embedded chain as the responder
pool and asserts no delegation constraint. **Fix:** verify the responder is the issuer or an `id-kp-OCSPSigning`
delegate of the issuer; pass the actual issuer cert. Add `thisUpdate/nextUpdate` freshness via
`OCSP_check_validity` and reject expired responses (`:1717-1737`). Complete the `TODO M5` certID full match
(issuer-hash + serial, not serial-only) at `:604-606, 704-708, 1720-1722`.

### D3 — B-T signatures must actually carry a signature timestamp (🟠 conformance)
`:1002-1012` fetches an RFC-3161 token, logs it, and discards it — and hashes it over the cert DER, not the CMS
`signatureValue`. **Fix:** embed the token as the SignerInfo unsigned attribute
`id-aa-signatureTimeStampToken` computed over the signature value. If PoDoFo's `PdfSignerCms` can't add it,
post-process the CMS. Until embedded, do not label output "B-T".

### D4 — Shadow / incremental-save-attack detection (🔴 spoofing)
`:1358-1376` treats any appended revision merely *containing the bytes* `/DSS`/`/ByteRange` as a benign LTV
update. **Fix:** parse the appended revision's xref/trailer and diff the object graph; only a new DSS dict /
DocTimeStamp field is benign. Substring matching is not a security control.

### D5 — ByteRange hole + DocTimeStamp size (🟠)
`:1335-1352` only checks `off1==0`. **Fix:** assert the excluded gap `[off1+len1, off2)` is exactly the
`/Contents` placeholder and `off2` immediately follows it; reject holes elsewhere. At `:558-578`, hard-fail if
the TSA token exceeds the 32 KB `/Contents` reservation (no silent truncation of `/DocTimeStamp`).

### D6 — Post-condition re-validation after DSS/timestamp append (🟠)
After `buildDssDictionary` (`:404-509`) and `addDocTimestamp` (`:529`), re-run `validateSignatures` and assert
the prior approval signature is still integrity-intact; fail the op otherwise. Use ONE shared raw-`/Contents`
extraction primitive for both signing and validation (today duplicated/divergent at `:809-817, 1417-1426`),
and add a unit test asserting the SHA-1 VRI key matches an independent reference.

### D7 — Surface partial outcome + signing time (🟡 truth)
`SignDocumentHelper` drops `lastSignOutcome()` — surface `PartialLtvMissing` to the UI instead of reporting
total success/failure. Populate `SignatureInfo::date` from parsed signing time / DocTimeStamp genTime (today
shown but never set). Badge B-LT only when revocation info is actually present (not merely because a DSS exists).

### D8 — Off-thread signing (🟡)
`httpPost` (`:210-233`) runs a nested `QEventLoop` on the UI thread during signing → reentrancy/UAF risk if the
user re-clicks or closes the doc. Move signing + its TSA/OCSP calls off the UI thread; null-check `reply`;
enforce HTTPS for TSA/OCSP URLs.
</deliverables>

<constraints>
- SHA-256 only for hashing (SHA-1 confined to the VRI key per ETSI). RSA ≥ 2048 enforced (already present — keep).
- qpdf must never enter the signing path. Incremental DSS/timestamp via `SaveUpdate`/`WriteUpdate` only.
- All test hooks compiled out of production via `#ifdef GLYPHPDF_TESTING`.
</constraints>

<success_criteria>
- [ ] No production path can embed an unverified OCSP response; bypass + local-DER load are test-only
- [ ] OCSP responder delegation + freshness validated; certID full match implemented
- [ ] B-T embeds id-aa-signatureTimeStampToken over the signature value (or label downgraded)
- [ ] Shadow-attack detection parses the revision; ByteRange holes rejected; oversized TSA token hard-fails
- [ ] Prior signature re-validated after DSS/TS append; one shared /Contents extractor; VRI-key unit test
- [ ] Partial outcome + signing date surfaced; signing runs off the UI thread; ctest green incl. adversarial fixtures
</success_criteria>

---
<!-- ===================================================================== -->
# AR-PROMPT-4 — Content-stream injection + PDF backend hardening

**Paste this entire block into a fresh CC session. Estimated effort: 2-4 days. Risk: MEDIUM-HIGH.**

<session_metadata>
Phase: Audit remediation — PDF object-graph safety
Priority: 🔴 active content-stream injection on the OCR/MRC path + signature-invalidating saves
Depends on: nothing (coordinate save-chokepoint with AR-PROMPT-2/3)
Agents: /backend (primary)
Estimated context: ~55% | Risk: MEDIUM-HIGH
</session_metadata>

<role>
You are a senior PDF-internals engineer fluent in the PoDoFo 1.1 object model and content-stream tokenization.
You never write user strings into a content stream without escaping, and you funnel all persistence through one
signature-aware save.
</role>

<deliverables>

### D1 — Content-stream injection on MRC/OCR sandwich text (🔴; §6)
`PdfEditorEngine.cpp:574-583` hand-rolls escaping for only `\ ( )` then writes raw `toUtf8()`. Attacker-controlled
OCR text (NUL/newline/unbalanced parens) corrupts/injects the stream.
**Fix:** `cs += "(" + QByteArray::fromStdString(pdfEscapeLiteralString(w.text)) + ") Tj\n";` Audit every other
content-stream write site for the same pattern.

### D2 — One signature-aware save chokepoint (🟠; §6)
~13 mutators call `doc.Save()` directly (rotatePage:572, cropPage:944, resizePage:963, reorderPages:992,
insertPageFromBytes:618, deletePage:633, insertBlankPage:646, moveImage:2340, resizeImage:2368, rotateImage:2403,
replaceImage:2462, deleteImage:2506, deleteObjectAt:787), bypassing `writeUpdate`'s signature logic → silently
invalidate signatures on signed PDFs.
**Fix:** route every persistence through one method that detects signatures and uses `WriteUpdate`, or refuses
the op on signed docs (as redaction does). Also fix `resolveDocument` (`:158`) so it never loads a divergent
second copy from disk and saves that.

### D3 — Integer overflow in image buffers (🟠)
`width*height*3` is computed in `int` before any cap. `addImageWatermark` (`:3083`) has NO dimension cap.
**Fix:** apply the existing 10000-px cap to `addImageWatermark`; compute the product in `qint64` everywhere
(also `replaceImage:2446`, `optimizeDocument:3366`).

### D4 — Replace raw-string content-stream surgery (🟡 corruption)
`rewriteImageMatrix` (`:2169-2212`) and `deleteImage` (`:2472-2514`) use `std::string::find`/`rfind` heuristics
(`/Im1` matches inside `/Im12`; wrong `cm`/`q` grabbed).
**Fix:** use the operator-tokenizing `PdfContentStreamReader` path (as `listImages`/`redactCanvasRecursively`
already do), matching the XObject name token exactly.

### D5 — PdfStringEscape round-trip + double-escape (🟠 data loss)
`PdfStringEscape.cpp` idempotency heuristic (`:15-23`) makes escape/unescape lossy for input containing a
backslash-letter pair; `writeDjotPieceInfo` (`PoDoFoBackend.cpp:2587`) pre-escapes then wraps in `PdfString`
(double escaping).
**Fix:** store raw UTF-8 in `PdfString` (which escapes on write); drop the idempotency heuristic or store Djot in
a stream object. Add a round-trip test over backslash/control-byte payloads.

### D6 — Release-mode error visibility + crafted-PDF robustness (🟡)
Many catch blocks log only under `#ifdef QT_DEBUG` and return false silently (`metadata` 254, `setMetadata`
285-292 bare catch, plus image ops). Log `qWarning`/`qCritical` unconditionally. `extractAnnotations` (`:2851`)
calls `GetReal()` on `/Rect` entries without a type check → one malformed annotation aborts all extraction; guard
with `IsNumberOrReal()` and skip the bad annotation. `optimizeDocument` dedup (`:3405-3417`) is a no-op returning
success — either implement reference rewriting or remove it from options/estimate.
</deliverables>

<constraints>
- Never write user strings into content streams without `pdfEscapeLiteralString`.
- Don't change the redaction tokenizer behavior relied on by AR-PROMPT-2; coordinate the shared save chokepoint.
</constraints>

<success_criteria>
- [ ] No unescaped user string reaches any content stream; injection test (NUL/newline/parens) passes
- [ ] Every mutator persists through one signature-aware save; signed-doc edits no longer invalidate signatures
- [ ] Image dimension math is qint64 + capped everywhere; no overflow
- [ ] Image matrix/delete use the tokenizer; PdfStringEscape round-trip is lossless; no double-escape
- [ ] Release builds log failures; crafted /Rect doesn't abort extraction; optimize dedup is real or removed
- [ ] ctest green; atomic commits
</success_criteria>

---
<!-- ===================================================================== -->
# AR-PROMPT-5 — Subprocess / IO / OCR hardening

**Paste this entire block into a fresh CC session. Estimated effort: 2-3 days. Risk: MEDIUM.**

<session_metadata>
Phase: Audit remediation — process/IO/supply-chain safety
Priority: 🟠 HIGH — .bat injection, false-success exporters, unverified model downloads
Depends on: AR-PROMPT-1 (soffice taskkill already removed there)
Agents: /backend, /devops
Estimated context: ~50% | Risk: MEDIUM
</session_metadata>

<deliverables>

### D1 — Safe .bat / external-tool invocation (🟠 injection)
`VeraPdfValidator.cpp:79` (and any `.bat`/`.cmd`) — QProcess can't run batch files directly via CreateProcess, and
CMD re-quotes the user PDF path (`report & x.pdf`). **Fix:** prefer the extension-less launcher or the underlying
`java -jar`; if a `.bat` must be used, invoke `cmd /c call "<bat>"` with rigorous quoting and reject shell
metacharacters in the path. Also check `exitCode()` and distinguish "validator error" from "PDF invalid"; add
`waitForStarted` so "didn't start" ≠ "timed out".

### D2 — Exporters must verify bytes hit disk (🟠 false success)
`ConversionManager` `exportToWord/Excel/Csv/PowerPoint/Text/Html` and `convertImagesToPdf` return `true` without
checking `QTextStream::status()`/`QFile::error()`/`zip_close()` return / non-empty output. **Fix:** verify write
success + non-empty output before returning true; propagate `zip_close` failure. (BatchMode trusts these returns.)

### D3 — Verify OCR model/language downloads (🟠 supply chain)
`OcrEngine.cpp:48-118` writes/loads downloaded `.traineddata` with only size bounds — no hash. **Fix:** ship a
manifest of expected SHA-256 per language and verify before write/load; or make network download opt-in and rely
on bundled tessdata. Add an `element_count == product(shape)` check before indexing ONNX output tensors
(`PpOcrDecoder.cpp:394-404, 514-520`); clamp `recognizeCrop` `rw` (`:487`).

### D4 — Harden the temp directory (🟡)
`TempFileManager` uses a fixed predictable `%TEMP%/GlyphPDF` shared dir (temp-squatting; sensitive PDFs + the
downloaded MSI live there). **Fix:** per-session randomized subdir with restrictive perms; scope stale-cleanup to
GlyphPDF's own `glyph_`-prefixed entries only.

### D5 — Verify MSI publisher before msiexec (🟠)
`UpdateChecker.cpp:266-299` trusts the manifest's SHA-256 (rooted only in TLS) and runs `msiexec` with no
Authenticode check. **Fix:** verify the downloaded MSI's Authenticode signature/cert subject against a pinned
"GlyphPDF" publisher before launching, independent of the manifest; re-verify the file hash at apply time
(close the download→apply TOCTOU).

### D6 — Office import dimension cap + thread guard (🟡)
Cap `convertImagesToPdf` image dimensions like OcrEngine (10000 px). Move the main-thread guard from
`OcrEngine::initialize` onto the recognition entry points (`processImage`/`getRawText`). Clean up the stray
developer-monologue comments in `exportToHtml` (`:286-304`) and its O(pages) double-load.
</deliverables>

<success_criteria>
- [ ] No `.bat`/CMD-quoting injection; veraPDF distinguishes tool-error from invalid-PDF
- [ ] Exporters verify write + non-empty output; BatchMode no longer reports green over corrupt files
- [ ] OCR downloads hash-verified (or opt-in); ONNX tensor bounds checked; temp dir per-session + private
- [ ] MSI Authenticode publisher verified before msiexec; apply-time hash re-check
- [ ] ctest green (external-tool suites skip cleanly when tool absent); atomic commits
</success_criteria>

---
<!-- ===================================================================== -->
# AR-PROMPT-6 — Concurrency & performance

**Paste this entire block into a fresh CC session. Estimated effort: 3-5 days. Risk: HIGH.**

<session_metadata>
Phase: Audit remediation — concurrency correctness + real parallelism
Priority: 🔴 prefetch UAF (if not already fixed in AR-PROMPT-1) + 🟠 fake parallelism + latent pipeline deadlock
Depends on: AR-PROMPT-1 (D2 prefetch cancel/join landed)
Agents: /backend (primary)
Estimated context: ~60% | Risk: HIGH — interleavings; test under --repeat-until-fail
</session_metadata>

<deliverables>

### D1 — Real render parallelism (🔴 architecture)
`PdfiumBackend` serializes every render behind one `m_mutex` (`:89,128,183`), so LaneScheduler/CrossPagePipeline/
prefetch are fake parallelism and background prefetch can stall foreground UI. **Fix:** use a pool of
`PdfiumBackend` instances (one document per worker) for concurrent render/extract, OR explicitly document render
as serial and give background prefetch lower priority so it can't starve foreground. Pick one and justify.

### D2 — O(1) LRU + consistent hash/equality (🟠 perf + correctness)
`RenderCache` LRU is O(n) per hit via `QList::removeOne`+`prepend` (`:157,180,237,248`) → O(n²) scrolling; hashing
uses exact double bits while equality uses `qFuzzyCompare` (`RenderCache.h:32-55`), violating the hash invariant
(duplicate tiles, missed lookups), and XOR-combined hashing collides. **Fix:** intrusive LRU (`std::list` +
`QHash<key,{value,iterator}>`); quantize scale/subRect to a grid and hash/compare the quantized integers via
`qHashMulti`.

### D3 — Remove block-a-pool-thread waits (🟠 latent deadlock)
`PipelineStage.h:60-67` stage2 and `LayoutEnsemble::detect` (`:198-220`) block a CPU-pool thread on
`waitForFinished()` for GPU-lane results; with `setLaneCapacity(CPU,2)` and backpressure 4 the pool deadlocks.
**Fix:** continuation-chaining (futures driving futures, `.then(...).unwrap()`) instead of blocking a worker;
assert/clamp `backpressure < cpuPool.maxThreadCount()`. Don't have a CPU-lane task synchronously fan out to and
block on the GPU lane.

### D4 — pageSize broken-promise + closure lifetimes (🟡)
`RenderCache.cpp:75-115` — if the fulfilling thread throws, the page's future stays broken forever, poisoning that
page's metadata cache. **Fix:** try/catch the fulfillment, set a default on failure, erase the entry under the
write lock. Audit every async closure to hold weak/shared refs to ALL captured objects, not just the primary
(systemic root cause of the UAFs).

### D5 — Hot-path + MRC perf (🟡)
Throttle `checkMemoryPressure()` (`:129-131`) instead of a syscall per `getOrRender`. Replace MRC per-pixel
`pixSetPixel` (`MrcPageProcessor.cpp:335-344`) with packed-row writes via `pixGetData`/`pixGetWpl` (or
`pixThresholdToBinary`). Make `setLaneCapacity(GPU,...)` (`LaneScheduler.cpp:91`) warn/assert instead of silently
no-op. Delete the dead `OrderedResultQueue` template or actually route through it; document the disjoint-index
write contract in `OcrPipeline.cpp:347`.
</deliverables>

<constraints>
- Keep the persistent GPU warm worker (never spawn-per-page) and the dedicated CPU pool (not the global pool).
- Verify race fixes under `ctest --repeat-until-fail 5`; add TSan/helgrind notes if feasible.
</constraints>

<success_criteria>
- [ ] Render concurrency is real (instance pool) or honestly documented + prefetch deprioritized
- [ ] RenderCache LRU is O(1) with consistent hash/equality; no duplicate-tile waste
- [ ] No CPU-pool thread blocks on another lane; backpressure < cpu capacity enforced; no deadlock at low capacity
- [ ] No broken-promise cache poisoning; closures hold weak/shared refs to all captures
- [ ] Memory-pressure throttled; MRC packs rows; dead code removed; ctest green under repeat
</success_criteria>

---
<!-- ===================================================================== -->
# AR-PROMPT-7 — Annotations-in-PDF + UI threading + close safety

**Paste this entire block into a fresh CC session. Estimated effort: 2-4 days. Risk: MEDIUM-HIGH.**

<session_metadata>
Phase: Audit remediation — data fidelity + UI responsiveness
Priority: 🔴 annotations never written to PDF + 🔴 UI-thread freezes + 🔴 quit-loses-work
Depends on: nothing
Agents: /frontend (primary), /backend
Estimated context: ~50% | Risk: MEDIUM-HIGH
</session_metadata>

<deliverables>

### D1 — Embed annotations into the saved PDF (🔴 data fidelity)
`HomeController::onSave` writes a `.ann` JSON sidecar but never calls `IPdfEditorEngine::embedAnnotations()`
(only a comment references it, `CommentsWidget.cpp:404`). Every highlight/comment/shape/signature is invisible in
other readers and lost if the sidecar is separated. **Fix:** route Save / Save As through `embedAnnotations` so
annotations are written into the PDF; keep the sidecar only as an editable-source cache. Add a test: annotate →
save → re-open with the engine → assert annotations present in the PDF object graph.

### D2 — Get long ops off the UI thread (🔴 freeze)
veraPDF (`PdfAValidationPanel::runValidation`) and signature validation (`ModeStrip.cpp:117-163`) run synchronously
on the GUI thread; PagesMode derives page count via ~12 synchronous `extractPageAsBytes` calls (`:462-491`) and
split runs N×extract+insert without pumping the loop (`:687-754`). **Fix:** move all to `QtConcurrent`/`QThread`
workers with `QFutureWatcher`; use the real `pageCount()`; cache validation/signature results; show busy progress.

### D3 — Cancelable progress everywhere (🟠)
SecurityController builds `QProgressDialog(..., QString(), ...)` (no cancel button) for sign/encrypt/redact/sanitize/
certify/timestamp; encrypt/permissions never `show()` the dialog. **Fix:** real cooperative cancel (kill the
subprocess / abort the worker); call `show()` uniformly; reword internal jargon ("Applying redactions
asynchronously…").

### D4 — Save-on-exit (🔴 data loss)
No `closeEvent` exists. **Fix:** override `GpMainWindow::closeEvent`; when `DocumentSession::isDirty`, prompt
Save / Discard / Cancel. Add a test or manual-verify note.

### D5 — setOverlayImage dead code (🟡)
`PdfViewerWidget::setOverlayImage` (`:1073`) stores an image and calls `update()` but there is no `paintEvent`, so
it never draws. **Fix:** implement painting or remove the API and its callers.
</deliverables>

<success_criteria>
- [ ] Annotations are embedded in the PDF on Save/Save As (verified by re-opening); sidecar is cache-only
- [ ] veraPDF, signature validation, page-count, split all run off the UI thread with progress + cancel
- [ ] Every long security op is cancelable; progress dialogs shown consistently
- [ ] Quitting with unsaved changes prompts Save/Discard/Cancel
- [ ] No dead overlay API; ctest green; atomic commits
</success_criteria>

---
<!-- ===================================================================== -->
# AR-PROMPT-8 — UI truth & dead-surface cleanup

**Paste this entire block into a fresh CC session. Estimated effort: 2-4 days. Risk: MEDIUM.**

<session_metadata>
Phase: Audit remediation — "every surface works or isn't shown" (SCOPE LOCK §5)
Priority: 🔴 mock content presented as real + misleading labels + ~70 dead "future release" buttons
Depends on: nothing
Agents: /frontend (primary)
Estimated context: ~55% | Risk: MEDIUM
</session_metadata>

<deliverables>

### D1 — Remove fake/mock content presented as real (🔴; Pattern 5)
`OCRMode.cpp:241-347` seeds hardcoded fake results ("$2,418M / 14.2% / ROVER 64%", static 12-item page list);
`CompareMode.cpp:31` hardcodes "Q4-Report-v1↔v2". **Fix:** start empty ("Run OCR to begin"); populate from the real
document; set compare labels from the actual files.

### D2 — Fix misleading labels (🔴; Pattern 6)
`ModeStrip.cpp:132-144` shows "● AUTOSAVED · <time>" on dirty docs and prints `QTime::currentTime()` as a save time
when none exists. **Fix:** dirty → "● UNSAVED (autosaved hh:mm:ss)"; never print current time as a save time.
Single-source the version string (MenuBar.cpp:61 hardcodes "1.0.1" vs `UpdateChecker::currentVersion()`).

### D3 — Hide planned tools; wire or remove dead forms (🔴; SCOPE LOCK)
RibbonModel/Ribbon render ~70 tools permanently disabled with "Planned for a future release" (= preview banners
the scope-lock forbids). **Fix:** hide planned tools until implemented. Wire or remove: SignaturesPanel decorative
fields (`:90-107`), PagesMode 9-button toolbar with no connections (`:151-167`), PdfAValidationPanel inert
Fix/Convert/Jump buttons, CompareMode "Close Compare" / RedactMode "Cancel" trapped-mode buttons.

### D4 — Destructive-action safety (🟠 data loss)
FindBar "Redact All" (`:75-77,116-120`) is one-click destructive with no confirm and a search-not-complete race;
BatchMode overwrites existing outputs without confirm for ≥2 files (`:643-646`); EncryptionDialog has no
confirm-password field and no "lost password = unrecoverable" warning. **Fix:** add confirmations / ensure search
completion / confirm-password + warning; disable the permissions group until an owner password is entered.

### D5 — Real thumbnails + atomic reorder (🟠)
PagesMode thumbnails are blank gray squares (`:510-511`); reorder applies N separate `reorderPages` calls (partial-
failure risk, not one undo). **Fix:** render real thumbnails; apply one permutation atomically as a single undo command.

### D6 — Privacy default + a11y/RTL/DPI pass (🟡)
`update/checkOnStartup` defaults ON (`PreferencesDialog.cpp:105`) with no first-run consent → default OFF or add a
first-run opt-in. Replace fixed-px / 9-10px fonts with layout-driven point sizing; ship + honor the RTL toggle
(currently itself disabled) and use leading/trailing alignment; add `setAccessibleName`/tooltips/mnemonics to
icon-only and ribbon controls. Use `QDialogButtonBox` for consistent button order across dialogs.
</deliverables>

<constraints>
- Telemetry stays absent (verified clean) — do not add any. Outbound calls limited to update check (opt-in), signing OCSP/TSA, local OCR/Ollama.
- "Every surface either works or is not shown" — no new disabled-with-tooltip placeholders.
</constraints>

<success_criteria>
- [ ] No fake OCR/Compare content; labels reflect real state; one version source
- [ ] No "future release" disabled buttons; every visible control is wired or removed
- [ ] Redact All / batch overwrite / encryption have confirmations + warnings; real thumbnails; atomic reorder
- [ ] Update check opt-in; a11y names/tooltips/mnemonics added; RTL shipped; DPI-safe sizing; ctest green
</success_criteria>

---
<!-- ===================================================================== -->
# AR-PROMPT-9 — Djot dual-model correctness + ProvenanceGuard

**Paste this entire block into a fresh CC session. Estimated effort: 4-6 days. Risk: HIGH.**

<session_metadata>
Phase: Audit remediation — WS2 headline interchange claim
Priority: 🔴 "lossless Djot↔Semantic" is unimplemented (decode is a stub) + ProvenanceGuard is decorative
Depends on: nothing
Agents: /backend (primary)
Estimated context: ~60% | Risk: HIGH — the product's differentiation depends on these being true
</session_metadata>

<deliverables>

### D1 — Implement Djot decode (🔴; Pattern 5)
`LuaDjotCodec::djotToDocument` (`:264-316`) parses then discards the AST and returns an empty document, so the
round-trip yields "" for all input. **Fix:** implement the Lua-AST → `SemanticDocument` walk (mirror of the
`documentToDjot` emitter). Change `TestDjotRoundtrip.cpp:50` to assert STRUCTURE (sections count, block/inline
types, text), and un-mark the `QEXPECT_FAIL`s in `TestDjotFuzz.cpp:397,447,530,542`. Until decode is real, the
"lossless" claim must be removed from README/marketing.

### D2 — Enforce ProvenanceGuard by type, not convention (🔴)
Both call sites hardcode `EditPath::DirectStructural` (`HomeController.cpp:140`), so the refusal branch
(`DjotThenSave && isSigned`, `ProvenanceGuard.cpp:25`) is unreachable; the signed-doc guarantee holds only because
Djot-save-back isn't wired. **Fix:** (a) derive `origin`/`isSigned` from the loaded document's provenance, never
hardcode; (b) make the codec/mapper the chokepoint — `applySemanticToPdf` must require a `ProvenanceToken` minted
only by the guard, so no caller can reach the lossy PDF write without passing the gate. Replace bare-`false` return
(`PdfStructureMapper.cpp:211`) with a distinct `NotSupported` outcome (or throw `ProvenanceViolation`).

### D3 — SemanticDocument model hygiene (🟡)
Strict ownership tree uses `shared_ptr` (`Block.h/Inline.h/SemanticDocument.h`) → use `unique_ptr` with non-owning
back-refs; replace base-class virtual getters returning empty statics (`Block.cpp:11`, `Inline.cpp:10`) with a
proper variant/visitor or distinct node classes; one namespace per library (`pdfws` vs `pdfws_djot`); reuse one
`lua_State` for the round-trip benchmark instead of per-call.
</deliverables>

<success_criteria>
- [ ] djotToDocument produces a real tree; round-trip test asserts structure; fuzz QEXPECT_FAILs removed/passing
- [ ] applySemanticToPdf requires a guard-minted token; provenance derived from the doc; NotSupported is distinct
- [ ] SemanticDocument uses unique_ptr + typed nodes; single namespace; ctest green
- [ ] README/marketing "lossless" claim matches code reality
</success_criteria>

---
<!-- ===================================================================== -->
# AR-PROMPT-10 — Architecture refactors (interfaces, DI, errors, secrets)

**Paste this entire block into a fresh CC session. Estimated effort: 1-2 weeks. Risk: MEDIUM (broad, mechanical).**

<session_metadata>
Phase: Audit remediation — structural quality (elite-bar)
Priority: 🟠 maintainability + testability + a privacy defect (secrets dropped off-Windows)
Depends on: AR-PROMPT-3,4 landed (so interface decomposition follows real call patterns)
Agents: /architect (primary), /backend
Estimated context: ~65% | Risk: MEDIUM — land in small reviewable commits
</session_metadata>

<deliverables>

### D1 — Decompose `IPdfEditorEngine` + delete dead interfaces (🟠)
Split the ~60-method god-interface (`IPdfEditorEngine.h`) into role interfaces (`IPdfDocumentIO`, `IPageEditor`,
`IImageEditor`, `IRedactor`, `IEncryptor`, `IExporter`, `ISignatureAware`). Delete zero-implementer `IPdfPage`;
either promote `IPdfDocument/Renderer/Writer/Searcher` into real seams or remove them. Engines may implement several.

### D2 — Replace AppContext service-locator with DI + fix lifetime (🟠)
Inject the specific `shared_ptr<I...>` each component needs via its constructor; stop passing `const AppContext*`
down five layers (~290 derefs). Keep `AppContext` only as the composition root `Bootstrapper` fills. Fix the
raw-pointer-to-stack-local lifetime (`main.cpp:128` → window owns context by value/`unique_ptr`).

### D3 — Unify the error boundary (🟠)
Adopt one convention — `Result<T, ErrorInfo>` / `std::expected`. Ban exceptions crossing the engine boundary (the
codec throws `std::runtime_error` into `bool`-returning callers). Stop encoding errors as document content
(`PdfStructureMapper.cpp:190` error-paragraph). Fix `ErrorInfo::isOk()` to key on severity, not message-emptiness.

### D4 — Real `ISecretStore` (🟠 privacy)
`CredentialManager` no-ops off Windows (returns false, drops API keys). Build `ISecretStore` with DPAPI / libsecret /
macOS Keychain backends + an explicitly-labeled encrypted-file fallback; never silent-fail.

### D5 — Keep core layer clean (🟡)
`pdfws_core` links `Qt6::Gui` and forward-declares an engine-owned `PageOcrResult` (`IPdfEditorEngine.h:16`). Move
shared types into core (or a types target); keep heavy GUI types out of the core contract where feasible.
`IFormManager` should operate on an open-document handle, not reload the file per field.
</deliverables>

<constraints>
- Preserve the core→engines→commands→ui dependency direction; no reversals.
- Land as a series of small, individually-green commits; keep tests passing at each step.
</constraints>

<success_criteria>
- [ ] Role interfaces replace the god-interface; dead interfaces removed; mocks shrink accordingly
- [ ] Components receive injected dependencies; no `AppContext*` passed down layers; no stack-lifetime risk
- [ ] One error convention; no exceptions cross the engine boundary; no errors-as-content
- [ ] ISecretStore works on all platforms with no silent failure; core layer free of engine-owned types; ctest green
</success_criteria>

---
<!-- ===================================================================== -->
# AR-PROMPT-11 — Build / release hardening + legal

**Paste this entire block into a fresh CC session. Estimated effort: 2-4 days (+cert procurement). Risk: MEDIUM.**

<session_metadata>
Phase: Audit remediation — release gates (must precede any public tag)
Priority: 🔴 unsigned MSI + 🔴 unmet AGPL/GPL source obligations + 🟠 guards never exercised
Depends on: nothing (procure code-signing cert in parallel)
Agents: /devops (primary), /legal-review (license matrix)
Estimated context: ~50% | Risk: MEDIUM
</session_metadata>

<deliverables>

### D1 — Authenticode-sign EXE + MSI (🔴)
No `signtool` anywhere. **Fix:** sign `GlyphPDF.exe` before `wix build` and the MSI after, with an RFC-3161
timestamp; hash AFTER signing; gate the release pipeline so an unsigned artifact is never published. (Procure
EV/OV cert; until then, sign with a placeholder/dev cert and wire the step.)

### D2 — AGPL/GPL compliance for bundled veraPDF + OpenJDK (🔴 legal)
`deploy.ps1:151` bundles veraPDF (AGPL-3.0) + its OpenJDK 21 (GPL-2.0+CPE) with no source offer; neither is in
`LICENSE-3RD-PARTY.md`. **Fix:** add `VERAPDF-SOURCE-OFFER.txt` + the JDK `legal/` tree to the deploy; add both
rows to the matrix; gate deploy on their presence. Stage a `licenses/` dir containing EVERY upstream
LICENSE/COPYING/NOTICE (PDFium, OpenSSL, qpdf, Tesseract, Leptonica, ONNX, OpenJPEG, jbig2enc, Lua, Djot, Qt,
PoDoFo) — a summary table is not the license text.

### D3 — Make the license guards real (🟠; §6)
The MuPDF/Poppler FATAL_ERROR guards (`CMakeLists.txt:94-119`) never run in CI and only match bare target names.
**Fix:** broaden to namespaced targets (`Poppler::poppler`, `unofficial::poppler`, etc.); add a CI matrix leg that
`pacman -S`'s poppler/mupdf and asserts `cmake` configure FAILS.

### D4 — Binary hardening + build determinism (🟡)
Add `-fstack-protector-strong`, `-D_FORTIFY_SOURCE=2`, `-Wl,--dynamicbase,--nxcompat,--high-entropy-va`, CFG where
supported; enable LTO; `strip` staged binaries (they currently leak `C:\Users\User\Projects\pdf` DWARF paths).
Default `CMAKE_BUILD_TYPE=Release` when unset (the documented `cmake -B build` currently yields an unoptimized,
assert-enabled build). Bundle the official `vc_redist` payload instead of copying the build host's System32 CRT.

### D5 — Release-feature + winget integrity (🟠)
Hard-fail release builds if any shipped-feature define (`HAS_PDFIUM/HAS_TESSERACT/HAS_RAPIDOCR/HAS_QPDF`) is
unexpectedly off (the `find_package(... QUIET)` calls silently drop features today); emit a feature/version
summary. Regenerate the winget manifests for the current version (they're stale at 1.0.0: old ProductCode/URL/SHA)
and reconcile the MIT-vs-Apache license identity across winget/portable/CLAUDE.md.
</deliverables>

<constraints>
- Subprocess isolation does NOT waive AGPL §6/GPL §3 source-conveyance — D2 is mandatory before distributing.
- Do not ship any public 1.x until D1+D2 are complete (CLAUDE.md §9 gate).
</constraints>

<success_criteria>
- [ ] EXE+MSI Authenticode-signed (RFC-3161 timestamp); unsigned artifact is un-publishable
- [ ] veraPDF + OpenJDK source offers + full upstream license trees staged; matrix complete
- [ ] License guards fire in a CI leg; namespaced targets covered
- [ ] Hardening flags + LTO + strip; default Release; bundled vc_redist; release fails on missing features
- [ ] winget manifests current; license identity reconciled
</success_criteria>

---
<!-- ===================================================================== -->
# AR-PROMPT-12 — Test-coverage backfill + CI gates

**Paste this entire block into a fresh CC session. Estimated effort: 3-5 days. Risk: LOW-MEDIUM. RUN LAST.**

<session_metadata>
Phase: Audit remediation — prove the fixed behavior is real (Pattern 1/2)
Priority: 🟠 close the test gaps the audit found masking defects
Depends on: the prompts whose code each test covers (run AFTER 1-11)
Agents: /qa (primary)
Estimated context: ~45% | Risk: LOW-MEDIUM
</session_metadata>

<deliverables>

### D1 — Activate + add security regression tests
Confirm `testRedactionOnSignedDocIsBlocked` is a slot (AR-PROMPT-2 D7). Add: redaction verification scanning
streams + dicts; CTM-transformed-text leak; content-stream injection (NUL/newline/parens); signature
re-validation after DSS/TS append; VRI-key reference; OCSP bypass is compiled out in release.

### D2 — Remove environment-masking gaps
Pattern-redact PII tests must run without PDFium (or the suite documents the gap as a hard CI requirement, with a
PDFium-present CI leg). Provide a small committed OCR model fixture (or a CI leg that fetches+hash-verifies models)
so RapidOCR real-inference is actually exercised. Djot round-trip asserts structure (AR-PROMPT-9 D1).

### D3 — CI gates
Add (or create `.github/workflows/`): build + `ctest` on every push; the license-guard leg (AR-PROMPT-11 D3); a
release leg that runs `deploy.ps1`'s validation gate and a clean-VM smoke launch (the installer is currently built
only on one dev's machine and never verified to launch on a clean box); assert the shipped-feature defines are on.

### D4 — Clean up stale test metadata
Fix misleading `QEXPECT_FAIL`/"until M5" comments (`TestSignatureRealCrypto.cpp:422`), the vacuous non-null Djot
assertion, and any test asserting nothing. Confirm `RESOURCE_LOCK` on I/O-sharing tests; run
`--repeat-until-fail 5`.
</deliverables>

<success_criteria>
- [ ] Every CRITICAL fix from AR-PROMPT-1..9 has a regression test that fails pre-fix
- [ ] No security path is only-tested-under-`#ifdef`; OCR real-inference exercised in CI
- [ ] CI builds + tests + runs license guards + clean-VM smoke + feature-define assertions
- [ ] No vacuous/misleading tests; `--repeat-until-fail 5` green
</success_criteria>

---

## Vault & CLAUDE.md updates (do on your local machine)

The Obsidian vault (`C:\Users\User\.claude\memory\projects\glyphpdf\`) is **not reachable from the remote
execution container**, so it can't be edited from this session. Apply these on your machine:

1. **New note `10-audit-2026-06-16.md`** — paste the consolidated audit findings (13 CRITICAL + HIGH/MEDIUM/LOW)
   and link this remediation file. Suggested front matter: tags `#audit #glyphpdf #remediation`, date 2026-06-16.
2. **`01-current-state.md`** — add: "2026-06-16 full-codebase audit (8 domains). Headline claims (true redaction,
   lossless Djot, PAdES B-LTA, signed MSI) NOT fully met in v1.0.1. Remediation tracked in
   `docs/planning/AUDIT-2026-06-16-REMEDIATION.md` (AR-PROMPT-1..12). Public 1.x blocked on AR-PROMPT-1/2/3/11."
3. **`08-lessons-learned.md`** — add Pattern 20: "Audit found the recurring wired-to-UI-not-to-reality pattern
   recurred in headline features (visual-only redaction path; Djot decode stub; decorative ProvenanceGuard;
   annotations sidecar-only) with tests that only checked non-null / were configured QEXPECT_FAIL. Lesson: a green
   suite is not evidence a feature exists; assert behavior/structure, not pointers."
4. **`06-non-negotiables.md`** — add: "Redaction must chain document-level sanitize + GC save + post-open
   verification, single path. B-T must embed a signature timestamp. No test hooks in production crypto paths."
5. **`CLAUDE.md` §0/§5** (in-repo, editable here — see separate commit) — record the audit + point to this file.
