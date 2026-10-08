# GlyphPDF — consolidated code review and implementation handoff

**The branch has made measurable progress, but it is not ready for acceptance or reinstallation.** The latest fixes pass several independent regressions. Older application and engine boundaries still permit source destruction, cross-document edits and annotations, and closing after a failed save. Repair these before expanding the feature list.

Reviewed remote revision: **`b58b91054ca7a573a09c56d950588ac4f966df42`**, fetched from `origin/feat/parity-glm`. New work was compared with **`0caa45e7d0751caaa36a54a085c41211a422f019`**; the broader review used that preserved baseline and checked whether relevant code changed in the new delta. The frozen main baseline is `703fa34ece32733ea3b2093da94fa1aed94e1afc`. These findings apply to those revisions, not to later commits or dirty working files.

All three agents resumed and saved their code reports on 8 September before hitting the usage limit again. The coordinator completed the report reconciliation, evidence checks and research mapping. No production source, repository ledger, branch, installed app or private document was changed.

## Independent findings that block acceptance

The team identified **21 new findings: eight P1, twelve P2 and one P3**, in addition to earlier unresolved V/D/N findings. The tables summarize each repair; the linked lane reports contain exact source locations, reproduction details, limits and acceptance instructions.

### Source preservation and document identity — implement first

| ID | Priority | Evidence and consequence | Required repair and acceptance |
|---|---|---|---|
| EC01 | P1 | Real engine same-file Save and Rotate each turn a valid 7,008-byte, two-page PDF into a zero-byte file and return failure. Distinct-path Save succeeds. | Reuse candidate → validate → checked commit at the shared engine save boundary. Preserve source and existing output on every failure; reopen and inspect page/content identity. Preserve the separate signed incremental-update contract. |
| EC02 | P1 | A barrier-controlled autosave captures A's recovery path, then saves B through the shared editor. `A.pdf.autosave.pdf` contains `AUTOSAVE_B`; B receives the completion timestamp. | Capture document identity and enforce it under the save lock, or serialize an owned snapshot. Reject stale completion. Test A→B→A, close, save-as and same-path reload. |
| ARC01 | P1 | Opening B retains A's undo history. After B's editor is initialized, undoing A's rotation changes B from 0° to 270°. | Clear history on a successful document switch; commands must reject stale identity. Failed/canceled open preserves A. Use one Save/Discard/Cancel policy before switching dirty documents. |
| ARC02 | P1 | A's comment appears in clean B and is written into B's sidecar by the existing debounce. | Stop or complete A's pending write against A; initialize B's annotations to an explicit empty/default state. Bind delayed work to identity and revision; test inside the debounce interval and inspect both files. |
| ARC03 | P1 | The real close prompt accepts Save; the actual save fails; dismissing the error still closes the window with dirty work. | Return `Saved` / `Canceled` / `Failed` from Save. Close or switch only after successful persistence or explicit Discard. Test canceled destination and failed commit. |
| ARC05 | P1 | Open initializes the viewer/session but leaves the editing backend empty. Immediate Rotate fails until another path primes the engine. | Publish one successfully loaded document across viewer, session and editor. Test fresh Open→Rotate/Save and A→B without another tool initializing the editor. |
| EC03 | P1 | Both image undo commands call Delete Page after backup insertion fails; a failed restore can delete the following page. Empty and over-limit backups reach this boundary. | Require a restorable snapshot before destructive editing. Make restore/replacement transactional and propagate failure to history ownership; check page content/count and undo index. A guard alone does not fix Qt advancing the history index. |
| INF01 | P1 | The standalone cleanup CLI unlinks output before validating input. Same input/output deletes the source; invalid input deletes an existing destination. | Open/validate first and use a sibling output candidate. Reject or safely support aliases. Preserve existing bytes on input, processing and final-write failures. This is a utility finding, separate from the app. |

Engine details: [engine and command review](C:/Users/User/Documents/Codex/2026-09-05/read-c-users-user-projects-pdf/outputs/TEAM-ENGINE-CODE-REVIEW-2026-09-07.md). Application details: [architecture review](C:/Users/User/Documents/Codex/2026-09-05/read-c-users-user-projects-pdf/outputs/TEAM-ARCHITECTURE-REVIEW-2026-09-07.md).

The engine-only truncation and the window's replacement failure are distinct observations. A viewer-held handle may change the failure mode; the tests do not establish that every installed-app save truncates a file. Both require safe failure behavior.

### Remaining integration and quality findings

