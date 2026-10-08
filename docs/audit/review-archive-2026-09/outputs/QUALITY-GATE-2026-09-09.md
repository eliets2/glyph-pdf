# GlyphPDF independent quality gate — 9 September 2026

**Gate: FAIL. Do not treat this repair wave as accepted or ready for installation review.** Many repairs work, both pinned revisions compile, and the local Release suite passes 116/118 executables. Independent failure probes nevertheless reproduce source deletion, encrypted-save truncation, incorrect recovery saves, stale autosave writes, hidden mutations after failed commands, and new feature defects.

## Exact scope and evidence

- Published candidate: `origin/feat/parity-glm`, fetched and pinned at **e99c73365e14189fc6343da5836d84df24183152**. Delta from the last reviewed `b58b91054ca7a573a09c56d950588ac4f966df42`: 123 files; 16,062 insertions/722 deletions, including research documents.
- Additional committed local candidate: **a5840dcfc6b2716e5b11e626a46ae3ec0ba0eaeb**, 12 commits ahead of the fetched remote; 28 files, 2,526 insertions/86 deletions. Includes infrastructure repairs and measurement core/persistence. These changes are not published in the fetched remote.
- At the final ref check local HEAD was `f443f596ddf74885e104906e136d199ef81e6326`. Its sole additional change is the design-only send-for-signing research plan. The production code is identical to the tested local candidate. That new design document is not an implementation acceptance.
- Uncommitted `MeasureCore.h`, `AnnotationLayer.cpp/.h`, and untracked `MeasureMode.h` were excluded. The main checkout, shared implementation worktree, installed app, and repository ledger were not edited. No installer was run.
- The independent row guide was each candidate's `docs/audit/CURRENT-EVIDENCE-LEDGER-2026-09-05.md`, reconciled with the September 8 repair prompt and team reports. Research claims and implementer test logs were treated as context, not independent proof.

Immutable source snapshots, full logs, and probes are in [the gate workspace](<C:/Users/User/Documents/Codex/2026-09-05/read-c-users-user-projects-pdf/work/gate-2026-09-09>). `manifest.json` records source archive SHA-256 hashes. The companion `quality-gate-evidence-2026-09-09.zip` contains the review probes, fixture evidence, test logs and source anchors; `QUALITY-GATE-VERIFICATION-2026-09-09.json` inventories and hashes its entries.

Three agents began independent source-review lanes. Their turns stopped at an account usage limit before final reports. Their notes were leads, not acceptance. The root reviewer corrected probe-only setup errors, built both candidates, executed the saved probes, and validated the findings below. This is a review of the repair/delta contracts and their callers, not a claim that every source line or every research proposal has been certified.

## Build and test gate

Windows/MSYS2 UCRT64, GCC 16.1.0, Qt 6.11.0; fresh Ninja build directories. The three existing vendor trees were copied into each immutable snapshot, as documented in commit `05a3336`. This validates the builds with those vendors, not a clean download/bootstrap.

| Candidate | Independent result |
|---|---|
| Published `e99c733`, Debug with `-g0` | Configure and full build pass, **677/677 build steps**. First serial CTest run **106/116 passed**, 230.83 s. |
| Published targeted rerun | Six Windows profile/loopback-sensitive failures pass with normal Windows access. Of eight rerun executables, **6/8 pass**; BatchOpsCoverage and StatusBarSlim remain failing. The two Djot source-path failures were not silently staged away. This is not a clean 116/116 result. |
| Local `a5840dc`, Release | `GLYPHPDF_RELEASE_BUILD=ON`, `GLYPHPDF_ENABLE_LTO=ON`, compile commands enabled; all four required feature flags TRUE. Full build **686/686**, serial CTest **116/118 pass**, 91.57 s. BatchOpsCoverage and StatusBarSlim fail. |
| Release macro/version | Real compile evidence: **180 production + 250 test compilations**, forbidden production macro absent, both app and engine targets found. EXE file/product versions both **1.3.2.3**. |

Independent configure/build/test commands are preserved in `work/gate-2026-09-09/verify.py`; run with `remote build`, `remote test`, `remote retry`, or `local build` / `local test`. It records individual Qt test output and restores the generated CTest file afterward. No source test was modified to force a pass.

