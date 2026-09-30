# M-1 evidence — 7-Zip package password moved off argv (feat/sec-consume-ocsp)

Base: 2ccfd5ba. Suite: TestControllers (15 slots after this lane).
Finding: AUDIT-SECURITY-2026-09-25 M-1 (MEDIUM, CWE-214) — the AES-256 ZIP
package password traveled as `-p<password>` on the 7-Zip command line (create
and read-back), readable by any same-user process for the whole bounded run.

## Behavior verified against the SHIPPED 7-Zip (do-not-guess clause)

7-Zip 26.02 (x64) on this machine, probed empirically before any code change:
- CREATE: `7z a -tzip -mem=AES256 -p <archive> <file>` — `-p` with an EMPTY
  value PROMPTS ("Enter password (will not be echoed)") and reads the reply
  from stdin. A piped reply produces a genuinely encrypted archive (verified:
  `-p<that password>` opens it, `-pWrong` fails with exit 2).
- READ-BACK: `7z t <archive>` with NO `-p` switch prompts and reads stdin the
  same way. CAUTION: a BARE `-p` on `t` is parsed as an EMPTY password (no
  prompt) and fails — the read-back must OMIT the switch entirely.
- `-si` is stdin for archive DATA, not the password. No `-p@`/askpass exists.

## Fix

- SafeSave::runBoundedProcess gained an optional `stdinData` (written right
  after start, write channel then closed) and runExternalWriterCommit threads
  it through — all existing callers unchanged (default empty).
- HomeController::encryptedPackageCreateArgs / encryptedPackageValidateArgs —
  pure static arg builders (same seam status as planForExport/shareEmailUrl);
  the create argv carries a bare `-p`, the read-back argv carries no `-p` at
  all. createEncryptedPackage delivers `password.toUtf8() + '\n'` through the
  stdin pipe.

## Pin

testEncryptedPackageArgsCarryNoPassword: (a) create argv contains bare `-p`
and the password appears in NO argument; (b) read-back argv carries no `-p`;
(c) live end-to-end with the real 7z.exe (QSKIP when absent): create via
stdin password → read-back with stdin password exits 0 → WRONG password on
argv fails → NO password fails (the prompt-hang kill at the bounded deadline
counts as fail-closed).

- fail-before (historical argv constructions scoped back in): RED —
  "create must use the bare -p prompt switch" fails.
- NC (read-back bare `-p` scoped back in — the empty-password parse trap):
  RED — argv-shape comparison fails.
- pass-after ×3 serial (full suite): 15 passed, 0 failed each.

## Residual constraints (owner items, not forced)

- The stdin channel is only as trustworthy as the OS pipe: the exposure moves
  from "any process reading argv" to "same-user processes holding the child
  process handle" — the CWE-214 surface shrinks drastically on both Windows
  and Linux but is not zero. The audit's preferred long-term fix remains an
  in-process AES-256 ZIP writer (the app already ships OpenSSL).
- 7-Zip's interactive-prompt protocol is version behavior, not a documented
  contract; the pin's live section freezes the assumption against the
  installed 7z and QSKIPs cleanly where the behavior cannot be verified.
