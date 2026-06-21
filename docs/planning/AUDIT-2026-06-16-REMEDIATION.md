# GlyphPDF — Audit Remediation Prompts (2026-06-16) — v2, hardened

**Purpose.** Twelve self-contained Claude Code execution prompts that close every finding from the
2026-06-16 full-codebase audit (8 parallel domain reviews: crypto/signatures, PDF object backend,
redaction/encryption, subprocess/IO/OCR, concurrency/performance, core architecture/Djot,
build/packaging/tests, UI/UX). Each prompt is recast into the `<mega_prompt>` schema and hardened with
explicit **verification-before-completion**, **anti-self-report** guardrails, per-deliverable **evidence
gates**, and **failure-handling**. **One prompt = one fresh Claude Code session**, rooted at
`C:\Users\User\Projects\pdf`. Paste a single `<mega_prompt>` block as the first message; add no commentary.

**Audit headline.** Features were wired to the UI before being wired to reality, and tests/labels assert
the happy path is real when it isn't. None of the four headline claims — true redaction, lossless Djot
interchange, PAdES B-LTA, signed MSI — are fully true in the audited v1.0.1 code. **Do not ship any
public 1.x until AR-PROMPT-1 … AR-PROMPT-3 + AR-PROMPT-11 are complete.** (Note: v1.3.0/v1.3.1 were
released ahead of this gate; treat closing these prompts as the condition for a *trustworthy* 1.x line.)

**Severity legend:** 🔴 CRITICAL (release blocker / security / data loss / crash) · 🟠 HIGH · 🟡 MEDIUM · ⚪ LOW.

---

## Index & dependency-aware execution order

| Prompt | Theme | Severity span | Depends on |
|---|---|---|---|
| **AR-PROMPT-1** | Safety hotfix bundle — crashes + data loss | 🔴 | none (do first) |
| **AR-PROMPT-2** | Redaction unification & completeness (flagship) | 🔴🟠 | none (coordinate save chokepoint w/ 4) |
| **AR-PROMPT-3** | Signature/crypto conformance & security | 🔴🟠 | none |
| **AR-PROMPT-4** | Content-stream injection + PDF backend hardening | 🔴🟠 | none (coordinate w/ 2,3) |
| **AR-PROMPT-5** | Subprocess / IO / OCR hardening | 🟠🟡 | AR-PROMPT-1 (taskkill) |
| **AR-PROMPT-6** | Concurrency & performance | 🔴🟠🟡 | AR-PROMPT-1 (prefetch UAF) |
| **AR-PROMPT-7** | Annotations-in-PDF + UI threading + close safety | 🔴🟠 | none |
| **AR-PROMPT-8** | UI truth & dead-surface cleanup | 🔴🟠🟡 | none |
| **AR-PROMPT-9** | Djot dual-model correctness + ProvenanceGuard | 🔴🟠 | none |
| **AR-PROMPT-10** | Architecture refactors (interfaces, DI, errors, secrets) | 🟠🟡 | AR-PROMPT-3,4 landed |
| **AR-PROMPT-11** | Build / release hardening + legal (signing, AGPL/GPL) | 🔴🟠 | none |
| **AR-PROMPT-12** | Test-coverage backfill + CI gates | 🟠 | the prompts whose code it tests |

**Recommended order:** 1 → (2,3,4 in parallel — but see note) → (5,6,7,8,9 in parallel) → 10 → 11 → 12.
Run 11 before any tag; run 12 last so it asserts the real, fixed behavior. **Parallelism note:** prompts
share one working tree and one `build/`; only run them "in parallel" as *separate human-reviewed sessions
on separate branches/worktrees* — never two writers on the same checkout at once.

---

## SHARED HOUSE PROTOCOL — every prompt inherits this

> This block is the standing contract. Each `<mega_prompt>` references it as `@house-protocol`; obey it in
> full even though it is not repeated inside every prompt.

```xml
<house_protocol id="house-protocol">

  <phase_0_reading note="Read before writing any code. Localized to in-repo, reachable sources.">
    REQUIRED (in repo, always present):
      - CLAUDE.md  (§0 scope lock, §5 'every surface works or is not shown', §6 non-negotiables, §9 release gate)
      - README.md  (headline claims to keep honest)
      - CHANGELOG.md  (what each version actually shipped)
      - docs/SPEC-TRACEABILITY.md  (spec §9 → implementing file/symbol → status; your map of reality)
      - docs/PRD.md or PRD.md §27 (implementation-status matrix) + §28 (roadmap)
      - the exact files listed in this prompt's <inputs>
    OPTIONAL (read if present; may be absent in this checkout — do NOT block on them):
      - C:\Users\User\.claude\memory\projects\glyphpdf\08-lessons-learned.md  (Patterns 1/2/5/6/20)
      - C:\Users\User\.claude\memory\projects\glyphpdf\06-non-negotiables.md
      - C:\Users\User\.claude\memory\projects\glyphpdf\01-current-state.md
      - C:\Users\User\.claude\memory\knowledge\agent-execution-anti-patterns.md
  </phase_0_reading>

  <environment>
    Native Windows, MSYS2 ucrt64 toolchain. Bash-tool cwd resets to D:\ between calls — prefix every
    command with `cd /c/Users/User/Projects/pdf && ...`. For build/test add `export PATH="/c/msys64/ucrt64/bin:$PATH"`.
    The `build/` dir is pre-configured (Release, Ninja) with all gitignored deps (vendored PoDoFo 1.1,
    PDFium, ONNX, models) present — build with `cmake --build build`; do NOT delete or reconfigure it.
    Tests: `cd /c/Users/User/Projects/pdf/build && QT_QPA_PLATFORM=offscreen ctest --output-on-failure -j4`.
  </environment>

  <anti_self_report rule="A green suite is NOT evidence a feature exists (audit Patterns 1/2/20).">
    - Assert BEHAVIOR / STRUCTURE / BYTES, never pointers-non-null or "it compiled".
    - VERIFY-ON-DISK: after a fix, prove it by re-running the named command and pasting the ACTUAL output
      into your report. Never claim a result you did not observe in this session.
    - TEST PROVENANCE: a regression test only counts if (a) it lives on disk, (b) its result file's mtime
      is newer than your edits, and (c) it FAILS before the fix and PASSES after. State both observations.
    - Security/crypto/redaction claims require re-opening the OUTPUT artifact and scanning it — never trust
      in-memory state or a return value.
  </anti_self_report>

  <verification_before_completion>
    Do not mark any deliverable or success-criterion checkbox until you have run its evidence command and
    seen the expected observable. After ALL deliverables:
      `cd /c/Users/User/Projects/pdf/build && QT_QPA_PLATFORM=offscreen ctest --output-on-failure -j4 --repeat-until-fail 3`
    Report the real pass/fail line and note any pre-existing skips. If anything is red, you are NOT done.
  </verification_before_completion>

  <failure_handling>
    If a deliverable cannot meet its acceptance honestly: STOP, do not fake it. Specifically NEVER:
      - weaken/disable/`QEXPECT_FAIL` a test to make the suite green;
      - weaken a security control (signed-doc refusal, OCSP verify, escaping) to pass;
      - add a `TODO(audit-*)`/`FIXME` or a "scheduled for a future engine update" dialog;
      - report success you did not verify on disk.
    Instead: leave the code in a clean compiling state, commit what genuinely passed, and report the blocker
    (file:line, what you tried, why it can't pass) in the final report. A blocked deliverable honestly
    reported beats a faked green.
  </failure_handling>

  <git>
    Branch first (never commit on main). Atomic commit per deliverable; `git status` clean before each.
    Commit-message trailer: `Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>`.
    LOCAL COMMITS ONLY — do not push unless the user explicitly says push.
  </git>

  <scope_discipline>
    Smallest correct diff per fix. Opportunistic refactors belong to AR-PROMPT-6/8/10 — do not fold them in.
  </scope_discipline>

</house_protocol>
```

---
<!-- ===================================================================== -->
# AR-PROMPT-1 — Safety hotfix bundle (crashes + data loss)

**Paste the block below into a fresh CC session. Effort: 2-4 h. Risk: LOW.**

