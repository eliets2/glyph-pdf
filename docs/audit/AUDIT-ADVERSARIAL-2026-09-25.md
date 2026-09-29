# ADVERSARIAL AUDIT — full-codebase Wave 1 (native-adversary)

**Date**: 2026-09-29
**Auditor**: native-adversary (Wave 1 of the full-codebase audit)
**Target**: GlyphPDF `audit/sweep-all` @ 7eb5c67b — entire `src/` tree (~379 files, ~98.9k lines)
**Mode**: AUDIT-ONLY. Zero production-code changes. Findings below are **static adversarial analysis** (code-reading + grep sweeps across every `src/` subtree). No dynamic repro, sanitizer run, or fuzz artifact was produced in this round — per doctrine §4.C every finding below is therefore a **LEAD with an exploitability verdict**, not a dynamically promoted Finding. Evidence-class labels are explicit on each item.

**Method**: threat-enumeration over the ten attack surfaces in the audit brief; trust-boundary mapping (file open, sidecar files, embedded JS, Lua codec, network manifests, policy files, temp/session dirs); grep-driven sweeps for injection/memory/DoS patterns; refutation attempts against each candidate before reporting (pins checked where they exist).

---

## Verdict summary

| # | Sev | Surface | Class | Verdict | One line |
|---|-----|---------|-------|---------|----------|
| AD-01 | HIGH | signing/verification | CWE-345 (unauthenticated revocation data) | STATIC-CONFIRMED (logic-complete; dynamic repro pending) | DSS/OCSP entries are consumed without verifying the OCSP responder's signature — an attacker-appended (ISA-allowlisted) /DSS revision can forge "good" revocation status for a revoked signer |
| AD-02 | MED-HIGH | signing/ISA detection | CWE-20 (parser/regex mismatch) | HYPOTHESIS (needs one dynamic test) | The shadow-attack object scanner matches `N M obj` textually; a NUL-whitespace header (`12 0\0obj`) that PoDoFo tokenizes but the regex misses would smuggle a modified base object past the ISA classifier |
| AD-03 | MED | djot codec / parsing | CWE-674 + CWE-400 (unbounded recursion) | HYPOTHESIS (cheap falsification) | LuaDjotCodec's C++ AST walkers recurse without a depth cap over attacker-shaped nesting (nested headings → nested sections); deep input = native stack-exhaustion crash; reachability gated on the born-Djot import wiring |
| AD-04 | MED-LOW | UI | CWE-116 (rich-text injection / spoofing) | STATIC-CONFIRMED | Attacker-controlled document metadata (/Title, /Author, /Subject, /Keywords) and CMS signer CN render into QLabel(AutoText) unescaped — in-app HTML rendering, spoofing/phishing vehicle |
| AD-05 | LOW-MED | annotation rich text | CWE-159 (missing scheme allowlist) | STATIC-CONFIRMED (inert today, latent) | The djot `[text](url)` emitter writes any scheme (`file:`, `ms-msdt:`, `javascript:`) into `<a href>` — inert in the current previews (no anchorClicked wiring), but the same XHTML is written to /RC for third-party viewers |
| AD-06 | LOW | form JS sandbox | CWE-400 (deadline bypass) | HYPOTHESIS | QuickJS interrupt handler polls in the interpreter loop; catastrophic regex backtracking inside libregexp may not poll — a hostile /AA script could exceed the 250 ms budget |
| AD-07 | LOW | signing/verification | CWE-190 (narrowing order) | STATIC-CONFIRMED (unexploitable dead end) | `/Contents`-hole framing checks `static_cast<int>`-narrow offsets BEFORE the INT_MAX Malformed guard; on >2 GiB files the wrong-byte path runs first, but the later guard terminates — defects in check ORDER, not in outcome |
| AD-08 | LOW | form JS host | CWE-770 (bounded-only-per-string allocation) | OBSERVATION | `report.logs`/`blocked` aggregate per-event transfers; each is capped (4 MiB egress, 16 MiB engine heap, 1 s cascade) but the aggregate across events reaches tens of MB in host QStringLists |
| AD-09 | RESIDUAL | several | CWE-367 (TOCTOU windows) | OBSERVATION | Same-user race windows: policy owner-check vs read; redaction proof re-reads `sourcePath` after commit; update file swap between handle-verify and the msiexec child's own open |
| AD-10 | INFO | djot codec | hardening | OBSERVATION | `sandboxLuaState` nils io/os/loadfile/dofile/debug but keeps `load` and `require` (path-restricted) alive in the Lua state that parses hostile input |
| AD-11 | MED-LOW | redaction guards | CWE-636 (fail-open) | STATIC-CONFIRMED | `hasPdfSignatures`/`hasXfaDocument` swallow parse errors and answer "unsigned"/"XFA-free" — a malformed AcroForm bypasses the ER-2/G1 redaction refusals |