Two remaining suite failures:

1. **BatchOpsCoverage:** watermark output saves correctly, but completed-result accounting arrives after the completion summary. A separate three-run controller probe confirms the stale UI summary after waiting for `batchFinished` and then allowing another 150 ms of event processing. One published rerun also hit this accounting timing in a PDF/A case. See G12.
2. **StatusBarSlim:** `loadThemeSheet()` assumes the build directory is directly under the source directory (`../resources`). Both genuine out-of-source builds fail that assumption. Local Q02 fixes Djot similarly located resources, but misses this test. See G20.

Skips remain visible in the detailed logs: RapidOCR model inference, veraPDF integration/conformance, real Ollama ping, a signed-redaction fixture, a source-definition-dependent redaction check, and the gated revoked-certificate fixture. The suite does not independently establish real model readiness, full PDF/A conformance, or the broader signing retry contract. A full configure/link with Tesseract absent was not performed; the implementation's isolated translation-unit compilation is narrower evidence.

## Blocking and actionable findings

All source paths below are relative to the **pinned snapshot**, not the moving shared worktree. Published findings also apply to unchanged corresponding code in the local candidate; the direct engine/controller reproductions were linked against the published fresh build. Local measurement persistence was reproduced against the fresh local Release libraries.

### G01 — P1 — EC01 still truncates encrypted same-path saves

**Source:** `src/engines/podofo/PoDoFoBackend.cpp:242–263`.

The encrypted branch deliberately bypasses the new transaction. Load an ordinary two-page PDF, call `encryptDocument()` with a synthetic owner password, then `saveDocument()` to the same path: encryption setup succeeds, Save returns false, and the valid **15,090-byte source becomes 0 bytes**. Ordinary same-path Save and Rotate controls now succeed and preserve readable text.

**Fix:** cover newly encrypted/lazy-loaded documents with a safe candidate boundary, retaining the required encryption/password semantics. A documented exception does not satisfy the no-data-loss contract. Reopen the encrypted candidate with the appropriate credentials before replacement; preserve source and prior destination on every failure. Keep signed incremental updates a separately tested contract.

**Evidence:** `engine/probe.log`, `engine/engine_probe.cpp`, `engine/fixtures/save-2.pdf.before` and the zero-byte result.

### G02 — P1 — INF01's fixed temporary name can delete the input

**Source:** `tools/clean_scanned_pdf.py:257–324` (candidate construction and exception cleanup).

Use `result.pdf.cleaning-tmp.pdf` as input and `result.pdf` as output. They pass the input/output alias check. An injected processing failure closes the input and then unlinks it because it is also the fixed candidate path. A pre-existing unrelated candidate is likewise deleted. Processing also overwrites an existing `page_002_cleaned.png` before failure, contradicting the claimed artifact preservation. Missing/corrupt/same-output input controls do preserve the old destination.

**Fix:** create a uniquely owned temporary file; never unlink a path the run did not create. Stage PNGs separately and commit them according to an explicit success policy. Validate identity against every output/staging path and preserve old artifact bytes on failure.

**Evidence:** actual PyMuPDF/OpenCV/Pillow run, `infra/cleanup-results.json`; before/after hashes and fixture trees. This is not merely an AST ordering witness.

### G03 — P1 — new Bates batching deletes destinations and later inputs

**Source:** `src/shell/controllers/PagesController.cpp:130–157`.

`QFile::remove(output)` precedes opening/validating the input. A nonexistent input deletes its existing `_bated.pdf` destination. With inputs `first.pdf` and `first_bated.pdf`, processing the first overwrites the second original. The second output then contains FIRST_SOURCE and two Bates stamps, while SECOND_SOURCE_MUST_SURVIVE is lost.

**Fix:** preflight the whole input/output identity set, including Windows aliases and collisions with later inputs. Reserve distinct outputs and process through validated candidates with checked replacement. Do not destroy an existing output before work succeeds.

**Evidence:** real offscreen Bates dialog/controller, `features/bates-probe.log` and the saved fixture PDFs.

### G04 — P1 — EC02 checks autosave path but not resident document identity

**Source:** `src/engines/AutosaveManager.cpp:92,164–178`; `src/engines/PdfEditorEngine.cpp:233–250`.

