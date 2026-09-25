# RESIDUAL PLANS — execution-ready fix plans for the consolidated report's residuals (2026-09-21)

Lane: residuals-plan (`feat/residual-plans`, worktree pdf-sec). Deliverable class:
**audit + design only — zero production or test edits.** Every plan below is
written so a fresh fix lane can execute it without re-doing the investigation.

- **Input:** `docs/audit/CONSOLIDATED-REPORT-2026-09-20.md` §2.6 (15 residuals)
  + §7 open items. The report is not on `feat/parity-glm`; it was read via
  `git show feat/consolidated-report:docs/audit/CONSOLIDATED-REPORT-2026-09-20.md`.
- **Verification base:** `feat/residual-plans` = `feat/parity-glm` @ **26c9a415**
  (the mainline advanced past the coordinator-quoted ec9f16f6 during lane setup:
  SWEEP-W3 modularity-moves merged — 5c02b01d B1 SigningLabels seam, 791115bb
  dead-include sweep, 3c411cc8 AM1/AM2 ledger rows). **All file:line citations in
  this document are at 26c9a415.**
- **Verification verdicts:** 15/15 residuals planned. Of these:
  **11 STAND-AT-TIP**, **1 SPLIT** (core repaired at tip; a successor residual open),
  **2 RESOLVED-ON-LANE-BRANCH-UNMERGED** (fix exists, not on the mainline),
  **1 process residual** whose substance is a review/merge schedule (its inputs
  verified as unmerged-at-tip). Detail per plan.

Plan shape (as commissioned): **Residual → Root cause (verified at this tip,
file:line) → Exact change → Pin design → Negative control design → Risk class →
Suggested lane brief.** Risk legend: S = single function/file, trivially auditable;
M = one subsystem, named sibling callers; L = cross-subsystem contract change.

---

## Plan 1 — W1-05/F1 machine-policy enforcement structural close (§2.6 row 1; §7 item 6)

- **Residual:** A squatter (any standard user) can still plant a machine policy
  that GlyphPDF ENFORCES; shipped W1-05 fix is disclosure-only
  (`trustModelNote()` at every enforcement surface + support-bundle field).
- **Verification at tip:** STANDS. `src/core/PolicyController.cpp:62-116`
  (`PolicyController::load`) promotes ANY readable, schema-valid JSON at the
  resolved path to `State::Loaded` — there is no origin, ownership, ACL, or
  signature check. Path resolution `defaultPolicyPath()` (:50-60) accepts
  `GLYPHPDF_POLICY_PATH` (disclosed test seam, W2B probe) and falls back to
  `%PROGRAMDATA%…/GlyphPDF/policy.json`. Disclosure-only posture confirmed at
  `trustModelNote()` :166-183 and enforcement-note :185-197.
- **Root cause:** The policy surface was designed as "machine config is trusted
  because the machine wrote it" — a trust model that is FALSE on Windows where
  `%PROGRAMDATA%` subdirs are creatable by any standard user. The disclosure fix
  made the trust model honest; nothing yet makes the trust DECISION safe.
- **Exact change (design item, two candidate closes — pick one, document the other
  as rejected):**
  1. **Admin-tier ACL gate (recommended for Windows-first):** in
     `PolicyController::load`, after parse, verify the file's owner is
     Administrators/SYSTEM and no non-admin has WriteDac/WriteOwner access
     (`GetNamedSecurityInfoW` + `AccessCheck`); if not, set a new
     `State::UntrustedOwner` and enforce NOTHING, disclosing
     "policy present but not admin-owned — ignored" wherever `trustModelNote()`
     renders today. Non-Windows: keep current behavior behind `#ifdef` /
     `Q_OS_WIN` with the platform difference disclosed by the same note.
  2. **Signed/hashed policy:** policy.json gains a detached signature
     (or SHA-256 recorded at admin-deploy time in the registry HKLM); load
     verifies before enforcing. More moving parts (key custody) — fallback option.
- **Pin design (failing-first):** `tests/TestPolicyWiring.cpp` — new slot
  `plantedPolicyFromUserWritablePathIsNotEnforced`: write a schema-valid policy
  disabling a setting (use the same fixtures the squatting probe uses), point
  `GLYPHPDF_POLICY_PATH` at a temp path whose ACL mimics a user-created file
  (default ACL of a file created by the test process IS user-owned — the natural
  fixture), reload the controller, assert `state() == UntrustedOwner` and the
  managed setting's effective value is unchanged. Fails at tip (state == Loaded).