Count: **1 HIGH, 2 MED(+1 MED-HIGH), 3 LOW-MED, 3 LOW, 1 INFO, plus refutations.** All are static leads — promotion path for each is listed in the finding.

---

## CONFIRMED / LEAD findings

### AD-01 — DSS OCSP revocation status is consumed without responder-signature verification (revocation bypass via allowlisted ISA revision) — HIGH

**Component / asset**: `src/engines/SignatureManager.cpp`
- `Private::extractOcspFromDss` (lines 776–930) — extracts, certID-matches, returns; **never calls `OCSP_basic_verify` / never authenticates the responder**
- consumption (lines 3044–3115): REVOKED → downgrade to "Revoked"; `OCSP_check_validity` (thisUpdate/nextUpdate) → freshness; GOOD → nothing (stays "Valid"/"ValidWithDSS")
- the delivery vehicle: `isLegitimateIncrementalAppend` (lines 2391–2409, 2431–2443) — the ISA allowlist that *permits* an unsigned catalog `/DSS` addition and every new object reachable from the DSS subtree

**Entry point**: a signed PDF, then an attacker-appended incremental revision (plain e-mail/cloud transport — the document leaves the signer's control after signing). The revision adds `/DSS` with `/OCSPs [ <attacker DER> ]` plus matching `/Certs`.

**Bug class / CWE**: CWE-345 (insufficient verification of data authenticity), CWE-494-adjacent (untrusted data treated as validated).

**Attacker narrative (end-to-end)**:
1. A document signed with cert C is distributed. Cert C is later revoked (compromise, repudiation — the exact case revocation checking exists for).
2. Attacker appends an incremental revision: catalog gains `/DSS` (nothing else — the INV-1 allowlist at 2397–2409 explicitly blesses "DSS and nothing else"), DSS contains `/OCSPs` with a self-crafted OCSPBasicResponse: certID computed over C's issuer/serial (all public), `certStatus = good`, `thisUpdate = today`, `nextUpdate = +10 years`, signed by any throwaway key, plus that key's cert in `/Certs` (DSS-subtree objects are allowlisted at 2431–2432).
3. `validateSignatures`: chain verify passes (unchanged, ByteRange intact), `extractOcspFromDss` certID-matches (890–898), status is GOOD (3090 not taken), freshness passes (3098 — attacker-chosen window) → `trustStatus` stays **"ValidWithDSS"** / `isValid = true`. The UI presents the revoked signer as fully valid with long-term validation material.

The revocation machinery as wired is **payload-authenticated nowhere on the consume path**: no `OCSP_basic_verify`, no responder-cert match against the issuing CA, no delegation check, no nonce (NF-6 acknowledged at 3075–3080), no producedAt policy. A matching certID and a fresh window are trivially forgeable because they are unkeyed public data.

**The asymmetry is provable against the app's own invariant (D3)**: the SIGNING side already enforces the correct bar before embedding anything it fetched — `OCSP_basic_verify(basic, certs, ocspStore, 0)` against the trust store plus the issuer cert, with the explicit comment "they MUST NOT be blindly trusted" for chain intermediates (`SignatureManager.cpp:1754–1804`). The VERIFY side consumes the exact same data structure from an attacker-appendable location and skips every one of those checks. This is an internal-inconsistency defect: the intended invariant exists (D3), only the consume site fails to enforce it. And the delivery vehicle is confirmed narrow-but-open: `inv1CatalogUpdateAddsDssOnly` (2159–2191) treats "/DSS [as] the one free key" — a catalog rewrite that adds /DSS and nothing else passes, and every new object reachable from that DSS is allowlisted by `isLegitimateIncrementalAppend` step 2 (2431–2432).

**Evidence**: static, code-complete (function-level; every cited line read this round, including both OCSP paths end-to-end). No dynamic repro executed in this audit — promotion requires a repro: sign a doc with a test CA, revoke the cert, append the forged-DSS revision, assert `validateSignatures` reports Valid/ValidWithDSS (it must NOT). Handed to fuzz-harness-engineer as the top target; the existing `TestSignatureRealCrypto` fixtures carry a real CA (and even a `revoked_ocsp_response.der` fixture path, `SignatureManager.cpp:1791–1796`) and can host this directly.

**Exploitability**: high (logic-complete chain; every step attacker-feasible with public data and a text editor for DER construction via OpenSSL CLI).
**Blast radius**: trust-decoration of revoked signatures — a reviewer relying on GlyphPDF's verdict accepts a document whose signer cert is revoked. Does not forge coverage of modified bytes (ByteRange checks remain sound).
**Remediation (describe-only)**: at the consume site (validateSignatures, after `extractOcspFromDss` returns a DER), re-run the D3 verify block before reading status: `OCSP_basic_verify` against the *trust store* plus the signer's issuer cert (which the chain verify already derived), refusing in-DSS intermediates as trust anchors; require the responder to be the issuer or an authorized delegate (OCSPSigning EKU machinery from D-7 is reusable); optionally enforce a maxAge on thisUpdate. Fail-closed on any verify error (map to "UntrustedChain", never to ValidWithDSS). Symmetry with the embed path is the acceptance criterion: any response the app would refuse to EMBED at sign time must also be refused as evidence at verify time.
**Regression check**: new pin — forged GOOD response must NOT sustain "ValidWithDSS" (must downgrade); genuine responder-signed GOOD must keep it; forged REVOKED against a legit-good situation should still downgrade (it is unauthenticated either way — after the fix, unauthenticated entries must be refused entirely, both directions).

---

### AD-02 — ISA/shadow classifier can be evaded by tokenizer-vs-regex whitespace divergence — MED-HIGH (HYPOTHESIS)

**Component / asset**: `src/engines/SignatureManager.cpp:2380–2388` — `isLegitimateIncrementalAppend` extracts redefined/new object numbers with
`QRegularExpression("(\\d+)\\s+(\\d+)\\s+obj")` over `QString::fromUtf8(trailingBytes)`, then only validates objects it can see. Any appended object header this regex fails to match is **never checked** — modified base objects beyond the catalog/Info allowlists and new objects outside the DSS/timestamp allowlists pass silently, defeating the ISA downgrade at 3138–3145 (trustStatus stays "Valid").

**The gap**: PDF whitespace (ISO 32000-1 §7.2.2) includes NUL. PoDoFo's tokenizer (PoDoFo 1.x `IsWhitespace`) treats `\0` as whitespace, so `12 0\0obj << ... >> endobj` in an appended revision parses as object 12 — but QRegularExpression's `\s` does **not** match NUL, so the classifier never sees the redefinition. The attacker redefines a base object (e.g. a `/Pages` node or page `/Contents` — a *shadow attack*) with a NUL-separated header while the crypto verification of the untouched ByteRange still passes: content changed, verdict "Valid".

**Falsification plan (one test)**: build a base PDF with a signed ByteRange; append an incremental update whose redefinition of object N uses `N G\0obj`; parse with the vendored PoDoFo to confirm it honors the redefinition; assert `isLegitimateIncrementalAppend` returns false (it currently would return true if PoDoFo accepts NUL). If the vendored PoDoFo *rejects* NUL-whitespace headers, this hypothesis is refuted and the pin should be committed to freeze that assumption.
**Caveat stated honestly**: PoDoFo's source is not in this tree (`third_party/podofo/install` ships headers only), so the whitespace set could not be read here — hence HYPOTHESIS, not FINDING.
**Secondary hardening (independent of NUL)**: the scanner should not trust a textual object scan at all — enumerate the appended revision's objects from the parsed structure (the new xref's entries), which removes the entire class.
**Blast radius**: undetected post-signature content modification (shadow attack) presented as valid.
**Remediation**: parse the trailing revision's xref/trailer and validate every object it *actually defines* (structural enumeration), or at minimum replace `\s` with the full PDF whitespace class including `\0`, and scan raw bytes (not through QString UTF-8 decoding, which also mangles invalid sequences).