Queue autosave for dirty A behind a deterministic worker barrier; switch to B, replace/reopen A, then release the worker. `saveDocumentIfCurrent(expectedPath, ...)` accepts the new A because the string matches. The old recovery file is overwritten with NEW_INCARNATION_A. Completion is emitted even though generation validation prevents updating the new session timestamp. Rejecting the timestamp after the write is too late.

**Fix:** validate an engine-owned load identity and revision under the same mutex as serialization, or serialize an immutable captured snapshot. Carry the identity through commit as well as completion. Reject stale A→B→A and same-path reloads before touching recovery output.

**Evidence:** `EC02_ABA_*` in `engine/probe.log`; barrier probe and old/new recovery markers.

### G05 — P1 — recovered document Save writes the recovery file and accepts close

**Source:** `src/GpMainWindow.cpp:556–590`; `src/shell/controllers/HomeController.cpp:134–222` and the close handler.

Recovery sets the session path to the original PDF while the viewer/editor hold `<original>.autosave.pdf`. Save uses `viewer->filePath()`, returns Saved, and leaves the original byte-identical with ORIGINAL_BEFORE_RECOVERY. The session remains dirty, yet the close handler accepts Save and closes. The recovered content remains in the side file; the original has not been updated as the successful Save implies.

**Fix:** define one recovered-document identity with distinct input and intended save destination. A successful Save must commit the recovered content to the selected/original destination, synchronize viewer/editor/session, and clear dirty only for that committed revision. Close must verify that contract.

**Evidence:** `RECOVERY_SAVE_OUTCOME`, `RECOVERY_CLOSE_ACCEPTED` and original text in `architecture/probe.log`.

### G06 — P1 — a rejected mutation survives in memory and saves later

**Source:** `src/engines/podofo/PoDoFoBackend.cpp:350–378,1185–1214`; `src/commands/CropPageCommand.h`.

A real crop with commit failure leaves the disk unchanged, drops the command from history, leaves dirty false, and reports failure. Removing the fault and calling ordinary Save then persists that supposedly rejected crop. The transaction protects disk bytes but does not roll back the resident document, which has already been mutated/reseated.

**Fix:** keep the pre-mutation resident state until commit succeeds, or restore it on failure. Make this a common mutation transaction rule, not a crop-specific reload workaround. Verify rejected crop/delete/replace operations cannot leak into a later Save.

**Evidence:** `EC05_FAILED_PUSH_COUNT 0`, `DIRTY false`, `DISK_UNCHANGED true`, followed by changed geometry in `EC05_LATER_SAVE`.

### G07 — P2 — EC05 crop undo restores the wrong geometry

**Source:** `src/engines/podofo/PoDoFoBackend.cpp:1123–1183`.

The snapshot treats PDF `[x0,y0,x1,y1]` as Qt `(x,y,width,height)`. A real crop box `(10,20,500,700)` becomes `(10,20,510,720)` after Undo. An inherited CropBox restores the whole MediaBox `(0,0,612,792)` instead of the inherited effective box.

**Fix:** resolve inherited effective geometry, convert coordinates correctly, and preserve explicit/absent/inherited semantics as required. Test nonzero origins, inherited boxes and repeated undo/redo with saved/reopened geometry.

### G08 — P2 — V02/EC03 still advance history after failed undo

**Source:** `src/commands/EditFormFieldCommand.h` undo failure branch; `DeleteImageCommand.h` / `ReplaceImageCommand.h` undo branches.

A real form edit to EDITED followed by an injected failing restore leaves EDITED on disk, but stack index becomes 0, `isClean()` true, `canUndo()` false, `canRedo()` true. Image restore failure now correctly avoids deleting the following page, but still traverses history into a false clean state. Emitting `mutationFailed` reports the failure; it does not repair history. Image replacement also still consists of separately committed insertion/deletion steps.

**Fix:** implement a checked history traversal/transaction boundary that changes the history position only after successful restoration, and keep replacement atomic. Preserve the original failure tests while asserting index, retryability, saved artifact identity and clean state.

**Evidence:** real FormManager commit-fault probe and narrowly labeled image mocks in `engine/probe.log`. The image mock proves calls/history, not PDF geometry.

### G09 — P3 — EC06's new destructor can wait for its own worker