| ID | Priority | Problem | Smallest appropriate repair |
|---|---|---|---|
| EC04 | P2 | Windows fallback secret storage hashes a newly DPAPI-wrapped blob as a key on each call. Its default path fails its own store/read round-trip; the injected-key control passes. | Use DPAPI protect/unprotect for persisted data, or persist one protected random master key. Test the real default path across instances/processes with synthetic secrets. The OS credential-vault path is separate. |
| EC05 | P2 | Crop Undo only reloads; the history index moves back without restoring the CropBox. | Snapshot the effective original geometry and restore it through the shared safe mutation boundary. Verify saved/reopened geometry and history on success/failure. |
| ARC04 | P2 | Annotation edits do not dirty the document; successful-save source marks only the undo stack clean. | Keep one session dirty policy that includes non-command edits, and reset it only after successful PDF persistence. Sidecar persistence is a separate state. |
| ARC06 | P2 | MainWindow never gives the PDF/A panel the open document; it displays “No document loaded.” | Bind the panel and its async result to the active document identity. Test entry and A→B through MainWindow, including validator-unavailable wording. |
| ARC07 | P2 | Read-only prevents editing tool selection but page controller actions still mutate the PDF. | Enforce editability at the shared mutation/command boundary and use it for action enablement. Test all direct controller mutations. |
| NCR-01 | P2 | Case-only Windows source aliases bypass split naming preflight; one part fails while another is saved and the caller reports completion. | Compare filesystem identities, choose distinct names before writes and report per-part failures. Source overwrite was **not** reproduced; the locked source remained unchanged. |
| NCR-02 | P2 | Redaction `run()` bypasses `start()`'s one-shot gate and strong local state ownership. Two runs plus a start produce three completions. | Share the one-shot gate and retain execution state locally in both entry points. Test all run/start orderings; no synchronous UAF crash was reproduced. |
| EC06 | P3 | Render-cache `clear()` waits only for the most recent prefetch while a superseded render continues. | Drain all owned work before renderer retirement, or keep the unused API disabled. No production prefetch caller or viewer crash was found. |
| INF02 | P2 | Local MSI packaging can reuse a cached Debug/feature-disabled build while claiming Release. | Always configure/validate a dedicated release build and require its exact revision/configuration/test evidence before staging. |
| INF03 | P2 | Portable packaging says 1.3.1 while MSI expects 1.3.2.3. | Pass one version to every artifact and reject a missing/mismatched output. |
| INF04 | P2 | CI push filters exclude `feat/parity-glm`. | Trigger the intended branch/PR checks and associate required results with the reviewed commit. Hosted branch protection was not inspected. |
| INF05 | P2 | The release test-macro guard reads a potentially absent configure log and can pass without evidence. | Inspect actual production target compile definitions; missing evidence and forbidden macros must fail. No shipped test bypass is alleged. |
| INF06 | P2 | Fuzz scripts hardcode a Windows checkout in an Ubuntu job; failures and missing binaries can become green results. | Resolve root from the script, require dependencies/executable/results, propagate failures and retain crash artifacts. |

Details: [new commits](C:/Users/User/Documents/Codex/2026-09-05/read-c-users-user-projects-pdf/outputs/TEAM-NEW-COMMITS-REVIEW-2026-09-07.md), [build and tooling](C:/Users/User/Documents/Codex/2026-09-05/read-c-users-user-projects-pdf/outputs/TEAM-INFRASTRUCTURE-REVIEW-2026-09-07.md).

## What the newest commits actually repair

Use these scoped dispositions when updating `docs/audit/CURRENT-EVIDENCE-LEDGER-2026-09-05.md`. “Verified” applies to the specified acceptance contract at `b58b910`, not to an entire feature or every row that shares its implementation.

| Finding/package | Independent disposition |
|---|---|
| N01 signature tab mapping | Verified: visible Initials/Upload now dispatch and gate correctly. |
| N02 alphabetic labels | Verified for groundwork: 27/28 → aa/bb, checked against a PDFium label oracle. Writer/UI completion remains outside scope. |
| N03 PDF/A versions | Verified for saved version mapping, including 3U → PDF 1.7. **Not PDF/A conformance acceptance.** |
| N04 redaction label propagation | Verified at the shared request conversion and real Redact-mode save boundary. Security's modal was source-traced. N08 fitting remains open. |
| N05 signature cache | Verified for A→B→A invalidation and reuse gating on the actual session path notification. |
| N06 retry appearance | Verified for captured image reuse across two engine attempts. Actual `PartialLtvMissing` controller retry and failed output replacement still need independent acceptance. |
| N07 exit redaction | Verified for disarming the tool while retaining marks; no native drag walkthrough claimed. |
| N09 split | Original no-output failure repaired with real saved/reopened output tests. Overall partial because NCR-01 remains. |
| N10 dormant tests | Verified registration and execution of all four formerly orphaned checks. Their passing model-file assertions do not prove inference readiness. |
| D02 redaction | Async state ownership repair verified within executed cases; synchronous API residual NCR-02 remains. Keep AI D02 separately named. |
| E-1 adjacent-text corruption | Verified for the two same-stream regressions: both fail against old libraries with `PUBLIC_KEEP_XEXX`, then pass at `b58b910` with saved-byte and independent text checks. |
| CMP-align | Verified for tested structural sequence alignment, including near twins and insertion. Per-page token rows still pair by index; this does not close V04/general compare parity. |