---

### AD-03 — LuaDjotCodec: unbounded C++ recursion over attacker-shaped AST (stack-exhaustion crash) — MED (HYPOTHESIS, cheap falsification)

**Component / asset**: `src/pdfws_djot/LuaDjotCodec.cpp`
- `walkSection` (544–566) recurses per nested section; djot nests a section per heading-level increase, so input `# A\n## B\n### C\n...` (2–4 bytes per level) yields an arbitrarily deep section tree. The Lua instruction budget (M-1, `kMaxInstructions`, lines 66–83) bounds *time*, not *depth*; nothing bounds C++ stack.
- `walkInline` (400–442) and `collectInlineText` (385–397) recurse per emphasis nesting level (`__…__`-style wrapping is ~2 bytes per level).
- Additionally the `docmodel::Section`/`Block`/`Inline` shared_ptr trees are recursively destroyed — even a successful parse of a deep tree crashes later in the destructor.

**Entry point / reachability (stated precisely)**: `djotToDocument` is constructed in `Bootstrapper.cpp:50` and stored on `AppContext.djotCodec`; the born-Djot file-open import is not fully wired in this tree (HomeController.cpp:206–213 comments the wiring as pending; `.djot` open routes provenance tagging only). Today the decode is exercised by `tests/TestDjotFuzz` and test harnesses; the *encode* side (`documentToDjot`) recurses on the same section/inline trees built from OCR pipeline output (shallow today). The crash lands the moment djot import consumes attacker bytes — and the fuzzer should prove it now, before the wiring ships it.
**Impact if true**: native stack-overflow crash (DoS) from a file open; with GlyphPDF shipping no sandbox, an attacker-controlled stack overflow in the render/parse process is scored per doctrine §4.B against standard technique vocabulary (controlled stack smashing is a classic code-execution precondition, though no such primitive is demonstrated here).
**Falsification plan**: feed `djotToDocument` 100k ascending headings (`for i in 1..100000: "#"*i + " h\n"` — ~6 MB, well under the 256 MiB Lua cap and the instruction budget); expected: stack overflow crash. Second input: 50k-deep nested emphasis. Remediation (describe-only): depth cap in walkSection/walkInline/collectInlineText with honest failure (like the M-1 budget), plus an iterative destructor or depth-guarded tree build.
**Confidence**: 0.7 (recursion is certain; only frame-size/goroutine assumptions stand between 10k depth and a crash — Windows default 1 MB stack overflows near ~3–10k frames for these frame sizes).

