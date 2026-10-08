# GlyphPDF build, test and tooling review — 7 September 2026

**The infrastructure does not yet provide a reliable release gate.** This pass found one destructive path in the standalone cleanup tool and five concrete build/validation gaps. These are separate from the agents' engine, application and new-commit reviews.

Baseline: `0caa45e7d0751caaa36a54a085c41211a422f019`. Every file cited in INF01–INF06 was compared with `b58b91054ca7a573a09c56d950588ac4f966df42` and is unchanged. Repository source and installed applications were not modified. No installer, deployment script or real fuzz campaign was executed.

## INF01 — P1: cleanup CLI deletes the destination before validating the input

**Location:** `tools/clean_scanned_pdf.py:218–224`.

The tool unlinks the requested output PDF before opening the input. Passing the same path as input and output deletes the input, then fails to open it. Passing an invalid input with an existing output destroys the old output before reporting the input failure. Other rendering/processing errors after this point also leave the previous output lost. This is a standalone repository utility; it is not the installed app's save path.

**Evidence:** the unchanged `main()` AST was executed against disposable files, substituting the PDF-reader boundary only to stop before image processing. Both the same-path and missing-input cases raised `FileNotFoundError` after the destination disappeared. This proves filesystem ordering without claiming an OpenCV/PyMuPDF processing test. The source shows no recovery branch.

**Repair:** validate resolved input/output identities and open the input first. Write the new PDF to a sibling candidate, validate/close it, then replace the output atomically. Either explicitly reject same-file operation or support it through that candidate boundary. Preserve prior output on failure. Apply similarly careful ownership to deleting `page_*_cleaned.png` artifacts in a supplied output directory.

**Acceptance:** same input/output, nonexistent input, corrupt input, processing failure, final-write failure, and existing output. Verify byte-identical preservation whenever processing fails. A successful run should still produce the intended cleaned output; do not redesign the cleanup algorithm as part of this repair.

## INF02 — P2: local MSI pipeline can package a cached Debug or feature-disabled build

**Location:** `packaging/build-msi.ps1:171–180`; compare `CMakeLists.txt:58–80` and `.github/workflows/release.yml:122–140`.

The canonical local installer pipeline only configures when `build.ninja` is absent. With an existing Debug build it runs `cmake --build` and prints “Building Release” without changing the configuration. Even a new configure omits `GLYPHPDF_RELEASE_BUILD=ON` and `GLYPHPDF_ENABLE_LTO=ON`, unlike the release CI job. The CMake release hardening flags are explicitly disabled for Debug, so this is more than inaccurate status text. The script also does not run CTest before staging/signing.

**Evidence:** source/caller trace. The project's existing isolated build is a concrete Debug configuration of the kind this condition accepts, but the packaging script was not run. No claim is made that a currently distributed installer contains Debug code.

**Repair:** use a dedicated release build directory and always configure it with the release gate and intended optimization/test-fixture settings. Validate cached configuration when allowing `-SkipBuild`; require a recorded commit/build identity and successful release checks before signing/staging. The safe default must not silently reuse a developer build. Pass the actual build directory through deployment instead of having multiple scripts infer `build/` independently.

**Acceptance:** start with cached Debug, release feature disabled, and wrong-source builds; the pipeline must reconfigure or reject each. Confirm the final binary's compile/link configuration and commit identity. Failure of required tests must stop before artifacts are published.

## INF03 — P2: portable archive uses the wrong release version

**Location:** `packaging/build-portable.ps1:18–19`; caller `packaging/build-msi.ps1:42–44,250–268`.

The MSI script expects `1.3.2.3`, while its portable child hardcodes `1.3.1`. The child packages the newly staged payload as `GlyphPDF-1.3.1-x64-portable.zip` and writes that old version into the portable README. The parent looks for a `1.3.2.3` archive and silently omits its summary when it does not exist. CMake and WiX also say `1.3.2.3`.

**Evidence:** independently extracted version literals from the unchanged scripts. No artifact was rebuilt or published.

**Repair:** pass one authoritative version from the build/packaging entry point to the child, WiX metadata and generated README/manifest. Check the expected portable output exists and matches its version; missing output must fail the pipeline. Keep the solution small rather than introducing a release framework.

**Acceptance:** build an arbitrary test version and assert that EXE metadata, MSI metadata, archive name, README and hashes all correspond. The parent must report the archive actually produced.

## INF04 — P2: normal CI push checks exclude the active parity branch

**Location:** `.github/workflows/ci.yml:4–9`; same branch filter in `license-guard.yml:12–16`.

