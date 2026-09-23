# T2-4 Accessibility Auto-Tagging (P2) — Structure-Tree Construction Engine (DESIGN ONLY)

**Date:** 2026-09-21 · **Repo:** pdf-parity @ `ec9f16f6`, branch `feat/feature-plans` (from `feat/parity-glm`)
**Status:** Design proposal for the P2 wave of Tier-2 item **T2-4** (`docs/research/synthesis.md`
§2): the content-tagging engine the P1 checker disclosed as not-yet
(`AccessibilityChecker.h:15–17`, panel honesty box: "content tagging is not yet available").
No production code and no build files are touched by this document. **Decision requests:
none** — the scope (paragraphs + headings only), the never-auto-generate-Alt policy and the
not-a-conformance-claim discipline are recorded design calls under the tasking's pick-and-
justify mandate, each with its alternative documented (§7).

**Sources for status claims (all read at pinned revision `ec9f16f6` unless noted):**
`src/engines/AccessibilityChecker.{h,cpp}` (74/373 ln: the 6 P1 checks, walk idiom, the
"PoDoFo 1.1.0's struct-tree API is getters only — construction … against raw dictionaries, as
in exportPdfA" note at `.cpp:22–24`), `src/engines/AccessibilityFixes.{h,cpp}` (SafeSave fix
transactions, /Alt + /TU seams), `src/modes/AccessibilityPanel.{h,cpp}` (ARC06 identity,
injected fix runner, honesty box), `third_party/podofo/install/include/podofo/main/`
(143 headers, NONE structure-related — no `PdfStructTree*`; `PdfContentStreamReader.h`,
`PdfTextState.h`, `PdfPage.h`, `PdfObject.h`/`PdfDictionary.h` present),
`scripts/bootstrap-vendor-deps.sh:53` (`PODOFO_VER="1.1.0"`),
`src/engines/VeraPdfValidator.{h,cpp}` (subprocess veraPDF CLI, JSON report parser,
`TestVeraPdf` fixture-friend precedent), `src/engines/SafeSave.{h,cpp}` (candidate →
validate → atomic commit + fault seams), `src/engines/podofo/PoDoFoBackend.cpp:3319+`
(sanitize's raw-dict structure-tree WALK — the construction counterpart), ledger rows
`41300f0`/`c6a56b2` (`CURRENT-EVIDENCE-LEDGER-2026-09-05.md:991–1005`), tests
`TestAccessibilityChecker.cpp` (10), `TestAccessibilityFixes.cpp` (9), `TestAccessibilityPanel.cpp` (7+2).

Confidence legend follows the research-specialist 6-level scale (TRUE … UNVERIFIABLE).
PoDoFo header facts verified against the vendored tree at `ec9f16f6`; ISO facts cite
ISO 32000-1:2008 §14.8 (Adobe-hosted full text, as in the form-js plan's source register).

---

## 0. Executive summary (3 sentences)

The P1 checker + cheap fixes landed honest DETECTION for six gap classes, but the
highest-value item on its own report — an untagged document — can only be closed by a
tagging engine, and the vendored PoDoFo 1.1.0 provides no structure-tree construction API
(143 installed headers, none structure-related; the P1 code comment already records the
getters-only reality), so this plan constructs `/StructTreeRoot` **against raw dictionaries**
(the `exportPdfA`/sanitize idiom) on top of a **marked-content rewriting pass** over page
content streams, which is the load-bearing technical risk and therefore lands first behind
a text-preservation invariant. P2 scope is deliberately narrow and honest: **paragraphs
(`P`) and headings (`H1–H6`) only** — tables, lists, figures-as-structure and
nested-layout analysis are P3; heading detection is document-relative font-size/weight
CLUSTERING with disclosed accuracy limits (it is a heuristic, never a claim); `/Alt` text is
**never auto-generated** — images without `/Alt` are collected and PROMPTED through the
existing P1 fix seam before they enter the tree, or disclosed as left untagged. The engine
ships inside the existing SafeSave transaction discipline, never claims PDF/UA (the checker's
own header sentence is repeated in the tagging UI verbatim), and its acceptance tests run
through the repo's existing veraPDF subprocess integration plus self-owned structural
walks.

---

## 1. Feature contract

### 1.1 What ships (one paragraph)

A **Tag document…** action on the AccessibilityPanel runs the tagging engine over the open
document: it extracts positioned text runs per page (content-stream walk, ToUnicode-honest),
clusters them into lines and paragraphs, ranks font-size clusters into heading levels,
wraps each resulting element's runs in BDC/EMC marked content with assigned MCIDs, and
constructs the `/StructTreeRoot` dictionary tree (`P`, `H1–H6` elements; `/ParentTree`
number tree; `/MarkInfo /Marked true`; page `/StructParents`) as raw PDF dictionaries in a
SafeSave transaction. Images without `/Alt` are never silently tagged: the pre-flight
dialog lists them, prompts for descriptions (routed through the landed
`A11yFixKind::SetImageAltText` fix), and either includes them as `Figure` elements (when
described) or excludes them from the tree with a per-image disclosure. The result is a
document whose structure tree says exactly what the heuristics found — and a report that
says what the heuristics are.

### 1.2 Scope discipline (P2 vs P3, as gates)

| Element class | P2 | Rationale / honesty note |
|---|---|---|
| Paragraphs `P` | YES | the bulk of any document; line/paragraph clustering is the core engine |
| Headings `H1–H6` | YES | the navigation payload of the tree; heuristic (§3.3), user-verifiable in the report |
| Tables | NO — P3 | needs grid detection (ruling lines/column alignment), a different engine |
| Lists (`L`/`LI`/`LBody`) | NO — P3 | needs bullet/numbering recognition; a wrong list is worse than a plain paragraph |
| Figures in tree | YES, **only with user-supplied /Alt** (§4) | auto-tagging never invents descriptions |
| Multi-column layout | NO — P2 assumes single-column reading order, DISCLOSED per document when a column-suspect signature is detected (§3.4) | reading-order recovery is layout analysis; the honest P2 answer is "may interleave columns; reported" |
| Scanned/image-only pages | NO — detected (no text runs) and disclosed; OCR-first is a P3 candidate (the OCR pipeline exists, but tagging OCR output without review is a wrong-text hazard) | |

### 1.3 The not-a-conformance-claim discipline (verbatim contract)

The checker's header sentence — *"The checker never certifies PDF/UA conformance — a report
with zero findings is NOT a PDF/UA claim"* — is extended, not replaced: **a tagged document
is not a conforming document.** The tagging panel, the tagging report and the docs all say:
auto-tagging produces a best-effort structure tree from layout heuristics; it does not
verify reading order against intent, does not produce table/list structure, and does not
issue any conformance verdict of any kind (PDF/UA, Matterhorn, WCAG). The words
"conform*", "compliant", "PDF/UA" appear in the UI only inside this disclaimer.

---

## 2. The two technical realities that shape the design

### 2.1 PoDoFo 1.1.0 structure-tree API reality-check (raw-dict construction confirmed)

Verified against the vendored install tree (`third_party/podofo/install/include/podofo/main/`,
`PODOFO_VER="1.1.0"` pinned in `scripts/bootstrap-vendor-deps.sh:53`): the 143 installed
headers contain NO structure-tree class (no `PdfStructTree`, `PdfStructElement`, no
structure ENUMs). What exists and is used: `PdfObject`/`PdfDictionary`/`PdfArray`/
`PdfReference` raw construction (the `exportPdfA` and sanitize-walk idiom, cited in the
P1 checker's own comment), plus — decisive for the extraction half —
`PdfContentStreamReader` (typed operator walk with `PdfContentReaderArgs`, form-XObject
descend/skip flags) and `PdfTextState` (text-matrix/font/size state). **Design consequence:
the engine is a raw-dictionary writer + a content-stream rewriter, both on seams the repo
already trusts; no PoDoFo upgrade, no new dependency.** (Upstream PoDoFo gained
structure-tree helpers in later lines — not vendored here, and switching vendor versions is
a user decision this plan does not request.)

### 2.2 Tagging REQUIRES content-stream rewriting (the load-bearing risk)

A structure tree without marked content is decorative: assistive technology resolves
structure elements to page content through **MCIDs** (marked-content identifiers: `/P <</MCID n>> BDC … EMC`
wrapping the element's show-text operators, page `/StructParents`, tree `/ParentTree`).
PoDoFo's painter cannot wrap existing operators, so the engine performs a **token-faithful
rewrite**: read each page's content stream through `PdfContentStreamReader`, re-emit every
token unchanged except for inserted `BDC`/`EMC` pairs around the run groups each element
claims (inside `BT…ET` text objects, around `Tj`/`TJ` sequences). Risk profile, stated
plainly: a rewriting bug corrupts pages. Mitigations, in order of strength:

1. **The text-preservation invariant (the page-count invariant's sibling):** after the
   rewrite, the candidate page's text is RE-EXTRACTED through the same extractor and must
   equal the pre-rewrite extraction (sequence of (text, y-bucket) pairs, whitespace-canonical).
   Any divergence fails the candidate — the document is left byte-identical, the failure is
   disclosed per page. This is exactly the shape of `runPresetChain`'s reopen+page-count
   validation and the fix path's independent reopen (`AccessibilityFixes`).
2. Token-faithfulness: every non-inserted byte of operator semantics round-trips through the
   reader's typed walk; inline images (`BI…ID…EI`) pass through untouched (the reader has
   explicit inline-image handling); unknown/custom operators are emitted verbatim and DISCLOSED
   as untaggable regions (they end up OUTSIDE any marked content — legal PDF, honest gap).
3. The SafeSave transaction wraps the whole thing: unique candidate → per-page invariant →
   independent PoDoFo reopen of the candidate → checked atomic commit with the shell handle
   coordinator (the `AccessibilityFixes` shape, which already proved the
   destroy-before-rename discipline for the resident handle).
4. Page-count + page-appearance guard: page count equal; per-page, the set of used-font
   resources and XObjects is unchanged (a rewrite must add operators only).

---

## 3. The tagging engine (`src/engines/AccessibilityTagger.{h,cpp}`)

### 3.1 Pipeline (per page, then document assembly)

1. **Extract** — walk the page content stream (`PdfContentStreamReader`, descend form
   XObjects with a visited set, recording the descent path for MCID placement — XObject
   content is tagged IN its own stream; `/StructParents` applies to each content stream that
   carries marked content). Each text run: unicode string (see 3.2), position (text matrix
   at show time, y for line clustering, x for alignment), font size (effective, from the
   matrix scale × `/Tf` size), font resource name → font dict.
2. **Cluster lines** — group runs by y (tolerance from the page's modal run height;
   superscripts/subscripts fold into the line by baseline proximity with the outlier rule
   disclosed in the report when it fires).
3. **Cluster paragraphs** — line groups by: x-left alignment family (within tolerance),
   vertical gap ≤ k × line height (k from the document's own modal line spacing —
   document-relative, never absolute points), no intervening size-cluster change.
4. **Classify headings** (§3.3). Everything else is `P`.
5. **Rewrite + mark** (§2.2): MCIDs assigned per content stream in reading order of the
   elements.
6. **Assemble the tree** (§3.5): document `/StructTreeRoot`, element order = page order,
   reading order within page.
7. **Figures** (§4): image XObjects the pre-flight got described → `Figure` elements with
   `/Alt`, marked via their `Do` operator's `BDC` wrap (an image is one marked-content
   sequence — no stream rewrite needed for it beyond the `Do` wrap).

### 3.2 Text honesty (ToUnicode or disclosed)

Run text decodes through the font's `/ToUnicode` CMap when present (and the standard-14
encodings otherwise). A page whose fonts lack usable Unicode maps is **skipped and
disclosed per page** ("page 4 not tagged: font 'ABCDEF+Garamond' has no ToUnicode map —
extracted text would be wrong") — the engine NEVER writes text it cannot decode correctly
into `ActualText`-less structure elements, because a wrong text run in a tree is a silent
data-corruption class for screen-reader users, the exact honesty failure this codebase
exists to prevent. (No `/ActualText` is emitted in P2 — it is a P3 candidate alongside
tables/lists; emitting it without reliable decode is the same wrong-text hazard.)

### 3.3 Heading detection: document-relative clustering, honestly labeled

- **Sizes:** collect the effective sizes of paragraph-candidate first-lines; cluster with a
  simple 1-D gap threshold (cluster break when the size ratio exceeds ~1.08 — the perceptual
  "just noticeably larger" heuristic, tuned once on the test fixtures and PINNED there, not
  tuned on live documents). Rank clusters by size descending → levels `H1, H2, H3…` down to
  `H6`, mapped by rank with duplicates collapsed to the same level.
- **Weight:** font hints (`Bold`/`-B` in BaseFont name, `/FontDescriptor /StemV` ≥ ~0.2 ×
  `/Flags`-implied weight class, `/FontWeight` when present) boost a cluster's heading rank
  (a bold size-tie breaks toward heading).
- **Disclosure, not magic:** the report lists the detected size clusters with their assigned
  levels ("18.0pt ×14 runs → H1; 14.4pt ×63 runs → body"), so the user can SEE the
  classification and judge it. Known failure modes, named in the report wording: decorative
  covers/letterheads classify as headings; dense typographic documents with 3+ competing
  sizes produce noisy levels; headings distinguished only by color/color-space are INVISIBLE
  to this engine (P2 does no color analysis — disclosed). The engine ships a "review the
  result" posture (re-scan + visual check), never a "trust the tags" posture.
- **Numbering/outline leftovers:** an existing `/Outlines` tree is NOT consulted in P2
  (merging outline semantics with layout heuristics is P3; noted as a candidate because
  outline-derived headings are the one high-precision source many PDFs carry).

### 3.4 Reading-order honesty (single-column P2)

Runs order by (line y, then x). The detector computes a cheap column-suspect signature
(bimodal x-left distribution across body lines, or >15% of lines whose gaps contain another
line's x-range) and, when positive, tags the page anyway but flags it in the report:
"multi-column layout suspected — reading order in the tree may interleave columns; review
recommended". Overwriting manual structure: if the document ALREADY has a
`/StructTreeRoot`, tagging REFUSES with an honest whyNot ("this document is already tagged;
re-tagging would discard the existing structure") — the checker's `tagged` flag gates the
action. (A "re-tag anyway" force path is P3 with explicit destructive-action confirmation.)

### 3.5 Tree assembly (raw dicts, exact shape)

`/StructTreeRoot` (`/Type /StructTreeRoot`, `/K` kids = element refs, `/ParentTree` number
tree: content-stream key → array indexed by MCID → element ref, `/ParentTreeNextKey`),
elements `/Type /StructElem`, `/S /P|/H1…/H6|/Figure`, `/P` page ref, `/Pg`, `/K` = MCID int
or array for split elements, `/Parent` links, `/ID` byte-strings optional (omitted — fewer
false conformance signals). Catalog gains `/StructTreeRoot` + `/MarkInfo <</Marked true>>`;
each touched content stream's page dict gains `/StructParents n`. Note for the sanitize
lane: `PoDoFoBackend`'s sanitizer already strips `/Alt`, `/ActualText`, `/E` during
structure-tree sanitization (D5 pass 16) — the two features compose (tag-then-sanitize is a
user error the run order makes harmless; no special handling, disclosed in docs).

---

## 4. The /Alt policy: never auto-generate, always prompt

Auto-generating image descriptions (template text, filename echoes, "image" placeholders)
writes LIES into an accessibility surface — the one class of output this engine must never
produce (a screen reader reads it as fact). The policy:

1. Pre-flight lists every image XObject lacking `/Alt` (the checker's walk already finds
   them, bounded sample + truncation totals — reused verbatim, cap raised only if needed and
   re-pinned).
2. The dialog prompts PER IMAGE with an inline editor (routed to the landed
   `A11yFixKind::SetImageAltText` fix — one SafeSave transaction each, reusing its
   validation). Description is optional-but-disclosed: "Leave blank to exclude this image
   from the structure tree (screen readers will not announce it)."
3. Described images enter the tree as `Figure` elements with `/Alt` + a `BDC` wrap on their
   `Do` operator. Undescribed images stay OUT of the tree (an unannounced image is an honest
   gap; an empty or placeholder `/Alt` is a false statement — PDF/UA reads empty `/Alt` as a
   defect, which is exactly what it would be).
4. The run report names both counts (figures tagged / images left out).

---

## 5. PDF/UA-adjacent requirements list (the honest map)

What the P2 engine produces, mapped to the PDF/UA-1 (ISO 14289-1) requirement families it
TOUCHES — with the discipline that this is a coverage map, not a checklist claim:

| PDF/UA-adjacent requirement | P2 state |
|---|---|
| `/StructTreeRoot` present, tree well-formed, elements resolve to marked content | PRODUCED (+ self-owned structural verification, §6) |
| `/MarkInfo /Marked true` | SET |
| Natural reading order plausible | HEURISTIC (single-column; multi-column flagged) |
| Headings represent document structure | HEURISTIC (clustered, disclosed per document) |
| Figures carry `/Alt` | ONLY user-described ones; others excluded + disclosed |
| Document language `/Lang`, title `/Title` + `/DisplayDocTitle` | from the P1 fixes (unchanged seams; the panel suggests them post-tag if still missing) |
| Form fields carry `/TU` | from the P1 fixes (unchanged) |
| Tables/lists structured | NOT PRODUCED (P3) — the report says so when table/list signatures are suspected |
| Conformance verdict | NEVER ISSUED (§1.3) |

---

## 6. Acceptance + test plan

### 6.1 Self-owned structural checks (the primary gate)

`tests/TestAccessibilityTagger.cpp` (fixture style of `TestAccessibilityChecker.cpp` —
seeded documents built in-test by raw dicts + PoDoFo painter content, the checker-fixture
idiom):

- **Tree well-formedness walk** (the engine's own verifier, also a public function so the
  tests don't re-implement it): every `/K` resolves; every MCID in `/ParentTree` exists in
  its stream's marked content; every element's `/Pg` matches the stream it points into;
  no orphan `/Parent` links; `/Marked true` present. Pins: seeded heading/paragraph document
  → expected element sequence `H1, P, P, H2, P`; heading levels match the fixture's size
  ladder; MCIDs in reading order.
- **Text-preservation invariant** (§2.2): byte-level extraction equality pre/post per page;
  negative control — a fault seam (`TaggerFaultForTesting`, the `SaveFault` house pattern)
  that perturbs one operator during rewrite MUST fail the candidate and leave the original
  byte-identical (SHA-256, the E-1/R01 discipline).
- **ToUnicode honesty:** page with a no-ToUnicode font → page skipped, disclosed, OTHER
  pages still tagged; no `ActualText` anywhere in P2 output.
- **Alt policy:** undescribed images never produce `Figure` elements nor any `/Alt`;
  described ones do (through the SetImageAltText seam); empty-string description refused
  (the fix seam's existing rule).
- **Already-tagged refusal;** single-column signature absent/present disclosure;
  **truncation/caps:** a 200-image fixture tags/prompt-lists within the named cap with
  disclosed totals.
- **Commit safety:** commit fault mid-tag → original byte-identical, no temp residue
  (`SafeSave::FailBeforeCommit` seam); resident-handle coordination exercised (the
  AccessibilityFixes RUN_SERIAL lesson).

### 6.2 veraPDF structural checks (the independent reader)

Through the EXISTING subprocess integration (`VeraPdfValidator` + `TestVeraPdf` offline
fixture precedent — same pattern, new fixtures):

- **PDF/UA-1 validation of tagged fixtures** where the bundled CLI supports the UA-1
  profile (veraPDF 1.26+ ships it — MOSTLY_TRUE from the bundled CLI's `--flavour` help,
  verified at implementation; if the bundled build lacks UA-1, the test asserts on the
  PDF/A-2B run instead and the UA pin is recorded as CLI-dependent — the plan does not
  gate P2 on a CLI upgrade).
- Expected: the well-formed tagged fixture's UA violations are confined to the families P2
  does not produce (table/list structure absent → skipped vs failed as the CLI reports
  them); an intentionally corrupted tree (dropped `/ParentTree` entry — fault seam) must
  FAIL validation, proving the validator actually reads what we wrote (the teeth).
- Self-verifying: the same fixtures run through both the self-owned walk and veraPDF; the
  two verifiers must agree on well-formed vs broken.

### 6.3 UI honesty (`TestAccessibilityPanel` extensions)

"Tag document…" disabled without a document / with an already-tagged document (whyNot
shown); pre-flight dialog lists images + size-cluster summary; post-run re-scan shows
`tagged=true` and the struct-tree finding resolved; the not-a-conformance disclaimer text is
PINNED verbatim (the panel's existing honesty-box test idiom: "conform" absent outside the
disclaimer); run is off-thread with ARC06 identity tie (the scan idiom, reused).

### 6.4 Effort shape

Extraction+clustering ~1–1.5 wk (the `PdfContentStreamReader` walk + tests);
rewrite+invariant ~1.5–2 wk (the risk mass); tree assembly + Alt flow ~1 wk; panel/report
+ honesty tests ~0.5–1 wk. The invariant tests land BEFORE the rewrite lands (P2a writes
the verifier first — it is the spec of the rewriter).

---

## 7. Design calls of record (pick-and-justify, none blocking)

1. **P2 = paragraphs + headings only** (tables/lists/ActualText/multi-column/outline-derived
   headings → P3). Alternative considered: shipping lists in P2 (they are visually
   distinctive) — rejected: bullet/numbering recognition false-positives convert paragraphs
   into wrong list semantics, the same wrong-text hazard class as §3.2.
2. **Undescribed images are excluded, not placeholder-tagged** (§4). Alternative: tag as
   `Figure` with empty `/Alt` — rejected: an empty `/Alt` is itself a PDF/UA defect and a
   false statement of review.
3. **Already-tagged documents are refused, not merged** (§3.4). Alternative: merge heuristics
   under the existing tree — rejected: silently interleaving guessed structure with author
   structure produces a tree neither human nor machine can trust; the destructive "re-tag"
   path is P3 with explicit confirmation.
4. **Raw-dict construction, no vendor upgrade** (§2.1). The upgrade path exists upstream but
   changes a pinned vendored dependency — a user decision, not this plan's.

---

## 8. TL;DR (10 lines)

1. The P1 checker's biggest reported gap (untagged document) needs a tagging engine; PoDoFo
   1.1.0 has no structure-tree construction API (143 headers verified) — raw dicts it is,
   on the exportPdfA/sanitize idiom the repo already trusts.
2. The load-bearing risk is content-stream REWRITING (BDC/EMC + MCIDs are mandatory for a
   useful tree); it lands behind a text-preservation invariant and a fault-seam negative
   control, inside the SafeSave transaction discipline.
3. P2 scope: `P` + `H1–H6` only; tables/lists/ActualText/multi-column/outline-merge are P3
   with reasons on the record.
4. Headings = document-relative font-size clustering + weight hints, with the cluster table
   SHOWN to the user and known failure modes named — a heuristic disclosed, never a claim.
5. Text decoding is ToUnicode-honest: undecodable pages are skipped and disclosed, never
   tagged with wrong text.
6. `/Alt` is never auto-generated: prompt per image through the landed P1 fix seam; blank =
   excluded from the tree + disclosed.
7. Already-tagged documents are refused (merge is P3 with confirmation); single-column is
   the P2 reading-order assumption, column-suspect pages flagged.
8. Verification: self-owned structural walk + text invariant + veraPDF subprocess checks
   (UA-1 where the bundled CLI supports it, else PDF/A + recorded CLI-dependence); the
   corrupted-tree negative control proves the validator reads our tree.
9. The not-a-conformance-claim sentence is extended verbatim into the tagging UI; "PDF/UA"
   appears only inside the disclaimer.
10. No decision requests; no new dependencies; the existing sanitizer composes (strips /Alt
    /ActualText /E when sanitization runs).

---

## 9. Source register

**Codebase (pinned `ec9f16f6`):** `src/engines/AccessibilityChecker.h:8–23` (scope
discipline + the not-a-claim sentence), `AccessibilityChecker.cpp:22–24` (the PoDoFo
struct-tree getters-only note + raw-dict idiom), `:26–60` (resolve/stringAt walk helpers —
the probe idiom reused by §3.1), `kA11yMax*Findings` (caps pattern); `AccessibilityFixes.h:6–17`
(fix scope + SafeSave shape) / `.cpp` (destroy-before-rename lesson); `AccessibilityPanel.h:17–33`
(honesty contract), ARC06 + injected runner; `third_party/podofo/install/include/podofo/main/`
(no structure headers; `PdfContentStreamReader.h` args/flags/inline-image handling,
`PdfTextState.h`); `scripts/bootstrap-vendor-deps.sh:53`; `SafeSave.h:22–75`
(candidate/commit/fault seams + coordinator); `PoDoFoBackend.cpp:3319+` (D5 structure-tree
sanitize pass — composition note §3.5) and `:291+` (`exportPdfA` raw-dict writing idiom);
`BatchMode.cpp:1089–1176` (`runPresetChain` — reopen+invariant validation shape cited by
§2.2); `VeraPdfValidator.h:14–44` (report shape + fixture-friend test seam);
`CURRENT-EVIDENCE-LEDGER-2026-09-05.md:991–1005` (T2-4 P1 rows: checker `41300f0`, fixes
`c6a56b2`); `tests/TestAccessibility{Checker,Fixes,Panel}.cpp` (fixture + honesty-test
idioms); `docs/research/synthesis.md` §2 T2-4; ISO 32000-1:2008 §14.7 (marked content),
§14.8 (structure types; Adobe-hosted full text as cited in the form-js plan).

**Format note:** follows the house design-doc format of
`docs/research/batch-presets-implementation-plan.md` and
`docs/research/send-for-signing-implementation-plan.md` (pinned-revision source register,
graded claims, honest scope cuts, explicit design-calls section).