---

### AD-04 — Rich-text injection into QLabel(AutoText) from attacker document data — MED-LOW

**Component / asset** (representative sites, all verified this round):
- `src/ui/DocumentPropertiesWidget.cpp:107–113` — `/Title`, `/Author`, `/Subject`, `/Keywords`, `/Creator`, `/Producer` from the loaded PDF → `QLabel::setText` (default `Qt::AutoText`: any string that `Qt::mightBeRichText` scores as HTML renders as rich text)
- `src/ui/SignaturesWidget.cpp:65` — signature list label is the **CMS signer CN** (fully attacker-chosen on a self-signed cert)
- `src/modes/SignaturesPanel.cpp:227` — signer name into a details label

**Attacker narrative**: a hostile PDF with `/Title <h1>Document is corrupted — call +1-… or open support.glyphpdf-secure.example</h1>` (or a self-signed cert whose CN carries the same payload) renders formatted, colored, table-shaped spoofed UI *inside GlyphPDF's own properties/signature panels* — a phishing/masquerade vehicle aimed at exactly the trust-relevant surfaces (signature identity display). QLabel executes no script and does not auto-open links, so this is spoofing, not RCE/XSS-exec; the signature list is the worst site because it directly decorates trust decisions.
**Evidence**: static (Qt AutoText semantics + unescaped attacker strings at the cited lines; no `toHtmlEscaped` on those paths — the CompareWidget/BatchMode sites, by contrast, DO escape, showing the pattern is known and these sites were missed).
**Remediation**: set `Qt::PlainText` explicitly on every label that displays document-derived or cert-derived strings, or escape at the seam (one `toHtmlEscaped()` at the model→view boundary).
**Regression check**: pin that a `<h1>` Title renders literally in the properties widget and a `<b>` CN renders literally in the signature list.

