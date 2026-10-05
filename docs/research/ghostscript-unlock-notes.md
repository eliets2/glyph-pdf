# Research notes — Ghostscript-assisted PDF unlock (PRD §9.11 unlock bullet, SPEC-TRACEABILITY 9.17)

- **Date:** 2026-10-04
- **Lane:** `feat/gs-unlock` (worktree `D:/pdf/pdf-gs-unlock`, base `93863d70`)
- **Status:** implemented as RESOLUTION + DISCLOSURE ONLY. Ghostscript binaries are **NOT vendored and MUST NOT be committed** (see §1 — AGPL).

---

## 1. Licensing: why Ghostscript is never vendored (OWNER DECISION REQUIRED)

**TL;DR: Ghostscript (Artifex) is licensed under the GNU AGPL v3.0 — NOT LGPL like the
vendored 7-Zip. Shipping Ghostscript binaries with a proprietary application requires
either (a) licensing the entire combined work under AGPL terms, or (b) a commercial
license from Artifex. That is an owner-level business/licensing decision and is
deliberately NOT made in this lane.**

The contrast with the one existing vendored external tool is the whole point:

| | 7-Zip (`third_party/7zip/`) | Ghostscript (this feature) |
|---|---|---|
| License | GNU LGPL 2.1+ (some parts BSD / LGPL+unRAR) | **GNU AGPL 3.0** |
| Vendoring precedent | Committed to the repo with SHA-256 pins | **Refused** — AGPL §13 (network/remote-use clause) plus the held-form requirement make binary redistribution inside a closed-source desktop app a compliance commitment, not a technical step |
| Alternative | none needed | Artifex sells commercial licenses; OR the app stays AGPL-clean by NOT distributing Ghostscript |
| Decision owner | wave-2b lane (LGPL analysis documented) | **GlyphPDF owner** — this lane records the question, it does not answer it |

What this means concretely for the feature shipped here:

1. **No AGPL bytes enter the repository.** No DLL, no EXE, no installer, no extracted
   binary under `third_party/` or anywhere else. Resolution happens at *runtime on the
   user's machine* against a Ghostscript the USER installed. The app never ships,
   links, or copies Ghostscript.
2. **AGPL §13 / held-form analysis (documented for the owner):** invoking Ghostscript as
   a separate process on the user's own installation (the same aggregation shape used
   for 7-Zip) is generally understood as mere aggregation *when the two works are not
   distributed together*. If Ghostscript were EVER bundled into the installer/MSI, the
   combined distribution would import AGPL obligations onto the whole work (or require
   an Artifex commercial license). Any bundling proposal must go through the owner with
   that framing. A PoDoFo/qpdf-based future reimplementation (both are LGPL/MPL-family)
   would avoid the question entirely, at the cost of Ghostscript's superior re-distiller
   repair quality.
3. **The user-facing disclosure says where the tool must come from.** When Ghostscript
   is absent, the Unlock dialog names the standard install locations it searched and
   does not pretend the feature is unavailable for a technical reason — the reason is a
   licensing decision plus an absent user-side install.

## 2. Resolution policy (locator-disclose pattern)

`gp::GhostscriptLocator` (`src/engines/GhostscriptRunner.h`) owns the policy:

- **Standard install locations only.** `C:/Program Files/gs/gs*/bin/gswin64c.exe` and
  `C:/Program Files (x86)/gs/gs*/bin/gswin64c.exe` (the AGPL installer's default
  target). Among multiple installed versions the HIGHEST version directory wins.
- **No PATH leg — deliberately.** The 7-Zip precedent (wave-2b audit F-02, CWE-427)
  removed PATH/Program-Files fallback legs for the *vendored* tool because a planted
  binary would receive the document AND a secret. Here nothing is vendored, so the
  standard-install legs ARE the feature — but the PATH leg is still excluded: a
  `gs.exe` on PATH is whatever the ambient environment says it is (a different
  version with different distiller behavior, or a planted binary), and the feature
  never runs arbitrary binaries it cannot name and place. The disclosure names the
  exact searched locations so a missing install is diagnosable.
- **Absence is honest.** Empty result ⇒ the Unlock action discloses that Ghostscript
  was not found, shows the searched paths, and stops. No guessing, no silent fallback,
  no partial behavior.

## 3. Unlock semantics (why pdfwrite re-distillation)

The command shape (`GhostscriptRunner::buildUnlockArgs`):

```
gswin64c -dSAFER -dBATCH -dNOPAUSE -sDEVICE=pdfwrite -sOutputFile=<candidate> <input>
```