```xml
<mega_prompt id="AR-1" inherits="@house-protocol">

<session_metadata>
  Phase: Audit remediation — emergency safety fixes
  Priority: 🔴 BLOCKING — deterministic crashes + silent data loss reachable in shipped builds
  Depends on: nothing (do first) | Agents: /backend | Est. context ~25% | Risk: LOW (localized, high-confidence)
</session_metadata>

<role>
Senior C++17/Qt6 engineer. You fix crashes and data-loss bugs with the smallest correct diff, add one
regression test per fix that fails before and passes after, and never expand scope.
</role>

<mission>
Make five reachable defects unreachable, each proven by a regression test and a clean repeat-run suite:
(A) watermark null-deref crash, (B) cross-thread use-after-free in render prefetch on document close,
(C) force-kill of the user's LibreOffice that destroys unsaved work, (D) autosave-retry UAF, (E) AIChat
void* UAF. Done = no defect reproduces under its evidence command AND `ctest --repeat-until-fail 3` is green.
</mission>

<context>
  <product>GlyphPDF — privacy-first native Windows PDF workstation (C++17/Qt6.11, MSYS2 ucrt64, PoDoFo 1.1, PDFium); core→engines→commands→ui static-lib layers.</product>
  <avoidance_rules>No opportunistic refactors (AR-6/8/10 own those). No behavior change beyond the named fix.</avoidance_rules>
</context>

<inputs note="Read these exact sites first; line numbers are indicative — confirm by reading.">
  src/engines/podofo/PoDoFoBackend.cpp  (watermark + SearchFont call sites ~128, 744, 1007, 1036, 1354, 2997)
  src/engines/RenderCache.cpp + .h       (prefetch lambda ~256-300; destructor ~36)
  src/engines/ConversionManager.cpp      (convertOfficeToPdf ~405-486; taskkill ~431)
  src/engines/AutosaveManager.cpp + .h   (retry singleShot ~109-123)
  src/modes/AIChatPanel.cpp              (void* QListWidgetItem property ~107-128)
</inputs>

<deliverables>
  <D1 sev="🔴 crash" title="Watermark null-deref">
    PoDoFoBackend.cpp:2997 — addTextWatermark dereferences SearchFont("Helvetica") directly; every other
    call site null-checks and falls back to GetStandard14Font. No-Helvetica doc → nullptr → crash.
    FIX: `const PdfFont* f = doc.GetFonts().SearchFont("Helvetica"); if (!f) f = &doc.GetFonts().GetStandard14Font(PdfStandard14FontType::Helvetica); painter.TextState.SetFont(*f, ...);`
    EVIDENCE: new TestWatermark (or TestIntegration) — watermark a freshly created font-less PDF; asserts success, no crash.
  </D1>
  <D2 sev="🔴 crash" title="Render-prefetch use-after-free">
    RenderCache.cpp:277,297 — prefetch lambda captures `renderer` (IPdfRenderer*) by raw pointer and calls
    renderPage() on a pool thread; closing/replacing the doc frees the backend mid-render. weak_from_this()
    protects only the cache, not the renderer.
    FIX: on document close, cancel+join prefetch before the renderer is torn down
    (`m_prefetchCancelToken++; if (m_prefetchFuture.isRunning()) m_prefetchFuture.waitForFinished();`) AND/OR
    hold the renderer via weak_ptr/shared_ptr so the lambda re-checks liveness past the token check at :300.
    Document the ownership contract in a comment.
    EVIDENCE: stress test starts a prefetch then clears cache/renderer; no crash under `--repeat-until-fail`.
  </D2>
  <D3 sev="🔴 data loss" title="LibreOffice force-kill">
    ConversionManager.cpp:431-433 — every Office→PDF conversion runs `taskkill /F /IM soffice.bin /IM soffice.exe`,
    killing ALL the user's LibreOffice instances (and unsaved work). Reachable since the `#ifndef HAS_LIBREOFFICE`
    guard was removed.
    FIX: remove the blanket pre-kill. Launch soffice with a private profile (no shared lock to clear):
    `--env:UserInstallation=file:///<temp>/glyphpdf-soffice-<pid>` (dir via TempFileManager). Keep ONLY the
    timeout-path own-PID kill `taskkill /F /T /PID <ownpid>` (~460).
    EVIDENCE: Office import still works with LibreOffice present (manual-verify note acceptable + a guard test that
    asserts the conversion command contains no global soffice taskkill).
  </D3>
  <D4 sev="🔴 latent crash" title="Autosave retry UAF">
    AutosaveManager.cpp:109-123 — on rename failure the watcher schedules a 250 ms `QTimer::singleShot(.., this, lambda)`
    capturing this/m_document with no QPointer guard; closing the doc within 250 ms → UAF.
    FIX: route the retry through a member QTimer (child of `this`, auto-cancelled on destruction) or guard with
    QPointer. Make `m_saving` `std::atomic<bool>`.
    EVIDENCE: TestAutosave case for the rename-fail-then-destroy window.
  </D4>
  <D5 sev="🟠" title="AIChatPanel void* UAF">
    AIChatPanel.cpp:107-128 — a QListWidgetItem* is smuggled through a dynamic void* property; list clear /
    document switch / a second send racing onAiFinished dangles it.
    FIX: typed member pointer (or QPointer<QListWidgetItem>); disable input (button AND QLineEdit::returnPressed)
    while a request is in flight; clear/guard on document change.
    EVIDENCE: test (or guarded manual note) for the document-switch-mid-request path.
  </D5>
</deliverables>

<evidence_gate>
  Each D must have a test that FAILS pre-fix and PASSES post-fix (state both). Where a GUI test is impractical
  (D3/D5), a guard/unit test on the non-GUI seam plus an explicit manual-verify note is acceptable — but say which.
</evidence_gate>

<success_criteria>
  - [ ] D1-D5 fixed with the exact mechanisms above; each has a regression test (fails pre-fix / passes post-fix — both observed)
  - [ ] `ctest --output-on-failure -j4 --repeat-until-fail 3` green (note pre-existing skips); paste the real summary line
  - [ ] No new TODO(audit-*)/FIXME; `git status` clean; 5 atomic commits (local only)
  - [ ] Final report (<200 words): what was reachable, how each triggered, test added + its pre/post result
</success_criteria>

</mega_prompt>
```

---
<!-- ===================================================================== -->
# AR-PROMPT-2 — Redaction unification & completeness (flagship)

**Paste the block below into a fresh CC session. Effort: 2-4 days. Risk: HIGH (security feature).**

```xml
<mega_prompt id="AR-2" inherits="@house-protocol">

<session_metadata>
  Phase: Audit remediation — flagship security feature
  Priority: 🔴 BLOCKING — redaction can leak secrets the product's headline promises to remove
  Depends on: none (coordinate the save chokepoint with AR-4) | Agents: /security (primary), /backend
  Est. context ~55% | Risk: HIGH — get EVERY leak path; prove removal on disk, not by self-report
</session_metadata>

<role>
Senior document-security engineer who has internalized NSA "Redacting with Confidence" and the Edact-Ray
glyph-advance literature. You assume any un-excised byte is recoverable and you PROVE removal by re-opening
the output and scanning every decoded content stream AND every dictionary value.
</role>

<mission>
Collapse redaction to ONE trustworthy pipeline and prove, by re-opening the saved file, that transformed
text, images, OCR (Tr==3) text, and document-level hidden data are all gone — with the original copy
preserved and no half-redacted state on failure. Done = the verification gate (D7) passes on disk for a
fixture containing each leak class, and no code path returns success after visual-only output.
</mission>

<context>
  <non_negotiable>"Edact-Ray-defended TRUE redaction": content excised from the content stream, never black rectangles (CLAUDE.md §6).</non_negotiable>
  <current_state>
    STRONG path: PoDoFoBackend::applyRedactions (:1558) + redactCanvasRecursively — real content-stream surgery,
      CTM-aware IMAGE excision, SMask neutralize, Form-XObject recursion, Tr==3 OCR scrub, numeric-TJ Edact-Ray
      normalization, struct-tree MCID cleanup, signed-doc refusal.
    WEAK path: PdfPageOps::applyRedactionsToFile (:109) via PdfViewerWidget::applyRedactions (:900) — whitespace
      tokenization, no XObject recursion, images pass through, falls back to a black box and returns true.
    sanitizeDocument (PoDoFoBackend.cpp:1883) strips Info/XMP/EmbeddedFiles/JavaScript/Outlines/OCG/Thumb — but
      is NEVER called from any redaction path.
  </current_state>
  <avoidance_rules>
    Redaction is a FULL rewrite, never WriteUpdate (excised bytes must not survive in incremental history).
    Keep the Edact-Ray numeric-`[ N ] TJ` normalization (do not regress to per-glyph advances).
    Do not weaken the signed-doc refusal (ER-2 guard).
  </avoidance_rules>
</context>

<inputs>
  src/engines/podofo/PoDoFoBackend.cpp (applyRedactions 1558+, isIntersectingSpan 1196, CTM 1208-1224, text cursor 1403, numeric-TJ 1428-1446, sanitizeDocument 1883)
  src/engines/podofo/PdfPageOps.cpp/.h (applyRedactionsToFile 109)
  src/engines/PdfEditorEngine.cpp (applyPatternRedactions 1144-1202; redaction gate 1119-1142)
  src/engines/PatternRedactor.cpp/.h (findMatches, mergeCharBoxes 178; namedPattern 60)
  src/ui/PdfViewerWidget.cpp (applyRedactions 900-938; redactAllMatches)
  src/modes/RedactMode.cpp (page-range parser 285-302; onApplyRedactions 379-387)
  src/shell/controllers/SecurityController.cpp (redaction confirm copy 425-427)
  tests/TestRedaction.cpp (testRedactionOnSignedDocIsBlocked 619 — currently NOT a slot)
</inputs>