---

### AD-05 — djot link emitter lacks the scheme allowlist the viewer link path has — LOW-MED (latent)

**Component / asset**: `src/pdfws_djot/DjotToRichTextXhtml.cpp:62–65` — `link()` writes `<a href="%1">` with the URL only XML-escaped; **any scheme** passes (`file:///C:/Users/Public/x.exe`, `ms-msdt:`, `search-ms:`, `javascript:`).
**Context**: the page-link path enforces http/https/mailto (`src/ui/PdfViewerWidget.cpp:590–640`, "§9.1 P0 DEFECT 2(A)"), proving the threat model is owned — the djot emitter simply never got the same rule.
**Current exploitability**: the two preview widgets are read-only QTextEdits with **no** `anchorClicked` wiring (verified by grep), so links are inert in-app today. But the same `djotToXhtml` output is written to annotation `/RC` (`src/engines/podofo/PoDoFoBackend.cpp:5006–5008`) and is consumed by third-party viewers per §12.5.6.4 — a GlyphPDF-authored comment can carry a hostile-scheme link into other applications. And any future anchorClicked→QDesktopServices wiring turns the preview into the exact P0 the viewer already fixed.
**Remediation**: apply the same `isSafeLinkScheme` allowlist inside `XhtmlInlineSink::link` (and drop the href for anything else, keeping the visible text).
**Regression check**: `[a](ms-msdt:x)` must emit `<a>`-less text in the XHTML and /RC.

---

### AD-06 — QuickJS deadline may not fire inside native regex backtracking — LOW (HYPOTHESIS)

**Component / asset**: `src/engines/formjs/FormJsSandbox.cpp:151–161` (interrupt handler) — the whole-operation deadline is enforced via `JS_SetInterruptHandler`, which QuickJS polls from the JS interpreter loop. quickjs-ng's libregexp is a backtracking engine; whether regex execution polls the interrupt handler is not established in this tree (engine source not vendored here). A hostile `/AA /K` script running `/(a+)+$/.test("a".repeat(60))` would otherwise hold the UI thread far past the 250 ms budget on every keystroke.
**Falsification**: run that script under the sandbox, measure wall time vs deadline. If it exceeds the budget materially, the fix is a regex-time budget (engine-side) or dropping RegExp from the exposed intrinsics.
**Blast radius**: UI hang/DoS on form fill (attacker-authored form); bounded to the process.