- **Owner-password restrictions disappear** because the output is a freshly written PDF
  with no `/Encrypt` dictionary: Ghostscript reads the source (owner-password-only files
  open for reading with the empty user password), re-interprets every page, and writes a
  new document that carries no security handler at all. Content is preserved visually
  (the re-distiller's job); it is NOT byte-identical — fonts may be re-embedded,
  metadata is re-stamped by Ghostscript, and interactive semantics that cannot be
  re-expressed (some form-field action types, for example) can degrade. The Unlock
  dialog says exactly this before running.
- **Structurally broken protected files** get Ghostscript's error recovery for free:
  the pdfwrite path rebuilds what it can parse and drops what it cannot. This is the
  "re-distill broken protected files" bullet of §9.11 — a capability neither PoDoFo nor
  qpdf matches in practice (qpdf repairs structure but keeps the security handler
  semantics; PoDoFo is a strict parser).
- **User-password files are NOT bypassable.** A document encrypted against the empty
  user password (owner password only) is unlockable; a document requiring a real user
  password cannot be re-distilled without it, and Ghostscript fails. The feature then
  fails honestly — it never circumvents a document the user cannot legally open (PRD
  §9.11 bullet).
- **`-dSAFER` is MANDATORY.** SAFER disables PostScript-level file access outside the
  explicitly permitted paths; a hostile document must never get PostScript execution
  with filesystem reach. `-dNOSAFER` is never emitted by this code (pinned by test).

## 4. Secrets: the M-1 discipline (verified against Ghostscript 10.08.0)

The user password NEVER travels on the command line (M-1, AUDIT-SECURITY-2026-09-25,
CWE-214 — the same discipline as the 7-Zip `-p` prompt reply). What was tried,
empirically, against the installed AGPL Ghostscript 10.08.0
(`docs/audit/evidence-gs-unlock/experiments/`):

1. **First attempt runs WITHOUT any password** — most protected files are
   owner-password-only and open with the empty user password.
2. **A piped stdin prompt reply does NOT work** (the C-based `pdfi` interpreter
   does not read a non-tty prompt reply; the password-less run prints
   `**** This file requires a password for access.` and — the trap — **exits 0**
   while pdfwrite still emits a BLANK single-page PDF).
3. `-sPDFPassword=<pw>` on the command line works but puts the secret in argv:
   **rejected** (M-1).
4. **The working off-argv channel: a Ghostscript `@response file`.** The retry
   pass expands `-sPDFPassword="<password>"` from a file staged OWNER-ONLY
   (0600) in the SafeSave candidates directory; argv carries only `@<path>`.
   The file is deleted the moment the run ends (every path). Verified with a
   password containing spaces AND one containing a backslash (double-quoted
   token form); an embedded double quote cannot be expressed — such a password
   simply fails the retry honestly. The value is ALSO piped to the child's
   stdin (channel closed after the write) so a future prompt-reading
   interpreter build is answered too.
5. **The blank-husk trap is gated**: because the password refusal exits 0, the
   runner treats Ghostscript's own `No pages will be processed` diagnostic as a
   failure regardless of exit code (quoting the output). Verified that this
   signature does NOT fire for genuinely repairable documents (a broken-xref
   file re-distills with `…errors that were repaired or ignored` + pages
   processed) — so the repair mission survives the gate. Deliberately NO
   `-dPDFSTOPONERROR` (verified: it exits 1 on the repairable case too).

## 5. Transaction shape (SafeSave)

Identical skeleton to `HomeController::createEncryptedPackage` / `runExternalWriterCommit`
but owned by `GhostscriptRunner` so the tool's own error output can be captured:

1. `SafeSave::makeUniqueCandidate` reserves an owner-only candidate path.
2. Ghostscript writes the CANDIDATE (`-sOutputFile=`); the destination is never an
   argument to the process. stdout+stderr are merged and captured for error quoting.
3. Bounded, cancellable wait (the runner's own `runBoundedProcess` idiom modeled on
   `SafeSave::runBoundedProcess`): cancel/timeout KILL the process we own.
4. Candidate validation: exists, non-empty, starts with `%PDF`.
5. `SafeSave::commitFileToDestination` — atomic, checked; on any failure the
   destination is byte-identical and the candidate is removed. Commit-or-discard, no
   partial file.

## 6. UX (Security section)

`SecurityController::unlockPdf()` — a new Security ribbon action ("Unlock PDF…"):
an honest pre-flight dialog (what unlock does: restrictions removed, content preserved
visually, metadata re-stamped, output written to a NEW file — the source is never
modified), an optional password field (stdin-delivered), an absence disclosure naming
the searched install locations, and success/failure messages that quote Ghostscript's
own captured output on failure.

## 7. What would change the licensing posture (for the owner)

- Buying an Artifex commercial license ⇒ Ghostscript could be vendored like 7-Zip
  (SHA-256 pins, PROVENANCE.md, staged beside the executable — the whole existing
  apparatus applies).
- Deciding the app itself can be AGPL ⇒ same.
- Neither happens ⇒ the feature ships as it is here: the user installs Ghostscript,
  GlyphPDF finds and drives it, and discloses absence honestly.