<deliverables>
  <D1 sev="🔴" title="Delete the weak path">Remove PdfPageOps::applyRedactionsToFile and PdfViewerWidget::applyRedactions/redactAllMatches; route ALL UI redaction (RedactMode, FindBar "Redact All") through PoDoFoBackend::applyRedactions. No path may return success after only painting a rectangle.</D1>
  <D2 sev="🔴 leak" title="Text CTM fix">At PoDoFoBackend.cpp:1403 transform text cursor (textX, textY) and (textX+totalAdvance, textY) through current CTM × text matrix before the intersection test — mirror the image path (1208-1224). Fixture: body text under `2 0 0 2 0 0 cm`; assert glyphs gone from the decoded stream.</D2>
  <D3 sev="🔴 leak" title="Chain sanitize + GC save">After a successful content redaction, run sanitizeDocument's object-graph scrub (Info, XMP /Metadata, /Names/EmbeddedFiles, /Names/JavaScript, /OpenAction, /Outlines, /OCProperties, per-page /Thumb, /PieceInfo) over the in-memory doc, then save with PoDoFo's garbage-collection/clean option (orphaned objects e.g. removed-annotation /AP streams not serialized). All-or-nothing.</D3>
  <D4 sev="🔴 data loss" title="Redact to a new file">applyPatternRedactions/RedactMode write to a new file (default `{stem}_redacted.pdf`), keep the original, swap only on full success. On any per-page failure, reload/snapshot-restore the in-memory doc before returning false. Fix RedactMode.cpp:379-387 to stop claiming "not modified" after a partial mutation.</D4>
  <D5 sev="🔴 scope" title="Page-range parser">Replace RedactMode.cpp:285-302 (falls through to {-1,-1} = ALL PAGES on unparseable input) with PagesMode's comma-list parser. Invalid input is a HARD ERROR, never "all pages".</D5>
  <D6 sev="🟠" title="Pattern match coverage">PatternRedactor::mergeCharBoxes (:178) skips null char boxes, under-covering matches. If any char in a match lacks a box, fall back to a conservative full-line-height rect spanning the match, or fail the match loudly.</D6>
  <D7 sev="🔴 trust" title="Post-redaction verification gate">Re-open the saved output and assert redacted strings are ABSENT from every decoded content stream AND every dictionary value (Info/XMP/embedded files/bookmarks), and redacted image regions are blanked. Extend testRedactedTextUnextractable. Move testRedactionOnSignedDocIsBlocked (TestRedaction.cpp:619) into `private slots:` so it runs.</D7>
  <D8 sev="🔴 trust" title="Reconcile the confirm copy">SecurityController.cpp:425-427 tells users the tool "cannot guarantee secure removal … use a dedicated redaction tool." Once D1-D7 land, replace with copy that accurately describes the real Edact-Ray pipeline. If any gap remains, that gap is a release blocker, not a disclaimer.</D8>
</deliverables>

<evidence_gate>
  The acceptance artifact is the RE-OPENED output file, not the in-memory doc. Build one fixture per leak class
  (CTM-transformed text, image, Tr==3 OCR layer, Info/XMP/embedded/bookmark secret) and have D7 scan all of them.
  testRedactionOnSignedDocIsBlocked must be a slot and pass. Confirm test-file mtime > your edits.
</evidence_gate>

<success_criteria>
  - [ ] Exactly one redaction code path; weak path deleted; no path returns success on visual-only output
  - [ ] Transformed text, images, OCR Tr==3, AND document-level hidden data all excised — proven by re-opening output
  - [ ] Redaction writes a new file; original preserved; partial failure leaves no half-redacted state
  - [ ] Page-range parse failure is an error, never all-pages
  - [ ] testRedactionOnSignedDocIsBlocked runs (is a slot) and passes; verification test scans streams + dicts
  - [ ] ctest green incl. new fixtures (paste summary); atomic commits per deliverable; confirm copy matches reality
</success_criteria>

</mega_prompt>
```

---
<!-- ===================================================================== -->
# AR-PROMPT-3 — Signature / crypto conformance & security

**Paste the block below into a fresh CC session. Effort: 3-5 days. Risk: HIGH.**

```xml
<mega_prompt id="AR-3" inherits="@house-protocol">

<session_metadata>
  Phase: Audit remediation — PAdES correctness + crypto security
  Priority: 🔴 BLOCKING — shipped OCSP-verification bypass + B-T signatures that aren't timestamped + a signature-spoofing bypass
  Depends on: none | Agents: /security | Est. context ~60% | Risk: HIGH — spec conformance; add post-condition re-validation
</session_metadata>

<role>
PKI/PAdES engineer fluent in ETSI EN 319 122/132, RFC 3161/6960, and OpenSSL CMS/OCSP/X509 memory rules.
You NEVER ship a test hook in a production code path.
</role>

<mission>
Make every signature claim true and every test hook production-absent: no production path embeds an
unverified OCSP response; B-T output actually carries a signature timestamp (or is not labeled B-T); the
shadow-attack detector parses revisions instead of substring-matching; prior signatures stay valid after
DSS/timestamp append. Done = adversarial fixtures pass and the OCSP bypass is provably compiled out of release.
</mission>

<context>
  <subject>SignatureManager.cpp (~1785 lines): P12 loading, OCSP/TSA networking, PoDoFo PDF surgery, DSS/VRI construction, ~500-line validation state machine. Headline claim: local PAdES B-LTA.</subject>
  <avoidance_rules>
    SHA-256 only for hashing (SHA-1 confined to the VRI key per ETSI). RSA ≥ 2048 enforced (keep).
    qpdf must NEVER enter the signing path. Incremental DSS/timestamp via SaveUpdate/WriteUpdate only.
    ALL test hooks compiled out of production via `#ifdef GLYPHPDF_TESTING`.
  </avoidance_rules>
</context>

<inputs>src/engines/SignatureManager.cpp (bypass 1111-1120; fetchOcspResponse 314-336; OCSP_basic_verify 1110; freshness 1717-1737; certID TODO 604-606,704-708,1720-1722; B-T token 1002-1012; shadow detect 1358-1376; ByteRange 1335-1352; DocTimeStamp size 558-578; DSS 404-509; addDocTimestamp 529; /Contents extractors 809-817 & 1417-1426; httpPost 210-233)</inputs>

<deliverables>
  <D1 sev="🔴 security; §6" title="Remove OCSP verification bypass">:1111-1120 sets verifyOk=true for any cert whose filename contains `revoked`/`_cert` when a local .der exists; fetchOcspResponse (:314-336) loads arbitrary local DER next to the cert. FIX: gate BOTH the local-DER load and the verify bypass behind `#ifdef GLYPHPDF_TESTING` (or the existing setTrustStoreForTest injection). Production must never embed an unverified OCSP response.</D1>
  <D2 sev="🔴/🟠" title="OCSP responder trust + freshness">OCSP_basic_verify(basic, certs, store, 0) (:1110) passes the signer's own embedded chain as the responder pool and asserts no delegation. FIX: verify the responder is the issuer or an `id-kp-OCSPSigning` delegate of the issuer; pass the actual issuer cert. Add thisUpdate/nextUpdate freshness via OCSP_check_validity; reject expired (:1717-1737). Complete the certID full match (issuer-hash + serial, not serial-only) at :604-606, 704-708, 1720-1722.</D2>
  <D3 sev="🟠 conformance" title="B-T carries a real signature timestamp">:1002-1012 fetches an RFC-3161 token, logs it, discards it — and hashes it over the cert DER, not the CMS signatureValue. FIX: embed the token as the SignerInfo unsigned attribute `id-aa-signatureTimeStampToken` computed over the signature value. If PdfSignerCms can't add it, post-process the CMS. Until embedded, do not label output "B-T".</D3>
  <D4 sev="🔴 spoofing" title="Shadow / incremental-save-attack detection">:1358-1376 treats any appended revision merely CONTAINING the bytes `/DSS`/`/ByteRange` as a benign LTV update. FIX: parse the appended revision's xref/trailer and diff the object graph; only a new DSS dict / DocTimeStamp field is benign. Substring matching is not a security control.</D4>
  <D5 sev="🟠" title="ByteRange hole + DocTimeStamp size">:1335-1352 only checks off1==0. FIX: assert the excluded gap [off1+len1, off2) is EXACTLY the /Contents placeholder and off2 immediately follows it; reject holes elsewhere. At :558-578 hard-fail if the TSA token exceeds the 32 KB /Contents reservation (no silent truncation).</D5>
  <D6 sev="🟠" title="Post-condition re-validation">After buildDssDictionary (:404-509) and addDocTimestamp (:529), re-run validateSignatures and assert the prior approval signature is still integrity-intact; fail the op otherwise. Use ONE shared raw-/Contents extraction primitive for both signing and validation (today duplicated/divergent at :809-817, 1417-1426); add a unit test asserting the SHA-1 VRI key matches an independent reference.</D6>
  <D7 sev="🟡 truth" title="Surface partial outcome + signing time">SignDocumentHelper drops lastSignOutcome() — surface `PartialLtvMissing` to the UI instead of total success/failure. Populate SignatureInfo::date from parsed signing time / DocTimeStamp genTime (shown but never set). Badge B-LT only when revocation info is actually present (not merely because a DSS exists).</D7>
  <D8 sev="🟡" title="Off-thread signing">httpPost (:210-233) runs a nested QEventLoop on the UI thread during signing → reentrancy/UAF if the user re-clicks or closes the doc. Move signing + its TSA/OCSP calls off the UI thread; null-check reply; enforce HTTPS for TSA/OCSP URLs.</D8>
</deliverables>

<evidence_gate>
  Add adversarial fixtures: (a) shadow-attack PDF with a malicious appended revision → must be flagged; (b) a
  build-time assert/test that the bypass symbol is absent unless GLYPHPDF_TESTING; (c) sign → DSS/TS append →
  re-validate → prior signature still intact. Prove D1 with: production-config build does not link the bypass branch.
</evidence_gate>

