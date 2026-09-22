# Redaction-Stack Deep Audit + Future-Edit Roadmap (2026-09-21)

Lane: redaction-research (finisher). Base: `feat/redaction-research` @ **ec9f16f6**
(merge of feat/rotate270-fix into feat/parity-glm). AUDIT + PLAN ONLY — zero
production edits in this lane; every fix below is a *plan*.

**Base note**: mainline `feat/parity-glm` advanced to **26c9a415** during this
lane. `git diff --stat ec9f16f6 26c9a415` touches only SigningLabels /
SecurityController / Sidebar / PdfViewerWidget / NetworkTouchpoints + ledger
docs — **no redaction-stack files** (RedactionProof, RedactOperation,
PageSpaceTransform, podofo, FormManager untouched). Lane stayed on base
ec9f16f6; all line numbers below refer to that tree.

Method: source read of `src/core/RedactionProof.cpp` (verify, attribution,
8-surface sweep, countTextOperators), `src/engines/RedactOperation.cpp`
(transaction, preflight, proof wiring), `src/core/PageSpaceTransform.h` (the
W2B-1 law), `src/engines/PatternRedactor.h`, `src/engines/podofo/
PoDoFoBackend.cpp` (applyRedactions / exciseContentRegions /
redactCanvasRecursively / sanitizeDocumentContents / saveDocument), vendored
PoDoFo 1.1.0 headers (`third_party/podofo/install`) + upstream 1.1.0 sources
for PdfAnnotationCollection / PdfAnnotation / PdfMath / PdfMemDocument /
PdfDocument / PdfPage, competitor sources (Adobe docs, Edact-Ray literature,
Free Law Project X-Ray), **plus an executed probe**: `R14ProbeRedactSpace.exe`
built and run from a fresh `build-ra` at this tip (6 passed / 1 failed — §1.2;
run recipe in the lane handoff `.context/redaudit-wip.md`).

