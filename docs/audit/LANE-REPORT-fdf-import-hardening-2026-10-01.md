# LANE REPORT — CSV/FDF form-import parser hardening (wave 2b, lane #8)

Date: 2026-10-01
Branch: `feat/fdf-import-hardening` (worktree `D:/pdf/pdf-w2b-fdf`, base `fe6ca6f2`)
Scorecard: PARITY-SCORECARD-2026-09-30 §4 row 8 (July §3 row 38, July P1 — CLOSED by this lane)
Status: **COMPLETE** — all work committed, R7 evidence contract satisfied.

## Mission

`FormManager::importFormData` parsed attacker-reachable FDF/CSV with a `(.*?)`
regex and `split("\",\"")` while every export path had already been hardened
(PGR-16/17 class). Harden the import side: bounded parsing, resource caps,
typed honest errors, fail-closed (no half-imports, no crashes, no silent
corruption), with pins beside `TestFillFormNoOp`.

## What was found at base (check-first)

Confirmed NOT done: `FormManager::importFormData` (then at :1447-1477, now
:1492) still used the FDF regex + CSV `split("\",\"")`; zero test coverage
(grep `importFormData` in tests = 0 at base).

Pre-existing environment issue (owner item): the worktree was missing
`third_party/podofo/install/bin/libpodofo.dll`, so CMake silently resolved
MSYS2 podofo 0.10.4 and `pdfws_djot/PdfStructureMapper.cpp` failed to compile
(`PdfContent::GetStack` missing in 0.10.x headers). Fixed by copying the DLL
from the main worktree's identical vendored tree (import lib + CMake target
files verified byte-identical by hash/diff before copying; main worktree
itself untouched, read-only copy-in). With the vendored 1.1.0 restored, the
whole tree builds clean (BUILD_RC=0).

## File-by-file changes