**Source:** `src/engines/RenderCache.cpp:36–56,317–319`.

Draining superseded futures now works. However, a worker acquires a strong `self`. Drop the external owner while it renders; when the worker releases the last owner, `~RenderCache()` runs on that worker and waits for its own not-yet-finished future. A child probe releases rendering, but the global pool cannot drain within two seconds. Only that synthetic child is then exited to avoid hanging the review process.

**Fix:** arrange ownership/shutdown so worker completion never invokes a destructor that joins that worker. Preserve draining of all outstanding work. No production `prefetchViewport` caller was found; this is a public API/library lifetime defect, not a demonstrated current UI freeze.

### G10 — P2 — V05 allows stale OCR after A→B→A reopen

**Source:** `src/shell/controllers/EditController.cpp:553–581,702–872`; `DocumentSession::beginDocument`.

Mutation revision catches an edit within a session, but reopening changes document generation without changing that revision. The real application probe advances generation 5→7 while revision remains 2; the old review is still exportable. The mutation control correctly becomes non-exportable.

**Fix:** capture and validate document generation/load identity plus mutation revision at dispatch, completion and export. Bind the review image, words and output to the same identity; do not use path/page count as a substitute.

### G11 — P2 — D06 still confuses model presence and disable ownership

**Source:** `src/core/Capability.cpp:176–201` and `probeRapidModelsIn`.

Three arbitrary nonempty files report Available while actual RapidOCR initialization fails with a protobuf error. Separately, start with a widget disabled by another owner, apply an unavailable capability, then make the capability available: the registry re-enables the widget. The new test checks a narrower foreign-disable case and misses this transition.

**Fix:** distinguish installed files from initialized/runtime-ready models and share actual model resolution with the engine. Claim disable ownership only when the registry itself changes an enabled widget to disabled; preserve a foreign disable across unavailable→available transitions.

**Evidence:** actual invalid synthetic model initialization and widget transition in `architecture/probe.log`.

### G12 — P2 — batch completion summary precedes result accounting

**Source:** `src/modes/BatchMode.cpp:1304–1316,1528–1549`.

Only merge drains pending results before the completion summary. Three independent watermark runs emit `batchFinished` with success=0/failure=0/remaining=1; later accounting reaches success=1, but the visible summary stays **BATCH COMPLETE — 0 of 0 succeeded, 1 not processed**. The output really contains the original text and watermark. This explains the repeatable suite failure without falsely alleging that watermarking itself failed.

**Fix:** reconcile every completed result exactly once for all batch modes before summary/finished. Track accounted indices so late queued callbacks cannot double count. Tests must await the controller's completion contract and check the final summary and saved output.

**Evidence:** `features/batch-counts.log`, three saved artifacts, both full suite logs and published targeted rerun.

### G13 — P2 — Page Labels writer still uses unsafe same-file Save

**Source:** `src/core/PageLabels.cpp:159–172`; caller `src/modes/PagesMode.cpp:1629`.

The new path helper loads and saves the same file directly. A normal two-page content-bearing fixture shrinks from **20,667 bytes to 0**, returns false, and cannot reopen. The Pages UI invokes it on its SafeSave candidate, so that caller preserves the user's original but fails to label such documents. Direct API callers can lose their supplied file. Blank-page-only tests miss lazy content streams.

**Fix:** serialize to a distinct validated candidate and commit, or apply the in-memory tree mutation within an existing safe transaction without re-saving a lazy-loaded file over itself. Cover real text/image content and the actual Pages UI path.

### G14 — P2 — reopening sidecar-only edits loses unsaved-state tracking

**Source:** `src/GpMainWindow.cpp:635–707`, `PdfViewerWidget` load/annotation sidecar flow.

Add a comment to A, then open B: A's sidecar flushes correctly and B has no leaked annotation. However, no Save/Discard/Cancel guard runs, and reopening A restores its unembedded comment while marking the session clean. The annotation survives in its sidecar, but the original PDF still lacks it and normal close does not represent it as unsaved PDF work.

**Fix:** distinguish sidecar persistence from committing annotations into the PDF; preserve dirty/pending-embed state across reopen, and use an explicit checked transition policy when leaving dirty work.

### G15 — P2 — Bates batching leaves the editor on another document

