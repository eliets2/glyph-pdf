# GlyphPDF v1.2.0 — Security Patch Release

**Release date:** 2026-06-15
**Type:** Security patch (upgrade strongly recommended)

---

## Summary

v1.2.0 is a security patch release addressing five critical and high-severity
vulnerabilities discovered during an internal audit of the v1.0.1 codebase,
plus two architecture bug fixes. All users on v1.0.x should upgrade.

---

## Security Fixes

### SECFIX-1 — CRITICAL: OCSP test-fixture bypass removed from production builds

`GLYPH_TESTING` was unconditionally defined in `CMakeLists.txt`, which enabled
an OCSP `basic_verify` bypass in `SignatureManager.cpp` intended only for CI
fixture loading. Every production build shipped with signature-chain verification
disabled.

**Fix:** Replaced with `option(GLYPHPDF_ENABLE_TEST_FIXTURES OFF)`. Production
builds no longer define this symbol; CI passes `-DGLYPHPDF_ENABLE_TEST_FIXTURES=ON`.

---

### SECFIX-2 — HIGH: AES-256-GCM downgrade oracle in PdfEncryptPubSec

The AES version byte in `PdfEncryptPubSec` was unauthenticated. An unknown value
fell through to a legacy CBC path, allowing an attacker to strip GCM integrity
protection by flipping a single byte.

**Fix:** Unknown version bytes now throw `PdfError(InvalidEncryptionDict)`.

---

### SECFIX-3 — HIGH: TOCTOU in MSI update verifier

`UpdateChecker::verifyAuthenticode` called `WinVerifyTrust` with a file path
rather than an open handle, creating a race window between verification and
`msiexec` launch. On non-Windows builds there was no refusal to proceed.

**Fix:** The MSI is now opened with `CreateFileW` (deny-write sharing) before
verification; `WinVerifyTrust` receives the handle (`hFile`) with `pcwszFilePath`
set to `nullptr`. Non-Windows builds `#else`-branch into an unconditional abort.

---

### SECFIX-4 — HIGH: Unbounded recursion + over-broad allowlist in SignatureManager

`collectRefs` (used in shadow-attack detection) had no cycle guard or depth cap,
allowing a crafted PDF with circular indirect-reference loops to stack-overflow.
The `/Sig` allowlist also permitted any new signature object regardless of
`/SubFilter`, letting an attacker append an arbitrary signature under a legitimate
DocTimeStamp.

**Fix:** Added a `std::set<QPDFObjGen> visited` cycle guard and `kMaxDepth = 200`
cap. The allowlist now accepts only `/SubFilter /ETSI.RFC3161` (genuine
document timestamps); all other new `/Sig` objects are rejected as shadow attacks.

---

### SECFIX-5 — MEDIUM: OllamaProvider sent document content to arbitrary HTTPS hosts

`OllamaProvider` validated URL scheme but not host, so a document could configure
the Ollama endpoint to an arbitrary HTTPS server and exfiltrate content. The
per-message 32 KB truncation did not bound aggregate conversation length.

**Fix:** `isAllowedEndpoint()` now restricts to `localhost`/`127.0.0.1`/`::1` or
explicit HTTPS allowlist entries only. A 96 KB aggregate cap (`kMaxTotalBytes`)
trims oldest history entries when the conversation grows too large.

---

## Architecture Fixes

### ARCHFIX-1 — Undo-history corruption in EditTextInlineCommand

`redo()` re-captured the pre-edit page snapshot on every call, so the second
undo after a redo restored to a wrong state.

**Fix:** Capture is now guarded by `if (m_originalPageBytes.isEmpty())`.

### ARCHFIX-1 — OllamaProvider HTTP semaphore could block forever

`sem.acquire()` was called without a timeout inside `QtConcurrent::run`, able
to block the thread pool indefinitely if the main event loop stalled.

**Fix:** Replaced with `sem.tryAcquire(1, 20000)` (20-second timeout).

---

## Installation

### Installer

1. Download `GlyphPDF-1.2.0-x64.msi`
2. Verify SHA-256:
   ```powershell
   (Get-FileHash "GlyphPDF-1.2.0-x64.msi" -Algorithm SHA256).Hash
   # Expected: EA2ED66C256C3346E9ABAF300AB34F1191D1116EFC88BA1DC38AB0B354116BCA
   ```
3. Run the installer — upgrades in-place from v1.0.x, no uninstall required.

---

## Files

| File | SHA-256 |
|------|---------|
| `GlyphPDF-1.2.0-x64.msi` | `EA2ED66C256C3346E9ABAF300AB34F1191D1116EFC88BA1DC38AB0B354116BCA` |