Status: **COMPLETE** — audit + roadmap below; zero production edits (probe
run was read-only usage; tests/ untouched; this doc is the lane's only write).

## 1. R14 residual: annotation-only attribution on offset+/Rotate pages

### 1.1 What was flagged, twice

- **Flag 1** — R14 independent review, FINDING F1 (2026-09-14,
  INDEPENDENT-REVIEW-2026-09-14.md): `collectAnnotStrings` used PoDoFo
  `PdfAnnotation::GetRect()`, which folds the page's /Rotate into the rect at
  read time; on /Rotate 90 + offset pages the annot hit-test missed, nothing
  was swept for, and the proof certified PASS over a surviving annot secret.
  Fixed (F1): `RedactionProof.cpp:754` and `PoDoFoBackend.cpp:2600` switched to
  `GetRectRaw()` (raw /Rect dictionary read).
- **Flag 2** — this lane's re-run of `tests/R14ProbeRedactSpace` on base
  ec9f16f6 (fresh binary, 2026-09-22): `proofMustFailHonestlyOnSurvivingSecrets`
  FAILS with `annotAttributed == false`. Flagged as a residual "annotation
  hit-test vs userMark Y-convention mismatch under the W2B-1 law".

### 1.2 Probe ground truth at base ec9f16f6 (run 2026-09-22, offscreen, 6/7)

| Slot | Result |
|---|---|
| `proofMustFailHonestlyOnSurvivingSecrets` (composed: content mark + annot mark, /Rotate 90 + offset [0 200 612 1042]) | **FAIL** — `entries.first().removedStrings` lacks `AnnotSecretZebra` |
| `realRedactionExcisesSecretOnOffsetRotatedPage` | PASS |
| `rotatedPageAnnotOnlySecretMustNotFalsePass` (annot-only, same geometry) | PASS — `removed=["AnnotSecretZebra"]`, honest FAIL |
| `offsetOnlyPageAnnotAttributionControl` | PASS |
| `rotatedAnnotMarkFalsePassDemonstration` (combined fixture, annot mark ONLY) | PASS — `removed=["AnnotSecretZebra"]` |

### 1.3 Diagnosis: the production hit-test is CORRECT; the composed slot's
assert is scoped to the wrong entry

The composed failure is **not** a production geometry defect. Three mutually
independent pieces of evidence:

1. **Same fixture + same mark attributes correctly when alone.**
   `rotatedAnnotMarkFalsePassDemonstration` runs the IDENTICAL
   `makeCombinedPdf` fixture with the IDENTICAL `annotViewerMark()
   = QRectF(450, 100, 30, 200)` and attributes `AnnotSecretZebra`. There is no
   cross-mark state in `verify()` — each `EntryWork` is built independently per
   mark (RedactionProof.cpp:936-965).
2. **The sweep targets prove attribution happened in the composed run.** The
   composed proof's failure list contains `SURVIVOR [raw-bytes]
   AnnotSecretZebra` and `SURVIVOR [object-strings] AnnotSecretZebra`. The
   sweep targets are built ONLY from the entries' `removedStrings`
   (`buildTargets(derived, ...)`, RedactionProof.cpp:987-990). A string that
   was never attributed cannot be swept for. Therefore `AnnotSecretZebra` IS in
   some entry's `removedStrings` — the second one.
3. **The probe reads only `proof.entries.first()`** (R14ProbeRedactSpace.cpp:197,
   line `for (const auto& s : proof.entries.first().removedStrings)`).
   `entries[0]` is the content mark's entry (correctly holds the two content
   runs); the annot hit lands in `entries[1]`, which the probe never prints or
   scans. The assert at line 204 (`annotAttributed`) therefore fails while the
   production behavior is exactly the specified one: attribute per covering
   mark, sweep, and FAIL honestly over the survivor.

### 1.4 The Y-convention arithmetic, concretely (the trap that made flag 2
look real)

Both rects in the hit-test (RedactionProof.cpp:955-959) are stored **y-up**
(y() = LOWER edge, height positive):

- `as.rect` — built in `collectAnnotStrings` from
  `GetRectRaw().GetNormalized()` (raw /Rect, ISO 32000-1 §12.5.2 default user
  space): `QRectF(min(L,R), min(B,T), |W|, |H|)` — y-up.
- `userMark` — `PageSpace::viewerToUser(mark, pageGeometry(page))`
  (PageSpaceTransform.h:108-136): contractually y-up ("y() is the LOWER
  edge"), built from min/max corners.

The test is a plain interval overlap:

```cpp
as.rect.right()  >= userMark.left()  &&  as.rect.left() <= userMark.right()   // X overlap
as.rect.bottom() >= userMark.top()   &&  as.rect.top()  <= userMark.bottom()  // Y overlap
```

Qt's `QRectF::top()` returns `y()` (numerically SMALLER) and `bottom()`
returns `y()+height()` (numerically LARGER). On y-up storage the NAMES are
inverted relative to PDF semantics — `as.rect.top()` is the annot's PDF BOTTOM
edge — but the four comparisons are still exactly the two interval-overlap
predicates, so the arithmetic is correct. The naming inversion is precisely
what makes every re-reading of this code suspect a Y-flip bug; that suspicion,
not the code, is flag 2.

Numeric trace (the composed fixture): MediaBox [0 200 612 1042] (x0=0, y0=200,
W=612, H=842), /Rotate 90, raw /Rect [100 650 300 680].
`viewerToUser((450,100,30,200), rot 90)`: ux=[100,300], uy=[650,680] →
userMark = (100,650)-(300,680) — exactly the raw /Rect. Hit test: 300≥100,
100≤300, 680≥650, 650≤680 → HIT. Attribution verified working by slots 3 and 5.

### 1.5 Pinned PoDoFo 1.1.0 rect math (for future readers)

- `GetRectRaw()` (PdfAnnotation.cpp:60): `Corners::FromArray(dictionary /Rect)`;
  raises `ObjectNotFound` when /Rect is absent — caught by verify()'s
  annotation try/catch and surfaced as an honest UNSWEPT problem.
- `GetRect()` (PdfAnnotation.cpp:76) = `TransformCornersPage(GetRectRaw(),
  page)` (PdfMath.cpp:47): with /Rotate radians teta, builds
  `GetFrameRotationTransform(MediaBoxRaw, teta)` — rotate the box, re-align so
  the ROTATED+NORMALIZED MediaBox lower-left returns to `rect_1.X/Y`, then
  apply to both /Rect corners. Closed form for /Rotate 90 on MediaBox
  [x0 y0 x0+W y0+H]: `T(x,y) = (y − y0, x0+W+y0 − x)` (W/H swap + origin
  shift). Verified against source AND empirically: [100 650 300 680] →
  (450, 512, 30×200) — the probe's own dump value.
- Consequence: `GetRect()` output lives in the ROTATED frame and must never be
  intersected with `viewerToUser` output (raw user frame). All redaction-stack
  consumers now use `GetRectRaw()`; the known remaining `GetRect()` consumers
  are signing-side (`SignatureManager.cpp:1175, 2118`) — the W2B-1 "RESIDUAL
  (owner lane)" note, out of this lane's scope but re-flagged here.

### 1.6 FIX PLAN (function / change / pin) — no production change required

**Plan 1-A (the actual fix; probe-only, owner lane — tests are off-limits to
this lane).**
- Function: `tests/R14ProbeRedactSpace.cpp ::
  proofMustFailHonestlyOnSurvivingSecrets`.
- Change: stop asserting on `entries.first()`. Aggregate:
  `QStringList allRemoved; for (const auto& e : proof.entries) allRemoved +=
  e.removedStrings;` and assert `CombinedSecretAlpha ∈ allRemoved` AND
  `AnnotSecretZebra ∈ allRemoved`. STRONGER (recommended): assert per-entry
  scoping — the entry whose `region == secretViewerMark()` carries the content
  strings and NOT the annot string; the entry whose `region ==
  annotViewerMark()` carries exactly `["AnnotSecretZebra"]`. This pins
  mark-scoped attribution (a regression that attributed everything to every
  mark would otherwise pass the aggregate form).
- Pin: `R14ProbeRedactSpace` binary 7/7; `TestSep13LeadRedactionProof` and
  `TestRedactionProof` stay green; ledger row noting flag 2 was a probe-scoping
  defect (production attribution verified correct at base ec9f16f6).

**Plan 1-B (optional production hardening, LOW severity; separate lane).**
- Function: `RedactionProof.cpp` mark loop (lines 955-959).
- Change: keep the `>=`/`<=` interval form (edge-touching counts as a hit;
  over-attribution is the safe direction and `QRectF::intersects()` would
  silently tighten edge semantics) but add the one-line comment: "both rects
  stored y-up; Qt top()/bottom() name the OPPOSITE PDF edges; do not 'fix' the
  apparent inversion." This kills the trap that produced flag 2.
- Pin: text-only change; whole R14ProbeRedactSpace + redaction suites green.

**Plan 1-C (proof-granularity nit found while here, LOW).**
- Symptom: `rotatedPageAnnotOnlySecretMustNotFalsePass` reports `UNSWEPT
  [PageStreams] source/output page 1 content stream could not be decoded` for a
  page whose content stream is trivially empty/absent —
  `pageMechanics` (RedactionProof.cpp:152) returns `ok=false` when
  `page.GetContents()` is null, conflating "no content" with "undecodable".
- User story: a scanned or annotation-only page always produces UNSWEPT noise,
  training users to ignore UNSWEPT rows — diluting the signal of real
  undecodable-stream cases (which are the dangerous ones).
- Change: in `pageMechanics`, `GetContents() == nullptr` → `ok=true`, empty
  digest, `textOps=0`, plus a new `hasContents=false` flag; callers append the
  "could not be decoded" problem only on real decode exceptions. Verify the
  two `textOps >= 0` consumers (entry adjudication) handle the flag.
- Pin: annot-only fixture proof carries zero UNSWEPT rows; deliberately
  corrupt-stream fixture still reports UNSWEPT; TestRedactionProof green.

### 1.7 Fixture-authoring hazard discovered while diagnosing (log, not a defect)

PoDoFo 1.1.0's `PdfAnnotationCollection::CreateAnnot` and
`PdfAnnotation::SetRect` PRE-TRANSFORM their rect argument through
`TransformRectPage` (inverse frame rotation) before writing /Rect
(vendored PdfAnnotationCollection.cpp:17-19, PdfAnnotation.cpp:81-87). The
R14 fixture's saved raw /Rect stayed `[100 650 300 680]` (its `GetRect()`
decode matches exactly), but any future fixture needing exact raw-/Rect
control on a rotated page should use `SetRectRaw` / write the key directly
instead of trusting the convenience API's space contract.

## 2. Excision completeness classes

The excision core: `exciseContentRegions` (PoDoFoBackend.cpp:2473-2528) →
`redactCanvasRecursively` (page object; recurses Form XObjects with their own
/Resources on `Do`; neutralizes intersecting image XObjects + their /SMask to
1x1 transparent; Edact-Ray numeric-only `[N] TJ` gap substitution; Tr-3
invisible-text scrub; MCID struct-tree cleanup). Then `applyRedactions` paints
the black cover (2576-2582), walks intersecting annotations' /AP → /N for
image neutralization, and REMOVES those annotations (2584-2645). Guard: pages
with inline images or binary content ABORT the whole redaction (2485-2498 —
"no visual-only half edit").

**Save-side fact that decides two verdicts below**: `PdfMemDocument::Save`
runs `CollectGarbage()` BY DEFAULT (upstream PdfMemDocument.cpp:257-260;
`PdfSaveOptions::NoCollectGarbage` is the opt-OUT), and `saveDocument` uses
plain `Save` (PoDoFoBackend.cpp:552).

**Severity skew warning**: the honest-failure net is OPT-IN.
`RedactRequest::produceProof = false` by default
(src/engines/RedactOperation.h:73) and RedactApplyDialog's proof checkbox
starts unchecked unless the plan says so (src/modes/RedactApplyDialog.cpp:147).
Every "silent w/o proof" story below is the DEFAULT path, not a corner.

### 2.1 Annotation-layer content

- **Intersecting annotations (any subtype — FreeText, Text, Stamp, Ink,
  Widget, …): SAFE-BY-DESIGN.** The annot is removed from /Annots
  (`RemoveAnnotAt`) and the object is deliberately orphaned
  (vendored PdfAnnotationCollection.cpp:43-61, "it will be garbage collected")
  → the default-GC Save drops the object AND its /AP streams from the saved
  output. If anything ever changes to keep those bytes, the ObjectStrings
  sweep fails the proof loudly.
- **Appearance streams of removed annots: SAFE-BY-DESIGN.** /AP → /N Form
  XObject is walked for IMAGE neutralization before removal (NF-2,
  PoDoFoBackend.cpp:2610-2639); /R and /D variants are not walked, but moot —
  the whole object subtree dies at GC. (Note stays in code comments.)
- **Non-intersecting annotations: untouched BY DESIGN** (redaction removes
  what the mark covers) — safe as a contract, with two real cracks: G3, G5.
- **G3 (M, silent w/o proof) — widget field VALUES survive their widget.**
  A widget annotation is only the geometry; the value lives on the FIELD dict
  (`/V`, inherited via /Parent; `/DV` too), reachable from /AcroForm /Fields —
  a different object the excision never touches. Removing the widget under the
  mark deletes the visible glyphs; the string survives in the file. User
  story: "I drew the box over the form field, saved, ran my own string search
  — the value is still in there." Proof-mode catches it (ObjectStrings) — opt-in.
  - *Fix*: in the applyRedactions annot loop (PoDoFoBackend.cpp:2609), when
    subtype == Widget, walk the /Parent chain (same shape as
    RedactionProof.cpp:773-780) and REMOVE `/V` + `/DV` keys on the field
    root (mirrors sanitize step 22; keep the field object itself), recording
    the act in the manifest/audit note.
  - *Pin*: fixture with text field `/V (SecretValue)` whose widget rect is
    marked → saved output raw-byte + object-string sweep = 0 hits; the (now
    empty) field still exists; non-marked fields untouched. *Risk*: M
    (touches AcroForm). *Effort*: S-M.
- **G5 (M, silent w/o proof) — /Rect-underdraw.** Intersection uses the raw
  /Rect only. Ink/Stamp/FreeText appearances can draw OUTSIDE /Rect (missing
  or degenerate /Rect is legal; FreeText margins), so a mark over what the
  user SEES may not intersect /Rect: nothing excised, nothing attributed —
  and since attribution targets come from /Rect-intersecting annots, the
  proof has no target either. Fully silent class.
  - *Fix*: widen BOTH intersection tests (excision PoDoFoBackend.cpp:2602,
    attribution RedactionProof.cpp:955) to the union of raw /Rect and the
    /AP /N appearance bbox mapped through /Matrix when present.
    Over-approximation is the safe direction (same stated principle as
    `runIntersects`' generous vertical headroom, RedactionProof.cpp:783-791).
  - *Pin*: ink annot whose /AP spills left of /Rect → mark over the spill →
    attributed (proof FAILS over the survivor) and the annot removed;
    regression: R14 probe stays green. *Risk*: L-M (over-removal is
    visible but safe-direction). *Effort*: M.

### 2.2 OCG-layer content

- **Excision: SAFE-BY-DESIGN.** Layer state is a view-time filter; every
  layer's text operators live in the decoded content stream, so a mark
  excises marked-region glyphs regardless of visibility. (Corollary: the user
  cannot mark what they cannot see — hidden-layer content is untouched; see G4.)
- **G4 (M) — sanitize's OCG "flatten" REVEALS hidden layers.** sanitize
  step 17 removes `/OCProperties` entirely (PoDoFoBackend.cpp:3353-3356).
  With the default config gone, every layer-gated element renders VISIBLE in
  every viewer — the sanitized copy of a redacted document can reveal content
  the user never saw, never marked, and believed hidden. The opposite of
  redaction-safe flattening. User story: "I redacted page 3 and ticked
  'also create a sanitized copy'; the copy now shows a hidden layer with the
  reviewer names."
  - *Fix*: replace removal with an explicit OFF policy — keep /OCProperties,
    set every OCG `ON false` in /D (and list /OFF) so hidden stays hidden;
    pair with a preflight disclosure in RedactOperation ("page N contains
    hidden optional content; redaction removes marked regions only — hidden
    content remains present but not visible") and a pack note.
  - *Exact sites*: PoDoFoBackend.cpp:3353-3356 + RedactOperation preflight
    (~469-505). *Pin*: OCG fixture with a /OFF layer → sanitized output still
    hides the layer (/D dict assert or render probe); pack carries the note;
    move any TestSanitization pin from key-absence to hidden-state (the
    ed04426 precedent: data-absence pins, not weakened). *Risk*: M (sanitize
    contract change). *Effort*: M.

### 2.3 Type3 glyph content

*SAFE-BY-DESIGN.* Type3 text is ordinary `Tf/Tj` in the page stream — the
walker excises it by geometry like any other font; CharProcs streams hold only
glyph shapes (font data, not redacted content) and are swept raw by
DecodedStreams anyway. Acceptance note: when a Type3 base font can't be
resolved, advance computation falls back to Helvetica
(PoDoFoBackend.cpp:2254-2256), so the substituted gap width is approximate —
the Edact-Ray property (total advance preserved, per-glyph widths lost) holds
in the aggregate.

### 2.4 Text via patterns / shading

- **G2 (M-H, silent w/o proof) — tiling-pattern streams are not walked.**
  `redactCanvasRecursively` recurses ONLY Form XObjects on `Do`
  (PoDoFoBackend.cpp:2407-2426) and images on `Do` (2380-2406). Content inside
  a tiling pattern (`/PatternType 1` stream under canvas /Resources /Pattern,
  painted by `scn` + fill — no `Do`) is unreachable: text drawn ONLY inside
  the pattern survives stream surgery while the black cover paints over it.
  PDFium's extraction still sees pattern text, so ExtractedText fails the
  proof when it runs — with proof OFF this is the classic
  black-box-over-live-data.
  - *Fix (two stages)*: (a) HONEST ABORT now — extend the guard block
    (PoDoFoBackend.cpp:2485-2498): if canvas /Resources /Pattern holds a
    `/PatternType 1` stream containing a glyph-carrying text operator (reuse
    `countTextOperators`, RedactionProof.cpp:88-140 — the lexer exists),
    abort the page with a user-visible reason, exactly like the
    inline-image/binary guard. (b) Later: recurse into pattern streams with
    the pattern matrix composed at use time.
  - *Pin*: fixture whose only secret is drawn by a tiling pattern →
    `applyRedactions` false, nothing committed, named error; positive
    control: a text-free pattern → redaction proceeds. *Risk*: M (false
    refusals — scope strictly to tiling patterns whose streams contain text
    ops). *Effort*: M (a) / L (b).
- Shading (`sh`, /Shading dicts): no realistic text carrier identified —
  *INCONCLUSIVE*; covered by the G2 guard if ever encountered.

### 2.5 XFA-present documents

- **G1 (H) — legacy XFA form data is invisible to the entire stack.** Grep of
  the redaction stack (PoDoFoBackend.cpp, RedactOperation.cpp,
  RedactionProof.cpp, RedactMode.cpp, RedactApplyDialog.cpp) for `XFA`:
  ZERO hits. Nothing detects, refuses, excises, or discloses it.
  `/AcroForm /XFA` (or catalog /XFA) streams carry the full form data model —
  every field value, potentially re-encoding exactly the strings the user
  redacted. Sanitize clears `/V` + `/DV` (step 22, PoDoFoBackend.cpp:3446-3451)
  but leaves /XFA; excision never reaches it. Aggravator: modern viewers
  (including PDFium) often don't render XFA, so the user may redact the
  AcroForm-rendered field and never know an XFA copy exists. Proof's
  DecodedStreams + ObjectStrings sweeps DO see the XFA stream and fail loudly
  when a marked secret reappears — opt-in again. User story (no proof):
  "redaction said Completed; my XFA-aware editor opens the form and the
  'deleted' values are all there."
  - *Fix*: (a) PREFLIGHT REFUSAL (honest, S): in
    `RedactOperation::ExecutionState::execute` (RedactOperation.cpp:469-505),
    detect `/AcroForm /XFA` or catalog /XFA (add `hasXfaDocument()` beside
    `hasPdfSignatures()`) and refuse with the kSignedRefusal pattern's
    wording: legacy XFA form data cannot be excised; flatten/drop XFA first.
    (b) SANITIZE REMOVAL (M): add /XFA to sanitizeDocumentContents (AcroForm
    dict key; catalog /XFA for dynamic XFA) with pack disclosure. (c) PACK
    NOTE (S): extend kDisclaimer (RedactionProof.cpp:1166-1182) with the XFA
    limitation like the raster one.
  - *Pin*: XFA fixture → refused (a) / sanitized output XFA-free (b) / pack
    names the limitation (c); negative control: non-XFA docs unchanged.
    *Risk*: refusal path low; sanitize removal medium (workflows lose the
    form). *Effort*: S (a+c) / M (b).

## 3. PoDoFo COW / AssertMutable in-place-mutation scan (sanitize / redact / save)

Same class as ed04426 (E-2) and E-1: a wrapper or COW buffer is shared, a path
mutates or frees it in place, a later use reads through the stale side.

**The mechanism that makes this class lethal here**: `PdfMemDocument::Save`
garbage-collects orphaned objects BY DEFAULT (upstream PdfMemDocument.cpp:257-260),
so ANY key removal that orphans an object which a document-lifetime wrapper
still wraps becomes a dangling wrapper at the NEXT save/metadata touch.
`PdfDocument` caches, for the whole document lifetime: `m_Info` (PdfInfo),
`m_Outlines` (nullable), `m_Catalog` (unique_ptr — always), `m_AcroForm`
(once loaded), `m_NameTrees` (vendored PdfDocument.h:474-478 +
MustGetAcroForm/MustGetNames, PdfDocument.cpp:691-713).

### 3.1 Inventory of every in-place mutation on the paths (all audited)

| Site (PoDoFoBackend.cpp) | Mutation | Wrapped-root risk | Verdict |
|---|---|---|---|
| /Info (3273-3279) | dict `Clear()` in place, alias-guarded vs /Root | none — object kept | SAFE (ed04426 fix) |
| /Outlines (3366-3369) | dict `Clear()` in place | none — object kept | SAFE (ed04426 fix) |
| catalog /Metadata /PieceInfo /MarkInfo /OutputIntents /OpenAction /AA /OCProperties /Collection | `RemoveKey` — orphans VALUE objects | no wrapper wraps these values; m_Catalog wraps the catalog dict (key removal ≠ object freed) | SAFE today |
| /Names → EmbeddedFiles /JavaScript (3295-3309) | key removal on the /Names dict | m_NameTrees wraps the /Names ROOT — not removed; children only | SAFE today |
| page dict /AA /A /PieceInfo /Thumb /Metadata (3379-3393) | key removal on page dict | page object alive (page tree) | SAFE today |
| annot dict /Contents /RC /AA /A + RemoveAnnotAt (3396-3443) | key removals + index-deferred reverse-order removal | annot wrappers not reused after removal; orphans GC'd at Save | SAFE today |
| AcroForm fields /V /DV (3446-3451) | `GetFieldsIterator()` + field-dict key removal | m_AcroForm cached by iteration — but /AcroForm root never removed on these paths; absent-AcroForm case null-guards (upstream PdfDocument.h stepIntoPageOrForm) — verified, no throw on form-less docs | SAFE today |
| trailer /ID[1] (3461-3472) | array-element assign, FromRaw | trailer alive | SAFE |
| struct elements /ActualText /Alt /E (3326-3350) + cleanStructElement (1461+) | key removals, depth-capped | plain objects, no wrappers | SAFE today |
| excision stream rebuild + /AP walk | reads + targeted neutralization; `getEncodedStringWidth` deep raw copy (E-1) | — | SAFE (2c5a0d6 fix holds) |

**Live UAFs found: none.** Both historic fixes re-verified in code at this
tip (not just changelog-trusted).

### 3.2 L1 — the latent discipline gap (hardening; no live defect)

The invariant that saved the soak ("never orphan an object a PdfDocument cache
wraps — /Info and /Outlines are scrubbed IN PLACE") exists only as comments at
those two sites. `m_Catalog`, `m_AcroForm`, `m_NameTrees` are equally immortal
wrappers, and the file carries ~20 `RemoveKey` calls one refactor away from
orphaning one of those roots (e.g. a future "strip forms" sanitize option
doing `catalog.RemoveKey("AcroForm")` AFTER GetFieldsIterator cached
m_AcroForm → CollectGarbage frees it under the wrapper → the exact 0xc0000005
the 48h soak caught, now rarer and crueler).

- *Fix*: (a) centralize catalog-key removal in one helper
  (`removeCatalogKey(doc, name)`) that documents-and-asserts the forbidden set
  {/Root, /Info, /Outlines, /AcroForm, /Names} (debug hard assert; release:
  scrub-in-place fallback), and route the eight catalog RemoveKey sites
  through it; (b) extend tests/TestSanitizeTrailerUaf.cpp (owner lane) with a
  double-save pin per wrapper: load → scrub → Save → touch metadata / Save
  again — the soak-signature shape that failed before ed04426.
- *Pin*: new pins fail-before on a deliberately-introduced orphaning call,
  pass-after with the helper. *Risk*: L. *Effort*: S.

### 3.3 E-1 class residuals

`PdfString::GetString()` in-place lazy-evaluation on MUTATING paths: the
excision rebuild reads only freshly-parsed operand strings (parser-owned, not
COW-shared with saved objects); the read-side sweeps (collectStringBytes,
collectAnnotStrings) run on never-saved documents. PatternRedactor hands
geometry, not strings, to applyRedactions. No residual exposure found.

## 4. Competitor pattern research → adoptable patterns

Sources: Adobe's redact/sanitize workflow docs (helpx.adobe.com), university
guides describing Acrobat's Remove Hidden Information checklist behavior
(e.g. Columbia's Acrobat Pro guide), the "marks are annotations until applied"
disclosure literature (Preservica/TSL guides), Edact-Ray (PETS-era tool,
Wired-covered; detect/break/fix redaction leaks) and its successor literature
(van Heusden 2025, neural redacted-text segmentation — cites Edact-Ray as
near-unique prior art), and Free Law Project's X-Ray (freelawproject/x-ray —
bad-redaction detector with per-strategy findings over PACER documents).
Five concrete patterns worth adopting, each mapped to a GlyphPDF surface:

- **P1 — Enumerate-and-confirm sanitize scope (Acrobat's checklist).** Acrobat
  shows a found-and-will-remove list (annotations, metadata, hidden text,
  embedded data) with per-item deselect. GlyphPDF's sanitize is
  all-or-nothing. Map: RedactApplyDialog — cheap dry scan (every
  sanitizeDocumentContents step is already enumerable) rendered as
  "sanitize will strip: XMP, embedded files, JS, N annotation contents, field
  values, …". Pairs with G4: the checklist is where "OCG layers will be set
  hidden" gets consented to. Effort M.
- **P2 — Per-strategy coverage matrix in the report (X-Ray's granularity).**
  X-Ray reports each finding WITH the strategy that caught it; the strategy
  list doubles as a coverage statement. GlyphPDF's pack names surface +
  location per survivor — add the negative space: a ran / not-run / absent
  matrix per Surface AND per audit class (image text: NOT RUN — rasterize;
  pattern streams: GUARDED/ABORTED; XFA: PRESENT — not swept until G1 lands).
  Map: RedactionProof::Result::surfaces + a coverage block in
  toJson/toTextReport. Effort S-M.
- **P3 — Burned-in vs removable honesty + redact-annot residue sweep.** The
  literature's #1 failure class: distributing documents whose redaction MARKS
  are still removable annotations (unapplied Acrobat /Redact annots — delete
  the box, read the text). GlyphPDF burns boxes into content (the safe side)
  but never says so, and an INPUT file can carry unapplied /Redact annots
  from an Acrobat round-trip: they survive excision untouched and ship in the
  output. Map: (a) honesty line in the pack + RedactApplyDialog ("covering
  boxes are painted content, not annotations; they cannot be removed to
  reveal content"); (b) sweep for remaining /Subtype /Redact annots in the
  output → FAIL loudly; (c) auto-remove them in sanitize (which already
  removes RichMedia/Screen/Movie, PoDoFoBackend.cpp:3432-3439). Effort S.
- **P4 — Measure-the-attack-then-pin-the-property (Edact-Ray discipline).**
  The excision core already adopts the advance-gap defense and pins it
  (numeric-only `[N] TJ`; countTextOperators; TestExcisionCorruption).
  Institutionalize: every GAP fix lands with an attack-shaped pin (survivor
  fixture + honest-refusal fixture), not just a behavior pin — the Pin column
  of §5 is already written that way.
- **P5 — Rasterize-the-region escape hatch (competitors' answer to image
  text).** The pack honestly disclaims image-borne text; Acrobat-class tools
  rasterize the region instead. `Method::Rasterize` is already reserved in the
  proof schema (RedactionProof.h:62) so the pack survives the addition. Map:
  RedactOperation/engine path rasterizing the mark region (PDFium render →
  image → replace), manifest records Method::Rasterize, proof sweeps those as
  media (raw-only) honestly. Effort L — the only large item.

## 5. Roadmap + verdict counts

### 5.1 Verdict counts (16 classes across §1–§3)

| Verdict | Count | Items |
|---|---|---|
| VERIFIED-CORRECT (production, probe-proven) | 1 | R14 annotation attribution (was flagged as the residual) |
| PROBE-BUG (fix is test-side) | 1 | R14 composed-slot assert scoped to `entries.first()` (§1.3) |
| SAFE-BY-DESIGN | 6 | intersecting-annot removal+GC; /AP /N walk; non-intersecting annots; OCG excision; Type3; E-1/E-2 fixes hold |
| GAP | 6 | G1 XFA (H); G2 pattern text (M-H, silent w/o proof); G3 widget /V (M, silent w/o proof); G4 OCG sanitize reveal (M); G5 /Rect-underdraw (M, silent w/o proof); G6 empty-content UNSWEPT noise (L, §1.5 Plan 1-C) |
| LATENT (hardening) | 1 | L1 wrapper-vs-GC discipline (+ Plan 1-B comment pin) |
| INCONCLUSIVE | 1 | shading-pattern text (covered by G2's guard plan) |

Prior fixes re-verified at this tip: E-2 (ed04426, /Info + /Outlines in-place
scrub) and E-1 (2c5a0d6, getEncodedStringWidth deep raw copy) — intact.
Re-flagged out-of-scope residual: SignatureManager's remaining `GetRect()`
consumers (SignatureManager.cpp:1175, 2118 — §1.5, owner lane).

### 5.2 Prioritized roadmap

| # | Item | Files | Change shape | Pin | Risk | Effort |
|---|---|---|---|---|---|---|
| R1 | **G1 XFA** refusal + sanitize removal + pack note | RedactOperation.cpp (preflight ~469-505); PoDoFoBackend.cpp (sanitize step 22.5); RedactionProof.cpp (kDisclaimer 1166-1182) | `hasXfaDocument()` beside hasPdfSignatures → refuse (kSignedRefusal pattern); sanitize drops AcroForm /XFA + catalog /XFA; disclaimer line | XFA fixture: refused / sanitized XFA-free / pack names limitation; negative control unchanged | low (refuse+note) / medium (sanitize) | S (a+c), M (b) |
| R2 | **R14 probe fix** (§1.6 Plan 1-A) | tests/R14ProbeRedactSpace.cpp (owner lane) | aggregate removedStrings over ALL entries + per-entry scoping pin + viewerToUser W2B-1 pin | probe 7/7 (before 6/7, exactly one assert); TestSep13LeadRedactionProof + TestRedactionProof stay green | none | S |
| R3 | **G2 pattern-text honest abort** (stage a) | PoDoFoBackend.cpp exciseContentRegions guard (2485-2498) | scan canvas /Pattern for /PatternType 1 streams with glyph-carrying text ops (reuse countTextOperators); abort page with named reason | pattern-text fixture → applyRedactions false, no commit, named error; text-free-pattern control proceeds | medium (false-refusal scope) | M |
| R4 | **G3 widget /V** cleared under marked widgets | PoDoFoBackend.cpp applyRedactions annot loop (2609-2641) | on Widget subtype walk /Parent chain; RemoveKey /V + /DV on field root; manifest note | field fixture: value 0-hit in output raw bytes + object strings; field structure survives; unmarked fields untouched | medium | S-M |
| R5 | **G4 OCG reveal**: sanitize hides instead of revealing | PoDoFoBackend.cpp:3353-3356; RedactOperation.cpp preflight; pack note | keep /OCProperties; write /D with all OCGs OFF (+ /OFF list); preflight warning; pack note | OCG fixture: sanitized output keeps layer hidden; TestSanitization pin moved key-absence → hidden-state (ed04426 precedent) | medium (contract change) | M |
| R6 | **G5 /Rect-underdraw**: appearance-bbox-widened intersection | RedactionProof.cpp (955-961 + 738-781); PoDoFoBackend.cpp (2600-2608) | union of raw /Rect and /AP /N bbox (× /Matrix when present) in both intersection tests; over-approximation documented as safe direction | ink-annot spill fixture: mark over spill → attributed + removed; R14 probe stays green | low-medium | M |
| R7 | **L1 COW discipline** (+ §1.6 Plan 1-B comment) | PoDoFoBackend.cpp; tests/TestSanitizeTrailerUaf.cpp (owner lane); RedactionProof.cpp:955 comment | removeCatalogKey helper refusing the wrapper-wrapped roots (debug assert, release scrub-in-place); route the 8 catalog sites; double-save pin per wrapper; "don't fix the apparent y-up inversion" comment | fail-before pin with a deliberately orphaning call; soak-signature green | low | S |
| R8 | **G6 empty-content UNSWEPT noise** (§1.6 Plan 1-C) | RedactionProof.cpp pageMechanics (152-169) | `GetContents() == nullptr` → ok=true, empty digest, textOps=0, hasContents flag; callers only report UNSWEPT on real decode exceptions | annot-only fixture proof carries zero UNSWEPT rows; corrupt-stream fixture still UNSWEPT; TestRedactionProof green | low | S |
| R9 | **P3 redact-annot residue** sweep + sanitize removal + honesty line | RedactionProof.cpp (sweep); PoDoFoBackend.cpp (3432-3439 list); pack + dialog line | add /Redact to removed subtypes; proof FAIL on output /Redact presence; burned-in honesty line | Acrobat-round-trip fixture with unapplied /Redact marks → gone in output | low | S |
| R10 | **P2 coverage matrix** in the pack | RedactionProof.h/.cpp | per-Surface + per-class ran/not-run/absent block in toJson/toTextReport | pack JSON carries matrix; failureReasons unchanged | low | S-M |
| R11 | **P1 sanitize checklist** in RedactApplyDialog | src/modes/RedactApplyDialog.cpp(+h) | dry-scan enumeration of sanitizeDocumentContents steps shown before commit (absorbs R5's consent wording) | dialog lists found items for a metadata-rich fixture | low | M |
| R12 | **P5 Method::Rasterize** | RedactOperation.cpp + engines rasterize path; RedactionProof.h Method already reserved | rasterize mark region on image-text pages; manifest records Method::Rasterize; proof treats as media raw-only | scanned-region fixture: region rasterized, original image bytes absent; pack labels method | medium-high | L |

Dependency notes: R5 + R11 touch the same consent surface (land R5's wording
inside R11's checklist); R1(c), R9, R10 all extend the pack — land together or
in quick succession to avoid pack-schema churn (kProofFormatVersion stays 1
for note-level additions; bump only if the matrix changes reader parsing).
Priority order rationale: R1/R2/R3 first — R1 closes the only HIGH gap with an
honest refusal, R2 retires the twice-flagged residual for one S of test work,
R3 closes the worst silent (default-path) excision hole with the tool's own
established abort pattern.

## 6. Residuals / not covered here

- runIntersects vertical headroom (3.0fs/1.5fs) — intentionally generous;
  not re-flagged.
- Shared-resource over-excision: neutralizing an image XObject referenced both
  inside a redacted region and elsewhere (visible-content loss, not a leak) —
  known NF-2 trade-off, no incident on file.
- SignatureManager `GetRect()` consumers (§1.5) — signing-side W2B-1
  residual, owner lane.
- PDFiumBackend is render/extract-only in this stack; no redaction surgery
  there — nothing to audit for excision semantics.
- No src/ site indexes proof entries positionally today (grepped); R2's
  precision pin covers the consumer pattern anyway.