**Source:** `src/shell/controllers/PagesController.cpp:144–171`.

The batch loads each output into the shared interactive editor and never restores the active document. After the batch, viewer/session still point at `active.pdf` but rotating that active file returns false. This reopens ARC05 through a new caller.

**Fix:** use a per-file batch editor, as other batch operations already do, or restore one coherent identity on every exit. Test a normal interactive edit immediately after successful, partial and failed Bates batches.

### G16 — P2 — local MSI pipeline passes an absolute path that deploy appends again

**Local source:** `packaging/build-msi.ps1:266`; `packaging/deploy.ps1:17–22`.

The parent passes an absolute `-BuildDir`. PowerShell binds that abbreviation to `BuildDirName`, and the child joins it to ProjectRoot, producing `C:\repo\C:\repo\build-rel`. The exact child script rejects a controlled existing build fixture at its first read-only validation. No deployment/signing occurred.

**Fix:** accept an actual BuildDir path and distinguish rooted from relative values. Validate cached `CMAKE_HOME_DIRECTORY` and LTO as well: the validator currently accepts a conflicting CMake source path or LTO=OFF when the stamp claims the right tree. The Debug and release-feature-disabled negative controls correctly fail.

**Evidence:** `infra/controls/deploy-absolute-build-path.log` and validator control JSON/logs.

### G17 — P2 — local release define gate accepts incomplete target evidence

**Local source:** `packaging/check-release-defines.ps1:85`.

The `-and` condition rejects only when both required targets are absent. A compile database containing just the application, or just the engine, passes. Normal real release evidence passes and production GLYPH_TESTING/missing/empty evidence controls correctly fail.

**Fix:** require both target groups and fail when any required evidence is absent. Keep the negative macro fixture and intentional test-target exception.

### G18 — P2 — local vendor bootstrap hashes the wrong PDFium object

**Local source:** `scripts/bootstrap-vendor-deps.sh:35–36,68–74`.

The expected hash is the **DLL** hash (independently matches the staged DLL exactly), but `fetch()` compares it with the downloaded `.tgz` before extraction. Thus the new fresh-install PDFium path cannot validate the intended archive. The pre-existing-tree `check` mode cannot exercise this failure. CI correctly validates the extracted DLL.

**Fix:** pin the archive hash separately or extract then validate the DLL before staging. Exercise the missing-tree path in an isolated directory; do not present an existing-tree presence check as bootstrap acceptance.

### G19 — P2 — local oracle gate passes when every driver invocation fails

**Local source:** `fuzz/run_oracles.sh:91–95,119–122`.

The runner checks output existence but ignores driver exit status and reuses fixed output names. With six prior clean PDFs and a synthetic driver that always exits 9, the unmodified runner reports all CLEAN and exits **0**. The real oracle scripts actually run; they simply inspect stale PDFs.

**Fix:** require each driver command to succeed and require a fresh, owned per-run output/report directory. Fail missing/invalid reports as already implemented. Validate the fresh runner dependencies separately; a hosted Linux fuzz campaign was not run here.

### G20 — P2 — new theme regression test fails genuine out-of-source builds

**Source:** `tests/TestStatusBarSlim.cpp:54–75`.

The theme sheet is resolved using `applicationDirPath()/../resources`, so the test fails in both independent source/build layouts. Local Djot source-root injection works (both Djot suites pass there) but does not fix this remaining assumption.

**Fix:** inject the real source resource directory or embed the needed test resources. Run the test with source and build directories that are siblings, not just `<source>/build`.

### G21 — P2 — local measurement annotations persist zero-area rectangles

**Local source:** `src/engines/podofo/PoDoFoBackend.cpp:3373–3380`.

Unioning zero-size `QRectF(point, point)` values returns the final point, not the point set's bounding box. A 72×72 perimeter writes `/Rect [10 710 10 710]`; a distance writes `[82 692 82 692]`. The Release engine reports successful embedding and its own readback confirms 0×0 rectangles despite full point geometry. The new tests verify points and schema but not usable bounds.

**Fix:** compute explicit min/max extents and account for line thickness/appearance needs. Verify actual saved bounds and rendering/hit-test behavior with another reader. The same expression predates measurements for ink, so assess that shared branch too.