1. `src/core/interfaces/IFormManager.h`
   - Forward-declares `struct ErrorInfo;`; extends the pure-virtual
     `importFormData` with a defaulted `ErrorInfo *err = nullptr` out-param
     (source-compatible for any caller that doesn't opt in) + contract docs
     (bounded, fail-closed, no half-import).
2. `src/engines/FormManager.h`
   - Override signature updated to match.
3. `src/engines/FormManager.cpp`
   - Removes the FDF regex and CSV split (and the now-unused
     `<QRegularExpression>` include) and adds a row-8 parser block:
   - Caps: `kImportMaxFileBytes` 16 MiB (checked at open AND enforced on the
     bounded read, closing the size→read growth race), `kImportMaxFields`
     10 000, `kImportMaxStringChars` 1 MiB per name/value, `kImportMaxNesting`
     64.
   - `decodeUtf8Strict`: strict UTF-8 decode (QStringDecoder + hasError) —
     non-UTF-8 bytes refuse instead of silently becoming U+FFFD mojibake;
     UTF-8 BOM tolerated before the `%FDF` sniff.
   - `parseFdfFields`: string-aware bounded FdfCursor scanner. Bounded,
     monotonic (every branch advances ⇒ O(n), no regex backtracking on
     hostile input). Full PDF literal-string semantics (balanced parens,
     `\( \) \\ \n \r \t \b \f`, octal) via `parseLiteralRaw` collecting the
     raw escaped body, decoded through the canonical
     `pdfUnescapeLiteralString` — the exact inverse of `exportFormData`'s
     escaper (A-04's counterpart on the import side), then strict UTF-8.
     Hex-string `<...>` values, `/V` name values (e.g. button `/Yes`), and
     nested `<<dict>>`/`[array]` values (appearance streams) are handled
     within bounds. Honest structural refusals: missing `/Fields`, `/Fields`
     not followed by `[`, truncation mid-string/dict/array, unexpected
     tokens, over-cap strings/fields.
   - `parseCsvFields`: RFC-4180 state machine (FieldStart/Unquoted/InQuotes/
     AfterQuote). Quoted cells may hold commas, doubled quotes and raw
     newlines; quote violations, unterminated quotes at EOF, and unexpected
     characters after a closing quote refuse the whole file. Requires the
     exporter's `FieldName,FieldValue` header (case-insensitive) so a wrong
     file cannot import as garbage field names. Blank lines skipped; extra
     columns tolerated only when empty (trailing comma); empty field names,
     wrong column counts, and cap overruns refuse the WHOLE file with the
     offending row number — never a half-import.
   - Every refusal populates the typed `ErrorInfo` (severity Error, plain
     user message, technical detail with offsets/row/cap numbers) and logs a
     qWarning; nothing writes to `outputPath` until the ENTIRE file parsed.
   - Fills still land on the R01 transaction via `fillForm(..., lockFields=true)`
     — unchanged.
4. `src/shell/controllers/FormsController.cpp`
   - Passes `&importErr` and surfaces the typed reason in the "Import Failed"
     dialog (plain text, PGR-35 rule: message may quote file-derived text),
     replacing the generic "Could not import form data."; adds
     `core/ErrorInfo.h` include.
5. `tests/TestFillFormNoOp.cpp` (pins beside the existing
   `unknownFieldNameIsReported` pin; existing pin untouched)
   - Refusal pins (each asserts `!importFormData(...)`, typed ErrorInfo
     severity Error + honest message, and that NO output file was written):
     truncated FDF (cut mid-`/V` string), unterminated string open at EOF
     (nesting path), 10 001 fields, 1 MiB+1 value, 16 MiB+1 file, Latin-1
     bytes, empty/whitespace-only/header-only input, FDF without `/Fields`,
     malformed CSV record (THE half-import pin: 2 valid rows + 1 one-column
     row ⇒ whole file refused; message must cite row 3).
   - Fidelity pins (byte-exact round-trips through exportFormData):
     CSV cell with embedded comma/doubled quotes/raw newline; FDF value with
     `\(` `\)` `\\` `\n` escapes.
   - Positive controls: the app's own FDF and CSV exports re-import cleanly
     (over-refusal guards).
   - Zero-crash property: every hostile input above terminates in a typed
     refusal; the suite runs in <3 s.

Test-side strengthening note: the pins commit (7260d887) compiles against the
OLD signature so the fail-before run fails per-pin behaviorally; the fix
commit (098725db) strengthens the pins with typed-ErrorInfo assertions.
Strengthening only — no existing test weakened; justified in the commit
message per the runbook.

## R7 evidence (docs/audit/evidence-fdf-import/)

| Gate | Result | Evidence |
|---|---|---|
| 1. Fail-before (RED pins at base `fe6ca6f2`) | 11 FAILED / 5 PASS — every refusal case silently IMPORTED pre-fix; both fidelity pins proved silent corruption (CSV cell truncated at embedded newline; FDF `\\d` `\ne` escapes left literal). Positive controls + pre-existing pin PASS. | `red-before-pins-run1.log` |
| 2. Negative control (scoped revert, recorded ONCE) | Old regex/split parsers restored inside the new signature ⇒ 10 pins RED. The 16 MiB file-cap pin stayed green in THIS revert because the scoped revert retained the size check at open; its fail-before evidence is the base run, where the old code read the whole file unbounded. | `negative-control-scoped-revert.log` |
| 3. Pass-after ×3 consecutive SERIAL runs | 100% passed (2.82 s / 1.11 s / 0.67 s), plus a 4th green run after the negative-control restore (0.36 s). | `green-after-fix-run1.log`, `run2`, `run3`, `run4-post-negative-control.log` |
| Ripple sanity (new signature) | TestControllers, TestFormBuilder, TestFillFormLock, TestPersistenceOutcomes — 4/4 passed. | `forms-cluster-sanity.log` |
| Full build | `cmake --build build-rel --config Release -- -k 0` ⇒ BUILD_RC=0 (vendored podofo 1.1.0; LTO on). | build log tail in session transcript |

All ctest runs SERIAL (no -j), `QT_QPA_PLATFORM=offscreen`, ucrt64-first PATH
(one 0xc0000139 run occurred when ctest was invoked without it — rerun
correctly; that first invocation's log was overwritten by the valid rerun).

## Commits

- `7260d887` test(forms): row-8 pins (RED at base) + RED evidence log.
- `098725db` fix(forms): bounded fail-closed FDF/CSV import parsers with
  typed ErrorInfo refusals (row 8) + all evidence logs.

No merges, no pushes, no branch switches. Working tree clean.

## Known limits (honest)

- CSV header must be exactly `FieldName,FieldValue` (case-insensitive) — the
  format `exportFormData` writes. Hand-made CSVs with a different header now
  refuse honestly instead of importing garbage names (a behavior tightening,
  pinned as fail-closed doctrine).
- FDF hex-string values are decoded as UTF-8; Acrobat's UTF-16BE-with-BOM hex
  text strings would refuse with the typed UTF-8 error (fail-closed, not
  silent). Our own export writes literal UTF-8 strings, so export→import is
  lossless (pinned).
- `/T`-only branch nodes (non-leaf field dicts with /Kids) are skipped by
  design; only leaf `/T`+`/V` pairs become fill data.
- Duplicate field names keep last-write-wins (QVariantMap semantics,
  unchanged from base); values with formula-ish leads (`=`, `+`) import
  literally — `csvFormulaSafeCell` remains an EXPORT-boundary defense and is
  intentionally not applied on import.
- Empty-data files (empty/whitespace/header-only, no `/Fields`, zero rows)
  now REFUSE instead of performing a zero-change rewrite of the document —
  an honest no-op refusal (pinned).

## Owner items

1. `third_party/podofo/install/bin/libpodofo.dll` was missing from this
   worktree at base (runbook claims it is "ALREADY in your worktree"). Other
   wave-2b worktrees may be missing it too — symptom: pdfws_djot fails to
   compile on `PdfContent::GetStack` or tests exit 0xc0000135/0xc0000139.
   Suggest the integrator verify each worktree's vendored bin/ before folding.
2. The forms-JS / unsupported-field dialogs and the new Import-Failed message
   are user-facing strings in English (`tr()`-wrapped, translation-ready);
   the .ts files were not touched (out of lane scope).
