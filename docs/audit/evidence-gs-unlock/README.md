# Evidence — Ghostscript-assisted unlock lane (feat/gs-unlock, 2026-10-04)

This directory maps the R7 evidence for `docs/audit/LANE-REPORT-gs-unlock-2026-10-04.md`.

## Installation provenance (AGPL Ghostscript — NOT vendored)

- winget does not carry the official package (searched 2026-10-05: no
  `ArtifexSoftware.GhostScript`/AGPL ids — both attempts exit 20 "No package
  found"). Installed from the OFFICIAL release source instead:
  `https://github.com/ArtifexSoftware/ghostpdl-downloads/releases/download/gs10080/gs10080w64.exe`
- `published-SHA512SUMS.txt` — the release's own checksum file.
- `installer-sha512.txt` — local `sha512sum` of the downloaded installer;
  MATCHES the published line for `gs10080w64.exe`
  (`cb3ecc798508851ba28b05e3fb914ddefe78168190726376d8581546ce61d5b216ffa3359e6f829072786bc12350501c7b6f99110d578696b87213d157774269`).
- `installer-sha256.txt` — SHA-256 of the same installer:
  `52a91b8bf09298788d7a57b9206127026c23eacd75405f0a131e26dc381dce50`.
- Installed silently (`/VERYSILENT /NORESTART`) to the default
  `C:\Program Files\gs\gs10.08.0`; `gswin64c.exe --version` → `10.08.0`.
- The binaries are NOT committed anywhere in the repo (AGPL — see
  `docs/research/ghostscript-unlock-notes.md` §1).

## Empirics (§4 of the research notes) — experiment logs

Driven from `gs-evidence/experiments` with fixtures built by qpdf (AES-256/R6)
from the MSYS2 toolchain; logs copied verbatim:

- `o1.log` — owner-password-only PDF re-distilled with NO password: exit 0,
  `Page 1`, output has no `/Encrypt` (the unlock that works).
- `o2.log` — user-password PDF WITHOUT password: **exit 0** with
  `**** This file requires a password for access.` +
  `No pages will be processed (FirstPage > LastPage).` and a BLANK 1-page
  output PDF (the trap the no-pages signature gate closes).
- `o3.log` — same input WITH `-dPDFSTOPONERROR`: exit 1 (honest, but…).
- `o7.log` — REPAIRABLE broken-xref file without STOPONERROR: exit 0, pages
  processed, `**** This file had errors that were repaired or ignored.` (the
  repair mission — must not be broken).
- `o6.log` — the SAME repairable file WITH `-dPDFSTOPONERROR`: exit 1
  (`/rangecheck in --runpdf--`) → STOPONERROR rejected: it kills the repair
  mission.
- `o5.log` — truncated (half-byte) corrupted protected file: exit 0 with the
  `No pages will be processed` signature → caught by the gate, no partial
  output committed.
- `o8.log` — password piped on stdin: NOT consumed by pdfi (same refusal
  output) → stdin alone cannot deliver the password to this interpreter.
- `o9.log` — `-sPDFPassword=userpw` on argv: WORKS (exit 0, Page 1) but
  violates M-1 → rejected.
- `o10.log` — `-sPDFPassword` delivered via `@response file`: WORKS (exit 0,
  Page 1), secret never in argv → the chosen channel.
- `o12.log` — response file with a double-quoted token: password containing a
  SPACE works (`-sPDFPassword="pw space"` → Page 1).
- `o15.log` — password containing a BACKSLASH works
  (`-sPDFPassword="pw\test"` → Page 1; literal, no escape mangling).

## R7 runs

Filled in by the lane report: `red-*.log` (fail-before), `nc.log`
(neutralized-once), `pass-1.log` / `pass-2.log` / `pass-3.log` (pass-after,
SERIAL), `build-*.log` (BUILD_RC).