<success_criteria>
  - [ ] No production path can embed an unverified OCSP response; bypass + local-DER load are test-only (proven)
  - [ ] OCSP responder delegation + freshness validated; certID full match implemented
  - [ ] B-T embeds id-aa-signatureTimeStampToken over the signature value (or label downgraded)
  - [ ] Shadow-attack detection parses the revision; ByteRange holes rejected; oversized TSA token hard-fails
  - [ ] Prior signature re-validated after DSS/TS append; one shared /Contents extractor; VRI-key unit test passes
  - [ ] Partial outcome + signing date surfaced; signing off the UI thread; ctest green incl. adversarial fixtures
</success_criteria>

</mega_prompt>
```

---
<!-- ===================================================================== -->
# AR-PROMPT-4 — Content-stream injection + PDF backend hardening

**Paste the block below into a fresh CC session. Effort: 2-4 days. Risk: MEDIUM-HIGH.**

```xml
<mega_prompt id="AR-4" inherits="@house-protocol">

<session_metadata>
  Phase: Audit remediation — PDF object-graph safety
  Priority: 🔴 active content-stream injection on the OCR/MRC path + signature-invalidating saves
  Depends on: none (coordinate the save chokepoint with AR-2/3) | Agents: /backend | Est. context ~55% | Risk: MEDIUM-HIGH
</session_metadata>

<role>
Senior PDF-internals engineer fluent in the PoDoFo 1.1 object model and content-stream tokenization. You
never write user strings into a content stream without escaping, and you funnel all persistence through one
signature-aware save.
</role>

<mission>
No unescaped user string ever reaches a content stream, every mutator persists through one signature-aware
save, and image math can't overflow. Done = an injection fixture (NUL/newline/parens in OCR text) round-trips
safely and a signed-doc edit no longer invalidates the signature.
</mission>

<context>
  <avoidance_rules>Never write user strings into content streams without `pdfEscapeLiteralString`. Don't change the redaction tokenizer relied on by AR-2; coordinate the shared save chokepoint.</avoidance_rules>
</context>

<inputs>src/engines/PdfEditorEngine.cpp (OCR sandwich 574-583; resolveDocument 158; ~13 doc.Save() sites: rotatePage 572, cropPage 944, resizePage 963, reorderPages 992, insertPageFromBytes 618, deletePage 633, insertBlankPage 646, moveImage 2340, resizeImage 2368, rotateImage 2403, replaceImage 2462, deleteImage 2506, deleteObjectAt 787; addImageWatermark 3083; replaceImage 2446; optimizeDocument 3366, dedup 3405-3417; rewriteImageMatrix 2169-2212; deleteImage tokens 2472-2514; metadata 254; setMetadata 285-292; extractAnnotations 2851); src/engines/podofo/PdfStringEscape.cpp (15-23); src/engines/podofo/PoDoFoBackend.cpp (writeDjotPieceInfo 2587)</inputs>

<deliverables>
  <D1 sev="🔴; §6" title="Content-stream injection on MRC/OCR sandwich text">PdfEditorEngine.cpp:574-583 hand-rolls escaping for only `\ ( )` then writes raw toUtf8(). Attacker-controlled OCR text (NUL/newline/unbalanced parens) corrupts/injects the stream. FIX: `cs += "(" + QByteArray::fromStdString(pdfEscapeLiteralString(w.text)) + ") Tj\n";` Audit EVERY other content-stream write site for the same pattern.</D1>
  <D2 sev="🟠; §6" title="One signature-aware save chokepoint">~13 mutators (see inputs) call doc.Save() directly, bypassing writeUpdate's signature logic → silently invalidate signatures on signed PDFs. FIX: route every persistence through one method that detects signatures and uses WriteUpdate, or refuses the op on signed docs (as redaction does). Fix resolveDocument (:158) so it never loads a divergent second copy from disk and saves that.</D2>
  <D3 sev="🟠" title="Integer overflow in image buffers">`width*height*3` computed in int before any cap; addImageWatermark (:3083) has NO dimension cap. FIX: apply the existing 10000-px cap to addImageWatermark; compute the product in qint64 everywhere (also replaceImage:2446, optimizeDocument:3366).</D3>
  <D4 sev="🟡 corruption" title="Replace raw-string content-stream surgery">rewriteImageMatrix (:2169-2212) and deleteImage (:2472-2514) use std::string::find/rfind heuristics (`/Im1` matches inside `/Im12`; wrong cm/q grabbed). FIX: use the operator-tokenizing PdfContentStreamReader path (as listImages/redactCanvasRecursively do), matching the XObject name token exactly.</D4>
  <D5 sev="🟠 data loss" title="PdfStringEscape round-trip + double-escape">PdfStringEscape.cpp idempotency heuristic (:15-23) makes escape/unescape lossy for backslash-letter pairs; writeDjotPieceInfo (PoDoFoBackend.cpp:2587) pre-escapes then wraps in PdfString (double escaping). FIX: store raw UTF-8 in PdfString (which escapes on write); drop the idempotency heuristic or store Djot in a stream object. Add a round-trip test over backslash/control-byte payloads.</D5>
  <D6 sev="🟡" title="Release-mode error visibility + crafted-PDF robustness">Many catch blocks log only under `#ifdef QT_DEBUG` and return false silently (metadata 254, setMetadata 285-292 bare catch, image ops). Log qWarning/qCritical unconditionally. extractAnnotations (:2851) calls GetReal() on /Rect entries without a type check → one malformed annotation aborts all extraction; guard with IsNumberOrReal() and skip the bad annotation. optimizeDocument dedup (:3405-3417) is a no-op returning success — implement reference rewriting or remove it from options/estimate.</D6>
</deliverables>

<evidence_gate>Injection test feeds OCR text containing NUL, newline, and unbalanced parens → re-open output, assert the stream is well-formed and the text round-trips. Sign a doc, run each mutator, re-validate → signature intact (or op refused). PdfStringEscape round-trip test over backslash/control bytes is lossless.</evidence_gate>

<success_criteria>
  - [ ] No unescaped user string reaches any content stream; injection test (NUL/newline/parens) passes
  - [ ] Every mutator persists through one signature-aware save; signed-doc edits no longer invalidate signatures
  - [ ] Image dimension math is qint64 + capped everywhere; no overflow
  - [ ] Image matrix/delete use the tokenizer; PdfStringEscape round-trip lossless; no double-escape
  - [ ] Release builds log failures; crafted /Rect doesn't abort extraction; optimize dedup is real or removed
  - [ ] ctest green (paste summary); atomic commits
</success_criteria>

</mega_prompt>
```

---
<!-- ===================================================================== -->
# AR-PROMPT-5 — Subprocess / IO / OCR hardening

**Paste the block below into a fresh CC session. Effort: 2-3 days. Risk: MEDIUM.**

```xml
<mega_prompt id="AR-5" inherits="@house-protocol">

<session_metadata>
  Phase: Audit remediation — process/IO/supply-chain safety
  Priority: 🟠 HIGH — .bat injection, false-success exporters, unverified model downloads
  Depends on: AR-PROMPT-1 (soffice taskkill already removed there) | Agents: /backend, /devops | Est. context ~50% | Risk: MEDIUM
</session_metadata>

<role>Senior systems engineer for native Windows subprocess/IO security. You treat every external-tool path and downloaded byte as hostile until proven safe, and you never return success without confirming the bytes hit disk.</role>

<mission>No batch/CMD-quoting injection reaches an external tool; exporters fail loudly instead of reporting green over corrupt output; downloaded models and the update MSI are verified before use. Done = the evidence checks below observe each guard firing.</mission>

<inputs>src/engines/VeraPdfValidator.cpp (79); src/engines/ConversionManager.cpp (exportTo*/convertImagesToPdf; exportToHtml 286-304); src/engines/OcrEngine.cpp (48-118); src/engines/ocr/PpOcrDecoder.cpp (394-404, 514-520; recognizeCrop rw 487); src/core/TempFileManager.cpp/.h; src/core/UpdateChecker.cpp (266-299)</inputs>

<deliverables>
  <D1 sev="🟠 injection" title="Safe .bat / external-tool invocation">VeraPdfValidator.cpp:79 (and any .bat/.cmd) — QProcess can't run batch files via CreateProcess, and CMD re-quotes the user PDF path (`report & x.pdf`). FIX: prefer the extension-less launcher or the underlying `java -jar`; if a .bat must be used, invoke `cmd /c call "<bat>"` with rigorous quoting and reject shell metacharacters in the path. Check exitCode() and distinguish "validator error" from "PDF invalid"; add waitForStarted so "didn't start" ≠ "timed out".</D1>
  <D2 sev="🟠 false success" title="Exporters verify bytes hit disk">ConversionManager exportToWord/Excel/Csv/PowerPoint/Text/Html and convertImagesToPdf return true without checking QTextStream::status()/QFile::error()/zip_close() return / non-empty output. FIX: verify write success + non-empty output before returning true; propagate zip_close failure. (BatchMode trusts these returns.)</D2>
  <D3 sev="🟠 supply chain" title="Verify OCR model/language downloads">OcrEngine.cpp:48-118 writes/loads downloaded .traineddata with only size bounds — no hash. FIX: ship a manifest of expected SHA-256 per language and verify before write/load; OR make network download opt-in and rely on bundled tessdata. Add `element_count == product(shape)` checks before indexing ONNX output tensors (PpOcrDecoder.cpp:394-404, 514-520); clamp recognizeCrop rw (:487).</D3>
  <D4 sev="🟡" title="Harden the temp directory">TempFileManager uses a fixed predictable %TEMP%/GlyphPDF shared dir (temp-squatting; sensitive PDFs + the downloaded MSI live there). FIX: per-session randomized subdir with restrictive perms; scope stale-cleanup to GlyphPDF's own `glyph_`-prefixed entries only.</D4>
  <D5 sev="🟠" title="Verify MSI publisher before msiexec">UpdateChecker.cpp:266-299 trusts the manifest's SHA-256 (rooted only in TLS) and runs msiexec with no Authenticode check. FIX: verify the downloaded MSI's Authenticode signature/cert subject against a pinned "GlyphPDF" publisher before launching, independent of the manifest; re-verify the file hash at apply time (close the download→apply TOCTOU). NOTE: depends on AR-11 D1 producing a signed MSI to verify against.</D5>
  <D6 sev="🟡" title="Office import dimension cap + thread guard">Cap convertImagesToPdf image dimensions like OcrEngine (10000 px). Move the main-thread guard from OcrEngine::initialize onto the recognition entry points (processImage/getRawText). Clean up the stray developer-monologue comments in exportToHtml (:286-304) and its O(pages) double-load.</D6>
