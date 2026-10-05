# LANE REPORT — gs-unlock (Ghostscript-assisted PDF unlock, §9.11 unlock bullet / §9.17)

- **Date:** 2026-10-04 (executed into 2026-10-05)
- **Lane:** GlyphPDF feature lane — Ghostscript-assisted unlock
- **Worktree:** `D:/pdf/pdf-gs-unlock` — branch `feat/gs-unlock`, base `93863d70` (worktree created by this lane; `git worktree add -b feat/gs-unlock … 93863d70`)
- **Contract:** PRD §9.11 unlock bullet ("remove owner-password restrictions and re-distill structurally broken protected files — Ghostscript-assisted; disclosed when external tooling is absent; never bypasses a document the user cannot legally open"), PRD §20 edge-case row (owner-password vs user-password behavior disclosed), SPEC-TRACEABILITY 9.17 (and the 9.11 row updated)
- **Status:** COMPLETE — engine + Security-section UX + pins landed; R7 evidence complete; pass-after ×3 SERIAL green; closing full serial gate green (see §4)
- **Commits:** `36cefbbb` feat(9.17) engine + tests + CMake; `97a691a7` feat(9.17) Security UX + wiring; `fafd8660` docs(9.17) AGPL notes + evidence; plus the closing docs commit (this report + SPEC-TRACEABILITY)

## 1. What was built

### Engine — `src/engines/GhostscriptRunner.{h,cpp}`

Two units behind one header, in the SevenZipLocator house shape:

- **`gp::GhostscriptLocator`** — the install-policy owner. Resolves
  `gswin64c.exe` (then `gswin32c.exe`; `gs` on other platforms) from the
  STANDARD install roots only (`%ProgramFiles%/gs/gs*/bin`,
  `%ProgramFiles(x86)%/gs/gs*/bin` — the AGPL installer's defaults), highest
  version wins (QVersionNumber parse of the `gs<version>` directory name;
  unversioned directories NEVER launch). There is deliberately **NO PATH leg**
  (the wave-2b F-02/CWE-427 reasoning: a `gs` from PATH is an unnamed,
  unplaced binary receiving the document AND the password). Absence returns
  empty and `absenceDisclosure()` names every searched location plus the
  licensing reason — the honest-disclosure contract.
- **`gp::GhostscriptRunner`** — the unlock transaction, shaped like
  `SafeSave::runExternalWriterCommit` but owned here so the tool's own output
  can be captured: `SafeSave::makeUniqueCandidate` reserves an owner-only
  candidate → `gswin64c -dSAFER -dBATCH -dNOPAUSE -sDEVICE=pdfwrite
  -sOutputFile=<candidate> <source>` (destination NEVER a process argument) →
  candidate validated (`%PDF` header) → `SafeSave::commitFileToDestination`
  with a destination-identity precondition (E-6) so an in-place unlock never
  erases a concurrent writer. The process body is the runBoundedProcess idiom
  (owned process, bounded wait, cancel/timeout KILLs it) with one addition:
  merged, DRAINED stdout+stderr (tail-kept) so failures QUOTE Ghostscript —
  and so a verbose run cannot deadlock on a full pipe.

### The two findings that shaped the semantics (all verified against AGPL Ghostscript 10.08.0 — logs in `docs/audit/evidence-gs-unlock/experiments/`)

1. **The blank-husk trap.** The C-based PDF interpreter (`pdfi`) refuses a
   user-password document by printing
   `**** This file requires a password for access.` +
   `No pages will be processed (FirstPage > LastPage).` and — **exits 0** —
   while pdfwrite still writes a BLANK single-page PDF. Committing that would
   silently replace the document with an empty husk. The runner therefore
   treats the `No pages will be processed` diagnostic as a FAILURE regardless
   of exit code, with a specific message when the password signature is
   present. Verified this gate does NOT fire for genuinely repairable files
   (broken-xref re-distills with `…errors that were repaired or ignored` +
   pages processed) — the §9.11 "re-distill broken protected files" mission
   survives. `-dPDFSTOPONERROR` was tested and REJECTED: it exits 1 on the
   repairable case too (kills the repair mission).
2. **The M-1 secret channel.** `-sPDFPassword=<pw>` on the command line works
   but violates M-1 (CWE-214) — rejected. A piped stdin prompt reply is NOT
   consumed by pdfi (no non-tty prompt) — insufficient alone. The shipped
   channel: the retry pass expands `-sPDFPassword="<password>"` (double-quoted
   token — verified with passwords containing spaces AND backslashes) from a
   `@response file` staged OWNER-ONLY via `SafeSave::makeUniqueCandidate` in
   the candidates dir and DELETED on every path the moment the run ends; argv
   carries only `@<path>`. The value is additionally piped to the child's
   stdin (channel closed) for prompt-reading interpreter builds. An embedded
   double quote cannot be expressed in the response file — such a password
   simply fails the retry honestly (documented).

### UX — `SecurityController::unlockPdf` (Security ▸ "Unlock PDF…")

Mirrors `HomeController::createEncryptedPackage`: resolve FIRST (absence ⇒
warning dialog naming the searched locations — no PATH fallback, AGPL
non-bundling stated), then an honest pre-flight dialog (what unlock does:
restrictions removed, content preserved visually, metadata re-stamped, NEW
output file — the source is never modified; user-password documents are not
bypassable — optional password field), destination pick (default
`<name>-unlocked.pdf`), then QtConcurrent + QFutureWatcher + QProgressDialog
with cooperative cancel (kills the Ghostscript process we own). Success
offers "Open the unlocked copy"; failures quote Ghostscript's own captured
output. Wiring: `ToolId::UnlockGs` (append-only ordinal rule), `unlockGs`
ribbon entry in the Security group, `EditPolicy` mutating classification,
alias table, `commands.json` entry (9-line surgical insert).

### Licensing — the prominent record

**Ghostscript is AGPL-3.0 and is NEVER vendored, committed, staged, or
shipped by this repo** — no byte of it enters `third_party/`. The feature is
resolution + disclosure of the USER'S OWN installation. The full contrast
with the LGPL 7-Zip bundle, the AGPL §13 analysis, and what an owner would
need to decide before any bundling are in
`docs/research/ghostscript-unlock-notes.md` §1. The dialog itself says
"Ghostscript is not bundled with GlyphPDF (licensing: AGPL)".

## 2. Test suite — `tests/TestGhostscriptUnlock.cpp` (10 test functions)

1. `absentRootYieldsEmptyAndHonestDisclosure` — empty root ⇒ empty result;
   the disclosure names every standard root and the console binary.
2. `presentRootResolvesHighestVersionConsoleBinary` — planted gs9.55.0 +
   gs10.08.0 trees ⇒ the higher one's binary path.
3. `unversionedOrHalfPlantedDirsNeverResolve` — binaries outside a parseable
   `gs<version>/bin/` layout never launch.
4. `builderPinsSaferAndLayout` — `-dSAFER` mandatory, `-dNOSAFER` never,
   batch/no-pause/pdfwrite/candidate-output shape, source last, no
   password switch possible.
5. `realArgvNeverCarriesTheSecretAndAlwaysCarriesSafer` — the test re-execs
   ITSELF as the tool (TestEncryptedPackageSafeWrite idiom; mode rides on the
   environment because the runner OWNS the argv) and records the real argv of
   three invocations (a clean pass-1, and both passes of a needs-password
   run while the runner holds a marked secret): the secret appears in NO
   argv, `-sPDFPassword` never appears on a command line, and EVERY
   invocation carries `-dSAFER`.
6. `endToEndOwnerPasswordOnlyUnlocks` — real Ghostscript (QSKIP-honest when
   absent): owner-password-only AES-256 fixture (built with the app's own
   `PdfEditorEngine` — the "qpdf in the tree" fixture discipline; qpdf's CLI
   built the experiment fixtures) ⇒ output with NO `/Encrypt`, strictly
   opens password-less via PoDoFo.
7. `endToEndUserPasswordFailsHonestlyWithoutIt` — user-password fixture ⇒
   `!ok` at the Tool stage, the error names the password requirement and
   quotes Ghostscript, the sentinel destination is byte-identical, and no
   candidate debris remains in the staging dir.
8. `endToEndUserPasswordUnlocksWithIt` — the same fixture with the correct
   password (off argv) unlocks; the success is the password retry pass.
9. `endToEndCorruptedProtectedFailsWithoutCorruptingOutput` — truncated
   protected file ⇒ either re-distilled into a real PDF or an honest failure
   with NO output file (never the blank husk).
10. `endToEndFailedInPlaceLeavesSourceByteIdentical` — destination == source
    with no password ⇒ failure and the source bytes unchanged (E-6).

## 3. R7 evidence (all runs on `build-rel`; files in `docs/audit/evidence-gs-unlock/`)

| Stage | Result |
|---|---|
| fail-before RED (`red-fail-before.txt`) | `GhostscriptRunner.cpp` swapped for the feature-ABSENT stub (API present, behavior absent; `BUILD_RC=0` — `build-red.log`), rebuild + run: **5 pins RED** (disclosure, highest-version resolution, unversioned-guard, `-dSAFER` builder pin, real-argv secret pin — the two argv-pin legs fail at fixture setup because `consoleBinaryName()` is empty: the pin cannot even be exercised, which is itself the absence), **5 end-to-end legs SKIP honestly** (locator empty ⇒ the honest-absence path the feature itself defines), 2 pass (the trivially-satisfied absence checks). No other suite touched. |
| NC once (`nc-no-pages-gate.txt`, `build-nc.log`) | ONE neutralization: `const bool noPages = false;` in `runOnePass`. **Exactly 4 pins RED**, all gate-dependent: `endToEndUserPasswordFailsHonestlyWithoutIt` (`!r.ok` fails — the blank-husk "unlock" is ACCEPTED: the exact disaster the gate exists to prevent), `endToEndFailedInPlaceLeavesSourceByteIdentical` (same mechanism), and the two password-retry pins (`usedPassword` false — without the gate's Tool classification the runner never retries). The other 8 green — zero collateral. Restored (`grep "NC STAGE"` → 0), full rebuild `BUILD_RC=0` (`build-restore2.log`). |
| pass-after ×3 SERIAL (`pass-1.log`, `pass-2.log`, `pass-3.log`) | `ctest -R "^TestGhostscriptUnlock$"` — 1/1 suite green, RC=0 each run (10/10 test functions inside, 0 skipped — Ghostscript installed). |
| Closing gate (`full-gate.log`, `build-restore2.log`) | Full-tree serial ctest — see §4. |

First full build of the feature: `build5.log` BUILD_RC=0 (attempts 1–3 were
the provision fix below + two externally-killed runs, exit 137 — the
documented contention class; runbook-sanctioned incremental retries,
recorded).

## 4. Build + environment episodes (Rule 1, recorded)

1. **Vendored-podofo provision gap (environment, not code).** The fresh
   worktree's `third_party/podofo/install/` was missing `bin/libpodofo.dll`
   (the CMake vendor gate requires BOTH the cmake config AND the DLL), so the
   first configure silently resolved MSYS2's podofo 0.10.4 → API-mismatch
   build errors in pre-existing sources (`PdfStructureMapper.cpp`, the exact
   Q02 trap the CMake comment describes). Fix: copied the provisioned
   `libpodofo.dll` from the main tree's identical provision (read-only;
   SHA-256 `b25f21f9…` byte-identical on both sides), reconfigured clean —
   "Using vendored podofo 1.1.0", BUILD_RC=0. Nothing in the repo changed.
2. **Locator bug caught by the smoke harness before any commit.**
   `highestVersionInRoot` used `QFileInfo::absolutePath()` (the PARENT
   directory) instead of `absoluteFilePath()` — the locator returned empty on
   a real install. Found by a lane-internal standalone harness
   (`evidence-gs-unlock/experiments/smoke-main.cpp.txt`) run against the REAL
   installed Ghostscript BEFORE the R7 cycle; fixed; the transcript
   (`smoke-runner-output.txt`) shows all five behaviors verified end-to-end
   (owner-only unlock clean; user-password honest failure with no output;
   with-password unlock via the response file; corrupted input fails with no
   output; zero leftover response files/candidates).
3. **Two external kills, exit 137** (build3, restore-rebuild 1) — the prior
   lanes' documented contention class; both resumed clean via the runbook's
   incremental retry (`build4.log`, `build-restore2.log`).
4. **winget does not carry official Ghostscript** (both plausible IDs exit 20
   "No package found"); installed from the official
   `ghostpdl-downloads` release `gs10080w64.exe`, SHA-512 verified against
   the release's own `SHA512SUMS` (`cb3ecc79…` match), SHA-256 recorded
   (`52a91b8b…`), silent install (`/VERYSILENT`) to the default
   `C:\Program Files\gs\gs10.08.0`, `--version` → `10.08.0`. The installer
   and binaries are NOT committed (AGPL).

## 5. Constraints honored

- Work confined to `D:/pdf/pdf-gs-unlock` (main tree read-only throughout —
  the one read was the provisioned `libpodofo.dll`, verified byte-identical;
  no other worktree touched; build dir strictly in-worktree `build-rel`).
- No push, no merge/rebase onto main, no stash/gc, no worktree/branch
  deletion. Ghostscript binaries/license text NEVER committed (AGPL —
  resolution + disclosure only).
- No CLAUDE.md / SECURITY.md created. No existing test weakened (the wiring
  changes were verified green: TestRibbonIntegrity 6/6, TestCommandRegistry
  14/14, TestControllers 15/15, TestTaskNavRegistry 17/17).
- `-dSAFER` unconditional (pinned), `-dNOSAFER` never emitted (pinned),
  secret never in argv (pinned against the REAL process argv).