### G22 — P2 — local perimeter label and persisted geometry disagree

**Local source:** `src/engines/podofo/PoDoFoBackend.cpp:3470–3478`; measurement perimeter calculations.

The tool's closed-perimeter calculation labels a square **144 mm**, but persists four vertices as an open `/PolyLine` without repeating the first point. That encoded path is **108 mm** at the same scale. Own-reader closed-perimeter recomputation mirrors the assumption instead of detecting it.

**Fix:** define whether the tool measures an open polyline or a closed boundary and keep the displayed value, stored vertices, appearance and reader interpretation consistent. Verify length from the serialized path, not only GlyphPDF's helper.

### G23 — P2 — local calibration parser accepts invalid units as real calibration

**Local source:** `src/core/MeasureCore.h:199–252`, especially `.value_or(Unit::Pt)`.

`1 typo = 1 ft` and `1 in = 1 typo` both return calibrated scales; an unknown unit silently becomes pt. `1/4 999 in = 1 ft` is also accepted. This can produce plausible but incorrect measurements from malformed calibration input.

**Fix:** reject unknown unit tokens and trailing/unconsumed quantity text; distinguish omitted units with explicit defaults from invalid named units. Validate calibration before exposing a calibrated result.

## Independent ledger dispositions

“Verified” below is deliberately limited to the named contract and fixture evidence. It does not promote every historical row sharing a feature name.