</deliverables>

<evidence_gate>Feed a PDF path containing `&`/spaces to veraPDF → no shell execution, correct tool-error vs invalid-PDF distinction. Make an exporter target unwritable → returns false. Corrupt a model hash → load refused. External-tool suites must SKIP cleanly when the tool is absent.</evidence_gate>

<success_criteria>
  - [ ] No .bat/CMD-quoting injection; veraPDF distinguishes tool-error from invalid-PDF
  - [ ] Exporters verify write + non-empty output; BatchMode no longer reports green over corrupt files
  - [ ] OCR downloads hash-verified (or opt-in); ONNX tensor bounds checked; temp dir per-session + private
  - [ ] MSI Authenticode publisher verified before msiexec; apply-time hash re-check
  - [ ] ctest green (external-tool suites skip cleanly when tool absent); atomic commits
</success_criteria>

</mega_prompt>
```

---
<!-- ===================================================================== -->
# AR-PROMPT-6 — Concurrency & performance

**Paste the block below into a fresh CC session. Effort: 3-5 days. Risk: HIGH.**

```xml
<mega_prompt id="AR-6" inherits="@house-protocol">

<session_metadata>
  Phase: Audit remediation — concurrency correctness + real parallelism
  Priority: 🔴 prefetch UAF (if not already fixed in AR-1) + 🟠 fake parallelism + latent pipeline deadlock
  Depends on: AR-PROMPT-1 (D2 prefetch cancel/join landed) | Agents: /backend | Est. context ~60% | Risk: HIGH — interleavings
</session_metadata>

<role>Senior concurrency engineer. You reason in interleavings, hold weak/shared refs to every captured object, and verify race fixes under repeat-until-fail rather than by inspection.</role>

<mission>Make parallelism either real or honestly documented, eliminate the latent pool deadlock, and remove broken-promise cache poisoning. Done = no CPU-pool thread blocks on another lane, the LRU is O(1) with a consistent hash/equality invariant, and `ctest --repeat-until-fail 5` is green.</mission>

<context><avoidance_rules>Keep the persistent GPU warm worker (never spawn-per-page) and the dedicated CPU pool (not the global pool). Verify under `ctest --repeat-until-fail 5`; add TSan/helgrind notes if feasible.</avoidance_rules></context>

<inputs>src/engines/pdfium/PdfiumBackend.cpp (m_mutex 89,128,183); src/engines/RenderCache.cpp/.h (LRU 157,180,237,248; hash/eq h:32-55; fulfillment 75-115; memory pressure 129-131); src/engines/scheduling/PipelineStage.h (60-67); src/engines/ocr/LayoutEnsemble.cpp (detect 198-220); src/engines/scheduling/LaneScheduler.cpp (91); src/engines/mrc/MrcPageProcessor.cpp (335-344); src/engines/ocr/OcrPipeline.cpp (347)</inputs>

<deliverables>
  <D1 sev="🔴 architecture" title="Real render parallelism">PdfiumBackend serializes every render behind one m_mutex (:89,128,183), so LaneScheduler/CrossPagePipeline/prefetch are fake parallelism and background prefetch can stall foreground UI. FIX: use a POOL of PdfiumBackend instances (one document per worker) for concurrent render/extract, OR explicitly document render as serial and give background prefetch lower priority so it can't starve foreground. Pick one and justify in a comment.</D1>
  <D2 sev="🟠 perf + correctness" title="O(1) LRU + consistent hash/equality">RenderCache LRU is O(n) per hit via QList::removeOne+prepend (:157,180,237,248) → O(n²) scrolling; hashing uses exact double bits while equality uses qFuzzyCompare (RenderCache.h:32-55), violating the hash invariant (duplicate tiles, missed lookups); XOR-combined hashing collides. FIX: intrusive LRU (std::list + QHash<key,{value,iterator}>); quantize scale/subRect to a grid and hash/compare the quantized integers via qHashMulti.</D2>
  <D3 sev="🟠 latent deadlock" title="Remove block-a-pool-thread waits">PipelineStage.h:60-67 stage2 and LayoutEnsemble::detect (:198-220) block a CPU-pool thread on waitForFinished() for GPU-lane results; with setLaneCapacity(CPU,2) and backpressure 4 the pool deadlocks. FIX: continuation-chaining (futures driving futures, `.then(...).unwrap()`) instead of blocking a worker; assert/clamp `backpressure < cpuPool.maxThreadCount()`. A CPU-lane task must not synchronously fan out to and block on the GPU lane.</D3>
  <D4 sev="🟡" title="pageSize broken-promise + closure lifetimes">RenderCache.cpp:75-115 — if the fulfilling thread throws, the page's future stays broken forever, poisoning that page's metadata cache. FIX: try/catch the fulfillment, set a default on failure, erase the entry under the write lock. Audit EVERY async closure to hold weak/shared refs to ALL captured objects, not just the primary (systemic root cause of the UAFs).</D4>
  <D5 sev="🟡" title="Hot-path + MRC perf">Throttle checkMemoryPressure() (:129-131) instead of a syscall per getOrRender. Replace MRC per-pixel pixSetPixel (MrcPageProcessor.cpp:335-344) with packed-row writes via pixGetData/pixGetWpl (or pixThresholdToBinary). Make setLaneCapacity(GPU,...) (LaneScheduler.cpp:91) warn/assert instead of silently no-op. Delete the dead OrderedResultQueue template or actually route through it; document the disjoint-index write contract in OcrPipeline.cpp:347.</D5>
</deliverables>

<evidence_gate>Run the pipeline at `setLaneCapacity(CPU,2)` + backpressure 4 → must not deadlock (was the failing config). LRU: a scroll-stress test shows no duplicate tiles and bounded memory. All under `--repeat-until-fail 5`.</evidence_gate>

<success_criteria>
  - [ ] Render concurrency is real (instance pool) or honestly documented + prefetch deprioritized
  - [ ] RenderCache LRU is O(1) with consistent hash/equality; no duplicate-tile waste
  - [ ] No CPU-pool thread blocks on another lane; backpressure < cpu capacity enforced; no deadlock at low capacity
  - [ ] No broken-promise cache poisoning; closures hold weak/shared refs to all captures
  - [ ] Memory-pressure throttled; MRC packs rows; dead code removed; ctest green under repeat (paste summary)
</success_criteria>

</mega_prompt>
```

---
<!-- ===================================================================== -->
# AR-PROMPT-7 — Annotations-in-PDF + UI threading + close safety

**Paste the block below into a fresh CC session. Effort: 2-4 days. Risk: MEDIUM-HIGH.**

```xml
<mega_prompt id="AR-7" inherits="@house-protocol">

<session_metadata>
  Phase: Audit remediation — data fidelity + UI responsiveness
  Priority: 🔴 annotations never written to PDF + 🔴 UI-thread freezes + 🔴 quit-loses-work
  Depends on: none | Agents: /frontend (primary), /backend | Est. context ~50% | Risk: MEDIUM-HIGH
</session_metadata>

<role>Senior Qt application engineer. You keep the UI thread free, never lose user work silently, and prove data fidelity by re-opening the saved artifact in a second reader/engine.</role>

<mission>Annotations survive in the PDF itself (verified by re-opening with the engine), long operations never freeze the UI, and quitting with unsaved work always prompts. Done = annotate→save→re-open shows the annotations in the object graph, and the named long ops run on workers with progress + cancel.</mission>

<inputs>src/shell/controllers/HomeController.cpp (onSave); src/ui/CommentsWidget.cpp (404); src/modes/PdfAValidationPanel.cpp (runValidation); src/shell/ModeStrip.cpp (117-163); src/modes/PagesMode.cpp (page count 462-491; split 687-754); src/shell/controllers/SecurityController.cpp (QProgressDialog sites); src/GpMainWindow.cpp (closeEvent — absent); src/ui/PdfViewerWidget.cpp (setOverlayImage 1073)</inputs>