---

### AD-07 — /Contents-hole framing checks narrow to `int` before the INT_MAX guard — LOW (check-order defect)

**Component / asset**: `src/engines/SignatureManager.cpp:2649–2671` (`static_cast<int>(off1 + len1)` in the hole framing and gap extraction) vs the INT_MAX Malformed guard at **2715–2723**. On a >2 GiB crafted file the narrowed negative offsets reach `QByteArray::mid` (which clamps) and `fileData.mid(pos-20, 20)` before the guard rejects the signature as Malformed anyway. Net effect today: dead end, no exploitable read — but the guard must run FIRST; any future refactor that trusts the gap checks would inherit the wrong-byte path.
**Remediation**: hoist the INT_MAX bounds guard above the hole-framing block (pure reordering).

---

### AD-08 — Form-JS log/block sinks aggregate to tens of MB in host memory — LOW (observation)

`EgressGuard` caps each JS→C++ *transfer* (4 MiB) and the engine heap (16 MiB), but `runCalculateCascade` appends every event's `logs`/`blocked` into `CascadeReport` (`FormJsRunner.cpp:338–339`) — with the 1 s cascade budget that is up to ~4–5 events × up to 4 MiB of JSON each, realized as QStringLists on the host, per cascade, per fill step. Bounded, but multi-MB spikes from a hostile form are possible; consider a total-bytes budget across the cascade.

---

### AD-09 — Same-user TOCTOU windows (residual set) — RESIDUAL/OBSERVATION