| Finding/package | Independent disposition at this gate |
|---|---|
| EC01 | **Partial/rejected overall**: ordinary same-file save/rotate and failure preservation repaired; encrypted path G01 and resident rollback G06 block closure. |
| EC02 | **Rejected**: A→B guard insufficient; G04 reproduces A→B→A write. |
| EC03 | **Partial**: failed insertion no longer deletes following page; history/atomic restoration still unaccepted, G08. |
| EC04 | **Verified, Windows default-store scope**: write/read, second instance, separate process, no cleartext bytes, and corruption rejection reproduced. Not a portability/security proof for other store configurations. |
| EC05 | **Rejected**: offset/inherited geometry G07 and rejected-mutation persistence G06. |
| EC06 | **Partial**: superseded-worker clear drains correctly; final-owner self-join G09. |
| ARC01 | **Verified, ordinary Open A→B history clearing**: old Undo does not rotate B. Broader identity closure remains blocked. |
| ARC02 | **Verified, A/B sidecar isolation and pending old-path flush**: real comment stays with A. Pending PDF dirty state is G14. |
| ARC03/ARC04 | **Partial**: normal failed-save close guard and normal dirty signaling pass; recovery Save/close G05 and sidecar-reopen state G14 remain. |
| ARC05 | **Partial**: fresh Open then Rotate succeeds; Bates and recovery break shared identity, G15/G05. |
| ARC06 | **Verified, wiring**: active PDF/A panel receives document and reports missing validator instead of No Document. Conformance remains unverified. |
| ARC07 | **Verified for the explicit reviewed mutation list**: TestReadOnlyGate 5/5 and disabled Rotate control. The ledger itself excludes in-place OCR acceptance; do not claim universal read-only enforcement. |
| INF01 | **Partial/rejected overall**: initial input checks fixed; G02 still destroys owned-by-user paths/artifacts. |
| INF02 | **Local partial/rejected**: dedicated Release build works; packaging handoff/validator gaps G16. Remote does not contain this repair. |
| INF03 | **Local source/version evidence accepted; packaging not accepted**: CMake version wiring and actual EXE 1.3.2.3 confirmed. No MSI/portable artifact built. |
| INF04 | **Local workflow source repaired**: `feat/**` included; not independently verified by a hosted run at this unpublished revision. |
| INF05 | **Local partial**: real compile evidence and macro-negative controls work; required-target completeness G17. |
| INF06 | **Local partial**: loud compiler/executable/JSON checks improve source; stale driver output G19 defeats the oracle runner. No hosted fuzz acceptance. |
| Q02 | **Local partial**: Djot source-root tests pass in relocated build; bootstrap G18 and theme test G20 remain. |
| NCR-01 / N09 split residual | **Verified for case-only aliases, distinct output identities, and truthful partial-result dialog**: inspected case-folded preflight, TestPagesMode 31/31. |
| NCR-02 / D02 redaction ownership | **Verified for the reported one-shot and owned-state contracts**: source plus TestRedactTransaction 38/38, including run/run, run/start, start/run and owner-deletion cases. No sanitizer-wide claim. |
| V01 | **Verified for real-path form-import commit**: caller trace and TestPersistenceOutcomes 7/7 cover the corrected import boundary. Recovery Save is separate G05. |
| V02 | **Rejected**, G08. Reporting failure does not preserve history position. |
| V03 | **Verified for the reported column-collapse fixtures**: geometry-derived extraction/grid trace, TestConversionExtraction 13/13. Not universal PDF table reconstruction acceptance. |
| V04 | **Verified for ordinary short text edits and pinned insertion/reorder cases**: alignment trace, DiffEngine 29/29 and CompareEntry 26/26. |
| V05 | **Partial/rejected**, G10. Within-session mutation rejection passes; reopened identity fails. |
| V06 | **Verified for successful compound auto-detect undo/redo and partial placement accounting**: caller uses the seam, FormSafety 12/12. Failed history traversal remains governed by G08. |
| D01 recovered redaction banner | **Scoped source/fixture acceptance**: controller now uses the presenter's effective result; retry transaction/presenter cases execute. Do not confuse this with N06 signing retry. |
| D03 optional Tesseract | **Partial**: default preprocessing 25/25; full no-Tesseract configure/link still missing. Do not relabel isolated TU compilation as that full configuration. |
| D04 OCR overlay transform | **Verified for the reported letterboxing/pixel/hit-test cases**: source correction and OcrVerifyNavigation 16/16. |
| D06 | **Rejected overall**, G11; presence tests and limited disable controls do not close readiness/ownership. |
| N06 signing retry | Preserve earlier image-payload acceptance; **broader real PartialLtvMissing retry and failed-replacement preservation remain open**. Ledger still defers this scope. |
| N08 overlay fit | **Verified for tested Helvetica minimum-height/narrow-width contracts**: saved-artifact cases execute in RedactTransaction. Not a guarantee for arbitrary fonts/scripts. |
| N01–N05/N07/N10 and prior accepted D05/D07 scopes | No promotion beyond earlier scoped acceptance; relevant regression suites execute. Do not erase previous limitations. |
| New compression remove-unused/Flate downsampling | Targeted CompressJpegReencode 11/11, CompressStripSanitize 4/4 and CompressDialogHonesty 10/10 pass. Accept those fixture contracts; not all optimizations or document classes. |
| New PDF/A ICC/CIDSet/schema work | Structural/parser tests pass within the stated skips; **independent full veraPDF conformance remains unverified** here. |
| New Page Labels / Bates / batch completion | **Rejected**, G13/G03/G15/G12. |
| Local measurement core/persistence | **Rejected despite MeasureCore/MeasureRoundTrip passing**, G21–G23. Uncommitted interactive measurement UI was excluded. |

## Repair sequence and architecture conclusion

The recurring problem is incomplete enforcement of shared contracts. A safer engine save was added, but encrypted saving, Page Labels, Bates output replacement and resident rollback still diverge. Document generation was introduced, but autosave/review checks still rely on path or revision alone. Failure signals were added, but history position still advances. Tests often cover a helper or a favorable fixture while the next application boundary remains broken.

1. Fix G01–G06 first: common persistence, temporary ownership, batch input/output collisions, autosave identity, recovery destination and rollback of failed resident mutations.
2. Fix geometry/history and coherent identity: G07–G11, G14–G15. Keep sidecar durability distinct from embedding into the PDF.
3. Fix new feature contracts: G12–G13 and G21–G23. Reuse per-file batch editors and existing checked save boundaries; avoid another parallel framework.
4. Finish release/test reliability: G16–G20, full optional-dependency configuration, and the unclosed N06 signing retry boundary.
5. Run the targeted reproductions and a full suite on the final coherent revision. Supply source/commit identities, skips and fresh failing controls. Only then build/install the latest artifact for the separately requested live UI review.

Use `implemented-awaiting-review` for the new implementation pass. The shared repository ledger was left unchanged to avoid colliding with the ongoing implementation session; this independent report is the acceptance/rejection record to reconcile into it.