- **Negative control design:** scoped revert of the `load()` gate only (keep the
  disclosure code) → the pin's controller reports `Loaded` + enforced → pin fails
  → restore → green. Textual NC: pre-fix `PolicyController.cpp` has no
  `UntrustedOwner` token (grep-zero, same idiom as W2B's nc-w1-05).
- **Risk class:** **M.** Siblings to audit: every `PolicyController::instance()`
  consumer that reads managed keys (grep `PolicyController` in src/ —
  SecurityController TSA refusal E-5 surface, OCRMode language policy, Capability
  gates), TestPolicyController (12/12 pin set), TestPolicyWiring, the support
  bundle writer (`trustModel` field), and the W2B probe
  (W2BProbeSummaryPolicy — its squatter fixture will now legitimately report
  "ignored"; the probe's expectation must be updated in the SAME commit, disclosed
  in the lane report, or the probe will false-fail).
- **Suggested lane brief:**
  1. Read PolicyController.cpp:40-200 + TestPolicyWiring fixtures; write the
     failing pin `plantedPolicyFromUserWritablePathIsNotEnforced`.
  2. Implement the Windows owner/ACL gate as `State::UntrustedOwner`; disclosure
     string through `trustModelNote()`'s existing render path.
  3. Update W2BProbeSummaryPolicy expectations in the same commit (disclosed).
  4. NC: revert `load()` gate → pin fails → restore. Ledger row
     implemented-awaiting-review; cross-ref W1-05 rows + SWEEP-W2B §Residuals 3.

---

## Plan 2 — F2 embed-branch scoping: injectable TSA test seam (§2.6 row 2; §7 item 4 adj.) — DEEP PLAN (a)

- **Residual:** The B-T embed branch inside `signDocumentImpl` (the in-CMS
  timestamp-token embed) is evidenced by the committed probe + the fix's captured
  NC + source audit only. No LIVE hostile-https-TSA harness exists;
  `SignatureManager` enforces https (:259-262 refuses `http://`), and there is no
  test seam to inject a controllable TSA responder — so the honest-attainment
  logic (`timestampTokenValid`, "HTTP-200 ≠ attainment") has never been driven
  end-to-end by a test with a machine-controlled responder.
- **Verification at tip:** STANDS. The whole fetch path is hard-wired inside
  `SignatureManager::Private`:
  - `fetchTimestampToken` — src/engines/SignatureManager.cpp:222-254 (builds
    DER `TS_REQ`, posts to `tsaUrl`);
  - `httpPost` — :256-296 (static `QNetworkAccessManager`, 15 s transfer timeout,
    20 s semaphore wait, http:// refused at :259-262);
  - embed branch — `BtPdfSigner::ComputeSignature` :1393-1448: d2i of the CMS,
    digest over the signature value, `fetchTimestampToken` call :1417,
    **d2i_TS_RESP parse :1428 → embed + `timestampTokenValid = true` :1434**,
    garbage-body → B-B degradation warnings :1442-1447.
  Existing tests only exercise the NO-TSA and REFUSED-LOOPBACK paths
  (tests/TestSignatureRealCrypto.cpp:219-256, :965-990): there is no test that
  ever supplies a VALID TS_RESP through the transport, and none that supplies a
  hostile one (error page, wrong-OID token, status≠granted) — the F2 honesty
  logic at :1425-1441 is test-orphaned on its success/failure matrix.
- **Root cause:** `fetchTimestampToken`/`httpPost` are private members with no
  injection point, and the signer object (`BtPdfSigner`) is constructed inside
  `signDocumentImpl` — so even a public seam on the manager cannot reach the
  embed branch without a deliberate hook. The hostile-Responder question ("can a
  malicious https endpoint convince us to embed a non-token or claim B-T?")
  therefore has no automation; the F2 residual is exactly this unowned seam.
- **Exact change (seam design — no behavior change in production mode):**
  1. In `SignatureManager::Private` add one type alias + one member:
     ```cpp
     using TimestampTransport =
         std::function<QByteArray(const QString& tsaUrl, const QByteArray& tsReqDer)>;
     TimestampTransport timestampTransport;   // null => default transport
     ```
     Defaulted in the Private ctor to a lambda that calls the existing
     `fetchTimestampToken` body ( MOVE the DER-building into it unchanged;
     `httpPost` stays as-is). In the embed branch, line :1417 becomes:
     ```cpp
     QByteArray tsToken = m_priv->timestampTransport
         ? m_priv->timestampTransport(m_priv->tsaUrl, reqDer)
         : m_priv->fetchTimestampToken(reqDer);
     ```
     (refactor `fetchTimestampToken` to take the pre-built DER or keep building
     inside — either way the diff is confined to Private + the one call site).
  2. Expose a test-only injector on the public class
     (same idiom as the existing `setTrustStoreForTest` in the validation path):
     `void setTimestampTransportForTest(TimestampTransport fn);` declared in
     src/engines/SignatureManager.h next to the other `*ForTest` hooks, defined
     to set `d->timestampTransport`. No UI/command code touches it.
  3. The existing https refusal (:259-262) stays in the DEFAULT transport only —
     injected transports bypass it BY DESIGN (the test owns the transport; the
     production policy is pinned by a separate refused-loopback test that keeps
     using the default transport).
- **Pin design (the previously impossible tests — new file
  `tests/TestSignatureTsaSeam.cpp`, QtTest, offscreen, registered in
  CMakeLists.txt with the same `if(EXISTS)` guard pattern other optional targets
  use):**
  1. `grantedTokenEmbedsAndAttainsBt` — fixture: a real PKCS#12 (the same
     self-signed generator TestSignatureRealCrypto uses) + `setTsaUrl("https://
     tsa.invalid.example")`; transport lambda returns a REAL, well-formed TS_RESP
     bytes blob built ONCE with OpenSSL in a helper (`TS_RESP_new` +
     `TS_RESP_CTX`-free manual assembly, or a committed DER resource —
     `TS_STATUS_GRANTED`, sha256 imprint OID, embedded signer cert optional).
     Assert: outcome `Success`/B-T attained, `timestampTokenValid == true`,
     `timestampMissing == false`, and the embedded
     `id-smime-aa-timeStampToken` unsigned attribute is present in the output's
     CMS (re-read via PoDoFo/openssl and compare bytes to the injected token).
  2. `hostileErrorPageBodyIsNotEmbedded` — transport returns HTTP-200-style
     garbage (an HTML error page as QByteArray). Assert: outcome is the honest
     degradation (`timestampMissing == true`, `timestampTokenValid == false`),
     NO `/`…`timeStampToken` attribute in the output CMS, signature still valid
     B-B (integrityIntact true).
  3. `nonGrantedStatusTokenIsRejected` — well-formed TS_RESP but
     `status.status = TS_STATUS_REJECTION`. Assert: not embedded, tokenValid
     false. (At tip this MAY already fail correctly via a second-order check —
     the d2i parse succeeds for a rejection response, so the CURRENT code would
     embed it: if so, this pin catches a REAL honesty gap, not just coverage —
     scope the fix to "check `TS_RESP_get_status`-equivalent (PKIStatusInfo
     granted) before embedding" in the same seam commit.)
  4. `defaultTransportStillRefusesHttp` — no injection; `tsaUrl = "http://…"`
     loopback URL; assert refusal warning + B-B (guards the seam refactor
     against weakening the https policy).
- **Negative control design:** revert ONLY the call-site dispatch
  (`timestampTransport ? … : fetchTimestampToken` back to direct call) keeping
  the injector API compiled → tests 1-3 fail (transport never consulted —
  fixture observes no token / degraded outcome) → restore → green. Textual NC:
  pre-seam SignatureManager.h grep-zero `setTimestampTransportForTest`.
- **Risk class:** **M.** Siblings to audit: the OTHER ComputeSignature
  (B-LTA document-timestamp signer :622-690 — its `TimestampSigner` fetch also
  goes through `fetchTimestampToken`/`httpPost`; decide explicitly whether the
  seam covers it too — recommended YES, same member), `setTsaUrl` consumers
  (SecurityController/Preferences, E-5 policy-managed empty-tsaUrl refusal on
  feat/emergence-fixes 9463f6c0 — cross-lane note), OCSP fetch path if it shares
  `httpPost` (it does — same helper; the seam is deliberately
  timestamp-scoped, leave OCSP on the default), and
  TestSignatureRealCrypto's existing no-TSA/refused-loopback pins (must stay
  green unchanged).
- **Suggested lane brief:**
  1. Read SignatureManager.cpp:222-296 + :1393-1448 and TestSignatureRealCrypto
     TSA pins; write the 4 failing pins in new tests/TestSignatureTsaSeam.cpp.
  2. Land the `TimestampTransport` member + `setTimestampTransportForTest`
     injector; default path byte-identical behavior.
  3. If pin 3 exposes the rejection-status gap, gate the embed on PKIStatus
     granted and note it as a found-and-fixed honesty defect (its own ledger row).
  4. NC: dispatch-site revert → pins 1-3 fail → restore. Update SWEEP-W2B
     §Residuals 2 disposition: seam exists, hostile-responder matrix automated.
  5. Build dir discipline: `-j 2`, offscreen; PCH-bump rule if headers feed PCH.

---

## Plan 3 — L7/R14 FINDING F1 successor: runIntersects glyph-band margin merge (§2.6 row 3; §7 item 5; SWEEP-W2C §Residuals 2)

- **Residual:** Verification verdict is SPLIT. (a) The original F1 — annotation/
  form attribution partial on /Rotate pages — is **RESOLVED-AT-TIP**: commit
  **71891494** ("fix(redaction): F1 — annotation attribution + excision must use
  the RAW /Rect under the shared PageSpace law") IS an ancestor of 26c9a415;
  `collectAnnotStrings` reads `GetRectRaw()` with the rationale comment at
  src/core/RedactionProof.cpp:739-759, and rotated repros exist
  (tests/TestSep13LeadRedactionProof.cpp:78-98 rotated-page fixture;
  TestRedactionProof attribution pins). (b) The SUCCESSOR residual — the 3×fs
  ascender blanket in `runIntersects` (W2C's demonstrated false-ALARM class) —
  **STANDS-AT-TIP**: RedactionProof.cpp:792-800 still computes
  `runTop = baselineY + 3.0 * fs` (:798). The real fix EXISTS on
  `feat/runintersects-precision` (e620757b: glyph-band `inkTop/inkBottom` from
  PdfiumBackend + ±1em fallback) and is **NOT MERGED**.
- **Root cause:** `runIntersects` over-approximates each text run's vertical
  extent by 3×font-size above the baseline. Over-attribution is safe for the
  missed-secret direction (it can only widen the sweep) but W2C demonstrated the
  precision cost: a correct redaction whose mark's lower edge sits inside a
  NEIGHBOR line's headroom makes the proof sweep for (and find) the neighbor's
  still-alive text → proof FAILS a geometrically correct redaction
  (probe-w2c-loose.txt, 6/7 first run).
- **Exact change:** NOT a new patch — an **adoption plan**. Merge
  `feat/runintersects-precision` (one code commit e620757b + W2C doc/probe
  4525f0e1 + ledger flips) via the consolidation runbook (CONSOLIDATION-PLAN §5
  already rehearses this class), then verify at the merge tip: W2CProbeRotate270
  7/7 (probe-w2c-loose case flips to pass), TestRedactionProof full suite
  including the branch's 107 new pin lines, TestSep13LeadRedactionProof 8/8.
  The `hasInkBox` fallback (±1em) keeps the safe direction: never NARROWER than
  the ink band, and the missed-spot leg still fails loudly (W2C's verified half).
- **Pin design (already exists on the branch — re-verify, don't rewrite):**
  tests/TestRedactionProof.cpp gains glyph-band pins (the +107 lines) asserting
  the neighbor-line case: mark whose edge is inside the old 3×fs headroom of an
  adjacent live run no longer false-fails, while a mark overlapping the run's
  real ink band still attributes. Failing-first evidence: at tip the headroom
  pin fails (3×fs blanket) — reproducible by cherry-picking only the test hunk.
- **Negative control design:** the branch's own scoped NC (already executed by
  W2C for the W2B-1 half): revert RedactionProof.cpp's margin change alone →
  neighbor-line pin fails (false alarm returns) → restore. Post-merge, the SAME
  NC re-run at the merge tip proves the merge carried the fix (no drift).
- **Risk class:** **M.** Siblings to audit: `PdfiumBackend` text-run extraction
  gains `hasInkBox/inkTop/inkBottom` (branch touches PdfiumBackend.{h,cpp} —
  audit ALL runIntersects/text-run consumers: the proof's extracted-text sweep
  and the attribution path share the geometry, per TestRedactionProof:374-387),
  `TestSep13LeadRedactionProof` (L5/L8 rotated pins must stay green), and the
  ri-fix ledger row (flips to verified only after the merge tip verification).
- **Suggested lane brief:**
  1. Confirm e620757b diff scope (RedactionProof.cpp + PdfiumBackend + tests).
  2. Cherry-pick the test hunk onto 26c9a415-line worktree FIRST → observe
     fail-before (headroom pin) → drop.
  3. Merge feat/runintersects-precision (no strategy overrides needed — single
     overlapping file is RedactionProof.cpp, expected clean).
  4. Run W2CProbeRotate270 + TestRedactionProof + TestSep13LeadRedactionProof
     offscreen `-j 2`; NC re-run scoped to the margin change.
  5. Ledger: ri-fix row implemented-awaiting-review → verified (W2-protocol);
     cross-ref SWEEP-W2C §Residuals 2 + §2.6 row 3 of the consolidated report.

---

## Plan 4 — SignatureManager::signatureFieldAnchors rotation-imperfect read (§2.6 row 4; §7 item 7; SWEEP-W2C §Residuals 1)

- **Residual:** `signatureFieldAnchors` reads field/widget rects with `GetRect()`
  + a height-only flip — pre-existing rotation-imperfection on /Rotate 90/270;
  owner-owned, untouched by W2B-1. Consumer-side badge fix (E-3) is claimed by
  the consolidated report but lives on the UNMERGED emergence branch.
- **Verification at tip:** STANDS, with a merge-state correction.
  `SignatureManager::signatureFieldAnchors` at
  src/engines/SignatureManager.cpp:2105-2128: `const Rect r = widget->GetRect()`
  (:2118) — PoDoFo's `GetRect()` folds the page's /Rotate into the rect at read
  time (the exact read-time-adjustment defect class documented in
  RedactionProof.cpp:739-745 and fixed there by 71891494) — followed by the
  viewer top-left flip comment (:2123). The consumer fix E-3
  (feat/emergence-fixes ee2cf661 "read signature badge anchors through the
  page-space law") is **NOT at tip** — `git merge-base` confirms
  feat/emergence-fixes is not an ancestor of 26c9a415.
- **Root cause:** Same family as W2B-1/71891494: raw user-space /Rect is the
  only geometry that composes with the page-space law; `GetRect()` pre-applies
  rotation (and on 90/270 swaps W/H), then the consumer re-applies its own
  mapping — double-applied rotation, transposed anchors.
- **Exact change:** Two-part. (1) MERGE feat/emergence-fixes (ee2cf661 E-3 first,
  then the rest of EM-1..6 — see Plan 6) so the badge consumer reads anchors
  through PageSpaceTransform's law. (2) ENGINE-side re-audit (the standing
  owner request): change `signatureFieldAnchors` to read the RAW /Rect from the
  widget/field dictionary (PoDoFo `GetRectRaw()` idiom, mirrored from
  RedactionProof.cpp:758-764) + `pageGeometryFromMediaBox` to emit
  display-space anchors ONCE at the API boundary; keep the returned struct's
  contract (SignaturesPanel + SigningRequestRunner + ISignatureManager.h
  signature unchanged) and delete the per-consumer flip compensation that
  E-3's consumer code added, so the law lives in exactly one place (the engine).
- **Pin design (failing-first):** `tests/TestSignatureBadges.cpp` (or a new
  `TestSignatureAnchors.cpp` linked against pdfws_engines) —
  `anchorsOnRotatedPagesMatchLaw`: fixture PDFs built like
  TestSep13LeadRedactionProof's `makeRotatedPdf` (page dict /Rotate 90 and 270 +
  offset-origin MediaBox variant), one signature field per fixture at a known
  user-space rect; assert every returned anchor's display rect equals the law
  literal for that rotation (the same literals W2C verified for fields:
  rot90 Letter (211,147,131×61) → display x from [147 211 208 342] family;
  rot270+offset → [356 435 439 556] family). At tip (post-E-3-merge, pre-engine
  fix) the pin pins CURRENT consumer behavior; if the engine fix lands first the
  pin fails on transposed anchors — either order yields an observable fail state
  on the unfixed half.
- **Negative control design:** scoped revert of the engine's raw-read only
  (restore `GetRect()` + flip) → the rotated-literal pin fails on 270+offset →
  restore → green. Additionally the W2C NC trick applies: `git checkout 84a19f9
  -- src/core/PageSpaceTransform.h` re-transposes the whole law family — the
  pin must fail there too (proves it exercises the law, not literals).
- **Risk class:** **M.** Siblings to audit: `SignaturesPanel` (badge placement),
  `SigningRequestRunner` (anchor use in runSignStep prepared state),
  `SignatureFieldCreator` (creation-side rects — ALREADY law-corrected by
  W2B-1/W2C, must not be double-corrected), `FormManager` form-widget reads
  (same GetRect idiom — audit but do NOT change in this lane), and the E-3
  consumer code being replaced (its QEXPECT_FAIL/XFAIL markers, if any, flip).
- **Suggested lane brief:**
  1. Merge feat/emergence-fixes per Plan 6 sequencing; run TestSignatureBadges.
  2. Write the failing rotated-literal pin against `signatureFieldAnchors`.
  3. Switch the read to GetRectRaw + PageSpaceTransform; return display-space
     anchors; remove E-3's per-consumer compensation.
  4. NC: GetRect() revert → pin fails → restore; PageSpaceTransform NC → fails.
  5. Ledger: W2B-1 re-audit request row → resolved (engine+consumer both under
     the law); cross-ref §2.6 row 4, SWEEP-W2C §Residuals 1.

---

## Plan 5 — Sweep-legacy residual bundle (§2.6 row 5; §7 item 11)

- **Residual:** Seven recorded legacy-sweep leftovers. Verification at tip: ALL
  SEVEN STAND. Sub-plans (each is an independent S/M-sized lane; sequence
  (a)→(g) or run as one lane with one commit each):
  - **(a) extractLinks link-rect reader height-only flip — STANDS.**
    src/engines/podofo/PoDoFoBackend.cpp:5040-5122 (`extractLinks`); the flip is
    `info.rect = QRectF(x0, pageHeight - y1, x1 - x0, y1 - y0)` at **:5088** —
    ignores /Rotate and MediaBox lower-left origin (SL1's exact defect class).
    **Exact change:** read page geometry via `pageGeometryFromMediaBox` and map
    the raw /Rect through PageSpace::userToViewer (recipe = SL1's mapping, per
    SWEEP-LEGACY §"Same-class residuals"). **Pin:** new slot in the links test
    (or TestLegacyOriginSpace family): /Rotate 90 + 270 + offset fixtures with a
    URI link at a known raw /Rect; assert displayed rect equals law literal;
    fails at tip on 270. **NC:** revert the reader to the height-flip → pin
    fails → restore. **Risk M** (siblings: GpMainWindow link-click hit-testing,
    PdfEditorEngine::extractLinks wrapper contract, docs export of links).
  - **(b) T2-2 Find&Replace replacement writer height-only flip — STANDS.**
    `PoDoFoBackend::replaceTextRegions` PoDoFoBackend.cpp:2724; the draw-side
    flip at **:2784-2790** (`const double pdfTop = pageHeight - r.y();` …
    `DrawText(..., baseline)`). Same recipe as (a). **Pin:**
    replaceTextRegions on /Rotate 270 offset fixture — replacement text drawn at
    the law-mapped baseline (assert via PDFium text extraction rect roundtrip,
    the W2C probe idiom); fails at tip. **NC:** revert writer flip → pin fails.
    **Risk M** (siblings: PdfEditorEngine::replaceTextRegions :1620 contract,
    FindReplaceDialog UX flow, TestT2 family pins).
  - **(c) AP-stream BBox aspect on /Rotate pages — STANDS.** Appearance-stream
    BBox built at PoDoFoBackend.cpp:4439-4446 (`bboxArray` 0,0,W,H — unrotated
    aspect; position correct, /Matrix rotation deferred per sweep-legacy).
    **Exact change:** when the target page /Rotate ∈ {90,270}, set BBox to the
    transposed aspect (or add /Matrix) so stamp/image APs are not stretched.
    **Pin:** stamp a fixed-aspect image on a /Rotate 270 page → read back the
    AP dict → assert BBox aspect matches displayed aspect; fails at tip.
    **NC:** revert BBox aspect branch → pin fails. **Risk M** (siblings:
    SignatureFieldCreator AP construction — signature APs ride the W2B-1 fix
    boundary and must stay untouched or be re-pinned, StampLibrary consumers,
    TestSignatureAppearance 14/0 baseline).
  - **(d) PdfPageOps direct-write without SafeSave — STANDS.**
    src/engines/podofo/PdfPageOps.cpp:131-156 `writeDocumentFromPages` ends in
    `dstDoc.Save(outputPath.toUtf8().constData())` (:153) — in-place direct
    write; a crash mid-save truncates the ORIGINAL (mitigated today only by
    explicit Save-As dialogs). **Exact change:** route through
    `gp::SafeSave::makeUniqueCandidate` + `commitFileToDestination`
    (the BatchMode.cpp:1157-1161 idiom — already includes E-6's
    destination-identity precondition once feat/emergence-fixes merges).
    **Pin:** TestPageOps slot: writeDocumentFromPages whose destination commit
    is interrupted (inject via a read-only destination dir or a failing
    rename hook) → original bytes unchanged; fails at tip. **NC:** revert to
    direct Save → pin fails. **Risk M** (siblings: PagesMode + BatchMode
    callers of writeDocumentFromPages, tmp-dir discipline, disk-full paths).
  - **(e) exportToImage out-of-range "page" option renders ALL pages — STANDS.**
    src/engines/ConversionManager.cpp:441-457: `if (options.contains("page") &&
    page >= 0 && page < backend.pageCount())` (:449) — an out-of-range page
    (API/batch callers only) silently falls through to the all-pages loop
    (:456). **Exact change:** else-branch distinguishes "no page option" (all
    pages, current contract) from "page option present but out of range"
    (honest refusal / clamped error result). **Pin:** ConversionManager test:
    `page = 999` → expect failure-or-error result and exactly zero output
    files; fails at tip (writes N files). **NC:** revert guard → pin fails.
    **Risk S** (siblings: ConvertController options plumbing; UI never passes
    out-of-range — disclosed as API/batch-only).
  - **(f) CSV valid UTF-8 without BOM — STANDS (consumer-note class).**
    `ConversionManager::exportToCsv` ConversionManager.cpp:468-470 opens a bare
    `QTextStream` — no BOM. **Exact change (recommended):** prepend
    `QByteArray("\xEF\xBB\xBF")` on write (one line) OR keep and strengthen the
    consumer note in docs — decide by one Excel round-trip experiment; the
    spec-legal default is fine for parsers, hostile only to Excel.
    **Pin (if BOM adopted):** export → first three bytes are the BOM; fails
    pre-fix. **NC:** remove BOM line → pin fails. **Risk S** (siblings: any
    consumer diffing exported bytes — export comparison fixtures).
  - **(g) Negative /Rotate modulo (spec-legal) — STANDS as verification-grade
    residual.** The law itself is safe: `normalizeRotation`
    src/core/PageSpaceTransform.h:54-59 handles negatives (`r < 0 → r += 360`).
    The residual is the UNAUDITED set of raw /Rotate readers outside the law.
    **Exact change:** audit-only lane: grep all `/Rotate`/`GetRotate` reads in
    src/; every consumer must go through `normalizeRotation`; add pins with
    /Rotate -90 and 450 fixtures to TestRotate270PageSpace (both normalize to
    270 / 90). **NC:** break normalizeRotation (drop the `r<0` branch) → the
    new pins fail. **Risk S→M** (whatever the grep finds; expected: all readers
    already post-W2B-1 route through the header).
- **Cross-links:** §2.6 row 5; §7 item 11; SWEEP-LEGACY-2026-09-20.md
  §Coverage + §Residuals (rows: extractLinks ~:4992-note [now :5040], T2-2
  writer, AP-aspect, PdfPageOps, exportToImage, CSV, negative modulo); ledger
  §sweep-legacy rows.

---

## Plan 6 — EM-1..6 + San-UAF + ri-fix: verification review + merge sequencing (§2.6 row 6; §7 items 1, A4 slot)

- **Residual:** Process residual: the newest fix rows await verification review.
- **Verification at tip:** STANDS, and sharper than the report states: the fixes
  are ALSO not on the mainline. feat/emergence-fixes (EM-1 645f4994 → EM-6
  c1552c26 + ledger 65500182) is **NOT an ancestor of 26c9a415**; likewise
  feat/runintersects-precision (ri-fix e620757b) and the San-UAF
  sanitize-crash lane. Nothing at the tip contradicts the fixes; they are
  parked on lane branches awaiting A4 review + consolidation merges.
- **Root cause:** Sweep sequencing: fix lanes ran docs+code on their own
  branches; consolidation (CONSOLIDATION-PLAN §5) and verification (W2
  protocol) are deliberately serialized behind the re-soak window (ends
  2026-09-22T20:49:07+03) — the residual is the schedule, not a defect.
- **Exact change:** No production change. (1) Verification lane: run the W2
  protocol per row — EM-1..6 (each has its own pin suite), San-UAF
  (TestSanitization loop-repro first: SOAK-VERDICT §10.2 calls the
  `PdfDataContainer::AssertMutable` SegFault crash-class — "do not ship the
  next candidate without an explanation"), ri-fix (Plan 3). (2) Consolidation
  execution lane: merge in dependency order — ri-fix and emergence-fixes have
  ZERO overlapping production files with each other (RedactionProof/Pdfium vs
  forms/OCR/signatures/save), so either order is clean; AM1/AM2 are already
  merged (5c02b01d, 791115bb). (3) Flip ledger rows verified/contradicted per
  A4, then record A4.
- **Pin design:** N/A (process). The measurable gate = A4's pass accounting:
  per-row verdict + ledger flip commit. For San-UAF the gate IS a looped repro
  command (SOAK §10.2: `TestSanitization testSanitizeGeneratesUniqueTrailerID`
  in a loop until AssertMutable fires or N=500 clean).
- **Negative control design:** The W2 protocol's own refute step: for each EM
  row, revert the row's fix commit on a scratch branch → its pin must fail →
  restore. (EM rows landed with fail-before evidence; the review re-samples a
  subset, minimum EM-6 (SafeSave boundary) + EM-1 (EditPolicy gates).)
- **Risk class:** **L (schedule risk, not code risk).** Siblings: the re-soak
  verdict (A3) gates the candidate; consolidation runbook §5 merge order; any
  fix lane touching EM-touched files (BatchMode, SecurityController TSA refusal)
  must rebase AFTER these merges.
- **Suggested lane brief:**
  1. After re-soak verdict lands (A3), execute CONSOLIDATION-PLAN §5 merges for
     runintersects-precision + emergence-fixes (+ sanitize-crash).
  2. Run W2-protocol verification per row; loop-repro TestSanitization ×500.
  3. Flip ledger rows; record ADDENDUM SLOT A4 with verdicts + evidence paths.
  4. Cross-ref §2.6 rows 3, 6, 7 and Plans 3/7 of this document.

---

## Plan 7 — W2C residuals: /NM dedup experiment + TestRedactionProof flake triage (§2.6 row 7; SWEEP-W2C §Residuals 3, 5)

- **Residual:** (a) Chained annotation re-embed appends without /NM dedup —
  can the app's save flow re-embed the same annotation id twice on one
  lineage? (b) TestRedactionProof `proofFailsOnXmpSurvivor` pre-existing
  load-sensitive flake. (Note: W2C's "runIntersects precision RESOLVED by
  ri-fix" is resolved-ON-BRANCH only — see Plan 3.)
- **Verification at tip:** BOTH STAND. (a) `applyAnnotationsToDoc`
  (src/engines/podofo/PoDoFoBackend.cpp:4115-4514 call at :4514) builds
  `idToObjectMap` and CREATES new annots/XObjects per run (the W2C move probe's
  stage-B file carries the stale stage-A dict by construction); no /NM
  lookup-skip exists on the create path. (b) `proofFailsOnXmpSurvivor` at
  tests/TestRedactionProof.cpp:588 — present at tip, flake classification
  unchanged (passed in W2C's runs).
- **Root cause (a):** The re-embed path treats `AnnotationItem` lists as
  create-only; a lineage where the same logical annotation is applied twice
  (two save passes over an in-session overlay) appends a second widget with the
  same /NM instead of updating in place. Whether the UI can actually produce
  that lineage is the OPEN question (hygiene, not a demonstrated defect).
- **Exact change (a) — experiment FIRST, fix scoped to its outcome:**
  Experiment (design, runnable by a future lane): drive the production edit
  path twice — load → EditAnnotationCommand move → save (stage A) → reload
  stage-A file → move the same annotation again → save (stage B); then grep
  stage B's page /Annots for duplicate /NM values (W2C's probe already emits
  such files). ALSO drive the pure-UI lineage: annotate → save-in-place →
  annotate again → save, then same duplicate check. Outcomes: (i) UI lineage
  duplicates → real defect: fix `applyAnnotationsToDoc` to upsert by /NM
  (find existing widget dict with equal /NM → mutate geometry in place, skip
  create); (ii) only the double-ENGINE lineage duplicates → guard anyway
  (cheap upsert) + register the engine-path-only limitation; (iii) no
  duplication anywhere → close the question with the probe as evidence,
  update SWEEP-W2C §Residuals 3 disposition to closed-no-defect.
- **Pin design (a):** slot `reembedSameAnnotationDoesNotDuplicateNm` in
  tests/TestRedactApplyMarks or a new TestAnnotationLineage: stage-A/B
  sequence above; assert stage B's /Annots contains exactly ONE widget with
  the annotation's /NM and its /Rect is the stage-B law literal. Fails under
  outcome (i)/(ii) at tip; if outcome (iii), the pin still documents the
  single-widget contract (write it to match the DECIDED contract).
- **Negative control design (a):** revert the upsert branch (create-always)
  → the pin fails with 2 widgets sharing /NM → restore → green.
- **Exact change (b):** triage, don't blanket-retry. Read the XMP-survivor
  proof test's shared state: it runs in-suite after excision tests (load-
  sensitive per W2B). Steps: (1) run it 50× standalone + 50× in-suite locally,
  capture any failure's actual; (2) if it reproduces: it is a real ordering
  dependency — fix the test's isolation (own QTemporaryDir + no dependency on
  prior suite state), NOT production; (3) if it never reproduces at -j2
  offscreen: add `@try`-style flake register row (ledger) with the observed
  history (W2B report + W2C pass) and arerun-once policy per repo flake rules
  (TestRedactionProof is NOT on the sanctioned flake list — that is the gap
  this plan closes: either fix isolation or add it to the documented list).
- **Pin design (b):** the isolation fix IS the pin: assert the test creates
  its own fixture path and asserts on it only (code-review-level pin + the 50×
  stability run as evidence).
- **Negative control design (b):** re-introduce shared-state dependency
  (revert isolation) → in-suite ordering test fails → restore. For (a): the
  NC above.
- **Risk class:** **S** for (b); **M** for (a) (siblings: EditAnnotationCommand,
  AnnotationSerializer (id minting), the stamp AP path at PoDoFoBackend
  :4439-4446 which shares the re-embed code, redaction overlays that also
  write annots).
- **Suggested lane brief:**
  1. Run the two-lineage experiment; record outcome class (i)/(ii)/(iii).
  2. Implement per-outcome: upsert-by-/NM or close-with-evidence.
  3. Pin + NC per above; run TestRedactApplyMarks + TestRedactionProof.
  4. Triage the XMP flake (50× + isolation fix or sanctioned-flake-list row).
  5. Ledger rows for both; cross-ref SWEEP-W2C §Residuals 3/5, §2.6 row 7.

---

## Plan 8 — Adversary hypotheses W1-H1/H2/H3: the missing experiments, designed (§2.6 row 8; §7 item 14) — DEEP PLAN (b)

- **Residual:** Three hypotheses left HYPOTHESIS/INCONCLUSIVE with named
  missing pieces (SWEEP-W1-ADVERSARY §HYPOTHESES :139-144).
- **Verification at tip:** ALL THREE STAND (mechanism-level):
  - **W1-H2** is code-confirmed at tip:
    src/shell/controllers/SendForSigningController.cpp:179-201 — on
    `StepRefusal::DocumentChanged`, the code asks
    `QMessageBox::question(_mainWindow, tr("Document Changed Since
    Preparation"), …"Re-confirm this signing request against the document as
    it is now?")` at :180-185 and on Yes sets
    `input.userReconfirmedSha256 = pre.documentSha256` (:199-201) — the gate
    DETECTS and requires consent, but nothing re-renders the new bytes (no
    `markReload()`/viewer reload, no hash/summary in the dialog) before the
    question: the user consents to bytes they have not seen.
  - **W1-H3** is code-confirmed at tip: `runPresetPipelineStep`
    (src/modes/BatchMode.cpp:1088-1170) appends each candidate to
    `intermediates` (:1124) and the cleanup `for (const QString& path :
    intermediates) QFile::remove(path);` sits at :1164-1166 — AFTER the loop,
    on the normal-return path only. `runPresetMutatingStep` (:1016) calls
    engine seams (`optimizeDocument` / `exportPdfA` / `sanitizeDocument`) that
    can throw; the caller's mapped-lambda catch-all sits ABOVE this function,
    so a throw skips :1164-1166 and orphans candidates in the shared
    `%TEMP%/glyphpdf-candidates`.
  - **W1-H1** remains harness-bound: single-instance design; the commit-half
    mitigation (E-6 destination-identity precondition at the SafeSave
    boundary, c1552c26) is NOT at tip (unmerged — Plan 6), so at tip even the
    known mitigation is absent from the mainline.
- **Root cause:** Each hypothesis lacks exactly one piece of experimental
  machinery: H1 a two-process driver; H2 a GUI-flow driver that can swap the
  file mid-session; H3 a crafted PDF that makes an engine seam throw mid-chain.
- **Experiment designs (for future lanes; each is harness-only — no production
  edits until a hypothesis CONFIRMS):**
  - **W1-H3 (cheapest, run FIRST — pure test-side):**
    1. Craft the thrower: build a fixture PDF that passes
       `loadDocumentForEditing` but makes a mutating seam throw. Candidate
       seams: corrupt xref-adjacent structures that PoDoFo only touches during
       save (PdfEditorEngine exceptions are std::runtime-based — a /Pages tree
       that fails during `exportPdfA`'s re-write is the classic). Fallback if
       crafting is unreliable: a DEBUG-only env-guarded fault-injection hook
       (`GLYPHPDF_FAULT=optimizeDocument-throw`) in PdfEditorEngine —
       disclosed, test-only, same idiom as trust-store test hooks; prefer the
       crafted PDF, keep the hook as plan B with the honest label.
    2. Driver: extend tests/TestSweepW1PresetAdversary.cpp (already linked and
       registered per the adversary build notes) with a two-step preset whose
       step 2 throws on the fixture; assert AFTER the run:
       `%TEMP%/glyphpdf-candidates` contains orphaned candidates
       (`QTemporaryDir` sweep + count>0) — CONFIRMS the leak; then the SAME
       run with a try/catch-wrapped cleanup (the future fix) shows count==0.
    3. Consequence bound (already documented): temp pollution only; the
       `candidatesDirClean` pin stays the honest baseline.
    Fix shape IF confirmed: wrap the pipeline body in
    `try { … } catch (…) { for (path : intermediates) QFile::remove(path);
       throw; }` — a scope-guard (RAII `IntermediatesSweeper`) is the clean
       form; NO behavior change on the non-throw path.
  - **W1-H2 (GUI-flow harness):**
    1. Harness: extend the UX lane's offscreen flow driver (SWEEP-W3-UX
       harness slots, feat/sweep-w3-ux-resume 3f957d72 pattern): load a
       signing session for fixture F1; before invoking the run-sign step,
       overwrite the document path on disk with F2 (different content, same
       name); drive the controller to the re-confirm dialog; capture the
       screen state (offscreen QWidget::grab) + the viewer's rendered page
       hash at the moment of the question.
    2. Assertion (the experiment's output): the viewer still displays F1's
       bytes while the dialog asks about "the document as it is now" —
       demonstrating consent-before-reload; record F2's sha256 ≠ displayed
       sha256 as the evidence artifact.
    3. Fix shape IF confirmed (the report already sketches it): EITHER
       `markReload()` (viewer reloads + re-renders BEFORE the dialog) OR the
       dialog itself renders the new bytes' sha256 + page-count summary so
       consent is informed without trusting the stale view. Pin: dialog
       content contains the new digest; fails at tip. NC: revert dialog/
       reload change → pin fails.
  - **W1-H1 (two-process harness, P2 posture):**
    1. Scope honestly: P1 is single-user/single-instance — this experiment
       exists to decide whether P2 multi-session needs an engineering fix or a
       documented limitation.
    2. Harness: a small Qt-free driver (or two QProcess instances of the app
       driven via the existing batch/CLI entry points) both executing
       `runSignStep` on the SAME doc+sidecar pair with staggered starts
       (instance A: 0 ms; instance B: A-mid-write, approximated by starting B
       when A's candidate file appears in %TEMP%). Instrument: record both
       outcomes' sha256 + sidecar state after both exit.
    3. Assertion: lost-update = exactly one instance's signature present AND
       the other reported Success without refusal. E-6's merged
       destination-identity precondition should turn the interleaving into an
       honest second-instance refusal — so run the matrix twice: on the tip
       WITHOUT E-6 (document the raw lost-update, closing the hypothesis with
       mechanism-level evidence) and after Plan 6 merges E-6 (document the
       refusal — the mitigation working). Both outcomes CLOSE the hypothesis;
       only "silently dropped signature at the merge tip" would demand more.
- **Negative control design:** per experiment: the no-fault twin run must be
  clean (H3: zero-step preset leaves no orphans; H2: no-swap run has no
  dialog; H1: single-instance run signs normally) — each experiment ships with
  its own control so a positive result is attributable to the mechanism, not
  the harness.
- **Risk class:** **S** for H3 harness (test-only), **M** for H2 (GUI driver +
  possible dialog change), **L** for H1 (multi-process, E-6 dependency).
  Siblings to audit: H3 — the OTHER SafeSave candidate-chain seams
  (`runFillStep`, `SignatureFieldCreator`, `ReviewSummaryWriter::writePrintable`
  — the 5c8fd08 discipline quartet; the scope-guard should be evaluated for
  all four, not just presets); H2 — SigningRequestRunner::precheck contract
  (the refusal must keep working unchanged); H1 — SafeSave commit boundary
  consumers (every commitFileToDestination call site).
- **Suggested lane brief:**
  1. H3: craft thrower fixture (plan-B fault hook), add the orphan-count pin
     to TestSweepW1PresetAdversary, confirm/refute, land scope-guard fix if
     confirmed.
  2. H2: extend UX offscreen driver with mid-session swap; capture
     displayed-vs-disk digest evidence; implement informed-consent dialog or
     markReload; pin + NC.
  3. H1: two-process matrix (tip, then post-E-6-merge); close the hypothesis
     with either the refusal (good) or the documented P2 limitation.
  4. Ledger rows per hypothesis: CONFIRMED(+fix)/CLOSED(evidence), cross-ref
     SWEEP-W1-ADVERSARY §HYPOTHESES and §2.6 row 8.

---

## Plan 9 — UX batch-B/F2b-D1: merge the landed fixes (§2.6 row 9; §7 item 2 + A1 slot)

- **Residual:** Flows 4-7 "unrun (disk guard)" + F2b-D1 "one-line mkpath fix
  proposed, not yet landed" + F2a-F1 + drag native-confirmation.
- **Verification at tip:** RESOLVED-ON-LANE-BRANCH-UNMERGED — the report is
  stale relative to `feat/sweep-w3-ux-resume` (3f957d72): batch B ran COMPLETE
  (flows 4-7 verdicts recorded; one new misleading-state finding F6-F1), and
  **F2b-D1 is FIXED there** — commit **72069bd8** "fix(batch-presets): mkpath
  the store root at the save/rename boundary" with pin
  `TestBatchPresets::firstSaveOnCleanProfileCreatesStoreRoot` + fail-before +
  NC (evidence-w3-ux/ux-pin-f2bd1.txt). At tip 26c9a415 the breaker is LIVE:
  src/core/BatchPreset.cpp has NO `mkpath` (grep-zero at 26c9a415).
  F2a-F1 (merge-output naming, one `.arg()`) and F4d-D1 (background commit
  viewer-handle coordination) remain open ON the lane branch too.
- **Root cause:** Lane sequencing: the resume lane finished after the
  consolidated report's §2.6 snapshot; its work sits unmerged.
- **Exact change:** Adoption, not new code: merge `feat/sweep-w3-ux-resume`
  (docs + the one production commit 72069bd8 + harness repairs). Then the two
  still-open one-liners IF the coordinator wants them in the same pass:
  F2a-F1 (name the merge output in the completion modal — single `.arg()` in
  the merge completion path) and F6-F1 (PubSec open-failure wording —
  detection branch at the load-failure path naming certificate encryption).
  F4d-D1 is a real routing/commit-ownership change — separate lane (the
  branch's own Top-5 ranks it #2 with the fix shape written there).
- **Pin design:** already exists on the branch
  (`firstSaveOnCleanProfileCreatesStoreRoot`, 15/15 + fail-before + NC).
  Re-run at the merge tip as the verification. For F2a-F1 (if taken): pin =
  completion-modal text contains the output file name (offscreen dialog
  scrape idiom, fails pre-fix).
- **Negative control design:** branch's own NC (revert mkpath → pin fails →
  restore) re-verified at merge tip; for F2a-F1: revert `.arg()` → pin fails.
- **Risk class:** **S** (merge + one-liners). Siblings to audit:
  BatchPresetStore save/rename callers (preset pipeline run path, presets UI),
  TestBatchPresets suite, the merge-completion modal code path (F2a-F1), and
  the F4d-D1 XFAIL marker in flow4 (flips to XPASS when F4d-D1 is ever fixed —
  do NOT remove the marker in this pass).
- **Suggested lane brief:**
  1. Merge feat/sweep-w3-ux-resume; re-run TestBatchPresets (expect 15/15).
  2. Take F2a-F1 (one .arg + pin) and optionally F6-F1; leave F4d-D1 parked.
  3. Record ADDENDUM SLOT A1 cross-refs: batch-B verdicts + F2b-D1 fix SHA.
  4. Ledger: F2b-D1 row implemented-awaiting-review → verified post-merge.

---

## Plan 10 — Devops residuals: D1 models bootstrap, C1 fuzz CI, cleanup batch (§2.6 row 10; §7 items 8-10; SWEEP-W3-DEVOPS §7.1/§7.3)

- **Residual:** D1 models bootstrap gap (top gap: fresh clone cannot build the
  MSI); D2 models missing in 5/9 worktrees (pdf-clean still lacks them); C1
  fuzz workflow likely cannot pass (runner + missing cmake/ninja); C2/C3 stale
  CI comments; D3-D6 packaging dead scripts/comments; B1 release-box VCRT;
  §3.5 NOT-VERIFIED register (second-machine install, VCRT adequacy, veraPDF
  bundle, first-run no-network).
- **Verification at tip:** STANDS (verified by reading at 26c9a415):
  bootstrap script set still lacks a models step (no `models` reference in the
  bootstrap tooling per the devops audit's finding, unchanged at tip);
  `.github/workflows/glyphpdf-fuzz.yml` still pins `windows-latest` and no
  cmake/ninja in the msys2 install list (C1); stale comments C2/C3 present;
  packaging dead scripts (check-deps.bat, deploy-msys2.bat) present.
- **Root cause:** Operational gaps, each already root-caused by the devops
  lane with fixes specified but deliberately not coded (doc-only lane).
- **Exact change (four small, separate lanes):**
  1. **D1:** extend the vendor bootstrap script with a `models` step — the 5
     PROVENANCE.md URLs + SHA-256 pins already exist; download to `models/`,
     hash-verify, fail loudly on mismatch. Also restore `models/` in pdf-clean
     (copy from `D:/pdf/pdf` + verify 5 pins) BEFORE any integration packaging
     (devops §7.3 item 2 command).
  2. **C1:** 3-line CI edit: `windows-2022` runner + add `cmake` and `ninja`
     to the setup-msys2 install list. C2/C3: delete the two stale paragraphs.
     Validation: the job's Configure step on a runner (or at minimum a YAML
     lint + actionlint pass).
  3. **D3/D4:** delete `packaging/check-deps.bat` + `packaging/deploy-msys2.bat`
     (referenced-by-nothing verified by the devops lane); D5/D6: fix or delete
     the two stale packaging comments (WiX merge-module sentence; winget
     version header).
  4. **B1 + §3.5:** release-box task list (install VC++ 2015-2022 x64
     redistributable; execute the four NOT-VERIFIED items on a second machine)
     — schedule for the release-hardening pass; no repo change beyond a
     checklist doc row.
- **Pin design:** D1: bootstrap `check` mode gains models coverage — a
  hash-mismatch drill (flip one byte in a staged model → bootstrap check must
  exit nonzero). C1: CI truthfulness pin = the workflow YAML greps clean for
  `windows-2022` + `cmake`/`ninja` in the install list (a doc-lint-level
  assertion in the cleanup commit message + a workflow-lint run).
- **Negative control design:** D1: remove the models step → the check drill
  fails (models uncovered again); C1: revert the runner line → the YAML pin
  fails. (CI cannot be executed from this box — the pins are structural, the
  devops lane's honest boundary.)
- **Risk class:** **S** per item; D1 is **M** operationally (network fetch +
  new gate logic deserves its own reviewed change — the devops lane's own
  caveat). Siblings: deploy.ps1 (hard-fails without models — after D1 it
  should SUCCEED from clean, which is the acceptance test), PROVENANCE.md
  (single source of pins — do not fork the pins), fuzz lane owner (C1).
- **Suggested lane brief:**
  1. D1: bootstrap models step + pdf-clean restore + mismatch drill.
  2. C1/C2/C3: the 3-line CI edit + 2 comment deletions; actionlint.
  3. D3-D6: one-commit dead-script/comment cleanup (no behavior change).
  4. B1/§3.5: second-machine checklist into the release-hardening backlog.
  5. Cross-ref §2.6 row 10, SWEEP-W3-DEVOPS §7.1 rows D1/C1/D3-D6/B1.

---

## Plan 11 — W2 cleanup residuals (§2.6 row 11; §7 item 10; SWEEP-W2-TESTING §Residuals + ARCHAEOLOGIST §8 + SOAK-VERDICT §10.3)

- **Residual:** Five test/hygiene items. Verification at tip: ALL STAND.
  1. TestLaneScheduler wall-clock bound — tests/TestLaneScheduler.cpp:147-153
     (`elapsed < 10 * 100` "must beat serial sum") is load-sensitive by
     construction.
  2. TestSignatureValidation merge-or-retire — file present; the tautological
     assertion is at tests/TestSignatureValidation.cpp:131-132
     (`QVERIFY2(!sig.signerName.isEmpty() || sig.signerName.isEmpty(), …)`);
     3 unique pins to port identified by W2 (re-confirmed by archaeologist §8).
  3. R14ProbeBatchSkip — tests/R14ProbeBatchSkip.cpp EXISTS at tip and is
     UNREGISTERED (no CMakeLists reference — grep-zero), i.e. dead test
     infrastructure; archaeologist recommends register.
  4. TestEngineSave real-store QSettings — tests/TestEngineSave.cpp:226-239
     constructs `QSettings settings;` against the real user store (H2).
  5. Soak §10.3 candidates-dir hardening beyond RESOURCE_LOCK
     (SOAK-VERDICT §10.3 + §4.3: `leftoverCandidates()` over the shared
     `%TEMP%/glyphpdf-candidates`; ae636a5 RESOURCE_LOCK fixed the count
     family; soak shows leftover state can still leak at -j1).
- **Root cause:** Test-design debt accumulated under sweep pressure; each item
  already has a documented direction (W2 §Residuals).
- **Exact change:**
  1. TestLaneScheduler: redesign the bound from wall-clock to
     event-count/completion-ratio (assert all N jobs complete + completion
     count beats serial INVARIANT, or use a deterministic mock clock);
     delete `elapsed < 1000` entirely.
  2. TestSignatureValidation: port the 3 unique pins into
     TestSignatureRealCrypto (or TestSignatureBadges where they fit), then
     retire the file + CMake target; fix the :132 tautology as part of the
     port review (it asserts nothing — the ported pins replace it).
  3. R14ProbeBatchSkip: REGISTER it (add the CMake target + `if(EXISTS)`
     guard) and mark it `-D`-gated or tagged `R14-probe` so it runs
     deliberately; annotate the R14 doc (archaeologist's recommended option).
  4. TestEngineSave: inject a temp-dir-scoped QSettings
     (`QSettings::setPath`/`QSettingsSettings`-style fixture or
     `GLYPHPDF_SETTINGS_PATH`-equivalent env seam if the engine reads a named
     file) so the test never reads/writes the developer's real profile (H2).
  5. Candidates-dir: per-suite unique candidates root
     (`%TEMP%/glyphpdf-candidates/<suite-pid>/`) with the root swept at suite
     start/end — OR a suite-scoped env override consumed at
     `SafeSave::makeUniqueCandidate`; keep the shared dir for production.
- **Pin design:** 1: new bound survives a synthetic 500 ms stall (inject via a
  slow-job flag) without failure while still failing if the scheduler
  serializes (ratio pin). 2: the ported pins' pass + the retired target's
  absence from CMake (grep pin). 3: target builds + runs from a clean
  configure. 4: after the fixture, the test's QSettings writes land in the
  temp profile (assert file path prefix) — the real-store file untouched
  (hash the real profile path before/after where accessible). 5: two
  concurrent suites with the per-suite root observe zero cross-suite leftover
  counts (the ae636a5 repro, now green without RESOURCE_LOCK dependence).
- **Negative control design:** per item, revert the change → the pin fails
  (1: stall pin fails under wall-clock bound; 4: temp-scoping removed →
  real-path prefix assertion fails; 5: shared-root restored → cross-suite
  count pin fails). 2/3 are structural (grep/config pins invert cleanly).
- **Risk class:** **S** each (test-only except 5's small engine seam).
  Siblings: 4 — every test touching QSettings (grep tests/ for `QSettings`);
  5 — all SafeSave candidate consumers (the 5c8fd08 quartet + batch OCR path);
  2 — the merge targets' existing expectations (name collisions with ported
  slot names).
- **Suggested lane brief:**
  1. One cleanup pass lane, one commit per item, zero production change
     (except the candidates-root seam if adopted).
  2. Order: 3 (register) → 2 (port+retire) → 1 (bound redesign) → 4
     (QSettings isolation) → 5 (candidates root).
  3. Each with its NC per above; ledger rows implemented-awaiting-review.
  4. Cross-ref §2.6 row 11; SWEEP-W2-TESTING §Residuals; ARCHAEOLOGIST §8
     items 1-2; SOAK-VERDICT §10.3.

---

## Plan 12 — Archaeologist dispositions: AnnotationToolBar revival question + watch-list records (§2.6 row 12; §7 item 10 tail)

- **Residual:** 0 PROVEN-SAFE deletions (triple bar unmet — a finding, not
  actionable); AnnotationToolBar.{cpp,h} KEEP-ANYWAY with an open revival
  question the parity lane should close EXPLICITLY; LibSecretStore
  platform-gated load-bearing (never propose deletion on Windows-build
  evidence); TestImageDedup alive with its 09-09 ledger row superseded.
- **Verification at tip:** STANDS. src/ui/AnnotationToolBar.cpp and .h present
  at tip (kept); revival marker commit **e5a5f012** exists
  ("AnnotationToolBar kept current for future revival", ribbon Comment tab
  exposes all 13 markup tools). LibSecretStore gated at CMakeLists.txt:464-479
  (conditional sources). tests/TestImageDedup.cpp present (alive, registered).
- **Root cause:** The only actionable item is a DECISION gap: the file is
  kept-with-marker but nobody owns the keep/kill decision.
- **Exact change:** Close the question explicitly, either direction, with a
  one-commit record: (a) KEEP: replace the vague revival marker with a
  dated, owner-named keep-note in AnnotationToolBar.h's header comment
  ("kept as revival base for <specific parity gap>; re-decide <milestone>") +
  ledger row flipping the archaeologist's NEEDS-REVIEW to CLOSED-KEEP; or
  (b) DELETE: port anything load-bearing (the 13-tool pin e5a5f012 pins the
  RIBBON, not the toolbar class — verify the pin does not include the class),
  delete both files + any CMake reference, ledger CLOSED-DELETE with the
  bundle-archive note (CONSOLIDATION-PLAN §6 covers reversibility).
  Recommendation: (a) KEEP — the ribbon pin e5a5f012 explicitly frames the
  class as the revival base; deleting it now buys nothing before the parity
  pass that would decide revival anyway.
- **Pin design:** For (a): a grep-level structural pin is enough — the header
  keep-note names an owner+milestone (checked by the ledger review, not a
  test). For (b): the existing ribbon pin (e5a5f012's test) must stay green
  AFTER deletion — that IS the pin (toolbar class is genuinely unreferenced).
- **Negative control design:** For (b): re-add a reference to
  AnnotationToolBar from a UI file → build breaks (trivially detectable); for
  (a): remove the keep-note → ledger review fails the row.
- **Risk class:** **S** (docs/one-file). Siblings to audit (for (b) only):
  every include of AnnotationToolBar.h (grep — expect zero beyond itself per
  archaeologist §2), CMakeLists ui sources list, the e5a5f012 pin's includes.
  LibSecretStore + TestImageDedup need NO code change: record their
  dispositions in the ledger (platform-gated load-bearing / row superseded)
  so no future deletion pass re-litigates them.
- **Suggested lane brief:**
  1. Decide KEEP vs DELETE (recommend KEEP per e5a5f012 framing).
  2. KEEP: owner+milestone keep-note in AnnotationToolBar.h; ledger
     CLOSED-KEEP row; same commit records LibSecretStore + TestImageDedup
     dispositions.
  3. DELETE (only if the parity lane objects to KEEP): port-check the ribbon
     pin, delete files + CMake refs, ledger CLOSED-DELETE, bundle note.
  4. Cross-ref §2.6 row 12; SWEEP-W3-ARCHAEOLOGIST §1-3, §6-8.

---

## Plan 13 — Research roadmap tail + matrix CSV mechanical defects (§2.6 row 13; §7 item 12; SWEEP-W3-RESEARCH §2/§4.2/§6)

- **Residual:** Open roadmap items reconciled-not-upgraded (form-JS P3,
  send-for-signing P2-P4, presets P2/P3, T2-4 tagging/PDF-UA, T2-5 batch
  split/password-strip, N5 reverse wire-up, N4 offline-degraded wording, N38
  GPO/ADMX/MSI/license tail, Tier-3 pool) + three matrix CSV mechanical
  defects.
- **Verification at tip:** STANDS. docs/research/ corpus present at tip
  (RESEARCH-BACKLOG-2026-09-10.md, form-js-implementation-plan.md, batch
  -presets-implementation-plan.md, …) with the reconciled statuses. The CSV
  defects are live at tip: `docs/audit/FEATURE-COMMAND-MATRIX-2026-09-09.csv`
  line 302 (`measure-export-csv` row) parses as **28 fields** (verified with
  awk — unquoted commas in `output_or_state_change`); duplicate
  `stable_command_id` `certify` at lines 141/306; rows 52-54 still describe
  MeasureMode as UNTRACKED WIP despite landing e73a446.
- **Root cause:** Mechanical CSV authoring (unquoted cell), id-collision
  between the old inventory and the N18 row, and a pre-landing row never
  rebaselined. Roadmap items are NOT defects — the residual is only that the
  corpus must not silently upgrade them (a program-backlog discipline).
- **Exact change:**
  1. Line 302: quote the `output_or_state_change` cell (RFC-4180 repair; the
     intended review_status "implemented-awaiting-review" becomes parseable).
  2. Lines 141/306: split the duplicated `certify` id (`certify` for the
     original, `certify-n18` or the §C alias map) — matrix owner's call at
     next rebaseline; minimal repair = alias note in the NOTES file.
  3. Rows 52-54: rebaseline to landed state (MeasureMode e73a446; panel
     wired; menu mirrors live).
  4. Roadmap tail: NO status changes — the plan is a scheduling act: the
     program backlog (RESEARCH-BACKLOG) already carries these; record
     owner+sequence for the W4 pass in the backlog doc's open-items section
     (one paragraph, no status flips).
- **Pin design:** A CSV well-formedness check (the matrix's own pin): an
  awk/PowerShell one-liner committed as a doc-tooling script or a CI-style
  check that every row parses to exactly the header's field count and
  `stable_command_id` values are unique — fails pre-repair (28-field row +
  duplicate id), passes post. Live at docs/audit (script) rather than CI.
- **Negative control design:** Re-introduce an unquoted comma in any cell →
  the checker fails; duplicate an id → fails. (Self-evident inversion.)
- **Risk class:** **S.** Siblings: consumers of the matrix CSV
  (FEATURE-COMMAND-MATRIX-NOTES §G references, any doc quoting row numbers —
  the repair shifts NOTHING if only quoting changes; the 52-54 rebaseline
  changes content → note it in NOTES §G.3), the research corpus docs that
  cite row ids.
- **Suggested lane brief:**
  1. Fix line 302 quoting; validate field count == header count via the
     checker script; commit script + repair together.
  2. Alias-note for the duplicate certify id; rebaseline rows 52-54.
  3. Backlog paragraph for the roadmap tail (owners + sequence, no flips).
  4. Cross-ref §2.6 row 13; SWEEP-W3-RESEARCH §4.2 findings table + §6.

---

## Plan 14 — Perf residuals R1-R8 (§2.6 row 14; §7 item 13; PERF-BASELINE §5-§7.1) — DEEP PLAN (c)

- **Residual:** The first measured baseline leaves eight explicitly-recorded
  coverage gaps (R1-R8), plus F5 (a 16x render succeeded where a 64 Mpx guard
  was expected to bound it — view-path scoping question) and the redact-apply
  median variance flag (32→77 ms quiet-run median, min 28 matches the loaded
  floor — re-probe before quoting either number).
- **Verification at tip:** STANDS, with two precise code anchors:
  (1) The 64 Mpx guard is VIEW-SCOPED by construction:
  src/ui/PdfViewerWidget.cpp:1213-1240 — `kMaxRenderPixels = 64*1000*1000` +
  `kMaxRenderSide` live in an anonymous namespace inside the view widget and
  bound `checkedRenderSize` into the view's render path only; the perf
  harness's 16x render (13472×9520 ≈ 128 Mpx, 84 ms) went through the
  ENGINE/Pdfium path (per-scale driver), which no guard covers — F5 explained
  at the code level. (2) The perf harness itself (tools/perf/*.cpp,
  run_perf_suite.ps1, wait_quiet.ps1) is on `feat/sweep-w3-perf` (2ec24f41)
  and NOT at tip — R-work must start by merging or vendoring that harness.
  The redact-apply re-probe need is live (baseline §7.1 note 2).
  Baseline anchors for targets: quiet-run medians — startup-external-wall
  150 ms / MainWindow ctor 95 ms; paginate 150p@2x 1298 ms; compare-50p
  807 ms / 461 MiB; redact-apply 77 ms (suspect); sign-local-p12 33 ms;
  save-roundtrip 27 ms; per-scale render 2-84 ms (0.5x-16x).
- **Root cause:** Offscreen, synthetic-corpus, driver-based measurement
  cannot see warm OS caches, interactive latency, frame pacing, GPU raster,
  or multi-monitor effects — R1-R8 are measurement-design gaps, not product
  regressions (F1's finding — startup dominated by MainWindow ctor, engine
  ~0 ms — is the only optimization-shaped lead).
- **Per-residual measure/fix/target:**
  - **R1 warm-vs-cold start split (+ exe-qrc resource loading).** MEASURE:
    cold = first launch after reboot (or `WTS`/standby-flush surrogate);
    warm = 2nd+ launch; n≥20 each; split the external wall into
    process-create / qrc-resource init (splash/icon/fonts/translations —
    currently null-path in the driver) / MainWindow ctor. FIX (plausible):
    defer non-first-paint resources (translations + icon theme) post-show;
    prelink/FullLTO already in place; expected win only if qrc init proves
    >30 ms. TARGET: warm P95 < 400 ms external wall; cold P95 < 2 s
    (the review's gate, already met ~10x at 150/174 ms — keep and record).
  - **R2 office/scan corpus.** MEASURE: 3-5 REAL complex fixtures (scanned
    image-PDF, mixed drawing+text office export, 100+ MB scan class), same
    drivers, open-first-page + paginate + redact + save. FIX: none expected
    from measurement alone; feeds F2's per-job memory budget definition.
    TARGET: record P95 open-to-first-page < 1 s per corpus class (the
    review's gate) and a memory-per-page table (the 9 MiB/page @150 DPI
    datapoint generalized); no regression gate until two runs agree.
  - **R3 interactive click/typing latency.** MEASURE: windowed (NOT
    offscreen) harness — SendInput/PostMessage-driven click on ribbon +
    keystroke into a form field, timestamp at input vs first paint delta
    (QElapsedTimer around the event + frame callback); n≥50. FIX (plausible):
    none indicated by adjacent evidence (app open 13/21 ms; redact 32/79 ms);
    only if p95 > 100 ms, profile the specific widget. TARGET: visible
    response p95 ≤ 100 ms (the review's target), recorded per interaction.
  - **R4 cancel ack/stop latency.** MEASURE: start a long op (OCR on a
    100+ page fixture or batch-50), hit cancel at a fixed offset (e.g. 500 ms
    in), time (a) UI ack (button state flips) (b) worker stop (thread join /
    flag observed) (c) partial-output disclosure text. BatchMode/OCR cancel
    flags are the instrumentation points (src/modes/BatchMode.h,
    src/modes/OCRMode.h lifecycle states :41-55). FIX: if stop > 1 s, the
    long op's inner loop lacks a cancellation check — insert per-page checks.
    TARGET: ack ≤ 100 ms, stop ≤ 1 s (the review's targets).
  - **R5 frame pacing / dropped frames / two-page path.** MEASURE: windowed
    harness, continuous scroll + zoom sweep across the supported scales on
    the 20p fixture; record present/absent-presentation gaps (swapchain
    timestamps or high-frequency paint-event deltas) for QPdfView AND the
    two-page view path. FIX: per-scale tile cache or render-thread if gaps
    > 1 frame at 1x-4x. TARGET: no dropped frames at 1x scroll on the
    reference machine; per-scale render latency table extends the existing
    2-84 ms single-page row to the two-page path (≤2.2x single-page).
  - **R6 48 h soak + interaction latency.** OWNED ELSEWHERE (feat/soak-48h
    lane; re-soak verdict = A3). Plan contribution: fold R3/R4 harness
    numbers into the soak's interaction-latency record once (one session,
    not two).
  - **R7 GPU-accelerated rendering paths.** MEASURE: same per-scale suite
    with QRhi/backend variants (software raster vs D3D11 via QQuick/
    QPdfView's RHI backend where available; record `QSG_RHI_BACKEND` and
    composition mode). FIX: only if software raster dominates a REAL frame
    budget (unlikely at these sizes — 16x 84 ms is the worst case).
    TARGET: record; adopt GPU path only where it beats software by >2x at
    interactive scales; otherwise document software-raster as the shipped
    path (honest disposition, no speculative churn).
  - **R8 multi-monitor / display-scale / thermal.** MEASURE: the R5 harness
    on (a) 4K + 1080p mixed monitors, (b) DPI 100%/150%/200%, (c) after a
    30 min load soak (thermal). FIX: none expected; record device-pixel-ratio
    handling correctness (anchors/badges at 200% — cross-check E-3's law
    read at HiDPI). TARGET: document per-DPI frame table; any DPI-dependent
    defect becomes its own finding row, not a perf regression.
  - **F5 guard scoping (attached residual).** Exact change (if the
    coordinator accepts a guard broadening): hoist `checkedRenderSize`
    (PdfViewerWidget.cpp:1223) into a shared core/render helper and apply it
    at the ENGINE-side render entry the perf driver used, with the budget
    (64 Mpx) as a constant both paths share; the driver's 16x expectation
    then gets the documented bounded-render clamp instead of an unguarded
    success. Pin: harness render at 16x returns the clamped size (or an
    explicit refusal) — fails pre-hoist. NC: revert hoist → unclamped again.
    Risk M (siblings: every PdfiumBackend render call site — redaction
    proof's text extraction is NOT a render, must not be clamped; thumbnail
    rendering if any; TestPerformance expectations).
  - **Redact-apply re-probe (attached residual).** MEASURE: 20×3 blocks with
    per-iteration sub-timers (apply-marks vs save-commit) added to the perf
    driver, quiet gate re-run at two different times of day; record bimodality
    hypothesis (AV scan / Defender sync vs GC-ish effects). FIX: none until
    the distribution is understood. TARGET: a quoteable median with a
    disclosed bimodal split, replacing the current suspect 77 ms.
- **Pin design:** Perf pins are harness-internal: each new measurement lands
  in run_perf_suite.ps1's JSON schema with a named metric (warm-start-wall,
  click-latency-p95, cancel-ack-ms, cancel-stop-ms, frame-gap-count,
  gpu-scale-table, redact-apply-apply-sub / -save-sub) so the NEXT baseline
  compares apples-to-apples; the F5 hoist pin (above) is the only failing-
  first test in this plan.
- **Negative control design:** F5 hoist: revert → clamp pin fails. Harness
  additions: each new metric must reproduce the BASELINE's existing numbers
  within noise on the overlap (startup, redact-apply) — a new driver that
  shifts the old medians >10% is the control failing (methodology drift
  detector).
- **Risk class:** **M** for the harness/R-work (measurement-only, but shared
  machine discipline: wait_quiet gate is load-sensitive — the baseline's own
  §7.2 shows mid-run bursts); **M** for the F5 hoist (render call sites);
  **S** for the redact re-probe. Siblings to audit (F5 hoist): PdfiumBackend
  render entries, PdfViewerWidget's existing guard tests, TestPerformance,
  the 16x driver expectations on feat/sweep-w3-perf.
- **Suggested lane brief:**
  1. Merge (or vendor) tools/perf from feat/sweep-w3-perf onto the work tip;
     re-run the baseline block to reproduce §7.1 medians (drift control).
  2. Add sub-timer redact-apply re-probe (S) — quote-or-retire the 77 ms.
  3. R3/R4 windowed harness (cancel ack/stop + click latency) — targets
     100 ms / 1 s.
  4. R1 warm/cold split + qrc timing; R5/R7/R8 frame+DPI+GPU tables (one
     windowed session each); R2 office corpus run.
  5. F5 guard hoist ONLY with coordinator sign-off (behavior change): pin +
     NC as above. Publish PERF-BASELINE addendum; cross-ref §2.6 row 14.

---

## Plan 15 — Soak/consolidation residuals (§2.6 row 15; §7 items 4, 15; A3/A5 slots)

- **Residual:** First-soak candidate has ~4 h endurance evidence only
  (Windows-Update restart at 4h00m21s); re-soak (candidate b17106a, exe
  509da2c8…) verdict pending; origin/feat/parity-glm behind local tip (push
  step); main-merge resolution classes R2/R3/R4 need release-owner sign-off;
  local-only branch set out of scope.
- **Verification at tip:** STANDS. `git rev-list --count
  origin/feat/parity-glm..feat/parity-glm` = **5** today (was 7 at
  CONSOLIDATION-PLAN time — the origin ref moved; the push step is still
  owed). Re-soak window ends 2026-09-22T20:49:07+03 (TODAY) — verdict still
  unrecorded at lane start (A3 unfilled; RESOAK start doc at
  feat/soak-48h-resume 0fad38c0). feat/soak-48h NOT merged (mainline still
  lacks the soak evidence docs — consolidation runbook §5 covers them).
- **Root cause:** Sequencing: the 48 h window physically cannot have closed
  before the report; the push step is deliberately gated behind the
  consolidation runbook's zero-loss proofs; R2/R3/R4 are judgment calls
  reserved for a human pass (52 deleted-paths dispositions, version adoption,
  workflow union — rehearsed mechanically, policy undecided).
- **Exact change:** Execution per CONSOLIDATION-PLAN §5/§9 — no new design
  needed, this plan pins the ORDER and the gates:
  1. Read the re-soak verdict (A3) when the window closes; classify per the
     pre-declarations (SOAK END line, RESTART DETECTED survivals,
     failing-test buckets). Gate: candidate-attributable failure →
     consolidation pauses for the specific fix; TestSanitization
     AssertMutable ×2 → Plan 6's loop-repro FIRST.
  2. Push step (§5.0 step 1): push local parity-glm to origin (5 commits
     today, recount at execution time). Never force.
  3. Merge sequence per §5 with the rehearsed resolution classes; R2/R3/R4
     human pass (~30 min over the 130-file conflict list) with the
     release-owner BEFORE the main merge, not after.
  4. A5: record the final tip T, folded-ref proofs, archive bundle hash.
- **Pin design:** The consolidation plan's own proofs are the pins:
  `rev-list --count <b> ^HEAD` == 0 for every merge input at T (rehearsal
  idiom, §10); archive bundle SHA-256 recorded in A5; the deploy validator +
  dependency closure re-run at T (devops §1 gates) as the post-consolidation
  smoke.
- **Negative control design:** Any merge input left unmerged → its
  rev-list count > 0 → the proof fails (structural inversion); a forced
  history (forbidden) would break the rehearsed commit-SHA ledger — the
  rehearsal evidence (3b19bdef…a82036df on the scratch worktree) is the
  reference to diff against.
- **Risk class:** **L** (schedule + judgment, zero code). Siblings: every
  unmerged lane branch (Plans 3, 6, 9 feed it), the live worktrees pinned to
  local-only branches (pdf-keyC soak, pdf-sec UI — CONSOLIDATION-PLAN §9.7:
  do NOT fold while pinned), and the Windows-Update/restart regime for any
  future soak.
- **Suggested lane brief:**
  1. When the window closes: read the re-soak loop output; record A3 verdict
     with the pre-declared buckets; escalate any candidate-attributable miss.
  2. Recount origin lag; push parity-glm (no force).
  3. Execute §5 merges with release-owner sign-off on R2/R3/R4 FIRST.
  4. Record A5: final tip T + zero-loss proofs + bundle hash + validator
     re-run evidence. Cross-ref §2.6 row 15; CONSOLIDATION-PLAN §5/§9/§10.

---

## Verification summary table (all citations at 26c9a415)

| §2.6 row | Residual (short) | Verdict at tip | Key evidence |
|---|---|---|---|
| 1 | W1-05 machine-policy enforcement | STANDS | PolicyController.cpp:62-116 (no ACL/origin check); disclosure :166-183 |
| 2 | F2 embed-branch scoping | STANDS | SignatureManager.cpp:222-296 (no seam), :1393-1448 (embed branch) |
| 3 | L7/R14 F1 attribution | SPLIT: core RESOLVED-AT-TIP (71891494); successor headroom STANDS | RedactionProof.cpp:739-764 (raw /Rect) vs :798 (3×fs blanket); ri-fix e620757b unmerged |
| 4 | signatureFieldAnchors rotation | STANDS (E-3 consumer fix NOT at tip) | SignatureManager.cpp:2105-2128; feat/emergence-fixes unmerged |
| 5 | Sweep-legacy bundle (7 items) | ALL 7 STAND | PoDoFoBackend.cpp:5088, :2784-2790, :4439-4446; PdfPageOps.cpp:153; ConversionManager.cpp:449-456, :468-470; PageSpaceTransform.h:54-59 |
| 6 | EM/San-UAF/ri-fix review | STANDS (+ fixes UNMERGED) | feat/emergence-fixes 645f4994..c1552c26, feat/runintersects-precision — none an ancestor of 26c9a415 |
| 7 | /NM dedup + XMP flake | BOTH STAND | PoDoFoBackend.cpp:4115 (create-only); TestRedactionProof.cpp:588 |
| 8 | W1-H1/H2/H3 | ALL 3 STAND (mechanisms code-confirmed for H2/H3) | SendForSigningController.cpp:179-201; BatchMode.cpp:1164-1166 |
| 9 | UX batch-B + F2b-D1 | RESOLVED-ON-LANE-BRANCH-UNMERGED | feat/sweep-w3-ux-resume 72069bd8 (mkpath); NO mkpath in BatchPreset.cpp at tip |
| 10 | Devops D1/D2/C1-C3/D3-D6/B1 | STANDS | no models bootstrap step; fuzz yml windows-latest + no cmake/ninja |
| 11 | W2 cleanup residuals | ALL 5 STAND | TestLaneScheduler.cpp:147-153; TestSignatureValidation.cpp:131-132; R14ProbeBatchSkip unregistered; TestEngineSave.cpp:226-239; SOAK-VERDICT §10.3 |
| 12 | Archaeologist dispositions | STANDS | AnnotationToolBar.{cpp,h} present; e5a5f012 marker; CMakeLists.txt:464-479 |
| 13 | Research tail + matrix CSV | STANDS | CSV line 302 = 28 fields (verified); docs/research/ corpus at tip |
| 14 | Perf R1-R8 + F5 + redact re-probe | STANDS | PdfViewerWidget.cpp:1213-1240 (guard view-scoped); tools/perf on feat/sweep-w3-perf only |
| 15 | Soak/consolidation | STANDS | origin 5 commits behind (recounted); re-soak verdict pending; feat/soak-48h unmerged |

## Method note

Every plan's root-cause section was verified against the tree at 26c9a415 by
this lane (grep/read; zero builds run — this is a docs-only lane; the pin
designs name the failing-first evidence for the EXECUTING lane to observe).
Source documents not at the tip were read via `git show` from their lane
branches (feat/sweep-w2c, feat/sweep-w3-perf, feat/sweep-w3-ux-resume,
feat/sweep-w3-devops, feat/sweep-w3-archaeo, feat/sweep-w3-research,
feat/soak-48h-resume, feat/consolidation-plan) — the same pattern the
consolidated-report lane documented in its §7.3. Branch-tip drift during this
lane's own session: the mainline itself advanced (ec9f16f6 → 26c9a415) during
setup; recorded above and in `.context/resplan-wip.md`. Docs-only discipline:
this lane's commits touch only docs/audit/ and this document. No push; no
reset/clean/force/gc/prune; no other worktree touched.