All require local write access at or above the victim's own privilege — noted as accepted-risk candidates, not attack-surface:
- **Policy file** (`src/core/PolicyController.cpp:151–161`): admin-ownership verified, then content read — a directory-writable attacker can swap bytes in between (PROGRAMDATA inheritance makes this dependent on deployment ACLs; W1-05's structural close is otherwise complete).
- **Redaction proof** (`src/engines/RedactOperation.cpp:709–725`): the proof re-reads `sourcePath` from disk after the output is committed; a source mutation in that window yields a garbage verdict (false-pass or false-fail) rather than corruption.
- **Update apply** (`src/core/UpdateChecker.cpp:386–397`): SHA-256 + Authenticode verify the held handle, then `startDetached("msiexec", …)` — the child re-opens the path after our handle closes; a same-user process can swap the file in that gap (MSI's own Authenticode prompt is suppressed by `/qb`).

---

### AD-10 — Lua sandbox keeps `load`/`require` alive — INFO (hardening)

`sandboxLuaState` (`src/pdfws_djot/LuaDjotCodec.cpp:42–52`) nils io/os/loadfile/dofile/debug but leaves `load` (Lua 5.4 base) and `require` (path-restricted to the djot dir) in the state that parses hostile input. The vendored djot parser never passes input to `load`, so this is defense-in-depth only — but the entire reason the sandbox exists is that vendored-parser trust ages poorly. Nil them after library load like their siblings.

---

### AD-11 — Redaction preflight gates (signed / XFA) fail OPEN on parse errors — MED-LOW

**Component / asset**: `src/engines/podofo/PoDoFoBackend.cpp` — `hasPdfSignatures` ("Cannot determine — treat conservatively as unsigned" → `return false`) and `hasXfaDocument` ("Cannot determine — treat as XFA-free"). Both swallow `PdfError` from the field-walk / dictionary lookups and report the *unsafe* answer. These are the ER-2 and G1 gates consumed by `RedactOperation::execute` (488–494) and `PdfEditorEngine`'s four signature-guarded mutations (368, 1622, 1670, 2018).

**Attacker narrative**: a hostile document whose `/AcroForm /Fields` iterator throws mid-walk (corrupt field dictionary, dangling indirect ref) reports "unsigned"; redaction then proceeds on a signed document — the exact scenario the ER-2 refusal exists to prevent (original content recoverable / silent signature destruction). Same shape for XFA: detection failure → redaction proceeds → the G1 "second copy of every form value in XFA streams" leaks into the redacted output; the RedactionProof would catch string survivors only when the proof is requested and the XFA copy is a literal byte match.
**Bug class / CWE**: CWE-636 (not failing closed) / CWE-755.
**Note**: `hasXfaDocument`'s failure is partially backstopped when sanitize runs (it scrubs XFA keys regardless of detection), but sanitize is optional in the redact transaction.
**Remediation**: invert the failure direction — on `PdfError` during signature/XFA determination return `true` (signed / has-XFA) so the refusal fires; the honest-cost direction ("redaction refused on an undecidable doc") is a workflow annoyance, the current direction is a silent safety-gate bypass.
**Regression check**: pin that a document whose /Fields entry throws reports `hasPdfSignatures() == true`; same for a throwing AcroForm on the XFA probe.

---

## Surfaces attacked and REFUTED (with pins)

| Candidate | Refutation |
|---|---|
| PDF link annotation → arbitrary scheme/code exec | `isSafeLinkScheme` http/https/mailto allowlist enforced at the single `openUrl` seam (`PdfViewerWidget.cpp:590–640`); TolerantMode scheme parse is scheme-safe |
| Batch-preset naming traversal (W1-01) | Remediated: `resolveNaming` containment gate on the RESOLVED name (`BatchPreset.cpp:82,128`) — the original repro's three failing slots are the regression suite |
| Sidecar aliased field bindings (W1-02) | Remediated: `fromJson` duplicate-binding lint (`SigningRequestModel.cpp:220–240`) |
| Machine-policy squatting (W1-05) | Structural close: admin-tier ownership gate at the load boundary (`PolicyController.cpp:67–90,151–161`), fail-closed, disclosed |
| CSV formula injection in text export | `csvFormulaSafeCell` at the emission boundary (`ConversionManager.cpp:497–514`) with narrow plain-number exemption (documented M3 tradeoff) |
| HTML export injection | `exportToHtml` escapes text and font names (`ConversionManager.cpp:441–442`); attribute quoting unbreakable |
| LibreOffice command injection | `QProcess` arg-list, never shell (`ConversionManager.cpp:637–645`); extension allowlist on inputs |
| Update chain (manifest→download→install) | HTTPS-enforced manifest (`UpdateChecker.cpp:44–56,64–72`), mandatory SHA-256 (319–332), Authenticode on the held handle + publisher match (347–378, 410–475), non-Windows refuses to launch |
| Form JS sandbox (escape, infinite loop, memory) | No host-I/O globals (quickjs-libc never registered); single absolute whole-operation deadline across every engine entry incl. hostile getters (the previously-bypassable gaps are closed and documented at `FormJsSandbox.cpp:163–181`); 16 MiB heap + 1 MiB stack caps; 4 MiB per-transfer egress cap; egress verbs hard-nooped with audit entries (`AFormShim.cpp:453–483`); `__proto__` literal-embed bug fixed via JSON.parse-of-string-literal (PGR-38, `FormJsSandbox.cpp:264–274`); cascade cycle dedup + hard cap (`FormJsRunner.cpp:220–232, 299–307`) |
| Redaction proof false-pass via unswept surface | Sweep covers raw bytes, object strings, decoded streams, extracted text, Info dictionary, embedded files incl. nested PDFs/OLE (unsweepable payloads → **Unswept, never Clean**, `RedactionProof.cpp:373–378,573–604`); Edact-Ray glyph-advance defense verified in BOTH directions — the excision emits single numeric-only TJ gaps (`PoDoFoBackend.cpp:2459–2476`) and the proof's operator counter refuses to count numeric-only `[N] TJ` as glyph-carrying (`RedactionProof.cpp:86–139`) so an excision cannot hide behind its own gap |
| Signed-document in-place save (ISA by the app itself) | Provenance guard refuses in-place save on signed docs (`HomeController.cpp:198–228`); redaction refuses signed/XFA documents outright (`RedactOperation.cpp:38–50, 488–494`) |
| Pre-shared-key/secret-store weakness | DPAPI-wrapped + AES-256-GCM versioned store (`EncryptedFileSecretStore.cpp`, v3 generation pins service-name GCM context) |
| ByteRange geometry attacks (overlap/offset holes) | Overlap check → `ByteRangeOverlap`; hole must start at 0, be hex/whitespace-only, and be preceded by `/Contents` (2641–2685); trailing revisions classified structurally (modulo AD-02) |

## Coverage matrix (audit brief → verdict)

| Brief area | Coverage | Outcome |
|---|---|---|
| 1 Document parsing | pdfium/podofo/qpdf backends, PoFoDictRead, djot codec, Lua | AD-03, AD-10; backends delegate to hardened libs (PoDoFo 1.1 vendored) |
| 2 Signing/certification | SignatureManager (full 3153 lines), ISA classifier, INV-1 helpers, ByteRange geometry | **AD-01, AD-02, AD-07** |
| 3 Redaction | RedactOperation (full), excision core, RedactionProof sweep breadth, hasPdfSignatures/hasXfaDocument gates | Refuted (false-pass), **AD-11** (fail-open gates), AD-09 (proof TOCTOU) |
| 4 Form/JavaScript | FormJsSandbox/Runner/AFormShim (full) | Refuted (escape/DoS), AD-06, AD-08 |
| 5 File I/O | SafeSave, TempFileManager, RedactOperation commit path | Refuted (candidate+QSaveFile+divergence re-check); AD-09 |
| 6 Export/conversion | ConversionManager (CSV/HTML/soffice), ReviewSummaryWriter, BatchMode | Refuted (escaped/arg-list) |
| 7 UI | PdfViewerWidget links, Comments/Inspector djot preview, DocumentProperties, Signatures panels, window titles | **AD-04, AD-05** |
| 8 Policy bypass | PolicyController, Capability, read-only/signature guards | W1-05 closed (verified); AD-09 residual |
| 9 Cryptography | CMS verify path, trust store (Windows ROOT/custom), OCSP, DPAPI store, PAdES/TSA fetch | **AD-01**; chain/trust handling otherwise sound (no partial-chain flag, EKU guard, weak-key guard, signing-time window) |
| 10 DoS | Lua budget/memcap, JS deadlines, cascade caps, regex scanners | AD-03, AD-06, AD-08 |

## Not covered this round (residuals for Wave 2)

- Dynamic execution: no repro built, no sanitizer build run, no fuzz corpus seeded (audit-only constraint). Every finding carries its falsification/repro plan.
- `src/engines/pdfium/PdfiumBackend.cpp` and `src/GpMainWindow.cpp` read selectively (routing/entry), not line-by-line; pdfium delegates rendering to the vendored pdfium — parser memory-safety there is upstream's to own, but the seam constants (`PdfiumEnvironment`) deserve a dedicated pass.
- `src/engines/ai/OllamaProvider.cpp` endpoint handling (policy-gated) — LLM-trust-boundary layer (17) not exercised: no prompt-content path into shell/HTML was found, but a targeted pass is cheap.
- OCR engines (`src/engines/ocr/*`: ONNX model loading, image preprocessing) — model files are trusted-content today; a Wave-2 pass should treat them as hostile inputs.
- Windows ACL reality of the deployed policy/PROGRAMDATA directories (deployment-dependent; needs an installed-machine check).

## Provenance

- Branch: `audit/sweep-all`, base commit `7eb5c67b` (fix(signing): 1.7c — B-LT DSS append NoMetadataUpdate).
- Pre-existing unstaged change `tests/TestSignatureRealCrypto.cpp` observed at audit start — NOT part of this audit's commit (left untouched).