<deliverables>
  <D1 sev="🔴 data fidelity" title="Embed annotations into the saved PDF">HomeController::onSave writes a .ann JSON sidecar but never calls IPdfEditorEngine::embedAnnotations() (only a comment references it, CommentsWidget.cpp:404). Every highlight/comment/shape/signature is invisible in other readers and lost if the sidecar is separated. FIX: route Save / Save As through embedAnnotations so annotations are written into the PDF; keep the sidecar only as an editable-source cache. TEST: annotate → save → re-open with the engine → assert annotations present in the PDF object graph.</D1>
  <D2 sev="🔴 freeze" title="Get long ops off the UI thread">veraPDF (PdfAValidationPanel::runValidation) and signature validation (ModeStrip.cpp:117-163) run synchronously on the GUI thread; PagesMode derives page count via ~12 synchronous extractPageAsBytes calls (:462-491) and split runs N×extract+insert without pumping the loop (:687-754). FIX: move all to QtConcurrent/QThread workers with QFutureWatcher; use the real pageCount(); cache validation/signature results; show busy progress.</D2>
  <D3 sev="🟠" title="Cancelable progress everywhere">SecurityController builds QProgressDialog(.., QString(), ..) (no cancel button) for sign/encrypt/redact/sanitize/certify/timestamp; encrypt/permissions never show() the dialog. FIX: real cooperative cancel (kill the subprocess / abort the worker); call show() uniformly; reword internal jargon ("Applying redactions asynchronously…").</D3>
  <D4 sev="🔴 data loss" title="Save-on-exit">No closeEvent exists. FIX: override GpMainWindow::closeEvent; when DocumentSession::isDirty, prompt Save / Discard / Cancel. Add a test or manual-verify note.</D4>
  <D5 sev="🟡" title="setOverlayImage dead code">PdfViewerWidget::setOverlayImage (:1073) stores an image and calls update() but there is no paintEvent, so it never draws. FIX: implement painting or remove the API and its callers.</D5>
</deliverables>

<evidence_gate>D1's proof is the RE-OPENED PDF object graph, not the sidecar. For D2, demonstrate the UI thread stays responsive (no synchronous engine call on the GUI thread remains — grep the changed call sites).</evidence_gate>

<success_criteria>
  - [ ] Annotations embedded in the PDF on Save/Save As (verified by re-opening); sidecar is cache-only
  - [ ] veraPDF, signature validation, page-count, split all run off the UI thread with progress + cancel
  - [ ] Every long security op is cancelable; progress dialogs shown consistently
  - [ ] Quitting with unsaved changes prompts Save/Discard/Cancel
  - [ ] No dead overlay API; ctest green (paste summary); atomic commits
</success_criteria>

</mega_prompt>
```

---
<!-- ===================================================================== -->
# AR-PROMPT-8 — UI truth & dead-surface cleanup

**Paste the block below into a fresh CC session. Effort: 2-4 days. Risk: MEDIUM.**

```xml
<mega_prompt id="AR-8" inherits="@house-protocol">