**Still open/partial:** N08 overlay glyphs exceed a small rectangle (714.836 vs allowed top 712); D01 Security still renders the obsolete partial-result banner after recovery; D06 model readiness/refresh/disable-state ownership remains incomplete. Retain earlier V01/V02 form import/failed-undo findings, V03 table structure, V04 rewritten-page classification, V05 OCR revision identity, V06 auto-detect history, D03 optional OCR build, and D04 OCR coordinate findings until independently repaired. Dirty/uncommitted candidate fixes were excluded.

Keep prior D05 safe sanitized-output replacement and D07 shared default-ON policy acceptances within their earlier scopes. The new N10 run now adds execution evidence for the registered D07 policy check. The prior [late review](C:/Users/User/Documents/Codex/2026-09-05/read-c-users-user-projects-pdf/outputs/LATEST-QUALITY-REVIEW-2026-09-07.md) and [package review](C:/Users/User/Documents/Codex/2026-09-05/read-c-users-user-projects-pdf/outputs/UPDATE-QUALITY-REVIEW-2026-09-07.md) retain the detailed historical row dispositions; unchanged rows are not silently promoted by this pass.

Ledger cleanup: merge duplicate U07 and duplicate D02-redaction rows; distinguish AI D02 from redaction D02; replace “this commit” placeholders; attach the independent E-1 evidence; correct stale target/suite counts. Implementation commits and green aggregate totals alone cannot assign verified status.

## Build, test and review coverage

- Full isolated build at `b58b910`: **639/639 steps, exit 0**, Debug with `-g0`, GCC 16.1.0/Qt 6.11.0 and the three documented vendor trees. Eleven warnings remain; no Release-package validation was performed.
- First full CTest run: **106/108 passed** in 96.54 seconds. The two Djot targets expected a different source/build relationship. Staging the unchanged pinned Djot files at their expected path produced a **2/2 targeted rerun** in 0.35 seconds. Every target has a passing result; this was not one clean 108/108 run.
- Detailed logs cover 26 suites. Captured skips include three model-dependent RapidOCR checks and one revoked-certificate test requiring the test macro. Do not treat skips as accepted inference/crypto behavior.
- Engine/application lane manifests together enumerate **all 295 tracked files under `src/` at the baseline**, with no gaps or overlap; their SHA-256 values match the source archive. Coverage includes focused traces, full reads of small files and structural scans. The manifests distinguish those depths. This is **not a line-by-line proof of every file**.
- The new-commit lane read all 34 delta files. The infrastructure lane separately traced build, packaging, workflow and utility boundaries. Vendored library implementation, complete dependency permutations, sanitizer campaigns, Office/veraPDF interoperability and the installed UI were not accepted by this review.

## GLM Flash implementation order

1. **Shared engine save:** EC01, then the utility's independent INF01. Reuse SafeSave, preserve signed-update semantics, and retain before/after hashes with failure injection.
2. **One document identity:** ARC05/ARC01/ARC02 plus EC02. Implement one existing session transition boundary; share identity/revision with commands, jobs, sidecar saves and task panels. Do not add separate registries for each consumer.
3. **Persistence and history outcomes:** ARC03/ARC04, EC03/EC05 and original V01/V02/V06. Preserve dirty work and history when writes fail. Validate the real controller, not just a mock manager.
4. **Finish current contracts:** ARC06/ARC07, EC04, NCR-01/NCR-02, D01/D06/N08 and the remaining older V/D queue. Gate acceptance on actual model initialization and saved artifacts where those are the contract.
5. **Release evidence:** INF02–INF06 and source-root/bootstrap documentation. A clean dedicated Release configuration and its required tests must precede any proposed installer.
6. **Then improve workflows:** use the [research reconciliation](C:/Users/User/Documents/Codex/2026-09-05/read-c-users-user-projects-pdf/outputs/RESEARCH-RECONCILIATION-2026-09-08.md). Address OCR correction, comment review, clear task preflight/results and compare navigation before new measurement, form scripting, agents or signing-package systems.

For each implementation package, state the exact input/output and failure contract, change the shared production boundary, supply one or more meaningful reproductions, and report the commit and remaining limits. Leave it `implemented-awaiting-review`. A separate review must reproduce the old failure or a meaningful negative control and inspect the repaired output before assigning scoped `verified`.

## Deliverables and evidence

The existing [GLM implementation/UI plan](C:/Users/User/Documents/Codex/2026-09-05/read-c-users-user-projects-pdf/outputs/GLM-FLASH-IMPLEMENTATION-AND-UI-PLAN-2026-09-05.md) now points here as its current checkpoint. Its older specifications remain useful; this report controls the current queue.

[Consolidated evidence archive](C:/Users/User/Documents/Codex/2026-09-05/read-c-users-user-projects-pdf/outputs/team-quality-evidence-2026-09-08.zip) contains lane reports, probes/runners, generated fixtures, logs, coverage, source/ledger metadata and a checksummed manifest. Large builds and vendor binaries are omitted; reproduce using the pinned source and documented dependencies. The final architecture sidecar was subsequently cleared by the same probe; its earlier persisted-note observation is captured in the runtime log, not claimed as the final sidecar's contents.

Reinstallation and the newest installed UI review remain the next phase **after code acceptance**. No live UI verdict is inferred from the old installed version or these offscreen integration probes.
