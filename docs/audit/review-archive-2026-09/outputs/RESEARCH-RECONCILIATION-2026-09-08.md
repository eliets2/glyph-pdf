# GlyphPDF — research reconciliation and workflow handoff

**Use the research for workflow design, but correct its GlyphPDF status and ranking assumptions before implementing the synthesis.** The immediate work remains source-preserving saves, document identity, reliable history and truthful completion. The new code review supplies direct counterexamples to several research claims of already-guaranteed safety.

Research location: `C:\Users\User\Projects\pdf\.context\research\`. That directory is absent from `pdf-parity`; reading it does not move the software review back to main. The review target remains `b58b91054ca7a573a09c56d950588ac4f966df42`.

## What was read

The folder initially contained 17 curated briefs: 16 competitor reports and one community report. **`synthesis.md` appeared during this review and was also read**, bringing the curated set to 18. I read the briefs' methods, conclusions, GlyphPDF deltas, failure/experience findings and recommendations, with additional relevant feature tables; the synthesis was read in full. The raw HTML, PDFs, JSON and text caches were inventoried, not exhaustively revalidated. This is a reconciliation of existing research, not a new hands-on review of all competitors.

The evidence archive records a hash inventory of the local inputs and the sections selected for reading. Four useful workflow references were additionally checked on their official sites: ABBYY's correction workflow, Bluebeam's saved filters, Sejda's OCR disclosure and iLovePDF's compare modes. Prices, legal disputes, market share, review scores and claims of absolute superiority were not refreshed or adopted here.

## Corrections to make before consuming `synthesis.md`

| ID | Research issue and exact location | Correction / implementation consequence |
|---|---|---|
| RQ01 | `synthesis.md` method and “moat” M4 turn ledger implementations into broad verified safe-save guarantees. Similar claims appear in `masterpdf.md` failure #2, `okular.md` Finding 2, and several GlyphPDF delta tables. | EC01 reproduces engine source truncation; EC02 and ARC01/ARC02 reproduce cross-document state; ARC03 closes after failed Save. Keep previous FormManager/redaction acceptances scoped to their specific boundaries. Remove broad safety marketing until the real application boundaries pass. |
| RQ02 | Synthesis T2-7 marks DocMDP certification MISSING; T3-1 marks recipient certificate encryption MISSING. | Current source already contains `SignatureManager::certifyDocument`, DocMDP transform generation and `SecurityController` Certify dispatch; `PdfEditorEngine` also contains recipient certificate encryption and `PdfEncryptPubSec`. Reclassify as **implemented; exact caller/interop acceptance to assess**, not “build from scratch.” Source presence is not whole-feature acceptance. |
| RQ03 | Synthesis T1-2 says selective excision ships in 14/16 while its own text identifies Sejda and Okular as absent; the inputs also identify rasterized output and undocumented mechanisms. | Even the two stated absences cap that count at 14; the prose then mixes selective excision, rasterized removal and unknown implementations under one label. The corpus must be recounted by actual mechanism before publishing an excision count. Treat “no competitor offers proof” as **not established**, not 0/16 proven absence. A missing documentation page does not prove absent code. |
| RQ04 | Synthesis M1 counts “3/16” by including GlyphPDF in the list, although the denominator is 16 competitors; other rows combine absent, paid, capped and cloud features into “don't.” | Keep GlyphPDF outside the competitor denominator. Use separate fields for capability existence, platform, tier, processing location, depth and evidence confidence. Price-gated existence still counts as existence. Publish counts only when every included row can be reconstructed. |
| RQ05 | Synthesis groups PDFgear/UPDF/PDF Expert as cloud task grids whose every free tier is metered and whose documents upload by default; their own briefs distinguish local desktop cores from cloud AI/web tools, and PDFgear's brief describes an uncapped core. | Use per-feature/per-platform locality, not a whole-product label. For GlyphPDF, “loopback accepted” proves endpoint policy, not what another local process does with data. CapabilityRegistry is a disclosure mechanism, not a network enforcement audit. |
| RQ06 | `updf.md` OCR delta, `pdf24.md` §2.3 and several other briefs infer better OCR from ROVER/dual-engine architecture. | No common-corpus accuracy benchmark supports that conclusion. D06 still reports usable models without successful initialization, and three newest RapidOCR checks skipped for missing models. Require actual model initialization, multilingual saved-text tests and a measured corpus before accuracy claims. |
| RQ07 | Synthesis method and C3/C11 blur LANDED, implemented-awaiting-review and verified. Some briefs describe form auto-detect as compound-undo complete and PDF/A/accessibility as accepted. | Preserve the independent ledger protocol. Real heuristic detection exists, but V06's controller history gap remains; PDF/A version mapping N03 is accepted only for versions; ARC06 proves the host panel is unwired. Reading-order heuristics do not establish accessible-PDF conformance. |
| RQ08 | Synthesis treats competitor feature prevalence as demand proof, snippets as frequency estimates and large features as cheap/no-risk. T1 measurement says “no engine risk”; T3 MCP says a local endpoint keeps all bytes local. | Treat prevalence and search-result samples as prioritization signals, not market-size estimates or proven demand. Calibration, CropBox/rotation/units, annotation round-trip, scripting and external callers add real contracts. A local MCP server can still serve a client that transmits content elsewhere. No such feature is authorized by reading this research. |
| RQ09 | `okular.md` Finding 2 treats non-byte-identical saved output as proof of unsafe saving. Several summaries similarly mix whole-output identity with source preservation. | A valid edited PDF normally changes bytes. Distinguish unchanged source on failed/new-output operations, preservation of unaffected content, and validity of the intentionally changed output. Test each contract instead of requiring impossible whole-output identity after edits. |
| RQ10 | Synthesis resolves conflicting PRD/cloud requirements as though the research can choose product policy; AI/JavaScript/print recommendations also conflict across briefs. | Treat these as design proposals. Keep current user-authorized scope: code review first, then latest-build UI review. Do not let a research note silently authorize new network services, a scripting runtime, pricing changes or a broad print engine. |

Concrete current source anchors for RQ02 are `src/engines/SignatureManager.cpp:1432,1658`, `src/shell/controllers/SecurityController.cpp:243,824`, and `src/engines/PdfEditorEngine.cpp:839–950` at the pinned revision. These were read directly; no new certification/encryption interoperability test was run for this research reconciliation.

Several research briefs appropriately flag uncertain edition matrices, missing checkmarks in extracted PDFs, unavailable pages, snippet-only community evidence and vendor-reported accuracy. Preserve those caveats in the combined matrix. Do not turn “not documented” into “does not exist,” an old vulnerability report into a current exploit claim, or local processing into a regulatory-compliance claim.

## Useful ideas, translated into bounded implementation work

These are design recommendations following the current defects, not additional claims that GlyphPDF already implements them.

### 1. OCR correction should retain source context and keyboard flow

ABBYY documents navigation through low-confidence text with a magnified source region and direct correction. Sejda explicitly offers language/output choices and tells users to review recognition results. Adopt that clarity in the existing OCR Verify screen. [ABBYY correction workflow](https://help.abbyy.com/en-us/finereader/16/user_guide/checkingtext/), [Sejda OCR](https://www.sejda.com/ocr-pdf).

**Use:** existing OCR review/session/navigation components. Correct D03/D04/D06 and V05 before expanding controls. Maintain one selected word, source region and confidence definition; show a truthful unavailable state when models cannot initialize. Make next/previous uncertain word, correction, skip and source zoom usable by keyboard. Distinguish skipped review from corrected text.

**Acceptance:** actual installed models; low-confidence and no-suspect documents; Latin plus Arabic/mixed-script fixtures; rotated/cropped scans; failure/cancel then retry; A→B during recognition; edits after recognition starts. Reopen the saved PDF and verify both text and position. Report confidence as a model signal, not a guarantee. Add dictionary/custom-language systems only after a demonstrated need.

### 2. Comments should behave as one persistent review list

Bluebeam documents per-column filters and reusable saved filters for review work. This is a useful next step for the existing comments table after persistence is correct. [Bluebeam Filter List](https://support.bluebeam.com/revu/how-to/manage-and-review-markups-with-filter-list.html).

**Use:** existing annotation records and table/filter/export path. Repair ARC02/ARC04 first. Add a small status vocabulary and saved filter settings only if they round-trip in the existing serializer. Row selection, count, page navigation and export should use the same filtered result. Keep actual document records separate from user display preferences. Start with the existing columns; a formula/custom-column subsystem and proprietary Bluebeam interchange are separate proposals.

**Acceptance:** A/B document switches and pending saves; filtering and clearing; numeric page ordering; replies; Unicode/commas/newlines in exported CSV; persisted statuses and reload. A future printable summary must show the same selected records and identify the source/revision.

### 3. Task entry and completion should explain the actual operation

The task-oriented Sejda/iLovePDF/Smallpdf briefs emphasize clear verbs, input/output choices and a short completion path. Keep GlyphPDF's existing TaskNav/CapabilityRegistry and improve those contracts rather than rebuilding the shell.

**Use:** a consistent preflight stating input document, selected pages, operation, output location and concrete unavailable reason. For split, show final collision-resolved filenames before execution. For redaction, distinguish marked, applied, sanitized, partial, canceled and failed states. For signatures, retain the effective outcome of retries. A completion screen can offer opening the output or a compatible next task, but only after commit succeeds.

**Acceptance:** fix NCR-01, D01 and N08; one of two split parts fails and the UI identifies it; no source file becomes an output alias; recovered sanitization displays the recovered state; labels fit the saved rectangles. Reopening the result must match the completion message.

### 4. Compare should connect evidence to the right page pair

iLovePDF's current compare page presents separate text and content-overlay modes with synchronized scrolling. That separation is useful: text changes and visual changes answer different questions. [iLovePDF compare](https://www.ilovepdf.com/compare-pdf).

**Use:** first finish the existing comparison model and mapping contract. Feed structural alignment into both navigation and the text-row pairing where required; explain blank/image-only limitations. Keep an optional visual overlay as a separate future mode, not evidence that current text alignment is correct.

**Acceptance:** insertion plus an edit, repeated pages, page reorder, short rewritten text, image-only pages and different page sizes. A selected report row must target the same old/new page pair in both views and exports. Retain V04 and the current token-by-index limitation until independently fixed.

### 5. Batch presets come after trustworthy individual operations

The Acrobat, Foxit, ABBYY and Sejda briefs repeatedly propose named reusable sequences. This is a reasonable later improvement over the existing batch/hot-folder engine, provided individual operations have explicit outcomes and source preservation.

**Use:** a small versioned preset schema over already-supported operations; validated input/output paths, deterministic naming and per-item/per-step status. Reuse current capability checks. Reject unknown operations/options rather than executing arbitrary commands. Do not add a new workflow framework or agent service to store a short ordered list.

**Acceptance:** bad preset, unavailable model, source/output aliases, second-step failure, cancel between files, rerun, existing destinations and one failed item among successes. Preserve originals and make partial completion reviewable. Avoid inferring “already OCR'd” from any nonempty text on a mixed page; test mixed digital/scanned documents before adding skip-text behavior.

## Map of all curated inputs

| Local brief / key sections | Useful contribution | Current dependency or disposition |
|---|---|---|
| `abbyy-finereader.md` §1, Top 5 | Source zoom, uncertainty navigation, editable recognition regions | OCR identity/readiness/geometry first; no inferred accuracy superiority |
| `acrobat.md` Loved workflows, delta, Top 10 | Visual page organization, review summaries, repeatable actions | Transaction/history repair; certification and encryption already have implementations |
| `bluebeam.md` §5, Loved workflows | Filtered review list, statuses, saved filters and summaries | ARC02/ARC04 first; defer custom formulas and AEC breadth |
| `foxit.md` §2–§6 | Saved batch actions, capability explanations, deployment controls | Reliable jobs and INF02–INF06 first |
| `ilovepdf.md` §3, §5; compare table | Clear task verbs, result-to-next-task handoff, distinct compare modes | NCR-01/D01 and real page-pair mapping |
| `libreoffice-draw.md` delta, §7 | Explicit conversion fidelity and validation boundaries | Reopen outputs; separate rasterized export from selective editing; no PDF/A tag-only acceptance |
| `masterpdf.md` Loved workflows, Top 5 | Type-specific object selection and property editing | EC03/EC05/V02 history and save contracts before deeper editing |
| `nitro.md` A/C, Top 10 | Familiar action vocabulary, reviewed detection, reusable requests | Treat NER, local signing packages and MCP as future proposals |
| `okular.md` Findings 2–4, summary | Keyboard annotation loop and explicit interchange behavior | ARC01/ARC02/ARC04; correct byte-identity reasoning |
| `pdf24.md` §2, §4–§5 | Batch defaults, skip-text ideas, deployment predictability | Mixed-page tests and release configuration before broader automation |
| `pdfexpert.md` corrections, §3–§5 | Task continuity, reading comfort and annotation summary | UX reference; no Windows UI comparison performed; repair autosave/session first |
| `pdfgear.md` §17–§19 | Quick area text extraction and low-friction page tasks | D06/V05 and validated existing action boundaries; no automatic AI expansion |
| `pdfxchange.md` §16, source limits | Selection detail, review history and explicit export limitations | Existing snapshots/serialization first; conformance and tier assumptions remain separate |
| `r-pdf-mining.md` method, Findings, Top 10 | Directional demand for local processing, reliable output and verifiable redaction | Search samples are not a census; stale July implementation claims corrected by current evidence |
| `sejda.md` §3–§4, synthesis | Preflight disclosure, language/output choices and filename templates | Capability truth and collision-safe output paths |
| `smallpdf.md` §17–§20 | Simple presets and guided task handoff | Preserve useful advanced controls; verify every preset changes actual output |
| `updf.md` §17–§18 | Direct task access and reading customization | Reliability before surface breadth; local-core/cloud-AI distinction |
| `synthesis.md` full | Combined candidate backlog and explicit source conflicts | Apply RQ01–RQ10; insert a correctness/release stage ahead of all feature tiers |

## Revised order and later UI review

The first stage is the [consolidated code-review queue](C:/Users/User/Documents/Codex/2026-09-05/read-c-users-user-projects-pdf/outputs/TEAM-QUALITY-REVIEW-2026-09-08.md). Next, finish the five existing workflows above. Measurement, general form JavaScript, a signing-request package, OCG authoring, XFA, a public automation endpoint and full accessibility remediation remain individually scoped proposals. Competitor counts alone do not justify placing them ahead of reproduced data-loss defects.

After code acceptance and a separately identified latest installer, review the installed UI with synthetic fixtures: fresh Open→edit→save; dirty document switch and Undo; annotation persistence; OCR correction and saved text; compare navigation; redaction partial/retry; split collision/partial completion; keyboard navigation and DPI/theme layouts. Record build identity and observed behavior. The older installed application was not used as evidence for current package quality.