Push triggers include only `main` and `audit-remediation`. A direct push to the active `feat/parity-glm` branch does not trigger these jobs. A pull request targeting main can still trigger them; this finding does not claim that an open PR lacks CI. The comment promising checks on every active remediation push is stale.

**Repair:** include the maintained feature-branch pattern or remove the narrow push allowlist and use the project's intended pull-request/merge protections. Avoid hardcoding another soon-stale branch name if a stable pattern exists. Verify the actual repository rules separately; this pass reviewed workflow source, not hosted branch-protection settings.

**Acceptance:** a commit to the active branch triggers the intended build/test job, and the required status is associated with the exact commit being reviewed.

## INF05 — P2: release macro assertion reads a configure log instead of compiler flags

**Location:** `.github/workflows/release.yml:145–153`.

The “GLYPH_TESTING not defined” step reads `build/CMakeFiles/CMakeOutput.log` with missing-file errors suppressed. The current CMake build has no such file, so this check can print PASS without inspecting any data. Even where the log exists, it is not a reliable record of target compile definitions. The required contract is about the shipped application/engine targets; test executables may intentionally have test-specific definitions.

**Evidence:** source trace and filesystem inspection of the successful baseline build. No unsafe macro was enabled in any production build during this review. This is an ineffective check, not evidence of a currently shipped test bypass.

**Repair:** inspect generated compile commands or explicit target properties for the production targets and fail when the expected evidence is missing. Keep intentional test-target definitions distinct. Add a negative gate fixture that would fail if the forbidden definition reached a production compilation, and a normal release control that passes.

**Acceptance:** verify both forbidden-macro-present and evidence-file-missing cases fail. A grep over a configure log cannot substitute for this boundary check.

## INF06 — P2: Djot fuzz CI can pass without building or running the fuzzer

**Location:** `.github/workflows/glyphpdf-fuzz.yml:64–79`; `fuzz/build_clang/build_djot_clang.sh:18–31`.

The Ubuntu job calls a script with `ROOT="/c/Users/User/Projects/pdf"`. On a normal Ubuntu checkout, that path does not exist. The workflow converts build failure into a successful `echo`, skips the run if the executable is missing, and explicitly ignores the fuzzer's nonzero exit if it does run. Thus a green Djot job does not establish that the harness built, ran, or survived without a finding.

The redaction scripts also hardcode this developer checkout (`fuzz/run_oracles.sh:9`, and the build driver). Running them from another checkout can target the wrong code. The oracle runner only treats a missing driver output as a failure; it does not comprehensively reject missing/invalid oracle results. These scripts must be repaired before they are used as acceptance evidence.

**Evidence:** workflow/script trace. No fuzz run or hosted CI execution was claimed.

**Repair:** derive the repository root from the script location, declare the actual runner dependencies, require the built executable, and propagate build/harness exit codes. If a fuzz configuration is deliberately unavailable, represent it as an explicit separate skipped job rather than a successful security gate. Archive crashes with `if: always()` while retaining the failure. Validate oracle JSON and expected case count; do not infer success from absent “LEAK” text.

**Acceptance:** fresh runner/checkout, missing compiler, compile failure, missing executable, seeded crashing input, missing oracle output, and a clean short campaign. Each failure must reach the job result and its artifact should still be retained.

## Test and maintenance observations

The baseline inventory contains 294 text-bearing first-party files under `src` (60,247 text lines), plus the binary icon: 295 files total and 130 text files under `tests` (30,548 lines). These counts establish scope, not a claim of line-by-line verification. The agents supply production review coverage separately. This infrastructure pass indexed the build/test registration, skip and fixture boundaries, and deeply traced the cited workflows/scripts; it did not rerun every historical fixture itself. The newest-commit agent owns the fresh full build/test result.

The root CMake file has 3,115 lines, much of it repeated target registration/deployment. A small helper for the existing Qt-test pattern could reduce drift in source-root injection, runtime DLLs, logging and timeouts. Adopt it incrementally after the correctness fixes, preserving target-specific behavior. The existing N10 defect illustrates why executable test inventories and named-case runs matter; compiling functions containing assertions does not register them as tests.

Model-dependent, external-converter and validator tests contain explicit skips. Full-suite success must retain the skip details and should not be presented as OCR model or PDF/A conformance acceptance. Historical `CLAUDE.md` counts, build paths and release assertions are stale and should point to the evidence ledger and actual build manifest.

The current tool/helper witness data are in the consolidated evidence package under `team-infrastructure-review/witnesses.json`. They include the per-file comparison to `b58b910`, destructive CLI ordering reproductions, version extraction and release-log check. No source fix, deployment or installation is part of this review.