<session_metadata>
  Phase: Audit remediation — "every surface works or isn't shown" (SCOPE LOCK §5)
  Priority: 🔴 mock content presented as real + misleading labels + ~70 dead "future release" buttons
  Depends on: none | Agents: /frontend | Est. context ~55% | Risk: MEDIUM
  Already-landed (verify, don't redo): OCRMode demo-data removal (v1.3.1); winget version bump; update-check first-run notice. Reconcile against current code before editing.
</session_metadata>

<role>Senior product-UI engineer enforcing "every surface either works or is not shown." You never present mock data as real and never print a fabricated value (a save time, a version) the system doesn't actually hold.</role>

<mission>Every visible control is wired or removed; every label reflects real state; no fabricated data. Done = no "Planned for a future release" disabled control remains, OCR/Compare show real document state, and destructive actions are confirmed.</mission>

<inputs>src/modes/OCRMode.cpp (241-347 — partly addressed in v1.3.1: verify); src/modes/CompareMode.cpp (31); src/shell/ModeStrip.cpp (132-144); src/shell/MenuBar.cpp (61); src/shell/RibbonModel.cpp + Ribbon.cpp (plannedTools); src/modes/SignaturesPanel.cpp (90-107); src/modes/PagesMode.cpp (toolbar 151-167; thumbnails 510-511); src/modes/PdfAValidationPanel.cpp; src/ui/FindBar.cpp (75-77,116-120); src/modes/BatchMode.cpp (643-646); src/ui/EncryptionDialog.cpp; src/ui/PreferencesDialog.cpp (105)</inputs>

<deliverables>
  <D1 sev="🔴; Pattern 5" title="Remove fake/mock content presented as real">OCRMode.cpp:241-347 seeded hardcoded fake results ("$2,418M / 14.2% / ROVER 64%", static 12-item page list) — VERIFY this is already removed (v1.3.1) and finish any remnant; CompareMode.cpp:31 hardcodes "Q4-Report-v1↔v2". FIX: start empty ("Run OCR to begin"); populate from the real document; set compare labels from the actual files.</D1>
  <D2 sev="🔴; Pattern 6" title="Fix misleading labels">ModeStrip.cpp:132-144 shows "● AUTOSAVED · <time>" on dirty docs and prints QTime::currentTime() as a save time when none exists. FIX: dirty → "● UNSAVED (autosaved hh:mm:ss)"; never print current time as a save time. Single-source the version string (MenuBar.cpp:61 hardcodes "1.0.1" vs UpdateChecker::currentVersion()/CMake PROJECT_VERSION).</D2>
  <D3 sev="🔴; SCOPE LOCK" title="Hide planned tools; wire or remove dead surfaces">RibbonModel/Ribbon render ~70 tools permanently disabled with "Planned for a future release" (= preview banners the scope-lock forbids). FIX: HIDE planned tools until implemented. Wire or remove: SignaturesPanel decorative fields (:90-107), PagesMode 9-button toolbar with no connections (:151-167), PdfAValidationPanel inert Fix/Convert/Jump buttons, CompareMode "Close Compare" / RedactMode "Cancel" trapped-mode buttons.</D3>
  <D4 sev="🟠 data loss" title="Destructive-action safety">FindBar "Redact All" (:75-77,116-120) is one-click destructive with no confirm and a search-not-complete race; BatchMode overwrites existing outputs without confirm for ≥2 files (:643-646); EncryptionDialog has no confirm-password field and no "lost password = unrecoverable" warning. FIX: add confirmations / ensure search completion / confirm-password + warning; disable the permissions group until an owner password is entered.</D4>
  <D5 sev="🟠" title="Real thumbnails + atomic reorder">PagesMode thumbnails are blank gray squares (:510-511); reorder applies N separate reorderPages calls (partial-failure risk, not one undo). FIX: render real thumbnails; apply one permutation atomically as a single undo command.</D5>
  <D6 sev="🟡" title="Privacy default + a11y/RTL/DPI pass">update/checkOnStartup (PreferencesDialog.cpp:105) — audit recommends default OFF or first-run consent (a first-run notice already landed in v1.3.1; reconcile to the audit's preference). Replace fixed-px / 9-10px fonts with layout-driven point sizing; ship + honor the RTL toggle (currently itself disabled) and use leading/trailing alignment; add setAccessibleName/tooltips/mnemonics to icon-only and ribbon controls. Use QDialogButtonBox for consistent button order across dialogs.</D6>
</deliverables>

<context><avoidance_rules>Telemetry stays absent (verified clean) — do not add any. Outbound calls limited to update check (opt-in), signing OCSP/TSA, local OCR/Ollama. "Every surface either works or is not shown" — no new disabled-with-tooltip placeholders.</avoidance_rules></context>

<evidence_gate>Grep the ribbon model for residual "future release"/plannedTools-disabled entries → none visible. TestRibbonIntegrity (already enforces enabled⇒handled and no planned-yet-handled tool) must stay green after hiding. Confirm no label prints currentTime() as a save time.</evidence_gate>

<success_criteria>
  - [ ] No fake OCR/Compare content; labels reflect real state; one version source
  - [ ] No "future release" disabled buttons; every visible control is wired or removed
  - [ ] Redact All / batch overwrite / encryption have confirmations + warnings; real thumbnails; atomic reorder
  - [ ] Update-check default reconciled to audit preference; a11y names/tooltips/mnemonics; RTL shipped; DPI-safe sizing; ctest green
</success_criteria>

</mega_prompt>
```

---
<!-- ===================================================================== -->
# AR-PROMPT-9 — Djot dual-model correctness + ProvenanceGuard

**Paste the block below into a fresh CC session. Effort: 4-6 days. Risk: HIGH.**

```xml
<mega_prompt id="AR-9" inherits="@house-protocol">

<session_metadata>
  Phase: Audit remediation — WS2 headline interchange claim
  Priority: 🔴 "lossless Djot↔Semantic" is unimplemented (decode is a stub) + ProvenanceGuard is decorative
  Depends on: none | Agents: /backend | Est. context ~60% | Risk: HIGH — the product's differentiation depends on these being true
</session_metadata>

<role>Senior document-model engineer. You make the "lossless" claim true by asserting STRUCTURE on a real round-trip, and you make the security guard a type-level chokepoint no caller can bypass.</role>

<mission>djotToDocument produces a real tree (round-trip asserts structure, not "" or non-null), and the lossy PDF write is unreachable without a guard-minted token. Done = the un-marked fuzz/round-trip tests pass and applySemanticToPdf cannot be called without a ProvenanceToken.</mission>

<inputs>src/pdfws_djot/LuaDjotCodec.cpp (264-316); tests/TestDjotRoundtrip.cpp (50); tests/TestDjotFuzz.cpp (QEXPECT_FAIL 397,447,530,542); src/shell/controllers/HomeController.cpp (140); src/pdfws_djot/ProvenanceGuard.cpp (25); src/docmodel/PdfStructureMapper.cpp (190, 211); src/docmodel/Block.h/.cpp (11) + Inline.h/.cpp (10) + SemanticDocument.h; README.md/CHANGELOG.md ("lossless" claim)</inputs>

<deliverables>
  <D1 sev="🔴; Pattern 5" title="Implement Djot decode">LuaDjotCodec::djotToDocument (:264-316) parses then DISCARDS the AST and returns an empty document, so the round-trip yields "" for all input. FIX: implement the Lua-AST → SemanticDocument walk (mirror of the documentToDjot emitter). Change TestDjotRoundtrip.cpp:50 to assert STRUCTURE (sections count, block/inline types, text), and un-mark the QEXPECT_FAILs in TestDjotFuzz.cpp:397,447,530,542 (only after they genuinely pass). Until decode is real, the "lossless" claim must be removed from README/marketing.</D1>
  <D2 sev="🔴" title="Enforce ProvenanceGuard by type, not convention">Both call sites hardcode EditPath::DirectStructural (HomeController.cpp:140), so the refusal branch (DjotThenSave && isSigned, ProvenanceGuard.cpp:25) is unreachable; the signed-doc guarantee holds only because Djot-save-back isn't wired. FIX: (a) derive origin/isSigned from the loaded document's provenance, never hardcode; (b) make the codec/mapper the chokepoint — applySemanticToPdf must REQUIRE a ProvenanceToken minted only by the guard, so no caller can reach the lossy PDF write without passing the gate. Replace bare-false return (PdfStructureMapper.cpp:211) with a distinct NotSupported outcome (or throw ProvenanceViolation).</D2>
  <D3 sev="🟡" title="SemanticDocument model hygiene">Strict ownership tree uses shared_ptr (Block.h/Inline.h/SemanticDocument.h) → use unique_ptr with non-owning back-refs; replace base-class virtual getters returning empty statics (Block.cpp:11, Inline.cpp:10) with a proper variant/visitor or distinct node classes; one namespace per library (pdfws vs pdfws_djot); reuse one lua_State for the round-trip benchmark instead of per-call.</D3>
</deliverables>

<evidence_gate>The round-trip test must assert structural equality (section/block/inline counts + text), and the fuzz tests must pass WITHOUT QEXPECT_FAIL. Demonstrate D2 by attempting applySemanticToPdf without a token → won't compile / is rejected at the type boundary. If decode can't be made lossless in this pass, DO NOT un-mark the fuzz tests and DO remove the "lossless" claim — report the gap.</evidence_gate>

<success_criteria>
  - [ ] djotToDocument produces a real tree; round-trip test asserts structure; fuzz QEXPECT_FAILs removed/passing
  - [ ] applySemanticToPdf requires a guard-minted token; provenance derived from the doc; NotSupported is distinct
  - [ ] SemanticDocument uses unique_ptr + typed nodes; single namespace; ctest green (paste summary)
  - [ ] README/marketing "lossless" claim matches code reality
</success_criteria>

</mega_prompt>
```

---
<!-- ===================================================================== -->
# AR-PROMPT-10 — Architecture refactors (interfaces, DI, errors, secrets)

**Paste the block below into a fresh CC session. Effort: 1-2 weeks. Risk: MEDIUM (broad, mechanical).**

```xml
<mega_prompt id="AR-10" inherits="@house-protocol">

<session_metadata>
  Phase: Audit remediation — structural quality (elite-bar)
  Priority: 🟠 maintainability + testability + a privacy defect (secrets dropped off-Windows)
  Depends on: AR-PROMPT-3,4 landed (so interface decomposition follows real call patterns) | Agents: /architect (primary), /backend | Est. context ~65% | Risk: MEDIUM — land in small reviewable commits
</session_metadata>

<role>Senior software architect. You land broad structural change as a series of small, individually-green commits, never breaking the build between them, and you never let a refactor silently drop a security/privacy guarantee.</role>

<mission>Replace the god-interface with role interfaces, inject dependencies instead of passing a service locator down five layers, unify the error boundary, and make secret storage real on every platform. Done = each step is individually green, no AppContext* is threaded through layers, and ISecretStore never silent-fails.</mission>

<context><avoidance_rules>Preserve the core→engines→commands→ui dependency direction; no reversals. Keep tests passing at EACH step (not just at the end).</avoidance_rules></context>

<inputs>src/core/interfaces/IPdfEditorEngine.h (~60 methods; PageOcrResult fwd-decl :16); src/core/interfaces/IPdfPage.h + IPdfDocument/Renderer/Writer/Searcher; src/core/AppContext.h (~290 derefs across layers); src/app/main.cpp (128); src/core/ErrorInfo.* (isOk); src/docmodel/PdfStructureMapper.cpp (190); src/core/CredentialManager.* ; src/core/interfaces/IFormManager.h; CMakeLists.txt (pdfws_core links Qt6::Gui)</inputs>

<deliverables>
  <D1 sev="🟠" title="Decompose IPdfEditorEngine + delete dead interfaces">Split the ~60-method god-interface into role interfaces (IPdfDocumentIO, IPageEditor, IImageEditor, IRedactor, IEncryptor, IExporter, ISignatureAware). Delete zero-implementer IPdfPage; either promote IPdfDocument/Renderer/Writer/Searcher into real seams or remove them. Engines may implement several.</D1>
  <D2 sev="🟠" title="Replace AppContext service-locator with DI + fix lifetime">Inject the specific shared_ptr<I...> each component needs via its constructor; stop passing const AppContext* down five layers (~290 derefs). Keep AppContext only as the composition root Bootstrapper fills. Fix the raw-pointer-to-stack-local lifetime (main.cpp:128 → window owns context by value/unique_ptr).</D2>
  <D3 sev="🟠" title="Unify the error boundary">Adopt one convention — Result<T, ErrorInfo> / std::expected. Ban exceptions crossing the engine boundary (the codec throws std::runtime_error into bool-returning callers). Stop encoding errors as document content (PdfStructureMapper.cpp:190 error-paragraph). Fix ErrorInfo::isOk() to key on severity, not message-emptiness.</D3>
  <D4 sev="🟠 privacy" title="Real ISecretStore">CredentialManager no-ops off Windows (returns false, drops API keys). Build ISecretStore with DPAPI / libsecret / macOS Keychain backends + an explicitly-labeled encrypted-file fallback; never silent-fail.</D4>
  <D5 sev="🟡" title="Keep core layer clean">pdfws_core links Qt6::Gui and forward-declares an engine-owned PageOcrResult (IPdfEditorEngine.h:16). Move shared types into core (or a types target); keep heavy GUI types out of the core contract where feasible. IFormManager should operate on an open-document handle, not reload the file per field.</D5>
</deliverables>

<evidence_gate>After each commit: `cmake --build build` clean AND `ctest -j4` green (paste the summary at least at start, midpoint, and end). Grep confirms no `const AppContext*` parameter threads through engine/command/ui call sites after D2. ISecretStore has a test proving no-silent-failure on the fallback path.</evidence_gate>

<success_criteria>
  - [ ] Role interfaces replace the god-interface; dead interfaces removed; mocks shrink accordingly
  - [ ] Components receive injected dependencies; no AppContext* passed down layers; no stack-lifetime risk
  - [ ] One error convention; no exceptions cross the engine boundary; no errors-as-content
  - [ ] ISecretStore works on all platforms with no silent failure; core layer free of engine-owned types; ctest green
</success_criteria>

</mega_prompt>
```

---
<!-- ===================================================================== -->
# AR-PROMPT-11 — Build / release hardening + legal

**Paste the block below into a fresh CC session. Effort: 2-4 days (+cert procurement). Risk: MEDIUM.**

```xml
<mega_prompt id="AR-11" inherits="@house-protocol">

<session_metadata>
  Phase: Audit remediation — release gates (MUST precede any public tag)
  Priority: 🔴 unsigned MSI + 🔴 unmet AGPL/GPL source obligations + 🟠 guards never exercised
  Depends on: none (procure code-signing cert in parallel) | Agents: /devops (primary), /legal-review | Est. context ~50% | Risk: MEDIUM
  Already-landed (verify, don't redo): winget manifests bumped to current version + new ProductCode; branded MSI wizard. License-identity reconcile + signing remain.
</session_metadata>

<role>Senior release engineer + license-compliance reviewer. You never publish an unsigned or license-incomplete artifact, and you make every release gate fail loudly in CI rather than on one dev's machine.</role>

<mission>The pipeline cannot publish an unsigned MSI or one missing required source offers/licenses; the license guards actually fire in CI. Done = an unsigned or license-incomplete artifact is un-publishable by the pipeline, proven by a failing gate.</mission>

<context><avoidance_rules>Subprocess isolation does NOT waive AGPL §6/GPL §3 source-conveyance — D2 is mandatory before distributing. Do not ship any public 1.x until D1+D2 are complete (CLAUDE.md §9 gate).</avoidance_rules></context>

<inputs>packaging/build-msi.ps1; packaging/GlyphPDF.wxs; packaging/deploy.ps1 (151); LICENSE-3RD-PARTY.md; CMakeLists.txt (license guards 94-119; feature find_package QUIET); packaging/winget/* ; CLAUDE.md (§9); packaging/signing-instructions.md</inputs>

<deliverables>
  <D1 sev="🔴" title="Authenticode-sign EXE + MSI">No signtool anywhere. FIX: sign GlyphPDF.exe before `wix build` and the MSI after, with an RFC-3161 timestamp; compute the published SHA-256 AFTER signing; gate the release pipeline so an unsigned artifact is never published. Procure EV/OV cert; until then, sign with a placeholder/dev cert and wire the step so it's a one-line cert swap. (This also unblocks AR-5 D5's publisher check.)</D1>
  <D2 sev="🔴 legal" title="AGPL/GPL compliance for bundled veraPDF + OpenJDK">deploy.ps1:151 bundles veraPDF (AGPL-3.0) + its OpenJDK 21 (GPL-2.0+CPE) with no source offer; neither is in LICENSE-3RD-PARTY.md. FIX: add VERAPDF-SOURCE-OFFER.txt + the JDK legal/ tree to the deploy; add both rows to the matrix; gate deploy on their presence. Stage a `licenses/` dir containing EVERY upstream LICENSE/COPYING/NOTICE (PDFium, OpenSSL, qpdf, Tesseract, Leptonica, ONNX, OpenJPEG, jbig2enc, Lua, Djot, Qt, PoDoFo) — a summary table is not the license text.</D2>
  <D3 sev="🟠; §6" title="Make the license guards real">The MuPDF/Poppler FATAL_ERROR guards (CMakeLists.txt:94-119) never run in CI and only match bare target names. FIX: broaden to namespaced targets (Poppler::poppler, unofficial::poppler, etc.); add a CI matrix leg that `pacman -S`'s poppler/mupdf and asserts `cmake` configure FAILS.</D3>
  <D4 sev="🟡" title="Binary hardening + build determinism">Add -fstack-protector-strong, -D_FORTIFY_SOURCE=2, -Wl,--dynamicbase,--nxcompat,--high-entropy-va, CFG where supported; enable LTO; strip staged binaries (they leak `C:\Users\User\Projects\pdf` DWARF paths). Default CMAKE_BUILD_TYPE=Release when unset (documented `cmake -B build` currently yields an unoptimized, assert-enabled build). Bundle the official vc_redist payload instead of copying the build host's System32 CRT.</D4>
  <D5 sev="🟠" title="Release-feature + winget integrity">Hard-fail release builds if any shipped-feature define (HAS_PDFIUM/HAS_TESSERACT/HAS_RAPIDOCR/HAS_QPDF) is unexpectedly off (find_package(.. QUIET) silently drops features today); emit a feature/version summary. VERIFY winget manifests are current (already bumped — confirm ProductCode/URL/SHA match the signed MSI) and reconcile the MIT-vs-Apache license identity across winget/portable/CLAUDE.md.</D5>
</deliverables>

<evidence_gate>Prove the gate, not the intent: temporarily feed the pipeline an unsigned artifact → it refuses to publish. Run the license-guard CI leg with poppler present → cmake configure fails. Confirm signtool output shows an RFC-3161 timestamp and the published hash is of the SIGNED file.</evidence_gate>

<success_criteria>
  - [ ] EXE+MSI Authenticode-signed (RFC-3161 timestamp); unsigned artifact is un-publishable (gate proven)
  - [ ] veraPDF + OpenJDK source offers + full upstream license trees staged; matrix complete
  - [ ] License guards fire in a CI leg; namespaced targets covered
  - [ ] Hardening flags + LTO + strip; default Release; bundled vc_redist; release fails on missing features
  - [ ] winget manifests current + match the signed MSI; license identity reconciled
</success_criteria>

</mega_prompt>
```

---
<!-- ===================================================================== -->
# AR-PROMPT-12 — Test-coverage backfill + CI gates

**Paste the block below into a fresh CC session. Effort: 3-5 days. Risk: LOW-MEDIUM. RUN LAST.**

```xml
<mega_prompt id="AR-12" inherits="@house-protocol">

<session_metadata>
  Phase: Audit remediation — prove the fixed behavior is real (Patterns 1/2)
  Priority: 🟠 close the test gaps the audit found masking defects
  Depends on: the prompts whose code each test covers (run AFTER 1-11) | Agents: /qa | Est. context ~45% | Risk: LOW-MEDIUM
</session_metadata>

<role>Senior QA/test engineer. You write tests that would FAIL on the pre-fix code, assert behavior/structure (never pointers), and gate the build in CI so regressions can't reach a tag.</role>

<mission>Every CRITICAL fix from AR-1..9 has a regression test that fails on pre-fix code; no security path is only-tested-under-#ifdef; CI builds + tests + runs the license guards + a clean-VM smoke launch. Done = `--repeat-until-fail 5` green and the CI gates exist and pass.</mission>

<inputs>tests/TestRedaction.cpp; tests/TestSignatureRealCrypto.cpp (422); tests/TestDjotRoundtrip.cpp + TestDjotFuzz.cpp; tests/ (PII/PDFium-gated, OCR model fixture, RESOURCE_LOCK); .github/workflows/ (ci.yml, release.yml); packaging/deploy.ps1 (validation gate)</inputs>

<deliverables>
  <D1 title="Activate + add security regression tests">Confirm testRedactionOnSignedDocIsBlocked is a slot (AR-2 D7). Add: redaction verification scanning streams + dicts; CTM-transformed-text leak; content-stream injection (NUL/newline/parens); signature re-validation after DSS/TS append; VRI-key reference; OCSP bypass compiled out in release. Each must fail on the corresponding pre-fix code (note how you confirmed).</D1>
  <D2 title="Remove environment-masking gaps">Pattern-redact PII tests must run without PDFium (or the suite documents the gap as a HARD CI requirement, with a PDFium-present CI leg). Provide a small committed OCR model fixture (or a CI leg that fetches+hash-verifies models) so RapidOCR real-inference is actually exercised. Djot round-trip asserts structure (AR-9 D1).</D2>
  <D3 title="CI gates">Add (or extend .github/workflows/): build + ctest on every push; the license-guard leg (AR-11 D3); a release leg that runs deploy.ps1's validation gate and a clean-VM smoke launch (the installer is currently built only on one dev's machine and never verified to launch on a clean box); assert the shipped-feature defines are on.</D3>
  <D4 title="Clean up stale test metadata">Fix misleading QEXPECT_FAIL/"until M5" comments (TestSignatureRealCrypto.cpp:422), the vacuous non-null Djot assertion, and any test asserting nothing. Confirm RESOURCE_LOCK on I/O-sharing tests; run `--repeat-until-fail 5`.</D4>
</deliverables>

<evidence_gate>For each new regression test, demonstrate it FAILS against the pre-fix behavior (e.g. by stubbing back the bug or citing the AR-prompt that fixed it and showing red→green). A test that passes on both old and new code is not a regression test — fix it.</evidence_gate>

<success_criteria>
  - [ ] Every CRITICAL fix from AR-1..9 has a regression test that fails pre-fix (documented)
  - [ ] No security path only-tested-under-#ifdef; OCR real-inference exercised in CI
  - [ ] CI builds + tests + runs license guards + clean-VM smoke + feature-define assertions
  - [ ] No vacuous/misleading tests; `--repeat-until-fail 5` green (paste summary)
</success_criteria>

</mega_prompt>
```

---

## Vault & CLAUDE.md updates (apply on the local machine)

The Obsidian vault (`C:\Users\User\.claude\memory\projects\glyphpdf\`) may be unreachable from a remote
container; from a local session it is editable. Apply:

1. **New note `10-audit-2026-06-16.md`** — consolidated findings (13 CRITICAL + HIGH/MEDIUM/LOW), linking this
   file. Front matter: tags `#audit #glyphpdf #remediation`, date 2026-06-16.
2. **`01-current-state.md`** — "2026-06-16 full-codebase audit (8 domains). Headline claims (true redaction,
   lossless Djot, PAdES B-LTA, signed MSI) NOT fully met in audited code. Remediation in
   `docs/planning/AUDIT-2026-06-16-REMEDIATION.md` (AR-PROMPT-1..12). Trustworthy public 1.x blocked on AR-1/2/3/11."
3. **`08-lessons-learned.md`** — Pattern 20: "wired-to-UI-not-to-reality recurred in headline features
   (visual-only redaction path; Djot decode stub; decorative ProvenanceGuard; annotations sidecar-only) with
   tests that only checked non-null / were QEXPECT_FAIL. A green suite is not evidence a feature exists; assert
   behavior/structure, not pointers."
4. **`06-non-negotiables.md`** — "Redaction must chain document-level sanitize + GC save + post-open
   verification, single path. B-T must embed a signature timestamp. No test hooks in production crypto paths."
5. **`CLAUDE.md` §0/§5** (in-repo) — record the audit + point to this file.

---

### Changelog of this upgrade (v2 vs the generated v1)
- Recast all 12 prompts into the `<mega_prompt>` schema (role / mission / context / inputs / deliverables /
  evidence_gate / success_criteria) while preserving every file:line and exact fix mechanism.
- Added a single shared `<house_protocol>` carrying anti-self-report, verify-on-disk, verification-before-completion,
  and failure-handling rules — referenced by every prompt instead of repeated 12×.
- Reframed each objective as an outcome ("Done = …") and added a per-prompt `<evidence_gate>` (the observable that
  proves the deliverable, e.g. "re-open the output and scan", not "it compiled").
- Localized PHASE-0 reading to reachable in-repo files; vault files are optional-if-present.
- Annotated the items already (partly) landed in v1.3.0/v1.3.1 (OCR demo-data removal, winget bump, update first-run
  notice, branded installer) as "verify, don't redo" so a fresh session reconciles rather than duplicates.
